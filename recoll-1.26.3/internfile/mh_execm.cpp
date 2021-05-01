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
#include <stdio.h>

#include <iostream>
#include <sstream>
using namespace std;

#include "cstr.h"
#include "mh_execm.h"
#include "mh_html.h"
#include "log.h"
#include "cancelcheck.h"
#include "smallut.h"
#include "md5ut.h"
#include "rclconfig.h"
#include "mimetype.h"
#include "idfile.h"

#include <sys/types.h>
#include "safesyswait.h"

bool MimeHandlerExecMultiple::startCmd()
{
    LOGDEB("MimeHandlerExecMultiple::startCmd\n");
    if (params.empty()) {
	// Hu ho
	LOGERR("MHExecMultiple::startCmd: empty params\n");
	m_reason = "RECFILTERROR BADCONFIG";
	return false;
    }

    // Command name
    string cmd = params.front();
    
    m_maxmemberkb = 50000;
    m_config->getConfParam("membermaxkbs", &m_maxmemberkb);
    ostringstream oss;
    oss << "RECOLL_FILTER_MAXMEMBERKB=" << m_maxmemberkb;
    m_cmd.putenv(oss.str());

    m_cmd.putenv("RECOLL_CONFDIR", m_config->getConfDir());
    m_cmd.putenv(m_forPreview ? "RECOLL_FILTER_FORPREVIEW=yes" :
		"RECOLL_FILTER_FORPREVIEW=no");

    m_cmd.setrlimit_as(m_filtermaxmbytes);
    m_adv.setmaxsecs(m_filtermaxseconds);
    m_cmd.setAdvise(&m_adv);

    // Build parameter list: delete cmd name
    vector<string>myparams(params.begin() + 1, params.end());

    if (m_cmd.startExec(cmd, myparams, 1, 1) < 0) {
        m_reason = string("RECFILTERROR HELPERNOTFOUND ") + cmd;
        missingHelper = true;
        return false;
    }
    return true;
}

// Note: data is not used if this is the "document:" field: it goes
// directly to m_metaData[cstr_dj_keycontent] to avoid an extra copy
// 
// Messages are made of data elements. Each element is like:
// name: len\ndata
// An empty line signals the end of the message, so the whole thing
// would look like:
// Name1: Len1\nData1Name2: Len2\nData2\n
bool MimeHandlerExecMultiple::readDataElement(string& name, string &data)
{
    string ibuf;

    // Read name and length
    if (m_cmd.getline(ibuf) <= 0) {
        LOGERR("MHExecMultiple: getline error\n");
        return false;
    }
    
    LOGDEB1("MHEM:rde: line [" << ibuf << "]\n");

    // Empty line (end of message) ?
    if (!ibuf.compare("\n")) {
        LOGDEB("MHExecMultiple: Got empty line\n");
        name.clear();
        return true;
    }

    // Filters will sometimes abort before entering the real protocol, ie if
    // a module can't be loaded. Check the special filter error first word:
    if (ibuf.find("RECFILTERROR ") == 0) {
        m_reason = ibuf;
        if (ibuf.find("HELPERNOTFOUND") != string::npos)
            missingHelper = true;
        return false;
    }

    // We're expecting something like Name: len\n
    vector<string> tokens;
    stringToTokens(ibuf, tokens);
    if (tokens.size() != 2) {
        LOGERR("MHExecMultiple: bad line in filter output: [" << ibuf << "]\n");
        return false;
    }
    vector<string>::iterator it = tokens.begin();
    name = *it++;
    string& slen = *it;
    int len;
    if (sscanf(slen.c_str(), "%d", &len) != 1) {
        LOGERR("MHExecMultiple: bad line in filter output: [" << ibuf << "]\n");
        return false;
    }

    if (len / 1024 > m_maxmemberkb) {
        LOGERR("MHExecMultiple: data len > maxmemberkb\n");
        return false;
    }
    
    // Hack: check for 'Document:' and read directly the document data
    // to m_metaData[cstr_dj_keycontent] to avoid an extra copy of the bulky
    // piece
    string *datap = &data;
    if (!stringlowercmp("document:", name)) {
        datap = &m_metaData[cstr_dj_keycontent];
    } else {
        datap = &data;
    }

    // Read element data
    datap->erase();
    if (len > 0 && m_cmd.receive(*datap, len) != len) {
        LOGERR("MHExecMultiple: expected " << len << " bytes of data, got " <<
               datap->length() << "\n");
        return false;
    }
    LOGDEB1("MHExecMe:rdDtElt got: name [" << name << "] len " << len <<
            "value [" << (datap->size() > 100 ? 
                          (datap->substr(0, 100) + " ...") : datap) << endl);
    return true;
}

bool MimeHandlerExecMultiple::next_document()
{
    LOGDEB("MimeHandlerExecMultiple::next_document(): [" << m_fn << "]\n");
    if (m_havedoc == false)
	return false;

    if (missingHelper) {
	LOGDEB("MHExecMultiple::next_document(): helper known missing\n");
	return false;
    }

    if (m_cmd.getChildPid() <= 0 && !startCmd()) {
        return false;
    }

    m_metaData.clear();
    
    // Send request to child process. This maybe the first/only
    // request for a given file, or a continuation request. We send an
    // empty file name in the latter case.
    // We also compute the file md5 before starting the extraction:
    // under Windows, we may not be able to do it while the file
    // is opened by the filter.
    ostringstream obuf;
    string file_md5;
    if (m_filefirst) {
	if (!m_forPreview && !m_nomd5) {
	    string md5, xmd5, reason;
	    if (MD5File(m_fn, md5, &reason)) {
		file_md5 = MD5HexPrint(md5, xmd5);
	    } else {
		LOGERR("MimeHandlerExecM: cant compute md5 for [" << m_fn <<
                       "]: " << reason << "\n");
	    }
	}
        obuf << "FileName: " << m_fn.length() << "\n" << m_fn;
        // m_filefirst is set to true by set_document_file()
        m_filefirst = false;
    } else {
        obuf << "Filename: " << 0 << "\n";
    }
    if (!m_ipath.empty()) {
	LOGDEB("next_doc: sending ipath " << m_ipath.length() << " val [" <<
               m_ipath << "]\n");
        obuf << "Ipath: " << m_ipath.length() << "\n" << m_ipath;
    }
    if (!m_dfltInputCharset.empty()) {
        obuf << "DflInCS: " << m_dfltInputCharset.length() << "\n" 
	     << m_dfltInputCharset;
    }
    obuf << "Mimetype: " << m_mimeType.length() << "\n" << m_mimeType;
    obuf << "\n";
    if (m_cmd.send(obuf.str()) < 0) {
        m_cmd.zapChild();
        LOGERR("MHExecMultiple: send error\n");
        return false;
    }

    m_adv.reset();

    // Read answer (multiple elements)
    LOGDEB1("MHExecMultiple: reading answer\n");
    bool eofnext_received = false;
    bool eofnow_received = false;
    bool fileerror_received = false;
    bool subdocerror_received = false;
    string ipath;
    string mtype;
    string charset;
    for (int loop=0;;loop++) {
        string name, data;
        try {
            if (!readDataElement(name, data)) {
                m_cmd.zapChild();
                return false;
            }
        } catch (HandlerTimeout) {
            LOGINFO("MHExecMultiple: timeout\n");
            m_cmd.zapChild();
            return false;
        } catch (CancelExcept) {
            LOGINFO("MHExecMultiple: interrupt\n");
            m_cmd.zapChild();
            return false;
        }
        if (name.empty())
            break;
        if (!stringlowercmp("eofnext:", name)) {
            LOGDEB("MHExecMultiple: got EOFNEXT\n");
            eofnext_received = true;
        } else if (!stringlowercmp("eofnow:", name)) {
            LOGDEB("MHExecMultiple: got EOFNOW\n");
            eofnow_received = true;
        } else if (!stringlowercmp("fileerror:", name)) {
            LOGDEB("MHExecMultiple: got FILEERROR\n");
	    fileerror_received = true;
        } else if (!stringlowercmp("subdocerror:", name)) {
            LOGDEB("MHExecMultiple: got SUBDOCERROR\n");
	    subdocerror_received = true;
        } else if (!stringlowercmp("ipath:", name)) {
            ipath = data;
            LOGDEB("MHExecMultiple: got ipath [" << data << "]\n");
        } else if (!stringlowercmp("charset:", name)) {
            charset = data;
            LOGDEB("MHExecMultiple: got charset [" << data << "]\n");
        } else if (!stringlowercmp("mimetype:", name)) {
            mtype = data;
            LOGDEB("MHExecMultiple: got mimetype [" << data << "]\n");
        } else {
            string nm = stringtolower((const string&)name);
            trimstring(nm, ":");
            LOGDEB("MHExecMultiple: got [" << nm << "] -> [" << data << "]\n");
            m_metaData[nm] += data;
        }
        if (loop == 200) {
            // ?? 
            LOGERR("MHExecMultiple: handler sent more than 200 attributes\n");
            return false;
        }
    }

    if (eofnow_received || fileerror_received) {
        // No more docs
        m_havedoc = false;
        return false;
    }
    if (subdocerror_received) {
	return false;
    }

    // It used to be that eof could be signalled just by an empty document, but
    // this was wrong. Empty documents can be found ie in zip files and should 
    // not be interpreted as eof.
    if (m_metaData[cstr_dj_keycontent].empty()) {
        LOGDEB0("MHExecMultiple: got empty document inside [" << m_fn <<
                "]: [" << ipath << "]\n");
    }

    if (!ipath.empty()) {
	// If this has an ipath, it is an internal doc from a
	// multi-document file. In this case, either the filter
	// supplies the mimetype, or the ipath MUST be a filename-like
	// string which we can use to compute a mime type
        m_metaData[cstr_dj_keyipath] = ipath;
        if (mtype.empty()) {
	    LOGDEB0("MHExecMultiple: no mime type from filter, using ipath "
                    "for a guess\n");
            mtype = mimetype(ipath, 0, m_config, false);
            if (mtype.empty()) {
                // mimetype() won't call idFile when there is no file. Do it
                mtype = idFileMem(m_metaData[cstr_dj_keycontent]);
                if (mtype.empty()) {
                    // Note this happens for example for directory zip members
                    // We could recognize them by the end /, but wouldn't know
                    // what to do with them anyway.
                    LOGINFO("MHExecMultiple: cant guess mime type\n");
                    mtype = "application/octet-stream";
                }
            }
        }
        m_metaData[cstr_dj_keymt] = mtype;
	if (!m_forPreview) {
	    string md5, xmd5;
	    MD5String(m_metaData[cstr_dj_keycontent], md5);
	    m_metaData[cstr_dj_keymd5] = MD5HexPrint(md5, xmd5);
	}
    } else {
	// "Self" document.
        m_metaData[cstr_dj_keymt] = mtype.empty() ? cstr_texthtml : mtype;
        m_metaData.erase(cstr_dj_keyipath);
	if (!m_forPreview) {
            m_metaData[cstr_dj_keymd5] = file_md5;
        }
    }

    handle_cs(m_metaData[cstr_dj_keymt], charset);

    if (eofnext_received)
        m_havedoc = false;

    LOGDEB0("MHExecMultiple: returning " <<
            m_metaData[cstr_dj_keycontent].size() <<
            " bytes of content, mtype [" << m_metaData[cstr_dj_keymt] <<
            "] charset [" << m_metaData[cstr_dj_keycharset] << "]\n");
    LOGDEB2("MHExecMultiple: metadata: \n" << metadataAsString());
    return true;
}

