/* Copyright (C) 2007 J.F.Dockes
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

#ifndef TEST_STOPLIST
#include "log.h"
#include "readfile.h"
#include "unacpp.h"
#include "smallut.h"
#include "stoplist.h"

#ifndef NO_NAMESPACES
namespace Rcl 
{
#endif

bool StopList::setFile(const string &filename)
{
    m_stops.clear();
    string stoptext, reason;
    if (!file_to_string(filename, stoptext, &reason)) {
	LOGDEB0("StopList::StopList: file_to_string("  << (filename) << ") failed: "  << (reason) << "\n" );
	return false;
    }
    set<string> stops;
    stringToStrings(stoptext, stops);
    for (set<string>::iterator it = stops.begin(); 
	 it != stops.end(); it++) {
	string dterm;
	unacmaybefold(*it, dterm, "UTF-8", UNACOP_UNACFOLD);
	m_stops.insert(dterm);
    }

    return true;
}

// Most sites will have an empty stop list. We try to optimize the
// empty set case as much as possible. empty() is probably sligtly faster than
// find() in this case.
bool StopList::isStop(const string &term) const
{
    return m_stops.empty() ? false : m_stops.find(term) != m_stops.end();
}


#ifndef NO_NAMESPACES
}
#endif

#else // TEST_STOPLIST

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <string>
#include <iostream>

#include "stoplist.h"

using namespace std;
using namespace Rcl;

static char *thisprog;

static char usage [] =
"trstoplist stopstermsfile\n\n"
;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

const string tstwords[] = {
    "the", "is", "xweird", "autre", "autre double", "mot1", "mot double",
};
const int tstsz = sizeof(tstwords) / sizeof(string);

int main(int argc, char **argv)
{
  int count = 10;
    
  thisprog = argv[0];
  argc--; argv++;

  if (argc != 1)
    Usage();
  string filename = argv[0]; argc--;

  StopList sl(filename);

  for (int i = 0; i < tstsz; i++) {
      const string &tst = tstwords[i];
      cout << "[" << tst << "] " << 
	  (sl.isStop(tst) ? "in stop list" : "not in stop list") << endl;
  }
  exit(0);
}

#endif // TEST_STOPLIST

