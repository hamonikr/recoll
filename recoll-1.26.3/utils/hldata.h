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
#ifndef _hldata_h_included_
#define _hldata_h_included_

#include <vector>
#include <string>
#include <set>
#include <unordered_map>

/** Store data about user search terms and their expansions. This is used
 * mostly for highlighting result text and walking the matches, generating 
 * spelling suggestions.
 */
struct HighlightData {
    /** The user terms, excluding those with wildcards. This list is
     * intended for orthographic suggestions so the terms are always
     * lowercased, unaccented or not depending on the type of index 
     * (as the spelling dictionary is generated from the index terms).
     */
    std::set<std::string> uterms;

    /** The db query terms linked to the uterms entry they were expanded from. 
     * This is used for aggregating term stats when generating snippets (for 
     * choosing the best terms, allocating slots, etc. )
     */
    std::unordered_map<std::string, std::string> terms;

    /** The original user terms-or-groups. This is for display
     * purposes: ie when creating a menu to look for a specific
     * matched group inside a preview window. We want to show the
     * user-entered data in the menu, not some transformation, so
     * these are always raw, diacritics and case preserved.
     */
    std::vector<std::vector<std::string> > ugroups;

    /** Processed/expanded terms and groups. Used for looking for
     * regions to highlight. A group can be a PHRASE or NEAR entry
     * Terms are just groups with 1 entry. All
     * terms are transformed to be compatible with index content
     * (unaccented and lowercased as needed depending on
     * configuration), and the list may include values
     * expanded from the original terms by stem or wildcard expansion.
     */
    struct TermGroup {
        // We'd use an union but no can do
        std::string term;
        std::vector<std::vector<std::string> > orgroups;
        int slack{0};

        /* Index into ugroups. As a user term or group may generate
         * many processed/expanded terms or groups, this is how we
         * relate an expansion to its source (used, e.g. for
         * generating anchors for walking search matches in the
         * preview window). */
        size_t grpsugidx{0};
        enum TGK {TGK_TERM, TGK_NEAR, TGK_PHRASE};
        TGK kind{TGK_TERM};
    };
    std::vector<TermGroup> index_term_groups;

    void clear() {
	uterms.clear();
	ugroups.clear();
	index_term_groups.clear();
    }
    void append(const HighlightData&);

    // Print (debug)
    std::string toString() const;
};

/* The following is used by plaintorich.cpp for finding zones to
   highlight and by rclabsfromtext.cpp to choose fragments for the
   abstract */

struct GroupMatchEntry {
    // Start/End byte offsets in the document text
    std::pair<int, int> offs;
    // Index of the search group this comes from: this is to relate a 
    // match to the original user input.
    size_t grpidx;
    GroupMatchEntry(int sta, int sto, size_t idx) 
        : offs(sta, sto), grpidx(idx) {
    }
};

// Find NEAR or PHRASE matches for one group of terms.
//
// @param hldata User query expansion descriptor (see above). We only use
//      the index_term_groups entry
//
// @param grpidx Index in hldata.index_term_groups for the group we
//     process. This is used by us to get the terms, group type
//     (phrase/near) and slacks. We also set it in the output
//     GroupMatchEntry structures to allow the caller to link a match
//     with a specific user input (e.g. for walking the match in the
//     GUI preview)
//
// @param inplists Position lists for the the group terms. This is the
//     data used to look for matches.
//
// @param gpostobytes Translation of term position to start/end byte
//     offsets. This is used to translate term positions to byte
//     positions in the output, for ease of use by caller.
//
// @param[out] tboffs Found matches. Each match has a begin and end
//     byte offset and an index linking to the origin data in the
//     HighlightData structure.
extern bool matchGroup(
    const HighlightData& hldata,
    unsigned int grpidx,
    const std::unordered_map<std::string, std::vector<int>>& inplists,
    const std::unordered_map<int, std::pair<int,int>>& gpostobytes,
    std::vector<GroupMatchEntry>& tboffs
    );

#endif /* _hldata_h_included_ */
