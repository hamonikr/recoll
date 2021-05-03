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

#include "autoconfig.h"

#include <memory>

#include "cstr.h"
#include "rcldb.h"
#include "searchdata.h"
#include "rclquery.h"
#include "subtreelist.h"
#include "log.h"

bool subtreelist(RclConfig *config, const string& top, 
                 vector<string>& paths)
{
    LOGDEB("subtreelist: top: ["  << (top) << "]\n" );
    Rcl::Db rcldb(config);
    if (!rcldb.open(Rcl::Db::DbRO)) {
        LOGERR("subtreelist: can't open database in [" << config->getDbDir() <<
               "]: " << rcldb.getReason() << "\n");
        return false;
    }

    Rcl::SearchData *sd = new Rcl::SearchData(Rcl::SCLT_OR, cstr_null);
    std::shared_ptr<Rcl::SearchData> rq(sd);

    sd->addClause(new Rcl::SearchDataClausePath(top, false));

    Rcl::Query query(&rcldb);
    query.setQuery(rq);
    int cnt = query.getResCnt();

    for (int i = 0; i < cnt; i++) {
        Rcl::Doc doc;
        if (!query.getDoc(i, doc))
            break;
        string path = fileurltolocalpath(doc.url);
        if (!path.empty())
            paths.push_back(path);
    }
    return true;
}
