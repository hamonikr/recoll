/* Copyright (C) 2016 J.F.Dockes
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

#include <string>
#include <vector>
#include <iostream>

#include "exefetcher.h"

#include "log.h"
#include "pathut.h"
#include "rclconfig.h"
#include "execmd.h"
#include "rcldoc.h"

using namespace std;

class EXEDocFetcher::Internal {
public:
    string bckid;
    vector<string> sfetch;
    vector<string> smkid;
    bool docmd(const vector<string>& cmd, const Rcl::Doc& idoc, string& out) {
        ExecCmd ecmd;
        // We're always called for preview (or Open)
        ecmd.putenv("RECOLL_FILTER_FORPREVIEW=yes");
        string udi;
        idoc.getmeta(Rcl::Doc::keyudi, &udi);
        vector<string> args(cmd);
        args.push_back(udi);
        args.push_back(idoc.url);
        args.push_back(idoc.ipath);
        int status = ecmd.doexec1(args, 0, &out);
        if (status == 0) {
            LOGDEB("EXEDocFetcher::Internal: got [" << out << "]\n");
            return true;
        } else {
            LOGERR("EXEDOcFetcher::fetch: " << bckid << ": " <<
                   stringsToString(cmd) << " failed for " << udi << " " <<
                   idoc.url << " " << idoc.ipath << "\n");
            return false;
        }
    }
};

EXEDocFetcher::EXEDocFetcher(const EXEDocFetcher::Internal& _m)
{
    m = new Internal(_m);
    LOGDEB("EXEDocFetcher::EXEDocFetcher: fetch is " <<
           stringsToString(m->sfetch) << "\n");
}

bool EXEDocFetcher::fetch(RclConfig* cnf, const Rcl::Doc& idoc, RawDoc& out)
{
    out.kind = RawDoc::RDK_DATADIRECT;
    return m->docmd(m->sfetch, idoc, out.data);
}

bool EXEDocFetcher::makesig(RclConfig* cnf, const Rcl::Doc& idoc, string& sig)
{
    return m->docmd(m->smkid, idoc, sig);
}

// Lookup bckid in the config and create an appropriate fetcher.
std::unique_ptr<EXEDocFetcher> exeDocFetcherMake(RclConfig *config,
                                                 const string& bckid)
{
    // The config we only read once, not gonna change.
    static ConfSimple *bconf;
    if (!bconf) {
        string bconfname = path_cat(config->getConfDir(), "backends");
        LOGDEB("exeDocFetcherMake: using config in " << bconfname << "\n");
        bconf = new ConfSimple(bconfname.c_str(), true);
        if (!bconf->ok()) {
            delete bconf;
            bconf = 0;
            LOGDEB("exeDocFetcherMake: bad/no config: " << bconfname << "\n");
            return 0;
        }
    }

    EXEDocFetcher::Internal m;
    m.bckid = bckid;

    string sfetch;
    if (!bconf->get("fetch", sfetch, bckid) || sfetch.empty()) {
        LOGERR("exeDocFetcherMake: no 'fetch' for [" << bckid << "]\n");
        return 0;
    }
    stringToStrings(sfetch, m.sfetch);
    // We look up the command as we do for filters for now
    m.sfetch[0] = config->findFilter(m.sfetch[0]);
    if (!path_isabsolute(m.sfetch[0])) {
        LOGERR("exeDocFetcherMake: " << m.sfetch[0] <<
               " not found in exec path or filters dir\n");
        return 0;
    }

    string smkid;
    if (!bconf->get("makesig", smkid, bckid) || smkid.empty()) {
        LOGDEB("exeDocFetcherMake: no 'makesig' for [" << bckid << "]\n");
        return 0;
    }
    stringToStrings(smkid, m.smkid);
    m.smkid[0] = config->findFilter(m.smkid[0]);
    if (!path_isabsolute(m.smkid[0])) {
        LOGERR("exeDocFetcherMake: " << m.smkid[0] <<
               " not found in exec path or filters dir\n");
        return 0;
    }
    return std::unique_ptr<EXEDocFetcher>(new EXEDocFetcher(m));
}
