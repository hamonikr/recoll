/* Copyright (C) 2005 J.F.Dockes 
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

/**
 * Management of the auxiliary databases listing stems and their expansion 
 * terms
 */

#include "autoconfig.h"

#include "safeunistd.h"

#include <algorithm>
#include <map>
#include <iostream>
#include <string>
using namespace std;

#include <xapian.h>

#include "stemdb.h"
#include "log.h"
#include "smallut.h"
#include "synfamily.h"
#include "unacpp.h"
#include "rclconfig.h"

namespace Rcl {

/**
 * Expand for one or several languages
 */
bool StemDb::stemExpand(const std::string& langs, const std::string& _term,
			vector<string>& result)
{
    vector<string> llangs;
    stringToStrings(langs, llangs);
    
    // The stemdb keys may have kept their diacritics or not but they
    // are always lower-case. It would be more logical for the term
    // transformers to perform before doing the stemming, but this
    // would be inefficient when there are several stemming languages
    string term;
    unacmaybefold(_term, term, "UTF-8", UNACOP_FOLD);

    for (vector<string>::const_iterator it = llangs.begin();
	 it != llangs.end(); it++) {
	SynTermTransStem stemmer(*it);
	XapComputableSynFamMember expander(getdb(), synFamStem, *it, &stemmer);
	(void)expander.synExpand(term, result);
    }

    if (!o_index_stripchars) {
	string unac;
	unacmaybefold(term, unac, "UTF-8", UNACOP_UNAC);
	// Expand the unaccented stem, using the unaccented stem
	// db. Because it's a different db, We need to do it even if
	// the input has no accent (unac == term)
	for (vector<string>::const_iterator it = llangs.begin();
	     it != llangs.end(); it++) {
	    SynTermTransStem stemmer(*it);
	    XapComputableSynFamMember expander(getdb(), synFamStemUnac, 
					       *it, &stemmer);
	    (void)expander.synExpand(unac, result);
	}
    }

    if (result.empty())
	result.push_back(term);

    sort(result.begin(), result.end());
    vector<string>::iterator uit = unique(result.begin(), result.end());
    result.resize(uit - result.begin());
    LOGDEB1("stemExpand:"  << (langs) << ": "  << (term) << " ->  "  << (stringsToString(result)) << "\n" );
    return true;
}


}

