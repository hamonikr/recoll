/* Copyright (C) 2004-2019 J.F.Dockes
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
 *
 * flock emulation:
 *   Emulate flock on platforms that lack it, primarily Windows and MinGW.
 *
 *   This is derived from sqlite3 sources.
 *   https://www.sqlite.org/src/finfo?name=src/os_win.c
 *   https://www.sqlite.org/copyright.html
 *
 *   Written by Richard W.M. Jones <rjones.at.redhat.com>
 *
 *   Copyright (C) 2008-2019 Free Software Foundation, Inc.
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef BUILDING_RECOLL
#include "autoconfig.h"
#else
#include "config.h"
#endif

#include "pathut.h"

#include "smallut.h"
#ifdef MDU_INCLUDE_LOG
#include MDU_INCLUDE_LOG
#else
#include "log.h"
#endif

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <math.h>
#include <regex>
#include <set>
#include <sstream>
#include <stack>
#include <stdio.h>
#include <vector>
#include <fcntl.h>

// Listing directories: we include the normal dirent.h on Unix-derived
// systems, and on MinGW, where it comes with a supplemental wide char
// interface. When building with MSVC, we use our bundled msvc_dirent.h,
// which is equivalent to the one in MinGW
#ifdef _MSC_VER
#include "msvc_dirent.h"
#else // !_MSC_VER
#include <dirent.h>
#endif // _MSC_VER


#ifdef _WIN32

#ifndef _MSC_VER
#undef WINVER
#define WINVER 0x0601
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#define LOGFONTW void
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <io.h>
#include <sys/stat.h>
#include <direct.h>
#include <Shlobj.h>
#include <Stringapiset.h>

#if !defined(S_IFLNK)
#define S_IFLNK 0
#endif
#ifndef S_ISDIR
# define S_ISDIR(ST_MODE) (((ST_MODE) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
# define S_ISREG(ST_MODE) (((ST_MODE) & _S_IFMT) == _S_IFREG)
#endif
#define MAXPATHLEN PATH_MAX
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#ifndef R_OK
#define R_OK 4
#endif

#define STAT _wstati64
#define LSTAT _wstati64
#define STATBUF _stati64
#define ACCESS _waccess
#define OPENDIR ::_wopendir
#define DIRHDL _WDIR
#define CLOSEDIR _wclosedir
#define READDIR ::_wreaddir
#define REWINDDIR ::_wrewinddir
#define DIRENT _wdirent
#define DIRHDL _WDIR
#define MKDIR(a,b) _wmkdir(a)
#define OPEN ::_wopen
#define UNLINK _wunlink
#define RMDIR _wrmdir
#define CHDIR _wchdir

#define SYSPATH(PATH, SPATH) wchar_t PATH ## _buf[2048];      \
    utf8towchar(PATH, PATH ## _buf, 2048);                    \
    wchar_t *SPATH = PATH ## _buf;

#define ftruncate _chsize_s

#ifdef _MSC_VER
// For getpid
#include <process.h>
#define getpid _getpid

#define PATHUT_SSIZE_T int
#endif // _MSC_VER

#else /* !_WIN32 -> */

#include <unistd.h>
#include <sys/param.h>
#include <pwd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>

#define STAT stat
#define LSTAT lstat
#define STATBUF stat
#define ACCESS access
#define OPENDIR ::opendir
#define DIRHDL DIR
#define CLOSEDIR closedir
#define READDIR ::readdir
#define REWINDDIR ::rewinddir
#define DIRENT dirent
#define DIRHDL DIR
#define MKDIR(a,b) mkdir(a,b)
#define O_BINARY 0
#define OPEN ::open
#define UNLINK ::unlink
#define RMDIR ::rmdir
#define CHDIR ::chdir

#define SYSPATH(PATH, SPATH) const char *SPATH = PATH.c_str()


#endif /* !_WIN32 */

using namespace std;

#ifndef PATHUT_SSIZE_T
#define PATHUT_SSIZE_T ssize_t
#endif


#ifdef _WIN32

std::string wchartoutf8(const wchar_t *in, size_t len)
{
    std::string out;
    wchartoutf8(in, out, len);
    return out;
}

bool wchartoutf8(const wchar_t *in, std::string& out, size_t wlen)
{
    LOGDEB1("WCHARTOUTF8: in [" << in << "]\n");
    out.clear();
    if (nullptr == in) {
        return true;
    }
    if (wlen == 0) {
        wlen = wcslen(in);
    }
    int flags = WC_ERR_INVALID_CHARS;
    int bytes = ::WideCharToMultiByte(
        CP_UTF8, flags, in, wlen, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        LOGERR("wchartoutf8: conversion error1\n");
        fwprintf(stderr, L"wchartoutf8: conversion error1 for [%s]\n", in);
        return false;
    }
    char *cp = (char *)malloc(bytes+1);
    if (nullptr == cp) {
        LOGERR("wchartoutf8: malloc failed\n");
        return false;
    }
    bytes = ::WideCharToMultiByte(
        CP_UTF8, flags, in, wlen, cp, bytes, nullptr, nullptr);
    if (bytes <= 0) {
        LOGERR("wchartoutf8: CONVERSION ERROR2\n");
        free(cp);
        return false;
    }
    cp[bytes] = 0;
    out = cp;
    free(cp);
    //fwprintf(stderr, L"wchartoutf8: in: [%s]\n", in);
    //fprintf(stderr, "wchartoutf8: out:  [%s]\n", out.c_str());
    return true;
}

bool utf8towchar(const std::string& in, wchar_t *out, size_t obytescap)
{
    size_t wcharsavail = obytescap / sizeof(wchar_t);
    if (nullptr == out || wcharsavail < 1) {
        return false;
    }
    out[0] = 0;

    int wcharcnt = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, in.c_str(), in.size(), nullptr, 0);
    if (wcharcnt <= 0) {
        LOGERR("utf8towchar: conversion error for [" << in << "]\n");
        return false;
    }
    if (wcharcnt + 1 >  int(wcharsavail)) {
        LOGERR("utf8towchar: not enough space\n");
        return false;
    }
    wcharcnt = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, in.c_str(), in.size(), out, wcharsavail);
    if (wcharcnt <= 0) {
        LOGERR("utf8towchar: conversion error for [" << in << "]\n");
        return false;
    }
    out[wcharcnt] = 0;
    return true;
}

std::unique_ptr<wchar_t[]> utf8towchar(const std::string& in)
{
    // Note that as we supply in.size(), mbtowch computes the size
    // without a terminating 0 (and won't write in the second call of
    // course). We take this into account by allocating one more and
    // terminating the output.
    int wcharcnt = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, in.c_str(), in.size(), nullptr, 0);
    if (wcharcnt <= 0) {
        LOGERR("utf8towchar: conversion error for [" << in << "]\n");
        return std::unique_ptr<wchar_t[]>();
    }
    auto buf = unique_ptr<wchar_t[]>(new wchar_t[wcharcnt+1]);

    wcharcnt = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, in.c_str(), in.size(),
        buf.get(), wcharcnt);
    if (wcharcnt <= 0) {
        LOGERR("utf8towchar: conversion error for [" << in << "]\n");
        return std::unique_ptr<wchar_t[]>();
    }
    buf.get()[wcharcnt] = 0;
    return buf;
}

/// Convert \ separators to /
void path_slashize(string& s)
{
    for (string::size_type i = 0; i < s.size(); i++) {
        if (s[i] == '\\') {
            s[i] = '/';
        }
    }
}
void path_backslashize(string& s)
{
    for (string::size_type i = 0; i < s.size(); i++) {
        if (s[i] == '/') {
            s[i] = '\\';
        }
    }
}
static bool path_strlookslikedrive(const string& s)
{
    return s.size() == 2 && isalpha(s[0]) && s[1] == ':';
}

static bool path_hasdrive(const string& s)
{
    if (s.size() >= 2 && isalpha(s[0]) && s[1] == ':') {
        return true;
    }
    return false;
}
static bool path_isdriveabs(const string& s)
{
    if (s.size() >= 3 && isalpha(s[0]) && s[1] == ':' && s[2] == '/') {
        return true;
    }
    return false;
}

/* Operations for the 'flock' call (same as Linux kernel constants).  */
# define LOCK_SH 1       /* Shared lock.  */
# define LOCK_EX 2       /* Exclusive lock.  */
# define LOCK_UN 8       /* Unlock.  */

/* Can be OR'd in to one of the above.  */
# define LOCK_NB 4       /* Don't block when locking.  */

#include <io.h>

/* Determine the current size of a file.  Because the other braindead
 * APIs we'll call need lower/upper 32 bit pairs, keep the file size
 * like that too.
 */
static BOOL
file_size (HANDLE h, DWORD * lower, DWORD * upper)
{
    *lower = GetFileSize (h, upper);
    /* It appears that we can't lock an empty file, a lock is always
       over a data section. But we seem to be able to set a lock
       beyond the current file size, which is enough to get Pidfile
       working */
    if (*lower == 0 && *upper == 0) {
        *lower = 100;
    }
    return 1;
}

/* LOCKFILE_FAIL_IMMEDIATELY is undefined on some Windows systems. */
# ifndef LOCKFILE_FAIL_IMMEDIATELY
#  define LOCKFILE_FAIL_IMMEDIATELY 1
# endif

/* Acquire a lock. */
static BOOL
do_lock (HANDLE h, int non_blocking, int exclusive)
{
    BOOL res;
    DWORD size_lower, size_upper;
    OVERLAPPED ovlp;
    int flags = 0;

    /* We're going to lock the whole file, so get the file size. */
    res = file_size (h, &size_lower, &size_upper);
    if (!res)
        return 0;

    /* Start offset is 0, and also zero the remaining members of this struct. */
    memset (&ovlp, 0, sizeof ovlp);

    if (non_blocking)
        flags |= LOCKFILE_FAIL_IMMEDIATELY;
    if (exclusive)
        flags |= LOCKFILE_EXCLUSIVE_LOCK;

    return LockFileEx (h, flags, 0, size_lower, size_upper, &ovlp);
}

/* Unlock reader or exclusive lock. */
static BOOL
do_unlock (HANDLE h)
{
    int res;
    DWORD size_lower, size_upper;

    res = file_size (h, &size_lower, &size_upper);
    if (!res)
        return 0;

    return UnlockFile (h, 0, 0, size_lower, size_upper);
}

/* Now our BSD-like flock operation. */
int
flock (int fd, int operation)
{
    HANDLE h = (HANDLE) _get_osfhandle (fd);
    DWORD res;
    int non_blocking;

    if (h == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }

    non_blocking = operation & LOCK_NB;
    operation &= ~LOCK_NB;

    switch (operation) {
    case LOCK_SH:
        res = do_lock (h, non_blocking, 0);
        break;
    case LOCK_EX:
        res = do_lock (h, non_blocking, 1);
        break;
    case LOCK_UN:
        res = do_unlock (h);
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    /* Map Windows errors into Unix errnos.  As usual MSDN fails to
     * document the permissible error codes.
     */
    if (!res) {
        DWORD err = GetLastError ();
        switch (err){
            /* This means someone else is holding a lock. */
        case ERROR_LOCK_VIOLATION:
            errno = EAGAIN;
            break;

            /* Out of memory. */
        case ERROR_NOT_ENOUGH_MEMORY:
            errno = ENOMEM;
            break;

        case ERROR_BAD_COMMAND:
            errno = EINVAL;
            break;

            /* Unlikely to be other errors, but at least don't lose the
             * error code.
             */
        default:
            errno = err;
        }

        return -1;
    }

    return 0;
}

std::string path_shortpath(const std::string& path)
{
    SYSPATH(path, syspath);
    wchar_t wspath[MAX_PATH];
    int ret = GetShortPathNameW(syspath, wspath, MAX_PATH);
    if (ret == 0) {
        LOGERR("GetShortPathNameW failed for [" << path << "]\n");
        return path;
    } else if (ret >= MAX_PATH) {
        LOGERR("GetShortPathNameW [" << path << "] too long " <<
               path.size() << " MAX_PATH " << MAX_PATH << "\n");
        return path;
    }
    string shortpath;
    wchartoutf8(wspath, shortpath);
    return shortpath;
}

#endif /* _WIN32 */

bool fsocc(const string& path, int *pc, long long *avmbs)
{
    static const int FSOCC_MB = 1024 * 1024;
#ifdef _WIN32
    ULARGE_INTEGER freebytesavail;
    ULARGE_INTEGER totalbytes;
    SYSPATH(path, syspath);
    if (!GetDiskFreeSpaceExW(syspath, &freebytesavail, &totalbytes, NULL)) {
        return false;
    }
    if (pc) {
        *pc = int((100 * freebytesavail.QuadPart) / totalbytes.QuadPart);
    }
    if (avmbs) {
        *avmbs = int(totalbytes.QuadPart / FSOCC_MB);
    }
    return true;
#else /* !_WIN32 */

    struct statvfs buf;
    if (statvfs(path.c_str(), &buf) != 0) {
        return false;
    }

    if (pc) {
        double fsocc_used = double(buf.f_blocks - buf.f_bfree);
        double fsocc_totavail = fsocc_used + double(buf.f_bavail);
        double fpc = 100.0;
        if (fsocc_totavail > 0) {
            fpc = 100.0 * fsocc_used / fsocc_totavail;
        }
        *pc = int(fpc);
    }
    if (avmbs) {
        *avmbs = 0;
        if (buf.f_bsize > 0) {
            int ratio = buf.f_frsize > FSOCC_MB ? buf.f_frsize / FSOCC_MB :
                FSOCC_MB / buf.f_frsize;

            *avmbs = buf.f_frsize > FSOCC_MB ?
                ((long long)buf.f_bavail) * ratio :
                ((long long)buf.f_bavail) / ratio;
        }
    }
    return true;
#endif /* !_WIN32 */
}


string path_PATHsep()
{
    static const string w(";");
    static const string u(":");
#ifdef _WIN32
    return w;
#else
    return u;
#endif
}

void path_catslash(string& s)
{
#ifdef _WIN32
    path_slashize(s);
#endif
    if (s.empty() || s[s.length() - 1] != '/') {
        s += '/';
    }
}

string path_cat(const string& s1, const string& s2)
{
    string res = s1;
    path_catslash(res);
    res +=  s2;
    return res;
}

string path_getfather(const string& s)
{
    string father = s;
#ifdef _WIN32
    path_slashize(father);
#endif

    // ??
    if (father.empty()) {
        return "./";
    }

    if (path_isroot(father)) {
        return father;
    }

    if (father[father.length() - 1] == '/') {
        // Input ends with /. Strip it, root special case was tested above
        father.erase(father.length() - 1);
    }

    string::size_type slp = father.rfind('/');
    if (slp == string::npos) {
        return "./";
    }

    father.erase(slp);
    path_catslash(father);
    return father;
}

string path_getsimple(const string& s)
{
    string simple = s;
#ifdef _WIN32
    path_slashize(simple);
#endif

    if (simple.empty()) {
        return simple;
    }

    string::size_type slp = simple.rfind('/');
    if (slp == string::npos) {
        return simple;
    }

    simple.erase(0, slp + 1);
    return simple;
}

string path_basename(const string& s, const string& suff)
{
    string simple = path_getsimple(s);
    string::size_type pos = string::npos;
    if (suff.length() && simple.length() > suff.length()) {
        pos = simple.rfind(suff);
        if (pos != string::npos && pos + suff.length() == simple.length()) {
            return simple.substr(0, pos);
        }
    }
    return simple;
}

string path_suffix(const string& s)
{
    string::size_type dotp = s.rfind('.');
    if (dotp == string::npos) {
        return string();
    }
    return s.substr(dotp + 1);
}

string path_home()
{
#ifdef _WIN32
    string dir;
    // Using wgetenv does not work well, depending on the
    // environment I get wrong values for the accented chars (works
    // with recollindex started from msys command window, does not
    // work when started from recoll. SHGet... fixes this
    //const wchar_t *cp = _wgetenv(L"USERPROFILE");
    wchar_t *cp;
    SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &cp);
    if (cp != 0) {
        wchartoutf8(cp, dir);
    }
    if (dir.empty()) {
        cp = _wgetenv(L"HOMEDRIVE");
        wchartoutf8(cp, dir);
        if (cp != 0) {
            string dir1;
            const wchar_t *cp1 = _wgetenv(L"HOMEPATH");
            wchartoutf8(cp1, dir1);
            if (cp1 != 0) {
                dir = path_cat(dir, dir1);
            }
        }
    }
    if (dir.empty()) {
        dir = "C:/";
    }
    dir = path_canon(dir);
    path_catslash(dir);
    return dir;
#else
    uid_t uid = getuid();

    struct passwd *entry = getpwuid(uid);
    if (entry == 0) {
        const char *cp = getenv("HOME");
        if (cp) {
            return cp;
        } else {
            return "/";
        }
    }

    string homedir = entry->pw_dir;
    path_catslash(homedir);
    return homedir;
#endif
}

string path_tildexpand(const string& s)
{
    if (s.empty() || s[0] != '~') {
        return s;
    }
    string o = s;
#ifdef _WIN32
    path_slashize(o);
#endif

    if (s.length() == 1) {
        o.replace(0, 1, path_home());
    } else if (s[1] == '/') {
        o.replace(0, 2, path_home());
    } else {
        string::size_type pos = s.find('/');
        string::size_type l = (pos == string::npos) ? s.length() - 1 : pos - 1;
#ifdef _WIN32
        // Dont know what this means. Just replace with HOME
        o.replace(0, l + 1, path_home());
#else
        struct passwd *entry = getpwnam(s.substr(1, l).c_str());
        if (entry) {
            o.replace(0, l + 1, entry->pw_dir);
        }
#endif
    }
    return o;
}

bool path_isroot(const string& path)
{
    if (path.size() == 1 && path[0] == '/') {
        return true;
    }
#ifdef _WIN32
    if (path.size() == 3 && isalpha(path[0]) && path[1] == ':' &&
        (path[2] == '/' || path[2] == '\\')) {
        return true;
    }
#endif
    return false;
}

bool path_isdesc(const string& _top, const string& _sub)
{
    string top = path_canon(_top);
    string sub = path_canon(_sub);
    path_catslash(top);
    path_catslash(sub);
    for (;;) {
        if (sub == top) {
            return true;
        }
        string::size_type l = sub.size();
        sub = path_getfather(sub);
        if (sub.size() == l || sub.size() < top.size()) {
            // At root or sub shorter than top: done
            if (sub == top) {
                return true;
            } else {
                return false;
            }
        }
    }
}

bool path_isabsolute(const string& path)
{
    if (!path.empty() && (path[0] == '/'
#ifdef _WIN32
                          || path_isdriveabs(path)
#endif
            )) {
        return true;
    }
    return false;
}

string path_absolute(const string& is)
{
    if (is.length() == 0) {
        return is;
    }
    string s = is;
#ifdef _WIN32
    path_slashize(s);
#endif
    if (!path_isabsolute(s)) {
        s = path_cat(path_cwd(), s);
#ifdef _WIN32
        path_slashize(s);
#endif
    }
    return s;
}

string path_canon(const string& is, const string* cwd)
{
    if (is.length() == 0) {
        return is;
    }
    string s = is;
#ifdef _WIN32
    path_slashize(s);
    // fix possible path from file: absolute url
    if (s.size() && s[0] == '/' && path_hasdrive(s.substr(1))) {
        s = s.substr(1);
    }
#endif

    if (!path_isabsolute(s)) {
        if (cwd) {
            s = path_cat(*cwd, s);
        } else {
            s = path_cat(path_cwd(), s);
        }
    }
    vector<string> elems;
    stringToTokens(s, elems, "/");
    vector<string> cleaned;
    for (vector<string>::const_iterator it = elems.begin();
         it != elems.end(); it++) {
        if (*it == "..") {
            if (!cleaned.empty()) {
                cleaned.pop_back();
            }
        } else if (it->empty() || *it == ".") {
        } else {
            cleaned.push_back(*it);
        }
    }
    string ret;
    if (!cleaned.empty()) {
        for (vector<string>::const_iterator it = cleaned.begin();
             it != cleaned.end(); it++) {
            ret += "/";
#ifdef _WIN32
            if (it == cleaned.begin() && path_strlookslikedrive(*it)) {
                // Get rid of just added initial "/"
                ret.clear();
            }
#endif
            ret += *it;
        }
    } else {
        ret = "/";
    }

#ifdef _WIN32
    // Raw drive needs a final /
    if (path_strlookslikedrive(ret)) {
        path_catslash(ret);
    }
#endif

    return ret;
}

bool path_makepath(const string& ipath, int mode)
{
    string path = path_canon(ipath);
    vector<string> elems;
    stringToTokens(path, elems, "/");
    path = "/";
    for (const auto& elem : elems) {
#ifdef _WIN32
        PRETEND_USE(mode);
        if (path == "/" && path_strlookslikedrive(elem)) {
            path = "";
        }
#endif
        path += elem;
        // Not using path_isdir() here, because this cant grok symlinks
        // If we hit an existing file, no worry, mkdir will just fail.
        LOGDEB1("path_makepath: testing existence: ["  << path << "]\n");
        if (!path_exists(path)) {
            LOGDEB1("path_makepath: creating directory ["  << path << "]\n");
            SYSPATH(path, syspath);
            if (MKDIR(syspath, mode) != 0)  {
                //cerr << "mkdir " << path << " failed, errno " << errno << endl;
                return false;
            }
        }
        path += "/";
    }
    return true;
}

bool path_chdir(const std::string& path)
{
    SYSPATH(path, syspath);
    return CHDIR(syspath) == 0;
}

std::string path_cwd()
{
#ifdef _WIN32
    wchar_t *wd = _wgetcwd(nullptr, 0);
    if (nullptr == wd) {
        return std::string();
    }
    string sdname;
    wchartoutf8(wd, sdname);
    free(wd);
    path_slashize(sdname);
    return sdname;
#else
    char wd[MAXPATHLEN+1];
    if (nullptr == getcwd(wd, MAXPATHLEN+1)) {
        return string();
    }
    return wd;
#endif
}

bool path_unlink(const std::string& path)
{
    SYSPATH(path, syspath);
    return UNLINK(syspath) == 0;
}

bool path_rmdir(const std::string& path)
{
    SYSPATH(path, syspath);
    return RMDIR(syspath) == 0;
}

bool path_streamopen(const std::string& path, int mode, std::fstream& outstream)
{
#if defined(_WIN32) && defined (_MSC_VER)
    // MSC STL has support for using wide chars in fstream
    // constructor. We need this if, e.g. the user name/home directory
    // is not ASCII. Actually don't know how to do this with gcc
    wchar_t wpath[MAX_PATH + 1];
    utf8towchar(path, wpath, MAX_PATH);
    outstream.open(wpath, std::ios_base::openmode(mode));
#else
    outstream.open(path, std::ios_base::openmode(mode));
#endif
    if (!outstream.is_open()) {
        return false;
    }
    return true;
}

bool path_isdir(const string& path, bool follow)
{
    struct STATBUF st;
    SYSPATH(path, syspath);
    int ret = follow ? STAT(syspath, &st) : LSTAT(syspath, &st);
    if (ret < 0) {
        return false;
    }
    if (S_ISDIR(st.st_mode)) {
        return true;
    }
    return false;
}

bool path_isfile(const string& path, bool follow)
{
    struct STATBUF st;
    SYSPATH(path, syspath);
    int ret = follow ? STAT(syspath, &st) : LSTAT(syspath, &st);
    if (ret < 0) {
        return false;
    }
    if (S_ISREG(st.st_mode)) {
        return true;
    }
    return false;
}

long long path_filesize(const string& path)
{
    struct STATBUF st;
    SYSPATH(path, syspath);
    if (STAT(syspath, &st) < 0) {
        return -1;
    }
    return (long long)st.st_size;
}

bool path_samefile(const std::string& p1, const std::string& p2)
{
#ifdef _WIN32
    std::string cp1, cp2;
    cp1 = path_canon(p1);
    cp2 = path_canon(p2);
    return !cp1.compare(cp2);
#else
    struct stat st1, st2;
    if (stat(p1.c_str(), &st1))
        return false;
    if (stat(p2.c_str(), &st2))
        return false;
    if (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino) {
        return true;
    }
    return false;
#endif
}

int path_fileprops(const std::string path, struct PathStat *stp, bool follow)
{
    if (nullptr == stp) {
        return -1;
    }
    memset(stp, 0, sizeof(struct PathStat));
    struct STATBUF mst;
    SYSPATH(path, syspath);
    int ret = follow ? STAT(syspath, &mst) : LSTAT(syspath, &mst);
    if (ret != 0) {
        return ret;
    }
    stp->pst_size = mst.st_size;
    stp->pst_mode = mst.st_mode;
    stp->pst_mtime = mst.st_mtime;
#ifdef _WIN32
    stp->pst_ctime = mst.st_mtime;
#else
    stp->pst_ino = mst.st_ino;
    stp->pst_dev = mst.st_dev;
    stp->pst_ctime = mst.st_ctime;
    stp->pst_blocks = mst.st_blocks;
    stp->pst_blksize = mst.st_blksize;
#endif
    switch (mst.st_mode & S_IFMT) {
    case S_IFDIR: stp->pst_type = PathStat::PST_DIR;break;
    case S_IFLNK:  stp->pst_type = PathStat::PST_SYMLINK;break;
    case S_IFREG: stp->pst_type = PathStat::PST_REGULAR;break;
    default: stp->pst_type = PathStat::PST_OTHER;break;
    }
    return 0;
}

bool path_exists(const string& path)
{
    SYSPATH(path, syspath);
    return ACCESS(syspath, 0) == 0;
}
bool path_readable(const string& path)
{
    SYSPATH(path, syspath);
    return ACCESS(syspath, R_OK) == 0;
}
bool path_access(const std::string& path, int mode)
{
    SYSPATH(path, syspath);
    return ACCESS(syspath, mode) == 0;
}

/* There is a lot of vagueness about what should be percent-encoded or
 * not in a file:// url. The constraint that we have is that we may use
 * the encoded URL to compute (MD5) a thumbnail path according to the
 * freedesktop.org thumbnail spec, which itself does not define what
 * should be escaped. We choose to exactly escape what gio does, as
 * implemented in glib/gconvert.c:g_escape_uri_string(uri, UNSAFE_PATH). 
 * Hopefully, the other desktops have the same set of escaped chars. 
 * Note that $ is not encoded, so the value is not shell-safe.
 */
string url_encode(const string& url, string::size_type offs)
{
    string out = url.substr(0, offs);
    const char *cp = url.c_str();
    for (string::size_type i = offs; i < url.size(); i++) {
        unsigned int c;
        const char *h = "0123456789ABCDEF";
        c = cp[i];
        if (c <= 0x20 ||
            c >= 0x7f ||
            c == '"' ||
            c == '#' ||
            c == '%' ||
            c == ';' ||
            c == '<' ||
            c == '>' ||
            c == '?' ||
            c == '[' ||
            c == '\\' ||
            c == ']' ||
            c == '^' ||
            c == '`' ||
            c == '{' ||
            c == '|' ||
            c == '}') {
            out += '%';
            out += h[(c >> 4) & 0xf];
            out += h[c & 0xf];
        } else {
            out += char(c);
        }
    }
    return out;
}

static inline int h2d(int c) {
    if ('0' <= c && c <= '9')
        return c - '0';
    else if ('A' <= c && c <= 'F')
        return 10 + c - 'A';
    else if ('a' <= c && c <= 'f')
        return 10 + c - 'a';
    else 
        return -1;
}

string url_decode(const string &in)
{
    if (in.size() <= 2)
        return in;
    string out;
    out.reserve(in.size());
    const char *cp = in.c_str();
    string::size_type i = 0;
    for (; i < in.size() - 2; i++) {
        if (cp[i] == '%') {
            int d1 = h2d(cp[i+1]);
            int d2 = h2d(cp[i+2]);
            if (d1 != -1 && d2 != -1) {
                out += (d1 << 4) + d2;
            } else {
                out += '%';
                out += cp[i+1];
                out += cp[i+2];
            }
            i += 2;
        } else {
            out += cp[i];
        }
    }
    while (i < in.size()) {
        out += cp[i++];
    }
    return out;
}

string url_gpath(const string& url)
{
    // Remove the access schema part (or whatever it's called)
    string::size_type colon = url.find_first_of(":");
    if (colon == string::npos || colon == url.size() - 1) {
        return url;
    }
    // If there are non-alphanum chars before the ':', then there
    // probably is no scheme. Whatever...
    for (string::size_type i = 0; i < colon; i++) {
        if (!isalnum(url.at(i))) {
            return url;
        }
    }

    // In addition we canonize the path to remove empty host parts
    // (for compatibility with older versions of recoll where file://
    // was hardcoded, but the local path was used for doc
    // identification.
    return path_canon(url.substr(colon + 1));
}

string url_parentfolder(const string& url)
{
    // In general, the parent is the directory above the full path
    string parenturl = path_getfather(url_gpath(url));
    // But if this is http, make sure to keep the host part. Recoll
    // only has file or http urls for now.
    bool isfileurl = urlisfileurl(url);
    if (!isfileurl && parenturl == "/") {
        parenturl = url_gpath(url);
    }
    return isfileurl ? string("file://") + parenturl :
        string("http://") + parenturl;
}


// Convert to file path if url is like file:
// Note: this only works with our internal pseudo-urls which are not
// encoded/escaped
string fileurltolocalpath(string url)
{
    if (url.find("file://") == 0) {
        url = url.substr(7, string::npos);
    } else {
        return string();
    }

#ifdef _WIN32
    // Absolute file urls are like: file:///c:/mydir/...
    // Get rid of the initial '/'
    if (url.size() >= 3 && url[0] == '/' && isalpha(url[1]) && url[2] == ':') {
        url = url.substr(1);
    }
#endif

    // Removing the fragment part. This is exclusively used when
    // executing a viewer for the recoll manual, and we only strip the
    // part after # if it is preceded by .html
    string::size_type pos;
    if ((pos = url.rfind(".html#")) != string::npos) {
        url.erase(pos + 5);
    } else if ((pos = url.rfind(".htm#")) != string::npos) {
        url.erase(pos + 4);
    }

    return url;
}

static const string cstr_fileu("file://");

string path_pathtofileurl(const string& path)
{
    // We're supposed to receive a canonic absolute path, but on windows we
    // may need to add a '/' in front of the drive spec
    string url(cstr_fileu);
    if (path.empty() || path[0] != '/') {
        url.push_back('/');
    }
    url += path;
    return url;
}

bool urlisfileurl(const string& url)
{
    return url.find("file://") == 0;
}

static std::regex
re_uriparse("^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\\?([^#]*))?(#(.*))?",
            std::regex::extended);

ParsedUri::ParsedUri(std::string uri)
{
    std::smatch mr;
    parsed = regex_match(uri, mr, re_uriparse);
    if (!parsed)
        return;
    // cf http://www.ietf.org/rfc/rfc2396.txt
    // scheme    = $2
    // authority = $4
    // path      = $5
    // query     = $7
    // fragment  = $9
    if (mr[2].matched) {
        scheme = mr[2].str();
    }
    if (mr[4].matched) {
        string auth = mr[4].str();
        // user:pass@host, user@host
        string::size_type at = auth.find_first_of('@');
        if (at != string::npos) {
            host = auth.substr(at+1);
            string::size_type colon = auth.find_first_of(':');
            if (colon != string::npos && colon < at) {
                user = auth.substr(0, colon);
                pass = auth.substr(colon+1, at-colon-1);
            } else {
                user = auth.substr(0, at);
            }
        } else {
            host.swap(auth);
        }
        string::size_type pc = host.find_first_of(':');
        if (pc != string::npos) {
            port = host.substr(pc+1);
            host = host.substr(0, pc);
        }
    }
    if (mr[5].matched) {
        path = mr[5].str();
    }
    if (mr[7].matched) {
        query = mr[7].str();
        string::size_type pos=0, amp, eq;
        string nm, val;
        for (;;) {
            nm.clear();
            val.clear();
            amp = query.find_first_of('&', pos);
            //cerr << "pos " << pos << " amp " << amp << endl;
            if (amp > pos && amp != string::npos) {
                eq = query.find_first_of('=', pos);
                if (eq > amp || eq == string::npos) {
                    nm = query.substr(pos, amp-pos);
                } else {
                    nm = query.substr(pos, eq-pos);
                    val = query.substr(eq+1, amp-eq-1);
                }
                pos = amp + 1;
            } else if (amp == string::npos) {
                if (pos < query.size()-1) {
                    eq = query.find_first_of('=', pos);
                    if (eq == string::npos) {
                        nm = query.substr(pos);
                    } else {
                        nm = query.substr(pos, eq-pos);
                        val = query.substr(eq+1);
                    }
                }
                pos = query.size()-1;
            } else {
                pos++;
            }
            if (!nm.empty()) {
                parsedquery.push_back(pair<string,string>(nm, val));
            }
            if (pos >= query.size()-1) {
                break;
            }
        }
        
    }
    if (mr[9].matched) {
        fragment = mr[9].str();
    }
}

/// Directory reading interface. UTF-8 on Windows.
class PathDirContents::Internal {
public:
    ~Internal() {
        if (dirhdl) {
            CLOSEDIR(dirhdl);
        }
    }
        
    DIRHDL *dirhdl{nullptr};
    PathDirContents::Entry entry;
    std::string dirpath;
};

PathDirContents::PathDirContents(const std::string& dirpath)
{
    m = new Internal;
    m->dirpath = dirpath;
}

PathDirContents::~PathDirContents()
{
    delete m;
}

bool PathDirContents::opendir()
{
    if (m->dirhdl) {
        CLOSEDIR(m->dirhdl);
        m->dirhdl = nullptr;
    }
    const std::string& dp{m->dirpath};
    SYSPATH(dp, sysdir);
    m->dirhdl = OPENDIR(sysdir);
#ifdef _WIN32
    if (nullptr == m->dirhdl) {
        int rc = GetLastError();
        LOGERR("opendir failed: LastError " << rc << endl);
        if (rc == ERROR_NETNAME_DELETED) {
            // 64: share disconnected.
            // Not too sure of the errno in this case.
            // Make sure it's not one of the permissible ones
            errno = ENODEV;
        }
    }
#endif
    return nullptr != m->dirhdl;
}

void PathDirContents::rewinddir()
{
    REWINDDIR(m->dirhdl);
}

const struct PathDirContents::Entry* PathDirContents::readdir()
{
    struct DIRENT *ent = READDIR(m->dirhdl);
    if (nullptr == ent) {
        return nullptr;
    }
#ifdef _WIN32
    string sdname;
    if (!wchartoutf8(ent->d_name, sdname)) {
        LOGERR("wchartoutf8 failed for " << ent->d_name << endl);
        return nullptr;
    }
    const char *dname = sdname.c_str();
#else
    const char *dname = ent->d_name;
#endif
    m->entry.d_name = dname;
    return &m->entry;
}


bool listdir(const string& dir, string& reason, set<string>& entries)
{
    ostringstream msg;
    PathDirContents dc(dir);
    
    if (!path_isdir(dir)) {
        msg << "listdir: " << dir <<  " not a directory";
        goto out;
    }
    if (!path_access(dir, R_OK)) {
        msg << "listdir: no read access to " << dir;
        goto out;
    }

    if (!dc.opendir()) {
        msg << "listdir: cant opendir " << dir << ", errno " << errno;
        goto out;
    }
    const struct PathDirContents::Entry *ent;
    while ((ent = dc.readdir()) != 0) {
        if (ent->d_name == "." || ent->d_name == "..") {
            continue;
        }
        entries.insert(ent->d_name);
    }

out:
    reason = msg.str();
    if (reason.empty()) {
        return true;
    }
    return false;
}

// We do not want to mess with the pidfile content in the destructor:
// the lock might still be in use in a child process. In fact as much
// as we'd like to reset the pid inside the file when we're done, it
// would be very difficult to do it right and it's probably best left
// alone.
Pidfile::~Pidfile()
{
    this->close();
}

int Pidfile::read_pid()
{
    SYSPATH(m_path, syspath);
    int fd = OPEN(syspath, O_RDONLY);
    if (fd == -1) {
        return -1;
    }

    char buf[16];
    int i = read(fd, buf, sizeof(buf) - 1);
    ::close(fd);
    if (i <= 0) {
        return -1;
    }
    buf[i] = '\0';
    char *endptr;
    int pid = strtol(buf, &endptr, 10);
    if (endptr != &buf[i]) {
        return - 1;
    }
    return pid;
}

int Pidfile::flopen()
{
    SYSPATH(m_path, syspath);
    if ((m_fd = OPEN(syspath, O_RDWR | O_CREAT, 0644)) == -1) {
        m_reason = "Open failed: [" + m_path + "]: " + strerror(errno);
        return -1;
    }

#ifdef sun
    struct flock lockdata;
    lockdata.l_start = 0;
    lockdata.l_len = 0;
    lockdata.l_type = F_WRLCK;
    lockdata.l_whence = SEEK_SET;
    if (fcntl(m_fd, F_SETLK,  &lockdata) != 0) {
        int serrno = errno;
        this->close()
            errno = serrno;
        m_reason = "fcntl lock failed";
        return -1;
    }
#else
    int operation = LOCK_EX | LOCK_NB;
    if (flock(m_fd, operation) == -1) {
        int serrno = errno;
        this->close();
        errno = serrno;
        m_reason = "flock failed";
        return -1;
    }
#endif // ! sun

    if (ftruncate(m_fd, 0) != 0) {
        /* can't happen [tm] */
        int serrno = errno;
        this->close();
        errno = serrno;
        m_reason = "ftruncate failed";
        return -1;
    }
    return 0;
}

int Pidfile::open()
{
    if (flopen() < 0) {
        return read_pid();
    }
    return 0;
}

int Pidfile::write_pid()
{
    /* truncate to allow multiple calls */
    if (ftruncate(m_fd, 0) == -1) {
        m_reason = "ftruncate failed";
        return -1;
    }
    char pidstr[20];
    sprintf(pidstr, "%u", int(getpid()));
    lseek(m_fd, 0, 0);
    if (::write(m_fd, pidstr, strlen(pidstr)) != (PATHUT_SSIZE_T)strlen(pidstr)) {
        m_reason = "write failed";
        return -1;
    }
    return 0;
}

int Pidfile::close()
{
    int ret = -1;
    if (m_fd >= 0) {
        ret = ::close(m_fd);
        m_fd = -1;
    }
    return ret;
}

int Pidfile::remove()
{
    SYSPATH(m_path, syspath);
    return UNLINK(syspath);
}

// Call funcs that need static init (not initially reentrant)
void pathut_init_mt()
{
    path_home();
}
