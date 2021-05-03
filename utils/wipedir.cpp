/* Copyright (C) 2004-2019 J.F.Dockes
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
#include "autoconfig.h"

#include "wipedir.h"

#include <string>

#include "log.h"
#include "pathut.h"

#ifdef _WIN32
#  include "safeunistd.h"
#else // Not windows ->
#  include <unistd.h>
#endif


int wipedir(const std::string& dir, bool selfalso, bool recurse)
{
    int ret = -1;

    if (!path_isdir(dir)) {
        LOGERR("wipedir: " << dir << " not a directory\n");
        return -1;
    }

    if (!path_access(dir, R_OK|W_OK|X_OK)) {
        LOGSYSERR("wipedir", "access", dir);
        return -1;
    }

    PathDirContents dc(dir);
    if (!dc.opendir()) {
        LOGSYSERR("wipedir", "opendir", dir);
        return -1;
    }
    int remaining = 0;
    const struct PathDirContents::Entry *ent;
    while ((ent = dc.readdir()) != 0) {
        const std::string& dname{ent->d_name};
        if (dname == "." || dname == "..")
            continue;

        std::string fn = path_cat(dir, dname);

        if (path_isdir(fn)) {
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
            if (!path_unlink(fn)) {
                LOGSYSERR("wipedir", "unlink", fn);
                goto out;
            }
        }
    }

    ret = remaining;
    if (selfalso && ret == 0) {
        if (!path_rmdir(dir)) {
            LOGSYSERR("wipedir", "rmdir", dir);
            ret = -1;
        }
    }

out:
    return ret;
}
