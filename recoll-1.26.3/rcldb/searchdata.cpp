/* Copyright (C) 2006 J.F.Dockes
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

// Handle translation from rcl's SearchData structures to Xapian Queries

#include "autoconfig.h"

#include <stdio.h>

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iostream>
using namespace std;

#include "xapian.h"

#include "cstr.h"
#include "rcldb.h"
#include "rcldb_p.h"
#include "searchdata.h"
#include "log.h"
#include "smallut.h"
#include "textsplit.h"
#include "unacpp.h"
#include "utf8iter.h"
#include "stoplist.h"
#include "rclconfig.h"
#include "termproc.h"
#include "synfamily.h"
#include "stemdb.h"
#include "expansiondbs.h"
#include "base64.h"
#include "daterange.h"

namespace Rcl {

typedef  vector<SearchDataClause *>::iterator qlist_it_t;
typedef  vector<SearchDataClause *>::const_iterator qlist_cit_t;

void SearchData::commoninit()
{
    m_haveDates = false;
    m_maxSize = size_t(-1);
    m_minSize = size_t(-1);
    m_haveWildCards = false;
    m_autodiacsens = false;
    m_autocasesens = true;
    m_maxexp = 10000;
    m_maxcl = 100000;
    m_softmaxexpand = -1;
}

SearchData::~SearchData() 
{
    LOGDEB0("SearchData::~SearchData\n" );
    for (qlist_it_t it = m_query.begin(); it != m_query.end(); it++)
        delete *it;
}

// This is called by the GUI simple search if the option is set: add
// (OR) phrase to a query (if it is simple enough) so that results
// where the search terms are close and in order will come up on top.
// We remove very common terms from the query to avoid performance issues.
bool SearchData::maybeAddAutoPhrase(Rcl::Db& db, double freqThreshold)
{
    LOGDEB0("SearchData::maybeAddAutoPhrase()\n" );
    // cerr << "BEFORE SIMPLIFY\n"; dump(cerr);
    simplify();
    // cerr << "AFTER SIMPLIFY\n"; dump(cerr);

    if (!m_query.size()) {
        LOGDEB2("SearchData::maybeAddAutoPhrase: empty query\n" );
        return false;
    }

    string field;
    vector<string> words;
    // Walk the clause list. If this is not an AND list, we find any
    // non simple clause or different field names, bail out.
    for (qlist_it_t it = m_query.begin(); it != m_query.end(); it++) {
        SClType tp = (*it)->m_tp;
        if (tp != SCLT_AND) {
            LOGDEB2("SearchData::maybeAddAutoPhrase: wrong tp "  << (tp) << "\n" );
            return false;
        }
        SearchDataClauseSimple *clp = 
            dynamic_cast<SearchDataClauseSimple*>(*it);
        if (clp == 0) {
            LOGDEB2("SearchData::maybeAddAutoPhrase: dyncast failed\n" );
            return false;
        }
        if (it == m_query.begin()) {
            field = clp->getfield();
        } else {
            if (clp->getfield().compare(field)) {
                LOGDEB2("SearchData::maybeAddAutoPhrase: diff. fields\n" );
                return false;
            }
        }

        // If there are wildcards or quotes in there, bail out
        if (clp->gettext().find_first_of("\"*[?") != string::npos) { 
            LOGDEB2("SearchData::maybeAddAutoPhrase: wildcards\n" );
            return false;
        }

        // Do a simple word-split here, not the full-blown
        // textsplit. Spans of stopwords should not be trimmed later
        // in this function, they will be properly split when the
        // phrase gets processed by toNativeQuery() later on.
        vector<string> wl;
        stringToStrings(clp->gettext(), wl);
        words.insert(words.end(), wl.begin(), wl.end());
    }


    // Trim the word list by eliminating very frequent terms
    // (increasing the slack as we do it):
    int slack = 0;
    int doccnt = db.docCnt();
    if (!doccnt)
        doccnt = 1;
    string swords;
    for (vector<string>::iterator it = words.begin(); 
         it != words.end(); it++) {
        double freq = double(db.termDocCnt(*it)) / doccnt;
        if (freq < freqThreshold) {
            if (!swords.empty())
                swords.append(1, ' ');
            swords += *it;
        } else {
            LOGDEB0("SearchData::Autophrase: ["  << *it << "] too frequent ("
                    << (100 * freq) << " %" << ")\n" );
            slack++;
        }
    }
    
    // We can't make a phrase with a single word :)
    int nwords = TextSplit::countWords(swords);
    if (nwords <= 1) {
        LOGDEB2("SearchData::maybeAddAutoPhrase: ended with 1 word\n" );
        return false;
    }

    // Increase the slack: we want to be a little more laxist than for
    // an actual user-entered phrase
    slack += 1 + nwords / 3;
    
    m_autophrase = std::shared_ptr<SearchDataClauseDist>(
        new SearchDataClauseDist(SCLT_PHRASE, swords, slack, field));
    return true;
}

// Add clause to current list. OR lists cant have EXCL clauses.
bool SearchData::addClause(SearchDataClause* cl)
{
    if (m_tp == SCLT_OR && cl->getexclude()) {
        LOGERR("SearchData::addClause: cant add EXCL to OR list\n" );
        m_reason = "No Negative (AND_NOT) clauses allowed in OR queries";
        return false;
    }
    cl->setParent(this);
    m_haveWildCards = m_haveWildCards || cl->m_haveWildCards;
    m_query.push_back(cl);
    return true;
}

// Am I a file name only search ? This is to turn off term highlighting.
// There can't be a subclause in a filename search: no possible need to recurse
bool SearchData::fileNameOnly() 
{
    for (qlist_it_t it = m_query.begin(); it != m_query.end(); it++)
        if (!(*it)->isFileName())
            return false;
    return true;
}

// The query language creates a lot of subqueries. See if we can merge them.
void SearchData::simplify()
{
    for (unsigned int i = 0; i < m_query.size(); i++) {
        if (m_query[i]->m_tp != SCLT_SUB)
            continue;
        //C[est ce dyncast qui crashe??
        SearchDataClauseSub *clsubp = 
            dynamic_cast<SearchDataClauseSub*>(m_query[i]);
        if (clsubp == 0) {
            // ??
            continue;
        }
        if (clsubp->getSub()->m_tp != m_tp)
            continue;

        clsubp->getSub()->simplify();

        // If this subquery has special attributes, it's not a
        // candidate for collapsing, except if it has no clauses, because
        // then, we just pick the attributes.
        if (!clsubp->getSub()->m_filetypes.empty() || 
            !clsubp->getSub()->m_nfiletypes.empty() ||
            clsubp->getSub()->m_haveDates || 
            clsubp->getSub()->m_maxSize != size_t(-1) ||
            clsubp->getSub()->m_minSize != size_t(-1) ||
            clsubp->getSub()->m_haveWildCards) {
            if (!clsubp->getSub()->m_query.empty())
                continue;
            m_filetypes.insert(m_filetypes.end(),
                               clsubp->getSub()->m_filetypes.begin(),
                               clsubp->getSub()->m_filetypes.end());
            m_nfiletypes.insert(m_nfiletypes.end(),
                               clsubp->getSub()->m_nfiletypes.begin(),
                               clsubp->getSub()->m_nfiletypes.end());
            if (clsubp->getSub()->m_haveDates && !m_haveDates) {
                m_dates = clsubp->getSub()->m_dates;
            }
            if (m_maxSize == size_t(-1))
                m_maxSize = clsubp->getSub()->m_maxSize;
            if (m_minSize == size_t(-1))
                m_minSize = clsubp->getSub()->m_minSize;
            m_haveWildCards = m_haveWildCards ||
                clsubp->getSub()->m_haveWildCards;
            // And then let the clauses processing go on, there are
            // none anyway, we will just delete the subquery.
        }
        

        bool allsametp = true;
        for (qlist_it_t it1 = clsubp->getSub()->m_query.begin(); 
             it1 != clsubp->getSub()->m_query.end(); it1++) {
            // We want all AND or OR clause, and same as our conjunction
            if (((*it1)->getTp() != SCLT_AND && (*it1)->getTp() != SCLT_OR) || 
                (*it1)->getTp() != m_tp) {
                allsametp = false;
                break;
            }
        }
        if (!allsametp)
            continue;

        // All ok: delete the clause_sub, and insert the queries from
        // its searchdata in its place
        m_query.erase(m_query.begin() + i);
        m_query.insert(m_query.begin() + i, 
                       clsubp->getSub()->m_query.begin(),
                       clsubp->getSub()->m_query.end());
        for (unsigned int j = i; 
             j < i + clsubp->getSub()->m_query.size(); j++) {
            m_query[j]->setParent(this);
        }
        i += int(clsubp->getSub()->m_query.size()) - 1;

        // We don't want the clauses to be deleted when the parent is, as we
        // know own them.
        clsubp->getSub()->m_query.clear();
        delete clsubp;
    }
}

// Extract terms and groups for highlighting
void SearchData::getTerms(HighlightData &hld) const
{
    for (qlist_cit_t it = m_query.begin(); it != m_query.end(); it++) {
	if (!((*it)->getmodifiers() & SearchDataClause::SDCM_NOTERMS) &&
	    !(*it)->getexclude()) {
	    (*it)->getTerms(hld);
	}
    }
    return;
}

static const char * tpToString(SClType t)
{
    switch (t) {
    case SCLT_AND: return "AND";
    case SCLT_OR: return "OR";
    case SCLT_FILENAME: return "FILENAME";
    case SCLT_PHRASE: return "PHRASE";
    case SCLT_NEAR: return "NEAR";
    case SCLT_PATH: return "PATH";
    case SCLT_SUB: return "SUB";
    default: return "UNKNOWN";
    }
}

static string dumptabs;

void SearchData::dump(ostream& o) const
{
    o << dumptabs <<
        "SearchData: " << tpToString(m_tp) << " qs " << int(m_query.size()) << 
        " ft " << m_filetypes.size() << " nft " << m_nfiletypes.size() << 
        " hd " << m_haveDates << " maxs " << int(m_maxSize) << " mins " << 
        int(m_minSize) << " wc " << m_haveWildCards << "\n";
    for (std::vector<SearchDataClause*>::const_iterator it =
             m_query.begin(); it != m_query.end(); it++) {
        o << dumptabs;
        (*it)->dump(o);
        o << "\n";
    }
//    o << dumptabs << "\n";
}

void SearchDataClause::dump(ostream& o) const
{
    o << "SearchDataClause??";
}

void SearchDataClauseSimple::dump(ostream& o) const
{
    o << "ClauseSimple: " << tpToString(m_tp) << " ";
    if (m_exclude)
        o << "- ";
    o << "[" ;
    if (!m_field.empty())
        o << m_field << " : ";
    o << m_text << "]";
}

void SearchDataClauseFilename::dump(ostream& o) const
{
    o << "ClauseFN: ";
    if (m_exclude)
        o << " - ";
    o << "[" << m_text << "]";
}

void SearchDataClausePath::dump(ostream& o) const
{
    o << "ClausePath: ";
    if (m_exclude)
        o << " - ";
    o << "[" << m_text << "]";
}

void SearchDataClauseRange::dump(ostream& o) const
{
    o << "ClauseRange: ";
    if (m_exclude)
        o << " - ";
    o << "[" << gettext() << "]";
}

void SearchDataClauseDist::dump(ostream& o) const
{
    if (m_tp == SCLT_NEAR)
        o << "ClauseDist: NEAR ";
    else
        o << "ClauseDist: PHRA ";
            
    if (m_exclude)
        o << " - ";
    o << "[";
    if (!m_field.empty())
        o << m_field << " : ";
    o << m_text << "]";
}

void SearchDataClauseSub::dump(ostream& o) const
{
    o << "ClauseSub {\n";
    dumptabs += '\t';
    m_sub->dump(o);
    dumptabs.erase(dumptabs.size()- 1);
    o << dumptabs << "}";
}

} // Namespace Rcl

