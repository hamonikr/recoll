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

void MimeHandlerText::getparams()
{
    m_config->getConfParam("textfilemaxmbs", &m_maxmbs);

    // Text file page size: if set, we split text files into
    // multiple documents
    int ps = 1000;
    m_config->getConfParam("textfilepagekbs", &ps);
    if (ps != -1) {
        ps *= 1024;
        m_paging = true;
    } else {
        m_paging = false;
    }
    m_pagesz = size_t(ps);
    m_offs = 0;
}

// Process a plain text file
bool MimeHandlerText::set_document_file_impl(const string&, const string &fn)
{
    LOGDEB("MimeHandlerText::set_document_file: [" << fn << "] offs " <<
           m_offs << "\n");

    m_fn = fn;
    // file size for oversize check
    m_totlen = path_filesize(m_fn);
    if (m_totlen < 0) {
        LOGERR("MimeHandlerText::set_document_file: stat " << m_fn <<
               " errno " << errno << "\n");
        return false;
    }

#ifndef _WIN32
    // Check for charset defined in extended attribute as per:
    // http://freedesktop.org/wiki/CommonExtendedAttributes
    pxattr::get(m_fn, "charset", &m_charsetfromxattr);
#endif

    getparams();
    if (m_maxmbs != -1 && m_totlen / (1024*1024) > m_maxmbs) {
        LOGINF("MimeHandlerText: file too big (textfilemaxmbs=" << m_maxmbs <<
               "), contents will not be indexed: " << fn << endl);
    } else {
        if (!readnext()) {
            return false;
        } 
    }
    m_havedoc = true;
    return true;
}

bool MimeHandlerText::set_document_string_impl(const string&,
                                               const string& otext)
{
    m_fn.clear();
    m_totlen = otext.size();

    getparams();
    if (m_maxmbs != -1 && m_totlen / (1024*1024) > m_maxmbs) {
        LOGINF("MimeHandlerText: text too big (textfilemaxmbs=" << m_maxmbs <<
               "), contents will not be indexed\n");
    } else {
        if (!m_paging || (m_totlen <= (int64_t)m_pagesz)) {
            // Avoid copy for texts smaller than page size
            m_paging = false;
            m_text = otext;
            m_offs = m_totlen;
        } else {
            m_alltext = otext;
            readnext();
        }
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
    if (!m_forPreview) {
        string md5, xmd5;
        MD5String(m_text, md5);
        m_metaData[cstr_dj_keymd5] = MD5HexPrint(md5, xmd5);
    }
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

        int64_t start_offset = m_offs - srclen;
        string buf = lltodecstr(start_offset);

        // Don't set ipath for the first chunk to avoid having 2
        // records for small files (one for the file, one for the
        // first chunk). This is a hack. The right thing to do would
        // be to use a different mtype for files over the page size,
        // and keep text/plain only for smaller files.
        if (start_offset != 0)
            m_metaData[cstr_dj_keyipath] = buf;

        readnext();

        // This ensures that the first chunk (offs==srclen) of a
        // multi-chunk file does have an ipath. Else it stands for the
        // whole file (see just above), which used to be the case but
        // does not seem right
        if (m_havedoc)
            m_metaData[cstr_dj_keyipath] = buf;

        return true;
    }
}

bool MimeHandlerText::readnext()
{
    string reason;
    m_text.clear();
    if (!m_fn.empty()) {
        if (!file_to_string(m_fn, m_text, m_offs, m_pagesz, &reason)) {
            LOGERR("MimeHandlerText: can't read file: "  << reason << "\n" );
            m_havedoc = false;
            return false;
        }
    } else {
        m_text = m_alltext.substr((size_t)m_offs, m_pagesz);
    }

    if (m_text.length() == 0) {
        // EOF
        m_havedoc = false;
        return true;
    }

    // If possible try to adjust the chunk to end right after a line
    // Don't do this for the last chunk. Last chunk of exactly the
    // page size might be unduly split, no big deal
    if (m_text.length() == m_pagesz && m_text.back() != '\n' &&
        m_text.back() != '\r') {
        string::size_type pos = m_text.find_last_of("\n\r");
        if (pos != string::npos && pos != 0) {
            m_text.erase(pos);
        }
    }
    m_offs += m_text.length();
    return true;
}
