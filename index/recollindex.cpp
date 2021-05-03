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
#include <signal.h>
#include <errno.h>
#include <fnmatch.h>
#ifndef _WIN32
#include <sys/time.h>
#include <sys/resource.h>
#else
#include <direct.h>
#endif
#include "safefcntl.h"
#include "safeunistd.h"
#include <getopt.h>

#include <iostream>
#include <list>
#include <string>
#include <cstdlib>

using namespace std;

#include "log.h"
#include "rclinit.h"
#include "indexer.h"
#include "smallut.h"
#include "chrono.h"
#include "pathut.h"
#include "rclutil.h"
#include "rclmon.h"
#include "x11mon.h"
#include "cancelcheck.h"
#include "checkindexed.h"
#include "rcldb.h"
#include "readfile.h"
#ifndef DISABLE_WEB_INDEXER
#include "webqueue.h"
#endif
#include "recollindex.h"
#include "fsindexer.h"
#ifndef _WIN32
#include "rclionice.h"
#endif
#include "execmd.h"
#include "checkretryfailed.h"
#include "circache.h"
#include "idxdiags.h"

// Command line options
static int     op_flags;
#define OPT_C 0x1     
#define OPT_c 0x2
#define OPT_d 0x4     
#define OPT_D 0x8     
#define OPT_E 0x10    
#define OPT_e 0x20    
#define OPT_f 0x40    
#define OPT_h 0x80    
#define OPT_i 0x200   
#define OPT_K 0x400   
#define OPT_k 0x800   
#define OPT_l 0x1000  
#define OPT_m 0x2000  
#define OPT_n 0x4000  
#define OPT_P 0x8000  
#define OPT_p 0x10000 
#define OPT_R 0x20000 
#define OPT_r 0x40000 
#define OPT_S 0x80000 
#define OPT_s 0x100000
#define OPT_w 0x200000
#define OPT_x 0x400000
#define OPT_Z 0x800000
#define OPT_z 0x1000000

#define OPTVAL_WEBCACHE_COMPACT 1000
#define OPTVAL_WEBCACHE_BURST 1001
#define OPTVAL_DIAGS_NOTINDEXED 1002
#define OPTVAL_DIAGS_DIAGSFILE 1003

static struct option long_options[] = {
    {"webcache-compact", 0, 0, OPTVAL_WEBCACHE_COMPACT},
    {"webcache-burst", required_argument, 0, OPTVAL_WEBCACHE_BURST},
    {"notindexed", 0, 0, OPTVAL_DIAGS_NOTINDEXED},
    {"diagsfile", required_argument, 0, OPTVAL_DIAGS_DIAGSFILE},
    {0, 0, 0, 0}
};

ReExec *o_reexec;

// Globals for atexit cleanup
static ConfIndexer *confindexer;

// This is set as an atexit routine, 
static void cleanup()
{
    deleteZ(confindexer);
    IdxDiags::theDiags().flush();
    recoll_exitready();
}

// This holds the state of topdirs (exist+nonempty) on indexing
// startup. If it changes after a resume from sleep we interrupt the
// indexing (the assumption being that a volume has been mounted or
// unmounted while we slept). This is not foolproof as the user can
// always pull out a removable volume while we work. It just avoids a
// harmful purge in a common case.
static vector<string> o_topdirs;
static vector<bool> o_topdirs_emptiness;

bool topdirs_state(vector<bool> tdlstate)
{
    tdlstate.clear();
    for (const auto& dir : o_topdirs) {
        tdlstate.push_back(path_empty(dir));
    }
    return true;
}
    
static void sigcleanup(int sig)
{
    if (sig == RCLSIG_RESUME) {
        vector<bool> emptiness;
        topdirs_state(emptiness);
        if (emptiness != o_topdirs_emptiness) {
            string msg = "Recollindex: resume: topdirs state changed while "
                "we were sleeping\n";
            cerr << msg;
            LOGDEB(msg);
            CancelCheck::instance().setCancel();
            stopindexing = 1;
        }
    } else {
        cerr << "Recollindex: got signal " << sig <<
            ", registering stop request\n";
        LOGDEB("Got signal " << sig << ", registering stop request\n");
        CancelCheck::instance().setCancel();
        stopindexing = 1;
    }
}

static void makeIndexerOrExit(RclConfig *config, bool inPlaceReset)
{
    if (!confindexer) {
        confindexer = new ConfIndexer(config);
        if (inPlaceReset)
            confindexer->setInPlaceReset();
    }
    if (!confindexer) {
        cerr << "Cannot create indexer" << endl;
        exit(1);
    }
}

void rclIxIonice(const RclConfig *config)
{
    PRETEND_USE(config);
#ifndef _WIN32
    string clss, classdata;
    if (!config->getConfParam("monioniceclass", clss) || clss.empty())
        clss = "3";
    // Classdata may be empty (must be for idle class)
    config->getConfParam("monioniceclassdata", classdata);
    rclionice(clss, classdata);
#endif
}

static void setMyPriority(const RclConfig *config)
{
    PRETEND_USE(config);
#ifndef _WIN32
    int prio{19};
    std::string sprio;
    config->getConfParam("idxniceprio", sprio);
    if (!sprio.empty()) {
        prio = atoi(sprio.c_str());
    }
    if (setpriority(PRIO_PROCESS, 0, prio) != 0) {
        LOGINFO("recollindex: can't setpriority(), errno " << errno << "\n");
    }
    // Try to ionice. This does not work on all platforms
    rclIxIonice(config);
#endif
}


class MakeListWalkerCB : public FsTreeWalkerCB {
public:
    MakeListWalkerCB(list<string>& files, const vector<string>& selpats)
        : m_files(files), m_pats(selpats) {}
    virtual FsTreeWalker::Status processone(
        const string& fn, const struct PathStat *, FsTreeWalker::CbFlag flg) {
        if (flg== FsTreeWalker::FtwDirEnter || flg == FsTreeWalker::FtwRegular){
            if (m_pats.empty()) {
                m_files.push_back(fn);
            } else {
                for (const auto& pat : m_pats) {
                    if (fnmatch(pat.c_str(), fn.c_str(), 0) == 0) {
                        m_files.push_back(fn);
                        break;
                    }
                }
            }
        }
        return FsTreeWalker::FtwOk;
    }
    list<string>& m_files;
    const vector<string>& m_pats;
};

// Build a list of things to index, then call purgefiles and/or
// indexfiles.  This is basically the same as find xxx | recollindex
// -i [-e] without the find (so, simpler but less powerful)
bool recursive_index(RclConfig *config, const string& top, 
                     const vector<string>& selpats)
{
    list<string> files;
    MakeListWalkerCB cb(files, selpats);
    FsTreeWalker walker;
    walker.walk(top, cb);
    bool ret = false;
    if (op_flags & OPT_e) {
        if (!(ret = purgefiles(config, files))) {
            return ret;
        }
    }
    if (!(op_flags & OPT_e) || ((op_flags & OPT_e) &&(op_flags & OPT_i))) {
        ret = indexfiles(config, files);
    }
    return ret;
}

// Index a list of files. We just call the top indexer method, which
// will sort out what belongs to the indexed trees and call the
// appropriate indexers.
//
// This is called either from the command line or from the monitor. In
// this case we're called repeatedly in the same process, and the
// confindexer is only created once by makeIndexerOrExit (but the db closed and
// flushed every time)
bool indexfiles(RclConfig *config, list<string> &filenames)
{
    if (filenames.empty())
        return true;
    makeIndexerOrExit(config, (op_flags & OPT_Z) != 0);
    // The default is to retry failed files
    int indexerFlags = ConfIndexer::IxFNone; 
    if (op_flags & OPT_K) 
        indexerFlags |= ConfIndexer::IxFNoRetryFailed; 
    if (op_flags & OPT_f)
        indexerFlags |= ConfIndexer::IxFIgnoreSkip;
    if (op_flags & OPT_P) {
        indexerFlags |= ConfIndexer::IxFDoPurge;
    }
    return confindexer->indexFiles(filenames, indexerFlags);
}

// Delete a list of files. Same comments about call contexts as indexfiles.
bool purgefiles(RclConfig *config, list<string> &filenames)
{
    if (filenames.empty())
        return true;
    makeIndexerOrExit(config, (op_flags & OPT_Z) != 0);
    return confindexer->purgeFiles(filenames, ConfIndexer::IxFNone);
}

// Create stemming and spelling databases
bool createAuxDbs(RclConfig *config)
{
    makeIndexerOrExit(config, false);

    if (!confindexer->createStemmingDatabases())
        return false;

    if (!confindexer->createAspellDict())
        return false;

    return true;
}

// Create additional stem database 
static bool createstemdb(RclConfig *config, const string &lang)
{
    makeIndexerOrExit(config, false);
    return confindexer->createStemDb(lang);
}

// Check that topdir entries are valid (successful tilde exp + abs
// path) or fail.
// In addition, topdirs, skippedPaths, daemSkippedPaths entries should
// match existing files or directories. Warn if they don't
static bool checktopdirs(RclConfig *config, vector<string>& nonexist)
{
    if (!config->getConfParam("topdirs", &o_topdirs)) {
        cerr << "No 'topdirs' parameter in configuration\n";
        LOGERR("recollindex:No 'topdirs' parameter in configuration\n");
        return false;
    }

    // If a restricted list for real-time monitoring exists check that
    // all entries are descendants from a topdir
    vector<string> mondirs;
    if (config->getConfParam("monitordirs", &mondirs)) {
        for (const auto& sub : mondirs) {
            bool found{false};
            for (const auto& top : o_topdirs) {
                if (path_isdesc(top, sub)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                string s("Real time monitoring directory entry " + sub +
                         " is not part of the topdirs tree\n");
                cerr << s;
                LOGERR(s);
                return false;
            }
        }
    }

    bool onegood{false};
    for (auto& dir : o_topdirs) {
        dir = path_tildexpand(dir);
        if (!dir.size() || !path_isabsolute(dir)) {
            if (dir[0] == '~') {
                cerr << "Tilde expansion failed: " << dir << endl;
                LOGERR("recollindex: tilde expansion failed: " << dir << "\n");
            } else {
                cerr << "Not an absolute path: " << dir << endl;
                LOGERR("recollindex: not an absolute path: " << dir << "\n");
            }
            return false;
        }
        if (!path_exists(dir)) {
            nonexist.push_back(dir);
        } else {
            onegood = true;
        }
    }
    topdirs_state(o_topdirs_emptiness);

    // We'd like to check skippedPaths too, but these are wildcard
    // exprs, so reasonably can't

    return onegood;
}


string thisprog;

static const char usage [] =
"\n"
"recollindex [-h] \n"
"    Print help\n"
"recollindex [-z|-Z] [-k]\n"
"    Index everything according to configuration file\n"
"    -z : reset database before starting indexing\n"
"    -Z : in place reset: consider all documents as changed. Can also\n"
"         be combined with -i or -r but not -m\n"
"    -k : retry files on which we previously failed\n"
"    --diagsfile <outputpath> : list skipped or otherwise not indexed documents to <outputpath>\n"
"       <outputpath> will be truncated\n"
#ifdef RCL_MONITOR
"recollindex -m [-w <secs>] -x [-D] [-C]\n"
"    Perform real time indexing. Don't become a daemon if -D is set.\n"
"    -w sets number of seconds to wait before starting.\n"
"    -C disables monitoring config for changes/reexecuting.\n"
"    -n disables initial incremental indexing (!and purge!).\n"
#ifndef DISABLE_X11MON
"    -x disables exit on end of x11 session\n"
#endif /* DISABLE_X11MON */
#endif /* RCL_MONITOR */
"recollindex -e [<filepath [path ...]>]\n"
"    Purge data for individual files. No stem database updates.\n"
"    Reads paths on stdin if none is given as argument.\n"
"recollindex -i [-f] [-Z] [<filepath [path ...]>]\n"
"    Index individual files. No database purge or stem database updates\n"
"    Will read paths on stdin if none is given as argument\n"
"    -f : ignore skippedPaths and skippedNames while doing this\n"
"recollindex -r [-K] [-f] [-Z] [-p pattern] <top> \n"
"   Recursive partial reindex. \n"
"     -p : filter file names, multiple instances are allowed, e.g.: \n"
"        -p *.odt -p *.pdf\n"
"     -K : skip previously failed files (they are retried by default)\n"
"recollindex -l\n"
"    List available stemming languages\n"
"recollindex -s <lang>\n"
"    Build stem database for additional language <lang>\n"
"recollindex -E\n"
"    Check configuration file for topdirs and other paths existence\n"
"recollindex --webcache-compact : recover wasted space from the Web cache\n"
"recollindex --webcache-burst <targetdir> : extract entries from the Web cache to the target\n"
"recollindex --notindexed [filepath [filepath ...]] : check if the file arguments are indexed\n"
"   will read file paths from stdin if there are no arguments\n"
#ifdef FUTURE_IMPROVEMENT
"recollindex -W\n"
"    Process the Web queue\n"
#endif
#ifdef RCL_USE_ASPELL
"recollindex -S\n"
"    Build aspell spelling dictionary.>\n"
#endif
"Common options:\n"
"    -c <configdir> : specify config directory, overriding $RECOLL_CONFDIR\n"
#if defined(HAVE_POSIX_FADVISE)
"    -d : call fadvise() with the POSIX_FADV_DONTNEED flag on indexed files\n"
"          (avoids trashing the page cache)\n";
#endif
;

static void Usage()
{
    FILE *fp = (op_flags & OPT_h) ? stdout : stderr;
    fprintf(fp, "%s: Usage: %s", path_getsimple(thisprog).c_str(), usage);
    fprintf(fp, "Recoll version: %s\n", Rcl::version_string().c_str());
    exit((op_flags & OPT_h)==0);
}

static RclConfig *config;

static void lockorexit(Pidfile *pidfile, RclConfig *config)
{
    PRETEND_USE(config);
    pid_t pid;
    if ((pid = pidfile->open()) != 0) {
        if (pid > 0) {
            cerr << "Can't become exclusive indexer: " << pidfile->getreason()
                 << ". Return (other pid?): " << pid << endl;
#ifndef _WIN32
            // Have a look at the status file. If the other process is
            // a monitor we can tell it to start an incremental pass
            // by touching the configuration file
            DbIxStatus status;
            readIdxStatus(config, status);
            if (status.hasmonitor) {
                string cmd("touch ");
                string path = path_cat(config->getConfDir(), "recoll.conf");
                cmd += path;
                int status;
                if ((status = system(cmd.c_str()))) {
                    cerr << cmd << " failed with status " << status << endl;
                } else {
                    cerr << "Monitoring indexer process was notified of "
                        "indexing request\n";
                }
            }
#endif
        } else {
            cerr << "Can't become exclusive indexer: " << pidfile->getreason()
                 << endl;
        }            
        exit(1);
    }
    if (pidfile->write_pid() != 0) {
        cerr << "Can't become exclusive indexer: " << pidfile->getreason() <<
            endl;
        exit(1);
    }
}

static string reasonsfile;
extern ConfSimple idxreasons;
static void flushIdxReasons()
{
    if (reasonsfile.empty())
        return;
    if (reasonsfile == "stdout") {
        idxreasons.write(cout);
    } else if (reasonsfile == "stderr") {
        idxreasons.write(std::cerr);
    } else {
        ofstream out;
        try {
            out.open(reasonsfile, ofstream::out|ofstream::trunc);
            idxreasons.write(out);
        } catch (...) {
            std::cerr << "Could not write reasons file " << reasonsfile << endl;
            idxreasons.write(std::cerr);
        }
    }
}

// With more recent versions of mingw, we could use -municode to
// enable wmain.  Another workaround is to use main, then call
// GetCommandLineW and CommandLineToArgvW, to then call wmain(). If
// ever we need to build with mingw again.
#if defined(_WIN32) && defined(_MSC_VER)
#define USE_WMAIN 1
#endif

#if USE_WMAIN
#define WARGTOSTRING(w) wchartoutf8(w)
static vector<const char*> argstovector(int argc, wchar_t **argv, vector<string>& storage)
#else
#define WARGTOSTRING(w) (w)
    static vector<const char*> argstovector(int argc, char **argv, vector<string>& storage)
#endif
{
    vector<const char *> args(argc+1);
    storage.resize(argc+1);
    thisprog = path_absolute(WARGTOSTRING(argv[0]));
    for (int i = 0; i < argc; i++) {
        storage[i] = WARGTOSTRING(argv[i]);
        args[i] = storage[i].c_str();
    }
    return args;
}


// Working directory before we change: it's simpler to change early
// but some options need the original for computing absolute paths.
static std::string orig_cwd;

// A bit of history: it's difficult to pass non-ASCII parameters
// (e.g. path names) on the command line under Windows without using
// Unicode. It was first thought possible to use a temporary file to
// hold the args, and make sure that the path for this would be ASCII,
// based on using shortpath(). Unfortunately, this does not work in
// all cases, so the second change was to use wmain(). The
// args-in-file was removed quite a long time after.
#if USE_WMAIN
int wmain(int argc, wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
#ifndef _WIN32
    // The reexec struct is used by the daemon to shed memory after
    // the initial indexing pass and to restart when the configuration
    // changes
    o_reexec = new ReExec;
    o_reexec->init(argc, argv);
#endif

    // Only actually useful on Windows: convert wargs to utf-8 chars
    vector<string> astore;
    vector<const char*> args = argstovector(argc, argv, astore);

    vector<string> selpatterns;
    int sleepsecs{60};
    string a_config;
    int ret;
    bool webcache_compact{false};
    bool webcache_burst{false};
    bool diags_notindexed{false};
    
    std::string burstdir;
    std::string diagsfile;
    while ((ret = getopt_long(argc, (char *const*)&args[0], "c:CDdEefhikKlmnPp:rR:sS:w:xZz",
                              long_options, NULL)) != -1) {
        switch (ret) {
        case 'c':  op_flags |= OPT_c; a_config = optarg; break;
#ifdef RCL_MONITOR
        case 'C': op_flags |= OPT_C; break;
        case 'D': op_flags |= OPT_D; break;
#endif
#if defined(HAVE_POSIX_FADVISE)
        case 'd': op_flags |= OPT_d; break;
#endif
        case 'E': op_flags |= OPT_E; break;
        case 'e': op_flags |= OPT_e; break;
        case 'f': op_flags |= OPT_f; break;
        case 'h': op_flags |= OPT_h; break;
        case 'i': op_flags |= OPT_i; break;
        case 'k': op_flags |= OPT_k; break;
        case 'K': op_flags |= OPT_K; break;
        case 'l': op_flags |= OPT_l; break;
        case 'm': op_flags |= OPT_m; break;
        case 'n': op_flags |= OPT_n; break;
        case 'P': op_flags |= OPT_P; break;
        case 'p': op_flags |= OPT_p; selpatterns.push_back(optarg); break;
        case 'r': op_flags |= OPT_r; break;
        case 'R':   op_flags |= OPT_R; reasonsfile = optarg; break;
        case 's': op_flags |= OPT_s; break;
#ifdef RCL_USE_ASPELL
        case 'S': op_flags |= OPT_S; break;
#endif
        case 'w':   op_flags |= OPT_w;
            if ((sscanf(optarg, "%d", &sleepsecs)) != 1) 
                Usage(); 
            break;
        case 'x': op_flags |= OPT_x; break;
        case 'Z': op_flags |= OPT_Z; break;
        case 'z': op_flags |= OPT_z; break;

        case OPTVAL_WEBCACHE_COMPACT: webcache_compact = true; break;
        case OPTVAL_WEBCACHE_BURST: burstdir = optarg; webcache_burst = true;break;
        case OPTVAL_DIAGS_NOTINDEXED: diags_notindexed = true;break;
        case OPTVAL_DIAGS_DIAGSFILE: diagsfile = optarg;break;
        default: Usage(); break;
        }
    }
    int aremain = argc - optind;

    if (op_flags & OPT_h)
        Usage();

#ifndef RCL_MONITOR
    if (op_flags & (OPT_m | OPT_w|OPT_x)) {
        std::cerr << "-m not available: real-time monitoring was not "
            "configured in this build\n";
        exit(1);
    }
#endif

    if ((op_flags & OPT_z) && (op_flags & (OPT_i|OPT_e|OPT_r)))
        Usage();
    if ((op_flags & OPT_Z) && (op_flags & (OPT_m)))
        Usage();
    if ((op_flags & OPT_E) && (op_flags & ~(OPT_E|OPT_c))) {
        Usage();
    }

    string reason;
    int flags = RCLINIT_IDX;
    if ((op_flags & OPT_m) && !(op_flags&OPT_D)) {
        flags |= RCLINIT_DAEMON;
    }
    config = recollinit(flags, cleanup, sigcleanup, reason, &a_config);
    if (config == 0 || !config->ok()) {
        addIdxReason("init", reason);
        flushIdxReasons();
        std::cerr << "Configuration problem: " << reason << endl;
        exit(1);
    }

    // Auxiliary, non-index-related things. Avoids having a separate binary.
    if (webcache_compact || webcache_burst || diags_notindexed) {
        std::string ccdir = config->getWebcacheDir();
        std::string reason;
        if (webcache_compact) {
            if (!CirCache::compact(ccdir, &reason)) {
                std::cerr << "Web cache compact failed: " << reason << "\n";
                exit(1);
            }
        } else if (webcache_burst) {
            if (!CirCache::burst(ccdir, burstdir, &reason)) {
                std::cerr << "Web cache burst failed: " << reason << "\n";
                exit(1);
            }
        } else if (diags_notindexed) {
            std::vector<std::string> filepaths;
            while (aremain--) {
                filepaths.push_back(args[optind++]);
            }
            if (!checkindexed(config, filepaths)) {
                exit(1);
            }
        }
            
        exit(0);
    }

#ifndef _WIN32
    o_reexec->atexit(cleanup);
#endif

    vector<string> nonexist;
    if (!checktopdirs(config, nonexist)) {
        std::cerr << "topdirs not set or only contains invalid paths.\n";
        addIdxReason("init", "topdirs not set or only contains invalid paths.");
        flushIdxReasons();
        exit(1);
    }

    if (nonexist.size()) {
        ostream& out = (op_flags & OPT_E) ? cout : cerr;
        if (!(op_flags & OPT_E)) {
            cerr << "Warning: invalid paths in topdirs, skippedPaths or "
                "daemSkippedPaths:\n";
        }
        for (const auto& entry : nonexist) {
            out << entry << endl;
        }
    }
    if ((op_flags & OPT_E)) {
        exit(0);
    }

    if (op_flags & OPT_l) {
        if (aremain != 0) 
            Usage();
        vector<string> stemmers = ConfIndexer::getStemmerNames();
        for (const auto& stemmer : stemmers) {
            cout << stemmer << endl;
        }
        exit(0);
    }
    
    orig_cwd = path_cwd();
    string rundir;
    config->getConfParam("idxrundir", rundir);
    if (!rundir.empty()) {
        if (!rundir.compare("tmp")) {
            rundir = tmplocation();
        }
        LOGINFO("recollindex: changing current directory to [" <<rundir<<"]\n");
        if (!path_chdir(rundir)) {
            LOGSYSERR("main", "chdir", rundir);
        }
    }

    if (!diagsfile.empty()) {
        if (!IdxDiags::theDiags().init(diagsfile)) {
            std::cerr << "Could not initialize diags file " << diagsfile << "\n";
            LOGERR("recollindex: Could not initialize diags file " << diagsfile << "\n");
        }
    }
    bool rezero((op_flags & OPT_z) != 0);
    bool inPlaceReset((op_flags & OPT_Z) != 0);

    // The default is not to retry previously failed files by default.
    // If -k is set, we do.
    // If the checker script says so, we do too, except if -K is set.
    int indexerFlags = ConfIndexer::IxFNoRetryFailed;
    if (op_flags & OPT_k) {
        indexerFlags &= ~ConfIndexer::IxFNoRetryFailed; 
    } else {
        if (op_flags & OPT_K) {
            indexerFlags |= ConfIndexer::IxFNoRetryFailed;
        } else {
            if (checkRetryFailed(config, false)) {
                indexerFlags &= ~ConfIndexer::IxFNoRetryFailed; 
            } else {
                indexerFlags |= ConfIndexer::IxFNoRetryFailed;
            }
        }
    }
    if (indexerFlags & ConfIndexer::IxFNoRetryFailed) {
        LOGDEB("recollindex: files in error will not be retried\n");
    } else {
        LOGDEB("recollindex: files in error will be retried\n");
    }

#if defined(HAVE_POSIX_FADVISE)
    if (op_flags & OPT_d) {
        indexerFlags |= ConfIndexer::IxFCleanCache;
    }
#endif
    
    Pidfile pidfile(config->getPidfile());
    lockorexit(&pidfile, config);

    // Log something at LOGINFO to reset the trace file. Else at level
    // 3 it's not even truncated if all docs are up to date.
    LOGINFO("recollindex: starting up\n");
    setMyPriority(config);

    // Init status updater
    if (nullptr == statusUpdater(config, op_flags & OPT_x)) {
        std::cerr << "Could not initialize status updater\n";
        LOGERR("Could not initialize status updater\n");
        exit(1);
    }
    statusUpdater()->update(DbIxStatus::DBIXS_NONE, "");
    
    if (op_flags & OPT_r) {
        if (aremain != 1) 
            Usage();
        string top = args[optind++]; aremain--;
        top = path_canon(top, &orig_cwd);
        bool status = recursive_index(config, top, selpatterns);
        if (confindexer && !confindexer->getReason().empty()) {
            addIdxReason("indexer", confindexer->getReason());
            cerr << confindexer->getReason() << endl;
        }
        flushIdxReasons();
        exit(status ? 0 : 1);
    } else if (op_flags & (OPT_i|OPT_e)) {
        list<string> filenames;
        if (aremain == 0) {
            // Read from stdin
            char line[1024];
            while (fgets(line, 1023, stdin)) {
                string sl(line);
                trimstring(sl, "\n\r");
                filenames.push_back(sl);
            }
        } else {
            while (aremain--) {
                filenames.push_back(args[optind++]);
            }
        }

        // Note that -e and -i may be both set. In this case we first erase,
        // then index. This is a slightly different from -Z -i because we 
        // warranty that all subdocs are purged.
        bool status = true;
        if (op_flags & OPT_e) {
            status = purgefiles(config, filenames);
        }
        if (status && (op_flags & OPT_i)) {
            status = indexfiles(config, filenames);
        }
        if (confindexer && !confindexer->getReason().empty()) {
            addIdxReason("indexer", confindexer->getReason());
            cerr << confindexer->getReason() << endl;
        }
        flushIdxReasons();
        exit(status ? 0 : 1);
    } else if (op_flags & OPT_s) {
        if (aremain != 1) 
            Usage();
        string lang = args[optind++]; aremain--;
        exit(!createstemdb(config, lang));

#ifdef RCL_USE_ASPELL
    } else if (op_flags & OPT_S) {
        makeIndexerOrExit(config, false);
        exit(!confindexer->createAspellDict());
#endif // ASPELL

#ifdef RCL_MONITOR
    } else if (op_flags & OPT_m) {
        if (aremain != 0) 
            Usage();
        statusUpdater()->setMonitor(true);
        if (!(op_flags&OPT_D)) {
            LOGDEB("recollindex: daemonizing\n");
#ifndef _WIN32
            if (daemon(0,0) != 0) {
                addIdxReason("monitor", "daemon() failed");
                cerr << "daemon() failed, errno " << errno << endl;
                LOGERR("daemon() failed, errno " << errno << "\n");
                flushIdxReasons();
                exit(1);
            }
#endif
        }
        // Need to rewrite pid, it changed
        pidfile.write_pid();
        // Not too sure if I have to redo the nice thing after daemon(),
        // can't hurt anyway (easier than testing on all platforms...)
        setMyPriority(config);

        if (sleepsecs > 0) {
            LOGDEB("recollindex: sleeping " << sleepsecs << "\n");
            for (int i = 0; i < sleepsecs; i++) {
                sleep(1);
                // Check that x11 did not go away while we were sleeping.
                if (!(op_flags & OPT_x) && !x11IsAlive()) {
                    LOGDEB("X11 session went away during initial sleep period\n");
                    exit(0);
                }
            }
        }

        if (!(op_flags & OPT_n)) {
            makeIndexerOrExit(config, inPlaceReset);
            LOGDEB("Recollindex: initial indexing pass before monitoring\n");
            if (!confindexer->index(rezero, ConfIndexer::IxTAll, indexerFlags)
                || stopindexing) {
                LOGERR("recollindex, initial indexing pass failed, "
                       "not going into monitor mode\n");
                flushIdxReasons();
                exit(1);
            } else {
                // Record success of indexing pass with failed files retries.
                if (!(indexerFlags & ConfIndexer::IxFNoRetryFailed)) {
                    checkRetryFailed(config, true);
                }
            }
            deleteZ(confindexer);
#ifndef _WIN32
            o_reexec->insertArgs(vector<string>(1, "-n"));
            LOGINFO("recollindex: reexecuting with -n after initial full "
                    "pass\n");
            // Note that -n will be inside the reexec when we come
            // back, but the monitor will explicitly strip it before
            // starting a config change exec to ensure that we do a
            // purging pass in this latter case (full restart).
            o_reexec->reexec();
#endif
        }

        statusUpdater()->update(DbIxStatus::DBIXS_MONITOR, "");

        int opts = RCLMON_NONE;
        if (op_flags & OPT_D)
            opts |= RCLMON_NOFORK;
        if (op_flags & OPT_C)
            opts |= RCLMON_NOCONFCHECK;
        if (op_flags & OPT_x)
            opts |= RCLMON_NOX11;
        bool monret = startMonitor(config, opts);
        MONDEB(("Monitor returned %d, exiting\n", monret));
        exit(monret == false);
#endif // MONITOR

    }

    makeIndexerOrExit(config, inPlaceReset);
    bool status = confindexer->index(rezero, ConfIndexer::IxTAll, indexerFlags);
    // Record success of indexing pass with failed files retries.
    if (status && !(indexerFlags & ConfIndexer::IxFNoRetryFailed)) {
        checkRetryFailed(config, true);
    }
    if (!status) 
        cerr << "Indexing failed" << endl;
    if (!confindexer->getReason().empty()) {
        addIdxReason("indexer", confindexer->getReason());
        cerr << confindexer->getReason() << endl;
    }
    statusUpdater()->update(DbIxStatus::DBIXS_DONE, "");
    flushIdxReasons();
    return !status;
}
