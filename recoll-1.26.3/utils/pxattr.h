#ifndef _pxattr_h_included_
#define _pxattr_h_included_

/* @(#$Id: pxattr.h,v 1.5 2009-01-20 13:48:34 dockes Exp $  (C) 2009 J.F.Dockes
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
#include <string>
#include <vector>
using std::string;
using std::vector;

/**
 * Provide a uniform C++ API for extended file attributes on Linux/FreeBSD 
 * and MacOSX.
 *
 * We only deal with user attributes. Other namespaces are very
 * system-specific and would be difficult to use in a portable way.
 *
 * Linux and FreeBSD treat differently the attributes name space
 * segmentation: Linux uses the first name segment ("user.", "system.", ...),
 * FreeBSD uses an enumeration.
 *
 * We handle this by using only domain-internal names in the interface:
 * that is, the caller specifies the names as, ie, 'org.myapp.somename' 
 * not 'user.org.myapp.somename'. pxattr will deal with adding/removing
 * the 'user.' part as needed. 
 *
 * MacOsX does not segment the attribute name space.
 * 
 * In order to avoid conflicts, it is recommended that attributes
 * names be chosen in a "reverse dns" fashion, ie:
 * org.recoll.indexing.status
 *
 * The interface provided should work the same way on all 3 systems,
 * it papers over such differences as the "list" output format,
 * the existence of CREATE/UPDATE distinctions, etc.
 * 
 * Diagnostics: all functions return false on error, and preserve the errno 
 * value or set it as appropriate.
 *
 * For path-based interfaces, the PXATTR_NOFOLLOW flag can be set to decide if
 * symbolic links will be acted on or followed.
 */
namespace pxattr {
    /** nspace might be used in the future if we support multiple namespaces.*/
    enum nspace {
	/** User name space */
	PXATTR_USER
    };

    /** Flags can modify the behaviour of some methods */
    enum flags {PXATTR_NONE=0, 
		/** Act on link instead of following it */
		PXATTR_NOFOLLOW = 1, 
		/** Fail if existing */
		PXATTR_CREATE=2, 
		/** Fail if new */
		PXATTR_REPLACE=4 
    };

    /**
     * Retrieve the named attribute from path. 
     */
    bool get(const string& path, const string& name, string* value, 
	     flags flags = PXATTR_NONE, nspace dom = PXATTR_USER);
    /**
     * Retrieve the named attribute from open file. 
     */
    bool get(int fd, const string& name, string* value, 
	     flags flags = PXATTR_NONE, nspace dom = PXATTR_USER);
    /**
     * Set the named attribute on path. 
     */
    bool set(const string& path, const string& name, const string& value, 
	     flags flags = PXATTR_NONE, nspace dom = PXATTR_USER);
    /**
     * Set the named attribute on open file. 
     */
    bool set(int fd, const string& name, const string& value, 
	     flags flags = PXATTR_NONE, nspace dom = PXATTR_USER);
    /**
     * Delete the named attribute from path.
     */
    bool del(const string& path, const string& name, 
	     flags flags = PXATTR_NONE, nspace dom = PXATTR_USER);
    /**
     * Delete the named attribute from open file.
     */
    bool del(int fd, const string& name, 
	     flags flags = PXATTR_NONE, nspace dom = PXATTR_USER);
    /**
     * List attribute names from path.
     */
    bool list(const string& path, vector<string>* names, 
	      flags flags = PXATTR_NONE, nspace dom = PXATTR_USER);
    /**
     * List attribute names from open file.
     */
    bool list(int fd, vector<string>* names, 
	      flags flags = PXATTR_NONE, nspace dom = PXATTR_USER);

    /**
     * Compute actual/system attribute name from external name 
     * (ie: myattr->user.myattr)
     */
    bool sysname(nspace dom, const string& pname, string* sname);
    /**
     * Compute external name from actual/system name 
     * (ie: user.myattr->myattr)
     */
    bool pxname(nspace dom, const string& sname, string* pname);
}


#endif /* _pxattr_h_included_ */
