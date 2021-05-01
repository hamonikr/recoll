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

#include <memory>

#include "log.h"
#include "rclconfig.h"
#include "fetcher.h"
#include "fsfetcher.h"
#include "webqueuefetcher.h"
#include "exefetcher.h"

std::unique_ptr<DocFetcher> docFetcherMake(RclConfig *config,
                                           const Rcl::Doc& idoc)
{
    if (idoc.url.empty()) {
        LOGERR("docFetcherMakeg:: no url in doc!\n" );
        return std::unique_ptr<DocFetcher>();
    }
    string backend;
    idoc.getmeta(Rcl::Doc::keybcknd, &backend);
    if (backend.empty() || !backend.compare("FS")) {
	return std::unique_ptr<DocFetcher>(new FSDocFetcher);
#ifndef DISABLE_WEB_INDEXER
    } else if (!backend.compare("BGL")) {
	return std::unique_ptr<DocFetcher>(new WQDocFetcher);
#endif
    } else {
        std::unique_ptr<DocFetcher> f(exeDocFetcherMake(config, backend));
        if (!f) {
            LOGERR("DocFetcherFactory: unknown backend [" << backend << "]\n");
        }
	return f;
    }
}

