/* Copyright (C) 2004-2021 J.F.Dockes
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
#ifndef _FSTREEWALK_H_INCLUDED_
#define _FSTREEWALK_H_INCLUDED_

#include <string>
#include <vector>

struct PathStat;

class FsTreeWalkerCB;

/**
 * Class implementing a Unix directory recursive walk.
 *
 * A user-defined function object is called for every file or
 * directory. Patterns to be ignored can be set before starting the
 * walk. Options control whether we follow symlinks and whether we recurse
 * on subdirectories.
 */
class FsTreeWalker {
public:
    // Global option to use FNM_PATHNAME when matching paths (for
    // skippedPaths).  
    // We initially used FNM_PATHNAME, and we can't change it now
    // (because of all the config files around). So add global option
    // to not use the flag, which can be set from rclconfig by adding
    // a value to the config file (skippedPathsNoFnmPathname)
    static bool o_useFnmPathname;
    static void setNoFnmPathname() {
        o_useFnmPathname = false;
    }

    // Global option to observe a "nowalk" file, which makes us treat
    // directories as if they were in skippedPaths) if the file exists
    // inside the directory.
    static std::string o_nowalkfn;
    static void setNoWalkFn(const std::string& nowalkfn) {
        o_nowalkfn = nowalkfn;
    }

    // Flags for call to processone(). FtwDirEnter is used when
    // entering a directory. FtwDirReturn is used when returning to it
    // after processing a subdirectory.
    enum CbFlag {FtwRegular, FtwDirEnter, FtwDirReturn, FtwSkipped};
    enum Status {FtwOk=0, FtwError=1, FtwStop=2, 
                 FtwStatAll = FtwError|FtwStop};
    enum Options {FtwOptNone = 0, FtwNoRecurse = 1, FtwFollow = 2,
                  FtwNoCanon = 4, FtwSkipDotFiles = 8,
                  // Only callback for skipped files and directories,
                  // for getting a list of skipped stuff. We don't
                  // descend into skipped directories.
                  // ** The callback will receive a null struct stat pointer.**
                  FtwOnlySkipped = 0x10, 
                  // Tree walking options.  Natural is close to depth first: process
                  //   directory entries as we see them, recursing into subdirectories at 
                  //   once 
                  // Breadth means we process all files and dirs at a given directory level
                  // before going deeper.
                  //
                  // FilesThenDirs is close to Natural, except that we process all files in a 
                  //   given directory before going deeper: allows keeping only a single 
                  //   directory open
                  // We don't do pure depth first (process subdirs before files), this does 
                  // not appear to make any sense.
                  FtwTravNatural = 0x10000, FtwTravBreadth = 0x20000, 
                  FtwTravFilesThenDirs = 0x40000, 
                  FtwTravBreadthThenDepth = 0x80000
    };
    static const int FtwTravMask;
    FsTreeWalker(int opts = FtwTravNatural);
    ~FsTreeWalker();

    void setOpts(int opts);
    int getOpts();
    void setDepthSwitch(int);
    void setMaxDepth(int);

    /** 
     * Begin file system walk.
     * @param dir is not checked against the ignored patterns (this is 
     *     a feature and must not change.
     * @param cb the function object that will be called back for every 
     *    file-system object (called both at entry and exit for directories).
     */
    Status walk(const std::string &dir, FsTreeWalkerCB& cb);
    /** Get explanation for error */
    std::string getReason();
    int getErrCnt();

    /**
     * Add a pattern (file or dir) to be ignored (ie: #* , *~)
     */
    bool addSkippedName(const std::string &pattern); 
    /** Set the ignored patterns set */
    bool setSkippedNames(const std::vector<std::string> &patterns);
    /** Set the exclusive patterns set */
    bool setOnlyNames(const std::vector<std::string> &patterns);

    /** Same for skipped paths: this are paths, not names, under which we
        do not descend (ie: /home/me/.recoll) */
    bool addSkippedPath(const std::string &path); 
    /** Set the ignored paths list */
    bool setSkippedPaths(const std::vector<std::string> &patterns);

    /** Test if path/name should be skipped. This can be used independently of
     * an actual tree walk */
    bool inSkippedPaths(const std::string& path, bool ckparents = false);
    bool inSkippedNames(const std::string& name);
    bool inOnlyNames(const std::string& name);

private:
    Status iwalk(const std::string &dir, struct PathStat *stp,
                 FsTreeWalkerCB& cb);
    class Internal; 
    Internal *data;
};

class FsTreeWalkerCB {
public:
    virtual ~FsTreeWalkerCB() {}
    // Only st_mtime, st_ctime, st_size, st_mode (filetype bits: dir/reg/lnk),
    virtual FsTreeWalker::Status 
    processone(const std::string&, const struct PathStat *,
               FsTreeWalker::CbFlag) = 0;
};

// Utility function. Somewhat like du.
int64_t fsTreeBytes(const std::string& topdir);

#endif /* _FSTREEWALK_H_INCLUDED_ */
