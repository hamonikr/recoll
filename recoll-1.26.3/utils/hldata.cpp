/* Copyright (C) 2017-2019 J.F.Dockes
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

#include "hldata.h"

#include <algorithm>
#include <limits.h>

#include "log.h"
#include "smallut.h"

using std::string;
using std::unordered_map;
using std::vector;
using std::pair;

#undef DEBUGGROUPS
#ifdef DEBUGGROUPS
#define LOGRP LOGINF
#else
#define LOGRP LOGDEB1
#endif

// Combined position list for or'd terms
struct OrPList {
    void addplist(const string& term, const vector<int>* pl) {
        terms.push_back(term);
        plists.push_back(pl);
        indexes.push_back(0);
        totalsize += pl->size();
    }

    // Returns -1 for eof, else the next smallest value in the
    // combined lists, according to the current indexes.
    int value() {
        int minval = INT_MAX;
        int minidx = -1;
        for (unsigned ii = 0; ii < indexes.size(); ii++) {
            const vector<int>& pl(*plists[ii]);
            if (indexes[ii] >= pl.size())
                continue; // this list done
            if (pl[indexes[ii]] < minval) {
                minval = pl[indexes[ii]];
                minidx = ii;
            }
        }
        if (minidx != -1) {
            LOGRP("OrPList::value() -> " << minval << " for " <<
                  terms[minidx] << "\n");
            currentidx = minidx;
            return minval;
        } else {
            LOGRP("OrPList::value(): EOL for " << stringsToString(terms)<<"\n");
            return -1;
        }
    }

    int next() {
        if (currentidx != -1) {
            indexes[currentidx]++;
        }
        return value();
    }
    
    int size() const {
        return totalsize;
    }
    void rewind() {
        for (auto& idx : indexes) {
            idx = 0;
        }
        currentidx = -1;
    }

    vector<const vector<int>*> plists;
    vector<unsigned int> indexes;
    vector<string> terms;
    int currentidx{-1};
    int totalsize{0};
};

static inline void setWinMinMax(int pos, int& sta, int& sto)
{
    if (pos < sta) {
        sta = pos;
    }
    if (pos > sto) {
        sto = pos;
    }
}

/*
 * @param window the total width for the "near" area, in positions.

 * @param plists the position vectors for the terms. The array is
 *    sorted shorted first for optimization. The function does a
 *    recursive call on the next array if the match is still possible
 *    after dealing with the current one

 * @param plist_idx the index for the position list we will work with.
 * @param min, max the current minimum and maximum term positions.
 * @param[output] sp, ep, the start and end positions of the found match.
 * @param minpos  Highest end of a found match. While looking for
 *   further matches, we don't want the search to extend before
 *   this, because it does not make sense for highlight regions to
 *   overlap.
 * @param isphrase if true, the position lists are in term order, and
 *     we only look for the next match beyond the current window top.
 */
static bool do_proximity_test(
    const int window, vector<OrPList>& plists,
    unsigned int plist_idx, int min, int max, int *sp, int *ep, int minpos,
    bool isphrase)
{
    // Overlap interdiction: possibly adjust window start by input minpos
    int actualminpos = isphrase ? max + 1 : max + 1 - window;
    if (actualminpos < minpos)
        actualminpos = minpos;
    LOGRP("do_prox_test: win " << window << " plist_idx " << plist_idx <<
          " min " <<  min << " max " << max << " minpos " << minpos <<
          " isphrase " << isphrase << " actualminpos " << actualminpos << "\n");

    // Find 1st position bigger than window start. A previous call may
    // have advanced the index, so we begin by retrieving the current
    // value.
    int nextpos = plists[plist_idx].value();
    while (nextpos != -1 && nextpos < actualminpos)
        nextpos = plists[plist_idx].next();

    // Look for position inside window. If not found, no match. If
    // found: if this is the last list we're done, else recurse on
    // next list after adjusting the window
    while (nextpos != -1) {
        if (nextpos > min + window - 1) {
            return false;
        }
        if (plist_idx + 1 == plists.size()) {
            // We already checked pos > min, now we also have pos <
            // max, and we are the last list: done: set return values.
            setWinMinMax(nextpos, *sp, *ep);
            return true;
        }
        setWinMinMax(nextpos, min, max);
        if (do_proximity_test(window, plists, plist_idx + 1,
                              min, max, sp, ep, minpos, isphrase)) {
            return true;
        }
        nextpos = plists[plist_idx].next();
    }
    return false;
}


// Find matches for one group of terms
bool matchGroup(const HighlightData& hldata,
                unsigned int grpidx,
                const unordered_map<string, vector<int>>& inplists,
                const unordered_map<int, pair<int,int>>& gpostobytes,
                vector<GroupMatchEntry>& tboffs)
{

    const auto& tg(hldata.index_term_groups[grpidx]);
    bool isphrase =  tg.kind == HighlightData::TermGroup::TGK_PHRASE;
    string allplterms;
    for (const auto& entry:inplists) {
        allplterms += entry.first + " ";
    }
    LOGRP("matchGroup: isphrase " << isphrase <<
          ". Have plists for [" << allplterms << "]\n");
    LOGRP("matchGroup: hldata: " << hldata.toString() << std::endl);
    
    int window = int(tg.orgroups.size() + tg.slack);
    // The position lists we are going to work with. We extract them from the 
    // (string->plist) map
    vector<OrPList> orplists;

    // Find the position list for each term in the group and build the
    // combined lists for the term or groups (each group is the result
    // of the exansion of one user term). It is possible that this
    // particular group was not actually matched by the search, so
    // that some terms are not found, in which case we bail out.
    for (const auto& group : tg.orgroups) {
        orplists.push_back(OrPList());
        for (const auto& term : group) {
            const auto pl = inplists.find(term);
            if (pl == inplists.end()) {
                LOGRP("TextSplitPTR::matchGroup: term [" << term <<
                      "] not found in plists\n");
                continue;
            }
            orplists.back().addplist(pl->first, &(pl->second));
        }
        if (orplists.back().plists.empty()) {
            LOGRP("No positions list found for group " <<
                   stringsToString(group) << std::endl);
            orplists.pop_back();
        }
    }

    // I think this can't actually happen, was useful when we used to
    // prune the groups, but doesn't hurt.
    if (orplists.size() < 2) {
        LOGRP("TextSplitPTR::matchGroup: no actual groups found\n");
        return false;
    }

    if (!isphrase) {
        // Sort the positions lists so that the shorter is first
        std::sort(orplists.begin(), orplists.end(),
                  [](const OrPList& a, const OrPList& b) -> bool {
                      return a.size() < b.size();
                  }
            );
    }

    // Minpos is the highest end of a found match. While looking for
    // further matches, we don't want the search to extend before
    // this, because it does not make sense for highlight regions to
    // overlap
    int minpos = 0;
    // Walk the shortest plist and look for matches
    int pos;
    while ((pos = orplists[0].next()) != -1) {
        int sta = INT_MAX, sto = 0;
        LOGDEB2("MatchGroup: Testing at pos " << pos << "\n");
        if (do_proximity_test(
                window, orplists, 1, pos, pos, &sta, &sto, minpos, isphrase)) {
            setWinMinMax(pos, sta, sto);
            LOGRP("TextSplitPTR::matchGroup: MATCH termpos [" << sta <<
                    "," << sto << "]\n"); 
            minpos = sto + 1;
            // Translate the position window into a byte offset window
            auto i1 =  gpostobytes.find(sta);
            auto i2 =  gpostobytes.find(sto);
            if (i1 != gpostobytes.end() && i2 != gpostobytes.end()) {
                LOGDEB2("TextSplitPTR::matchGroup: pushing bpos " <<
                        i1->second.first << " " << i2->second.second << "\n");
                tboffs.push_back(GroupMatchEntry(i1->second.first, 
                                                 i2->second.second, grpidx));
            } else {
                LOGDEB0("matchGroup: no bpos found for " << sta << " or "
                        << sto << "\n");
            }
        } else {
            LOGRP("matchGroup: no group match found at this position\n");
        }
    }

    return !tboffs.empty();
}

string HighlightData::toString() const
{
    string out;
    out.append("\nUser terms (orthograph): ");
    for (const auto& term : uterms) {
        out.append(" [").append(term).append("]");
    }
    out.append("\nUser terms to Query terms:");
    for (const auto& entry: terms) {
        out.append("[").append(entry.first).append("]->[");
        out.append(entry.second).append("] ");
    }
    out.append("\nGroups: ");
    char cbuf[200];
    sprintf(cbuf, "index_term_groups size %d ugroups size %d",
            int(index_term_groups.size()), int(ugroups.size()));
    out.append(cbuf);

    size_t ugidx = (size_t) - 1;
    for (HighlightData::TermGroup tg : index_term_groups) {
        if (ugidx != tg.grpsugidx) {
            ugidx = tg.grpsugidx;
            out.append("\n(");
            for (unsigned int j = 0; j < ugroups[ugidx].size(); j++) {
                out.append("[").append(ugroups[ugidx][j]).append("] ");
            }
            out.append(") ->");
        }
        if (tg.kind == HighlightData::TermGroup::TGK_TERM) {
            out.append(" <").append(tg.term).append(">");
        } else {
            out.append(" {");
            for (unsigned int j = 0; j < tg.orgroups.size(); j++) {
                out.append(" {");
                for (unsigned int k = 0; k < tg.orgroups[j].size(); k++) {
                    out.append("[").append(tg.orgroups[j][k]).append("]");
                }
                out.append("}");
            }
            sprintf(cbuf, "%d", tg.slack);
            out.append("}").append(cbuf);
        }
    }
    out.append("\n");
    return out;
}

void HighlightData::append(const HighlightData& hl)
{
    uterms.insert(hl.uterms.begin(), hl.uterms.end());
    terms.insert(hl.terms.begin(), hl.terms.end());
    size_t ugsz0 = ugroups.size();
    ugroups.insert(ugroups.end(), hl.ugroups.begin(), hl.ugroups.end());

    size_t itgsize = index_term_groups.size();
    index_term_groups.insert(index_term_groups.end(),
                             hl.index_term_groups.begin(),
                             hl.index_term_groups.end());
    // Adjust the grpsugidx values for the newly inserted entries
    for (unsigned int idx = itgsize; idx < index_term_groups.size(); idx++) {
        index_term_groups[idx].grpsugidx += ugsz0;
    }
}
