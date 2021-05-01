/* Copyright (C) 2004 J.F.Dockes
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

////////////////////////////////////////////////////////////////////
/** Things dealing with walking the terms lists and expansion dbs */

#include "autoconfig.h"

#include <string>

#include "log.h"
#include "rcldb.h"
#include "rcldb_p.h"
#include "stemdb.h"
#include "expansiondbs.h"
#include "strmatcher.h"

using namespace std;

namespace Rcl {

// File name wild card expansion. This is a specialisation ot termMatch
bool Db::filenameWildExp(const string& fnexp, vector<string>& names, int max)
{
    string pattern = fnexp;
    names.clear();

    // If pattern is not capitalized, not quoted (quoted pattern can't
    // get here currently anyway), and has no wildcards, we add * at
    // each end: match any substring
    if (pattern[0] == '"' && pattern[pattern.size()-1] == '"') {
        pattern = pattern.substr(1, pattern.size() -2);
    } else if (pattern.find_first_of(cstr_minwilds) == string::npos && 
               !unaciscapital(pattern)) {
        pattern = "*" + pattern + "*";
    } // else let it be

    LOGDEB("Rcl::Db::filenameWildExp: pattern: ["  << pattern << "]\n");

    // We inconditionnally lowercase and strip the pattern, as is done
    // during indexing. This seems to be the only sane possible
    // approach with file names and wild cards. termMatch does
    // stripping conditionally on indexstripchars.
    string pat1;
    if (unacmaybefold(pattern, pat1, "UTF-8", UNACOP_UNACFOLD)) {
        pattern.swap(pat1);
    }

    TermMatchResult result;
    if (!idxTermMatch(ET_WILD, string(), pattern, result, max,
                      unsplitFilenameFieldName))
        return false;
    for (const auto& entry : result.entries) {
        names.push_back(entry.term);
    }
    if (names.empty()) {
        // Build an impossible query: we know its impossible because we
        // control the prefixes!
        names.push_back(wrap_prefix("XNONE") + "NoMatchingTerms");
    }
    return true;
}

// Walk the Y terms and return min/max
bool Db::maxYearSpan(int *minyear, int *maxyear)
{
    LOGDEB("Rcl::Db:maxYearSpan\n");
    *minyear = 1000000; 
    *maxyear = -1000000;
    TermMatchResult result;
    if (!idxTermMatch(ET_WILD, string(), "*", result, -1, "xapyear")) {
        LOGINFO("Rcl::Db:maxYearSpan: termMatch failed\n");
        return false;
    }
    for (const auto& entry : result.entries) {
        if (!entry.term.empty()) {
            int year = atoi(strip_prefix(entry.term).c_str());
            if (year < *minyear)
                *minyear = year;
            if (year > *maxyear)
                *maxyear = year;
        }
    }
    return true;
}

bool Db::getAllDbMimeTypes(std::vector<std::string>& exp)
{
    Rcl::TermMatchResult res;
    if (!idxTermMatch(Rcl::Db::ET_WILD, "", "*", res, -1, "mtype")) {
        return false;
    }
    for (const auto& entry : res.entries) {
        exp.push_back(Rcl::strip_prefix(entry.term));
    }
    return true;
}

class TermMatchCmpByWcf {
public:
    int operator()(const TermMatchEntry& l, const TermMatchEntry& r) {
        return r.wcf - l.wcf < 0;
    }
};
class TermMatchCmpByTerm {
public:
    int operator()(const TermMatchEntry& l, const TermMatchEntry& r) {
        return l.term.compare(r.term) > 0;
    }
};
class TermMatchTermEqual {
public:
    int operator()(const TermMatchEntry& l, const TermMatchEntry& r) {
        return !l.term.compare(r.term);
    }
};

static const char *tmtptostr(int typ)
{
    switch (typ) {
    case Db::ET_WILD: return "wildcard";
    case Db::ET_REGEXP: return "regexp";
    case Db::ET_STEM: return "stem";
    case Db::ET_NONE:
    default: return "none";
    }
}

// Find all index terms that match an input along different expansion modes:
// wildcard, regular expression, or stemming. Depending on flags we perform
// case and/or diacritics expansion (this can be the only thing requested).
// If the "field" parameter is set, we return a list of appropriately
// prefixed terms (which are going to be used to build a Xapian
// query). 
// This routine performs case/diacritics/stemming expansion against
// the auxiliary tables, and possibly calls idxTermMatch() for work
// using the main index terms (filtering, retrieving stats, expansion
// in some cases).
bool Db::termMatch(int typ_sens, const string &lang, const string &_term,
                   TermMatchResult& res, int max,  const string& field,
                   vector<string>* multiwords)
{
    int matchtyp = matchTypeTp(typ_sens);
    if (!m_ndb || !m_ndb->m_isopen)
        return false;
    Xapian::Database xrdb = m_ndb->xrdb;

    bool diac_sensitive = (typ_sens & ET_DIACSENS) != 0;
    bool case_sensitive = (typ_sens & ET_CASESENS) != 0;
    // Path elements (used for dir: filtering) are special because
    // they are not unaccented or lowercased even if the index is
    // otherwise stripped.
    bool pathelt = (typ_sens & ET_PATHELT) != 0;
    
    LOGDEB0("Db::TermMatch: typ " << tmtptostr(matchtyp) << " diacsens " <<
            diac_sensitive << " casesens " << case_sensitive << " pathelt " <<
            pathelt << " lang ["  <<
            lang << "] term [" << _term << "] max " << max << " field [" <<
            field << "] stripped " << o_index_stripchars << " init res.size "
            << res.entries.size() << "\n");

    // If index is stripped, no case or diac expansion can be needed:
    // for the processing inside this routine, everything looks like
    // we're all-sensitive: no use of expansion db.
    // Also, convert input to lowercase and strip its accents.
    string term = _term;
    if (o_index_stripchars) {
        diac_sensitive = case_sensitive = true;
        if (!pathelt && !unacmaybefold(_term, term, "UTF-8", UNACOP_UNACFOLD)) {
            LOGERR("Db::termMatch: unac failed for [" << _term << "]\n");
            return false;
        }
    }

    // The case/diac expansion db
    SynTermTransUnac unacfoldtrans(UNACOP_UNACFOLD);
    XapComputableSynFamMember synac(xrdb, synFamDiCa, "all", &unacfoldtrans);

    if (matchtyp == ET_WILD || matchtyp == ET_REGEXP) {
        std::shared_ptr<StrMatcher> matcher;
        if (matchtyp == ET_WILD) {
            matcher = std::shared_ptr<StrMatcher>(new StrWildMatcher(term));
        } else {
            matcher = std::shared_ptr<StrMatcher>(new StrRegexpMatcher(term));
        }
        if (!diac_sensitive || !case_sensitive) {
            // Perform case/diac expansion on the exp as appropriate and
            // expand the result.
            vector<string> exp;
            if (diac_sensitive) {
                // Expand for diacritics and case, filtering for same diacritics
                SynTermTransUnac foldtrans(UNACOP_FOLD);
                synac.synKeyExpand(matcher.get(), exp, &foldtrans);
            } else if (case_sensitive) {
                // Expand for diacritics and case, filtering for same case
                SynTermTransUnac unactrans(UNACOP_UNAC);
                synac.synKeyExpand(matcher.get(), exp, &unactrans);
            } else {
                // Expand for diacritics and case, no filtering
                synac.synKeyExpand(matcher.get(), exp);
            }
            // Retrieve additional info and filter against the index itself
            for (const auto& term : exp) {
                idxTermMatch(ET_NONE, "", term, res, max, field);
            }
            // And also expand the original expression against the
            // main index: for the common case where the expression
            // had no case/diac expansion (no entry in the exp db if
            // the original term is lowercase and without accents).
            idxTermMatch(typ_sens, lang, term, res, max, field);
        } else {
            idxTermMatch(typ_sens, lang, term, res, max, field);
        }

    } else {
        // Expansion is STEM or NONE (which may still need synonyms
        // and case/diac exp)

        vector<string> lexp;
        if (diac_sensitive && case_sensitive) {
            // No case/diac expansion
            lexp.push_back(term);
        } else if (diac_sensitive) {
            // Expand for accents and case, filtering for same accents,
            SynTermTransUnac foldtrans(UNACOP_FOLD);
            synac.synExpand(term, lexp, &foldtrans);
        } else if (case_sensitive) {
            // Expand for accents and case, filtering for same case
            SynTermTransUnac unactrans(UNACOP_UNAC);
            synac.synExpand(term, lexp, &unactrans);
        } else {
            // We are neither accent- nor case- sensitive and may need stem
            // expansion or not. Expand for accents and case
            synac.synExpand(term, lexp);
        }

        if (matchtyp == ET_STEM || (typ_sens & ET_SYNEXP)) {
            // Note: if any of the above conds is true, we are insensitive to
            // diacs and case (enforced in searchdatatox:termexpand
            // Need stem expansion. Lowercase the result of accent and case
            // expansion for input to stemdb.
            for (auto& term : lexp) {
                string lower;
                unacmaybefold(term, lower, "UTF-8", UNACOP_FOLD);
                term.swap(lower);
            }
            sort(lexp.begin(), lexp.end());
            lexp.erase(unique(lexp.begin(), lexp.end()), lexp.end());

            if (matchtyp == ET_STEM) {
                StemDb sdb(xrdb);
                vector<string> exp1;
                for (const auto& term : lexp) {
                    sdb.stemExpand(lang, term, exp1);
                }
                exp1.swap(lexp);
                sort(lexp.begin(), lexp.end());
                lexp.erase(unique(lexp.begin(), lexp.end()), lexp.end());
                LOGDEB("Db::TermMatch: stemexp: " << stringsToString(lexp)
                       << "\n");
            }

            if (m_syngroups.ok() && (typ_sens & ET_SYNEXP)) {
                LOGDEB("Db::TermMatch: got syngroups\n");
                vector<string> exp1(lexp);
                for (const auto& term : lexp) {
                    vector<string> sg = m_syngroups.getgroup(term);
                    if (!sg.empty()) {
                        LOGDEB("Db::TermMatch: syngroups out: " <<
                               term << " -> " << stringsToString(sg) << "\n");
                        for (const auto& synonym : sg) {
                            if (synonym.find_first_of(" ") != string::npos) {
                                if (multiwords) {
                                    multiwords->push_back(synonym);
                                }
                            } else {
                                exp1.push_back(synonym);
                            }
                        }
                    }
                }
                lexp.swap(exp1);
                sort(lexp.begin(), lexp.end());
                lexp.erase(unique(lexp.begin(), lexp.end()), lexp.end());
            }

            // Expand the resulting list for case and diacritics (all
            // stemdb content is case-folded)
            vector<string> exp1;
            for (const auto& term: lexp) {
                synac.synExpand(term, exp1);
            }
            exp1.swap(lexp);
            sort(lexp.begin(), lexp.end());
            lexp.erase(unique(lexp.begin(), lexp.end()), lexp.end());
        }

        // Filter the result against the index and get the stats,
        // possibly add prefixes.
        LOGDEB("Db::TermMatch: final lexp before idx filter: " <<
               stringsToString(lexp) << "\n");
        for (const auto& term : lexp) {
            idxTermMatch(Rcl::Db::ET_WILD, "", term, res, max, field);
        }
    }

    TermMatchCmpByTerm tcmp;
    sort(res.entries.begin(), res.entries.end(), tcmp);
    TermMatchTermEqual teq;
    vector<TermMatchEntry>::iterator uit = 
        unique(res.entries.begin(), res.entries.end(), teq);
    res.entries.resize(uit - res.entries.begin());
    TermMatchCmpByWcf wcmp;
    sort(res.entries.begin(), res.entries.end(), wcmp);
    if (max > 0) {
        // Would need a small max and big stem expansion...
        res.entries.resize(MIN(res.entries.size(), (unsigned int)max));
    }
    return true;
}

bool Db::Native::idxTermMatch_p(
    int typ, const string &lang, const string &root,
    std::function<bool(const string& term,
                       Xapian::termcount colfreq,
                       Xapian::doccount termfreq)> client,
    const string& prefix)
{
    Xapian::Database xdb = xrdb;

    std::shared_ptr<StrMatcher> matcher;
    if (typ == ET_REGEXP) {
        matcher = std::shared_ptr<StrMatcher>(new StrRegexpMatcher(root));
        if (!matcher->ok()) {
            LOGERR("termMatch: regcomp failed: " << matcher->getreason());
            return false;
        }
    } else if (typ == ET_WILD) {
        matcher = std::shared_ptr<StrMatcher>(new StrWildMatcher(root));
    }

    // Find the initial section before any special char
    string::size_type es = string::npos;
    if (matcher) {
        es = matcher->baseprefixlen();
    }

    // Initial section: the part of the prefix+expr before the
    // first wildcard character. We only scan the part of the
    // index where this matches
    string is;
    if (es == string::npos) {
        is = prefix + root;
    } else if (es == 0) {
        is = prefix;
    } else {
        is = prefix + root.substr(0, es);
    }
    LOGDEB2("termMatch: initsec: [" << is << "]\n");

    for (int tries = 0; tries < 2; tries++) { 
        try {
            Xapian::TermIterator it = xdb.allterms_begin(); 
            if (!is.empty())
                it.skip_to(is.c_str());
            for (; it != xdb.allterms_end(); it++) {
                const string ixterm{*it};
                // If we're beyond the terms matching the initial
                // section, end
                if (!is.empty() && ixterm.find(is) != 0)
                    break;

                // Else try to match the term. The matcher content
                // is without prefix, so we remove this if any. We
                // just checked that the index term did begin with
                // the prefix.
                string term;
                if (!prefix.empty()) {
                    term = ixterm.substr(prefix.length());
                } else {
                    if (has_prefix(ixterm)) {
                        continue;
                    }
                    term = ixterm;
                }

                if (matcher && !matcher->match(term))
                    continue;

                if (!client(ixterm, xdb.get_collection_freq(ixterm),
                            it.get_termfreq())) {
                    break;
                }
            }
            m_rcldb->m_reason.erase();
            break;
        } catch (const Xapian::DatabaseModifiedError &e) {
            m_rcldb->m_reason = e.get_msg();
            xdb.reopen();
            continue;
        } XCATCHERROR(m_rcldb->m_reason);
        break;
    }
    if (!m_rcldb->m_reason.empty()) {
        LOGERR("termMatch: " << m_rcldb->m_reason << "\n");
        return false;
    }

    return true;
}


// Second phase of wildcard/regexp term expansion after case/diac
// expansion: expand against main index terms
bool Db::idxTermMatch(int typ_sens, const string &lang, const string &root,
                      TermMatchResult& res, int max,  const string& field)
{
    int typ = matchTypeTp(typ_sens);
    LOGDEB1("Db::idxTermMatch: typ " << tmtptostr(typ) << " lang [" <<
            lang << "] term [" << root << "] max "  << max << " field [" <<
            field << "] init res.size " << res.entries.size() << "\n");

    if (typ == ET_STEM) {
        LOGFATAL("RCLDB: internal error: idxTermMatch called with ET_STEM\n");
        abort();
    }
    string prefix;
    if (!field.empty()) {
        const FieldTraits *ftp = 0;
        if (!fieldToTraits(field, &ftp, true) || ftp->pfx.empty()) {
            LOGDEB("Db::termMatch: field is not indexed (no prefix): [" <<
                   field << "]\n");
        } else {
            prefix = wrap_prefix(ftp->pfx);
        }
    }
    res.prefix = prefix;

    int rcnt = 0;
    bool ret = m_ndb->idxTermMatch_p(
        typ, lang, root,
        [&res, &rcnt, max](const string& term,
                    Xapian::termcount cf, Xapian::doccount tf) {
            res.entries.push_back(TermMatchEntry(term, cf, tf));
            // The problem with truncating here is that this is done
            // alphabetically and we may not keep the most frequent 
            // terms. OTOH, not doing it may stall the program if
            // we are walking the whole term list. We compromise
            // by cutting at 2*max
            if (max > 0 && ++rcnt >= 2*max)
                return false;
            return true;
        }, prefix);

    return ret;
}

/** Term list walking. */
class TermIter {
public:
    Xapian::TermIterator it;
    Xapian::Database db;
};
TermIter *Db::termWalkOpen()
{
    if (!m_ndb || !m_ndb->m_isopen)
        return 0;
    TermIter *tit = new TermIter;
    if (tit) {
        tit->db = m_ndb->xrdb;
        XAPTRY(tit->it = tit->db.allterms_begin(), tit->db, m_reason);
        if (!m_reason.empty()) {
            LOGERR("Db::termWalkOpen: xapian error: " << m_reason << "\n");
            return 0;
        }
    }
    return tit;
}
bool Db::termWalkNext(TermIter *tit, string &term)
{
    XAPTRY(
        if (tit && tit->it != tit->db.allterms_end()) {
            term = *(tit->it)++;
            return true;
        }
        , tit->db, m_reason);

    if (!m_reason.empty()) {
        LOGERR("Db::termWalkOpen: xapian error: " << m_reason << "\n");
    }
    return false;
}
void Db::termWalkClose(TermIter *tit)
{
    try {
        delete tit;
    } catch (...) {}
}

bool Db::termExists(const string& word)
{
    if (!m_ndb || !m_ndb->m_isopen)
        return 0;

    XAPTRY(if (!m_ndb->xrdb.term_exists(word)) return false,
           m_ndb->xrdb, m_reason);

    if (!m_reason.empty()) {
        LOGERR("Db::termWalkOpen: xapian error: " << m_reason << "\n");
        return false;
    }
    return true;
}

bool Db::stemDiffers(const string& lang, const string& word,
                     const string& base)
{
    Xapian::Stem stemmer(lang);
    if (!stemmer(word).compare(stemmer(base))) {
        LOGDEB2("Rcl::Db::stemDiffers: same for " << word << " and " <<
                base << "\n");
        return false;
    }
    return true;
}

} // End namespace Rcl
