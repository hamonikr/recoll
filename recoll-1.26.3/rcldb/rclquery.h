/* Copyright (C) 2008 J.F.Dockes
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
#ifndef _rclquery_h_included_
#define _rclquery_h_included_
#include <string>
#include <vector>

#include <memory>
#include "searchdata.h"

#ifndef NO_NAMESPACES
namespace Rcl {
#endif

class Db;
class Doc;

enum abstract_result {
    ABSRES_ERROR = 0,
    ABSRES_OK = 1,
    ABSRES_TRUNC = 2,
    ABSRES_TERMMISS = 4
};

// Snippet entry for makeDocAbstract
class Snippet {
public:
    Snippet(int page, const std::string& snip) 
	: page(page), snippet(snip)
    {
    }
    Snippet& setTerm(const std::string& trm)
    {
        term = trm;
        return *this;
    }
    int page;
    std::string term;
    std::string snippet;
};

        
/**
 * An Rcl::Query is a question (SearchData) applied to a
 * database. Handles access to the results. Somewhat equivalent to a
 * cursor in an rdb.
 *
 */
class Query {
public:
    Query(Db *db);
    ~Query();

    /** Get explanation about last error */
    std::string getReason() const {
        return m_reason;
    }

    /** Choose sort order. Must be called before setQuery */
    void setSortBy(const std::string& fld, bool ascending = true);
    const std::string& getSortBy() const {
        return m_sortField;
    }
    bool getSortAscending() const {
        return m_sortAscending;
    }

    /** Return or filter results with identical content checksum */
    void setCollapseDuplicates(bool on) {
        m_collapseDuplicates = on;
    }

    /** Accept data describing the search and query the index. This can
     * be called repeatedly on the same object which gets reinitialized each
     * time.
     */
    bool setQuery(std::shared_ptr<SearchData> q);


    /**  Get results count for current query.
     *
     * @param useestimate Use get_matches_estimated() if true, else 
     *     get_matches_lower_bound()
     * @param checkatleast checkatleast parameter to get_mset(). Use -1 for 
     *     full scan.
     */
    int getResCnt(int checkatleast=1000, bool useestimate=false);

    /** Get document at rank i in current query results. */
    bool getDoc(int i, Doc &doc, bool fetchtext = false);

    /** Get possibly expanded list of query terms */
    bool getQueryTerms(std::vector<std::string>& terms);

    /** Build synthetic abstract for document, extracting chunks relevant for
     * the input query. This uses index data only (no access to the file) */
    // Abstract returned as one string
    bool makeDocAbstract(const Doc &doc, std::string& abstract);
    // Returned as a snippets vector
    bool makeDocAbstract(const Doc &doc, std::vector<std::string>& abstract);
    // Returned as a vector of pair<page,snippet> page is 0 if unknown
    int makeDocAbstract(const Doc &doc, std::vector<Snippet>& abst, 
                        int maxoccs= -1, int ctxwords = -1, bool sortbypage=false);
    /** Retrieve page number for first match for "significant" query term 
     *  @param term returns the chosen term */
    int getFirstMatchPage(const Doc &doc, std::string& term);

    /** Retrieve a reference to the searchData we are using */
    std::shared_ptr<SearchData> getSD() {
        return m_sd;
    }

    /** Expand query to look for documents like the one passed in */
    std::vector<std::string> expand(const Doc &doc);

    /** Return the Db we're set for */
    Db *whatDb() const {
        return m_db;
    }

    /* make this public for access from embedded Db::Native */
    class Native;
    Native *m_nq;

private:
    std::string m_reason; // Error explanation
    Db    *m_db;
    void  *m_sorter;
    std::string m_sortField;
    bool   m_sortAscending;
    bool   m_collapseDuplicates;     
    int    m_resCnt;
    std::shared_ptr<SearchData> m_sd;
    int    m_snipMaxPosWalk;

    /* Copyconst and assignement private and forbidden */
    Query(const Query &) {}
    Query & operator=(const Query &) {return *this;};
};

#ifndef NO_NAMESPACES
}
#endif // NO_NAMESPACES


#endif /* _rclquery_h_included_ */
