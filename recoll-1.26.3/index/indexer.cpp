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
#include "autoconfig.h"

#include <stdio.h>
#include <errno.h>

#include <algorithm>

#include "cstr.h"
#include "log.h"
#include "recollindex.h"
#include "indexer.h"
#include "fsindexer.h"
#ifndef DISABLE_WEB_INDEXER
#include "webqueue.h"
#endif
#include "mimehandler.h"
#include "pathut.h"
#include "idxstatus.h"
#include "execmd.h"
#include "safesysstat.h"

#ifdef RCL_USE_ASPELL
#include "rclaspell.h"
#endif

using std::list;
using std::string;
using std::vector;

// Global stop request flag. This is checked in a number of place in the
// indexing routines.
int stopindexing;

// This would more logically live in recollindex.cpp, but then librecoll would
// have an undefined symbol
ConfSimple idxreasons;
void addIdxReason(string who, string reason)
{
    reason = neutchars(reason, "\r\n");
    if (!idxreasons.set(who, reason)) {
        std::cerr << "addIdxReason: confsimple set failed\n";
    }
}

#ifndef DISABLE_WEB_INDEXER
bool runWebFilesMoverScript(RclConfig *config)
{
    static string downloadsdir;
    if (downloadsdir.empty()) {
        if (!config->getConfParam("webdownloadsdir", downloadsdir)) {
            downloadsdir = path_tildexpand("~/Downloads");
        }
    }
    static string cmdpath;
    vector<string> args;
#ifdef _WIN32
    const static string cmdnm{"python"};
    args.push_back(config->findFilter("recoll-we-move-files.py"));
#else
    const static string cmdnm{"recoll-we-move-files.py"};
#endif
    if (cmdpath.empty()) {
        cmdpath = config->findFilter(cmdnm);
        if (cmdpath.empty()) {
            LOGERR("runWFMoverScript: recoll-we-move-files.py not found\n");
            return false;
        }
    }

    /* Arrange to not actually run the script if the directory did not change */
    static time_t dirmtime;
    time_t ndirmtime = 0;
    struct stat st;
    if (path_fileprops(downloadsdir.c_str(), &st) == 0) {
        ndirmtime = st.st_mtime;
    }
    /* If stat fails, presumably Downloads does not exist or is not
       accessible, dirmtime and mdirmtime stay at 0, and we never
       execute the script, which is the right thing. */
    if (dirmtime != ndirmtime) {
        /* The script is going to change the directory, so updating
           dirmtime before it runs means that we are going to execute
           it one time too many (it will run without doing anything),
           but we can't set the mtime to after the run in case files
           are created during the run. */
        dirmtime = ndirmtime;
        ExecCmd cmd;
        int status = cmd.doexec(cmdpath, args);
        return status == 0;
    }
    return true;
}
#endif

ConfIndexer::ConfIndexer(RclConfig *cnf, DbIxStatusUpdater *updfunc)
    : m_config(cnf), m_db(cnf), m_fsindexer(0), 
      m_doweb(false), m_webindexer(0),
      m_updater(updfunc)
{
    m_config->getConfParam("processwebqueue", &m_doweb);
}

ConfIndexer::~ConfIndexer()
{
     deleteZ(m_fsindexer);
#ifndef DISABLE_WEB_INDEXER
     deleteZ(m_webindexer);
#endif
}

// Determine if this is likely the first time that the user runs
// indexing.  We don't look at the xapiandb as this may have been
// explicitly removed for valid reasons, but at the indexing status
// file, which should be unexistant-or-empty only before any indexing
// has ever run
bool ConfIndexer::runFirstIndexing()
{
    // Indexing status file existing and not empty ?
    if (path_filesize(m_config->getIdxStatusFile()) > 0) {
	LOGDEB0("ConfIndexer::runFirstIndexing: no: status file not empty\n");
	return false;
    }
    // And only do this if the user has kept the default topdirs (~). 
    vector<string> tdl = m_config->getTopdirs();
    if (tdl.size() != 1 || tdl[0].compare(path_canon(path_tildexpand("~")))) {
	LOGDEB0("ConfIndexer::runFirstIndexing: no: not home only\n");
	return false;
    }
    return true;
}

bool ConfIndexer::firstFsIndexingSequence()
{
    LOGDEB("ConfIndexer::firstFsIndexingSequence\n");
    deleteZ(m_fsindexer);
    m_fsindexer = new FsIndexer(m_config, &m_db, m_updater);
    if (!m_fsindexer) {
	return false;
    }
    int flushmb = m_db.getFlushMb();
    m_db.setFlushMb(2);
    m_fsindexer->index(IxFQuickShallow);
    m_db.doFlush();
    m_db.setFlushMb(flushmb);
    return true;
}

bool ConfIndexer::index(bool resetbefore, ixType typestorun, int flags)
{
    Rcl::Db::OpenMode mode = resetbefore ? Rcl::Db::DbTrunc : Rcl::Db::DbUpd;
    if (!m_db.open(mode)) {
	LOGERR("ConfIndexer: error opening database " << m_config->getDbDir() <<
               " : " << m_db.getReason() << "\n");
        addIdxReason("indexer", m_db.getReason());
	return false;
    }

    m_config->setKeyDir(cstr_null);
    if (typestorun & IxTFs) {
	if (runFirstIndexing()) {
	    firstFsIndexingSequence();
	}
        deleteZ(m_fsindexer);
        m_fsindexer = new FsIndexer(m_config, &m_db, m_updater);
        if (!m_fsindexer || !m_fsindexer->index(flags)) {
            if (stopindexing) {
                addIdxReason("indexer", "Indexing was interrupted.");
            } else {
                addIdxReason("indexer", "Index creation failed. See log.");
            }
	    m_db.close();
            return false;
        }
    }
#ifndef DISABLE_WEB_INDEXER
    if (m_doweb && (typestorun & IxTWebQueue)) {
        runWebFilesMoverScript(m_config);
        deleteZ(m_webindexer);
        m_webindexer = new WebQueueIndexer(m_config, &m_db, m_updater);
        if (!m_webindexer || !m_webindexer->index()) {
	    m_db.close();
            addIdxReason("indexer", "Web index creation failed. See log");
            return false;
        }
    }
#endif
    if (typestorun == IxTAll) {
        // Get rid of all database entries that don't exist in the
        // filesystem anymore. Only if all *configured* indexers ran.
        if (m_updater && !m_updater->update(DbIxStatus::DBIXS_PURGE, "")) {
	    m_db.close();
            addIdxReason("indexer", "Index purge failed. See log");
	    return false;
	}
        m_db.purge();
    }

    // The close would be done in our destructor, but we want status
    // here. Makes no sense to check for cancel, we'll have to close
    // anyway
    if (m_updater)
	m_updater->update(DbIxStatus::DBIXS_CLOSING, string());
    if (!m_db.close()) {
	LOGERR("ConfIndexer::index: error closing database in " <<
               m_config->getDbDir() << "\n");
        addIdxReason("indexer", "Index close/flush failed. See log");
	return false;
    }

    if (m_updater && !m_updater->update(DbIxStatus::DBIXS_CLOSING, string()))
	return false;
    bool ret = true;
    if (!createStemmingDatabases()) {
	ret = false;
    }
    if (m_updater && !m_updater->update(DbIxStatus::DBIXS_CLOSING, string()))
	return false;

    // Don't fail indexing because of an aspell issue: we ignore the status.
    // Messages were written to the reasons output
    (void)createAspellDict();
    
    clearMimeHandlerCache();
    if (m_updater)
	m_updater->update(DbIxStatus::DBIXS_DONE, string());
    return ret;
}

bool ConfIndexer::indexFiles(list<string>& ifiles, int flag)
{
    list<string> myfiles;
    string origcwd = m_config->getOrigCwd();
    for (const auto& entry : ifiles) {
	myfiles.push_back(path_canon(entry, &origcwd));
    }
    myfiles.sort();

    if (!m_db.open(Rcl::Db::DbUpd)) {
	LOGERR("ConfIndexer: indexFiles error opening database " <<
               m_config->getDbDir() << "\n");
	return false;
    }
    m_config->setKeyDir(cstr_null);
    bool ret = false;
    if (!m_fsindexer)
        m_fsindexer = new FsIndexer(m_config, &m_db, m_updater);
    if (m_fsindexer)
        ret = m_fsindexer->indexFiles(myfiles, flag);
    LOGDEB2("ConfIndexer::indexFiles: fsindexer returned " << ret << ", " <<
            myfiles.size() << " files remainining\n");
#ifndef DISABLE_WEB_INDEXER

    if (m_doweb && !myfiles.empty() && !(flag & IxFNoWeb)) {
        if (!m_webindexer)
            m_webindexer = new WebQueueIndexer(m_config, &m_db, m_updater);
        if (m_webindexer) {
            ret = ret && m_webindexer->indexFiles(myfiles);
        } else {
            ret = false;
        }
    }
#endif
    if (flag & IxFDoPurge) {
        m_db.purge();
    }
    // The close would be done in our destructor, but we want status here
    if (!m_db.close()) {
	LOGERR("ConfIndexer::index: error closing database in " <<
               m_config->getDbDir() << "\n");
	return false;
    }
    ifiles = myfiles;
    clearMimeHandlerCache();
    return ret;
}

// Update index for specific documents. The docs come from an index
// query, so the udi, backend etc. fields are filled.
bool ConfIndexer::updateDocs(vector<Rcl::Doc> &docs, IxFlag flag)
{
    vector<string> paths;
    docsToPaths(docs, paths);
    list<string> files(paths.begin(), paths.end());
    if (!files.empty()) {
	return indexFiles(files, flag);
    }
    return true;
}

bool ConfIndexer::purgeFiles(list<string> &files, int flag)
{
    list<string> myfiles;
    string origcwd = m_config->getOrigCwd();
    for (const auto& entry : files) {
	myfiles.push_back(path_canon(entry, &origcwd));
    }
    myfiles.sort();

    if (!m_db.open(Rcl::Db::DbUpd)) {
	LOGERR("ConfIndexer: purgeFiles error opening database " <<
               m_config->getDbDir() << "\n");
	return false;
    }
    bool ret = false;
    m_config->setKeyDir(cstr_null);
    if (!m_fsindexer)
        m_fsindexer = new FsIndexer(m_config, &m_db, m_updater);
    if (m_fsindexer)
        ret = m_fsindexer->purgeFiles(myfiles);

#ifndef DISABLE_WEB_INDEXER
    if (m_doweb && !myfiles.empty() && !(flag & IxFNoWeb)) {
        if (!m_webindexer)
            m_webindexer = new WebQueueIndexer(m_config, &m_db, m_updater);
        if (m_webindexer) {
            ret = ret && m_webindexer->purgeFiles(myfiles);
        } else {
            ret = false;
        }
    }
#endif

    // The close would be done in our destructor, but we want status here
    if (!m_db.close()) {
	LOGERR("ConfIndexer::purgefiles: error closing database in " <<
               m_config->getDbDir() << "\n");
	return false;
    }
    return ret;
}

// Create stemming databases. We also remove those which are not
// configured. 
bool ConfIndexer::createStemmingDatabases()
{
    string slangs;
    bool ret = true;
    if (m_config->getConfParam("indexstemminglanguages", slangs)) {
        if (!m_db.open(Rcl::Db::DbUpd)) {
            LOGERR("ConfIndexer::createStemmingDb: could not open db\n");
            addIdxReason("stemming", "could not open db");
            return false;
        }
	vector<string> langs;
	stringToStrings(slangs, langs);

	// Get the list of existing stem dbs from the database (some may have 
	// been manually created, we just keep those from the config
	vector<string> dblangs = m_db.getStemLangs();
	vector<string>::const_iterator it;
	for (it = dblangs.begin(); it != dblangs.end(); it++) {
	    if (find(langs.begin(), langs.end(), *it) == langs.end())
		m_db.deleteStemDb(*it);
	}
	ret = ret && m_db.createStemDbs(langs);
        if (!ret) {
            addIdxReason("stemming", "stem db creation failed");
        }
    }
    m_db.close();
    return ret;
}

bool ConfIndexer::createStemDb(const string &lang)
{
    if (!m_db.open(Rcl::Db::DbUpd))
	return false;
    vector<string> langs;
    stringToStrings(lang, langs);
    return m_db.createStemDbs(langs);
}

// The language for the aspell dictionary is handled internally by the aspell
// module, either from a configuration variable or the NLS environment.
bool ConfIndexer::createAspellDict()
{
    LOGDEB2("ConfIndexer::createAspellDict()\n");
#ifdef RCL_USE_ASPELL
    // For the benefit of the real-time indexer, we only initialize
    // noaspell from the configuration once. It can then be set to
    // true if dictionary generation fails, which avoids retrying
    // it forever.
    static int noaspell = -12345;
    if (noaspell == -12345) {
	noaspell = false;
	m_config->getConfParam("noaspell", &noaspell);
    }
    if (noaspell)
	return true;

    if (!m_db.open(Rcl::Db::DbRO)) {
        LOGERR("ConfIndexer::createAspellDict: could not open db\n");
	return false;
    }

    Aspell aspell(m_config);
    string reason;
    if (!aspell.init(reason)) {
	LOGERR("ConfIndexer::createAspellDict: aspell init failed: " <<
               reason << "\n");
	noaspell = true;
	return false;
    }
    LOGDEB("ConfIndexer::createAspellDict: creating dictionary\n");
    if (!aspell.buildDict(m_db, reason)) {
	LOGERR("ConfIndexer::createAspellDict: aspell buildDict failed: " <<
               reason << "\n");
        addIdxReason("aspell", reason);
	noaspell = true;
	return false;
    }
#endif
    return true;
}

vector<string> ConfIndexer::getStemmerNames()
{
    return Rcl::Db::getStemmerNames();
}

