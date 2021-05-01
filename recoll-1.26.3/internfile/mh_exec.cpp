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

#include <sys/types.h>
#include <time.h>
#include "safesyswait.h"

#include <list>

#include "cstr.h"
#include "execmd.h"
#include "mh_exec.h"
#include "mh_html.h"
#include "log.h"
#include "cancelcheck.h"
#include "smallut.h"
#include "md5ut.h"
#include "rclconfig.h"

using namespace std;

MEAdv::MEAdv(int maxsecs) 
  : m_filtermaxseconds(maxsecs) 
{
    m_start = time(0L);
}
void MEAdv::reset()
{
    m_start = time(0L);

}

void MEAdv::newData(int n) 
{
    LOGDEB2("MHExec:newData(" << n << ")\n");
    if (m_filtermaxseconds > 0 && 
        time(0L) - m_start > m_filtermaxseconds) {
        LOGERR("MimeHandlerExec: filter timeout (" << m_filtermaxseconds <<
               " S)\n");
        throw HandlerTimeout();
    }
    // If a cancel request was set by the signal handler (or by us
    // just above), this will raise an exception. Another approach
    // would be to call ExeCmd::setCancel().
    CancelCheck::instance().checkCancel();
}


MimeHandlerExec::MimeHandlerExec(RclConfig *cnf, const std::string& id)
    : RecollFilter(cnf, id), missingHelper(false), m_filtermaxseconds(900),
      m_filtermaxmbytes(0), m_handlernomd5(false), m_hnomd5init(false),
      m_nomd5(false)
{
    m_config->getConfParam("filtermaxseconds", &m_filtermaxseconds);
    m_config->getConfParam("filtermaxmbytes", &m_filtermaxmbytes);
}

bool MimeHandlerExec::set_document_file_impl(const std::string& mt, 
                                             const std::string &file_path)
{
    // Can't do this in constructor as script name not set yet. Do it
    // once on first call
    unordered_set<string> nomd5tps;
    bool tpsread(false);
    
    if (false == m_hnomd5init) {
        m_hnomd5init = true;
        if (m_config->getConfParam("nomd5types", &nomd5tps)) {
            tpsread = true;
            if (!nomd5tps.empty()) {
                if (params.size() &&
                    nomd5tps.find(path_getsimple(params[0])) !=
                    nomd5tps.end()) {
                    m_handlernomd5 = true;
                }
                // On windows the 1st param is often a script interp
                // name (e.g. "python", and the script name is 2nd
                if (params.size() > 1 &&
                    nomd5tps.find(path_getsimple(params[1])) !=
                    nomd5tps.end()) {
                    m_handlernomd5 = true;
                }
            }
        }
    }

    m_nomd5 = m_handlernomd5;

    if (!m_nomd5) {
        // Check for MIME type based md5 suppression
        if (!tpsread) {
            m_config->getConfParam("nomd5types", &nomd5tps);
        }
        if (nomd5tps.find(mt) != nomd5tps.end()) {
            m_nomd5 = true;
        }
    }
    
    m_fn = file_path;
    m_havedoc = true;
    return true;
}

bool MimeHandlerExec::skip_to_document(const string& ipath) 
{
    LOGDEB("MimeHandlerExec:skip_to_document: [" << ipath << "]\n");
    m_ipath = ipath;
    return true;
}

// Execute an external program to translate a file from its native
// format to text or html.
bool MimeHandlerExec::next_document()
{
    if (m_havedoc == false)
	return false;
    m_havedoc = false;
    if (missingHelper) {
	LOGDEB("MimeHandlerExec::next_document(): helper known missing\n");
	return false;
    }

    if (params.empty()) {
	// Hu ho
	LOGERR("MimeHandlerExec::next_document: empty params\n");
	m_reason = "RECFILTERROR BADCONFIG";
	return false;
    }

    // Command name
    string cmd = params.front();
    
    // Build parameter vector: delete cmd name and add the file name
    vector<string>myparams(params.begin() + 1, params.end());
    myparams.push_back(m_fn);
    if (!m_ipath.empty())
	myparams.push_back(m_ipath);

    // Execute command, store the output
    string& output = m_metaData[cstr_dj_keycontent];
    output.erase();
    ExecCmd mexec;
    MEAdv adv(m_filtermaxseconds);
    mexec.setAdvise(&adv);
    mexec.putenv("RECOLL_CONFDIR", m_config->getConfDir());
    mexec.putenv(m_forPreview ? "RECOLL_FILTER_FORPREVIEW=yes" :
                 "RECOLL_FILTER_FORPREVIEW=no");
    mexec.setrlimit_as(m_filtermaxmbytes);

    int status;
    try {
        status = mexec.doexec(cmd, myparams, 0, &output);
    } catch (HandlerTimeout) {
	LOGERR("MimeHandlerExec: handler timeout\n" );
        status = 0x110f;
    } catch (CancelExcept) {
	LOGERR("MimeHandlerExec: cancelled\n" );
        status = 0x110f;
    }

    if (status) {
	LOGERR("MimeHandlerExec: command status 0x" <<
               std::hex << status << std::dec << " for " << cmd << "\n");
	if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
	    // That's how execmd signals a failed exec (most probably
	    // a missing command). Let'hope no filter uses the same value as
	    // an exit status... Disable myself permanently and signal the 
	    // missing cmd.
	    missingHelper = true;
	    m_reason = string("RECFILTERROR HELPERNOTFOUND ") + cmd;
	} else if (output.find("RECFILTERROR") == 0) {
	    // If the output string begins with RECFILTERROR, then it's 
	    // interpretable error information out from a recoll script
	    m_reason = output;
	    list<string> lerr;
	    stringToStrings(output, lerr);
	    if (lerr.size() > 2) {
		list<string>::iterator it = lerr.begin();
		it++;
		if (*it == "HELPERNOTFOUND") {
		    // No use trying again and again to execute this filter, 
		    // it won't work.
		    missingHelper = true;
		}
	    }		    
	}
	return false;
    }

    finaldetails();
    return true;
}

void MimeHandlerExec::handle_cs(const string& mt, const string& icharset)
{
    string charset(icharset);

    // cfgFilterOutputCharset comes from the mimeconf filter
    // definition line and defaults to UTF-8 if empty. If the value is
    // "default", we use the default input charset value defined in
    // recoll.conf (which may vary depending on directory)
    if (charset.empty()) {
	charset = cfgFilterOutputCharset.empty() ? cstr_utf8 : 
	    cfgFilterOutputCharset;
	if (!stringlowercmp("default", charset)) {
	    charset = m_dfltInputCharset;
	}
    }
    m_metaData[cstr_dj_keyorigcharset] = charset;

    // If this is text/plain transcode_to/check utf-8
    if (!mt.compare(cstr_textplain)) {
	(void)txtdcode("mh_exec/m");
    } else {
	m_metaData[cstr_dj_keycharset] = charset;
    }
}

void MimeHandlerExec::finaldetails()
{
    // The default output mime type is html, but it may be defined
    // otherwise in the filter definition.
    m_metaData[cstr_dj_keymt] = cfgFilterOutputMtype.empty() ? cstr_texthtml : 
	cfgFilterOutputMtype;

    if (!m_forPreview && !m_nomd5) {
	string md5, xmd5, reason;
	if (MD5File(m_fn, md5, &reason)) {
	    m_metaData[cstr_dj_keymd5] = MD5HexPrint(md5, xmd5);
	} else {
	    LOGERR("MimeHandlerExec: cant compute md5 for [" << m_fn << "]: " <<
                   reason << "\n");
	}
    }

    handle_cs(m_metaData[cstr_dj_keymt]);
}

