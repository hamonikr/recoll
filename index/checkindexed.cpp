/* Copyright (C) 2021 J.F.Dockes
 *
 * License: GPL 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "autoconfig.h"

#include "checkindexed.h"

#include <stdio.h>
#include <iostream>

#include "rclconfig.h"
#include "fileudi.h"
#include "rcldb.h"
#include "rcldoc.h"
#include "smallut.h"

class PathYielder {
public:
    PathYielder(const std::vector<std::string>& paths)
        : m_paths(paths) {
        if (m_paths.size()) {
            m_index = 0;
        }
    }
    std::string getPath() {
        if (m_index >= 0) {
            if (m_index < int(m_paths.size())) {
                return m_paths[m_index++];
            }
        } else {
            char line[1024];
            if (fgets(line, 1023, stdin)) {
                std::string sl(line);
                trimstring(sl, "\n\r");
                return sl;
            }
        }
        return std::string();
    }
    int m_index{-1};
    const std::vector<std::string>& m_paths;
};

bool checkindexed(RclConfig *conf, const std::vector<std::string>& filepaths)
{
    PathYielder paths(filepaths);
    Rcl::Db db(conf);
    if (!db.open(Rcl::Db::DbRO)) {
        std::cerr << "Could not open index for reading\n";
        return false;
    }
    for (;;) {
        auto path = paths.getPath();
        if (path.empty()) {
            break;
        }
        std::string udi;
        make_udi(path, std::string(), udi);
        Rcl::Doc doc;
        if (!db.getDoc(udi, "", doc)) {
            std::cerr << "Unexpected error from getdoc\n";
            return false;
        }
        // See comments in getdoc
        if (doc.pc == -1) {
            std::cout << "ABSENT " << path << std::endl;
        } else {
            std::string sig;
            if (!doc.getmeta(Rcl::Doc::keysig, &sig) ||
                sig.back() == '+') {
                std::cout << "ERROR " << path << std::endl;
            }
        }
    }
    return true;
}
