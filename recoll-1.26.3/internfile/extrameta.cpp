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

#include <errno.h>

#include "rclconfig.h"
#include "pxattr.h"
#include "log.h"
#include "cstr.h"
#include "rcldoc.h"
#include "execmd.h"

using std::string;
using std::map;

static void docfieldfrommeta(RclConfig* cfg, const string& name, 
			     const string &value, Rcl::Doc& doc)
{
    string fieldname = cfg->fieldCanon(name);
    LOGDEB0("Internfile:: setting [" << fieldname <<
            "] from cmd/xattr value [" << value << "]\n");
    if (fieldname == cstr_dj_keymd) {
	doc.dmtime = value;
    } else {
	doc.meta[fieldname] = value;
    }
}

void reapXAttrs(const RclConfig* cfg, const string& path, 
		map<string, string>& xfields)
{
    LOGDEB2("reapXAttrs: [" << path << "]\n");
#ifndef _WIN32
    // Retrieve xattrs names from files and mapping table from config
    vector<string> xnames;
    if (!pxattr::list(path, &xnames)) {
        if (errno == ENOTSUP) {
            LOGDEB("FileInterner::reapXattrs: pxattr::list: errno " <<
                   errno << "\n");
        } else {
            LOGERR("FileInterner::reapXattrs: pxattr::list: errno " <<
                   errno << "\n");
        }
	return;
    }
    const map<string, string>& xtof = cfg->getXattrToField();

    // Record the xattrs: names found in the config are either skipped
    // or mapped depending if the translation is empty. Other names
    // are recorded as-is
    for (vector<string>::const_iterator it = xnames.begin();
	 it != xnames.end(); it++) {
	string key = *it;
	map<string, string>::const_iterator mit = xtof.find(*it);
	if (mit != xtof.end()) {
	    if (mit->second.empty()) {
		continue;
	    } else {
		key = mit->second;
	    }
	}
	string value;
	if (!pxattr::get(path, *it, &value, pxattr::PXATTR_NOFOLLOW)) {
	    LOGERR("FileInterner::reapXattrs: pxattr::get failed for " << *it
                   << ", errno " << errno << "\n");
	    continue;
	}
	// Encode should we ?
	xfields[key] = value;
	LOGDEB2("reapXAttrs: [" << key << "] -> [" << value << "]\n");
    }
#endif
}

void docFieldsFromXattrs(RclConfig *cfg, const map<string, string>& xfields, 
			 Rcl::Doc& doc)
{
    for (map<string,string>::const_iterator it = xfields.begin(); 
	 it != xfields.end(); it++) {
	docfieldfrommeta(cfg, it->first, it->second, doc);
    }
}

void reapMetaCmds(RclConfig* cfg, const string& path, 
		  map<string, string>& cfields)
{
    const vector<MDReaper>& reapers = cfg->getMDReapers();
    if (reapers.empty())
	return;
    map<char,string> smap = {{'f', path}};
    for (vector<MDReaper>::const_iterator rp = reapers.begin();
	 rp != reapers.end(); rp++) {
	vector<string> cmd;
	for (vector<string>::const_iterator it = rp->cmdv.begin();
	     it != rp->cmdv.end(); it++) {
	    string s;
	    pcSubst(*it, s, smap);
	    cmd.push_back(s);
	}
	string output;
	if (ExecCmd::backtick(cmd, output)) {
	    cfields[rp->fieldname] =  output;
	}
    }
}

// Set fields from external commands
// These override those from xattrs and can be later augmented by
// values from inside the file.
//
// This is a bit atrocious because some entry names are special:
// "modificationdate" will set mtime instead of an ordinary field,
// and the output from anything beginning with "rclmulti" will be
// interpreted as multiple fields in configuration file format...
void docFieldsFromMetaCmds(RclConfig *cfg, const map<string, string>& cfields, 
			   Rcl::Doc& doc)
{
    for (map<string,string>::const_iterator it = cfields.begin(); 
	 it != cfields.end(); it++) {
	if (!it->first.compare(0, 8, "rclmulti")) {
	    ConfSimple simple(it->second);
	    if (simple.ok()) {
		vector<string> names = simple.getNames("");
		for (vector<string>::const_iterator nm = names.begin(); 
		     nm != names.end(); nm++) {
		    string value;
		    if (simple.get(*nm, value)) {
			docfieldfrommeta(cfg, *nm, value, doc);
		    }
		}
	    }
	} else {
	    docfieldfrommeta(cfg, it->first, it->second, doc);
	}
    }
}

