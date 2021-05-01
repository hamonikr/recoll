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
#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <vector>
#include <sstream>

#include "xapian.h"

#include "cstr.h"
#include "rclconfig.h"
#include "log.h"
#include "rcldb.h"
#include "rcldb_p.h"
#include "rclquery.h"
#include "rclquery_p.h"
#include "conftree.h"
#include "smallut.h"
#include "chrono.h"
#include "searchdata.h"
#include "unacpp.h"

using namespace std;

namespace Rcl {
// This is used as a marker inside the abstract frag lists, but
// normally doesn't remain in final output (which is built with a
// custom sep. by our caller).
static const string cstr_ellipsis("...");

// Field names inside the index data record may differ from the rcldoc ones
// (esp.: caption / title)
static const string& docfToDatf(const string& df)
{
    if (!df.compare(Doc::keytt)) {
        return cstr_caption;
    } else if (!df.compare(Doc::keymt)) {
        return cstr_dmtime;
    } else {
        return df;
    }
}

// Sort helper class. As Xapian sorting is lexicographic, we do some
// special processing for special fields like dates and sizes. User
// custom field data will have to be processed before insertion to
// achieve equivalent results.
#if XAPIAN_MAJOR_VERSION == 1 && XAPIAN_MINOR_VERSION < 2
class QSorter : public Xapian::Sorter
#else
class QSorter : public Xapian::KeyMaker
#endif
{
public:
    QSorter(const string& f) 
        : m_fld(docfToDatf(f) + "=") {
        m_ismtime = !m_fld.compare("dmtime=");
        if (m_ismtime)
            m_issize = false;
        else 
            m_issize = !m_fld.compare("fbytes=") || !m_fld.compare("dbytes=") ||
                !m_fld.compare("pcbytes=");
    }

    virtual std::string operator()(const Xapian::Document& xdoc) const {
        string data = xdoc.get_data();
        // It would be simpler to do the record->Rcl::Doc thing, but
        // hand-doing this will be faster. It makes more assumptions
        // about the format than a ConfTree though:
        string::size_type i1, i2;
        i1 = data.find(m_fld);
        if (i1 == string::npos) {
            if (m_ismtime) {
                // Ugly: specialcase mtime as it's either dmtime or fmtime
                i1 = data.find("fmtime=");
                if (i1 == string::npos) {
                    return string();
                }
            } else {
                return string();
            }
        }
        i1 += m_fld.length();
        if (i1 >= data.length())
            return string();
        i2 = data.find_first_of("\n\r", i1);
        if (i2 == string::npos)
            return string();

        string term = data.substr(i1, i2-i1);
        if (m_ismtime) {
            return term;
        } else if (m_issize) {
            // Left zeropad values for appropriate numeric sorting
            leftzeropad(term, 12);
            return term;
        }

        // Process data for better sorting. We should actually do the
        // unicode thing
        // (http://unicode.org/reports/tr10/#Introduction), but just
        // removing accents and majuscules will remove the most
        // glaring weirdnesses (or not, depending on your national
        // approach to collating...)
        string sortterm;
        // We're not even sure the term is utf8 here (ie: url)
        if (!unacmaybefold(term, sortterm, "UTF-8", UNACOP_UNACFOLD)) {
            sortterm = term;
        }
        // Also remove some common uninteresting starting characters
        i1 = sortterm.find_first_not_of(" \t\\\"'([*+,.#/");
        if (i1 != 0 && i1 != string::npos) {
            sortterm = sortterm.substr(i1, sortterm.size()-i1);
        }

        LOGDEB2("QSorter: [" << term << "] -> [" << sortterm << "]\n");
        return sortterm;
    }

private:
    string m_fld;
    bool   m_ismtime;
    bool   m_issize;
};

Query::Query(Db *db)
    : m_nq(new Native(this)), m_db(db), m_sorter(0), m_sortAscending(true),
      m_collapseDuplicates(false), m_resCnt(-1), m_snipMaxPosWalk(1000000)
{
    if (db)
        db->getConf()->getConfParam("snippetMaxPosWalk", &m_snipMaxPosWalk);
}

Query::~Query()
{
    deleteZ(m_nq);
    if (m_sorter) {
        delete (QSorter*)m_sorter;
        m_sorter = 0;
    }
}

void Query::setSortBy(const string& fld, bool ascending) {
    if (fld.empty()) {
        m_sortField.erase();
    } else {
        m_sortField = m_db->getConf()->fieldQCanon(fld);
        m_sortAscending = ascending;
    }
    LOGDEB0("RclQuery::setSortBy: [" << m_sortField << "] " <<
            (m_sortAscending ? "ascending" : "descending") << "\n");
}

// Prepare query out of user search data
bool Query::setQuery(std::shared_ptr<SearchData> sdata)
{
    LOGDEB("Query::setQuery:\n");

    if (!m_db || !m_nq) {
        LOGERR("Query::setQuery: not initialised!\n");
        return false;
    }
    m_resCnt = -1;
    m_reason.erase();

    m_nq->clear();
    m_sd = sdata;
    
    Xapian::Query xq;
    if (!sdata->toNativeQuery(*m_db, &xq)) {
        m_reason += sdata->getReason();
        return false;
    }

    m_nq->xquery = xq;

    string d;
    for (int tries = 0; tries < 2; tries++) {
        try {
            m_nq->xenquire = new Xapian::Enquire(m_db->m_ndb->xrdb);
            if (m_collapseDuplicates) {
                m_nq->xenquire->set_collapse_key(Rcl::VALUE_MD5);
            } else {
                m_nq->xenquire->set_collapse_key(Xapian::BAD_VALUENO);
            }
            m_nq->xenquire->set_docid_order(Xapian::Enquire::DONT_CARE);
            if (!m_sortField.empty() && 
                stringlowercmp("relevancyrating", m_sortField)) {
                if (m_sorter) {
                    delete (QSorter*)m_sorter;
                    m_sorter = 0;
                }
                m_sorter = new QSorter(m_sortField);
                // It really seems there is a xapian bug about sort order, we 
                // invert here.
                m_nq->xenquire->set_sort_by_key((QSorter*)m_sorter, 
                                                !m_sortAscending);
            }
            m_nq->xenquire->set_query(m_nq->xquery);
            m_nq->xmset = Xapian::MSet();
            // Get the query description and trim the "Xapian::Query"
            d = m_nq->xquery.get_description();
            m_reason.erase();
            break;
        } catch (const Xapian::DatabaseModifiedError &e) {
            m_reason = e.get_msg();
            m_db->m_ndb->xrdb.reopen();
            continue;
        } XCATCHERROR(m_reason);
        break;
    }

    if (!m_reason.empty()) {
        LOGDEB("Query::SetQuery: xapian error " << m_reason << "\n");
        return false;
    }
        
    if (d.find("Xapian::Query") == 0)
        d.erase(0, strlen("Xapian::Query"));

    sdata->setDescription(d);
    m_sd = sdata;
    LOGDEB("Query::SetQuery: Q: " << sdata->getDescription() << "\n");
    return true;
}

bool Query::getQueryTerms(vector<string>& terms)
{
    if (!m_nq)
        return false;

    terms.clear();
    Xapian::TermIterator it;
    string ermsg;
    try {
        for (it = m_nq->xquery.get_terms_begin(); 
             it != m_nq->xquery.get_terms_end(); it++) {
            terms.push_back(*it);
        }
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
        LOGERR("getQueryTerms: xapian error: " << ermsg << "\n");
        return false;
    }
    return true;
}

int Query::makeDocAbstract(const Doc &doc, vector<Snippet>& abstract, 
                           int maxoccs, int ctxwords, bool sortbypage)
{
    LOGDEB("makeDocAbstract: maxoccs " << maxoccs << " ctxwords "  <<
           ctxwords << "\n");
    if (!m_db || !m_db->m_ndb || !m_db->m_ndb->m_isopen || !m_nq) {
        LOGERR("Query::makeDocAbstract: no db or no nq\n");
        return ABSRES_ERROR;
    }
    int ret = ABSRES_ERROR;
    XAPTRY(ret = m_nq->makeAbstract(doc.xdocid, abstract, maxoccs, ctxwords, sortbypage),
           m_db->m_ndb->xrdb, m_reason);
    if (!m_reason.empty()) {
        LOGDEB("makeDocAbstract: makeAbstract: reason: " << m_reason << "\n");
        return ABSRES_ERROR;
    }
    return ret;
}

bool Query::makeDocAbstract(const Doc &doc, vector<string>& abstract)
{
    vector<Snippet> vpabs;
    if (!makeDocAbstract(doc, vpabs))
        return false;
    for (vector<Snippet>::const_iterator it = vpabs.begin();
         it != vpabs.end(); it++) {
        string chunk;
        if (it->page > 0) {
            ostringstream ss;
            ss << it->page;
            chunk += string(" [p ") + ss.str() + "] ";
        }
        chunk += it->snippet;
        abstract.push_back(chunk);
    }
    return true;
}

bool Query::makeDocAbstract(const Doc &doc, string& abstract)
{
    vector<Snippet> vpabs;
    if (!makeDocAbstract(doc, vpabs))
        return false;
    for (vector<Snippet>::const_iterator it = vpabs.begin(); 
         it != vpabs.end(); it++) {
        abstract.append(it->snippet);
        abstract.append(cstr_ellipsis);
    }
    return m_reason.empty() ? true : false;
}

int Query::getFirstMatchPage(const Doc &doc, string& term)
{
    LOGDEB1("Db::getFirstMatchPage\n");;
    if (!m_nq) {
        LOGERR("Query::getFirstMatchPage: no nq\n");
        return false;
    }
    int pagenum = -1;
    XAPTRY(pagenum = m_nq->getFirstMatchPage(Xapian::docid(doc.xdocid), term),
           m_db->m_ndb->xrdb, m_reason);
    return m_reason.empty() ? pagenum : -1;
}


// Mset size
static const int qquantum = 50;

// Get estimated result count for query. Xapian actually does most of
// the search job in there, this can be long
int Query::getResCnt(int checkatleast, bool useestimate)
{
    if (!m_db || !m_nq || !m_nq->xenquire) {
        LOGERR("Query::getResCnt: no query opened\n");
        return -1;
    }
    LOGDEB0("Query::getResCnt: checkatleast " << checkatleast << " estimate " <<
            useestimate << "\n");

    if (m_resCnt >= 0)
        return m_resCnt;

    if (m_nq->xmset.size() <= 0) {
        Chrono chron;
        XAPTRY(if (checkatleast == -1)
                   checkatleast = m_db->docCnt();
               m_nq->xmset = m_nq->xenquire->get_mset(0, qquantum, checkatleast),
               m_db->m_ndb->xrdb, m_reason);
        if (!m_reason.empty()) {
            LOGERR("xenquire->get_mset: exception: " << m_reason << "\n");
            return -1;
        }
        LOGDEB("Query::getResCnt: get_mset: " << chron.millis() << " mS\n");
    }

    if (useestimate) {
        m_resCnt = m_nq->xmset.get_matches_estimated();
    } else {
        m_resCnt = m_nq->xmset.get_matches_lower_bound();
    }
    LOGDEB("Query::getResCnt: " << m_resCnt << "\n");
    return m_resCnt;
}


// Get document at rank xapi in query results.  We check if the
// current mset has the doc, else ask for an other one. We use msets
// of qquantum documents.
//
// Note that as stated by a Xapian developer, Enquire searches from
// scratch each time get_mset() is called. So the better performance
// on subsequent calls is probably only due to disk caching.
bool Query::getDoc(int xapi, Doc &doc, bool fetchtext)
{
    LOGDEB1("Query::getDoc: xapian enquire index " << xapi << "\n");
    if (!m_nq || !m_nq->xenquire) {
        LOGERR("Query::getDoc: no query opened\n");
        return false;
    }

    int first = m_nq->xmset.get_firstitem();
    int last = first + m_nq->xmset.size() -1;

    if (!(xapi >= first && xapi <= last)) {
        LOGDEB("Fetching for first " << xapi << ", count " << qquantum << "\n");

        XAPTRY(m_nq->xmset = m_nq->xenquire->get_mset(xapi, qquantum,  
                                                      (const Xapian::RSet *)0),
               m_db->m_ndb->xrdb, m_reason);

        if (!m_reason.empty()) {
            LOGERR("enquire->get_mset: exception: " << m_reason << "\n");
            return false;
        }
        if (m_nq->xmset.empty()) {
            LOGDEB("enquire->get_mset: got empty result\n");
            return false;
        }
        first = m_nq->xmset.get_firstitem();
        last = first + m_nq->xmset.size() -1;
    }

    Xapian::Document xdoc;
    Xapian::docid docid = 0;
    int pc = 0;
    int collapsecount = 0;
    string data;
    string udi;
    m_reason.erase();
    for (int xaptries=0; xaptries < 2; xaptries++) {
        try {
            xdoc = m_nq->xmset[xapi-first].get_document();
            collapsecount = m_nq->xmset[xapi-first].get_collapse_count();
            docid = *(m_nq->xmset[xapi-first]);
            pc = m_nq->xmset.convert_to_percent(m_nq->xmset[xapi-first]);
            data = xdoc.get_data();
            m_reason.erase();
            Chrono chron;
            m_db->m_ndb->xdocToUdi(xdoc, udi);
            LOGDEB2("Query::getDoc: " << chron.millis() << " ms for udi [" <<
                    udi << "], collapse count " << collapsecount << "\n");
            break;
        } catch (Xapian::DatabaseModifiedError &error) {
            // retry or end of loop
            m_reason = error.get_msg();
            continue;
        }
        XCATCHERROR(m_reason);
        break;
    }
    if (!m_reason.empty()) {
        LOGERR("Query::getDoc: " << m_reason << "\n");
        return false;
    }
    doc.meta[Rcl::Doc::keyudi] = udi;

    doc.pc = pc;
    char buf[200];
    if (collapsecount > 0) {
        sprintf(buf,"%3d%% (%d)", pc, collapsecount + 1);
    } else {
        sprintf(buf,"%3d%%", pc);
    }
    doc.meta[Doc::keyrr] = buf;

    if (collapsecount > 0) {
        sprintf(buf, "%d", collapsecount);
        doc.meta[Rcl::Doc::keycc] = buf;
    }

    // Parse xapian document's data and populate doc fields
    return m_db->m_ndb->dbDataToRclDoc(docid, data, doc, fetchtext);
}

vector<string> Query::expand(const Doc &doc)
{
    LOGDEB("Rcl::Query::expand()\n");
    vector<string> res;
    if (!m_nq || !m_nq->xenquire) {
        LOGERR("Query::expand: no query opened\n");
        return res;
    }

    for (int tries = 0; tries < 2; tries++) {
        try {
            Xapian::RSet rset;
            rset.add_document(Xapian::docid(doc.xdocid));
            // We don't exclude the original query terms.
            Xapian::ESet eset = m_nq->xenquire->get_eset(20, rset, false);
            LOGDEB("ESet terms:\n");
            // We filter out the special terms
            for (Xapian::ESetIterator it = eset.begin(); 
                 it != eset.end(); it++) {
                LOGDEB(" [" << (*it) << "]\n");
                if ((*it).empty() || has_prefix(*it))
                    continue;
                res.push_back(*it);
                if (res.size() >= 10)
                    break;
            }
            m_reason.erase();
            break;
        } catch (const Xapian::DatabaseModifiedError &e) {
            m_reason = e.get_msg();                    
            m_db->m_ndb->xrdb.reopen();
            continue;
        } XCATCHERROR(m_reason);
        break;
    }

    if (!m_reason.empty()) {
        LOGERR("Query::expand: xapian error " << m_reason << "\n");
        res.clear();
    }

    return res;
}

}

