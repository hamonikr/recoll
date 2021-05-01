/*
Copyright (c) 2009 Jean-Francois Dockes

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

/** \file pxattr.cpp 
    \brief Portable External Attributes API
 */

// PXALINUX: platforms like kfreebsd which aren't linux but use the
// same xattr interface
#if defined(__gnu_linux__) || \
    (defined(__FreeBSD_kernel__)&&defined(__GLIBC__)&&!defined(__FreeBSD__)) ||\
    defined(__CYGWIN__)
#define PXALINUX
#endif

// If the platform is not known yet, let this file be empty instead of
// breaking the compile, this will let the build work if the rest of
// the software is not actually calling us. If it does call us, this
// will bring attention to the necessity of a port.
//
// If the platform is known not to support extattrs (e.g.__OpenBSD__),
// just let the methods return errors (like they would on a non-xattr
// fs on e.g. linux)

#if defined(__DragonFly__) || defined(__OpenBSD__)
#define HAS_NO_XATTR
#endif

#if defined(__FreeBSD__) || defined(PXALINUX) || defined(__APPLE__) \
    || defined(HAS_NO_XATTR)


#ifndef TEST_PXATTR
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(__FreeBSD__)
#include <sys/extattr.h>
#include <sys/uio.h>
#elif defined(PXALINUX)
#include <sys/xattr.h>
#elif defined(__APPLE__)
#include <sys/xattr.h>
#elif defined(HAS_NO_XATTR)
#else
#error "Unknown system can't compile"
#endif

#include "pxattr.h"

namespace pxattr {

class AutoBuf {
public:
    char *buf;
    AutoBuf() : buf(0) {}
    ~AutoBuf() {if (buf) free(buf); buf = 0;}
    bool alloc(int n) 
    {
	if (buf) {
	    free(buf);
	    buf = 0;
	}
	buf = (char *)malloc(n); 
	return buf != 0;
    }
};

static bool 
get(int fd, const string& path, const string& _name, string *value,
    flags flags, nspace dom)
{
    string name;
    if (!sysname(dom, _name, &name)) 
	return false;

    ssize_t ret = -1;
    AutoBuf buf;

#if defined(__FreeBSD__)
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = extattr_get_link(path.c_str(), EXTATTR_NAMESPACE_USER, 
				   name.c_str(), 0, 0);
	} else {
	    ret = extattr_get_file(path.c_str(), EXTATTR_NAMESPACE_USER, 
				   name.c_str(), 0, 0);
	}
    } else {
	ret = extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, name.c_str(), 0, 0);
    }
    if (ret < 0)
	return false;
    if (!buf.alloc(ret+1)) // Don't want to deal with possible ret=0
	return false;
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = extattr_get_link(path.c_str(), EXTATTR_NAMESPACE_USER, 
				   name.c_str(), buf.buf, ret);
	} else {
	    ret = extattr_get_file(path.c_str(), EXTATTR_NAMESPACE_USER, 
				   name.c_str(), buf.buf, ret);
	}
    } else {
	ret = extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, 
			     name.c_str(), buf.buf, ret);
    }
#elif defined(PXALINUX)
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = lgetxattr(path.c_str(), name.c_str(), 0, 0);
	} else {
	    ret = getxattr(path.c_str(), name.c_str(), 0, 0);
	}
    } else {
	ret = fgetxattr(fd, name.c_str(), 0, 0);
    }
    if (ret < 0)
	return false;
    if (!buf.alloc(ret+1)) // Don't want to deal with possible ret=0
	return false;
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = lgetxattr(path.c_str(), name.c_str(), buf.buf, ret);
	} else {
	    ret = getxattr(path.c_str(), name.c_str(), buf.buf, ret);
	}
    } else {
	ret = fgetxattr(fd, name.c_str(), buf.buf, ret);
    }
#elif defined(__APPLE__)
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = getxattr(path.c_str(), name.c_str(), 0, 0, 0, XATTR_NOFOLLOW);
	} else {
	    ret = getxattr(path.c_str(), name.c_str(), 0, 0, 0, 0);
	}
    } else {
	ret = fgetxattr(fd, name.c_str(), 0, 0, 0, 0);
    }
    if (ret < 0)
	return false;
    if (!buf.alloc(ret+1)) // Don't want to deal with possible ret=0
	return false;
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = getxattr(path.c_str(), name.c_str(), buf.buf, ret, 0, 
			   XATTR_NOFOLLOW);
	} else {
	    ret = getxattr(path.c_str(), name.c_str(), buf.buf, ret, 0, 0);
	}
    } else {
	ret = fgetxattr(fd, name.c_str(), buf.buf, ret, 0, 0);
    }
#else
    errno = ENOTSUP;
#endif

    if (ret >= 0)
	value->assign(buf.buf, ret);
    return ret >= 0;
}

static bool 
set(int fd, const string& path, const string& _name, 
    const string& value, flags flags, nspace dom)
{
    string name;
    if (!sysname(dom, _name, &name)) 
	return false;

    ssize_t ret = -1;

#if defined(__FreeBSD__)
    
    if (flags & (PXATTR_CREATE|PXATTR_REPLACE)) {
	// Need to test existence
	bool exists = false;
	ssize_t eret;
	if (fd < 0) {
	    if (flags & PXATTR_NOFOLLOW) {
		eret = extattr_get_link(path.c_str(), EXTATTR_NAMESPACE_USER, 
				       name.c_str(), 0, 0);
	    } else {
		eret = extattr_get_file(path.c_str(), EXTATTR_NAMESPACE_USER, 
				       name.c_str(), 0, 0);
	    }
	} else {
	    eret = extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, 
				  name.c_str(), 0, 0);
	}
	if (eret >= 0)
	    exists = true;
	if (eret < 0 && errno != ENOATTR)
	    return false;
	if ((flags & PXATTR_CREATE) && exists) {
	    errno = EEXIST;
	    return false;
	}
	if ((flags & PXATTR_REPLACE) && !exists) {
	    errno = ENOATTR;
	    return false;
	}
    }
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = extattr_set_link(path.c_str(), EXTATTR_NAMESPACE_USER, 
				   name.c_str(), value.c_str(), value.length());
	} else {
	    ret = extattr_set_file(path.c_str(), EXTATTR_NAMESPACE_USER, 
				   name.c_str(), value.c_str(), value.length());
	}
    } else {
	ret = extattr_set_fd(fd, EXTATTR_NAMESPACE_USER, 
			     name.c_str(), value.c_str(), value.length());
    }
#elif defined(PXALINUX)
    int opts = 0;
    if (flags & PXATTR_CREATE)
	opts = XATTR_CREATE;
    else if (flags & PXATTR_REPLACE)
	opts = XATTR_REPLACE;
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = lsetxattr(path.c_str(), name.c_str(), value.c_str(), 
			    value.length(), opts);
	} else {
	    ret = setxattr(path.c_str(), name.c_str(), value.c_str(), 
			   value.length(), opts);
	}
    } else {
	ret = fsetxattr(fd, name.c_str(), value.c_str(), value.length(), opts);
    }
#elif defined(__APPLE__)
    int opts = 0;
    if (flags & PXATTR_CREATE)
	opts = XATTR_CREATE;
    else if (flags & PXATTR_REPLACE)
	opts = XATTR_REPLACE;
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = setxattr(path.c_str(), name.c_str(), value.c_str(), 
			   value.length(),  0, XATTR_NOFOLLOW|opts);
	} else {
	    ret = setxattr(path.c_str(), name.c_str(), value.c_str(), 
			   value.length(),  0, opts);
	}
    } else {
	ret = fsetxattr(fd, name.c_str(), value.c_str(), 
			value.length(), 0, opts);
    }
#else
    errno = ENOTSUP;
#endif
    return ret >= 0;
}

static bool 
del(int fd, const string& path, const string& _name, flags flags, nspace dom) 
{
    string name;
    if (!sysname(dom, _name, &name)) 
	return false;

    int ret = -1;

#if defined(__FreeBSD__)
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = extattr_delete_link(path.c_str(), EXTATTR_NAMESPACE_USER,
				      name.c_str());
	} else {
	    ret = extattr_delete_file(path.c_str(), EXTATTR_NAMESPACE_USER,
				      name.c_str());
	}
    } else {
	ret = extattr_delete_fd(fd, EXTATTR_NAMESPACE_USER, name.c_str());
    }
#elif defined(PXALINUX)
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = lremovexattr(path.c_str(), name.c_str());
	} else {
	    ret = removexattr(path.c_str(), name.c_str());
	}
    } else {
	ret = fremovexattr(fd, name.c_str());
    }
#elif defined(__APPLE__)
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = removexattr(path.c_str(), name.c_str(), XATTR_NOFOLLOW);
	} else {
	    ret = removexattr(path.c_str(), name.c_str(), 0);
	}
    } else {
	ret = fremovexattr(fd, name.c_str(), 0);
    }
#else
    errno = ENOTSUP;
#endif
    return ret >= 0;
}

static bool 
list(int fd, const string& path, vector<string>* names, flags flags, nspace dom)
{
    ssize_t ret = -1;
    AutoBuf buf;

#if defined(__FreeBSD__)
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = extattr_list_link(path.c_str(), EXTATTR_NAMESPACE_USER, 0, 0);
	} else {
	    ret = extattr_list_file(path.c_str(), EXTATTR_NAMESPACE_USER, 0, 0);
	}
    } else {
	ret = extattr_list_fd(fd, EXTATTR_NAMESPACE_USER, 0, 0);
    }
    if (ret < 0) 
	return false;
    if (!buf.alloc(ret+1)) // NEEDED on FreeBSD (no ending null)
	return false;
    buf.buf[ret] = 0;
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = extattr_list_link(path.c_str(), EXTATTR_NAMESPACE_USER, 
				    buf.buf, ret);
	} else {
	    ret = extattr_list_file(path.c_str(), EXTATTR_NAMESPACE_USER, 
				    buf.buf, ret);
	}
    } else {
	ret = extattr_list_fd(fd, EXTATTR_NAMESPACE_USER, buf.buf, ret);
    }
#elif defined(PXALINUX)
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = llistxattr(path.c_str(), 0, 0);
	} else {
	    ret = listxattr(path.c_str(), 0, 0);
	}
    } else {
	ret = flistxattr(fd, 0, 0);
    }
    if (ret < 0) 
	return false;
    if (!buf.alloc(ret+1)) // Don't want to deal with possible ret=0
	return false;
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = llistxattr(path.c_str(), buf.buf, ret);
	} else {
	    ret = listxattr(path.c_str(), buf.buf, ret);
	}
    } else {
	ret = flistxattr(fd, buf.buf, ret);
    }
#elif defined(__APPLE__)
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = listxattr(path.c_str(), 0, 0, XATTR_NOFOLLOW);
	} else {
	    ret = listxattr(path.c_str(), 0, 0, 0);
	}
    } else {
	ret = flistxattr(fd, 0, 0, 0);
    }
    if (ret < 0) 
	return false;
    if (!buf.alloc(ret+1)) // Don't want to deal with possible ret=0
	return false;
    if (fd < 0) {
	if (flags & PXATTR_NOFOLLOW) {
	    ret = listxattr(path.c_str(), buf.buf, ret, XATTR_NOFOLLOW);
	} else {
	    ret = listxattr(path.c_str(), buf.buf, ret, 0);
	}
    } else {
	ret = flistxattr(fd, buf.buf, ret, 0);
    }
#else
    errno = ENOTSUP;
#endif

    if (ret < 0)
        return false;

    char *bufstart = buf.buf;

    // All systems return a 0-separated string list except FreeBSD
    // which has length, value pairs, length is a byte. 
#if defined(__FreeBSD__)
    char *cp = buf.buf;
    unsigned int len;
    while (cp < buf.buf + ret + 1) {
	len = *cp;
	*cp = 0;
	cp += len + 1;
    }
    bufstart = buf.buf + 1;
    *cp = 0; // don't forget, we allocated one more
#endif


    if (ret > 0) {
	int pos = 0;
	while (pos < ret) {
	    string n = string(bufstart + pos);
	    string n1;
	    if (pxname(PXATTR_USER, n, &n1)) {
		names->push_back(n1);
	    }
	    pos += n.length() + 1;
	}
    }
    return true;
}

static const string nullstring("");

bool get(const string& path, const string& _name, string *value,
	 flags flags, nspace dom)
{
    return get(-1, path, _name, value, flags, dom);
}
bool get(int fd, const string& _name, string *value, flags flags, nspace dom)
{
    return get(fd, nullstring, _name, value, flags, dom);
}
bool set(const string& path, const string& _name, const string& value,
	 flags flags, nspace dom)
{
    return set(-1, path, _name, value, flags, dom);
}
bool set(int fd, const string& _name, const string& value, 
	 flags flags, nspace dom)
{
    return set(fd, nullstring, _name, value, flags, dom);
}
bool del(const string& path, const string& _name, flags flags, nspace dom) 
{
    return del(-1, path, _name, flags, dom);
}
bool del(int fd, const string& _name, flags flags, nspace dom) 
{
    return del(fd, nullstring, _name, flags, dom);
}
bool list(const string& path, vector<string>* names, flags flags, nspace dom)
{
    return list(-1, path, names, flags, dom);
}
bool list(int fd, vector<string>* names, flags flags, nspace dom)
{
    return list(fd, nullstring, names, flags, dom);
}

#if defined(PXALINUX) || defined(COMPAT1)
static const string userstring("user.");
#else
static const string userstring("");
#endif
bool sysname(nspace dom, const string& pname, string* sname)
{
    if (dom != PXATTR_USER) {
	errno = EINVAL;
	return false;
     }
    *sname = userstring + pname;
    return true;
}

bool pxname(nspace dom, const string& sname, string* pname) 
{
    if (!userstring.empty() && sname.find(userstring) != 0) {
	errno = EINVAL;
	return false;
    }
    *pname = sname.substr(userstring.length());
    return true;
}

} // namespace pxattr

#else // TEST_PXATTR Testing / driver ->

#include "pxattr.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ftw.h>
#include <sys/types.h>
#include <regex.h>

#include <iostream>
#include <fstream>
#include <map>
#include <algorithm>
#include <string>

using namespace std;


static int antiverbose;

static void printsyserr(const string& msg)
{
    if (antiverbose >= 2)
        return;
    cerr << msg << " " << strerror(errno) << endl;
}

#define message(X)                              \
    {                                           \
        if (antiverbose == 0) {                 \
            cout << X;                          \
        }                                       \
    }
    
static void dotests();

// \-quote character c in input \ -> \\, nl -> \n cr -> \rc -> \c
static void quote(const string& in, string& out, int c)
{
    out.clear();
    for (string::const_iterator it = in.begin(); it != in.end(); it++) {
	if (*it == '\\') {
	    out += "\\\\";
	} else if (*it == "\n"[0]) {
	    out += "\\n";
	} else if (*it == "\r"[0]) {
	    out += "\\r";
	} else if (*it == c) {
	    out += "\\";
	    out += c;
	} else {
	    out += *it;
	}
    }
}

// \-unquote input \n -> nl, \r -> cr, \c -> c
static void unquote(const string& in, string& out)
{
    out.clear();
    for (unsigned int i = 0; i < in.size(); i++) {
	if (in[i] == '\\') {
	    if (i == in.size() -1) {
		out += in[i];
	    } else {
		int c = in[++i];
		switch (c) {
		case 'n': out += "\n";break;
		case 'r': out += "\r";break;
		default: out += c;
		}
	    }
	} else {
	    out += in[i];
	}
    }
}

// Find first unquoted c in input: c preceded by odd number of backslashes
string::size_type find_first_unquoted(const string& in, int c)
{
    int q = 0;
    for (unsigned int i = 0;i < in.size(); i++) {
	if (in[i] == '\\') {
	    q++;
	} else if (in[i] == c) {
	    if (q&1) {
		// quoted
		q = 0;
	    } else {
		return i;
	    }
	} else {
	    q = 0;
	}
    }
    return string::npos;
}

static const string PATH_START("Path: ");
static bool listattrs(const string& path)
{
    vector<string> names;
    if (!pxattr::list(path, &names)) {
	if (errno == ENOENT) {
	    return false;
	}
	printsyserr("pxattr::list");
	exit(1);
    }
    if (names.empty()) {
        return true;
    }

    // Sorting the names would not be necessary but it makes easier comparing
    // backups
    sort(names.begin(), names.end());

    string quoted;
    quote(path, quoted, 0);
    message(PATH_START << quoted << endl);
    for (vector<string>::const_iterator it = names.begin(); 
	 it != names.end(); it++) {
	string value;
	if (!pxattr::get(path, *it, &value)) {
	    if (errno == ENOENT) {
		return false;
	    }
	    printsyserr("pxattr::get");
	    exit(1);
	}
	quote(*it, quoted, '=');
	message(" " << quoted << "=");
	quote(value, quoted, 0);
	message(quoted << endl);
    }
    return true;
}

bool setxattr(const string& path, const string& name, const string& value)
{
    if (!pxattr::set(path, name, value)) {
	printsyserr("pxattr::set");
	return false;
    }
    return true;
}

bool printxattr(const string &path, const string& name)
{
    string value;
    if (!pxattr::get(path, name, &value)) {
	if (errno == ENOENT) {
	    return false;
	}
	printsyserr("pxattr::get");
        return false;
    }
    message(PATH_START << path << endl);
    message(" " << name << " => " << value << endl);
    return true;
}

bool delxattr(const string &path, const string& name) 
{
    if (pxattr::del(path, name) < 0) {
	printsyserr("pxattr::del");
        return false;
    }
    return true;
}

// Restore xattrs stored in file created by pxattr -lR output
static void restore(const char *backupnm)
{
    istream *input;
    ifstream fin;
    if (!strcmp(backupnm, "stdin")) {
	input = &cin;
    } else {
	fin.open(backupnm, ios::in);
	input = &fin;
    }

    bool done = false;
    int linenum = 0;
    string path;
    map<string, string> attrs;
    while (!done) {
	string line;
	getline(*input, line);
	if (!input->good()) {
	    if (input->bad()) {
                cerr << "Input I/O error" << endl;
		exit(1);
	    }
	    done = true;
	} else {
	    linenum++;
	}

	// message("Got line " << linenum << " : [" << line << "] done " << 
	// done << endl);

	if (line.find(PATH_START) == 0 || done) {
	    if (!path.empty() && !attrs.empty()) {
		for (map<string,string>::const_iterator it = attrs.begin();
		     it != attrs.end(); it++) {
		    setxattr(path, it->first, it->second);
		}
	    }
	    if (!done) {
		line = line.substr(PATH_START.size(), string::npos);
		unquote(line, path);
		attrs.clear();
	    }
	} else if (line.empty()) {
	    continue;
	} else {
	    // Should be attribute line
	    if (line[0] != ' ') {
		cerr << "Found bad line (no space) at " << linenum << endl;
		exit(1);
	    }
	    string::size_type pos = find_first_unquoted(line, '=');
	    if (pos == string::npos || pos < 2 || pos >= line.size()) {
		cerr << "Found bad line at " << linenum << endl;
		exit(1);
	    }
	    string qname = line.substr(1, pos-1);
	    pair<string,string> entry;
	    unquote(qname, entry.first);
	    unquote(line.substr(pos+1), entry.second);
	    attrs.insert(entry);
	}
    }
}

static char *thisprog;
static char usage [] =
"pxattr [-hs] -n name pathname [...] : show value for name\n"
"pxattr [-hs] -n name -r regexp pathname [...] : test value against regexp\n"
"pxattr [-hs] -n name -v value pathname [...] : add/replace attribute\n"
"pxattr [-hs] -x name pathname [...] : delete attribute\n"
"pxattr [-hs] [-l] [-R] pathname [...] : list attribute names and values\n"
"  For all the options above, if no pathname arguments are given, pxattr\n"
"  will read file names on stdin, one per line.\n"
" [-h] : don't follow symbolic links (act on link itself)\n"
" [-R] : recursive listing. Args should be directory(ies)\n"
" [-s] : be silent. With one option stdout is suppressed, with 2 stderr too\n"
"pxattr -S <backupfile> Restore xattrs from file created by pxattr -lR output\n"
"               if backupfile is 'stdin', reads from stdin\n"
"pxattr -T: run tests on temp file in current directory" 
"\n"
;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_h     0x2
#define OPT_l     0x4
#define OPT_n	  0x8
#define OPT_r     0x10
#define OPT_R     0x20
#define OPT_S     0x40
#define OPT_T     0x80
#define OPT_s     0x100
#define OPT_v	  0x200
#define OPT_x     0x400

// Static values for ftw
static string name, value;

bool regex_test(const char *path, regex_t *preg)
{
    string value;
    if (!pxattr::get(path, name, &value)) {
	if (errno == ENOENT) {
	    return false;
	}
	printsyserr("pxattr::get");
        return false;
    }

    int ret = regexec(preg, value.c_str(), 0, 0, 0);
    if (ret == 0) {
        message(path << endl);
        return true;
    } else if (ret == REG_NOMATCH) {
        return false;
    } else {
        char errmsg[200];
        regerror(ret, preg, errmsg, 200);
        errno = 0;
        printsyserr("regexec");
        return false;
    }
}

bool processfile(const char* fn, const struct stat *, int)
{
    //message("processfile " << fn << " opflags " << op_flags << endl);

    if (op_flags & OPT_l) {
	return listattrs(fn);
    } else if (op_flags & OPT_n) {
	if (op_flags & OPT_v) {
	    return setxattr(fn, name, value);
	} else {
	    return printxattr(fn, name);
	} 
    } else if (op_flags & OPT_x) {
	return delxattr(fn, name);
    }
    Usage();
}

int ftwprocessfile(const char* fn, const struct stat *sb, int typeflag)
{
    processfile(fn, sb, typeflag);
    return 0;
}

int main(int argc, char **argv)
{
    const char *regexp_string;
    thisprog = argv[0];
    argc--; argv++;
    
    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    /* Cas du "adb - core" */
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'l':	op_flags |= OPT_l; break;
	    case 'n':	op_flags |= OPT_n; if (argc < 2)  Usage();
		name = *(++argv); argc--; 
		goto b1;
	    case 'R':	op_flags |= OPT_R; break;
	    case 'r':	op_flags |= OPT_r; if (argc < 2)  Usage();
		regexp_string = *(++argv); argc--; 
		goto b1;
	    case 's':	antiverbose++; break;
	    case 'S':	op_flags |= OPT_S; break;
	    case 'T':	op_flags |= OPT_T; break;
	    case 'v':	op_flags |= OPT_v; if (argc < 2)  Usage();
		value = *(++argv); argc--; 
		goto b1;
	    case 'x':	op_flags |= OPT_x; if (argc < 2)  Usage();
		name = *(++argv); argc--; 
		goto b1;
	    default: Usage();	break;
	    }
    b1: argc--; argv++;
    }

    if (op_flags & OPT_T)  {
	if (argc > 0)
	    Usage();
	dotests();
	exit(0);
    }
    if ((op_flags & OPT_r) && !(op_flags & OPT_n)) {
        Usage();
    }
    
    if (op_flags & OPT_S)  {
	if (argc != 1)
	    Usage();
	restore(argv[0]);
	exit(0);
    }
    regex_t regexp;
    if (op_flags & OPT_r) {
        int err = regcomp(&regexp, regexp_string, REG_NOSUB|REG_EXTENDED);
        if (err) {
            char errmsg[200];
            regerror(err, &regexp, errmsg, 200);
            cerr << "regcomp(" << regexp_string << ") error: " << errmsg <<
                endl;
            exit(1);
        }
    }
    
    // Default option is 'list'
    if ((op_flags&(OPT_l|OPT_n|OPT_x)) == 0)
	op_flags |= OPT_l;

    bool readstdin = false;
    if (argc == 0)
	readstdin = true;

    int exitvalue = 0;
    for (;;) {
	const char *fn = 0;
	if (argc > 0) {
	    fn = *argv++; 
	    argc--;
	} else if (readstdin) {
	    static char filename[1025];
	    if (!fgets(filename, 1024, stdin))
		break;
	    filename[strlen(filename)-1] = 0;
	    fn = filename;
	} else
	    break;

	if (op_flags & OPT_R) {
	    if (ftw(fn, ftwprocessfile, 20))
		exit(1);
	} else if (op_flags & OPT_r) {
            if (!regex_test(fn, &regexp)) {
                exitvalue = 1;
            }
        } else {
	    if (!processfile(fn, 0, 0)) {
                exitvalue = 1;
            }
	}
    } 

    exit(exitvalue);
}

static void fatal(const string& s)
{
    printsyserr(s.c_str());
    exit(1);
}

static bool testbackups()
{
    static const char *top = "ttop";
    static const char *d1 = "d1";
    static const char *d2 = "d2";
    static const char *tfn1 = "tpxattr1.txt";
    static const char *tfn2 = "tpxattr2.txt";
    static const char *dump = "attrdump.txt";
    static const char *NAMES[] = {"ORG.PXATTR.NAME1", 
				  "ORG=PXATTR\"=\\=\n", 
				  "=", "Name4"};
    static const char *VALUES[] = 
	{"VALUE1", "VALUE2", "VALUE3=VALUE3equal",
	 "VALUE4\n is more like"
	 " normal text\n with new lines and \"\\\" \\\" backslashes"};

    static const int nattrs = sizeof(NAMES) / sizeof(char *);

    if (mkdir(top, 0777))
	fatal("Cant mkdir ttop");
    if (chdir(top))
	fatal("cant chdir ttop");
    if (mkdir(d1, 0777) || mkdir(d2, 0777))
	fatal("Can't mkdir ttdop/dx\n");
    if (chdir(d1))
	fatal("chdir d1");

    int fd;
    if ((fd = open(tfn1, O_RDWR|O_CREAT, 0755)) < 0)
	fatal("create d1/tpxattr1.txt");
    /* Set attrs */
    for (int i = 0; i < nattrs; i++) {
	if (!pxattr::set(fd, NAMES[i], VALUES[i]))
	    fatal("pxattr::set");
    }
    close(fd);
    if ((fd = open(tfn2, O_RDWR|O_CREAT, 0755)) < 0)
	fatal("create d1/tpxattr2.txt");
    /* Set attrs */
    for (int i = 0; i < nattrs; i++) {
	if (!pxattr::set(fd, NAMES[i], VALUES[i]))
	    fatal("pxattr::set");
    }
    close(fd);

    /* Create dump */
    string cmd;
    cmd = string("pxattr -lR . > " ) + dump;
    if (system(cmd.c_str()))
	fatal(cmd + " in d1");
    if (chdir("../d2"))
	fatal("chdir ../d2");
    if (close(open(tfn1, O_RDWR|O_CREAT, 0755)))
	fatal("create d2/tpxattr.txt");
    if (close(open(tfn2, O_RDWR|O_CREAT, 0755)))
	fatal("create d2/tpxattr.txt");
    cmd = string("pxattr -S ../d1/" ) + dump;
    if (system(cmd.c_str()))
	fatal(cmd);
    cmd = string("pxattr -lR . > " ) + dump;
    if (system(cmd.c_str()))
	fatal(cmd + " in d2");
    cmd = string("diff ../d1/") + dump + " " + dump;
    if (system(cmd.c_str()))
	fatal(cmd);
    cmd = string("cat ") + dump;
    system(cmd.c_str());

    if (1) {
	unlink(dump);
	unlink(tfn1);
	unlink(tfn2);
	if (chdir("../d1"))
	    fatal("chdir ../d1");
	unlink(dump);
	unlink(tfn1);
	unlink(tfn2);
	if (chdir("../"))
	    fatal("chdir .. 1");
	if (rmdir(d1))
	    fatal("rmdir d1");
	if (rmdir(d2))
	    fatal("rmdir d2");
	if (chdir("../"))
	    fatal("chdir .. 2");
	if (rmdir(top))
	    fatal("rmdir ttop");
    }
    return true;
}

static void dotests()
{
    static const char *tfn = "pxattr_testtmp.xyz";
    static const char *NAMES[] = {"ORG.PXATTR.NAME1", "ORG.PXATTR.N2", 
				  "ORG.PXATTR.LONGGGGGGGGisSSSHHHHHHHHHNAME3"};
    static const char *VALUES[] = {"VALUE1", "VALUE2", "VALUE3"};

    /* Create test file if it doesn't exist, remove all attributes */
    int fd = open(tfn, O_RDWR|O_CREAT, 0755);
    if (fd < 0) {
	printsyserr("open/create");
	exit(1);
    }

    if (!antiverbose)
	message("Cleanup old attrs\n");
    vector<string> names;
    if (!pxattr::list(tfn, &names)) {
	printsyserr("pxattr::list");
	exit(1);
    }
    for (vector<string>::const_iterator it = names.begin(); 
	 it != names.end(); it++) {
	string value;
	if (!pxattr::del(fd, *it)) {
	    printsyserr("pxattr::del");
	    exit(1);
	}
    }
    /* Check that there are no attributes left */
    names.clear();
    if (!pxattr::list(tfn, &names)) {
	printsyserr("pxattr::list");
	exit(1);
    }
    if (names.size() != 0) {
	errno=0;printsyserr("Attributes remain after initial cleanup !\n");
	for (vector<string>::const_iterator it = names.begin();
	     it != names.end(); it++) {
            if (antiverbose < 2)
                cerr << *it << endl;
	}
	exit(1);
    }

    /* Create attributes, check existence and value */
    message("Creating extended attributes\n");
    for (int i = 0; i < 3; i++) {
	if (!pxattr::set(fd, NAMES[i], VALUES[i])) {
	    printsyserr("pxattr::set");
	    exit(1);
	}
    }
    message("Checking creation\n");
    for (int i = 0; i < 3; i++) {
	string value;
	if (!pxattr::get(tfn, NAMES[i], &value)) {
	    printsyserr("pxattr::get");
	    exit(1);
	}
	if (value.compare(VALUES[i])) {
            errno = 0;
	    printsyserr("Wrong value after create !");
	    exit(1);
	}
    }

    /* Delete one, check list */
    message("Delete one\n");
    if (!pxattr::del(tfn, NAMES[1])) {
	printsyserr("pxattr::del one name");
	exit(1);
    }
    message("Check list\n");
    for (int i = 0; i < 3; i++) {
	string value;
	if (!pxattr::get(fd, NAMES[i], &value)) {
	    if (i == 1)
		continue;
	    printsyserr("pxattr::get");
	    exit(1);
	} else if (i == 1) {
	    errno=0;
            printsyserr("Name at index 1 still exists after deletion\n");
	    exit(1);
	}
	if (value.compare(VALUES[i])) {
            errno = 0;
	    printsyserr("Wrong value after delete 1 !");
	    exit(1);
	}
    }

    /* Test the CREATE/REPLACE flags */
    // Set existing with flag CREATE should fail
    message("Testing CREATE/REPLACE flags use\n");
    if (pxattr::set(tfn, NAMES[0], VALUES[0], pxattr::PXATTR_CREATE)) {
	errno=0;printsyserr("Create existing with flag CREATE succeeded !\n");
	exit(1);
    }
    // Set new with flag REPLACE should fail
    if (pxattr::set(tfn, NAMES[1], VALUES[1], pxattr::PXATTR_REPLACE)) {
	errno=0;printsyserr("Create new with flag REPLACE succeeded !\n");
	exit(1);
    }
    // Set new with flag CREATE should succeed
    if (!pxattr::set(fd, NAMES[1], VALUES[1], pxattr::PXATTR_CREATE)) {
	errno=0;printsyserr("Create new with flag CREATE failed !\n");
	exit(1);
    }
    // Set existing with flag REPLACE should succeed
    if (!pxattr::set(fd, NAMES[0], VALUES[0], pxattr::PXATTR_REPLACE)) {
	errno=0;printsyserr("Create existing with flag REPLACE failed !\n");
	exit(1);
    }
    close(fd);
    unlink(tfn);

    if (testbackups())
	exit(0);
    exit(1);
}
#endif // Testing pxattr

#endif // Supported systems.
