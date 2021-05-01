/* Copyright (C) 2005 J.F.Dockes
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

#ifndef TEST_HISTORY
#include "safeunistd.h"

#include "dynconf.h"
#include "base64.h"
#include "smallut.h"
#include "log.h"

using namespace std;

// Well known keys for history and external indexes.
const string docHistSubKey = "docs";
const string allEdbsSk = "allExtDbs";
const string actEdbsSk = "actExtDbs";
const string advSearchHistSk = "advSearchHist";

RclDynConf::RclDynConf(const std::string &fn)
    : m_data(fn.c_str())
{
    if (m_data.getStatus() != ConfSimple::STATUS_RW) {
        // Maybe the config dir is readonly, in which case we try to
        // open readonly, but we must also handle the case where the
        // history file does not exist
        if (access(fn.c_str(), 0) != 0) {
            m_data = ConfSimple(string(), 1);
        } else {
            m_data = ConfSimple(fn.c_str(), 1);
        }
    }
}

bool RclDynConf::insertNew(const string &sk, DynConfEntry &n, DynConfEntry &s,
			   int maxlen)
{
    if (!rw()) {
        LOGDEB("RclDynConf::insertNew: not writable\n");
        return false;
    }
    // Is this doc already in list ? If it is we remove the old entry
    vector<string> names = m_data.getNames(sk);
    vector<string>::const_iterator it;
    bool changed = false;
    for (it = names.begin(); it != names.end(); it++) {
	string oval;
	if (!m_data.get(*it, oval, sk)) {
	    LOGDEB("No data for " << *it << "\n");
	    continue;
	}
	s.decode(oval);

	if (s.equal(n)) {
	    LOGDEB("Erasing old entry\n");
	    m_data.erase(*it, sk);
	    changed = true;
	}
    }

    // Maybe reget things
    if (changed)
	names = m_data.getNames(sk);

    // Need to prune ?
    if (maxlen > 0 && names.size() >= (unsigned int)maxlen) {
	// Need to erase entries until we're back to size. Note that
	// we don't ever reset numbers. Problems will arise when
	// history is 4 billion entries old
	it = names.begin();
	for (unsigned int i = 0; i < names.size() - maxlen + 1; i++, it++) {
	    m_data.erase(*it, sk);
	}
    }

    // Increment highest number
    unsigned int hi = names.empty() ? 0 : 
	(unsigned int)atoi(names.back().c_str());
    hi++;
    char nname[20];
    sprintf(nname, "%010u", hi);

    string value;
    n.encode(value);
    LOGDEB1("Encoded value [" << value << "] (" << value.size() << ")\n");
    if (!m_data.set(string(nname), value, sk)) {
	LOGERR("RclDynConf::insertNew: set failed\n");
	return false;
    }
    return true;
}

bool RclDynConf::eraseAll(const string &sk)
{
    if (!rw()) {
        LOGDEB("RclDynConf::eraseAll: not writable\n");
        return false;
    }
    for (const auto& nm : m_data.getNames(sk)) {
	m_data.erase(nm, sk);
    }
    return true;
}

// Specialization for plain strings ///////////////////////////////////

bool RclDynConf::enterString(const string sk, const string value, int maxlen)
{
    if (!rw()) {
        LOGDEB("RclDynConf::enterString: not writable\n");
        return false;
    }
    RclSListEntry ne(value);
    RclSListEntry scratch;
    return insertNew(sk, ne, scratch, maxlen);
}

#else

#include <string>
#include <iostream>

#include "history.h"
#include "log.h"


#ifndef NO_NAMESPACES
using namespace std;
#endif

static string thisprog;

static string usage =
    "trhist [opts] <filename>\n"
    " [-s <subkey>]: specify subkey (default: RclDynConf::docHistSubKey)\n"
    " [-e] : erase all\n"
    " [-a <string>] enter string (needs -s, no good for history entries\n"
    "\n"
    ;

static void
Usage(void)
{
    cerr << thisprog  << ": usage:\n" << usage;
    exit(1);
}

static int        op_flags;
#define OPT_e     0x2
#define OPT_s     0x4
#define OPT_a     0x8

int main(int argc, char **argv)
{
    string sk = "docs";
    string value;

    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    /* Cas du "adb - core" */
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'a':	op_flags |= OPT_a; if (argc < 2)  Usage();
		value = *(++argv); argc--; 
		goto b1;
	    case 's':	op_flags |= OPT_s; if (argc < 2)  Usage();
		sk = *(++argv);	argc--; 
		goto b1;
	    case 'e':	op_flags |= OPT_e; break;
	    default: Usage();	break;
	    }
    b1: argc--; argv++;
    }
    if (argc != 1)
	Usage();
    string filename = *argv++;argc--;

    RclDynConf hist(filename, 5);
    DebugLog::getdbl()->setloglevel(DEBDEB1);
    DebugLog::setfilename("stderr");

    if (op_flags & OPT_e) {
	hist.eraseAll(sk);
    } else if (op_flags & OPT_a) {
	if (!(op_flags & OPT_s)) 
	    Usage();
	hist.enterString(sk, value);
    } else {
	for (int i = 0; i < 10; i++) {
	    char docname[200];
	    sprintf(docname, "A very long document document name"
		    "is very long indeed and this is the end of "
		    "it here and exactly here:\n%d",	i);
	    hist.enterDoc(string(docname), "ipathx");
	}

	list<RclDHistoryEntry> hlist = hist.getDocHistory();
	for (list<RclDHistoryEntry>::const_iterator it = hlist.begin();
	     it != hlist.end(); it++) {
	    printf("[%ld] [%s] [%s]\n", it->unixtime, 
		   it->fn.c_str(), it->ipath.c_str());
	}
    }
}

#endif

