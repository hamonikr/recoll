/* Copyright (C) 2014 J.F.Dockes
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

#include "checkretryfailed.h"

#include "safesysstat.h"

#include <string>
#include <vector>

#include "rclconfig.h"
#include "execmd.h"
#include "log.h"
#include "pathut.h"
#include "recollindex.h"

using namespace std;

bool checkRetryFailed(RclConfig *conf, bool record)
{
#ifdef _WIN32
    // Under Windows we only retry if the recollindex program is newer
    // than the index
    struct stat st;
    string path(thisprog);
    if (path_suffix(path).empty()) {
        path = path + ".exe";
    }
    if (path_fileprops(path, &st) != 0) {
        LOGERR("checkRetryFailed: can't stat the program file: " <<
               thisprog << endl);
        return false;
    }
    time_t exetime = st.st_mtime;
    if (path_fileprops(conf->getDbDir(), &st) != 0) {
        // Maybe it just does not exist.
        LOGDEB("checkRetryFailed: can't stat the index directory: " <<
               conf->getDbDir() << endl);
        return false;
    }
    time_t dbtime = st.st_mtime;
    return exetime > dbtime;
#else
    string cmd;

    if (!conf->getConfParam("checkneedretryindexscript", cmd)) {
        LOGDEB("checkRetryFailed: 'checkneedretryindexscript' "
               "not set in config\n");
        // We could toss a dice ? Say no retry in this case.
        return false;
    }

    // Look in the filters directory (ies). If not found execpath will
    // be the same as cmd, and we'll let execvp do its thing.
    string execpath = conf->findFilter(cmd);

    vector<string> args;
    if (record) {
        args.push_back("1");
    }
    ExecCmd ecmd;
    int status = ecmd.doexec(execpath, args);
    if (status == 0) {
        return true;
    } 
    return false;
#endif
}

