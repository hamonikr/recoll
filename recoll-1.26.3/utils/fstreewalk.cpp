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
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include "safesysstat.h"
#include <cstring>
#include <algorithm>

#include <sstream>
#include <vector>
#include <deque>
#include <set>

#include "cstr.h"
#include "log.h"
#include "pathut.h"
#include "fstreewalk.h"
#include "transcode.h"

using namespace std;

bool FsTreeWalker::o_useFnmPathname = true;
string FsTreeWalker::o_nowalkfn;

const int FsTreeWalker::FtwTravMask = FtwTravNatural|
    FtwTravBreadth|FtwTravFilesThenDirs|FtwTravBreadthThenDepth;

#ifndef _WIN32
// dev/ino means nothing on Windows. It seems that FileId could replace it
// but we only use this for cycle detection which we just disable.
class DirId {
public:
    dev_t dev;
    ino_t ino;
    DirId(dev_t d, ino_t i) : dev(d), ino(i) {}
    bool operator<(const DirId& r) const {
        return dev < r.dev || (dev == r.dev && ino < r.ino);
    }
};
#endif

class FsTreeWalker::Internal {
public:
    Internal(int opts)
        : options(opts), depthswitch(4), maxdepth(-1), errors(0) {
    }
    int options;
    int depthswitch;
    int maxdepth;
    int basedepth;
    stringstream reason;
    vector<string> skippedNames;
    vector<string> onlyNames;
    vector<string> skippedPaths;
    // When doing Breadth or FilesThenDirs traversal, we keep a list
    // of directory paths to be processed, and we do not recurse.
    deque<string> dirs;
    int errors;
#ifndef _WIN32
    set<DirId> donedirs;
#endif
    void logsyserr(const char *call, const string &param) {
        errors++;
        reason << call << "(" << param << ") : " << errno << " : " << 
            strerror(errno) << endl;
    }
};

FsTreeWalker::FsTreeWalker(int opts)
{
    data = new Internal(opts);
}

FsTreeWalker::~FsTreeWalker()
{
    delete data;
}

void FsTreeWalker::setOpts(int opts)
{
    if (data) {
        data->options = opts;
    }
}
int FsTreeWalker::getOpts()
{
    if (data) {
        return data->options;
    } else {
        return 0;
    }
}
void FsTreeWalker::setDepthSwitch(int ds)
{
    if (data) {
        data->depthswitch = ds;
    }
}
void FsTreeWalker::setMaxDepth(int md)
{
    if (data) {
        data->maxdepth = md;
    }
}

string FsTreeWalker::getReason()
{
    string reason = data->reason.str();
    data->reason.str(string());
    data->errors = 0;
    return reason;
}

int FsTreeWalker::getErrCnt()
{
    return data->errors;
}

bool FsTreeWalker::addSkippedName(const string& pattern)
{
    if (find(data->skippedNames.begin(), 
             data->skippedNames.end(), pattern) == data->skippedNames.end())
        data->skippedNames.push_back(pattern);
    return true;
}
bool FsTreeWalker::setSkippedNames(const vector<string> &patterns)
{
    data->skippedNames = patterns;
    return true;
}
bool FsTreeWalker::inSkippedNames(const string& name)
{
    for (const auto& pattern : data->skippedNames) {
        if (fnmatch(pattern.c_str(), name.c_str(), 0) == 0) {
            return true;
        }
    }
    return false;
}
bool FsTreeWalker::setOnlyNames(const vector<string> &patterns)
{
    data->onlyNames = patterns;
    return true;
}
bool FsTreeWalker::inOnlyNames(const string& name)
{
    if (data->onlyNames.empty()) {
        // Not set: all match
        return true;
    }
    for (const auto& pattern : data->onlyNames) {
        if (fnmatch(pattern.c_str(), name.c_str(), 0) == 0) {
            return true;
        }
    }
    return false;
}

bool FsTreeWalker::addSkippedPath(const string& ipath)
{
    string path = (data->options & FtwNoCanon) ? ipath : path_canon(ipath);
    if (find(data->skippedPaths.begin(), 
             data->skippedPaths.end(), path) == data->skippedPaths.end())
        data->skippedPaths.push_back(path);
    return true;
}
bool FsTreeWalker::setSkippedPaths(const vector<string> &paths)
{
    data->skippedPaths = paths;
    for (vector<string>::iterator it = data->skippedPaths.begin();
         it != data->skippedPaths.end(); it++)
        if (!(data->options & FtwNoCanon))
            *it = path_canon(*it);
    return true;
}
bool FsTreeWalker::inSkippedPaths(const string& path, bool ckparents)
{
    int fnmflags = o_useFnmPathname ? FNM_PATHNAME : 0;
#ifdef FNM_LEADING_DIR
    if (ckparents)
        fnmflags |= FNM_LEADING_DIR;
#endif

    for (vector<string>::const_iterator it = data->skippedPaths.begin(); 
         it != data->skippedPaths.end(); it++) {
#ifndef FNM_LEADING_DIR
        if (ckparents) {
            string mpath = path;
            while (mpath.length() > 2) {
                if (fnmatch(it->c_str(), mpath.c_str(), fnmflags) == 0) 
                    return true;
                mpath = path_getfather(mpath);
            }
        } else 
#endif /* FNM_LEADING_DIR */
            if (fnmatch(it->c_str(), path.c_str(), fnmflags) == 0) {
                return true;
            }
    }
    return false;
}

static inline int slashcount(const string& p)
{
    int n = 0;
    for (unsigned int i = 0; i < p.size(); i++)
        if (p[i] == '/')
            n++;
    return n;
}

FsTreeWalker::Status FsTreeWalker::walk(const string& _top, 
                                        FsTreeWalkerCB& cb)
{
    string top = (data->options & FtwNoCanon) ? _top : path_canon(_top);

    if ((data->options & FtwTravMask) == 0) {
        data->options |= FtwTravNatural;
    }

    data->basedepth = slashcount(top); // Only used for breadthxx
    struct stat st;
    // We always follow symlinks at this point. Makes more sense.
    if (path_fileprops(top, &st) == -1) {
        // Note that we do not return an error if the stat call
        // fails. A temp file may have gone away.
        data->logsyserr("stat", top);
        return errno == ENOENT ? FtwOk : FtwError;
    }

    // Recursive version, using the call stack to store state. iwalk
    // will process files and recursively descend into subdirs in
    // physical order of the current directory.
    if ((data->options & FtwTravMask) == FtwTravNatural) {
        return iwalk(top, &st, cb);
    }

    // Breadth first of filesThenDirs semi-depth first order
    // Managing queues of directories to be visited later, in breadth or
    // depth order. Null marker are inserted in the queue to indicate
    // father directory changes (avoids computing parents all the time).
    data->dirs.push_back(top);
    Status status;
    while (!data->dirs.empty()) {
        string dir, nfather;
        if (data->options & (FtwTravBreadth|FtwTravBreadthThenDepth)) {
            // Breadth first, pop and process an older dir at the
            // front of the queue. This will add any child dirs at the
            // back
            dir = data->dirs.front();
            data->dirs.pop_front();
            if (dir.empty()) {
                // Father change marker. 
                if (data->dirs.empty())
                    break;
                dir = data->dirs.front();
                data->dirs.pop_front();
                nfather = path_getfather(dir);
                if (data->options & FtwTravBreadthThenDepth) {
                    // Check if new depth warrants switch to depth first
                    // traversal (will happen on next loop iteration).
                    int curdepth = slashcount(dir) - data->basedepth;
                    if (curdepth >= data->depthswitch) {
                        //fprintf(stderr, "SWITCHING TO DEPTH FIRST\n");
                        data->options &= ~FtwTravMask;
                        data->options |= FtwTravFilesThenDirs;
                    }
                }
            }
        } else {
            // Depth first, pop and process latest dir
            dir = data->dirs.back();
            data->dirs.pop_back();
            if (dir.empty()) {
                // Father change marker. 
                if (data->dirs.empty())
                    break;
                dir = data->dirs.back();
                data->dirs.pop_back();
                nfather = path_getfather(dir);
            }
        }

        // If changing parent directory, advise our user.
        if (!nfather.empty()) {
            if (path_fileprops(nfather, &st) == -1) {
                data->logsyserr("stat", nfather);
                return errno == ENOENT ? FtwOk : FtwError;
            }
            if ((status = cb.processone(nfather, &st, FtwDirReturn)) & 
                (FtwStop|FtwError)) {
                return status;
            }
        }

        if (path_fileprops(dir, &st) == -1) {
            data->logsyserr("stat", dir);
            return errno == ENOENT ? FtwOk : FtwError;
        }
        // iwalk will not recurse in this case, just process file entries
        // and append subdir entries to the queue.
        status = iwalk(dir, &st, cb);
        if (status != FtwOk)
            return status;
    }
    return FtwOk;
}

#ifdef _WIN32
#define DIRENT _wdirent
#define DIRHDL _WDIR
#define OPENDIR _wopendir
#define CLOSEDIR _wclosedir
#define READDIR _wreaddir
#else
#define DIRENT dirent
#define DIRHDL DIR
#define OPENDIR opendir
#define CLOSEDIR closedir
#define READDIR readdir
#endif

// Note that the 'norecurse' flag is handled as part of the directory read. 
// This means that we always go into the top 'walk()' parameter if it is a 
// directory, even if norecurse is set. Bug or Feature ?
FsTreeWalker::Status FsTreeWalker::iwalk(const string &top, 
                                         struct stat *stp,
                                         FsTreeWalkerCB& cb)
{
    Status status = FtwOk;
    bool nullpush = false;

    // Tell user to process the top entry itself
    if (S_ISDIR(stp->st_mode)) {
        if ((status = cb.processone(top, stp, FtwDirEnter)) & 
            (FtwStop|FtwError)) {
            return status;
        }
    } else if (S_ISREG(stp->st_mode)) {
        return cb.processone(top, stp, FtwRegular);
    } else {
        return status;
    }

    int curdepth = slashcount(top) - data->basedepth;
    if (data->maxdepth >= 0 && curdepth >= data->maxdepth) {
        LOGDEB1("FsTreeWalker::iwalk: Maxdepth reached: ["  << (top) << "]\n" );
        return status;
    }

    // This is a directory, read it and process entries:

#ifndef _WIN32
    // Detect if directory already seen. This could just be several
    // symlinks pointing to the same place (if FtwFollow is set), it
    // could also be some other kind of cycle. In any case, there is
    // no point in entering again.
    // For now, we'll ignore the "other kind of cycle" part and only monitor
    // this is FtwFollow is set
    if (data->options & FtwFollow) {
        DirId dirid(stp->st_dev, stp->st_ino);
        if (data->donedirs.find(dirid) != data->donedirs.end()) {
            LOGINFO("Not processing [" << top <<
                    "] (already seen as other path)\n");
            return status;
        }
        data->donedirs.insert(dirid);
    }
#endif

    SYSPATH(top, systop);
    DIRHDL *d = OPENDIR(systop);
    if (nullptr == d) {
        data->logsyserr("opendir", top);
#ifdef _WIN32
        int rc = GetLastError();
        LOGERR("opendir failed: LastError " << rc << endl);
        if (rc == ERROR_NETNAME_DELETED) {
            // 64: share disconnected.
            // Not too sure of the errno in this case.
            // Make sure it's not one of the permissible ones
            errno = ENODEV;
        }
#endif
        switch (errno) {
        case EPERM:
        case EACCES:
        case ENOENT:
#ifdef _WIN32
            // We get this quite a lot, don't know why. To be checked.
        case EINVAL:
#endif
            // No error set: indexing will continue in other directories
            goto out;
        default:
            status = FtwError;
            goto out;
        }
    }

    struct DIRENT *ent;
    while (errno = 0, ((ent = READDIR(d)) != 0)) {
        string fn;
        struct stat st;
#ifdef _WIN32
        string sdname;
        if (!wchartoutf8(ent->d_name, sdname)) {
            LOGERR("wchartoutf8 failed in " << top << endl);
            continue;
        }
        const char *dname = sdname.c_str();
#else
        const char *dname = ent->d_name;
#endif
        // Maybe skip dotfiles
        if ((data->options & FtwSkipDotFiles) && dname[0] == '.')
            continue;
        // Skip . and ..
        if (!strcmp(dname, ".") || !strcmp(dname, "..")) 
            continue;

        // Skipped file names match ?
        if (!data->skippedNames.empty()) {
            if (inSkippedNames(dname))
                continue;
        }
        fn = path_cat(top, dname);
        int statret =  path_fileprops(fn.c_str(), &st, data->options&FtwFollow);
        if (statret == -1) {
            data->logsyserr("stat", fn);
#ifdef _WIN32
            int rc = GetLastError();
            LOGERR("stat failed: LastError " << rc << endl);
            if (rc == ERROR_NETNAME_DELETED) {
                status = FtwError;
                goto out;
            }
#endif
            continue;
        }

        if (!data->skippedPaths.empty()) {
            // We do not check the ancestors. This means that you can have
            // a topdirs member under a skippedPath, to index a portion of
            // an ignored area. This is the way it had always worked, but
            // this was broken by 1.13.00 and the systematic use of 
            // FNM_LEADING_DIR
            if (inSkippedPaths(fn, false))
                continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (!o_nowalkfn.empty() && path_exists(path_cat(fn, o_nowalkfn))) {
                continue;
            }
            if (data->options & FtwNoRecurse) {
                status = cb.processone(fn, &st, FtwDirEnter);
            } else {
                if (data->options & FtwTravNatural) {
                    status = iwalk(fn, &st, cb);
                } else {
                    // If first subdir, push marker to separate
                    // from entries for other dir. This is to help
                    // with generating DirReturn callbacks
                    if (!nullpush) {
                        if (!data->dirs.empty() && 
                            !data->dirs.back().empty())
                            data->dirs.push_back(cstr_null);
                        nullpush = true;
                    }
                    data->dirs.push_back(fn);
                    continue;
                }
            }
            // Note: only recursive case gets here.
            if (status & (FtwStop|FtwError))
                goto out;
            if (!(data->options & FtwNoRecurse)) 
                if ((status = cb.processone(top, &st, FtwDirReturn)) 
                    & (FtwStop|FtwError))
                    goto out;
        } else if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
            // Filtering patterns match ?
            if (!data->onlyNames.empty()) {
                if (!inOnlyNames(dname))
                    continue;
            }
            if ((status = cb.processone(fn, &st, FtwRegular)) & 
                (FtwStop|FtwError)) {
                goto out;
            }
        }
        // We ignore other file types (devices etc...)
    } // readdir loop
    if (errno) {
        // Actual readdir error, not eof.
        data->logsyserr("readdir", top);
#ifdef _WIN32
        int rc = GetLastError();
        LOGERR("Readdir failed: LastError " << rc << endl);
        if (rc == ERROR_NETNAME_DELETED) {
            status = FtwError;
            goto out;
        }
#endif
    }

out:
    if (d)
        CLOSEDIR(d);
    return status;
}


int64_t fsTreeBytes(const string& topdir)
{
    class bytesCB : public FsTreeWalkerCB {
    public:
        FsTreeWalker::Status processone(const string &path, 
                                        const struct stat *st,
                                        FsTreeWalker::CbFlag flg) {
            if (flg == FsTreeWalker::FtwDirEnter ||
                flg == FsTreeWalker::FtwRegular) {
#ifdef _WIN32
                totalbytes += st->st_size;
#else
                totalbytes += st->st_blocks * 512;
#endif
            }
            return FsTreeWalker::FtwOk;
        }
        int64_t totalbytes{0};
    };
    FsTreeWalker walker;
    bytesCB cb;
    FsTreeWalker::Status status = walker.walk(topdir, cb);
    if (status != FsTreeWalker::FtwOk) {
        LOGERR("fsTreeBytes: walker failed: " << walker.getReason() << endl);
        return -1;
    }
    return cb.totalbytes;
}
