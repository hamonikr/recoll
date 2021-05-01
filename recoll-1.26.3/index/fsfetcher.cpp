/* Copyright (C) 2012-2019 J.F.Dockes
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

#include <errno.h>
#include "safesysstat.h"

#include "log.h"
#include "cstr.h"
#include "fetcher.h"
#include "fsfetcher.h"
#include "fsindexer.h"
#include "pathut.h"

using std::string;

static DocFetcher::Reason urltopath(RclConfig* cnf, const Rcl::Doc& idoc,
                                    string& fn, struct stat& st)
{
    // The url has to be like file://
    fn = fileurltolocalpath(idoc.url);
    if (fn.empty()) {
        LOGERR("FSDocFetcher::fetch/sig: non fs url: [" << idoc.url << "]\n");
        return DocFetcher::FetchOther;
    }
    cnf->setKeyDir(path_getfather(fn));
    bool follow = false;
    cnf->getConfParam("followLinks", &follow);

    if (path_fileprops(fn, &st, follow) < 0) {
        LOGERR("FSDocFetcher::fetch: stat errno " << errno << " for [" << fn
               << "]\n");
        return DocFetcher::FetchNotExist;
    }
    return DocFetcher::FetchOk;
}

bool FSDocFetcher::fetch(RclConfig* cnf, const Rcl::Doc& idoc, RawDoc& out)
{
    string fn;
    if (urltopath(cnf, idoc, fn, out.st) != DocFetcher::FetchOk)
        return false;
    out.kind = RawDoc::RDK_FILENAME;
    out.data = fn;
    return true;
}
    
bool FSDocFetcher::makesig(RclConfig* cnf, const Rcl::Doc& idoc, string& sig)
{
    string fn;
    struct stat st;
    if (urltopath(cnf, idoc, fn, st) != DocFetcher::FetchOk)
        return false;
    FsIndexer::makesig(&st, sig);
    return true;
}

DocFetcher::Reason FSDocFetcher::testAccess(RclConfig* cnf, const Rcl::Doc& idoc)
{
    string fn;
    struct stat st;
    DocFetcher::Reason reason = urltopath(cnf, idoc, fn, st);
    if (reason != DocFetcher::FetchOk) {
        return reason;
    }
    if (!path_readable(fn)) {
        return DocFetcher::FetchNoPerm;
    }
    // We have no way to know if the file is fully readable without
    // trying (local Windows locks), which would take too much time.
    return DocFetcher::FetchOther;
}
