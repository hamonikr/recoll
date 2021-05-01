/* Copyright (C) 2004-2018 J.F.Dockes
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
#include <cstring>
#include <exception>
#include "safeunistd.h"
#include <math.h>
#include <time.h>

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iostream>

using namespace std;

#include "xapian.h"

#include "rclconfig.h"
#include "log.h"
#include "rcldb.h"
#include "rcldb_p.h"
#include "stemdb.h"
#include "textsplit.h"
#include "transcode.h"
#include "unacpp.h"
#include "conftree.h"
#include "pathut.h"
#include "rclutil.h"
#include "smallut.h"
#include "chrono.h"
#include "searchdata.h"
#include "rclquery.h"
#include "rclquery_p.h"
#include "rclvalues.h"
#include "md5ut.h"
#include "cancelcheck.h"
#include "termproc.h"
#include "expansiondbs.h"
#include "rclinit.h"
#include "internfile.h"
#include "utf8fn.h"
#include "wipedir.h"
#ifdef RCL_USE_ASPELL
#include "rclaspell.h"
#endif
#include "zlibut.h"

#ifndef XAPIAN_AT_LEAST
// Added in Xapian 1.4.2. Define it here for older versions
#define XAPIAN_AT_LEAST(A,B,C) \
 (XAPIAN_MAJOR_VERSION > (A) || \
 (XAPIAN_MAJOR_VERSION == (A) && \
 (XAPIAN_MINOR_VERSION > (B) || \
 (XAPIAN_MINOR_VERSION == (B) && XAPIAN_REVISION >= (C)))))
#endif


// Recoll index format version is stored in user metadata. When this change,
// we can't open the db and will have to reindex.
static const string cstr_RCL_IDX_VERSION_KEY("RCL_IDX_VERSION_KEY");
static const string cstr_RCL_IDX_VERSION("1");
static const string cstr_RCL_IDX_DESCRIPTOR_KEY("RCL_IDX_DESCRIPTOR_KEY");

static const string cstr_mbreaks("rclmbreaks");

namespace Rcl {

// Some prefixes that we could get from the fields file, but are not going
// to ever change.
static const string fileext_prefix = "XE";
const string mimetype_prefix = "T";
static const string xapday_prefix = "D";
static const string xapmonth_prefix = "M";
static const string xapyear_prefix = "Y";
const string pathelt_prefix = "XP";
static const string udi_prefix("Q");
static const string parent_prefix("F");

// Special terms to mark begin/end of field (for anchored searches), and
// page breaks
string start_of_field_term;
string end_of_field_term;
const string page_break_term = "XXPG/";

// Special term to mark documents with children.
const string has_children_term("XXC/");

// Field name for the unsplit file name. Has to exist in the field file 
// because of usage in termmatch()
const string unsplitFilenameFieldName = "rclUnsplitFN";
static const string unsplitfilename_prefix = "XSFS";

// Empty string md5s 
static const string cstr_md5empty("d41d8cd98f00b204e9800998ecf8427e");

static const int MB = 1024 * 1024;

string version_string(){
    return string("Recoll ") + string(PACKAGE_VERSION) + string(" + Xapian ") +
        string(Xapian::version_string());
}

// Synthetic abstract marker (to discriminate from abstract actually
// found in document)
static const string cstr_syntAbs("?!#@");

// Compute the unique term used to link documents to their origin. 
// "Q" + external udi
static inline string make_uniterm(const string& udi)
{
    string uniterm(wrap_prefix(udi_prefix));
    uniterm.append(udi);
    return uniterm;
}

// Compute parent term used to link documents to their parent document (if any)
// "F" + parent external udi
static inline string make_parentterm(const string& udi)
{
    // I prefer to be in possible conflict with omega than with
    // user-defined fields (Xxxx) that we also allow. "F" is currently
    // not used by omega (2008-07)
    string pterm(wrap_prefix(parent_prefix));
    pterm.append(udi);
    return pterm;
}

Db::Native::Native(Db *db) 
    : m_rcldb(db), m_isopen(false), m_iswritable(false),
      m_noversionwrite(false)
#ifdef IDX_THREADS
    , m_wqueue("DbUpd", 
	       m_rcldb->m_config->getThrConf(RclConfig::ThrDbWrite).first),
      m_totalworkns(0LL), m_havewriteq(false)
#endif // IDX_THREADS
{ 
    LOGDEB1("Native::Native: me " << this << "\n");
}

Db::Native::~Native() 
{ 
    LOGDEB1("Native::~Native: me " << this << "\n");
#ifdef IDX_THREADS
    if (m_havewriteq) {
	void *status = m_wqueue.setTerminateAndWait();
        if (status) {
            LOGDEB1("Native::~Native: worker status " << status << "\n");
        }
    }
#endif // IDX_THREADS
}

#ifdef IDX_THREADS
void *DbUpdWorker(void* vdbp)
{
    recoll_threadinit();
    Db::Native *ndbp = (Db::Native *)vdbp;
    WorkQueue<DbUpdTask*> *tqp = &(ndbp->m_wqueue);

    DbUpdTask *tsk = 0;
    for (;;) {
	size_t qsz = -1;
	if (!tqp->take(&tsk, &qsz)) {
	    tqp->workerExit();
	    return (void*)1;
	}
	bool status = false;
	switch (tsk->op) {
	case DbUpdTask::AddOrUpdate:
	    LOGDEB("DbUpdWorker: got add/update task, ql " << qsz << "\n");
	    status = ndbp->addOrUpdateWrite(
                tsk->udi, tsk->uniterm, tsk->doc, tsk->txtlen, tsk->rawztext);
	    break;
	case DbUpdTask::Delete:
	    LOGDEB("DbUpdWorker: got delete task, ql " << qsz << "\n");
	    status = ndbp->purgeFileWrite(false, tsk->udi, tsk->uniterm);
	    break;
	case DbUpdTask::PurgeOrphans:
	    LOGDEB("DbUpdWorker: got orphans purge task, ql " << qsz << "\n");
	    status = ndbp->purgeFileWrite(true, tsk->udi, tsk->uniterm);
	    break;
	default:
	    LOGERR("DbUpdWorker: unknown op " << tsk->op << " !!\n");
	    break;
	}
	if (!status) {
	    LOGERR("DbUpdWorker: xxWrite failed\n");
	    tqp->workerExit();
	    delete tsk;
	    return (void*)0;
	}
	delete tsk;
    }
}

void Db::Native::maybeStartThreads()
{
    m_havewriteq = false;
    const RclConfig *cnf = m_rcldb->m_config;
    int writeqlen = cnf->getThrConf(RclConfig::ThrDbWrite).first;
    int writethreads = cnf->getThrConf(RclConfig::ThrDbWrite).second;
    if (writethreads > 1) {
	LOGINFO("RclDb: write threads count was forced down to 1\n");
	writethreads = 1;
    }
    if (writeqlen >= 0 && writethreads > 0) {
	if (!m_wqueue.start(writethreads, DbUpdWorker, this)) {
	    LOGERR("Db::Db: Worker start failed\n");
	    return;
	}
	m_havewriteq = true;
    }
    LOGDEB("RclDb:: threads: haveWriteQ " << m_havewriteq << ", wqlen " <<
           writeqlen << " wqts " << writethreads << "\n");
}

#endif // IDX_THREADS

void Db::Native::openWrite(const string& dir, Db::OpenMode mode)
{
    int action = (mode == Db::DbUpd) ? Xapian::DB_CREATE_OR_OPEN :
        Xapian::DB_CREATE_OR_OVERWRITE;

#ifdef _WIN32
    // On Windows, Xapian is quite bad at erasing partial db which can
    // occur because of open file deletion errors.
    if (mode == DbTrunc) {
        if (path_exists(path_cat(dir, "iamchert"))) {
            wipedir(dir);
            unlink(dir.c_str());
        }
    }
#endif
    
    if (path_exists(dir)) {
        // Existing index. 
        xwdb = Xapian::WritableDatabase(dir, action);
        if (action == Xapian::DB_CREATE_OR_OVERWRITE ||
            xwdb.get_doccount() == 0) {
            // New or empty index. Set the "store text" option
            // according to configuration. The metadata record will be
            // written further down.
            m_storetext = o_index_storedoctext;
            LOGDEB("Db:: index " << (m_storetext?"stores":"does not store") <<
                   " document text\n");
        } else {
            // Existing non empty. Get the option from the index.
            storesDocText(xwdb);
        }
    } else {
        // New index. If possible, and depending on config, use a stub
        // to force using Chert. No sense in doing this if we are
        // storing the text anyway.
#if XAPIAN_AT_LEAST(1,3,0) && XAPIAN_HAS_CHERT_BACKEND
        // Xapian with Glass and Chert support. If storedoctext is
        // specified in the configuration, use the default backend
        // (Glass), else force Chert. There might be reasons why
        // someone would want to use Chert and store text anyway, but
        // it's an exotic case, and things are complicated enough
        // already.
        if (o_index_storedoctext) {
            xwdb = Xapian::WritableDatabase(dir, action);
            m_storetext = true;
        } else {
            // Force Chert format, don't store the text.
            string stub = path_cat(m_rcldb->m_config->getConfDir(),
                                   "xapian.stub");
            FILE *fp = fopen(stub.c_str(), "w");
            if (nullptr == fp) {
                throw(string("Can't create ") + stub);
            }
            fprintf(fp, "chert %s\n", dir.c_str());
            fclose(fp);
            xwdb = Xapian::WritableDatabase(stub, action);
            m_storetext = false;
        }
        LOGINF("Rcl::Db::openWrite: new index will " << (m_storetext?"":"not ")
               << "store document text\n");
#else
        // Old Xapian (chert only) or much newer (no chert). Use the
        // default index backend and let the user decide of the
        // abstract generation method. The configured default is to
        // store the text.
        xwdb = Xapian::WritableDatabase(dir, action);
        m_storetext = o_index_storedoctext;
#endif
    }

    // If the index is empty, write the data format version, 
    // and the storetext option value inside the index descriptor (new
    // with recoll 1.24, maybe we'll have other stuff to store in
    // there in the future).
    if (xwdb.get_doccount() == 0) {
        string desc = string("storetext=") + (m_storetext ? "1" : "0") + "\n";
        xwdb.set_metadata(cstr_RCL_IDX_DESCRIPTOR_KEY, desc);
        xwdb.set_metadata(cstr_RCL_IDX_VERSION_KEY, cstr_RCL_IDX_VERSION);
    }

    m_iswritable = true;

#ifdef IDX_THREADS
    maybeStartThreads();
#endif
}

void Db::Native::storesDocText(Xapian::Database& db)
{
    string desc = db.get_metadata(cstr_RCL_IDX_DESCRIPTOR_KEY);
    ConfSimple cf(desc, 1);
    string val;
    m_storetext = false;
    if (cf.get("storetext", val) && stringToBool(val)) {
        m_storetext = true;
    }
    LOGDEB("Db:: index " << (m_storetext?"stores":"does not store") <<
           " document text\n");
}

void Db::Native::openRead(const string& dir)
{
    m_iswritable = false;
    xrdb = Xapian::Database(dir);
    storesDocText(xrdb);
}

/* See comment in class declaration: return all subdocuments of a
 * document given by its unique id. */
bool Db::Native::subDocs(const string &udi, int idxi, 
			 vector<Xapian::docid>& docids) 
{
    LOGDEB2("subDocs: [" << uniterm << "]\n");
    string pterm = make_parentterm(udi);
    vector<Xapian::docid> candidates;
    XAPTRY(docids.clear();
           candidates.insert(candidates.begin(), xrdb.postlist_begin(pterm), 
			     xrdb.postlist_end(pterm)),
           xrdb, m_rcldb->m_reason);
    if (!m_rcldb->m_reason.empty()) {
        LOGERR("Rcl::Db::subDocs: " << m_rcldb->m_reason << "\n");
        return false;
    } else {
	for (unsigned int i = 0; i < candidates.size(); i++) {
	    if (whatDbIdx(candidates[i]) == (size_t)idxi) {
		docids.push_back(candidates[i]);
	    }
	}
        LOGDEB0("Db::Native::subDocs: returning " << docids.size() << " ids\n");
        return true;
    }
}

bool Db::Native::xdocToUdi(Xapian::Document& xdoc, string &udi)
{
    Xapian::TermIterator xit;
    XAPTRY(xit = xdoc.termlist_begin();
	   xit.skip_to(wrap_prefix(udi_prefix)),
           xrdb, m_rcldb->m_reason);
    if (!m_rcldb->m_reason.empty()) {
	LOGERR("xdocToUdi: xapian error: " << m_rcldb->m_reason << "\n");
	return false;
    }
    if (xit != xdoc.termlist_end()) {
	udi = *xit;
	if (!udi.empty()) {
	    udi = udi.substr(wrap_prefix(udi_prefix).size());
	    return true;
	}
    }
    return false;
}

// Clear term from document if its frequency is 0. This should
// probably be done by Xapian when the freq goes to 0 when removing a
// posting, but we have to do it ourselves
bool Db::Native::clearDocTermIfWdf0(Xapian::Document& xdoc, const string& term)
{
    LOGDEB1("Db::clearDocTermIfWdf0: [" << term << "]\n");

    // Find the term
    Xapian::TermIterator xit;
    XAPTRY(xit = xdoc.termlist_begin(); xit.skip_to(term);,
	   xrdb, m_rcldb->m_reason);
    if (!m_rcldb->m_reason.empty()) {
	LOGERR("Db::clearDocTerm...: [" << term << "] skip failed: " <<
               m_rcldb->m_reason << "\n");
	return false;
    }
    if (xit == xdoc.termlist_end() || term.compare(*xit)) {
	LOGDEB0("Db::clearDocTermIFWdf0: term [" << term <<
                "] not found. xit: [" <<
                (xit == xdoc.termlist_end() ? "EOL": *xit) << "]\n");
	return false;
    }

    // Clear the term if its frequency is 0
    if (xit.get_wdf() == 0) {
	LOGDEB1("Db::clearDocTermIfWdf0: clearing [" << term << "]\n");
	XAPTRY(xdoc.remove_term(term), xwdb, m_rcldb->m_reason);
	if (!m_rcldb->m_reason.empty()) {
	    LOGDEB0("Db::clearDocTermIfWdf0: failed [" << term << "]: " <<
                    m_rcldb->m_reason << "\n");
	}
    }
    return true;
}

// Holder for term + pos
struct DocPosting {
    DocPosting(string t, Xapian::termpos ps)
	: term(t), pos(ps) {}
    string term;
    Xapian::termpos pos;
};

// Clear all terms for given field for given document.
// The terms to be cleared are all those with the appropriate
// prefix. We also remove the postings for the unprefixed terms (that
// is, we undo what we did when indexing).
bool Db::Native::clearField(Xapian::Document& xdoc, const string& pfx,
			    Xapian::termcount wdfdec)
{
    LOGDEB1("Db::clearField: clearing prefix [" << pfx << "] for docid " <<
            xdoc.get_docid() << "\n");

    vector<DocPosting> eraselist;

    string wrapd = wrap_prefix(pfx);

    m_rcldb->m_reason.clear();
    for (int tries = 0; tries < 2; tries++) {
	try {
	    Xapian::TermIterator xit;
	    xit = xdoc.termlist_begin();
	    xit.skip_to(wrapd);
	    while (xit != xdoc.termlist_end() && 
		!(*xit).compare(0, wrapd.size(), wrapd)) {
		LOGDEB1("Db::clearfield: erasing for [" << *xit << "]\n");
		Xapian::PositionIterator posit;
		for (posit = xit.positionlist_begin();
		     posit != xit.positionlist_end(); posit++) {
		    eraselist.push_back(DocPosting(*xit, *posit));
		    eraselist.push_back(DocPosting(strip_prefix(*xit), *posit));
		}
		xit++;
	    }
	} catch (const Xapian::DatabaseModifiedError &e) {
	    m_rcldb->m_reason = e.get_msg();
	    xrdb.reopen();
	    continue;
	} XCATCHERROR(m_rcldb->m_reason);
	break;
    }
    if (!m_rcldb->m_reason.empty()) {
	LOGERR("Db::clearField: failed building erase list: " <<
               m_rcldb->m_reason << "\n");
	return false;
    }

    // Now remove the found positions, and the terms if the wdf is 0
    for (vector<DocPosting>::const_iterator it = eraselist.begin();
	 it != eraselist.end(); it++) {
	LOGDEB1("Db::clearField: remove posting: [" << it->term << "] pos [" <<
                it->pos << "]\n");
	XAPTRY(xdoc.remove_posting(it->term, it->pos, wdfdec);, 
	       xwdb,m_rcldb->m_reason);
	if (!m_rcldb->m_reason.empty()) {
	    // Not that this normally fails for non-prefixed XXST and
	    // ND, don't make a fuss
	    LOGDEB1("Db::clearFiedl: remove_posting failed for [" << it->term <<
                    "]," << it->pos << ": " << m_rcldb->m_reason << "\n");
	}
	clearDocTermIfWdf0(xdoc, it->term);
    }
    return true;
}

// Check if doc given by udi is indexed by term
bool Db::Native::hasTerm(const string& udi, int idxi, const string& term)
{
    LOGDEB2("Native::hasTerm: udi [" << udi << "] term [" << term << "]\n");
    Xapian::Document xdoc;
    if (getDoc(udi, idxi, xdoc)) {
	Xapian::TermIterator xit;
	XAPTRY(xit = xdoc.termlist_begin();
	       xit.skip_to(term);,
	       xrdb, m_rcldb->m_reason);
	if (!m_rcldb->m_reason.empty()) {
	    LOGERR("Rcl::Native::hasTerm: " << m_rcldb->m_reason << "\n");
	    return false;
	}
	if (xit != xdoc.termlist_end() && !term.compare(*xit)) {
	    return true;
	}
    }
    return false;
}

// Retrieve Xapian document, given udi. There may be several identical udis
// if we are using multiple indexes.
Xapian::docid Db::Native::getDoc(const string& udi, int idxi, 
				 Xapian::Document& xdoc)
{
    string uniterm = make_uniterm(udi);
    for (int tries = 0; tries < 2; tries++) {
	try {
            Xapian::PostingIterator docid;
	    for (docid = xrdb.postlist_begin(uniterm); 
		 docid != xrdb.postlist_end(uniterm); docid++) {
		xdoc = xrdb.get_document(*docid);
		if (whatDbIdx(*docid) == (size_t)idxi)
		    return *docid;
	    }
	    // Udi not in Db.
	    return 0;
	} catch (const Xapian::DatabaseModifiedError &e) {
            m_rcldb->m_reason = e.get_msg();
	    xrdb.reopen();
            continue;
	} XCATCHERROR(m_rcldb->m_reason);
        break;
    }
    LOGERR("Db::Native::getDoc: Xapian error: " << m_rcldb->m_reason << "\n");
    return 0;
}

// Turn data record from db into document fields
bool Db::Native::dbDataToRclDoc(Xapian::docid docid, std::string &data, 
				Doc &doc, bool fetchtext)
{
    LOGDEB2("Db::dbDataToRclDoc: data:\n" << data << "\n");
    ConfSimple parms(data);
    if (!parms.ok())
	return false;

    doc.xdocid = docid;
    doc.haspages = hasPages(docid);

    // Compute what index this comes from, and check for path translations
    string dbdir = m_rcldb->m_basedir;
    doc.idxi = 0;
    if (!m_rcldb->m_extraDbs.empty()) {
	int idxi = int(whatDbIdx(docid));

	// idxi is in [0, extraDbs.size()]. 0 is for the main index,
	// idxi-1 indexes into the additional dbs array.
	if (idxi) {
	    dbdir = m_rcldb->m_extraDbs[idxi - 1];
	    doc.idxi = idxi;
	}
    }
    parms.get(Doc::keyurl, doc.idxurl);
    doc.url = doc.idxurl;
    m_rcldb->m_config->urlrewrite(dbdir, doc.url);
    if (!doc.url.compare(doc.idxurl))
	doc.idxurl.clear();

    // Special cases:
    parms.get(Doc::keytp, doc.mimetype);
    parms.get(Doc::keyfmt, doc.fmtime);
    parms.get(Doc::keydmt, doc.dmtime);
    parms.get(Doc::keyoc, doc.origcharset);
    parms.get(cstr_caption, doc.meta[Doc::keytt]);

    parms.get(Doc::keyabs, doc.meta[Doc::keyabs]);
    // Possibly remove synthetic abstract indicator (if it's there, we
    // used to index the beginning of the text as abstract).
    doc.syntabs = false;
    if (doc.meta[Doc::keyabs].find(cstr_syntAbs) == 0) {
	doc.meta[Doc::keyabs] = 
	    doc.meta[Doc::keyabs].substr(cstr_syntAbs.length());
	doc.syntabs = true;
    }
    parms.get(Doc::keyipt, doc.ipath);
    parms.get(Doc::keypcs, doc.pcbytes);
    parms.get(Doc::keyfs, doc.fbytes);
    parms.get(Doc::keyds, doc.dbytes);
    parms.get(Doc::keysig, doc.sig);

    // Normal key/value pairs:
    vector<string> keys = parms.getNames(string());
    for (vector<string>::const_iterator it = keys.begin(); 
	 it != keys.end(); it++) {
	if (doc.meta.find(*it) == doc.meta.end())
	    parms.get(*it, doc.meta[*it]);
    }
    doc.meta[Doc::keyurl] = doc.url;
    doc.meta[Doc::keymt] = doc.dmtime.empty() ? doc.fmtime : doc.dmtime;
    if (fetchtext) {
        getRawText(docid, doc.text);
    }
    return true;
}

bool Db::Native::hasPages(Xapian::docid docid)
{
    string ermsg;
    Xapian::PositionIterator pos;
    XAPTRY(pos = xrdb.positionlist_begin(docid, page_break_term); 
	   if (pos != xrdb.positionlist_end(docid, page_break_term)) {
	       return true;
	   },
	   xrdb, ermsg);
    if (!ermsg.empty()) {
	LOGERR("Db::Native::hasPages: xapian error: " << ermsg << "\n");
    }
    return false;
}

// Return the positions list for the page break term
bool Db::Native::getPagePositions(Xapian::docid docid, vector<int>& vpos)
{
    vpos.clear();
    // Need to retrieve the document record to check for multiple page breaks
    // that we store there for lack of better place
    map<int, int> mbreaksmap;
    try {
	Xapian::Document xdoc = xrdb.get_document(docid);
	string data = xdoc.get_data();
	Doc doc;
	string mbreaks;
	if (dbDataToRclDoc(docid, data, doc) && 
	    doc.getmeta(cstr_mbreaks, &mbreaks)) {
	    vector<string> values;
	    stringToTokens(mbreaks, values, ",");
	    for (unsigned int i = 0; i < values.size() - 1; i += 2) {
		int pos  = atoi(values[i].c_str()) + baseTextPosition;
		int incr = atoi(values[i+1].c_str());
		mbreaksmap[pos] = incr;
	    }
	}
    } catch (...) {
    }

    string qterm = page_break_term;
    Xapian::PositionIterator pos;
    try {
	for (pos = xrdb.positionlist_begin(docid, qterm); 
	     pos != xrdb.positionlist_end(docid, qterm); pos++) {
	    int ipos = *pos;
	    if (ipos < int(baseTextPosition)) {
		LOGDEB("getPagePositions: got page position " << ipos
                       << " not in body\n");
		// Not in text body. Strange...
		continue;
	    }
	    map<int, int>::iterator it = mbreaksmap.find(ipos);
	    if (it != mbreaksmap.end()) {
		LOGDEB1("getPagePositions: found multibreak at " << ipos <<
                        " incr " << it->second << "\n");
		for (int i = 0 ; i < it->second; i++) 
		    vpos.push_back(ipos);
	    }
	    vpos.push_back(ipos);
	} 
    } catch (...) {
	// Term does not occur. No problem.
    }
    return true;
}

int Db::Native::getPageNumberForPosition(const vector<int>& pbreaks, int pos)
{
    if (pos < int(baseTextPosition)) // Not in text body
	return -1;
    vector<int>::const_iterator it = 
	upper_bound(pbreaks.begin(), pbreaks.end(), pos);
    return int(it - pbreaks.begin() + 1);
}

bool Db::Native::getRawText(Xapian::docid docid_combined, string& rawtext)
{
    if (!m_storetext) {
        LOGDEB("Db::Native::getRawText: document text not stored in index\n");
        return false;
    }

    // Xapian get_metadata only works on a single index (else of
    // course, unicity of keys can't be ensured). When using multiple
    // indexes, we need to open the right one.
    size_t dbidx = whatDbIdx(docid_combined);
    Xapian::docid docid = whatDbDocid(docid_combined);
    string reason;
    if (dbidx != 0) {
        Xapian::Database db(m_rcldb->m_extraDbs[dbidx-1]);
        XAPTRY(rawtext = db.get_metadata(rawtextMetaKey(docid)), db, reason);
    } else {
        XAPTRY(rawtext = xrdb.get_metadata(rawtextMetaKey(docid)), xrdb, reason);
    }
    if (!reason.empty()) {
        LOGERR("Rcl::Db::getRawText: could not get value: " << reason << endl);
        return false;
    }
    if (rawtext.empty()) {
        return true;
    }
    ZLibUtBuf cbuf;
    inflateToBuf(rawtext.c_str(), rawtext.size(), cbuf);
    rawtext.assign(cbuf.getBuf(), cbuf.getCnt());
    return true;
}

// Note: we're passed a Xapian::Document* because Xapian
// reference-counting is not mt-safe. We take ownership and need
// to delete it before returning.
bool Db::Native::addOrUpdateWrite(
    const string& udi, const string& uniterm, Xapian::Document *newdocument_ptr, 
    size_t textlen, const string& rawztext)
{
#ifdef IDX_THREADS
    Chrono chron;
    std::unique_lock<std::mutex> lock(m_mutex);
#endif
    std::unique_ptr<Xapian::Document> doc_cleaner(newdocument_ptr);

    // Check file system full every mbyte of indexed text. It's a bit wasteful
    // to do this after having prepared the document, but it needs to be in
    // the single-threaded section.
    if (m_rcldb->m_maxFsOccupPc > 0 && 
	(m_rcldb->m_occFirstCheck || 
	 (m_rcldb->m_curtxtsz - m_rcldb->m_occtxtsz) / MB >= 1)) {
	LOGDEB("Db::add: checking file system usage\n");
	int pc;
	m_rcldb->m_occFirstCheck = 0;
	if (fsocc(m_rcldb->m_basedir, &pc) && pc >= m_rcldb->m_maxFsOccupPc) {
	    LOGERR("Db::add: stop indexing: file system " << pc << " %" <<
                   " full > max " << m_rcldb->m_maxFsOccupPc << " %" << "\n");
	    return false;
	}
	m_rcldb->m_occtxtsz = m_rcldb->m_curtxtsz;
    }

    const char *fnc = udi.c_str();
    string ermsg;

    // Add db entry or update existing entry:
    Xapian::docid did = 0;
    try {
	did = xwdb.replace_document(uniterm, *newdocument_ptr);
	if (did < m_rcldb->updated.size()) {
            // This is necessary because only the file-level docs are tested
            // by needUpdate(), so the subdocs existence flags are only set
            // here.
	    m_rcldb->updated[did] = true;
	    LOGINFO("Db::add: docid " << did << " updated [" << fnc << "]\n");
	} else {
	    LOGINFO("Db::add: docid " << did << " added [" << fnc << "]\n");
	}
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR("Db::add: replace_document failed: " << ermsg << "\n");
	ermsg.erase();
	// FIXME: is this ever actually needed?
	try {
	    xwdb.add_document(*newdocument_ptr);
	    LOGDEB("Db::add: " << fnc <<
                   " added (failed re-seek for duplicate)\n");
	} XCATCHERROR(ermsg);
	if (!ermsg.empty()) {
	    LOGERR("Db::add: add_document failed: " << ermsg << "\n");
	    return false;
	}
    }

    XAPTRY(xwdb.set_metadata(rawtextMetaKey(did), rawztext),
           xwdb, m_rcldb->m_reason);
    if (!m_rcldb->m_reason.empty()) {
        LOGERR("Db::addOrUpdate: set_metadata error: " <<
               m_rcldb->m_reason << "\n");
        // This only affects snippets, so let's say not fatal
    }
    
    // Test if we're over the flush threshold (limit memory usage):
    bool ret = m_rcldb->maybeflush(textlen);
#ifdef IDX_THREADS
    m_totalworkns += chron.nanos();
#endif
    return ret;
}

bool Db::Native::purgeFileWrite(bool orphansOnly, const string& udi, 
				const string& uniterm)
{
#if defined(IDX_THREADS) 
    // We need a mutex even if we have a write queue (so we can only
    // be called by a single thread) to protect about multiple acces
    // to xrdb from subDocs() which is also called from needupdate()
    // (called from outside the write thread !
    std::unique_lock<std::mutex> lock(m_mutex);
#endif // IDX_THREADS

    string ermsg;
    try {
	Xapian::PostingIterator docid = xwdb.postlist_begin(uniterm);
	if (docid == xwdb.postlist_end(uniterm)) {
	    return true;
        }
	if (m_rcldb->m_flushMb > 0) {
	    Xapian::termcount trms = xwdb.get_doclength(*docid);
	    m_rcldb->maybeflush(trms * 5);
	}
	string sig;
	if (orphansOnly) {
	    Xapian::Document doc = xwdb.get_document(*docid);
	    sig = doc.get_value(VALUE_SIG);
	    if (sig.empty()) {
		LOGINFO("purgeFileWrite: got empty sig\n");
		return false;
	    }
	} else {
	    LOGDEB("purgeFile: delete docid " << *docid << "\n");
            deleteDocument(*docid);
	}
	vector<Xapian::docid> docids;
	subDocs(udi, 0, docids);
	LOGDEB("purgeFile: subdocs cnt " << docids.size() << "\n");
	for (vector<Xapian::docid>::iterator it = docids.begin();
	     it != docids.end(); it++) {
	    if (m_rcldb->m_flushMb > 0) {
		Xapian::termcount trms = xwdb.get_doclength(*it);
		m_rcldb->maybeflush(trms * 5);
	    }
	    string subdocsig;
	    if (orphansOnly) {
		Xapian::Document doc = xwdb.get_document(*it);
		subdocsig = doc.get_value(VALUE_SIG);
		if (subdocsig.empty()) {
		    LOGINFO("purgeFileWrite: got empty sig for subdoc??\n");
		    continue;
		}
	    }
		
	    if (!orphansOnly || sig != subdocsig) {
		LOGDEB("Db::purgeFile: delete subdoc " << *it << "\n");
		deleteDocument(*it);
	    }
	}
	return true;
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR("Db::purgeFileWrite: " << ermsg << "\n");
    }
    return false;
}


/* Rcl::Db methods ///////////////////////////////// */

bool Db::o_inPlaceReset;

Db::Db(const RclConfig *cfp)
    : m_ndb(0),  m_mode(Db::DbRO), m_curtxtsz(0), m_flushtxtsz(0),
      m_occtxtsz(0), m_occFirstCheck(1), m_idxMetaStoredLen(150),
      m_idxAbsTruncLen(250), m_synthAbsLen(250), m_synthAbsWordCtxLen(4), 
      m_flushMb(-1), m_maxFsOccupPc(0)
{
    m_config = new RclConfig(*cfp);
    if (start_of_field_term.empty()) {
	if (o_index_stripchars) {
	    start_of_field_term = "XXST";
	    end_of_field_term = "XXND";
	} else {
	    start_of_field_term = "XXST/";
	    end_of_field_term = "XXND/";
	}
    }

    m_ndb = new Native(this);
    if (m_config) {
	m_config->getConfParam("maxfsoccuppc", &m_maxFsOccupPc);
	m_config->getConfParam("idxflushmb", &m_flushMb);
	m_config->getConfParam("idxmetastoredlen", &m_idxMetaStoredLen);
	m_config->getConfParam("idxtexttruncatelen", &m_idxTextTruncateLen);
    }
}

Db::~Db()
{
    LOGDEB2("Db::~Db\n");
    if (m_ndb == 0)
	return;
    LOGDEB("Db::~Db: isopen " << m_ndb->m_isopen << " m_iswritable " <<
           m_ndb->m_iswritable << "\n");
    i_close(true);
#ifdef RCL_USE_ASPELL
    delete m_aspell;
#endif
    delete m_config;
}

vector<string> Db::getStemmerNames()
{
    vector<string> res;
    stringToStrings(Xapian::Stem::get_available_languages(), res);
    return res;
}


bool Db::open(OpenMode mode, OpenError *error)
{
    if (error)
	*error = DbOpenMainDb;

    if (m_ndb == 0 || m_config == 0) {
	m_reason = "Null configuration or Xapian Db";
	return false;
    }
    LOGDEB("Db::open: m_isopen " << m_ndb->m_isopen << " m_iswritable " <<
           m_ndb->m_iswritable << " mode " << mode << "\n");

    if (m_ndb->m_isopen) {
	// We used to return an error here but I see no reason to
	if (!close())
	    return false;
    }
    if (!m_config->getStopfile().empty())
	m_stops.setFile(m_config->getStopfile());

    string dir = m_config->getDbDir();
    string ermsg;
    try {
	switch (mode) {
	case DbUpd:
	case DbTrunc: 
            m_ndb->openWrite(dir, mode);
            updated = vector<bool>(m_ndb->xwdb.get_lastdocid() + 1, false);
            // We used to open a readonly object in addition to the
            // r/w one because some operations were faster when
            // performed through a Database: no forced flushes on
            // allterms_begin(), used in subDocs(). This issue has
            // been gone for a long time (now: Xapian 1.2) and the
            // separate objects seem to trigger other Xapian issues,
            // so the query db is now a clone of the update one.
            m_ndb->xrdb = m_ndb->xwdb;
            LOGDEB("Db::open: lastdocid: " <<m_ndb->xwdb.get_lastdocid()<<"\n");
	    break;
	case DbRO:
	default:
            m_ndb->openRead(dir);
            for (auto& db : m_extraDbs) {
		if (error)
		    *error = DbOpenExtraDb;
		LOGDEB("Db::Open: adding query db [" << &db << "]\n");
                // An error here used to be non-fatal (1.13 and older)
                // but I can't see why
                m_ndb->xrdb.add_database(Xapian::Database(db));
	    }
	    break;
	}
	if (error)
	    *error = DbOpenMainDb;

	// Check index format version. Must not try to check a just created or
	// truncated db
	if (mode != DbTrunc && m_ndb->xrdb.get_doccount() > 0) {
	    string version = m_ndb->xrdb.get_metadata(cstr_RCL_IDX_VERSION_KEY);
	    if (version.compare(cstr_RCL_IDX_VERSION)) {
		m_ndb->m_noversionwrite = true;
		LOGERR("Rcl::Db::open: file index [" << version <<
                       "], software [" << cstr_RCL_IDX_VERSION << "]\n");
		throw Xapian::DatabaseError("Recoll index version mismatch",
					    "", "");
	    }
	}
	m_mode = mode;
	m_ndb->m_isopen = true;
	m_basedir = dir;
	if (error)
	    *error = DbOpenNoError;
	return true;
    } XCATCHERROR(ermsg);

    m_reason = ermsg;
    LOGERR("Db::open: exception while opening [" <<dir<< "]: " << ermsg << "\n");
    return false;
}

bool Db::storesDocText()
{
    if (!m_ndb || !m_ndb->m_isopen) {
        LOGERR("Db::storesDocText: called on non-opened db\n");
        return false;
    }
    return m_ndb->m_storetext;
}

bool Db::getDocRawText(Doc& doc)
{
    if (!m_ndb || !m_ndb->m_isopen) {
        LOGERR("Db::getDocRawText: called on non-opened db\n");
        return false;
    }
    return m_ndb->getRawText(doc.xdocid, doc.text);
}

// Note: xapian has no close call, we delete and recreate the db
bool Db::close()
{
    LOGDEB1("Db::close()\n");
    return i_close(false);
}
bool Db::i_close(bool final)
{
    if (m_ndb == 0)
	return false;
    LOGDEB("Db::i_close(" << final << "): m_isopen " << m_ndb->m_isopen <<
           " m_iswritable " << m_ndb->m_iswritable << "\n");
    if (m_ndb->m_isopen == false && !final) 
	return true;

    string ermsg;
    try {
	bool w = m_ndb->m_iswritable;
	if (w) {
#ifdef IDX_THREADS
	    waitUpdIdle();
#endif
	    if (!m_ndb->m_noversionwrite)
		m_ndb->xwdb.set_metadata(cstr_RCL_IDX_VERSION_KEY, 
					 cstr_RCL_IDX_VERSION);
	    LOGDEB("Rcl::Db:close: xapian will close. May take some time\n");
	}
	deleteZ(m_ndb);
	if (w)
	    LOGDEB("Rcl::Db:close() xapian close done.\n");
	if (final) {
	    return true;
	}
	m_ndb = new Native(this);
	if (m_ndb) {
	    return true;
	}
	LOGERR("Rcl::Db::close(): cant recreate db object\n");
	return false;
    } XCATCHERROR(ermsg);
    LOGERR("Db:close: exception while deleting db: " << ermsg << "\n");
    return false;
}

// Reopen the db with a changed list of additional dbs
bool Db::adjustdbs()
{
    if (m_mode != DbRO) {
        LOGERR("Db::adjustdbs: mode not RO\n");
        return false;
    }
    if (m_ndb && m_ndb->m_isopen) {
	if (!close())
	    return false;
	if (!open(m_mode)) {
	    return false;
	}
    }
    return true;
}

int Db::docCnt()
{
    int res = -1;
    if (!m_ndb || !m_ndb->m_isopen)
        return -1;

    XAPTRY(res = m_ndb->xrdb.get_doccount(), m_ndb->xrdb, m_reason);

    if (!m_reason.empty()) {
        LOGERR("Db::docCnt: got error: " << m_reason << "\n");
        return -1;
    }
    return res;
}

int Db::termDocCnt(const string& _term)
{
    int res = -1;
    if (!m_ndb || !m_ndb->m_isopen)
        return -1;

    string term = _term;
    if (o_index_stripchars)
	if (!unacmaybefold(_term, term, "UTF-8", UNACOP_UNACFOLD)) {
	    LOGINFO("Db::termDocCnt: unac failed for [" << _term << "]\n");
	    return 0;
	}

    if (m_stops.isStop(term)) {
	LOGDEB1("Db::termDocCnt [" << term << "] in stop list\n");
	return 0;
    }

    XAPTRY(res = m_ndb->xrdb.get_termfreq(term), m_ndb->xrdb, m_reason);

    if (!m_reason.empty()) {
        LOGERR("Db::termDocCnt: got error: " << m_reason << "\n");
        return -1;
    }
    return res;
}

bool Db::addQueryDb(const string &_dir) 
{
    string dir = _dir;
    LOGDEB0("Db::addQueryDb: ndb " << m_ndb << " iswritable " <<
            ((m_ndb)?m_ndb->m_iswritable:0) << " db [" << dir << "]\n");
    if (!m_ndb)
	return false;
    if (m_ndb->m_iswritable)
	return false;
    dir = path_canon(dir);
    if (find(m_extraDbs.begin(), m_extraDbs.end(), dir) == m_extraDbs.end()) {
	m_extraDbs.push_back(dir);
    }
    return adjustdbs();
}

bool Db::rmQueryDb(const string &dir)
{
    if (!m_ndb)
	return false;
    if (m_ndb->m_iswritable)
	return false;
    if (dir.empty()) {
	m_extraDbs.clear();
    } else {
	vector<string>::iterator it = find(m_extraDbs.begin(), 
					 m_extraDbs.end(), dir);
	if (it != m_extraDbs.end()) {
	    m_extraDbs.erase(it);
	}
    }
    return adjustdbs();
}

// Determining what index a doc result comes from is based on the
// modulo of the docid against the db count. Ref:
// http://trac.xapian.org/wiki/FAQ/MultiDatabaseDocumentID
bool Db::fromMainIndex(const Doc& doc)
{
    return m_ndb->whatDbIdx(doc.xdocid) == 0;
}

std::string Db::whatIndexForResultDoc(const Doc& doc)
{
    size_t idx = m_ndb->whatDbIdx(doc.xdocid);
    if (idx == (size_t)-1) {
        LOGERR("whatIndexForResultDoc: whatDbIdx returned -1 for " <<
               doc.xdocid << endl);
        return string();
    }
    // idx is [0..m_extraDbs.size()] 0 is for the main index, else
    // idx-1 indexes into m_extraDbs
    if (idx == 0) {
        return m_basedir;
    } else {
        return m_extraDbs[idx-1];
    }
}

size_t Db::Native::whatDbIdx(Xapian::docid id)
{
    LOGDEB1("Db::whatDbIdx: xdocid " << id << ", " <<
            m_rcldb->m_extraDbs.size() << " extraDbs\n");
    if (id == 0) 
	return (size_t)-1;
    if (m_rcldb->m_extraDbs.size() == 0)
	return 0;
    return (id - 1) % (m_rcldb->m_extraDbs.size() + 1);
}

// Return the docid inside the non-combined index
Xapian::docid Db::Native::whatDbDocid(Xapian::docid docid_combined)
{
    if (m_rcldb->m_extraDbs.size() == 0)
	return docid_combined;
    return (docid_combined - 1) / (m_rcldb->m_extraDbs.size() + 1) + 1;
}

bool Db::testDbDir(const string &dir, bool *stripped_p)
{
    string aerr;
    bool mstripped = true;
    LOGDEB("Db::testDbDir: [" << dir << "]\n");
    try {
	Xapian::Database db(dir);
	// If the prefix for mimetype is wrapped, it's an unstripped
	// index. T has been in use in recoll since the beginning and
	// all documents have a T field (possibly empty).
	Xapian::TermIterator term = db.allterms_begin(":T:");
	if (term == db.allterms_end()) {
	    mstripped = true;
        } else {
	    mstripped = false;
        }
        LOGDEB("testDbDir: " << dir << " is a " <<
               (mstripped ? "stripped" : "raw") << " index\n");
    } XCATCHERROR(aerr);
    if (!aerr.empty()) {
	LOGERR("Db::Open: error while trying to open database from [" <<
               dir << "]: " << aerr << "\n");
	return false;
    }
    if (stripped_p) 
	*stripped_p = mstripped;

    return true;
}

bool Db::isopen()
{
    if (m_ndb == 0)
	return false;
    return m_ndb->m_isopen;
}

// Try to translate field specification into field prefix. 
bool Db::fieldToTraits(const string& fld, const FieldTraits **ftpp,
                       bool isquery)
{
    if (m_config && m_config->getFieldTraits(fld, ftpp, isquery))
	return true;

    *ftpp = 0;
    return false;
}

// The splitter breaks text into words and adds postings to the Xapian
// document. We use a single object to split all of the document
// fields and position jumps to separate fields
class TextSplitDb : public TextSplitP {
 public:
    Xapian::Document &doc;   // Xapian document 
    // Base for document section. Gets large increment when we change
    // sections, to avoid cross-section proximity matches.
    Xapian::termpos basepos;
    // Current relative position. This is the remembered value from
    // the splitter callback. The term position is reset for each call
    // to text_to_words(), so that the last value of curpos is the
    // section size (last relative term position), and this is what
    // gets added to basepos in addition to the inter-section increment
    // to compute the first position of the next section.
    Xapian::termpos curpos;
    Xapian::WritableDatabase& wdb;

    TextSplitDb(Xapian::WritableDatabase& _wdb, Xapian::Document &d,
                TermProc *prc)
	: TextSplitP(prc), doc(d), basepos(1), curpos(0), wdb(_wdb)
    {}

    // Reimplement text_to_words to insert the begin and end anchor terms.
    virtual bool text_to_words(const string &in) 
    {
	string ermsg;

	try {
	    // Index the possibly prefixed start term.
	    doc.add_posting(ft.pfx + start_of_field_term, basepos, ft.wdfinc);
	    ++basepos;
	} XCATCHERROR(ermsg);
	if (!ermsg.empty()) {
	    LOGERR("Db: xapian add_posting error " << ermsg << "\n");
	    goto out;
	}

	if (!TextSplitP::text_to_words(in)) {
	    LOGDEB("TextSplitDb: TextSplit::text_to_words failed\n");
	    goto out;
	}

	try {
	    // Index the possibly prefixed end term.
	    doc.add_posting(ft.pfx + end_of_field_term, basepos + curpos + 1,
			    ft.wdfinc);
	    ++basepos;
	} XCATCHERROR(ermsg);
	if (!ermsg.empty()) {
	    LOGERR("Db: xapian add_posting error " << ermsg << "\n");
	    goto out;
	}

    out:
	basepos += curpos + 100;
	return true;
    }

    void setTraits(const FieldTraits& ftp) 
    {
        ft = ftp;
        if (!ft.pfx.empty())
            ft.pfx = wrap_prefix(ft.pfx);
    }

    friend class TermProcIdx;

private:
    FieldTraits ft;
};

class TermProcIdx : public TermProc {
public:
    TermProcIdx() : TermProc(0), m_ts(0), m_lastpagepos(0), m_pageincr(0) {}
    void setTSD(TextSplitDb *ts) {m_ts = ts;}

    bool takeword(const std::string &term, int pos, int, int)
    {
	// Compute absolute position (pos is relative to current segment),
	// and remember relative.
	m_ts->curpos = pos;
	pos += m_ts->basepos;
	// Don't try to add empty term Xapian doesnt like it... Safety check
	// this should not happen.
	if (term.empty())
	    return true;
	string ermsg;
	try {
	    // Index without prefix, using the field-specific weighting
	    LOGDEB1("Emitting term at " << pos << " : [" << term << "]\n");
            if (!m_ts->ft.pfxonly)
                m_ts->doc.add_posting(term, pos, m_ts->ft.wdfinc);

#ifdef TESTING_XAPIAN_SPELL
	    if (Db::isSpellingCandidate(term, false)) {
		m_ts->wdb.add_spelling(term);
	    }
#endif
	    // Index the prefixed term.
	    if (!m_ts->ft.pfx.empty()) {
		m_ts->doc.add_posting(m_ts->ft.pfx + term, pos, 
                                      m_ts->ft.wdfinc);
	    }
	    return true;
	} XCATCHERROR(ermsg);
	LOGERR("Db: xapian add_posting error " << ermsg << "\n");
	return false;
    }
    void newpage(int pos)
    {
	pos += m_ts->basepos;
	if (pos < int(baseTextPosition)) {
	    LOGDEB("newpage: not in body: " << pos << "\n");
	    return;
	}

	m_ts->doc.add_posting(m_ts->ft.pfx + page_break_term, pos);
	if (pos == m_lastpagepos) {
	    m_pageincr++;
	    LOGDEB2("newpage: same pos, pageincr " << m_pageincr <<
                    " lastpagepos " << m_lastpagepos << "\n");
	} else {
	    LOGDEB2("newpage: pos change, pageincr " << m_pageincr <<
                    " lastpagepos " << m_lastpagepos << "\n");
	    if (m_pageincr > 0) {
		// Remember the multiple page break at this position
		unsigned int relpos = m_lastpagepos - baseTextPosition;
		LOGDEB2("Remembering multiple page break. Relpos " << relpos <<
                        " cnt " << m_pageincr << "\n");
		m_pageincrvec.push_back(pair<int, int>(relpos, m_pageincr));
	    }
	    m_pageincr = 0;
	}
	m_lastpagepos = pos;
    }

    virtual bool flush()
    {
	if (m_pageincr > 0) {
	    unsigned int relpos = m_lastpagepos - baseTextPosition;
	    LOGDEB2("Remembering multiple page break. Position " << relpos <<
                    " cnt " << m_pageincr << "\n");
	    m_pageincrvec.push_back(pair<int, int>(relpos, m_pageincr));
	    m_pageincr = 0;
	}
	return TermProc::flush();
    }

    TextSplitDb *m_ts;
    // Auxiliary page breaks data for positions with multiple page breaks.
    int m_lastpagepos;
    // increment of page breaks at same pos. Normally 0, 1.. when several
    // breaks at the same pos
    int m_pageincr; 
    vector <pair<int, int> > m_pageincrvec;
};


// At the moment, we normally use the Xapian speller for Katakana and
// aspell for everything else
bool Db::getSpellingSuggestions(const string& word, vector<string>& suggs)
{
    LOGDEB("Db::getSpellingSuggestions:[" << word << "]\n");
    suggs.clear();
    if (nullptr == m_ndb) {
	return false;
    }

    string term = word;

    if (isSpellingCandidate(term, true)) {
        // Term is candidate for aspell processing
#ifdef RCL_USE_ASPELL
        bool noaspell = false;
        m_config->getConfParam("noaspell", &noaspell);
        if (noaspell) {
            return false;
        }
        if (nullptr == m_aspell) {
            m_aspell = new Aspell(m_config);
            if (m_aspell) {
                string reason;
                m_aspell->init(reason);
                if (!m_aspell->ok()) {
                    LOGDEB("Aspell speller init failed: " << reason << endl);
                    delete m_aspell;
                    m_aspell = 0;
                }
            }
        }

        if (nullptr == m_aspell) {
            LOGERR("Db::getSpellingSuggestions: aspell not initialized\n");
            return false;
        }

        list<string> asuggs;
        string reason;
        if (!m_aspell->suggest(*this, term, asuggs, reason)) {
            LOGERR("Db::getSpellingSuggestions: aspell failed: " << reason <<
                   "\n");
            return false;
        }
        suggs = vector<string>(asuggs.begin(), asuggs.end());
#endif
    } else {
#ifdef TESTING_XAPIAN_SPELL
        // Was not aspell candidate (e.g.: katakana). Maybe use Xapian
        // speller?
        if (isSpellingCandidate(term, false)) {
            if (!o_index_stripchars) {
                if (!unacmaybefold(word, term, "UTF-8", UNACOP_UNACFOLD)) {
                    LOGINFO("Db::getSpelling: unac failed for [" << word <<
                            "]\n");
                    return false;
                }
            }
            string sugg = m_ndb->xrdb.get_spelling_suggestion(term);
            if (!sugg.empty()) {
                suggs.push_back(sugg);
            }
        }
#endif
    }
    return true;
}

// Let our user set the parameters for abstract processing
void Db::setAbstractParams(int idxtrunc, int syntlen, int syntctxlen)
{
    LOGDEB1("Db::setAbstractParams: trunc " << idxtrunc << " syntlen " <<
            syntlen << " ctxlen " << syntctxlen << "\n");
    if (idxtrunc >= 0)
	m_idxAbsTruncLen = idxtrunc;
    if (syntlen > 0)
	m_synthAbsLen = syntlen;
    if (syntctxlen > 0)
	m_synthAbsWordCtxLen = syntctxlen;
}

bool Db::setSynGroupsFile(const string& fn)
{
    return m_syngroups.setfile(fn);
}
    
static const string cstr_nc("\n\r\x0c\\");
#define RECORD_APPEND(R, NM, VAL) {R += NM + "=" + VAL + "\n";}

// Add document in internal form to the database: index the terms in
// the title abstract and body and add special terms for file name,
// date, mime type etc. , create the document data record (more
// metadata), and update database
bool Db::addOrUpdate(const string &udi, const string &parent_udi, Doc &doc)
{
    LOGDEB("Db::add: udi [" << udi << "] parent [" << parent_udi << "]\n");
    if (m_ndb == 0)
	return false;

    // This document is potentially going to be passed to the index
    // update thread. The reference counters are not mt-safe, so we
    // need to do this through a pointer. The reference is just there
    // to avoid changing too much code (the previous version passed a copy).
    Xapian::Document *newdocument_ptr = new Xapian::Document;
    Xapian::Document &newdocument(*newdocument_ptr);
    
    // The term processing pipeline:
    TermProcIdx tpidx;
    TermProc *nxt = &tpidx;
    TermProcStop tpstop(nxt, m_stops);nxt = &tpstop;
    //TermProcCommongrams tpcommon(nxt, m_stops); nxt = &tpcommon;

    TermProcPrep tpprep(nxt);
    if (o_index_stripchars)
	nxt = &tpprep;

    TextSplitDb splitter(m_ndb->xwdb, newdocument, nxt);
    tpidx.setTSD(&splitter);

    // Udi unique term: this is used for file existence/uptodate
    // checks, and unique id for the replace_document() call.
    string uniterm = make_uniterm(udi);
    string rawztext; // Doc compressed text

    if (doc.onlyxattr) {
	// Only updating an existing doc with new extended attributes
	// data.  Need to read the old doc and its data record
	// first. This is so different from the normal processing that
	// it uses a fully separate code path (with some duplication
	// unfortunately)
	if (!m_ndb->docToXdocXattrOnly(&splitter, udi, doc, newdocument)) {
            delete newdocument_ptr;
	    return false;
        }
    } else {

        if (m_idxTextTruncateLen > 0) {
            doc.text = truncate_to_word(doc.text, m_idxTextTruncateLen);
        }
        
	// If the ipath is like a path, index the last element. This is
	// for compound documents like zip and chm for which the filter
	// uses the file path as ipath. 
	if (!doc.ipath.empty() && 
	    doc.ipath.find_first_not_of("0123456789") != string::npos) {
	    string utf8ipathlast;
	    // There is no way in hell we could have an idea of the
	    // charset here, so let's hope it's ascii or utf-8. We call
	    // transcode to strip the bad chars and pray
	    if (transcode(path_getsimple(doc.ipath), utf8ipathlast,
			  "UTF-8", "UTF-8")) {
		splitter.text_to_words(utf8ipathlast);
	    }
	}

	// Split and index the path from the url for path-based filtering
	{
	    string path = url_gpathS(doc.url);

#ifdef _WIN32
            // Windows file names are case-insensitive, so we
            // translate to UTF-8 and lowercase
            string upath = compute_utf8fn(m_config, path, false);            
            unacmaybefold(upath, path, "UTF-8", UNACOP_FOLD);
#endif

	    vector<string> vpath;
	    stringToTokens(path, vpath, "/");
	    // If vpath is not /, the last elt is the file/dir name, not a
	    // part of the path.
	    if (vpath.size())
		vpath.resize(vpath.size()-1);
	    splitter.curpos = 0;
	    newdocument.add_posting(wrap_prefix(pathelt_prefix),
				    splitter.basepos + splitter.curpos++);
	    for (vector<string>::iterator it = vpath.begin(); 
		 it != vpath.end(); it++){
		if (it->length() > 230) {
		    // Just truncate it. May still be useful because
		    // of wildcards
		    *it = it->substr(0, 230);
		}
		newdocument.add_posting(wrap_prefix(pathelt_prefix) + *it, 
					splitter.basepos + splitter.curpos++);
	    }
            splitter.basepos += splitter.curpos + 100;
	}

	// Index textual metadata.  These are all indexed as text with
	// positions, as we may want to do phrase searches with them (this
	// makes no sense for keywords by the way).
	//
	// The order has no importance, and we set a position gap of 100
	// between fields to avoid false proximity matches.
	for (const auto& entry: doc.meta) {
	    if (entry.second.empty()) {
                continue;
            }
            const FieldTraits *ftp{nullptr};
            fieldToTraits(entry.first, &ftp);
            if (ftp && ftp->valueslot) {
                LOGDEB("Adding value: for field " << entry.first << " slot "
                       << ftp->valueslot << endl);
                add_field_value(newdocument, *ftp, entry.second);
            }

            // There was an old comment here about not testing for
            // empty prefix, and we indeed did not test. I don't think
            // that it makes sense any more (and was in disagreement
            // with the LOG message. Really now: no prefix: no
            // indexing.
            if (ftp && !ftp->pfx.empty()) {
                LOGDEB0("Db::add: field [" << entry.first << "] pfx [" <<
                        ftp->pfx << "] inc " << ftp->wdfinc << ": [" <<
                        entry.second << "]\n");
                splitter.setTraits(*ftp);
                if (!splitter.text_to_words(entry.second)) {
                    LOGDEB("Db::addOrUpdate: split failed for " <<
                           entry.first << "\n");
                }
            } else {
                LOGDEB0("Db::add: no prefix for field [" <<
                        entry.first << "], no indexing\n");
            }
	}

        // Reset to no prefix and default params
        splitter.setTraits(FieldTraits());

	if (splitter.curpos < baseTextPosition)
	    splitter.basepos = baseTextPosition;

	// Split and index body text
	LOGDEB2("Db::add: split body: [" << doc.text << "]\n");

#ifdef TEXTSPLIT_STATS
	splitter.resetStats();
#endif
	if (!splitter.text_to_words(doc.text)) {
	    LOGDEB("Db::addOrUpdate: split failed for main text\n");
        } else {
            if (m_ndb->m_storetext) {
                ZLibUtBuf buf;
                deflateToBuf(doc.text.c_str(), doc.text.size(), buf);
                rawztext.assign(buf.getBuf(), buf.getCnt());
            }
        }

#ifdef TEXTSPLIT_STATS
	// Reject bad data. unrecognized base64 text is characterized by
	// high avg word length and high variation (because there are
	// word-splitters like +/ inside the data).
	TextSplit::Stats::Values v = splitter.getStats();
	// v.avglen > 15 && v.sigma > 12 
	if (v.count > 200 && (v.avglen > 10 && v.sigma / v.avglen > 0.8)) {
	    LOGINFO("RclDb::addOrUpdate: rejecting doc for bad stats count " <<
                    v.count << " avglen " << v.avglen << " sigma " << v.sigma <<
                    " url [" << doc.url << "] ipath [" << doc.ipath <<
                    "] text " << doc.text << "\n");
            delete newdocument_ptr;
	    return true;
	}
#endif

	////// Special terms for other metadata. No positions for these.
	// Mime type
	newdocument.add_boolean_term(wrap_prefix(mimetype_prefix) + doc.mimetype);

	// Simple file name indexed unsplit for specific "file name"
	// searches. This is not the same as a filename: clause inside the
	// query language.
	// We also add a term for the filename extension if any.
	string utf8fn;
	if (doc.getmeta(Doc::keyfn, &utf8fn) && !utf8fn.empty()) {
	    string fn;
	    if (unacmaybefold(utf8fn, fn, "UTF-8", UNACOP_UNACFOLD)) {
		// We should truncate after extracting the extension,
		// but this is a pathological case anyway
		if (fn.size() > 230)
		    utf8truncate(fn, 230);
		string::size_type pos = fn.rfind('.');
		if (pos != string::npos && pos != fn.length() - 1) {
		    newdocument.add_boolean_term(wrap_prefix(fileext_prefix) + 
						 fn.substr(pos + 1));
		}
		newdocument.add_term(wrap_prefix(unsplitfilename_prefix) + fn,0);
	    }
	}

	newdocument.add_boolean_term(uniterm);
	// Parent term. This is used to find all descendents, mostly
	// to delete them when the parent goes away
	if (!parent_udi.empty()) {
	    newdocument.add_boolean_term(make_parentterm(parent_udi));
	}

	// Fields used for selecting by date. Note that this only
	// works for years AD 0-9999 (no crash elsewhere, but things
	// won't work).
	time_t mtime = atoll(doc.dmtime.empty() ? doc.fmtime.c_str() : 
			     doc.dmtime.c_str());
        struct tm tmb;
        localtime_r(&mtime, &tmb);
        char buf[50]; // It's actually 9, but use 50 to suppress warnings.
        snprintf(buf, 50, "%04d%02d%02d",
                 tmb.tm_year+1900, tmb.tm_mon + 1, tmb.tm_mday);
            
	// Date (YYYYMMDD)
	newdocument.add_boolean_term(wrap_prefix(xapday_prefix) + string(buf)); 
	// Month (YYYYMM)
	buf[6] = '\0';
	newdocument.add_boolean_term(wrap_prefix(xapmonth_prefix) + string(buf));
	// Year (YYYY)
	buf[4] = '\0';
	newdocument.add_boolean_term(wrap_prefix(xapyear_prefix) + string(buf)); 


	//////////////////////////////////////////////////////////////////
	// Document data record. omindex has the following nl separated fields:
	// - url
	// - sample
	// - caption (title limited to 100 chars)
	// - mime type 
	//
	// The title, author, abstract and keywords fields are special,
	// they always get stored in the document data
	// record. Configurable other fields can be, too.
	//
	// We truncate stored fields abstract, title and keywords to
	// reasonable lengths and suppress newlines (so that the data
	// record can keep a simple syntax)

	string record;
	RECORD_APPEND(record, Doc::keyurl, doc.url);
	RECORD_APPEND(record, Doc::keytp, doc.mimetype);
	// We left-zero-pad the times so that they are lexico-sortable
	leftzeropad(doc.fmtime, 11);
	RECORD_APPEND(record, Doc::keyfmt, doc.fmtime);
	if (!doc.dmtime.empty()) {
	    leftzeropad(doc.dmtime, 11);
	    RECORD_APPEND(record, Doc::keydmt, doc.dmtime);
	}
	RECORD_APPEND(record, Doc::keyoc, doc.origcharset);

	if (doc.fbytes.empty())
	    doc.fbytes = doc.pcbytes;

	if (!doc.fbytes.empty()) {
	    RECORD_APPEND(record, Doc::keyfs, doc.fbytes);
	    leftzeropad(doc.fbytes, 12);
	    newdocument.add_value(VALUE_SIZE, doc.fbytes);
	}
	if (doc.haschildren) {
	    newdocument.add_boolean_term(has_children_term);
	}	
	if (!doc.pcbytes.empty())
	    RECORD_APPEND(record, Doc::keypcs, doc.pcbytes);
	char sizebuf[30]; 
	sprintf(sizebuf, "%u", (unsigned int)doc.text.length());
	RECORD_APPEND(record, Doc::keyds, sizebuf);

	// Note that we add the signature both as a value and in the data record
	if (!doc.sig.empty()) {
	    RECORD_APPEND(record, Doc::keysig, doc.sig);
	    newdocument.add_value(VALUE_SIG, doc.sig);
	}

	if (!doc.ipath.empty())
	    RECORD_APPEND(record, Doc::keyipt, doc.ipath);

        // Fields from the Meta array. Handle title specially because it has a 
        // different name inside the data record (history...)
        string& ttref = doc.meta[Doc::keytt];
        ttref = neutchars(truncate_to_word(ttref, m_idxMetaStoredLen), cstr_nc);
	if (!ttref.empty()) {
	    RECORD_APPEND(record, cstr_caption, ttref);
            ttref.clear();
        }

	// If abstract is empty, we make up one with the beginning of the
	// document. This is then not indexed, but part of the doc data so
	// that we can return it to a query without having to decode the
	// original file.
	// Note that the map accesses by operator[] create empty entries if they
	// don't exist yet.
        if (m_idxAbsTruncLen > 0) {
            string& absref = doc.meta[Doc::keyabs];
            trimstring(absref, " \t\r\n");
            if (absref.empty()) {
                if (!doc.text.empty())
                    absref = cstr_syntAbs + 
                        neutchars(truncate_to_word(doc.text, m_idxAbsTruncLen), 
                                  cstr_nc);
            } else {
                absref = neutchars(truncate_to_word(absref, m_idxAbsTruncLen), 
                                   cstr_nc);
            }
            // Do the append here to avoid the different truncation done
            // in the regular "stored" loop
            if (!absref.empty()) {
                RECORD_APPEND(record, Doc::keyabs, absref);
                absref.clear();
            }
        }
        
        // Append all regular "stored" meta fields
	const set<string>& stored = m_config->getStoredFields();
	for (set<string>::const_iterator it = stored.begin();
	     it != stored.end(); it++) {
	    string nm = m_config->fieldCanon(*it);
	    if (!doc.meta[nm].empty()) {
		string value = 
		    neutchars(truncate_to_word(doc.meta[nm], 
                                               m_idxMetaStoredLen), cstr_nc);
		RECORD_APPEND(record, nm, value);
	    }
	}

        // At this point, if the document "filename" field was empty,
        // try to store the "container file name" value. This is done
        // after indexing because we don't want search matches on
        // this, but the filename is often useful for display
        // purposes.
        const string *fnp = 0;
        if (!doc.peekmeta(Rcl::Doc::keyfn, &fnp) || fnp->empty()) {
            if (doc.peekmeta(Rcl::Doc::keytcfn, &fnp) && !fnp->empty()) {
		string value = 
		    neutchars(truncate_to_word(*fnp, 
                                               m_idxMetaStoredLen), cstr_nc);
		RECORD_APPEND(record, Rcl::Doc::keyfn, value);
            }
        }

	// If empty pages (multiple break at same pos) were recorded, save
	// them (this is because we have no way to record them in the
	// Xapian list
	if (!tpidx.m_pageincrvec.empty()) {
	    ostringstream multibreaks;
	    for (unsigned int i = 0; i < tpidx.m_pageincrvec.size(); i++) {
		if (i != 0)
		    multibreaks << ",";
		multibreaks << tpidx.m_pageincrvec[i].first << "," << 
		    tpidx.m_pageincrvec[i].second;
	    }
	    RECORD_APPEND(record, string(cstr_mbreaks), multibreaks.str());
	}
    
	// If the file's md5 was computed, add value and term. 
	// The value is optionally used for query result duplicate elimination, 
	// and the term to find the duplicates.
	// We don't do this for empty docs.
	const string *md5;
	if (doc.peekmeta(Doc::keymd5, &md5) && !md5->empty() &&
	    md5->compare(cstr_md5empty)) {
	    string digest;
	    MD5HexScan(*md5, digest);
	    newdocument.add_value(VALUE_MD5, digest);
	    newdocument.add_boolean_term(wrap_prefix("XM") + *md5);
	}

	LOGDEB0("Rcl::Db::add: new doc record:\n" << record << "\n");
	newdocument.set_data(record);
    }
#ifdef IDX_THREADS
    if (m_ndb->m_havewriteq) {
	DbUpdTask *tp = new DbUpdTask(
            DbUpdTask::AddOrUpdate, udi, uniterm, newdocument_ptr,
            doc.text.length(), rawztext);
	if (!m_ndb->m_wqueue.put(tp)) {
	    LOGERR("Db::addOrUpdate:Cant queue task\n");
            delete newdocument_ptr;
	    return false;
	} else {
	    return true;
	}
    }
#endif

    return m_ndb->addOrUpdateWrite(udi, uniterm, newdocument_ptr,
				   doc.text.length(), rawztext);
}

bool Db::Native::docToXdocXattrOnly(TextSplitDb *splitter, const string &udi, 
				    Doc &doc, Xapian::Document& xdoc)
{
    LOGDEB0("Db::docToXdocXattrOnly\n");
#ifdef IDX_THREADS
    std::unique_lock<std::mutex> lock(m_mutex);
#endif

    // Read existing document and its data record
    if (getDoc(udi, 0, xdoc) == 0) {
	LOGERR("docToXdocXattrOnly: existing doc not found\n");
	return false;
    }
    string data;
    XAPTRY(data = xdoc.get_data(), xrdb, m_rcldb->m_reason);
    if (!m_rcldb->m_reason.empty()) {
        LOGERR("Db::xattrOnly: got error: " << m_rcldb->m_reason << "\n");
        return false;
    }

    // Clear the term lists for the incoming fields and index the new values
    map<string, string>::iterator meta_it;
    for (const auto& ent : doc.meta) {
	const FieldTraits *ftp;
	if (!m_rcldb->fieldToTraits(ent.first, &ftp) || ftp->pfx.empty()) {
	    LOGDEB0("Db::xattrOnly: no prefix for field [" <<
                    ent.first << "], skipped\n");
	    continue;
	}
	// Clear the previous terms for the field
	clearField(xdoc, ftp->pfx, ftp->wdfinc);
	LOGDEB0("Db::xattrOnly: field [" << ent.first << "] pfx [" <<
                ftp->pfx << "] inc " << ftp->wdfinc << ": [" <<
                ent.second << "]\n");
	splitter->setTraits(*ftp);
	if (!splitter->text_to_words(ent.second)) {
	    LOGDEB("Db::xattrOnly: split failed for " << ent.first << "\n");
        }
    }
    xdoc.add_value(VALUE_SIG, doc.sig);

    // Parse current data record into a dict for ease of processing
    ConfSimple datadic(data);
    if (!datadic.ok()) {
	LOGERR("db::docToXdocXattrOnly: failed turning data rec to dict\n");
	return false;
    }

    // For each "stored" field, check if set in doc metadata and
    // update the value if it is
    const set<string>& stored = m_rcldb->m_config->getStoredFields();
    for (set<string>::const_iterator it = stored.begin();
	 it != stored.end(); it++) {
	string nm = m_rcldb->m_config->fieldCanon(*it);
	if (doc.getmeta(nm, 0)) {
	    string value = neutchars(
                truncate_to_word(doc.meta[nm], m_rcldb->m_idxMetaStoredLen), 
                cstr_nc);
	    datadic.set(nm, value, "");
	}
    }

    // Recreate the record. We want to do this with the local RECORD_APPEND
    // method for consistency in format, instead of using ConfSimple print
    vector<string> names = datadic.getNames("");
    data.clear();
    for (vector<string>::const_iterator it = names.begin(); 
	 it != names.end(); it++) {
	string value;
	datadic.get(*it, value, "");
	RECORD_APPEND(data, *it, value);
    }
    RECORD_APPEND(data, Doc::keysig, doc.sig);
    xdoc.set_data(data);
    return true;
}

#ifdef IDX_THREADS
void Db::waitUpdIdle()
{
    if (m_ndb->m_iswritable && m_ndb->m_havewriteq) {
	Chrono chron;
	m_ndb->m_wqueue.waitIdle();
	// We flush here just for correct measurement of the thread work time
	string ermsg;
	try {
	    m_ndb->xwdb.commit();
	} XCATCHERROR(ermsg);
	if (!ermsg.empty()) {
	    LOGERR("Db::waitUpdIdle: flush() failed: " << ermsg << "\n");
	}
	m_ndb->m_totalworkns += chron.nanos();
	LOGINFO("Db::waitUpdIdle: total xapian work " <<
                lltodecstr(m_ndb->m_totalworkns/1000000) << " mS\n");
    }
}
#endif

// Flush when idxflushmbs is reached
bool Db::maybeflush(int64_t moretext)
{
    if (m_flushMb > 0) {
	m_curtxtsz += moretext;
	if ((m_curtxtsz - m_flushtxtsz) / MB >= m_flushMb) {
	    LOGINF("Db::add/delete: txt size >= " << m_flushMb <<
                   " Mb, flushing\n");
	    return doFlush();
	}
    }
    return true;
}

bool Db::doFlush()
{
    if (!m_ndb) {
	LOGERR("Db::doFLush: no ndb??\n");
	return false;
    }
    string ermsg;
    try {
	m_ndb->xwdb.commit();
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR("Db::doFlush: flush() failed: " << ermsg << "\n");
	return false;
    }
    m_flushtxtsz = m_curtxtsz;
    return true;
}

void Db::setExistingFlags(const string& udi, unsigned int docid)
{
    if (m_mode == DbRO)
        return;
    if (docid == (unsigned int)-1) {
        LOGERR("Db::setExistingFlags: called with bogus docid !!\n");
        return;
    }
#ifdef IDX_THREADS
    std::unique_lock<std::mutex> lock(m_ndb->m_mutex);
#endif
    i_setExistingFlags(udi, docid);
}

void Db::i_setExistingFlags(const string& udi, unsigned int docid)
{
    // Set the up to date flag for the document and its
    // subdocs. needUpdate() can also be called at query time (for
    // preview up to date check), so no error if the updated bitmap is
    // of size 0
    if (docid >= updated.size()) {
        if (updated.size())
            LOGERR("needUpdate: existing docid beyond updated.size(). Udi [" <<
                   udi << "], docid " << docid << ", updated.size() " <<
                   updated.size() << "\n");
        return;
    } else {
        updated[docid] = true;
    }

    // Set the existence flag for all the subdocs (if any)
    vector<Xapian::docid> docids;
    if (!m_ndb->subDocs(udi, 0, docids)) {
        LOGERR("Rcl::Db::needUpdate: can't get subdocs\n");
        return;
    }
    for (auto docid : docids) {
        if (docid < updated.size()) {
            LOGDEB2("Db::needUpdate: docid " << docid << " set\n");
            updated[docid] = true;
        }
    }
}

// Test if doc given by udi has changed since last indexed (test sigs)
bool Db::needUpdate(const string &udi, const string& sig, 
                    unsigned int *docidp, string *osigp)
{
    if (m_ndb == 0)
        return false;

    if (osigp)
        osigp->clear();
    if (docidp)
        *docidp = 0;

    // If we are doing an in place or full reset, no need to test.
    if (o_inPlaceReset || m_mode == DbTrunc) {
	// For in place reset, pretend the doc existed, to enable
	// subdoc purge. The value is only used as a boolean in this case.
	if (docidp && o_inPlaceReset) {
	    *docidp = -1;
        }
	return true;
    }

    string uniterm = make_uniterm(udi);
    string ermsg;

#ifdef IDX_THREADS
    // Need to protect against interaction with the doc update/insert
    // thread which also updates the existence map, and even multiple
    // accesses to the readonly Xapian::Database are not allowed
    // anyway
    std::unique_lock<std::mutex> lock(m_ndb->m_mutex);
#endif

    // Try to find the document indexed by the uniterm. 
    Xapian::PostingIterator docid;
    XAPTRY(docid = m_ndb->xrdb.postlist_begin(uniterm), m_ndb->xrdb, m_reason);
    if (!m_reason.empty()) {
        LOGERR("Db::needUpdate: xapian::postlist_begin failed: " <<
               m_reason << "\n");
        return false;
    }
    if (docid == m_ndb->xrdb.postlist_end(uniterm)) {
        // No document exists with this path: we do need update
        LOGDEB("Db::needUpdate:yes (new): [" << uniterm << "]\n");
        return true;
    }
    Xapian::Document xdoc;
    XAPTRY(xdoc = m_ndb->xrdb.get_document(*docid), m_ndb->xrdb, m_reason);
    if (!m_reason.empty()) {
        LOGERR("Db::needUpdate: get_document error: " << m_reason << "\n");
        return true;
    }

    if (docidp) {
        *docidp = *docid;
    }

    // Retrieve old file/doc signature from value
    string osig;
    XAPTRY(osig = xdoc.get_value(VALUE_SIG), m_ndb->xrdb, m_reason);
    if (!m_reason.empty()) {
        LOGERR("Db::needUpdate: get_value error: " << m_reason << "\n");
        return true;
    }
    LOGDEB2("Db::needUpdate: oldsig [" << osig << "] new [" << sig << "]\n");

    if (osigp) {
        *osigp = osig;
    }

    // Compare new/old sig
    if (sig != osig) {
        LOGDEB("Db::needUpdate:yes: olsig [" << osig << "] new [" << sig <<
               "] [" << uniterm << "]\n");
        // Db is not up to date. Let's index the file
        return true;
    }

    // Up to date. Set the existance flags in the map for the doc and
    // its subdocs.
    LOGDEB("Db::needUpdate:no: [" << uniterm << "]\n");
    i_setExistingFlags(udi, *docid);
    return false;
}

// Return existing stem db languages
vector<string> Db::getStemLangs()
{
    LOGDEB("Db::getStemLang\n");
    vector<string> langs;
    if (m_ndb == 0 || m_ndb->m_isopen == false)
	return langs;
    StemDb db(m_ndb->xrdb);
    db.getMembers(langs);
    return langs;
}

/**
 * Delete stem db for given language
 */
bool Db::deleteStemDb(const string& lang)
{
    LOGDEB("Db::deleteStemDb(" << lang << ")\n");
    if (m_ndb == 0 || m_ndb->m_isopen == false || !m_ndb->m_iswritable)
	return false;
    XapWritableSynFamily db(m_ndb->xwdb, synFamStem);
    return db.deleteMember(lang);
}

/**
 * Create database of stem to parents associations for a given language.
 * We walk the list of all terms, stem them, and create another Xapian db
 * with documents indexed by a single term (the stem), and with the list of
 * parent terms in the document data.
 */
bool Db::createStemDbs(const vector<string>& langs)
{
    LOGDEB("Db::createStemDbs\n");
    if (m_ndb == 0 || m_ndb->m_isopen == false || !m_ndb->m_iswritable) {
	LOGERR("createStemDb: db not open or not writable\n");
	return false;
    }

    return createExpansionDbs(m_ndb->xwdb, langs);
}

/**
 * This is called at the end of an indexing session, to delete the
 * documents for files that are no longer there. This can ONLY be called
 * after a full file-system tree walk, else the file existence flags will 
 * be wrong.
 */
bool Db::purge()
{
    LOGDEB("Db::purge\n");
    if (m_ndb == 0)
	return false;
    LOGDEB("Db::purge: m_isopen " << m_ndb->m_isopen << " m_iswritable " <<
           m_ndb->m_iswritable << "\n");
    if (m_ndb->m_isopen == false || m_ndb->m_iswritable == false) 
	return false;

#ifdef IDX_THREADS
    // If we manage our own write queue, make sure it's drained and closed
    if (m_ndb->m_havewriteq)
	m_ndb->m_wqueue.setTerminateAndWait();
    // else we need to lock out other top level threads. This is just
    // a precaution as they should have been waited for by the top
    // level actor at this point
    std::unique_lock<std::mutex> lock(m_ndb->m_mutex);
#endif // IDX_THREADS

    // For xapian versions up to 1.0.1, deleting a non-existant
    // document would trigger an exception that would discard any
    // pending update. This could lose both previous added documents
    // or deletions. Adding the flush before the delete pass ensured
    // that any added document would go to the index. Kept here
    // because it doesn't really hurt.
    m_reason.clear();
    try {
        m_ndb->xwdb.commit();
    } XCATCHERROR(m_reason);
    if (!m_reason.empty()) {
        LOGERR("Db::purge: 1st flush failed: " << m_reason << "\n");
        return false;
    }

    // Walk the document array and delete any xapian document whose
    // flag is not set (we did not see its source during indexing).
    int purgecount = 0;
    for (Xapian::docid docid = 1; docid < updated.size(); ++docid) {
	if (!updated[docid]) {
	    if ((purgecount+1) % 100 == 0) {
		try {
		    CancelCheck::instance().checkCancel();
		} catch(CancelExcept) {
		    LOGINFO("Db::purge: partially cancelled\n");
		    break;
		}
	    }

	    try {
		if (m_flushMb > 0) {
		    // We use an average term length of 5 for
		    // estimating the doc sizes which is probably not
		    // accurate but gives rough consistency with what
		    // we do for add/update. I should fetch the doc
		    // size from the data record, but this would be
		    // bad for performance.
		    Xapian::termcount trms = m_ndb->xwdb.get_doclength(docid);
		    maybeflush(trms * 5);
		}
		m_ndb->deleteDocument(docid);
		LOGDEB("Db::purge: deleted document #" << docid << "\n");
	    } catch (const Xapian::DocNotFoundError &) {
		LOGDEB0("Db::purge: document #" << docid << " not found\n");
	    } catch (const Xapian::Error &e) {
		LOGERR("Db::purge: document #" << docid << ": " <<
                       e.get_msg() << "\n");
	    } catch (...) {
		LOGERR("Db::purge: document #" << docid << ": unknown error\n");
	    }
	    purgecount++;
	}
    }

    m_reason.clear();
    try {
        m_ndb->xwdb.commit();
    } XCATCHERROR(m_reason);
    if (!m_reason.empty()) {
        LOGERR("Db::purge: 2nd flush failed: " << m_reason << "\n");
        return false;
    }
    return true;
}

// Test for doc existence.
bool Db::docExists(const string& uniterm)
{
#ifdef IDX_THREADS
    // Need to protect read db against multiaccess. 
    std::unique_lock<std::mutex> lock(m_ndb->m_mutex);
#endif

    string ermsg;
    try {
	Xapian::PostingIterator docid = m_ndb->xrdb.postlist_begin(uniterm);
	if (docid == m_ndb->xrdb.postlist_end(uniterm)) {
	    return false;
        } else {
	    return true;
	}
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR("Db::docExists(" << uniterm << ") " << ermsg << "\n");
    }
    return false;
}

/* Delete document(s) for given unique identifier (doc and descendents) */
bool Db::purgeFile(const string &udi, bool *existed)
{
    LOGDEB("Db:purgeFile: [" << udi << "]\n");
    if (m_ndb == 0 || !m_ndb->m_iswritable)
	return false;

    string uniterm = make_uniterm(udi);
    bool exists = docExists(uniterm);
    if (existed)
	*existed = exists;
    if (!exists)
	return true;

#ifdef IDX_THREADS
    if (m_ndb->m_havewriteq) {
        string rztxt;
	DbUpdTask *tp = new DbUpdTask(DbUpdTask::Delete, udi, uniterm, 
				      0, (size_t)-1, rztxt);
	if (!m_ndb->m_wqueue.put(tp)) {
	    LOGERR("Db::purgeFile:Cant queue task\n");
	    return false;
	} else {
	    return true;
	}
    }
#endif
    /* We get there is IDX_THREADS is not defined or there is no queue */
    return m_ndb->purgeFileWrite(false, udi, uniterm);
}

/* Delete subdocs with an out of date sig. We do this to purge
   obsolete subdocs during a partial update where no general purge
   will be done */
bool Db::purgeOrphans(const string &udi)
{
    LOGDEB("Db:purgeOrphans: [" << udi << "]\n");
    if (m_ndb == 0 || !m_ndb->m_iswritable)
	return false;

    string uniterm = make_uniterm(udi);

#ifdef IDX_THREADS
    if (m_ndb->m_havewriteq) {
        string rztxt;
	DbUpdTask *tp = new DbUpdTask(DbUpdTask::PurgeOrphans, udi, uniterm, 
				      0, (size_t)-1, rztxt);
	if (!m_ndb->m_wqueue.put(tp)) {
	    LOGERR("Db::purgeFile:Cant queue task\n");
	    return false;
	} else {
	    return true;
	}
    }
#endif
    /* We get there is IDX_THREADS is not defined or there is no queue */
    return m_ndb->purgeFileWrite(true, udi, uniterm);
}

bool Db::dbStats(DbStats& res, bool listfailed)
{
    if (!m_ndb || !m_ndb->m_isopen)
	return false;
    Xapian::Database xdb = m_ndb->xrdb;

    XAPTRY(res.dbdoccount = xdb.get_doccount();
           res.dbavgdoclen = xdb.get_avlength();
	   res.mindoclen = xdb.get_doclength_lower_bound();
	   res.maxdoclen = xdb.get_doclength_upper_bound();
	   , xdb, m_reason);
    if (!m_reason.empty())
        return false;
    if (!listfailed) {
        return true;
    }

    // listfailed is set : look for failed docs
    string ermsg;
    try {
        for (unsigned int docid = 1; docid < xdb.get_lastdocid(); docid++) {
            try {
                Xapian::Document doc = xdb.get_document(docid);
                string sig = doc.get_value(VALUE_SIG);
                if (sig.empty() || sig[sig.size()-1] != '+') {
                    continue;
                }
                string data = doc.get_data();
                ConfSimple parms(data);
                if (!parms.ok()) {
                } else {
                    string url, ipath;
                    parms.get(Doc::keyipt, ipath);
                    parms.get(Doc::keyurl, url);
                    // Turn to local url or not? It seems to make more
                    // sense to keep the original urls as seen by the
                    // indexer.
                    // m_config->urlrewrite(dbdir, url);
                    if (!ipath.empty()) {
                        url += " | " + ipath;
                    }
                    res.failedurls.push_back(url);
                }
            } catch (Xapian::DocNotFoundError) {
                continue;
            }
        }
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR("Db::dbStats: " << ermsg << "\n");
        return false;
    }
    return true;
}

// Retrieve document defined by Unique doc identifier. This is used
// by the GUI history feature and by open parent/getenclosing
// ! The return value is always true except for fatal errors. Document
//  existence should be tested by looking at doc.pc
bool Db::getDoc(const string &udi, const Doc& idxdoc, Doc &doc)
{
    LOGDEB1("Db:getDoc: [" << udi << "]\n");
    int idxi = idxdoc.idxi;
    return getDoc(udi, idxi, doc);
}

bool Db::getDoc(const string &udi, const std::string& dbdir, Doc &doc)
{
    LOGDEB1("Db::getDoc(udi, dbdir): (" << udi << ", " << dbdir << ")\n");
    int idxi = -1;
    if (dbdir.empty() || dbdir == m_basedir) {
        idxi = 0;
    } else {
        for (unsigned int i = 0; i < m_extraDbs.size(); i++) {
            if (dbdir == m_extraDbs[i]) {
                idxi = int(i + 1);
                break;
            }
        }
    }
    LOGDEB1("Db::getDoc(udi, dbdir): idxi: " << idxi << endl);
    if (idxi < 0) {
        LOGERR("Db::getDoc(udi, dbdir): dbdir not in current extra dbs\n");
        return false;
    }
    return getDoc(udi, idxi, doc);
}

bool Db::getDoc(const string& udi, int idxi, Doc& doc)
{
    // Initialize what we can in any case. If this is history, caller
    // will make partial display in case of error
    if (m_ndb == 0)
	return false;
    doc.meta[Rcl::Doc::keyrr] = "100%";
    doc.pc = 100;
    Xapian::Document xdoc;
    Xapian::docid docid;
    if (idxi >= 0 && (docid = m_ndb->getDoc(udi, idxi, xdoc))) {
	string data = xdoc.get_data();
	doc.meta[Rcl::Doc::keyudi] = udi;
	return m_ndb->dbDataToRclDoc(docid, data, doc);
    } else {
	// Document found in history no longer in the
	// database.  We return true (because their might be
	// other ok docs further) but indicate the error with
	// pc = -1
	doc.pc = -1;
	LOGINFO("Db:getDoc: no such doc in current index: [" << udi << "]\n");
	return true;
    }
}

bool Db::hasSubDocs(const Doc &idoc)
{
    if (m_ndb == 0)
	return false;
    string inudi;
    if (!idoc.getmeta(Doc::keyudi, &inudi) || inudi.empty()) {
	LOGERR("Db::hasSubDocs: no input udi or empty\n");
	return false;
    }
    LOGDEB1("Db::hasSubDocs: idxi " << idoc.idxi << " inudi [" <<inudi << "]\n");

    // Not sure why we perform both the subDocs() call and the test on
    // has_children. The former will return docs if the input is a
    // file-level document, but the latter should be true both in this
    // case and if the input is already a subdoc, so the first test
    // should be redundant. Does not hurt much in any case, to be
    // checked one day.
    vector<Xapian::docid> docids;
    if (!m_ndb->subDocs(inudi, idoc.idxi, docids)) {
	LOGDEB("Db::hasSubDocs: lower level subdocs failed\n");
	return false;
    }
    if (!docids.empty())
	return true;

    // Check if doc has an "has_children" term
    if (m_ndb->hasTerm(inudi, idoc.idxi, has_children_term))
	return true;
    return false;
}

// Retrieve all subdocuments of a given one, which may not be a file-level
// one (in which case, we have to retrieve this first, then filter the ipaths)
bool Db::getSubDocs(const Doc &idoc, vector<Doc>& subdocs)
{
    if (m_ndb == 0)
	return false;

    string inudi;
    if (!idoc.getmeta(Doc::keyudi, &inudi) || inudi.empty()) {
	LOGERR("Db::getSubDocs: no input udi or empty\n");
	return false;
    }

    string rootudi;
    string ipath = idoc.ipath;
    LOGDEB0("Db::getSubDocs: idxi " << idoc.idxi << " inudi [" << inudi <<
            "] ipath [" << ipath << "]\n");
    if (ipath.empty()) {
	// File-level doc. Use it as root
	rootudi = inudi;
    } else {
	// See if we have a parent term
	Xapian::Document xdoc;
	if (!m_ndb->getDoc(inudi, idoc.idxi, xdoc)) {
	    LOGERR("Db::getSubDocs: can't get Xapian document\n");
	    return false;
	}
	Xapian::TermIterator xit;
	XAPTRY(xit = xdoc.termlist_begin();
	       xit.skip_to(wrap_prefix(parent_prefix)),
	       m_ndb->xrdb, m_reason);
	if (!m_reason.empty()) {
	    LOGERR("Db::getSubDocs: xapian error: " << m_reason << "\n");
	    return false;
	}
	if (xit == xdoc.termlist_end()) {
	    LOGERR("Db::getSubDocs: parent term not found\n");
	    return false;
	}
	rootudi = strip_prefix(*xit);
    }

    LOGDEB("Db::getSubDocs: root: [" << rootudi << "]\n");

    // Retrieve all subdoc xapian ids for the root
    vector<Xapian::docid> docids;
    if (!m_ndb->subDocs(rootudi, idoc.idxi, docids)) {
	LOGDEB("Db::getSubDocs: lower level subdocs failed\n");
	return false;
    }

    // Retrieve doc, filter, and build output list
    for (int tries = 0; tries < 2; tries++) {
	try {
	    for (vector<Xapian::docid>::const_iterator it = docids.begin();
		 it != docids.end(); it++) {
		Xapian::Document xdoc = m_ndb->xrdb.get_document(*it);
		string data = xdoc.get_data();
		string docudi;
		m_ndb->xdocToUdi(xdoc, docudi);
		Doc doc;
		doc.meta[Doc::keyudi] = docudi;
		doc.meta[Doc::keyrr] = "100%";
		doc.pc = 100;
		if (!m_ndb->dbDataToRclDoc(*it, data, doc)) {
		    LOGERR("Db::getSubDocs: doc conversion error\n");
		    return false;
		}
                if (ipath.empty() ||
                    FileInterner::ipathContains(ipath, doc.ipath)) {
                    subdocs.push_back(doc);
                }
	    }
	    return true;
	} catch (const Xapian::DatabaseModifiedError &e) {
            m_reason = e.get_msg();
	    m_ndb->xrdb.reopen();
            continue;
	} XCATCHERROR(m_reason);
        break;
    }

    LOGERR("Db::getSubDocs: Xapian error: " << m_reason << "\n");
    return false;
}

bool Db::getContainerDoc(const Doc &idoc, Doc& ctdoc)
{
    if (m_ndb == 0)
	return false;

    string inudi;
    if (!idoc.getmeta(Doc::keyudi, &inudi) || inudi.empty()) {
	LOGERR("Db::getContainerDoc: no input udi or empty\n");
	return false;
    }

    string rootudi;
    string ipath = idoc.ipath;
    LOGDEB0("Db::getContainerDoc: idxi " << idoc.idxi << " inudi [" << inudi <<
            "] ipath [" << ipath << "]\n");
    if (ipath.empty()) {
	// File-level doc ??
        ctdoc = idoc;
        return true;
    } 
    // See if we have a parent term
    Xapian::Document xdoc;
    if (!m_ndb->getDoc(inudi, idoc.idxi, xdoc)) {
        LOGERR("Db::getContainerDoc: can't get Xapian document\n");
        return false;
    }
    Xapian::TermIterator xit;
    XAPTRY(xit = xdoc.termlist_begin();
           xit.skip_to(wrap_prefix(parent_prefix)),
           m_ndb->xrdb, m_reason);
    if (!m_reason.empty()) {
        LOGERR("Db::getContainerDoc: xapian error: " << m_reason << "\n");
        return false;
    }
    if (xit == xdoc.termlist_end()) {
        LOGERR("Db::getContainerDoc: parent term not found\n");
        return false;
    }
    rootudi = strip_prefix(*xit);

    if (!getDoc(rootudi, idoc.idxi, ctdoc)) {
        LOGERR("Db::getContainerDoc: can't get container document\n");
        return false;
    }
    return true;
}

// Walk an UDI section (all UDIs beginning with input prefix), and
// mark all docs and subdocs as existing. Caller beware: Makes sense
// or not depending on the UDI structure for the data store. In practise,
// used for absent FS mountable volumes.
bool Db::udiTreeMarkExisting(const string& udi)
{
    LOGDEB("Db::udiTreeMarkExisting: " << udi << endl);
    string wrapd = wrap_prefix(udi_prefix);
    string expr = udi + "*";

#ifdef IDX_THREADS
    std::unique_lock<std::mutex> lock(m_ndb->m_mutex);
#endif

    bool ret = m_ndb->idxTermMatch_p(
        int(ET_WILD), cstr_null, expr,
        [this, &udi](const string& term, Xapian::termcount, Xapian::doccount) {
            Xapian::PostingIterator docid;
            XAPTRY(docid = m_ndb->xrdb.postlist_begin(term), m_ndb->xrdb,
                   m_reason);
            if (!m_reason.empty()) {
                LOGERR("Db::udiTreeWalk: xapian::postlist_begin failed: " <<
                       m_reason << "\n");
                return false;
            }
            if (docid == m_ndb->xrdb.postlist_end(term)) {
                LOGDEB("Db::udiTreeWalk:no doc for " << term << " ??\n");
                return false;
            }
            i_setExistingFlags(udi, *docid);
            LOGDEB0("Db::udiTreeWalk: uniterm: " << term << endl);
            return true;
        }, wrapd);
    return ret;
}

} // End namespace Rcl
