/* Copyright (C) 2007 J.F.Dockes
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

#ifndef _rcldb_p_h_included_
#define _rcldb_p_h_included_

#include "autoconfig.h"

#include <mutex>
#include <functional>

#include <xapian.h>

#ifdef IDX_THREADS
#include "workqueue.h"
#endif // IDX_THREADS
#include "xmacros.h"
#include "log.h"

namespace Rcl {

class Query;

#ifdef IDX_THREADS
// Task for the index update thread. This can be 
//  - add/update for a new / update documment
//  - delete for a deleted document
//  - purgeOrphans when a multidoc file is updated during a partial pass (no 
//    general purge). We want to remove subDocs that possibly don't
//    exist anymore. We find them by their different sig
// txtlen and doc are only valid for add/update else, len is (size_t)-1 and doc
// is empty
class DbUpdTask {
public:
    enum Op {AddOrUpdate, Delete, PurgeOrphans};
    // Note that udi and uniterm are strictly equivalent and are
    // passed both just to avoid recomputing uniterm which is
    // available on the caller site.
    // Take some care to avoid sharing string data (if string impl is cow)
    DbUpdTask(Op _op, const string& ud, const string& un, 
	      Xapian::Document *d, size_t tl, string& rztxt)
        : op(_op), udi(ud.begin(), ud.end()), uniterm(un.begin(), un.end()), 
          doc(d), txtlen(tl) {
        rawztext.swap(rztxt);
    }
    // Udi and uniterm equivalently designate the doc
    Op op;
    string udi;
    string uniterm;
    Xapian::Document *doc;
    // txtlen is used to update the flush interval. It's -1 for a
    // purge because we actually don't know it, and the code fakes a
    // text length based on the term count.
    size_t txtlen;
    string rawztext; // Compressed doc text
};
#endif // IDX_THREADS

class TextSplitDb;

// A class for data and methods that would have to expose
// Xapian-specific stuff if they were in Rcl::Db. There could actually be
// 2 different ones for indexing or query as there is not much in
// common.
class Db::Native {
 public:
    Db  *m_rcldb; // Parent
    bool m_isopen;
    bool m_iswritable;
    bool m_noversionwrite; //Set if open failed because of version mismatch!
    bool m_storetext{false};
#ifdef IDX_THREADS
    WorkQueue<DbUpdTask*> m_wqueue;
    std::mutex m_mutex;
    long long  m_totalworkns;
    bool m_havewriteq;
    void maybeStartThreads();
#endif // IDX_THREADS

    // Indexing 
    Xapian::WritableDatabase xwdb;
    // Querying (active even if the wdb is too)
    Xapian::Database xrdb;

    Native(Db *db);
    ~Native();

#ifdef IDX_THREADS
    friend void *DbUpdWorker(void*);
#endif // IDX_THREADS

    void openWrite(const std::string& dir, Db::OpenMode mode);
    void openRead(const string& dir);

    // Determine if an existing index is of the full-text-storing kind
    // by looking at the index metadata. Stores the result in m_storetext
    void storesDocText(Xapian::Database&);
    
    // Final steps of doc update, part which need to be single-threaded
    bool addOrUpdateWrite(const string& udi, const string& uniterm, 
			  Xapian::Document *doc, size_t txtlen
                          , const string& rawztext);

    /** Delete all documents which are contained in the input document, 
     * which must be a file-level one.
     * 
     * @param onlyOrphans if true, only delete documents which have
     * not the same signature as the input. This is used to delete docs
     * which do not exist any more in the file after an update, for
     * example the tail messages after a folder truncation). If false,
     * delete all.
     * @param udi the parent document identifier.
     * @param uniterm equivalent to udi, passed just to avoid recomputing.
     */
    bool purgeFileWrite(bool onlyOrphans, const string& udi, 
			const string& uniterm);

    bool getPagePositions(Xapian::docid docid, vector<int>& vpos);
    int getPageNumberForPosition(const vector<int>& pbreaks, int pos);

    bool dbDataToRclDoc(Xapian::docid docid, std::string &data, Doc &doc,
                        bool fetchtext = false);

    size_t whatDbIdx(Xapian::docid id);
    Xapian::docid whatDbDocid(Xapian::docid);

    /** Retrieve Xapian::docid, given unique document identifier, 
     * using the posting list for the derived term.
     * 
     * @param udi the unique document identifier (opaque hashed path+ipath).
     * @param idxi the database index, at query time, when using external
     *     databases.
     * @param[out] xdoc the xapian document.
     * @return 0 if not found
     */
    Xapian::docid getDoc(const string& udi, int idxi, Xapian::Document& xdoc);

    /** Retrieve unique document identifier for given Xapian document, 
     * using the document termlist 
     */
    bool xdocToUdi(Xapian::Document& xdoc, string &udi);

    /** Check if doc is indexed by term */
    bool hasTerm(const string& udi, int idxi, const string& term);

    /** Update existing Xapian document for pure extended attrs change */
    bool docToXdocXattrOnly(TextSplitDb *splitter, const string &udi, 
			    Doc &doc, Xapian::Document& xdoc);
    /** Remove all terms currently indexed for field defined by idx prefix */
    bool clearField(Xapian::Document& xdoc, const string& pfx, 
		    Xapian::termcount wdfdec);

    /** Check if term wdf is 0 and remove term if so */
    bool clearDocTermIfWdf0(Xapian::Document& xdoc, const string& term);

    /** Compute list of subdocuments for a given udi. We look for documents 
     * indexed by a parent term matching the udi, the posting list for the 
     * parentterm(udi)  (As suggested by James Aylett)
     *
     * Note that this is not currently recursive: all subdocs are supposed 
     * to be children of the file doc.
     * Ie: in a mail folder, all messages, attachments, attachments of
     * attached messages etc. must have the folder file document as
     * parent. 
     *
     * Finer grain parent-child relationships are defined by the
     * indexer (rcldb user), using the ipath.
     * 
     */
    bool subDocs(const string &udi, int idxi, vector<Xapian::docid>& docids);

    /** Matcher */
    bool idxTermMatch_p(int typ_sens,const string &lang,const std::string &term,
                        std::function<bool(const std::string& term,
                                           Xapian::termcount colfreq,
                                           Xapian::doccount termfreq)> client,
                        const string& field);

    /** Check if a page position list is defined */
    bool hasPages(Xapian::docid id);

    std::string rawtextMetaKey(Xapian::docid did) {
        // Xapian's Olly Betts avises to use a key which will
        // sort the same as the docid (which we do), and to
        // use Xapian's pack.h:pack_uint_preserving_sort() which is
        // efficient but hard to read. I'd wager that this
        // does not make much of a difference. 10 ascii bytes
        // gives us 10 billion docs, which is enough (says I).
        char buf[30];
        sprintf(buf, "%010d", did);
        return buf;
    }

    bool getRawText(Xapian::docid docid, string& rawtext);

    void deleteDocument(Xapian::docid docid) {
        string metareason;
        XAPTRY(xwdb.set_metadata(rawtextMetaKey(docid), string()),
               xwdb, metareason);
        if (!metareason.empty()) {
            LOGERR("deleteDocument: set_metadata error: " <<
                   metareason << "\n");
            // not fatal
        }
        xwdb.delete_document(docid);
    }
};

// This is the word position offset at which we index the body text
// (abstract, keywords, etc.. are stored before this)
static const unsigned int baseTextPosition = 100000;

}
#endif /* _rcldb_p_h_included_ */
