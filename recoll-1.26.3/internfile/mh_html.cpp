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

#include "cstr.h"
#include "mimehandler.h"
#include "log.h"
#include "readfile.h"
#include "transcode.h"
#include "mimeparse.h"
#include "myhtmlparse.h"
#include "indextext.h"
#include "mh_html.h"
#include "smallut.h"
#include "md5ut.h"

#include <iostream>

using namespace std;

bool MimeHandlerHtml::set_document_file_impl(const string& mt, const string &fn)
{
    LOGDEB0("textHtmlToDoc: " << fn << "\n");
    string otext;
    string reason;
    if (!file_to_string(fn, otext, &reason)) {
        LOGERR("textHtmlToDoc: cant read: " << fn << ": " << reason << "\n");
	return false;
    }
    m_filename = fn;
    return set_document_string(mt, otext);
}

bool MimeHandlerHtml::set_document_string_impl(const string& mt, 
                                               const string& htext) 
{
    m_html = htext;
    m_havedoc = true;

    if (!m_forPreview) {
	// We want to compute the md5 now because we may modify m_html later
	string md5, xmd5;
	MD5String(htext, md5);
	m_metaData[cstr_dj_keymd5] = MD5HexPrint(md5, xmd5);
    }
    return true;
}

bool MimeHandlerHtml::next_document()
{
    if (m_havedoc == false)
	return false;
    m_havedoc = false;
    // If set_doc(fn), take note of file name.
    string fn = m_filename;
    m_filename.erase();

    string charset = m_dfltInputCharset;
    LOGDEB("MHHtml::next_doc.: default supposed input charset: [" << charset
          << "]\n");
    // Override default input charset if someone took care to set one:
    map<string,string>::const_iterator it = m_metaData.find(cstr_dj_keycharset);
    if (it != m_metaData.end() && !it->second.empty()) {
	charset = it->second;
	LOGDEB("MHHtml: next_doc.: input charset from ext. metadata: [" <<
               charset << "]\n");
    }

    // - We first try to convert from the supposed charset
    //   (which may depend of the current directory) to utf-8. If this
    //   fails, we keep the original text
    // - During parsing, if we find a charset parameter, and it differs from
    //   what we started with, we abort and restart with the parameter value
    //   instead of the configuration one.

    MyHtmlParser result;
    for (int pass = 0; pass < 2; pass++) {
	string transcoded;
	LOGDEB("Html::mkDoc: pass " << pass << "\n");
	MyHtmlParser p;

	// Try transcoding. If it fails, use original text.
	int ecnt;
	if (!transcode(m_html, transcoded, charset, "UTF-8", &ecnt)) {
	    LOGDEB("textHtmlToDoc: transcode failed from cs '" <<
                   charset << "' to UTF-8 for[" << (fn.empty()?"unknown":fn) <<
                   "]");
	    transcoded = m_html;
	    // We don't know the charset, at all
	    p.reset_charsets();
	    charset.clear();
	} else {
	    if (ecnt) {
		if (pass == 0) {
		    LOGDEB("textHtmlToDoc: init transcode had " << ecnt <<
                           " errors for ["<<(fn.empty()?"unknown":fn)<< "]\n");
		} else {
		    LOGERR("textHtmlToDoc: final transcode had " << ecnt <<
                           " errors for ["<< (fn.empty()?"unknown":fn)<< "]\n");
		}
	    }
	    // charset has the putative source charset, transcoded is now
	    // in utf-8
	    p.set_charsets(charset, "utf-8");
	}

	try {
	    p.parse_html(transcoded);
	    // No exception: ok? But throw true to use the same
	    // code path as if an exception had been thrown by parse_html
	    throw true;
	    break;
	} catch (bool diag) {
	    result = p;
	    if (diag == true) {
		// Parser throws true at end of text. ok

		if (m_forPreview) {
		    // Save the html text
		    m_html = transcoded;
		    // In many cases, we need to change the charset decl,
		    // because the file was transcoded. It seems that just
		    // inserting one is enough (only the 1st one seems to
		    // be used by browsers/qtextedit).
                    string::size_type idx = m_html.find("<head>");
		    if (idx == string::npos)
			idx = m_html.find("<HEAD>");
		    if (idx != string::npos)
			m_html.replace(idx+6, 0, 
				       "<meta http-equiv=\"content-type\" "
				       "content=\"text/html; charset=utf-8\">");
		}

		break;
	    }

	    LOGDEB("textHtmlToDoc: charset [" << charset << "] doc charset ["<<
                   result.get_charset() << "]\n");
	    if (!result.get_charset().empty() && 
		!samecharset(result.get_charset(), result.fromcharset)) {
		LOGDEB("textHtmlToDoc: reparse for charsets\n");
		// Set the origin charset as specified in document before
		// transcoding again
		charset = result.get_charset();
	    } else {
		LOGERR("textHtmlToDoc:: error: non charset exception\n");
		return false;
	    }
	}
    }

    m_metaData[cstr_dj_keyorigcharset] = result.get_charset();
    m_metaData[cstr_dj_keycontent] = result.dump;
    m_metaData[cstr_dj_keycharset] = cstr_utf8;
    // Avoid setting empty values which would crush ones possibly inherited
    // from parent (if we're an attachment)
    if (!result.dmtime.empty())
	m_metaData[cstr_dj_keymd] = result.dmtime;
    m_metaData[cstr_dj_keymt] = cstr_textplain;

    for (map<string,string>::const_iterator it = result.meta.begin(); 
	 it != result.meta.end(); it++) {
	if (!it->second.empty())
	    m_metaData[it->first] = it->second;
    }
    return true;
}

