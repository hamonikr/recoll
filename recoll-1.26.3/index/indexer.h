/* Copyright (C) 2004 J.F.Dockes
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
#ifndef _INDEXER_H_INCLUDED_
#define _INDEXER_H_INCLUDED_
#include "rclconfig.h"

#include <string>
#include <list>
#include <map>
#include <vector>
#include <mutex>

#include "rcldb.h"
#include "rcldoc.h"
#include "idxstatus.h"

class FsIndexer;
class WebQueueIndexer;

/** Callback to say what we're doing. If the update func returns false, we
 * stop as soon as possible without corrupting state */
class DbIxStatusUpdater {
 public:
#ifdef IDX_THREADS
    std::mutex m_mutex;
#endif
    DbIxStatus status;
    virtual ~DbIxStatusUpdater(){}

    // Convenience: change phase/fn and update
    virtual bool update(DbIxStatus::Phase phase, const string& fn)
    {
#ifdef IDX_THREADS
	std::unique_lock<std::mutex>  lock(m_mutex);
#endif
        status.phase = phase;
        status.fn = fn;
        return update();
    }

    // To be implemented by user for sending info somewhere
    virtual bool update() = 0;
};

/**
 * The top level batch indexing object. Processes the configuration,
 * then invokes file system walking or other to populate/update the 
 * database(s). 
 */
class ConfIndexer {
 public:
    enum runStatus {IndexerOk, IndexerError};
    ConfIndexer(RclConfig *cnf, DbIxStatusUpdater *updfunc = 0);
    virtual ~ConfIndexer();

    // Indexer types. Maybe we'll have something more dynamic one day
    enum ixType {IxTNone, IxTFs=1, IxTWebQueue=2, 
                 IxTAll = IxTFs | IxTWebQueue};
    // Misc indexing flags
    enum IxFlag {IxFNone = 0, 
                 IxFIgnoreSkip = 1, // Ignore skipped lists
                 IxFNoWeb = 2, // Do not process the web queue.
                 // First pass: just do the top files so that the user can 
                 // try searching asap.
                 IxFQuickShallow = 4, 
                 // Do not retry files which previously failed ('+' sigs)
                 IxFNoRetryFailed = 8,
                 // Do perform purge pass even if we can't be sure we saw
                 // all files
                 IxFDoPurge = 16,
    };

    /** Run indexers */
    bool index(bool resetbefore, ixType typestorun, int f = IxFNone);

    const string &getReason() {return m_reason;}

    /** Stemming reset to config: create needed, delete unconfigured */
    bool createStemmingDatabases();

    /** Create stem database for given language */
    bool createStemDb(const string &lang);

    /** Create misspelling expansion dictionary if aspell i/f is available */
    bool createAspellDict();

    /** List possible stemmer names */
    static vector<string> getStemmerNames();

    /** Index a list of files. No db cleaning or stemdb updating */
    bool indexFiles(std::list<std::string> &files, int f = IxFNone);

    /** Update index for list of documents given as list of docs (out of query)
     */
    bool updateDocs(vector<Rcl::Doc> &docs, IxFlag f = IxFNone);

    /** Purge a list of files. */
    bool purgeFiles(std::list<std::string> &files, int f = IxFNone);

    /** Set in place reset mode */
    void setInPlaceReset() {m_db.setInPlaceReset();}
 private:
    RclConfig *m_config;
    Rcl::Db    m_db;
    FsIndexer *m_fsindexer; 
    bool                m_doweb;
    WebQueueIndexer *m_webindexer; 
    DbIxStatusUpdater  *m_updater;
    string              m_reason;

    // The first time we index, we do things a bit differently to
    // avoid user frustration (make at least some results available
    // fast by using several passes, the first ones to index common
    // interesting locations).
    bool runFirstIndexing();
    bool firstFsIndexingSequence();
};

#endif /* _INDEXER_H_INCLUDED_ */
