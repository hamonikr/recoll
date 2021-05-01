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
#ifndef TEST_IDFILE
#include "autoconfig.h"

#include <stdlib.h>
#include <ctype.h>
#include <cstring>

#include <fstream>
#include <sstream>

#include "idfile.h"
#include "log.h"

using namespace std;

// Bogus code to avoid bogus valgrind mt warnings about the
// initialization of treat_mbox_...  which I can't even remember the
// use of (it's not documented or ever set)
static int treat_mbox_as_rfc822;
class InitTMAR {
public:
    InitTMAR() {
        treat_mbox_as_rfc822 = getenv("RECOLL_TREAT_MBOX_AS_RFC822") ? 1 : -1;
    }
};
static InitTMAR initTM;

/** 
 * This code is currently ONLY used to identify mbox and mail message files
 * which are badly handled by standard mime type identifiers
 * There is a very old (circa 1990) mbox format using blocks of ^A (0x01) chars
 * to separate messages, that we don't recognize currently
 */

// Mail headers we compare to:
static const char *mailhs[] = {"From: ", "Received: ", "Message-Id: ", "To: ", 
			       "Date: ", "Subject: ", "Status: ", 
			       "In-Reply-To: "};
static const int mailhsl[] = {6, 10, 12, 4, 6, 9, 8, 13};
static const int nmh = sizeof(mailhs) / sizeof(char *);

const int wantnhead = 3;

// fn is for message printing
static string idFileInternal(istream& input, const char *fn)
{
    bool line1HasFrom = false;
    bool gotnonempty = false;
    int lookslikemail = 0;

    // emacs VM sometimes inserts very long lines with continuations or
    // not (for folder information). This forces us to look at many
    // lines and long ones
    int lnum = 1;
    for (int loop = 1; loop < 200; loop++, lnum++) {

#define LL 2*1024
	char cline[LL+1];
	cline[LL] = 0;
	input.getline(cline, LL-1);
	if (input.fail()) {
	    if (input.bad()) {
		LOGERR("idfile: error while reading ["  << (fn) << "]\n" );
		return string();
	    }
	    // Must be eof ?
	    break;
	}

	// gcount includes the \n
	std::streamsize ll = input.gcount() - 1; 
	if (ll > 0)
	    gotnonempty = true;

	LOGDEB2("idfile: lnum "  << (lnum) << " ll "  << ((unsigned int)ll) << ": ["  << (cline) << "]\n" );

	// Check for a few things that can't be found in a mail file,
	// (optimization to get a quick negative)

	// Empty lines
	if (ll <= 0) {
	    // Accept a few empty lines at the beginning of the file,
	    // otherwise this is the end of headers
	    if (gotnonempty || lnum > 10) {
		LOGDEB2("Got empty line\n" );
		break;
	    } else {
		// Don't increment the line counter for initial empty lines.
		lnum--;
		continue;
	    }
	}

	// emacs vm can insert VERY long header lines.
	if (ll > LL - 20) {
	    LOGDEB2("idFile: Line too long\n" );
	    return string();
	}

	// Check for mbox 'From ' line
	if (lnum == 1 && !strncmp("From ", cline, 5)) {
	    if (treat_mbox_as_rfc822 == -1) {
		line1HasFrom = true;
		LOGDEB2("idfile: line 1 has From_\n" );
	    }
	    continue;
	} 

	// Except for a possible first line with 'From ', lines must
	// begin with whitespace or have a colon 
	// (hope no one comes up with a longer header name !
	// Take care to convert to unsigned char because ms ctype does
	// like negative values
	if (!isspace((unsigned char)cline[0])) {
	    char *cp = strchr(cline, ':');
	    if (cp == 0 || (cp - cline) > 70) {
		LOGDEB2("idfile: can't be mail header line: ["  << (cline) << "]\n" );
		break;
	    }
	}

	// Compare to known headers
	for (int i = 0; i < nmh; i++) {
	    if (!strncasecmp(mailhs[i], cline, mailhsl[i])) {
		//fprintf(stderr, "Got [%s]\n", mailhs[i]);
		lookslikemail++;
		break;
	    }
	}
	if (lookslikemail >= wantnhead)
	    break;
    }
    if (line1HasFrom)
	lookslikemail++;

    if (lookslikemail >= wantnhead)
	return line1HasFrom ? string("text/x-mail") : string("message/rfc822");

    return string();
}

string idFile(const char *fn)
{
    ifstream input;
    input.open(fn, ios::in);
    if (!input.is_open()) {
	LOGERR("idFile: could not open ["  << (fn) << "]\n" );
	return string();
    }
    return idFileInternal(input, fn);
}

string idFileMem(const string& data)
{
    stringstream s(data, stringstream::in);
    return idFileInternal(s, "");
}

#else

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <iostream>

#include <fcntl.h>

using namespace std;

#include "log.h"

#include "idfile.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
	cerr << "Usage: idfile filename" << endl;
	exit(1);
    }
    DebugLog::getdbl()->setloglevel(DEBDEB1);
    DebugLog::setfilename("stderr");
    for (int i = 1; i < argc; i++) {
	string mime = idFile(argv[i]);
	cout << argv[i] << " : " << mime << endl;
    }
    exit(0);
}

#endif

