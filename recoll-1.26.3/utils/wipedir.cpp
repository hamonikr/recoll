/* Copyright (C) 2004 J.F.Dockes
 *
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

#ifndef TEST_WIPEDIR
#include "autoconfig.h"

#include <errno.h>
#include "safefcntl.h"
#include <sys/types.h>
#include "safesysstat.h"
#include "safeunistd.h"
#include <dirent.h>

#include <cstring>
#include <string>

#include "log.h"
#include "pathut.h"
#include "wipedir.h"

using namespace std;

int wipedir(const string& dir, bool selfalso, bool recurse)
{
    struct stat st;
    int statret;
    int ret = -1;

    statret = lstat(dir.c_str(), &st);
    if (statret == -1) {
	LOGERR("wipedir: cant stat "  << (dir) << ", errno "  << (errno) << "\n" );
	return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
	LOGERR("wipedir: "  << (dir) << " not a directory\n" );
	return -1;
    }

    if (access(dir.c_str(), R_OK|W_OK|X_OK) < 0) {
	LOGERR("wipedir: no write access to "  << (dir) << "\n" );
	return -1;
    }

    DIR *d = opendir(dir.c_str());
    if (d == 0) {
	LOGERR("wipedir: cant opendir "  << (dir) << ", errno "  << (errno) << "\n" );
	return -1;
    }
    int remaining = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != 0) {
	if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) 
	    continue;

	string fn = path_cat(dir, ent->d_name);

	struct stat st;
	int statret = lstat(fn.c_str(), &st);
	if (statret == -1) {
	    LOGERR("wipedir: cant stat "  << (fn) << ", errno "  << (errno) << "\n" );
	    goto out;
	}
	if (S_ISDIR(st.st_mode)) {
	    if (recurse) {
		int rr = wipedir(fn, true, true);
		if (rr == -1) 
		    goto out;
		else 
		    remaining += rr;
	    } else {
		remaining++;
	    }
	} else {
	    if (unlink(fn.c_str()) < 0) {
		LOGERR("wipedir: cant unlink "  << (fn) << ", errno "  << (errno) << "\n" );
		goto out;
	    }
	}
    }

    ret = remaining;
    if (selfalso && ret == 0) {
	if (rmdir(dir.c_str()) < 0) {
	    LOGERR("wipedir: rmdir("  << (dir) << ") failed, errno "  << (errno) << "\n" );
	    ret = -1;
	}
    }

 out:
    if (d)
	closedir(d);
    return ret;
}


#else // FILEUT_TEST

#include <stdio.h>
#include <stdlib.h>

#include "wipedir.h"

using namespace std;
static const char *thisprog;

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_r	  0x2 
#define OPT_s	  0x4 
static char usage [] =
"wipedir [-r -s] topdir\n"
" -r : recurse\n"
" -s : also delete topdir\n"
;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}
int main(int argc, const char **argv)
{
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    /* Cas du "adb - core" */
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'r':	op_flags |= OPT_r; break;
	    case 's':	op_flags |= OPT_s; break;
	    default: Usage();	break;
	    }
    b1: argc--; argv++;
    }

    if (argc != 1)
	Usage();

    string dir = *argv++;argc--;

    bool topalso = ((op_flags&OPT_s) != 0);
    bool recurse = ((op_flags&OPT_r) != 0);
    int cnt = wipedir(dir, topalso, recurse);
    printf("wipedir returned %d\n", cnt);
    exit(0);
}

#endif

