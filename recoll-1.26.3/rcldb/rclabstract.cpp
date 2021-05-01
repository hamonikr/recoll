/* Copyright (C) 2004-2017 J.F.Dockes
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

#include <math.h>

#include <map>
#include <unordered_map>
#include <deque>
#include <algorithm>

#include "log.h"
#include "rcldb.h"
#include "rcldb_p.h"
#include "rclquery.h"
#include "rclquery_p.h"
#include "textsplit.h"
#include "searchdata.h"
#include "utf8iter.h"
#include "hldata.h"
#include "chrono.h"

using namespace std;


namespace Rcl {

static Chrono chron;

// This is used as a marker inside the abstract frag lists, but
// normally doesn't remain in final output (which is built with a
// custom sep. by our caller).
static const string cstr_ellipsis("...");
static const string emptys;
// This is used to mark positions overlapped by a multi-word match term
static const string occupiedmarker("?");

#define DEBUGABSTRACT  
#ifdef DEBUGABSTRACT
#define LOGABS LOGDEB
#else
#define LOGABS LOGDEB2
#endif

// Unprefix terms. Actually it's not completely clear if we should
// remove prefixes and keep all terms or prune the prefixed
// ones. There is no good way to be sure what will provide the best
// result in general.
static const bool prune_prefixed_terms = true; 
static void noPrefixList(const vector<string>& in, vector<string>& out) 
{
    for (const auto& term : in) {
        if (prune_prefixed_terms) {
            if (has_prefix(term))
                continue;
        }
        out.push_back(strip_prefix(term));
    }
    sort(out.begin(), out.end());
    vector<string>::iterator it = unique(out.begin(), out.end());
    out.resize(it - out.begin());
}

bool Query::Native::getMatchTerms(unsigned long xdocid, vector<string>& terms)
{
    if (!xenquire) {
        LOGERR("Query::getMatchTerms: no query opened\n");
        return false;
    }

    terms.clear();
    Xapian::TermIterator it;
    Xapian::docid id = Xapian::docid(xdocid);
    vector<string> iterms;
    XAPTRY(iterms.insert(iterms.begin(),
                        xenquire->get_matching_terms_begin(id),
                        xenquire->get_matching_terms_end(id)),
           m_q->m_db->m_ndb->xrdb, m_q->m_reason);
    if (!m_q->m_reason.empty()) {
        LOGERR("getMatchTerms: xapian error: " << m_q->m_reason << "\n");
        return false;
    }
    noPrefixList(iterms, terms);
    return true;
}

// Retrieve db-wide frequencies for the query terms and store them in
// the query object. This is done at most once for a query, and the data is used
// while computing abstracts for the different result documents.
void Query::Native::setDbWideQTermsFreqs()
{
    // Do it once only for a given query.
    if (!termfreqs.empty())
        return;

    vector<string> qterms;
    {
        vector<string> iqterms;
        m_q->getQueryTerms(iqterms);
        noPrefixList(iqterms, qterms);
    }
    LOGDEB("Query terms: " << stringsToString(qterms) << endl);
    Xapian::Database &xrdb = m_q->m_db->m_ndb->xrdb;

    double doccnt = xrdb.get_doccount();
    if (doccnt == 0) 
        doccnt = 1;

    for (const auto& term : qterms) {
        termfreqs[term] = xrdb.get_termfreq(term) / doccnt;
        LOGABS("setDbWideQTermFreqs: [" << term << "] db freq " <<
               termfreqs[term] << "\n");
    }
}

// Compute matched terms quality coefficients for a matched document by
// retrieving the Within Document Frequencies and multiplying by
// overal term frequency, then using log-based thresholds.
// 2012: it's not too clear to me why exactly we do the log thresholds thing.
//  Preferring terms wich are rare either or both in the db and the document 
//  seems reasonable though
// To avoid setting a high quality for a low frequency expansion of a
// common stem, which seems wrong, we group the terms by
// root, compute a frequency for the group from the sum of member
// occurrences, and let the frequency for each group member be the
// aggregated frequency.
double Query::Native::qualityTerms(Xapian::docid docid, 
                                   const vector<string>& terms,
                                   multimap<double, vector<string> >& byQ)
{
    LOGABS("qualityTerms: entry " << chron.millis() << "mS\n");
    setDbWideQTermsFreqs();
    LOGABS("qualityTerms: setDbWide..:  " << chron.millis() << "mS\n");

    map<string, double> termQcoefs;
    double totalweight = 0;

    Xapian::Database &xrdb = m_q->m_db->m_ndb->xrdb;
    double doclen = xrdb.get_doclength(docid);
    if (doclen == 0) 
        doclen = 1;
    HighlightData hld;
    if (m_q->m_sd) {
        m_q->m_sd->getTerms(hld);
    }
    LOGABS("qualityTerms: m_sd->getTerms():  " << chron.millis() << "mS\n");

    // Group the input terms by the user term they were possibly
    // expanded from (by stemming)
    map<string, vector<string> > byRoot;
    for (const auto& term: terms) {
        const auto eit = hld.terms.find(term);
        if (eit != hld.terms.end()) {
            byRoot[eit->second].push_back(term);
        } else {
            LOGDEB0("qualityTerms: [" << term << "] not found in hld\n");
            byRoot[term].push_back(term);
        }
    }

#ifdef DEBUGABSTRACT
    {
        LOGABS("qualityTerms: hld: " << hld.toString() << "\n");
        string byRootstr;
        for (const auto& entry : byRoot) {
            byRootstr.append("[").append(entry.first).append("]->");
            for (const auto& term : entry.second) {
                byRootstr.append("[").append(term).append("] ");
            }
            byRootstr.append("\n");
        }
        LOGABS("qualityTerms: uterms to terms: " << chron.millis() << "mS " <<
               byRootstr << endl);
    }
#endif

    // Compute in-document and global frequencies for the groups. We
    // used to call termlist_begin() for each term. This was very slow
    // on big documents and long term lists. We now compute a sorted
    // list of terms (with pointers back to their root through a map),
    // and just call skip_to repeatedly
    vector<string> allterms;
    unordered_map<string, string> toRoot;
    for (const auto& group : byRoot) {
        for (const auto& term : group.second) {
            allterms.push_back(term);
            toRoot[term] = group.first;
        }
    }
    sort(allterms.begin(), allterms.end());
    allterms.erase(unique(allterms.begin(), allterms.end()), allterms.end());
    
    map<string, double> grpwdfs;
    map<string, double> grptfreqs;
    Xapian::TermIterator xtermit = xrdb.termlist_begin(docid);
    for (const auto& term : allterms) {
        const string& root = toRoot[term];
        xtermit.skip_to(term);
        if (xtermit != xrdb.termlist_end(docid) && *xtermit == term) {
            if (grpwdfs.find(root) != grpwdfs.end()) {
                grpwdfs[root] = xtermit.get_wdf() / doclen;
                grptfreqs[root] = termfreqs[term];
            } else {
                grpwdfs[root] += xtermit.get_wdf() / doclen;
                grptfreqs[root] += termfreqs[term];
            }
        } else {
            LOGDEB("qualityTerms: term not found in doc term list: " << term <<
                   endl);
        }
    }    
    LOGABS("qualityTerms: freqs compute:  " << chron.millis() << "mS\n");

    // Build a sorted by quality container for the groups
    for (const auto& group : byRoot) {
        double q = (grpwdfs[group.first]) * grptfreqs[group.first];
        q = -log10(q);
        if (q < 3) {
            q = 0.05;
        } else if (q < 4) {
            q = 0.3;
        } else if (q < 5) {
            q = 0.7;
        } else if (q < 6) {
            q = 0.8;
        } else {
            q = 1;
        }
        totalweight += q;
        byQ.insert(pair<double, vector<string> >(q, group.second));
    }

#ifdef DEBUGABSTRACT
    for (auto mit= byQ.rbegin(); mit != byQ.rend(); mit++) {
        LOGABS("qualityTerms: coef: " << mit->first << " group: " <<
               stringsToString(mit->second) << endl);
    }
#endif
    return totalweight;
}


// Return page number for first match of "significant" term.
int Query::Native::getFirstMatchPage(Xapian::docid docid, string& term)
{
    LOGDEB("Query::Native::getFirstMatchPage\n");
    chron.restart();
    if (!m_q|| !m_q->m_db || !m_q->m_db->m_ndb || !m_q->m_db->m_ndb->m_isopen) {
        LOGERR("Query::getFirstMatchPage: no db\n");
        return -1;
    }
    Rcl::Db::Native *ndb(m_q->m_db->m_ndb);
    Xapian::Database& xrdb(ndb->xrdb);

    vector<string> terms;
    getMatchTerms(docid, terms);

    if (terms.empty()) {
        LOGDEB("getFirstMatchPage: empty match term list (field match?)\n");
        return -1;
    }

    vector<int> pagepos;
    ndb->getPagePositions(docid, pagepos);
    if (pagepos.empty())
        return -1;
        
    setDbWideQTermsFreqs();

    // We try to use a page which matches the "best" term. Get a sorted list
    multimap<double, vector<string> > byQ;
    qualityTerms(docid, terms, byQ);

    for (auto mit = byQ.rbegin(); mit != byQ.rend(); mit++) {
        for (vector<string>::const_iterator qit = mit->second.begin();
             qit != mit->second.end(); qit++) {
            string qterm = *qit;
            Xapian::PositionIterator pos;
            string emptys;
            try {
                for (pos = xrdb.positionlist_begin(docid, qterm);
                     pos != xrdb.positionlist_end(docid, qterm); pos++) {
                    int pagenum = ndb->getPageNumberForPosition(pagepos, *pos);
                    if (pagenum > 0) {
                        term = qterm;
                        return pagenum;
                    }
                }
            } catch (...) {
                // Term does not occur. No problem.
            }
        }
    }
    return -1;
}

// Creating the abstract from index position data: populate the sparse
// array with the positions for a given query term, and mark the
// neighboring positions.
void Query::Native::abstractPopulateQTerm(
    Xapian::Database& xrdb,
    Xapian::docid docid,
    const string& qterm,
    int qtrmwrdcnt,
    int ctxwords,
    unsigned int maxgrpoccs,
    unsigned int maxtotaloccs,
    map<unsigned int, string>& sparseDoc,
    unordered_set<unsigned int>& searchTermPositions,
    unsigned int& maxpos,
    unsigned int& totaloccs,
    unsigned int& grpoccs,
    int& ret
    )
{
    Xapian::PositionIterator pos;

    // Walk the position list for this term.
    for (pos = xrdb.positionlist_begin(docid, qterm);
         pos != xrdb.positionlist_end(docid, qterm); pos++) {
        int ipos = *pos;
        if (ipos < int(baseTextPosition)) // Not in text body
            continue;
        LOGABS("makeAbstract: [" << qterm << "] at pos " <<
               ipos << " grpoccs " << grpoccs << " maxgrpoccs " <<
               maxgrpoccs << "\n");

        totaloccs++;
        grpoccs++;

        // Add adjacent slots to the set to populate at next
        // step by inserting empty strings. Special provisions
        // for adding ellipsis and for positions overlapped by
        // the match term.
        unsigned int sta = MAX(int(baseTextPosition), 
                               ipos - ctxwords);
        unsigned int sto = ipos + qtrmwrdcnt-1 + 
            m_q->m_db->getAbsCtxLen();
        for (unsigned int ii = sta; ii <= sto;  ii++) {
            if (ii == (unsigned int)ipos) {
                sparseDoc[ii] = qterm;
                searchTermPositions.insert(ii);
                if (ii > maxpos)
                    maxpos = ii;
            } else if (ii > (unsigned int)ipos && 
                       ii < (unsigned int)ipos + qtrmwrdcnt) {
                // Position for another word of the multi-word term
                sparseDoc[ii] = occupiedmarker;
            } else if (!sparseDoc[ii].compare(cstr_ellipsis)) {
                // For an empty slot, the test above has a side
                // effect of inserting an empty string which
                // is what we want. Do it also if it was an ellipsis
                sparseDoc[ii] = emptys;
            }
        }
        // Add ellipsis at the end. This may be replaced later by
        // an overlapping extract. Take care not to replace an
        // empty string here, we really want an empty slot,
        // use find()
        if (sparseDoc.find(sto+1) == sparseDoc.end()) {
            sparseDoc[sto+1] = cstr_ellipsis;
        }

        // Group done ?
        if (grpoccs >= maxgrpoccs) {
            ret |= ABSRES_TRUNC;
            LOGABS("Db::makeAbstract: max group occs cutoff\n");
            break;
        }
        // Global done ?
        if (totaloccs >= maxtotaloccs) {
            ret |= ABSRES_TRUNC;
            LOGABS("Db::makeAbstract: max occurrences cutoff\n");
            break;
        }
    }
}

// Creating the abstract from index position data: after the query
// terms have been inserted at their place in the sparse array, and
// the neighboring positions marked, populate the neighbours: for each
// term in the document, walk its position list and populate slots
// around the query terms. We arbitrarily truncate the list to avoid
// taking forever. If we do cutoff, the abstract may be inconsistant
// (missing words, potentially altering meaning), which is bad.
void Query::Native::abstractPopulateContextTerms(
    Xapian::Database& xrdb,
    Xapian::docid docid,
    unsigned int maxpos,
    map<unsigned int, string>& sparseDoc,
    int& ret
    )
{
    Xapian::TermIterator term;
    int cutoff = m_q->m_snipMaxPosWalk;
    for (term = xrdb.termlist_begin(docid);
         term != xrdb.termlist_end(docid); term++) {
        // Ignore prefixed terms
        if (has_prefix(*term))
            continue;
        if (m_q->m_snipMaxPosWalk > 0 && cutoff-- < 0) {
            ret |= ABSRES_TERMMISS;
            LOGDEB0("makeAbstract: max term count cutoff " <<
                    m_q->m_snipMaxPosWalk << "\n");
            break;
        }

        map<unsigned int, string>::iterator vit;
        Xapian::PositionIterator pos;
        for (pos = xrdb.positionlist_begin(docid, *term);
             pos != xrdb.positionlist_end(docid, *term); pos++) {
            if (m_q->m_snipMaxPosWalk > 0 && cutoff-- < 0) {
                ret |= ABSRES_TERMMISS;
                LOGDEB0("makeAbstract: max term count cutoff " <<
                        m_q->m_snipMaxPosWalk << "\n");
                break;
            }
            // If we are beyond the max possible position, stop
            // for this term
            if (*pos > maxpos) {
                break;
            }
            if ((vit = sparseDoc.find(*pos)) != sparseDoc.end()) {
                // Don't replace a term: the terms list is in
                // alphabetic order, and we may have several terms
                // at the same position, we want to keep only the
                // first one (ie: dockes and dockes@wanadoo.fr)
                if (vit->second.empty()) {
                    LOGDEB2("makeAbstract: populating: [" << *term <<
                            "] at " << *pos << "\n");
                    sparseDoc[*pos] = *term;
                }
            }
        }
    }
}

// Creating the abstract from position data: final phase: extract the
// snippets from the sparse array.
void Query::Native::abstractCreateSnippetsVector(
    Rcl::Db::Native *ndb,
    map<unsigned int, string>& sparseDoc,
    unordered_set<unsigned int>& searchTermPositions,
    vector<int>& vpbreaks,
    vector<Snippet>& vabs)
{
    vabs.clear();
    string chunk;
    bool incjk = false;
    int page = 0;
    string term;

    for (const auto& ent : sparseDoc) {
        LOGDEB2("Abtract:output "<< ent.first <<" -> [" <<ent.second <<"]\n");
        if (!occupiedmarker.compare(ent.second)) {
            LOGDEB("Abstract: qtrm position not filled ??\n");
            continue;
        }
        if (chunk.empty() && !vpbreaks.empty()) {
            page =  ndb->getPageNumberForPosition(vpbreaks, ent.first);
            if (page < 0) 
                page = 0;
            term.clear();
        }
        Utf8Iter uit(ent.second);
        bool newcjk = false;
        if (TextSplit::isCJK(*uit))
            newcjk = true;
        if (!incjk || (incjk && !newcjk))
            chunk += " ";
        incjk = newcjk;
        if (searchTermPositions.find(ent.first) != searchTermPositions.end())
            term = ent.second;
        if (ent.second == cstr_ellipsis) {
            vabs.push_back(Snippet(page, chunk).setTerm(term));
            chunk.clear();
        } else {
            if (ent.second.compare(end_of_field_term) && 
                ent.second.compare(start_of_field_term))
                chunk += ent.second;
        }
    }
    if (!chunk.empty())
        vabs.push_back(Snippet(page, chunk).setTerm(term));
}

// Creating the abstract from index position data: top level routine
int Query::Native::abstractFromIndex(
    Rcl::Db::Native *ndb,
    Xapian::docid docid,
    const vector<string>& matchTerms,
    const multimap<double, vector<string>> byQ,
    double totalweight,
    int ctxwords,
    unsigned int maxtotaloccs,
    vector<Snippet>& vabs,
    Chrono& chron
    )
{
    Xapian::Database& xrdb(ndb->xrdb);
    int ret = ABSRES_OK;
    // The terms 'array' that we partially populate with the document
    // terms, at their positions around the search terms positions:
    map<unsigned int, string> sparseDoc;
    // Also remember apart the search term positions so that we can list
    // them with their snippets.
    std::unordered_set<unsigned int> searchTermPositions;

    // Remember max position. Used to stop walking positions lists while 
    // populating the adjacent slots.
    unsigned int maxpos = 0;

    // Total number of occurences for all terms. We stop when we have too much
    unsigned int totaloccs = 0;

    // First pass to populate the sparse document: we walk the term
    // groups, beginning with the better ones, and insert each term at
    // its position. We also insert empty strings at the surrounding
    // positions. These are markers showing where we should insert
    // data during the next pass.
    for (auto mit = byQ.rbegin(); mit != byQ.rend(); mit++) {
        unsigned int maxgrpoccs;
        double q;
        if (byQ.size() == 1) {
            maxgrpoccs = maxtotaloccs;
            q = 1.0;
        } else {
            // We give more slots to the better term groups
            q = mit->first / totalweight;
            maxgrpoccs = int(ceil(maxtotaloccs * q));
        }
        unsigned int grpoccs = 0;

        // For each term in user term expansion group
        for (const auto& qterm : mit->second) {
            // Enough for this group ?
            if (grpoccs >= maxgrpoccs) 
                break;

            LOGABS("makeAbstract: [" << qterm << "] " << maxgrpoccs <<
                   " max grp occs (coef " << q << ")\n");

            // The match term may span several words (more than one position)
            int qtrmwrdcnt = 
                TextSplit::countWords(qterm, TextSplit::TXTS_NOSPANS);

            // Populate positions for this query term.
            // There may be query terms not in this doc. This raises an
            // exception when requesting the position list, we catch it ??
            // Not clear how this can happen because we are walking the
            // match list returned by Xapian. Maybe something with the
            // fields?
            try {
                abstractPopulateQTerm(xrdb, docid, qterm, qtrmwrdcnt, ctxwords,
                                      maxgrpoccs,maxtotaloccs, sparseDoc,
                                      searchTermPositions, maxpos, totaloccs,
                                      grpoccs, ret);
            } catch (...) {
                // Term does not occur. No problem.
            }

            if (totaloccs >= maxtotaloccs) {
                ret |= ABSRES_TRUNC;
                LOGABS("Db::makeAbstract: max1 occurrences cutoff\n");
                break;
            }
        }
    }
    maxpos += ctxwords + 1;

    LOGABS("makeAbstract:" << chron.millis() <<
           "mS:chosen number of positions " << totaloccs << "\n");

    // This can happen if there are term occurences in the keywords
    // etc. but not elsewhere ?
    if (totaloccs == 0) {
        LOGDEB("makeAbstract: no occurrences\n");
        return ABSRES_OK;
    }

    abstractPopulateContextTerms(xrdb, docid, maxpos, sparseDoc, ret);
    
    LOGABS("makeAbstract:" << chron.millis() << "mS: all term poslist read\n");

    vector<int> vpbreaks;
    ndb->getPagePositions(docid, vpbreaks);

    LOGABS("makeAbstract:" << chron.millis() << "mS: extracting. Got " <<
           vpbreaks.size() << " pages\n");

    // Finally build the abstract by walking the map (in order of position)
    abstractCreateSnippetsVector(ndb, sparseDoc, searchTermPositions,
                                 vpbreaks, vabs);
    
    LOGABS("makeAbtract: done in " << chron.millis() << " mS\n");
    return ret;
}


// Build a document abstract by extracting text chunks around the
// query terms.  This can either uses the index position lists, or the
// stored document text, with very different implementations.
//
// DatabaseModified and other general exceptions are catched and
// possibly retried by our caller.
//
// @param[out] vabs the abstract is returned as a vector of snippets.
int Query::Native::makeAbstract(Xapian::docid docid,
                                vector<Snippet>& vabs, 
                                int imaxoccs, int ictxwords, bool sortbypage)
{
    chron.restart();
    LOGDEB("makeAbstract: docid " << docid << " imaxoccs " <<
           imaxoccs << " ictxwords " << ictxwords << " sort by page " <<
           sortbypage << "\n");

    // The (unprefixed) terms matched by this document
    vector<string> matchedTerms;
    getMatchTerms(docid, matchedTerms);
    if (matchedTerms.empty()) {
        LOGDEB("makeAbstract:" << chron.millis() << "mS:Empty term list\n");
        return ABSRES_ERROR;
    }

    LOGDEB("Match terms: " << stringsToString(matchedTerms) << endl);

    // Retrieve the term frequencies for the query terms. This is
    // actually computed only once for a query, and for all terms in
    // the query (not only the matches for this doc)
    setDbWideQTermsFreqs();

    // Build a sorted by quality container for the match terms We are
    // going to try and show text around the less common search terms.
    // Terms issued from an original one by stem expansion are
    // aggregated by the qualityTerms() routine (this is what we call
    // 'term groups' in the following: index terms expanded from the
    // same user term).
    multimap<double, vector<string>> byQ;
    double totalweight = qualityTerms(docid, matchedTerms, byQ);
    LOGABS("makeAbstract:" << chron.millis() << "mS: computed Qcoefs.\n");
    // This can't happen, but would crash us
    if (totalweight == 0.0) {
        LOGERR("makeAbstract:"<<chron.millis()<<"mS: totalweight == 0.0 !\n");
        return ABSRES_ERROR;
    }

    Rcl::Db::Native *ndb(m_q->m_db->m_ndb);

    // Total number of slots we populate. The 7 is taken as
    // average word size. It was a mistake to have the user max
    // abstract size parameter in characters, we basically only deal
    // with words. We used to limit the character size at the end, but
    // this damaged our careful selection of terms
    const unsigned int maxtotaloccs = imaxoccs > 0 ? imaxoccs :
        m_q->m_db->getAbsLen() /(7 * (m_q->m_db->getAbsCtxLen() + 1));
    int ctxwords = ictxwords == -1 ? m_q->m_db->getAbsCtxLen() : ictxwords;
    LOGABS("makeAbstract:" << chron.millis() << "mS: mxttloccs " <<
           maxtotaloccs << " ctxwords " << ctxwords << "\n");

    if (ndb->m_storetext) {
        return abstractFromText(ndb, docid, matchedTerms, byQ,
                                totalweight, ctxwords, maxtotaloccs, vabs,
                                chron, sortbypage);
    } else {
        return abstractFromIndex(ndb, docid, matchedTerms, byQ,
                                 totalweight, ctxwords, maxtotaloccs, vabs,
                                 chron);
    }
}


}
