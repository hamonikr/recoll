/* Copyright (C) 2004 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef _PATHUT_H_INCLUDED_
#define _PATHUT_H_INCLUDED_

#include <string>
#include <vector>
#include <set>
#include <cstdint>
#include <fstream>

// Must be called in main thread before starting other threads
extern void pathut_init_mt();

/// Add a / at the end if none there yet.
extern void path_catslash(std::string& s);
/// Concatenate 2 paths
extern std::string path_cat(const std::string& s1, const std::string& s2);
/// Get the simple file name (get rid of any directory path prefix
extern std::string path_getsimple(const std::string& s);
/// Simple file name + optional suffix stripping
extern std::string path_basename(const std::string& s,
                                 const std::string& suff = std::string());
/// Component after last '.'
extern std::string path_suffix(const std::string& s);
/// Get the father directory
extern std::string path_getfather(const std::string& s);
/// Test if path is absolute
extern bool path_isabsolute(const std::string& s);
/// Test if path is root (x:/). root is defined by root/.. == root
extern bool path_isroot(const std::string& p);
/// Test if sub is a subdirectory of top. This is a textual test,
/// links not allowed
extern bool path_isdesc(const std::string& top, const std::string& sub);

/// Clean up path by removing duplicated / and resolving ../ + make it absolute
extern std::string path_canon(const std::string& s, const std::string *cwd = 0);

/// Check that path refers to same file. Uses dev/ino on Linux,
/// textual comparison on Windows.
bool path_samefile(const std::string& p1, const std::string& p2);

/// Get the current user's home directory
extern std::string path_home();
/// Expand ~ at the beginning of std::string
extern std::string path_tildexpand(const std::string& s);
/// Use getcwd() to make absolute path if needed. Beware: ***this can fail***
/// we return an empty path in this case.
extern std::string path_absolute(const std::string& s);

/// Stat parameter and check if it's a directory
extern bool path_isdir(const std::string& path, bool follow = false);
/// Stat parameter and check if it's a regular file
extern bool path_isfile(const std::string& path, bool follow = false);

/// Retrieve file size
extern long long path_filesize(const std::string& path);

/// Check that path is traversable and last element exists
/// Returns true if last elt could be checked to exist. False may mean that
/// the file/dir does not exist or that an error occurred.
bool path_exists(const std::string& path);
/// Same but must be readable
bool path_readable(const std::string& path);

#ifdef _WIN32
#  ifndef R_OK
#    define R_OK 4
#  endif
#  ifndef W_OK
#    define W_OK 2
#  endif
#  ifndef X_OK
// Not useful/supported on Windows. Define as R_OK
#    define X_OK R_OK
#  endif
#  ifndef F_OK
#    define F_OK 0
#  endif
#endif /* _WIN32 */
/// access() or _waccess()
bool path_access(const std::string& path, int mode);

/// Retrieve essential file attributes. This is used rather than a
/// bare stat() to ensure consistent use of the time fields (on
/// windows, we set ctime=mtime as ctime is actually the creation
/// time, for which we have no use).
/// Only st_mtime, st_ctime, st_size, st_mode (file type bits) are set on
/// all systems. st_dev and st_ino are set for special posix usage.
/// The rest is zeroed.
/// @ret 0 for success
struct PathStat {
    enum PstType {PST_REGULAR, PST_SYMLINK, PST_DIR, PST_OTHER};
    PstType pst_type;
    int64_t pst_size;
    uint64_t pst_mode;
    int64_t pst_mtime;
    int64_t pst_ctime;
    uint64_t pst_ino;
    uint64_t pst_dev;
    uint64_t pst_blocks;
    uint64_t pst_blksize;
};
extern int path_fileprops(const std::string path, struct PathStat *stp,
                          bool follow = true);


/// Return separator for PATH environment variable
extern std::string path_PATHsep();

#ifdef _WIN32
#include <memory>
bool wchartoutf8(const wchar_t *in, std::string& out, size_t len = 0);
std::string wchartoutf8(const wchar_t *in, size_t len = 0);
bool utf8towchar(const std::string& in, wchar_t *out, size_t obytescap);
std::unique_ptr<wchar_t[]> utf8towchar(const std::string& in);
#endif

/// Directory reading interface. UTF-8 on Windows.
class PathDirContents {
public:
    PathDirContents(const std::string& dirpath);
    ~PathDirContents();

    bool opendir();
    struct Entry {
        std::string d_name;
    };
    const struct Entry* readdir();
    void rewinddir();

private:
    class Internal;
    Internal *m{nullptr};
};

/// Dump directory
extern bool listdir(const std::string& dir, std::string& reason,
                    std::set<std::string>& entries);

/** A small wrapper around statfs et al, to return percentage of disk
    occupation
    @param[output] pc percent occupied
    @param[output] avmbs Mbs available to non-superuser. Mb=1024*1024
*/
bool fsocc(const std::string& path, int *pc, long long *avmbs = 0);

/// mkdir -p
extern bool path_makepath(const std::string& path, int mode);

///
bool path_chdir(const std::string& path);
std::string path_cwd();
bool path_unlink(const std::string& path);
bool path_rmdir(const std::string& path);
                 
/* Open file, trying to do the right thing with non-ASCII paths on
 * Windows, where it only works with MSVC at the moment if the path is
 * not ASCII, because it uses fstream(wchar_t*), which is an MSVC
 * extension. On other OSes, just builds the fstream.  We'd need to
 * find a way to make this work with g++. It would be easier in this
 * case to use a FILE (_openw(), then fdopen()), but conftree really
 * depends on std::iostream. 
 *
 * @param path an utf-8 file path.
 * @param mode is an std::fstream mode (ios::in etc.) */
extern bool path_streamopen(
    const std::string& path, int mode, std::fstream& outstream);

/// Encode according to rfc 1738
extern std::string url_encode(const std::string& url,
                              std::string::size_type offs = 0);
extern std::string url_decode(const std::string& encoded);
//// Convert to file path if url is like file://. This modifies the
//// input (and returns a copy for convenience)
extern std::string fileurltolocalpath(std::string url);
/// Test for file:/// url
extern bool urlisfileurl(const std::string& url);
///
extern std::string url_parentfolder(const std::string& url);
/// Return the host+path part of an url. This is not a general
/// routine, it does the right thing only in the recoll context
extern std::string url_gpath(const std::string& url);
/// Turn absolute path into file:// url
extern std::string path_pathtofileurl(const std::string& path);

/// URI parser, loosely from rfc2396.txt
class ParsedUri {
public:
    ParsedUri(std::string uri);
    bool parsed{false};
    std::string scheme;
    std::string user;
    std::string pass;
    std::string host;
    std::string port;
    std::string path;
    std::string query;
    std::vector<std::pair<std::string,std::string>> parsedquery;
    std::string fragment;
};

#ifdef _WIN32
/// Convert \ separators to /
void path_slashize(std::string& s);
void path_backslashize(std::string& s);
extern std::string path_shortpath(const std::string& path);
#else
#define path_shortpath(path) (path)
#endif

/// Lock/pid file class. This is quite close to the pidfile_xxx
/// utilities in FreeBSD with a bit more encapsulation. I'd have used
/// the freebsd code if it was available elsewhere
class Pidfile {
public:
    Pidfile(const std::string& path)    : m_path(path), m_fd(-1) {}
    ~Pidfile();
    /// Open/create the pid file.
    /// @return 0 if ok, > 0 for pid of existing process, -1 for other error.
    int open();
    /// Write pid into the pid file
    /// @return 0 ok, -1 error
    int write_pid();
    /// Close the pid file (unlocks)
    int close();
    /// Delete the pid file
    int remove();
    const std::string& getreason() {
        return m_reason;
    }
private:
    std::string m_path;
    int    m_fd;
    std::string m_reason;
    int read_pid();
    int flopen();
};

#endif /* _PATHUT_H_INCLUDED_ */
