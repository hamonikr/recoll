/* Copyright (C) 2004-2019 J.F.Dockes 
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
#include <stdint.h>
#include "safefcntl.h"
#include <sys/types.h>
#include "safesysstat.h"
#include "safeunistd.h"

#include <string>
#include <iostream>
#include <map>
#include <memory>

using namespace std;

#include "cstr.h"
#include "internfile.h"
#include "rcldoc.h"
#include "mimetype.h"
#include "log.h"
#include "mimehandler.h"
#include "execmd.h"
#include "pathut.h"
#include "rclconfig.h"
#include "mh_html.h"
#include "fileudi.h"
#include "cancelcheck.h"
#include "copyfile.h"
#include "fetcher.h"
#include "extrameta.h"
#include "uncomp.h"

// The internal path element separator. This can't be the same as the rcldb 
// file to ipath separator : "|"
// We replace it with a control char if it comes out of a filter (ie:
// rclzip or rclchm can do this). If you want the SOH control char
// inside an ipath, you're out of luck (and a bit weird).
static const string cstr_isep(":");

static const char cchar_colon_repl = '\x01';
static string colon_hide(const string& in)
{
    string out;
    for (string::const_iterator it = in.begin(); it != in.end(); it++) {
        out += *it == ':' ? cchar_colon_repl : *it;
    }
    return out;
}
static string colon_restore(const string& in)
{
    string out;
    for (string::const_iterator it = in.begin(); it != in.end(); it++) {
        out += *it == cchar_colon_repl ? ':' : *it;
    }
    return out;
}

// This is used when the user wants to retrieve a search result doc's parent
// (ie message having a given attachment)
bool FileInterner::getEnclosingUDI(const Rcl::Doc &doc, string& udi)
{
    LOGDEB("FileInterner::getEnclosingUDI(): url [" << doc.url <<
           "] ipath [" << doc.ipath << "]\n");
    string eipath = doc.ipath;
    string::size_type colon;
    if (eipath.empty())
        return false;
    if ((colon =  eipath.find_last_of(cstr_isep)) != string::npos) {
        eipath.erase(colon);
    } else {
        eipath.erase();
    }
    
    make_udi(url_gpath(doc.idxurl.empty() ? doc.url : doc.idxurl), eipath, udi);
    return true;
}

string FileInterner::getLastIpathElt(const string& ipath)
{
    string::size_type sep;
    if ((sep =  ipath.find_last_of(cstr_isep)) != string::npos) {
        return ipath.substr(sep + 1);
    } else {
        return ipath;
    }
}

bool FileInterner::ipathContains(const string& parent, const string& child)
{
    return child.find(parent) == 0 &&
        child.find(cstr_isep, parent.size()) == parent.size();
}

// Constructor: identify the input file, possibly create an
// uncompressed temporary copy, and create the top filter for the
// uncompressed file type.
//
// Empty handler on return says that we're in error, this will be
// processed by the first call to internfile().
// Split into "constructor calls init()" to allow use from other constructor
FileInterner::FileInterner(const string &fn, const struct stat *stp,
                           RclConfig *cnf, int flags, const string *imime)
{
    LOGDEB0("FileInterner::FileInterner(fn=" << fn << ")\n");
    if (fn.empty()) {
        LOGERR("FileInterner::FileInterner: empty file name!\n");
        return;
    }
    initcommon(cnf, flags);
    init(fn, stp, cnf, flags, imime);
}

// Note that we always succeed (set m_ok = true), except in internal
// inconsistency cases (which could just as well abort()).  Errors
// will be detected when internfile() is called. This is done so that
// our caller creates a doc record in all cases (with an error-flagged
// signature), so that the appropriate retry choices can be made. This
// used to not be the case, and was changed because this was the
// simplest way to solve the retry issues (simpler than changing the
// caller in e.g. fsindexer).
void FileInterner::init(const string &f, const struct stat *stp, RclConfig *cnf,
                        int flags, const string *imime)
{
    if (f.empty()) {
        LOGERR("FileInterner::init: empty file name!\n");
        return;
    }
    m_fn = f;

    // Compute udi for the input file. This is used by filters which
    // manage some kind of cache.  Indexing by udi makes things easier
    // because they sometimes get a temp as actual input.
    string udi;
    make_udi(f, cstr_null, udi);

    cnf->setKeyDir(path_getfather(m_fn));

    string l_mime;
    bool usfci = false;
    cnf->getConfParam("usesystemfilecommand", &usfci);

    // In general, even when the input mime type is set (when
    // previewing), we can't use it: it's the type for the actual
    // document, but this can be part of a compound document, and
    // we're dealing with the top level file here, or this could be a
    // compressed file. The flag tells us we really can use it
    // (e.g. the web indexer sets it).
    if (flags & FIF_doUseInputMimetype) {
        if (!imime) {
            LOGERR("FileInterner:: told to use null imime\n");
            return;
        }
        l_mime = *imime;
    } else {
        LOGDEB("FileInterner::init fn [" << f << "] mime [" <<
               (imime ? imime->c_str() : "(null)") << "] preview " <<
               m_forPreview << "\n");

        // Run mime type identification in any case (see comment above).
        l_mime = mimetype(m_fn, stp, m_cfg, usfci);

        // If identification fails, try to use the input parameter. This
        // is then normally not a compressed type (it's the mime type from
        // the db), and is only set when previewing, not for indexing
        if (l_mime.empty() && imime)
            l_mime = *imime;
    }

    int64_t docsize = stp->st_size;

    if (!l_mime.empty()) {
        // Has mime: check for a compressed file. If so, create a
        // temporary uncompressed file, and rerun the mime type
        // identification, then do the rest with the temp file.
        vector<string>ucmd;
        if (m_cfg->getUncompressor(l_mime, ucmd)) {
            // Check for compressed size limit
            int maxkbs = -1;
            if (!m_cfg->getConfParam("compressedfilemaxkbs", &maxkbs) ||
                maxkbs < 0 || !stp || int(stp->st_size / 1024) < maxkbs) {
                if (!m_uncomp->uncompressfile(m_fn, ucmd, m_tfile)) {
                    m_ok = true;
                    return;
                }
                LOGDEB1("FileInterner:: after ucomp: tfile " << m_tfile <<"\n");
                m_fn = m_tfile;
                // Stat the uncompressed file, mainly to get the size
                struct stat ucstat;
                if (path_fileprops(m_fn, &ucstat) != 0) {
                    LOGERR("FileInterner: can't stat the uncompressed file[" <<
                           m_fn << "] errno " << errno << "\n");
                    m_ok = true;
                    return;
                } else {
                    docsize = ucstat.st_size;
                }
                l_mime = mimetype(m_fn, &ucstat, m_cfg, usfci);
                if (l_mime.empty() && imime)
                    l_mime = *imime;
            } else {
                LOGINFO("FileInterner:: " << m_fn << " over size limit " <<
                        maxkbs << " kbs\n");
            }
        }
    }

    if (l_mime.empty()) {
        // No mime type. We let it through as config may warrant that
        // we index all file names
        LOGDEB0("FileInterner:: no mime: [" << m_fn << "]\n");
    }

    // Get fields computed from extended attributes. We use the
    // original file, not the m_fn which may be the uncompressed temp
    // file
    if (!m_noxattrs)
        reapXAttrs(m_cfg, f, m_XAttrsFields);

    // Gather metadata from external commands as configured.
    reapMetaCmds(m_cfg, f, m_cmdFields);

    m_mimetype = l_mime;

    // Look for appropriate handler (might still return empty)
    RecollFilter *df = getMimeHandler(l_mime, m_cfg, !m_forPreview);

    if (!df || df->is_unknown()) {
        // No real handler for this type, for now :( 
        LOGDEB("FileInterner:: unprocessed mime: [" << l_mime << "] [" << f <<
               "]\n");
        if (!df)
            return;
    }
    df->set_property(Dijon::Filter::OPERATING_MODE, 
                     m_forPreview ? "view" : "index");
    df->set_property(Dijon::Filter::DJF_UDI, udi);

    df->set_docsize(docsize);
    // Don't process init errors here: doing it later allows indexing
    // the file name of even a totally unparsable file
    (void)df->set_document_file(l_mime, m_fn);
    m_handlers.push_back(df);
    LOGDEB("FileInterner:: init ok " << l_mime << " [" << m_fn << "]\n");
    m_ok = true;
}

// Setup from memory data (ie: out of the web cache). imime needs to be set.
FileInterner::FileInterner(const string &data, RclConfig *cnf, 
                           int flags, const string& imime)
{
    LOGDEB0("FileInterner::FileInterner(data)\n");
    initcommon(cnf, flags);
    init(data, cnf, flags, imime);
}

void FileInterner::init(const string &data, RclConfig *cnf, 
                        int flags, const string& imime)
{
    if (imime.empty()) {
        LOGERR("FileInterner: inmemory constructor needs input mime type\n");
        return;
    }
    m_mimetype = imime;

    // Look for appropriate handler (might still return empty)
    RecollFilter *df = getMimeHandler(m_mimetype, m_cfg, !m_forPreview);

    if (!df) {
        // No handler for this type, for now :( if indexallfilenames
        // is set in the config, this normally wont happen (we get mh_unknown)
        LOGDEB("FileInterner:: unprocessed mime [" << m_mimetype << "]\n");
        return;
    }
    df->set_property(Dijon::Filter::OPERATING_MODE, 
                     m_forPreview ? "view" : "index");

    df->set_docsize(data.length());
    if (df->is_data_input_ok(Dijon::Filter::DOCUMENT_STRING)) {
        (void)df->set_document_string(m_mimetype, data);
    } else if (df->is_data_input_ok(Dijon::Filter::DOCUMENT_DATA)) {
        (void)df->set_document_data(m_mimetype, data.c_str(), data.length());
    } else if (df->is_data_input_ok(Dijon::Filter::DOCUMENT_FILE_NAME)) {
        TempFile temp = dataToTempFile(data, m_mimetype);
        if (temp.ok()) {
            (void)df->set_document_file(m_mimetype, temp.filename());
            m_tmpflgs[m_handlers.size()] = true;
            m_tempfiles.push_back(temp);
        }
    }
    // Don't process init errors here: doing it later allows indexing
    // the file name of even a totally unparsable file
    m_handlers.push_back(df);
    m_ok = true;
}

void FileInterner::initcommon(RclConfig *cnf, int flags)
{
    m_cfg = cnf;
    m_forPreview = ((flags & FIF_forPreview) != 0);
    m_uncomp = new Uncomp(m_forPreview);
    // Initialize handler stack.
    m_handlers.reserve(MAXHANDLERS);
    for (unsigned int i = 0; i < MAXHANDLERS; i++)
        m_tmpflgs[i] = false;
    m_targetMType = cstr_textplain;
    m_cfg->getConfParam("noxattrfields", &m_noxattrs);
    m_direct = false;
}

FileInterner::FileInterner(const Rcl::Doc& idoc, RclConfig *cnf, int flags)
{
    LOGDEB0("FileInterner::FileInterner(idoc)\n");
    initcommon(cnf, flags);

    std::unique_ptr<DocFetcher> fetcher(docFetcherMake(cnf, idoc));
    if (!fetcher) {
        LOGERR("FileInterner:: no backend\n");
        return;
    }
    DocFetcher::RawDoc rawdoc;
    if (!fetcher->fetch(cnf, idoc, rawdoc)) {
        LOGERR("FileInterner:: fetcher failed\n");
        return;
    }
    switch (rawdoc.kind) {
    case DocFetcher::RawDoc::RDK_FILENAME:
        init(rawdoc.data, &rawdoc.st, cnf, flags, &idoc.mimetype);
        break;
    case DocFetcher::RawDoc::RDK_DATA:
        init(rawdoc.data, cnf, flags, idoc.mimetype);
        break;
    case DocFetcher::RawDoc::RDK_DATADIRECT:
        // Note: only used for demo with the sample python external
        // mbox indexer at this point. The external program is
        // responsible for all the extraction process.
        init(rawdoc.data, cnf, flags, idoc.mimetype);
        m_direct = true;
        break;
    default:
        LOGERR("FileInterner::FileInterner(idoc): bad rawdoc kind ??\n");
    }
    return;
}

FileInterner::ErrorPossibleCause FileInterner::tryGetReason(RclConfig *cnf,
                                                            const Rcl::Doc& idoc)
{
    LOGDEB0("FileInterner::tryGetReason(idoc)\n");

    std::unique_ptr<DocFetcher> fetcher(docFetcherMake(cnf, idoc));
    if (!fetcher) {
        LOGERR("FileInterner:: no backend\n");
        return FileInterner::FetchNoBackend;
    }
    DocFetcher::Reason fetchreason = fetcher->testAccess(cnf, idoc);
    switch (fetchreason) {
    case DocFetcher::FetchNotExist: return FileInterner::FetchMissing;
    case DocFetcher::FetchNoPerm: return FileInterner::FetchPerm;
    default: return FileInterner::InternfileOther;
    }
}

bool FileInterner::makesig(RclConfig *cnf, const Rcl::Doc& idoc, string& sig)
{
    std::unique_ptr<DocFetcher> fetcher(docFetcherMake(cnf, idoc));
    if (!fetcher) {
        LOGERR("FileInterner::makesig no backend for doc\n");
        return false;
    }

    bool ret = fetcher->makesig(cnf, idoc, sig);
    return ret;
}

FileInterner::~FileInterner()
{
    for (auto& entry: m_handlers) {
        returnMimeHandler(entry);
    }
    delete m_uncomp;
    // m_tempfiles will take care of itself
}

// Create a temporary file for a block of data (ie: attachment) found
// while walking the internal document tree, with a type for which the
// handler needs an actual file (ie : external script).
TempFile FileInterner::dataToTempFile(const string& dt, const string& mt)
{
    // Create temp file with appropriate suffix for mime type
    TempFile temp(m_cfg->getSuffixFromMimeType(mt));
    if (!temp.ok()) {
        LOGERR("FileInterner::dataToTempFile: cant create tempfile: " <<
               temp.getreason() << "\n");
        return TempFile();
    }
    string reason;
    if (!stringtofile(dt, temp.filename(), reason)) {
        LOGERR("FileInterner::dataToTempFile: stringtofile: " <<reason << "\n");
        return TempFile();
    }
    return temp;
}

// See if the error string is formatted as a missing helper message,
// accumulate helper name if it is. The format of the message is:
// RECFILTERROR HELPERNOTFOUND program1 [program2 ...]
void FileInterner::checkExternalMissing(const string& msg, const string& mt)
{
    LOGDEB2("checkExternalMissing: [" << msg << "]\n");
    if (m_missingdatap && msg.find("RECFILTERROR") == 0) {
        vector<string> verr;
        stringToStrings(msg, verr);
        if (verr.size() > 2) {
            vector<string>::iterator it = verr.begin();
            it++;
            if (*it == "HELPERNOTFOUND") {
                it++;
                for (; it != verr.end(); it++) {
                    m_missingdatap->addMissing(*it, mt);
                }
            }
        }                   
    }
}

void FIMissingStore::getMissingExternal(string& out) 
{
    for (map<string, set<string> >::const_iterator it = 
             m_typesForMissing.begin(); it != m_typesForMissing.end(); it++) {
        out += string(" ") + it->first;
    }
    trimstring(out);
}

void FIMissingStore::getMissingDescription(string& out)
{
    out.erase();

    for (map<string, set<string> >::const_iterator it = 
             m_typesForMissing.begin(); it != m_typesForMissing.end(); it++) {
        out += it->first + " (";
        set<string>::const_iterator it3;
        for (it3 = it->second.begin(); 
             it3 != it->second.end(); it3++) {
            out += *it3 + " ";
        }
        trimstring(out);
        out += ")";
        out += "\n";
    }
}

FIMissingStore::FIMissingStore(const string& in)
{
    // The "missing" file is text. Each line defines a missing filter
    // and the list of mime types actually encountered that needed it
    // (see method getMissingDescription())

    vector<string> lines;
    stringToTokens(in, lines, "\n");

    for (vector<string>::const_iterator it = lines.begin();
         it != lines.end(); it++) {
        // Lines from the file are like: 
        //
        // filter name string (mime1 mime2) 
        //
        // We can't be too sure that there will never be a parenthesis
        // inside the filter string as this comes from the filter
        // itself. The list part is safer, so we start from the end.
        const string& line = *it;
        string::size_type lastopen = line.find_last_of("(");
        if (lastopen == string::npos)
            continue;
        string::size_type lastclose = line.find_last_of(")");
        if (lastclose == string::npos || lastclose <= lastopen + 1)
            continue;
        string smtypes = line.substr(lastopen+1, lastclose - lastopen - 1);
        vector<string> mtypes;
        stringToTokens(smtypes, mtypes);
        string filter = line.substr(0, lastopen);
        trimstring(filter);
        if (filter.empty())
            continue;

        for (vector<string>::const_iterator itt = mtypes.begin(); 
             itt != mtypes.end(); itt++) {
            m_typesForMissing[filter].insert(*itt);
        }
    }
}

// Helper for extracting a value from a map.
static inline bool getKeyValue(const map<string, string>& docdata, 
                               const string& key, string& value)
{
    auto it = docdata.find(key);
    if (it != docdata.end()) {
        value = it->second;
        LOGDEB2("getKeyValue: [" << key << "]->[" << value << "]\n");
        return true;
    }
    LOGDEB2("getKeyValue: no value for [" << key << "]\n");
    return false;
}

// Copy most metadata fields from the top filter to the recoll
// doc. Some fields need special processing, because they go into
// struct fields instead of metadata entry, or because we don't want
// to copy them.
bool FileInterner::dijontorcl(Rcl::Doc& doc)
{
    RecollFilter *df = m_handlers.back();
    if (df == 0) {
        //??
        LOGERR("FileInterner::dijontorcl: null top handler ??\n");
        return false;
    }
    for (const auto& ent :  df->get_meta_data()) {
        if (ent.first == cstr_dj_keycontent) {
            doc.text = ent.second;
            if (doc.fbytes.empty()) {
                // It's normally set by walking the filter stack, in
                // collectIpathAndMt, which was called before us.  It
                // can happen that the doc size is still empty at this
                // point if the last container filter is directly
                // returning text/plain content, so that there is no
                // ipath-less filter at the top
                lltodecstr(doc.text.length(), doc.fbytes);
                LOGDEB("FileInterner::dijontorcl: fbytes->" << doc.fbytes <<
                       endl);
            }
        } else if (ent.first == cstr_dj_keymd) {
            doc.dmtime = ent.second;
        } else if (ent.first == cstr_dj_keyanc) {
            doc.haschildren = true;
        } else if (ent.first == cstr_dj_keyorigcharset) {
            doc.origcharset = ent.second;
        } else if (ent.first == cstr_dj_keyfn) {
            // Only if not set during the stack walk
            const string *fnp = 0;
            if (!doc.peekmeta(Rcl::Doc::keyfn, &fnp) || fnp->empty())
                doc.meta[Rcl::Doc::keyfn] = ent.second;
        } else if (ent.first == cstr_dj_keymt || 
                   ent.first == cstr_dj_keycharset) {
            // don't need/want these.
        } else {
            LOGDEB2("dijontorcl: " << m_cfg->fieldCanon(ent.first) << " -> " <<
                    ent.second << endl);
            doc.addmeta(m_cfg->fieldCanon(ent.first), ent.second);
        }
    }
    if (doc.meta[Rcl::Doc::keyabs].empty() && 
        !doc.meta[cstr_dj_keyds].empty()) {
        doc.meta[Rcl::Doc::keyabs] = doc.meta[cstr_dj_keyds];
        doc.meta.erase(cstr_dj_keyds);
    }
    return true;
}

const set<string> nocopyfields{cstr_dj_keycontent, cstr_dj_keymd,
        cstr_dj_keyanc, cstr_dj_keyorigcharset, cstr_dj_keyfn,
        cstr_dj_keymt, cstr_dj_keycharset, cstr_dj_keyds};

static void copymeta(const RclConfig *cfg,Rcl::Doc& doc, const RecollFilter* hp)
{
    for (const auto& entry : hp->get_meta_data()) {
        if (nocopyfields.find(entry.first) == nocopyfields.end()) {
            doc.addmeta(cfg->fieldCanon(entry.first), entry.second);
        }
    }
}


// Collect the ipath from the filter stack.
// While we're at it, we also set the mimetype and filename,
// which are special properties: we want to get them from the topmost
// doc with an ipath, not the last one which is usually text/plain We
// also set the author and modification time from the last doc which
// has them.
// 
// The stack can contain objects with an ipath element (corresponding
// to actual embedded documents), and, towards the top, elements
// without an ipath element, for format translations of the last doc.
//
// The docsize is fetched from the first element without an ipath
// (first non container). If the last element directly returns
// text/plain so that there is no ipath-less element, the value will
// be set in dijontorcl(). 
// 
// The whole thing is a bit messy but it's not obvious how it should
// be cleaned up as the "inheritance" rules inside the stack are
// actually complicated.
void FileInterner::collectIpathAndMT(Rcl::Doc& doc) const
{
    LOGDEB2("FileInterner::collectIpathAndMT\n");

    // Set to true if any element in the stack sets an ipath. (at least one of
    // the docs is a compound).
    bool hasipath = false;

    if (!m_noxattrs) {
        docFieldsFromXattrs(m_cfg, m_XAttrsFields, doc);
    }

    docFieldsFromMetaCmds(m_cfg, m_cmdFields, doc);

    // If there is no ipath stack, the mimetype is the one from the
    // file, else we'll change it further down.
    doc.mimetype = m_mimetype;

    string pathelprev;
    for (unsigned int i = 0; i < m_handlers.size(); i++) {
        const map<string, string>& docdata = m_handlers[i]->get_meta_data();
        string ipathel;
        getKeyValue(docdata, cstr_dj_keyipath, ipathel);
        if (!ipathel.empty()) {
            // Non-empty ipath. This stack element is for an
            // actual embedded document, not a format translation.
            hasipath = true;
            doc.ipath += colon_hide(ipathel) + cstr_isep;
            getKeyValue(docdata, cstr_dj_keymt, doc.mimetype);
            getKeyValue(docdata, cstr_dj_keyfn, doc.meta[Rcl::Doc::keyfn]);
        } else {
            // We copy all the metadata from the topmost actual
            // document: either the first if it has no ipath, or the
            // last one with an ipath (before pure format
            // translations). This would allow, for example mh_execm
            // handlers to use setfield() instead of embedding
            // metadata in the HTML meta tags.
            if (i == 0 || !pathelprev.empty()) {
                copymeta(m_cfg, doc, m_handlers[i]);
            }
            if (doc.fbytes.empty()) {
                lltodecstr(m_handlers[i]->get_docsize(), doc.fbytes);
                LOGDEB("collectIpath..: fbytes->" << doc.fbytes << endl);
            }
        }
        // We set the author field from the innermost doc which has
        // one: allows finding, e.g. an image attachment having no
        // metadata by a search on the sender name. Only do this for
        // actually embedded documents (avoid replacing values from
        // metacmds for the topmost one). For a topmost doc, author
        // will be merged by dijontorcl() later on. About same for
        // dmtime, but an external value will be replaced, not
        // augmented if dijontorcl() finds an internal value.
        if (hasipath) {
            getKeyValue(docdata, cstr_dj_keyauthor, doc.meta[Rcl::Doc::keyau]);
            getKeyValue(docdata, cstr_dj_keymd, doc.dmtime);
        }
        pathelprev = ipathel;
    }

    if (hasipath) {
        // Trim ending ipath separator
        LOGDEB2("IPATH [" << doc.ipath << "]\n");
        if (doc.ipath.back() ==  cstr_isep[0]) {
            doc.ipath.erase(doc.ipath.end()-1);
        }
    }
}

// Remove handler from stack. Clean up temp file if needed.
void FileInterner::popHandler()
{
    if (m_handlers.empty())
        return;
    size_t i = m_handlers.size() - 1;
    if (m_tmpflgs[i]) {
        m_tempfiles.pop_back();
        m_tmpflgs[i] = false;
    }
    returnMimeHandler(m_handlers.back());
    m_handlers.pop_back();
}

enum addResols {ADD_OK, ADD_CONTINUE, ADD_BREAK, ADD_ERROR};

// Just got document from current top handler. See what type it is,
// and possibly add a filter/handler to the stack
int FileInterner::addHandler()
{
    const map<string, string>& docdata = m_handlers.back()->get_meta_data();
    string charset, mimetype;
    getKeyValue(docdata, cstr_dj_keycharset, charset);
    getKeyValue(docdata, cstr_dj_keymt, mimetype);

    LOGDEB("FileInterner::addHandler: back()  is " << mimetype <<
           " target [" << m_targetMType << "]\n");

    // If we find a document of the target type (text/plain in
    // general), we're done decoding. If we hit text/plain, we're done
    // in any case
    if (!stringicmp(mimetype, m_targetMType) || 
        !stringicmp(mimetype, cstr_textplain)) {
        m_reachedMType = mimetype;
        LOGDEB1("FileInterner::addHandler: target reached\n");
        return ADD_BREAK;
    }

    // We need to stack another handler. Check stack size
    if (m_handlers.size() >= MAXHANDLERS) {
        // Stack too big. Skip this and go on to check if there is
        // something else in the current back()
        LOGERR("FileInterner::addHandler: stack too high\n");
        return ADD_CONTINUE;
    }

    // We must not filter out HTML when it is an intermediate
    // conversion format. We discriminate between e.g. an HTML email
    // attachment (needs filtering) and a result of pdf conversion
    // (must process) by looking at the last ipath element: a
    // conversion will have an empty one (same test as in
    // collectIpathAndMT).
    string ipathel;
    getKeyValue(docdata, cstr_dj_keyipath, ipathel);
    bool dofilter = !m_forPreview &&
        (mimetype.compare(cstr_texthtml) || !ipathel.empty());
    RecollFilter *newflt = getMimeHandler(mimetype, m_cfg, dofilter);
    if (!newflt) {
        // If we can't find a handler, this doc can't be handled
        // but there can be other ones so we go on
        LOGINFO("FileInterner::addHandler: no filter for [" << mimetype <<
                "]\n");
        return ADD_CONTINUE;
    }
    newflt->set_property(Dijon::Filter::OPERATING_MODE, 
                         m_forPreview ? "view" : "index");
    if (!charset.empty())
        newflt->set_property(Dijon::Filter::DEFAULT_CHARSET, charset);

    // Get current content: we don't use getkeyvalue() here to avoid
    // copying the text, which may be big.
    string ns;
    const string *txt = &ns;
    {
        map<string,string>::const_iterator it;
        it = docdata.find(cstr_dj_keycontent);
        if (it != docdata.end())
            txt = &it->second;
    }
    bool setres = false;
    newflt->set_docsize(txt->length());
    if (newflt->is_data_input_ok(Dijon::Filter::DOCUMENT_STRING)) {
        setres = newflt->set_document_string(mimetype, *txt);
    } else if (newflt->is_data_input_ok(Dijon::Filter::DOCUMENT_DATA)) {
        setres = newflt->set_document_data(mimetype,txt->c_str(),txt->length());
    } else if (newflt->is_data_input_ok(Dijon::Filter::DOCUMENT_FILE_NAME)) {
        TempFile temp = dataToTempFile(*txt, mimetype);
        if (temp.ok() && 
            (setres = newflt->set_document_file(mimetype, temp.filename()))) {
            m_tmpflgs[m_handlers.size()] = true;
            m_tempfiles.push_back(temp);
            // Hack here, but really helps perfs: if we happen to
            // create a temp file for, ie, an image attachment, keep
            // it around for preview to use it through get_imgtmp()
            if (!mimetype.compare(0, 6, "image/")) {
                m_imgtmp = m_tempfiles.back();
            }
        }
    }
    if (!setres) {
        LOGINFO("FileInterner::addHandler: set_doc failed inside [" << m_fn <<
                "]  for mtype " << mimetype << "\n");
    }
    // Add handler and go on, maybe this one will give us text...
    m_handlers.push_back(newflt);
    LOGDEB1("FileInterner::addHandler: added\n");
    return setres ? ADD_OK : ADD_BREAK;
}

// Information and debug after a next_document error
void FileInterner::processNextDocError(Rcl::Doc &doc)
{
    collectIpathAndMT(doc);
    m_reason = m_handlers.back()->get_error();
    checkExternalMissing(m_reason, doc.mimetype);
    LOGERR("FileInterner::internfile: next_document error [" << m_fn <<
           (doc.ipath.empty() ? "" : "|") << doc.ipath << "] " <<
           doc.mimetype << " " << m_reason << "\n");
}

FileInterner::Status FileInterner::internfile(Rcl::Doc& doc,const string& ipath)
{
    LOGDEB("FileInterner::internfile. ipath [" << ipath << "]\n");

    // Get rid of possible image tempfile from older call
    m_imgtmp = TempFile();

    if (m_handlers.size() < 1) {
        // Just means the constructor failed
        LOGDEB("FileInterner::internfile: no handler: constructor failed\n");
        return FIError;
    }

    // Input Ipath vector when retrieving a given subdoc for previewing
    vector<string> vipath;
    if (!ipath.empty() && !m_direct) {
        stringToTokens(ipath, vipath, cstr_isep, true);
        for (auto& entry: vipath) {
            entry = colon_restore(entry);
        }
        if (!m_handlers.back()->skip_to_document(vipath[m_handlers.size()-1])){
            LOGERR("FileInterner::internfile: can't skip\n");
            return FIError;
        }
    }

    // Try to get doc from the topmost handler
    // Security counter: looping happens when we stack one other 
    // handler or when walking the file document tree without finding
    // something to index (typical exemple: email with multiple image
    // attachments and no image filter installed). So we need to be
    // quite generous here, especially because there is another
    // security in the form of a maximum handler stack size.
    int loop = 0;
    while (!m_handlers.empty()) {
        CancelCheck::instance().checkCancel();
        if (loop++ > 1000) {
            LOGERR("FileInterner:: looping!\n");
            return FIError;
        }
        // If there are no more docs at the current top level we pop and
        // see if there is something at the previous one
        if (!m_handlers.back()->has_documents()) {
            // If looking for a specific doc, this is an error. Happens if
            // the index is stale, and the ipath points to the wrong message
            // for exemple (one with less attachments)
            if (m_forPreview) {
                m_reason += "Requested document does not exist. ";
                m_reason += m_handlers.back()->get_error();
                LOGERR("FileInterner: requested document does not exist\n");
                return FIError;
            }
            popHandler();
            continue;
        }

        // While indexing, don't stop on next_document() error. There
        // might be ie an error while decoding an attachment, but we
        // still want to process the rest of the mbox! For preview: fatal.
        if (!m_handlers.back()->next_document()) {
            // Using a temp doc here because else we'd need to pop the
            // last ipath element when we do the pophandler (else the
            // ipath continues to grow in the current doc with each
            // consecutive error). It would be better to have
            // something like ipath.pop(). We do need the MIME type
            Rcl::Doc doc1 = doc;
            processNextDocError(doc1);
            doc.mimetype = doc1.mimetype;
            if (m_forPreview) {
                m_reason += "Requested document does not exist. ";
                m_reason += m_handlers.back()->get_error();
                LOGERR("FileInterner: requested document does not exist\n");
                return FIError;
            }
            popHandler();
            continue;
        }

        // Look at the type for the next document and possibly add
        // handler to stack.
        switch (addHandler()) {
        case ADD_OK: // Just go through: handler has been stacked, use it
            LOGDEB2("addHandler returned OK\n");
            break; 
        case ADD_CONTINUE: 
            // forget this doc and retrieve next from current handler
            // (ipath stays same)
            LOGDEB2("addHandler returned CONTINUE\n");
            continue;
        case ADD_BREAK: 
            // Stop looping: doc type ok, need complete its processing
            // and return it
            LOGDEB2("addHandler returned BREAK\n");
            goto breakloop; // when you have to you have to
        case ADD_ERROR: 
            LOGDEB2("addHandler returned ERROR\n");
            return FIError;
        }

        // If we have an ipath, meaning that we are seeking a specific
        // document (ie: previewing a search result), we may have to
        // seek to the correct entry of a compound doc (ie: archive or
        // mail). When we are out of ipath entries, we stop seeking,
        // the handlers stack may still grow for translation (ie: if
        // the target doc is msword, we'll still stack the
        // word-to-text translator).
        if (!ipath.empty()) {
            if (m_handlers.size() <= vipath.size() &&
                !m_handlers.back()->skip_to_document(vipath[m_handlers.size()-1])) {
                LOGERR("FileInterner::internfile: can't skip\n");
                return FIError;
            }
        }
    }
breakloop:
    if (m_handlers.empty()) {
        LOGDEB("FileInterner::internfile: conversion ended with no doc\n");
        return FIError;
    }

    // Compute ipath and significant mimetype.  ipath is returned
    // through doc.ipath. We also retrieve some metadata fields from
    // the ancesters (like date or author). This is useful for email
    // attachments. The values will be replaced by those internal to
    // the document (by dijontorcl()) if any, so the order of calls is
    // important. We used to only do this when indexing, but the aux
    // fields like filename and author may be interesting when
    // previewing too
    collectIpathAndMT(doc);
    if (m_forPreview) {
        doc.mimetype = m_reachedMType;
    }
    // Keep this AFTER collectIpathAndMT
    dijontorcl(doc);

    // Possibly destack so that we can test for FIDone. While doing this
    // possibly set aside an ancestor html text (for the GUI preview)
    while (!m_handlers.empty() && !m_handlers.back()->has_documents()) {
        if (m_forPreview) {
            MimeHandlerHtml *hth = 
                dynamic_cast<MimeHandlerHtml*>(m_handlers.back());
            if (hth) {
                m_html = hth->get_html();
            }
        }
        popHandler();
    }
    if (m_handlers.empty())
        return FIDone;
    else 
        return FIAgain;
}

bool FileInterner::tempFileForMT(TempFile& otemp, RclConfig* cnf, 
                                 const string& mimetype)
{
    TempFile temp(cnf->getSuffixFromMimeType(mimetype));
    if (!temp.ok()) {
        LOGERR("FileInterner::tempFileForMT: can't create temp file\n");
        return false;
    }
    otemp = temp;
    return true;
}

// Static method, creates a FileInterner object to do the job.
bool FileInterner::idocToFile(
    TempFile& otemp, const string& tofile, RclConfig *cnf,
    const Rcl::Doc& idoc, bool uncompress)
{
    LOGDEB("FileInterner::idocToFile\n");

    if (idoc.ipath.empty()) {
        // Because of the mandatory first conversion in the
        // FileInterner constructor, need to use a specific method.
        return topdocToFile(otemp, tofile, cnf, idoc, uncompress);
    }

    // We set FIF_forPreview for consistency with the previous version
    // which determined this by looking at mtype!=null. Probably
    // doesn't change anything in this case.
    FileInterner interner(idoc, cnf, FIF_forPreview);
    interner.setTargetMType(idoc.mimetype);
    return interner.interntofile(otemp, tofile, idoc.ipath, idoc.mimetype);
}

// This is only needed because the FileInterner constructor always performs
// the first conversion, so that we need another approach for accessing the
// original document (targetmtype won't do).
bool FileInterner::topdocToFile(
    TempFile& otemp, const string& tofile,
    RclConfig *cnf, const Rcl::Doc& idoc, bool uncompress)
{
    std::unique_ptr<DocFetcher> fetcher(docFetcherMake(cnf, idoc));
    if (!fetcher) {
        LOGERR("FileInterner::topdocToFile no backend\n");
        return false;
    }
    DocFetcher::RawDoc rawdoc;
    if (!fetcher->fetch(cnf, idoc, rawdoc)) {
        LOGERR("FileInterner::topdocToFile fetcher failed\n");
        return false;
    }
    const char *filename = "";
    TempFile temp;
    if (tofile.empty()) {
        if (!tempFileForMT(temp, cnf, idoc.mimetype)) {
            return false;
        }
        filename = temp.filename();
    } else {
        filename = tofile.c_str();
    }
    string reason;
    switch (rawdoc.kind) {
    case DocFetcher::RawDoc::RDK_FILENAME: {
        string fn(rawdoc.data);
        TempFile temp;
        if (uncompress && isCompressed(fn, cnf)) {
            if (!maybeUncompressToTemp(temp, fn, cnf, idoc)) {
                LOGERR("FileInterner::idocToFile: uncompress failed\n");
                return false;
            }
        }
        fn = temp.ok() ? temp.filename() : rawdoc.data;
        if (!copyfile(fn.c_str(), filename, reason)) {
            LOGERR("FileInterner::idocToFile: copyfile: " << reason << "\n");
            return false;
        }
    }
        break;
    case DocFetcher::RawDoc::RDK_DATA:
    case DocFetcher::RawDoc::RDK_DATADIRECT:
        if (!stringtofile(rawdoc.data, filename, reason)) {
            LOGERR("FileInterner::idocToFile: stringtofile: " << reason <<"\n");
            return false;
        }
        break;
    default:
        LOGERR("FileInterner::FileInterner(idoc): bad rawdoc kind ??\n");
    }

    if (tofile.empty())
        otemp = temp;
    return true;
}

bool FileInterner::interntofile(TempFile& otemp, const string& tofile,
                                const string& ipath, const string& mimetype)
{
    if (!ok()) {
        LOGERR("FileInterner::interntofile: constructor failed\n");
        return false;
    }
    Rcl::Doc doc;
    Status ret = internfile(doc, ipath);
    if (ret == FileInterner::FIError) {
        LOGERR("FileInterner::interntofile: internfile() failed\n");
        return false;
    }

    // Specialcase text/html. This is to work around a bug that will
    // get fixed some day: the internfile constructor always loads the
    // first handler so that at least one conversion is always
    // performed (and the access to the original data may be lost). A
    // common case is an "Open" on an HTML file (we end up
    // with text/plain content). As the HTML version is saved in this
    // case, use it.
    if (!stringlowercmp(cstr_texthtml, mimetype) && !get_html().empty()) {
        doc.text = get_html();
        doc.mimetype = cstr_texthtml;
    }

    const char *filename;
    TempFile temp;
    if (tofile.empty()) {
        if (!tempFileForMT(temp, m_cfg, mimetype)) {
            return false;
        }
        filename = temp.filename();
    } else {
        filename = tofile.c_str();
    }
    string reason;
    if (!stringtofile(doc.text, filename, reason)) {
        LOGERR("FileInterner::interntofile: stringtofile : " << reason << "\n");
        return false;
    }

    if (tofile.empty())
        otemp = temp;
    return true;
}

bool FileInterner::isCompressed(const string& fn, RclConfig *cnf)
{
    LOGDEB("FileInterner::isCompressed: [" << fn << "]\n");
    struct stat st;
    if (path_fileprops(fn, &st) < 0) {
        LOGERR("FileInterner::isCompressed: can't stat [" << fn << "]\n");
        return false;
    }
    string l_mime = mimetype(fn, &st, cnf, true);
    if (l_mime.empty()) {
        LOGERR("FileInterner::isUncompressed: can't get mime for [" << fn <<
               "]\n");
        return false;
    }

    vector<string> ucmd;
    if (cnf->getUncompressor(l_mime, ucmd)) {
        return true;
    }
    return false;
}

// Static.
bool FileInterner::maybeUncompressToTemp(TempFile& temp, const string& fn, 
                                         RclConfig *cnf, const Rcl::Doc& doc)
{
    LOGDEB("FileInterner::maybeUncompressToTemp: [" << fn << "]\n");
    struct stat st;
    if (path_fileprops(fn.c_str(), &st) < 0) {
        LOGERR("FileInterner::maybeUncompressToTemp: can't stat [" <<fn<<"]\n");
        return false;
    }
    string l_mime = mimetype(fn, &st, cnf, true);
    if (l_mime.empty()) {
        LOGERR("FileInterner::maybeUncompress.: can't id. mime for [" <<
               fn << "]\n");
        return false;
    }

    vector<string>ucmd;
    if (!cnf->getUncompressor(l_mime, ucmd)) {
        return true;
    }
    // Check for compressed size limit
    int maxkbs = -1;
    if (cnf->getConfParam("compressedfilemaxkbs", &maxkbs) &&
        maxkbs >= 0 && int(st.st_size / 1024) > maxkbs) {
        LOGINFO("FileInterner:: " << fn << " over size limit " << maxkbs <<
                " kbs\n");
        return false;
    }
    temp = TempFile(cnf->getSuffixFromMimeType(doc.mimetype));
    if (!temp.ok()) {
        LOGERR("FileInterner: cant create temporary file\n");
        return false;
    }

    Uncomp uncomp;
    string uncomped;
    if (!uncomp.uncompressfile(fn, ucmd, uncomped)) {
        return false;
    }

    // uncompressfile choses the output file name, there is good
    // reason for this, but it's not nice here. Have to move, the
    // uncompressed file, hopefully staying on the same dev.
    string reason;
    if (!renameormove(uncomped.c_str(), temp.filename(), reason)) {
        LOGERR("FileInterner::maybeUncompress: move [" << uncomped <<
               "] -> [" << temp.filename() << "] failed: " << reason << "\n");
        return false;
    }
    return true;
}
