/* Copyright (C) 2005 J.F.Dockes 
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "autoconfig.h"

#include <stdio.h>
#include <errno.h>
#include "safefcntl.h"
#include <sys/types.h>
#include "safeunistd.h"

#include <iostream>
#include <string>

#include "cstr.h"
#include "mh_text.h"
#include "log.h"
#include "readfile.h"
#include "md5ut.h"
#include "rclconfig.h"
#include "pxattr.h"
#include "pathut.h"

using namespace std;

const int MB = 1024*1024;
const int KB = 1024;

// Process a plain text file
bool MimeHandlerText::set_document_file_impl(const string& mt, const string &fn)
{
    LOGDEB("MimeHandlerText::set_document_file: [" << fn << "] offs " <<
           m_offs << "\n");

    m_fn = fn;
    // This should not be necessary, but it happens on msw that offset is large
    // negative at this point, could not find the reason (still trying).
    m_offs = 0;

    // file size for oversize check
    long long fsize = path_filesize(m_fn);
    if (fsize < 0) {
        LOGERR("MimeHandlerText::set_document_file: stat " << m_fn <<
               " errno " << errno << "\n");
        return false;
    }

#ifndef _WIN32
    // Check for charset defined in extended attribute as per:
    // http://freedesktop.org/wiki/CommonExtendedAttributes
    pxattr::get(m_fn, "charset", &m_charsetfromxattr);
#endif

    // Max file size parameter: texts over this size are not indexed
    int maxmbs = 20;
    m_config->getConfParam("textfilemaxmbs", &maxmbs);

    if (maxmbs == -1 || fsize / MB <= maxmbs) {
        // Text file page size: if set, we split text files into
        // multiple documents
        int ps = 1000;
        m_config->getConfParam("textfilepagekbs", &ps);
        if (ps != -1) {
            ps *= KB;
            m_paging = true;
        }
        // Note: size_t is guaranteed unsigned, so max if ps is -1
        m_pagesz = size_t(ps);
        if (!readnext())
            return false;
    } else {
        LOGINF("MimeHandlerText: file too big (textfilemaxmbs=" << maxmbs <<
               "), contents will not be indexed: " << fn << endl);
    }
    if (!m_forPreview) {
	string md5, xmd5;
	MD5String(m_text, md5);
	m_metaData[cstr_dj_keymd5] = MD5HexPrint(md5, xmd5);
    }
    m_havedoc = true;
    return true;
}

bool MimeHandlerText::set_document_string_impl(const string& mt,
                                               const string& otext)
{
    m_text = otext;
    if (!m_forPreview) {
	string md5, xmd5;
	MD5String(m_text, md5);
	m_metaData[cstr_dj_keymd5] = MD5HexPrint(md5, xmd5);
    }
    m_havedoc = true;
    return true;
}

bool MimeHandlerText::skip_to_document(const string& ipath)
{
    char *endptr;
    int64_t t = strtoll(ipath.c_str(), &endptr, 10);
    if (endptr == ipath.c_str()) {
	LOGERR("MimeHandlerText::skip_to_document: bad ipath offs ["  <<
               ipath << "]\n");
	return false;
    }
    m_offs = t;
    readnext();
    return true;
}

bool MimeHandlerText::next_document()
{
    LOGDEB("MimeHandlerText::next_document: m_havedoc "  << m_havedoc << "\n");

    if (m_havedoc == false)
	return false;

    if (m_charsetfromxattr.empty())
	m_metaData[cstr_dj_keyorigcharset] = m_dfltInputCharset;
    else 
	m_metaData[cstr_dj_keyorigcharset] = m_charsetfromxattr;

    m_metaData[cstr_dj_keymt] = cstr_textplain;

    size_t srclen = m_text.length();
    m_metaData[cstr_dj_keycontent].swap(m_text);

    // We transcode even if defcharset is supposedly already utf-8:
    // this validates the encoding.
    // txtdcode() truncates the text if transcoding fails
    (void)txtdcode("mh_text");


    // If the text length is 0 (the file is empty or oversize), or we are 
    // not paging, we're done
    if (srclen == 0 || !m_paging) {
        m_havedoc = false;
        return true;
    } else {
        // Paging: set ipath then read next chunk. 

        // Don't set ipath for the first chunk to avoid having 2
        // records for small files (one for the file, one for the
        // first chunk). This is a hack. The right thing to do would
        // be to use a different mtype for files over the page size,
        // and keep text/plain only for smaller files.
        string buf = lltodecstr(m_offs - srclen);
        if (m_offs - srclen != 0)
            m_metaData[cstr_dj_keyipath] = buf;
        readnext();
        // This ensures that the first chunk (offs==srclen) of a
        // multi-chunk file does have an ipath. Else it stands for the
        // whole file, which used to be the case but does not seem
        // right
        if (m_havedoc)
            m_metaData[cstr_dj_keyipath] = buf;
        return true;
    }
}

bool MimeHandlerText::readnext()
{
    string reason;
    m_text.clear();
    if (!file_to_string(m_fn, m_text, m_offs, m_pagesz, &reason)) {
        LOGERR("MimeHandlerText: can't read file: "  << reason << "\n" );
        m_havedoc = false;
        return false;
    }
    if (m_text.length() == 0) {
        // EOF
        m_havedoc = false;
        return true;
    }

    // If possible try to adjust the chunk to end right after a line
    // Don't do this for the last chunk. Last chunk of exactly the
    // page size might be unduly split, no big deal
    if (m_text.length() == m_pagesz) {
        string::size_type pos = m_text.find_last_of("\n\r");
        if (pos != string::npos && pos != 0) {
            m_text.erase(pos);
        }
    }
    m_offs += m_text.length();
    return true;
}

