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

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxslt/transform.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/xsltutils.h>

#include "cstr.h"
#include "mh_xslt.h"
#include "log.h"
#include "smallut.h"
#include "md5ut.h"
#include "rclconfig.h"
#include "readfile.h"

using namespace std;

// Do we need this? It would need to be called from recollinit
// Call once, not reentrant
// xmlInitParser();
// LIBXML_TEST_VERSION;
// Probably not:    xmlCleanupParser();
        

class FileScanXML : public FileScanDo {
public:
    FileScanXML(const string& fn) : m_fn(fn) {}
    virtual ~FileScanXML() {
        if (ctxt) {
            xmlFreeParserCtxt(ctxt);
        }
    }

    xmlDocPtr getDoc() {
        int ret;
        if ((ret = xmlParseChunk(ctxt, nullptr, 0, 1))) {
            xmlError *error = xmlGetLastError();
            LOGERR("FileScanXML: final xmlParseChunk failed with error " <<
                   ret << " error: " <<
                   (error ? error->message :
                    " null return from xmlGetLastError()") << "\n");
            return nullptr;
        }
        return ctxt->myDoc;
    }

    virtual bool init(int64_t size, string *) {
        LOGDEB1("FileScanXML: init: size " << size << endl);
        ctxt = xmlCreatePushParserCtxt(NULL, NULL, NULL, 0, m_fn.c_str());
        if (ctxt == nullptr) {
            LOGERR("FileScanXML: xmlCreatePushParserCtxt failed\n");
            return false;
        } else {
            return true;
        }
    }
    
    virtual bool data(const char *buf, int cnt, string*) {
        if (0) {
            string dt(buf, cnt);
            LOGDEB1("FileScanXML: data: cnt " << cnt << " data " << dt << endl);
        } else {
            LOGDEB1("FileScanXML: data: cnt " << cnt << endl);
        }            
        int ret;
        if ((ret = xmlParseChunk(ctxt, buf, cnt, 0))) {
            xmlError *error = xmlGetLastError();
            LOGERR("FileScanXML: xmlParseChunk failed with error " <<
                   ret << " for [" << buf << "] error " <<
                   (error ? error->message :
                    " null return from xmlGetLastError()") << "\n");
            return false;
        } else {
            LOGDEB1("xmlParseChunk ok (sent " << cnt << " bytes)\n");
            return true;
        }
    }

private:
    xmlParserCtxtPtr ctxt{nullptr};
    string m_fn;
};

class MimeHandlerXslt::Internal {
public:
    Internal(MimeHandlerXslt *_p)
        : p(_p) {}
    ~Internal() {
        if (metaOrAllSS) {
            xsltFreeStylesheet(metaOrAllSS);
        }
        if (bodySS) {
            xsltFreeStylesheet(bodySS);
        }
    }

    xsltStylesheet *prepare_stylesheet(const string& ssnm);
    bool process_doc_or_string(bool forpv, const string& fn, const string& data);
    bool apply_stylesheet(
        const string& fn, const string& member, const string& data,
        xsltStylesheet *ssp, string& result, string *md5p);

    MimeHandlerXslt *p;
    bool ok{false};
    string metamember;
    xsltStylesheet *metaOrAllSS{nullptr};
    string bodymember;
    xsltStylesheet *bodySS{nullptr};
    string result;
    string filtersdir;
};

MimeHandlerXslt::~MimeHandlerXslt()
{
    delete m;
}

MimeHandlerXslt::MimeHandlerXslt(RclConfig *cnf, const std::string& id,
                                 const std::vector<std::string>& params)
    : RecollFilter(cnf, id), m(new Internal(this))
{
    LOGDEB("MimeHandlerXslt: params: " << stringsToString(params) << endl);
    m->filtersdir = path_cat(cnf->getDatadir(), "filters");

    xmlSubstituteEntitiesDefault(0);
    xmlLoadExtDtdDefaultValue = 0;

    // params can be "xslt stylesheetall" or
    // "xslt metamember metastylesheet bodymember bodystylesheet"
    if (params.size() == 2) {
        m->metaOrAllSS = m->prepare_stylesheet(params[1]);
        if (m->metaOrAllSS) {
            m->ok = true;
        }
    } else if (params.size() == 5) {
        m->metamember = params[1];
        m->metaOrAllSS = m->prepare_stylesheet(params[2]);
        m->bodymember = params[3];
        m->bodySS =  m->prepare_stylesheet(params[4]);
        if (m->metaOrAllSS && m->bodySS) {
            m->ok = true;
        }
    } else {
        LOGERR("MimeHandlerXslt: constructor with wrong param vector: " <<
               stringsToString(params) << endl);
    }
}

xsltStylesheet *MimeHandlerXslt::Internal::prepare_stylesheet(const string& ssnm)
{
    string ssfn = path_cat(filtersdir, ssnm);
    FileScanXML XMLstyle(ssfn);
    string reason;
    if (!file_scan(ssfn, &XMLstyle, &reason)) {
        LOGERR("MimeHandlerXslt: file_scan failed for style sheet " <<
               ssfn << " : " << reason << endl);
        return nullptr;
    }
    xmlDoc *stl = XMLstyle.getDoc();
    if (stl == nullptr) {
        LOGERR("MimeHandlerXslt: getDoc failed for style sheet " <<
               ssfn << endl);
        return nullptr;
    }
    return xsltParseStylesheetDoc(stl);
}

bool MimeHandlerXslt::Internal::apply_stylesheet(
    const string& fn, const string& member, const string& data,
    xsltStylesheet *ssp, string& result, string *md5p)
{
    FileScanXML XMLdoc(fn);
    string md5, reason;
    bool res;
    if (!fn.empty()) {
        if (member.empty()) {
            res = file_scan(fn, &XMLdoc, 0, -1, &reason, md5p);
        } else {
            res = file_scan(fn, member, &XMLdoc, &reason);
        }
    } else {
        if (member.empty()) {
            res = string_scan(data.c_str(), data.size(), &XMLdoc, &reason, md5p);
        } else {
            res = string_scan(data.c_str(), data.size(), member, &XMLdoc,
                                                      &reason);
        }
    }
    if (!res) {
        LOGERR("MimeHandlerXslt::set_document_: file_scan failed for "<<
               fn << " " << member << " : " << reason << endl);
        return false;
    }

    xmlDocPtr doc = XMLdoc.getDoc();
    if (nullptr == doc) {
        LOGERR("MimeHandlerXslt::set_document_: no parsed doc\n");
        return false;
    }
    xmlDocPtr transformed = xsltApplyStylesheet(ssp, doc, NULL);
    if (nullptr == transformed) {
        LOGERR("MimeHandlerXslt::set_document_: xslt transform failed\n");
        xmlFreeDoc(doc);
        return false;
    }
    xmlChar *outstr;
    int outlen;
    xsltSaveResultToString(&outstr, &outlen, transformed, metaOrAllSS);
    result = string((const char*)outstr, outlen);
    xmlFree(outstr);
    xmlFreeDoc(transformed);
    xmlFreeDoc(doc);
    return true;
}

bool MimeHandlerXslt::Internal::process_doc_or_string(
    bool forpreview, const string& fn, const string& data)
{
    if (nullptr == metaOrAllSS && nullptr == bodySS) {
        LOGERR("MimeHandlerXslt::set_document_file_impl: both ss empty??\n");
        return false;
    }
    p->m_metaData[cstr_dj_keycharset] = cstr_utf8;
    if (nullptr == bodySS) {
        string md5;
        if (apply_stylesheet(fn, string(), data, metaOrAllSS, result,
                             forpreview ? nullptr : &md5)) {
            if (!forpreview) {
                p->m_metaData[cstr_dj_keymd5] = md5;
            }
            return true;
        }
        return false;
    } else {
        result = "<html>\n<head>\n<meta http-equiv=\"Content-Type\""
            "content=\"text/html; charset=UTF-8\">";
        string part;
        if (!apply_stylesheet(fn,metamember, data, metaOrAllSS, part, nullptr)) {
            return false;
        }
        result += part;
        result += "</head>\n<body>\n";
        if (!apply_stylesheet(fn, bodymember, data, bodySS, part, nullptr)) {
            return false;
        }
        result += part;
        result += "</body></html>";
    }
    return true;
}

bool MimeHandlerXslt::set_document_file_impl(const std::string& mt, 
                                             const std::string &fn)
{
    LOGDEB0("MimeHandlerXslt::set_document_file_: fn: " << fn << endl);
    if (!m || !m->ok) {
        return false;
    }
    bool ret = m->process_doc_or_string(m_forPreview, fn, string());
    if (ret) {
        m_havedoc = true;
    }
    return ret;
}

bool MimeHandlerXslt::set_document_string_impl(const string& mt, 
                                               const string& txt)
{
    LOGDEB0("MimeHandlerXslt::set_document_string_\n");
    if (!m || !m->ok) {
        return false;
    }
    bool ret = m->process_doc_or_string(m_forPreview, string(), txt);
    if (ret) {
        m_havedoc = true;
    }
    return ret;
}

bool MimeHandlerXslt::next_document()
{
    if (!m || !m->ok) {
        return false;
    }
    if (m_havedoc == false)
	return false;
    m_havedoc = false;
    m_metaData[cstr_dj_keymt] = cstr_texthtml;
    m_metaData[cstr_dj_keycontent].swap(m->result);
    LOGDEB1("MimeHandlerXslt::next_document: result: [" <<
            m_metaData[cstr_dj_keycontent] << "]\n");
    return true;
}

void MimeHandlerXslt::clear_impl()
{
    m_havedoc = false;
    m->result.clear();
}
