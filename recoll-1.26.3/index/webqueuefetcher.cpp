/* Copyright (C) 2012 J.F.Dockes
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

#include "webqueuefetcher.h"

#include <mutex>

#include "rcldoc.h"
#include "fetcher.h"
#include "log.h"
#include "webstore.h"

using std::string;

// We use a single WebStore object to access the data. We protect it
// against multiple thread access.
static std::mutex o_beagler_mutex;

bool WQDocFetcher::fetch(RclConfig* cnf, const Rcl::Doc& idoc, RawDoc& out)
{
    string udi;
    if (!idoc.getmeta(Rcl::Doc::keyudi, &udi) || udi.empty()) {
	LOGERR("WQDocFetcher:: no udi in idoc\n" );
	return false;
    }
    Rcl::Doc dotdoc;
    {
        std::unique_lock<std::mutex> locker(o_beagler_mutex);
	// Retrieve from our webcache (beagle data). The beagler
	// object is created at the first call of this routine and
	// deleted when the program exits.
	static WebStore o_beagler(cnf);
	if (!o_beagler.getFromCache(udi, dotdoc, out.data)) {
	    LOGINFO("WQDocFetcher::fetch: failed for [" << udi << "]\n");
	    return false;
	}
    }
    if (dotdoc.mimetype.compare(idoc.mimetype)) {
	LOGINFO("WQDocFetcher:: udi [" << udi << "], mimetp mismatch: in: [" <<
                idoc.mimetype << "], bgl [" << dotdoc.mimetype << "]\n");
    }
    out.kind = RawDoc::RDK_DATA;
    return true;
}
    
bool WQDocFetcher::makesig(RclConfig* cnf, const Rcl::Doc& idoc, string& sig)
{
    // Web queue sigs are empty
    sig.clear();
    return true;
}
