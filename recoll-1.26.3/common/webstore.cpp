/* Copyright (C) 2011 J.F.Dockes
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

#include "webstore.h"

#include <stdint.h>

#include "cstr.h"
#include "circache.h"
#include "log.h"
#include "rclconfig.h"
#include "pathut.h"
#include "rcldoc.h"

const string cstr_bgc_mimetype("mimetype");

WebStore::WebStore(RclConfig *cnf) 
{
    string ccdir = cnf->getWebcacheDir();

    int maxmbs = 40;
    cnf->getConfParam("webcachemaxmbs", &maxmbs);
    if ((m_cache = new CirCache(ccdir)) == 0) {
	LOGERR("WebStore: cant create CirCache object\n" );
	return;
    }
    if (!m_cache->create(int64_t(maxmbs)*1000*1024, CirCache::CC_CRUNIQUE)) {
	LOGERR("WebStore: cache file creation failed: " <<
               m_cache->getReason() << "\n");
	delete m_cache;
	m_cache = 0;
	return;
    }
}

WebStore::~WebStore()
{
    delete m_cache;
}

// Read  document from cache. Return the metadata as an Rcl::Doc
// @param htt Web Hit Type 
bool WebStore::getFromCache(const string& udi, Rcl::Doc &dotdoc, 
				    string& data, string *htt)
{
    string dict;

    if (m_cache == 0) {
	LOGERR("WebStore::getFromCache: cache is null\n");
	return false;
    }
    if (!m_cache->get(udi, dict, &data)) {
	LOGDEB("WebStore::getFromCache: get failed\n");
	return false;
    }

    ConfSimple cf(dict, 1);
    
    if (htt)
        cf.get(Rcl::Doc::keybght, *htt, cstr_null);

    // Build a doc from saved metadata 
    cf.get(cstr_url, dotdoc.url, cstr_null);
    cf.get(cstr_bgc_mimetype, dotdoc.mimetype, cstr_null);
    cf.get(cstr_fmtime, dotdoc.fmtime, cstr_null);
    cf.get(cstr_fbytes, dotdoc.pcbytes, cstr_null);
    dotdoc.sig.clear();
    vector<string> names = cf.getNames(cstr_null);
    for (vector<string>::const_iterator it = names.begin();
         it != names.end(); it++) {
        cf.get(*it, dotdoc.meta[*it], cstr_null);
    }
    dotdoc.meta[Rcl::Doc::keyudi] = udi;
    return true;
}

