/* Copyright (C) 2017-2018 J.F.Dockes
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

#ifndef _IDXSTATUS_H_INCLUDED_
#define _IDXSTATUS_H_INCLUDED_

#include <string>

// Current status of an indexing operation. This is updated in
// $RECOLL_CONFDIR/idxstatus.txt
class DbIxStatus {
 public:
    enum Phase {DBIXS_NONE,
		DBIXS_FILES, DBIXS_PURGE, DBIXS_STEMDB, DBIXS_CLOSING, 
		DBIXS_MONITOR,
		DBIXS_DONE};
    Phase phase;
    std::string fn;   // Last file processed
    int docsdone;  // Documents actually updated
    int filesdone; // Files tested (updated or not)
    int fileerrors; // Failed files (e.g.: missing input handler).
    int dbtotdocs;  // Doc count in index at start
    // Total files in index.This is actually difficult to compute from
    // the index so it's preserved from last indexing
    int totfiles;
    // Is this indexer a monitoring one? This is a permanent value
    // telling if option -m was set, not about what we are currently
    // doing
    bool hasmonitor{false};
    
    void reset() {
	phase = DBIXS_FILES;
	fn.erase();
	docsdone = filesdone = fileerrors = dbtotdocs = totfiles = 0;
    }
    DbIxStatus() {reset();}
};

class RclConfig;
extern void readIdxStatus(RclConfig *config, DbIxStatus &status);

#endif /* _IDXSTATUS_H_INCLUDED_ */
