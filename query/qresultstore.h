/* Copyright (C) 2017-2020 J.F.Dockes
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
#ifndef _QRESULTSTORE_H_INCLUDED_
#define _QRESULTSTORE_H_INCLUDED_

/**
 * Implement an efficient way to store the whole or part of a query result set.
 * This would naturally be done as a vector<Rcl::Doc>, but the natural
 * way leads to a huge space waste (8-10x), which may be a problem in
 * some cases. This is mostly used by the uprcl Media Server.
 */

#include <string>
#include <set>

namespace Rcl {
class Query;

class QResultStore {
public:
    QResultStore();
    ~QResultStore();

    /**
     * Fetch and store the results of the input query.
     *
     * @param q the executed query object to use for fetching results.
     * @param fldspec list of fields to be excluded or included.
     * @param isinc if true, the field list defines the fields to be stored, 
     *   else, those to be excluded.
     */
    bool storeQuery(Rcl::Query& q, std::set<std::string> fldspec = {},
                    bool isinc = false);

    /**  Retrieve count of stored results */
    int getCount();

    /**
     * Retrieve field value.
     *
     * @param docindex index in query results.
     * @param fldname field name.
     */
    const char *fieldValue(int docindex, const std::string& fldname);

    
    QResultStore(const QResultStore&) = delete;
    QResultStore& operator=(const QResultStore&) = delete;
    class Internal;
private:
    Internal *m{nullptr};
};

}
#endif /* _QRESULTSTORE_H_INCLUDED_ */
