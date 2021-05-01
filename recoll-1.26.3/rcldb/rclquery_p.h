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
#ifndef _rclquery_p_h_included_
#define _rclquery_p_h_included_

#include <map>
#include <vector>
#include <string>
#include <unordered_set>

#include <xapian.h>
#include "rclquery.h"

class Chrono;

namespace Rcl {

class Query::Native {
public:
    // The query I belong to
    Query *m_q;
    // query descriptor: terms and subqueries joined by operators
    // (or/and etc...)
    Xapian::Query xquery; 
    // Open query descriptor.
    Xapian::Enquire *xenquire;
    // Partial result set
    Xapian::MSet xmset;    
    // Term frequencies for current query. See makeAbstract, setQuery
    std::map<std::string, double>  termfreqs; 

    Native(Query *q)
        : m_q(q), xenquire(0) { }
    ~Native() {
        clear();
    }
    void clear() {
        delete xenquire; xenquire = 0;
        termfreqs.clear();
    }
    /** Return a list of terms which matched for a specific result document */
    bool getMatchTerms(unsigned long xdocid, std::vector<std::string>& terms);
    int makeAbstract(Xapian::docid id, std::vector<Snippet>&,
                     int maxoccs, int ctxwords, bool sortbypage);
    int getFirstMatchPage(Xapian::docid docid, std::string& term);
    void setDbWideQTermsFreqs();
    double qualityTerms(Xapian::docid docid, 
                        const std::vector<std::string>& terms,
                        std::multimap<double, std::vector<std::string> >& byQ);
    void abstractPopulateQTerm(
        Xapian::Database& xrdb,
        Xapian::docid docid,
        const string& qterm,
        int qtrmwrdcnt,
        int ctxwords,
        unsigned int maxgrpoccs,
        unsigned int maxtotaloccs,
        std::map<unsigned int, std::string>& sparseDoc,
        std::unordered_set<unsigned int>& searchTermPositions,
        unsigned int& maxpos,
        unsigned int& totaloccs,
        unsigned int& grpoccs,
        int& ret
        );
    void abstractPopulateContextTerms(
        Xapian::Database& xrdb,
        Xapian::docid docid,
        unsigned int maxpos,
        std::map<unsigned int, std::string>& sparseDoc,
        int& ret
        );
    void abstractCreateSnippetsVector(
        Db::Native *ndb,
        std::map<unsigned int, std::string>& sparseDoc,
        std::unordered_set<unsigned int>& searchTermPositions,
        std::vector<int>& vpbreaks,
        std::vector<Snippet>& vabs);
    int abstractFromIndex(
        Rcl::Db::Native *ndb,
        Xapian::docid docid,
        const std::vector<std::string>& matchTerms,
        const std::multimap<double, std::vector<std::string>> byQ,
        double totalweight,
        int ctxwords,
        unsigned int maxtotaloccs,
        std::vector<Snippet>& vabs,
        Chrono& chron
        );
    int abstractFromText(
        Rcl::Db::Native *ndb,
        Xapian::docid docid,
        const std::vector<std::string>& matchTerms,
        const std::multimap<double, std::vector<std::string>> byQ,
        double totalweight,
        int ctxwords,
        unsigned int maxtotaloccs,
        vector<Snippet>& vabs,
        Chrono& chron,
        bool sortbypage
        );
};

}
#endif /* _rclquery_p_h_included_ */
