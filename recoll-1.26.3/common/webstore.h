/* Copyright (C) 2009 J.F.Dockes
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
#ifndef _webstore_h_included_
#define _webstore_h_included_

#include <string>

class RclConfig;
namespace Rcl {
    class Db;
    class Doc;
}
class CirCache;

/**
 * Manage the CirCache for the Web Queue indexer. Separated from the main
 * indexer code because it's also used for querying (getting the data for a
 * preview 
 */
class WebStore {
public:
    WebStore(RclConfig *config);
    ~WebStore();

    bool getFromCache(const std::string& udi, Rcl::Doc &doc, std::string& data,
                      std::string *hittype = 0);
    // We could write proxies for all the circache ops, but why bother?
    CirCache *cc() {return m_cache;}

private:
    CirCache *m_cache;
};

extern const std::string cstr_bgc_mimetype;

#endif /* _webstore_h_included_ */
