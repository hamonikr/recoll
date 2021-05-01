/* Copyright (C) 2013 J.F.Dockes
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

#include "autoconfig.h"

#include <string>
using namespace std;

#include <xapian.h>

#include "log.h"
#include "rcldb.h"
#include "rcldb_p.h"
#include "xmacros.h"
#include "md5ut.h"
#include "searchdata.h"
#include "rclquery.h"

namespace Rcl {

/** Retrieve the dups of a given document. The input has to be a query result
  * because we use the xdocid. We get the md5 from this, then the dups */
bool Db::docDups(const Doc& idoc, vector<Doc>& odocs)
{
    if (m_ndb == 0) {
	LOGERR("Db::docDups: no db\n" );
	return false;
    }
    if (idoc.xdocid == 0) {
	LOGERR("Db::docDups: null xdocid in input doc\n" );
	return false;
    }
    // Get the xapian doc
    Xapian::Document xdoc;
    XAPTRY(xdoc = m_ndb->xrdb.get_document(Xapian::docid(idoc.xdocid)), 
	   m_ndb->xrdb, m_reason);
    if (!m_reason.empty()) {
	LOGERR("Db::docDups: xapian error: "  << (m_reason) << "\n" );
	return false;
    }

    // Get the md5
    string digest;
    XAPTRY(digest = xdoc.get_value(VALUE_MD5), m_ndb->xrdb, m_reason);
    if (!m_reason.empty()) {
	LOGERR("Db::docDups: xapian error: "  << (m_reason) << "\n" );
	return false;
    }
    if (digest.empty()) {
	LOGDEB("Db::docDups: doc has no md5\n" );
	return false;
    }
    string md5;
    MD5HexPrint(digest, md5);

    SearchData *sdp = new SearchData();
    std::shared_ptr<SearchData> sd(sdp);
    SearchDataClauseSimple *sdc = 
	new SearchDataClauseSimple(SCLT_AND, md5, "rclmd5");
    sdc->addModifier(SearchDataClause::SDCM_CASESENS);
    sdc->addModifier(SearchDataClause::SDCM_DIACSENS);
    sd->addClause(sdc);
    Query query(this);
    query.setCollapseDuplicates(0);
    if (!query.setQuery(sd)) {
	LOGERR("Db::docDups: setQuery failed\n" );
	return false;
    }
    int cnt = query.getResCnt();
    for (int i = 0; i < cnt; i++) {
	Doc doc;
	if (!query.getDoc(i, doc)) {
	    LOGERR("Db::docDups: getDoc failed at "  << (i) << " (cnt "  << (cnt) << ")\n" );
	    return false;
	}
	odocs.push_back(doc);
    }
    return true;
}

#if 0
    {
	vector<Doc> dups;
	bool ret;
	LOGDEB("DOCDUPS\n" );
	ret = m_db->docDups(doc, dups);
	if (!ret) {
	    LOGDEB("docDups failed\n" );
	} else if (dups.size() == 1) {
	    LOGDEB("No dups\n" );
	} else {
	    for (unsigned int i = 0; i < dups.size(); i++) {
		LOGDEB("Dup: "  << (dups[i].url) << "\n" );
	    }
	}
    }
#endif

}

