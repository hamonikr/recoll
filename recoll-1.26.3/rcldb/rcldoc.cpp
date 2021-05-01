/* Copyright (C) 2007-2018 J.F.Dockes
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

#include "rcldoc.h"
#include "log.h"
#include "rclutil.h"

using namespace std;

namespace Rcl {
const string Doc::keyabs("abstract");
const string Doc::keyapptg("rclaptg");
const string Doc::keyau("author");
const string Doc::keybcknd("rclbes");
const string Doc::keybght("beagleHitType");
const string Doc::keycc("collapsecount");
const string Doc::keychildurl("childurl");
const string Doc::keydmt("dmtime");
const string Doc::keyds("dbytes");
const string Doc::keyfmt("fmtime");
const string Doc::keyfn("filename");
const string Doc::keytcfn("containerfilename");
const string Doc::keyfs("fbytes");
const string Doc::keyipt("ipath");
const string Doc::keykw("keywords");
const string Doc::keymd5("md5");
const string Doc::keymt("mtime");
const string Doc::keyoc("origcharset");
const string Doc::keypcs("pcbytes");
const string Doc::keyrr("relevancyrating");
const string Doc::keysig("sig");
const string Doc::keysz("size");
const string Doc::keytp("mtype");
const string Doc::keytt("title");
const string Doc::keyudi("rcludi");
const string Doc::keyurl("url");

void Doc::dump(bool dotext) const
{
    LOGDEB("Rcl::Doc::dump: url: [" << url << "]\n");
    LOGDEB("Rcl::Doc::dump: idxurl: [" << idxurl << "]\n");
    LOGDEB("Rcl::Doc::dump: ipath: [" << ipath << "]\n");
    LOGDEB("Rcl::Doc::dump: mimetype: [" << mimetype << "]\n");
    LOGDEB("Rcl::Doc::dump: fmtime: [" << fmtime << "]\n");
    LOGDEB("Rcl::Doc::dump: dmtime: [" << dmtime << "]\n");
    LOGDEB("Rcl::Doc::dump: origcharset: [" << origcharset << "]\n");
    LOGDEB("Rcl::Doc::dump: syntabs: [" << syntabs << "]\n");
    LOGDEB("Rcl::Doc::dump: pcbytes: [" << pcbytes << "]\n");
    LOGDEB("Rcl::Doc::dump: fbytes: [" << fbytes << "]\n");
    LOGDEB("Rcl::Doc::dump: dbytes: [" << dbytes << "]\n");
    LOGDEB("Rcl::Doc::dump: sig: [" << sig << "]\n");
    LOGDEB("Rcl::Doc::dump: pc: [" << pc << "]\n");
    LOGDEB("Rcl::Doc::dump: xdocid: [" << (unsigned long)xdocid << "]\n");
    for (const auto& e : meta) {
        LOGDEB("Rcl::Doc::dump: meta[" << e.first <<"]->["<< e.second << "]\n");
    }
    if (dotext)
        LOGDEB("Rcl::Doc::dump: text: \n[" << text << "]\n");
}

// Copy ensuring no shared string data, for threading issues.
void Doc::copyto(Doc *d) const
{
    d->url.assign(url.begin(), url.end());
    d->idxurl.assign(idxurl.begin(), idxurl.end());
    d->idxi = idxi;
    d->ipath.assign(ipath.begin(), ipath.end());
    d->mimetype.assign(mimetype.begin(), mimetype.end());
    d->fmtime.assign(fmtime.begin(), fmtime.end());
    d->dmtime.assign(dmtime.begin(), dmtime.end());
    d->origcharset.assign(origcharset.begin(), origcharset.end());
    map_ss_cp_noshr(meta, &d->meta);
    d->syntabs = syntabs;
    d->pcbytes.assign(pcbytes.begin(), pcbytes.end());
    d->fbytes.assign(fbytes.begin(), fbytes.end());
    d->dbytes.assign(dbytes.begin(), dbytes.end());
    d->sig.assign(sig.begin(), sig.end());
    d->text.assign(text.begin(), text.end());
    d->pc = pc;
    d->xdocid = xdocid;
    d->haspages = haspages;
    d->haschildren = haschildren;
    d->onlyxattr = onlyxattr;
}

static const string cstr_fileu("file://");
bool docsToPaths(vector<Rcl::Doc> &docs, vector<string> &paths)
{
    for (const auto& idoc : docs) {
        string backend;
        idoc.getmeta(Rcl::Doc::keybcknd, &backend);

        // This only makes sense for file system files: beagle docs are
        // always up to date because they can't be updated in the cache,
        // only added/removed. Same remark as made inside internfile, we
        // need a generic way to handle backends.
        if (!backend.empty() && backend.compare("FS"))
            continue;

        // Filesystem document. The url has to be like file://
        if (idoc.url.find(cstr_fileu) != 0) {
            LOGERR("idx::docsToPaths: FS backend and non fs url: [" <<
                   idoc.url << "]\n");
            continue;
        }
        paths.push_back(idoc.url.substr(7, string::npos));
    }
    return true;
}

}
