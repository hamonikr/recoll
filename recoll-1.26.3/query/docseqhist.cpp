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
#include "docseqhist.h"

#include <stdio.h>
#include <math.h>
#include <time.h>

#include <cmath>
using std::vector;

#include "rcldb.h"
#include "fileudi.h"
#include "base64.h"
#include "log.h"
#include "smallut.h"

// Encode document history entry: 
// U + Unix time + base64 of udi
// The U distinguishes udi-based entries from older fn+ipath ones
bool RclDHistoryEntry::encode(string& value)
{
    string budi, bdir;
    base64_encode(udi, budi);
    base64_encode(dbdir, bdir);
    value = string("V ") + lltodecstr(unixtime) + " " + budi + " " + bdir;
    return true;
}

// Decode. We support historical entries which were like "time b64fn [b64ipath]"
// Previous entry format is "U time b64udi"
// Current entry format "V time b64udi [b64dir]"
bool RclDHistoryEntry::decode(const string &value)
{
    vector<string> vall;
    stringToStrings(value, vall);

    vector<string>::const_iterator it = vall.begin();
    udi.clear();
    dbdir.clear();
    string fn, ipath;
    switch (vall.size()) {
    case 2:
        // Old fn+ipath, null ipath case 
        unixtime = atoll((*it++).c_str());
        base64_decode(*it++, fn);
        break;
    case 3:
        if (!it->compare("U") || !it->compare("V")) {
            // New udi-based entry, no dir
            it++;
            unixtime = atoll((*it++).c_str());
            base64_decode(*it++, udi);
        } else {
            // Old fn + ipath. We happen to know how to build an udi
            unixtime = atoll((*it++).c_str());
            base64_decode(*it++, fn);
            base64_decode(*it, ipath);
        }
        break;
    case 4:
        // New udi-based entry, with directory
        it++;
        unixtime = atoll((*it++).c_str());
        base64_decode(*it++, udi);
        base64_decode(*it++, dbdir);
        break;
    default: 
        return false;
    }

    if (!fn.empty()) {
        // Old style entry found, make an udi, using the fs udi maker
        make_udi(fn, ipath, udi);
    }
    LOGDEB1("RclDHistoryEntry::decode: udi ["  << udi << "] dbdir [" <<
            dbdir << "]\n");
    return true;
}

bool RclDHistoryEntry::equal(const DynConfEntry& other)
{
    const RclDHistoryEntry& e = dynamic_cast<const RclDHistoryEntry&>(other);
    return e.udi == udi && e.dbdir == dbdir;
}

bool historyEnterDoc(Rcl::Db *db, RclDynConf *dncf, const Rcl::Doc& doc)
{
    string udi;
    if (db && doc.getmeta(Rcl::Doc::keyudi, &udi)) {
        std::string dbdir =  db->whatIndexForResultDoc(doc);
        LOGDEB("historyEnterDoc: [" << udi << ", " << dbdir << "] into " <<
               dncf->getFilename() << "\n");
        RclDHistoryEntry ne(time(0), udi, dbdir);
        RclDHistoryEntry scratch;
        return dncf->insertNew(docHistSubKey, ne, scratch, 200);
    } else {
        LOGDEB("historyEnterDoc: doc has no udi\n");
    }
    return false;
}

vector<RclDHistoryEntry> getDocHistory(RclDynConf* dncf)
{
    return dncf->getEntries<std::vector, RclDHistoryEntry>(docHistSubKey);
}

bool DocSequenceHistory::getDoc(int num, Rcl::Doc &doc, string *sh) 
{
    // Retrieve history list
    if (!m_hist)
	return false;
    if (m_history.empty())
	m_history = getDocHistory(m_hist);

    if (num < 0 || num >= (int)m_history.size())
	return false;

    // We get the history oldest first, but our users expect newest first
    RclDHistoryEntry& hentry = m_history[m_history.size() - 1 - num];

    if (sh) {
	if (m_prevtime < 0 || abs(m_prevtime - hentry.unixtime) > 86400) {
	    m_prevtime = hentry.unixtime;
	    time_t t = (time_t)(hentry.unixtime);
	    *sh = string(ctime(&t));
	    // Get rid of the final \n in ctime
	    sh->erase(sh->length()-1);
	} else {
	    sh->erase();
        }
    }

    bool ret = m_db->getDoc(hentry.udi, hentry.dbdir, doc);
    if (!ret || doc.pc == -1) {
	doc.url = "UNKNOWN";
        doc.ipath = "";
    }

    // Ensure the snippets link won't be shown as it does not make
    // sense (no query terms...)
    doc.haspages = 0;

    return ret;
}

int DocSequenceHistory::getResCnt()
{	
    if (m_history.empty())
	m_history = getDocHistory(m_hist);
    return int(m_history.size());
}

