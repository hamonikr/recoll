/* Copyright (C) 2004 J.F.Dockes 
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

#include <stdio.h>
#include <errno.h>
#ifndef _WIN32
#include <langinfo.h>
#include <sys/param.h>
#else
#include "wincodepages.h"
#endif
#include <limits.h>
#include "safesysstat.h"
#include "safeunistd.h"
#ifdef __FreeBSD__
#include <osreldate.h>
#endif

#include <algorithm>
#include <list>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

#include "cstr.h"
#include "pathut.h"
#include "rclutil.h"
#include "rclconfig.h"
#include "conftree.h"
#include "log.h"
#include "smallut.h"
#include "readfile.h"
#include "fstreewalk.h"
#include "cpuconf.h"
#include "execmd.h"

using namespace std;

// Static, logically const, RclConfig members or module static
// variables are initialized once from the first object build during
// process initialization.

// We default to a case- and diacritics-less index for now
bool o_index_stripchars = true;
// Default to storing the text contents for generating snippets. This
// is only an approximate 10% bigger index and produces nicer
// snippets.
bool o_index_storedoctext = true;

bool o_uptodate_test_use_mtime = false;

string RclConfig::o_localecharset; 
string RclConfig::o_origcwd; 

// We build this once. Used to ensure that the suffix used for a temp
// file of a given MIME type is the FIRST one from the mimemap config
// file. Previously it was the first in alphabetic (map) order, with
// sometimes strange results.
static unordered_map<string, string> mime_suffixes;

// Compute the difference of 1st to 2nd sets and return as plus/minus
// sets. Some args are std::set and some others stringToString()
// strings for convenience
void RclConfig::setPlusMinus(const string& sbase, const set<string>& upd,
                             string& splus, string& sminus)
{
    set<string> base;
    stringToStrings(sbase, base);

    vector<string> diff;
    auto it =
        set_difference(base.begin(), base.end(), upd.begin(), upd.end(),
                       std::inserter(diff, diff.begin()));
    sminus = stringsToString(diff);

    diff.clear();
    it = set_difference(upd.begin(), upd.end(), base.begin(), base.end(),
                        std::inserter(diff, diff.begin()));
    splus = stringsToString(diff);
}

/* Compute result of substracting strminus and adding strplus to base string.
   All string represent sets of values to be computed with stringToStrings() */
static void computeBasePlusMinus(set<string>& res, const string& strbase,
                                 const string& strplus, const string& strminus)
{
    set<string> plus, minus;
    res.clear();
    stringToStrings(strbase, res);
    stringToStrings(strplus, plus);
    stringToStrings(strminus, minus);
    for (auto& it : minus) {
        auto it1 = res.find(it);
        if (it1 != res.end()) {
            res.erase(it1);
        }
    }
    for (auto& it : plus) {
        res.insert(it);
    }
}

bool ParamStale::needrecompute()
{
    LOGDEB1("ParamStale:: needrecompute. parent gen " << parent->m_keydirgen <<
            " mine " << savedkeydirgen << "\n");

    if (!conffile) {
        LOGDEB("ParamStale::needrecompute: conffile not set\n");
        return false;
    }

    bool needrecomp = false;
    if (active && parent->m_keydirgen != savedkeydirgen) {
        savedkeydirgen = parent->m_keydirgen;
        for (unsigned int i = 0; i < paramnames.size(); i++) {
            string newvalue;
            conffile->get(paramnames[i], newvalue, parent->m_keydir);
            LOGDEB1("ParamStale::needrecompute: " << paramnames[i] << " -> " <<
                    newvalue << " keydir " << parent->m_keydir << endl);
            if (newvalue.compare(savedvalues[i])) {
                savedvalues[i] = newvalue;
                needrecomp = true;
            }
        }
    }
    return needrecomp;
}

const string& ParamStale::getvalue(unsigned int i) const
{
    if (i < savedvalues.size()) {
        return savedvalues[i];
    } else {
        static string nll;
        return nll;
    }
}

void ParamStale::init(ConfNull *cnf)
{
    conffile = cnf;
    active = false;
    if (conffile) {
        for (auto& nm : paramnames) {
            if (conffile->hasNameAnywhere(nm)) {
                active = true;
                break;
            }
        }
    }
    savedkeydirgen = -1;
}

bool RclConfig::isDefaultConfig() const
{
    string defaultconf = path_cat(path_homedata(),
                                  path_defaultrecollconfsubdir());
    path_catslash(defaultconf);
    string specifiedconf = path_canon(m_confdir);
    path_catslash(specifiedconf);
    return !defaultconf.compare(specifiedconf);
}


RclConfig::RclConfig(const RclConfig &r) 
    : m_oldstpsuffstate(this, "recoll_noindex"),
      m_stpsuffstate(this, {"noContentSuffixes", "noContentSuffixes+",
                  "noContentSuffixes-"}),
      m_skpnstate(this, {"skippedNames", "skippedNames+", "skippedNames-"}),
      m_onlnstate(this, "onlyNames"),
      m_rmtstate(this, "indexedmimetypes"),
      m_xmtstate(this, "excludedmimetypes"),
      m_mdrstate(this, "metadatacmds")
{
    initFrom(r);
}

RclConfig::RclConfig(const string *argcnf)
    : m_oldstpsuffstate(this, "recoll_noindex"),
      m_stpsuffstate(this, {"noContentSuffixes", "noContentSuffixes+",
                  "noContentSuffixes-"}),
      m_skpnstate(this, {"skippedNames", "skippedNames+", "skippedNames-"}),
      m_onlnstate(this, "onlyNames"),
      m_rmtstate(this, "indexedmimetypes"),
      m_xmtstate(this, "excludedmimetypes"),
      m_mdrstate(this, "metadatacmds")
{
    zeroMe();

    if (o_origcwd.empty()) {
        char buf[MAXPATHLEN];
        if (getcwd(buf, MAXPATHLEN)) {
            o_origcwd = string(buf);
        } else {
            fprintf(stderr, "recollxx: can't retrieve current working "
                    "directory: relative path translations will fail\n");
        }
    }

    // Compute our data dir name, typically /usr/local/share/recoll
    m_datadir = path_pkgdatadir();
    // We only do the automatic configuration creation thing for the default
    // config dir, not if it was specified through -c or RECOLL_CONFDIR
    bool autoconfdir = false;

    // Command line config name overrides environment
    if (argcnf && !argcnf->empty()) {
        m_confdir = path_absolute(*argcnf);
        if (m_confdir.empty()) {
            m_reason = 
                string("Cant turn [") + *argcnf + "] into absolute path";
            return;
        }
    } else {
        const char *cp = getenv("RECOLL_CONFDIR");
        if (cp) {
            m_confdir = path_canon(cp);
        } else {
            autoconfdir = true;
            m_confdir=path_cat(path_homedata(), path_defaultrecollconfsubdir());
        }
    }

    // Note: autoconfdir and isDefaultConfig() are normally the same. We just 
    // want to avoid the imperfect test in isDefaultConfig() if we actually know
    // this is the default conf
    if (!autoconfdir && !isDefaultConfig()) {
        if (!path_exists(m_confdir)) {
            m_reason = "Explicitly specified configuration "
                "directory must exist"
                " (won't be automatically created). Use mkdir first";
            return;
        }
    }

    if (!path_exists(m_confdir)) {
        if (!initUserConfig()) 
            return;
    }

    // This can't change once computed inside a process. It would be
    // nicer to move this to a static class initializer to avoid
    // possible threading issues but this doesn't work (tried) as
    // things would not be ready. In practise we make sure that this
    // is called from the main thread at once, by constructing a config
    // from recollinit
    if (o_localecharset.empty()) {
#ifdef _WIN32
        o_localecharset = winACPName();
#elif defined(__APPLE__)
        o_localecharset = "UTF-8";
#else
        const char *cp;
        cp = nl_langinfo(CODESET);
        // We don't keep US-ASCII. It's better to use a superset
        // Ie: me have a C locale and some french file names, and I
        // can't imagine a version of iconv that couldn't translate
        // from iso8859?
        // The 646 thing is for solaris. 
        if (cp && *cp && strcmp(cp, "US-ASCII") 
#ifdef sun
            && strcmp(cp, "646")
#endif
            ) {
            o_localecharset = string(cp);
        } else {
            // Use cp1252 instead of iso-8859-1, it's a superset.
            o_localecharset = string(cstr_cp1252);
        }
#endif
        LOGDEB1("RclConfig::getDefCharset: localecharset ["  <<
                o_localecharset << "]\n");
    }

    const char *cp;

    // Additional config directory, values override user ones
    if ((cp = getenv("RECOLL_CONFTOP"))) {
        m_cdirs.push_back(cp);
    } 

    // User config
    m_cdirs.push_back(m_confdir);

    // Additional config directory, overrides system's, overridden by user's
    if ((cp = getenv("RECOLL_CONFMID"))) {
        m_cdirs.push_back(cp);
    } 

    // Base/installation config
    m_cdirs.push_back(path_cat(m_datadir, "examples"));

    string cnferrloc;
    for (const auto& dir : m_cdirs) {
        cnferrloc += "[" + dir + "] or ";
    }
    if (cnferrloc.size() > 4) {
        cnferrloc.erase(cnferrloc.size()-4);
    }

    // Read and process "recoll.conf"
    if (!updateMainConfig()) {
        m_reason = string("No/bad main configuration file in: ") + cnferrloc;
        return;
    }

    // Other files
    mimemap = new ConfStack<ConfTree>("mimemap", m_cdirs, true);
    if (mimemap == 0 || !mimemap->ok()) {
        m_reason = string("No or bad mimemap file in: ") + cnferrloc;
        return;
    }

    // Maybe create the MIME to suffix association reverse map. Do it
    // in file order so that we can control what suffix is used when
    // there are several. This only uses the distributed file, not any
    // local customization (too complicated).
    if (mime_suffixes.empty()) {
        ConfSimple mm(
            path_cat(path_cat(m_datadir, "examples"), "mimemap").c_str());
        vector<ConfLine> order = mm.getlines();
        for (const auto& entry: order) {
            if (entry.m_kind == ConfLine::CFL_VAR) {
                LOGDEB1("CONFIG: " << entry.m_data << " -> " << entry.m_value <<
                        endl);
                // Remember: insert() only does anything for new keys,
                // so we only have the first value in the map
                mime_suffixes.insert(
                    pair<string,string>(entry.m_value, entry.m_data));
            }
        }
    }

    mimeconf = new ConfStack<ConfSimple>("mimeconf", m_cdirs, true);
    if (mimeconf == 0 || !mimeconf->ok()) {
        m_reason = string("No/bad mimeconf in: ") + cnferrloc;
        return;
    }
    mimeview = new ConfStack<ConfSimple>("mimeview", m_cdirs, false);
    if (mimeview == 0)
        mimeview = new ConfStack<ConfSimple>("mimeview", m_cdirs, true);
    if (mimeview == 0 || !mimeview->ok()) {
        m_reason = string("No/bad mimeview in: ") + cnferrloc;
        return;
    }
    if (!readFieldsConfig(cnferrloc))
        return;

    // Default is no threading
    m_thrConf = {{-1, 0}, {-1, 0}, {-1, 0}};

    m_ptrans = new ConfSimple(path_cat(m_confdir, "ptrans").c_str());

    m_ok = true;
    setKeyDir(cstr_null);

    initParamStale(m_conf, mimemap);

    return;
}

bool RclConfig::updateMainConfig()
{
    ConfStack<ConfTree> *newconf = 
        new ConfStack<ConfTree>("recoll.conf", m_cdirs, true);
    if (newconf == 0 || !newconf->ok()) {
        if (m_conf)
            return false;
        m_ok = false;
        initParamStale(0, 0);
        return false;
    }

    delete m_conf;
    m_conf = newconf;

    initParamStale(m_conf, mimemap);

    setKeyDir(cstr_null);

    bool bvalue = true;
    if (getConfParam("skippedPathsFnmPathname", &bvalue) && bvalue == false) {
        FsTreeWalker::setNoFnmPathname();
    }
    string nowalkfn;
    getConfParam("nowalkfn", nowalkfn);
    if (!nowalkfn.empty()) {
        FsTreeWalker::setNoWalkFn(nowalkfn);
    }
    
    static int m_index_stripchars_init = 0;
    if (!m_index_stripchars_init) {
        getConfParam("indexStripChars", &o_index_stripchars);
        getConfParam("indexStoreDocText", &o_index_storedoctext);
        getConfParam("testmodifusemtime", &o_uptodate_test_use_mtime);
        m_index_stripchars_init = 1;
    }

    if (getConfParam("cachedir", m_cachedir)) {
        m_cachedir = path_canon(path_tildexpand(m_cachedir));
    }
    return true;
}

ConfNull *RclConfig::cloneMainConfig()
{
    ConfNull *conf = new ConfStack<ConfTree>("recoll.conf", m_cdirs, false);
    if (conf == 0 || !conf->ok()) {
        m_reason = string("Can't read config");
        return 0;
    }
    return conf;
}

// Remember what directory we're under (for further conf->get()s), and 
// prefetch a few common values.
void RclConfig::setKeyDir(const string &dir) 
{
    if (!dir.compare(m_keydir))
        return;

    m_keydirgen++;
    m_keydir = dir;
    if (m_conf == 0)
        return;

    if (!m_conf->get("defaultcharset", m_defcharset, m_keydir))
        m_defcharset.erase();
}

bool RclConfig::getConfParam(const string &name, int *ivp, bool shallow) const
{
    string value;
    if (!getConfParam(name, value, shallow))
        return false;
    errno = 0;
    long lval = strtol(value.c_str(), 0, 0);
    if (lval == 0 && errno)
        return 0;
    if (ivp)
        *ivp = int(lval);
    return true;
}

bool RclConfig::getConfParam(const string &name, bool *bvp, bool shallow) const
{
    if (!bvp) 
        return false;

    *bvp = false;
    string s;
    if (!getConfParam(name, s, shallow))
        return false;
    *bvp = stringToBool(s);
    return true;
}

bool RclConfig::getConfParam(const string &name, vector<string> *svvp,
                             bool shallow) const
{
    if (!svvp) 
        return false;
    svvp->clear();
    string s;
    if (!getConfParam(name, s, shallow))
        return false;
    return stringToStrings(s, *svvp);
}

bool RclConfig::getConfParam(const string &name, unordered_set<string> *out,
                             bool shallow) const
{
    vector<string> v;
    if (!out || !getConfParam(name, &v, shallow)) {
        return false;
    }
    out->clear();
    out->insert(v.begin(), v.end());
    return true;
}

bool RclConfig::getConfParam(const string &name, vector<int> *vip,
                             bool shallow) const
{
    if (!vip) 
        return false;
    vip->clear();
    vector<string> vs;
    if (!getConfParam(name, &vs, shallow))
        return false;
    vip->reserve(vs.size());
    for (unsigned int i = 0; i < vs.size(); i++) {
        char *ep;
        vip->push_back(strtol(vs[i].c_str(), &ep, 0));
        if (ep == vs[i].c_str()) {
            LOGDEB("RclConfig::getConfParam: bad int value in [" << name <<
                   "]\n");
            return false;
        }
    }
    return true;
}

void RclConfig::initThrConf()
{
    // Default is no threading
    m_thrConf = {{-1, 0}, {-1, 0}, {-1, 0}};

    vector<int> vq;
    vector<int> vt;
    if (!getConfParam("thrQSizes", &vq)) {
        LOGINFO("RclConfig::initThrConf: no thread info (queues)\n");
        goto out;
    }

    // If the first queue size is 0, autoconf is requested.
    if (vq.size() > 0 && vq[0] == 0) {
        CpuConf cpus;
        if (!getCpuConf(cpus) || cpus.ncpus < 1) {
            LOGERR("RclConfig::initThrConf: could not retrieve cpu conf\n");
            cpus.ncpus = 1;
        }
        if (cpus.ncpus != 1) {
            LOGDEB("RclConfig::initThrConf: autoconf requested. " <<
                   cpus.ncpus << " concurrent threads available.\n");
        }

        // Arbitrarily set threads config based on number of CPUS. This also
        // depends on the IO setup actually, so we're bound to be wrong...
        if (cpus.ncpus == 1) {
            // Somewhat counter-intuitively (because of possible IO//)
            // it seems that the best config here is no threading
        } else if (cpus.ncpus < 4) {
            // Untested so let's guess...
            m_thrConf = {{2, 2}, {2, 2}, {2, 1}};
        } else if (cpus.ncpus < 6) {
            m_thrConf = {{2, 4}, {2, 2}, {2, 1}};
        } else {
            m_thrConf = {{2, 5}, {2, 3}, {2, 1}};
        }
        goto out;
    } else if (vq.size() > 0 && vq[0] < 0) {
        // threads disabled by config
        goto out;
    }

    if (!getConfParam("thrTCounts", &vt) ) {
        LOGINFO("RclConfig::initThrConf: no thread info (threads)\n");
        goto out;
    }

    if (vq.size() != 3 || vt.size() != 3) {
        LOGINFO("RclConfig::initThrConf: bad thread info vector sizes\n");
        goto out;
    }

    // Normal case: record info from config
    m_thrConf.clear();
    for (unsigned int i = 0; i < 3; i++) {
        m_thrConf.push_back({vq[i], vt[i]});
    }

out:
    ostringstream sconf;
    for (unsigned int i = 0; i < 3; i++) {
        sconf << "(" << m_thrConf[i].first << ", " << m_thrConf[i].second <<
            ") ";
    }

    LOGDEB("RclConfig::initThrConf: chosen config (ql,nt): " << sconf.str() <<
           "\n");
}

pair<int,int> RclConfig::getThrConf(ThrStage who) const
{
    if (m_thrConf.size() != 3) {
        LOGERR("RclConfig::getThrConf: bad data in rclconfig\n");
        return pair<int,int>(-1,-1);
    }
    return m_thrConf[who];
}

vector<string> RclConfig::getTopdirs(bool formonitor) const
{
    vector<string> tdl;
    if (formonitor) {
        if (!getConfParam("monitordirs", &tdl)) {
            getConfParam("topdirs", &tdl);
        }
    } else {
        getConfParam("topdirs", &tdl);
    }
    if (tdl.empty()) {
        LOGERR("RclConfig::getTopdirs: nothing to index:  topdirs/monitordirs "
               " are not set or have a bad list format\n");
        return tdl;
    }

    for (auto& dir : tdl) {
        dir = path_canon(path_tildexpand(dir));
    }
    return tdl;
}

const string& RclConfig::getLocaleCharset()
{
    return o_localecharset;
}

// Get charset to be used for transcoding to utf-8 if unspecified by doc
// For document contents:
//  If defcharset was set (from the config or a previous call, this
//   is done in setKeydir), use it.
//  Else, try to guess it from the locale
//  Use cp1252 (as a superset of iso8859-1) as ultimate default
//
// For filenames, same thing except that we do not use the config file value
// (only the locale).
const string& RclConfig::getDefCharset(bool filename) const
{
    if (filename) {
        return o_localecharset;
    } else {
        return m_defcharset.empty() ? o_localecharset : m_defcharset;
    }
}

// Get all known document mime values. We get them from the mimeconf
// 'index' submap.
// It's quite possible that there are other mime types in the index
// (defined in mimemap and not mimeconf, or output by "file -i"). We
// just ignore them, because there may be myriads, and their contents
// are not indexed. 
//
// This unfortunately means that searches by file names and mime type
// filtering don't work well together.
vector<string> RclConfig::getAllMimeTypes() const
{
    return mimeconf ? mimeconf->getNames("index") : vector<string>();
}

// Things for suffix comparison. We define a string class and string 
// comparison with suffix-only sensitivity
class SfString {
public:
    SfString(const string& s) : m_str(s) {}
    bool operator==(const SfString& s2) const {
        string::const_reverse_iterator r1 = m_str.rbegin(), re1 = m_str.rend(),
            r2 = s2.m_str.rbegin(), re2 = s2.m_str.rend();
        while (r1 != re1 && r2 != re2) {
            if (*r1 != *r2) {
                return 0;
            }
            ++r1; ++r2;
        }
        return 1;
    }
    string m_str;
};

class SuffCmp {
public:
    int operator()(const SfString& s1, const SfString& s2) const {
        //cout << "Comparing " << s1.m_str << " and " << s2.m_str << endl;
        string::const_reverse_iterator 
            r1 = s1.m_str.rbegin(), re1 = s1.m_str.rend(),
            r2 = s2.m_str.rbegin(), re2 = s2.m_str.rend();
        while (r1 != re1 && r2 != re2) {
            if (*r1 != *r2) {
                return *r1 < *r2 ? 1 : 0;
            }
            ++r1; ++r2;
        }
        return 0;
    }
};

typedef multiset<SfString, SuffCmp> SuffixStore;
#define STOPSUFFIXES ((SuffixStore *)m_stopsuffixes)

vector<string>& RclConfig::getStopSuffixes()
{
    bool needrecompute = m_stpsuffstate.needrecompute();
    needrecompute = m_oldstpsuffstate.needrecompute() || needrecompute;
    if (needrecompute || m_stopsuffixes == 0) {
        // Need to initialize the suffixes

        // Let the old customisation have priority: if recoll_noindex from
        // mimemap is set, it the user's (the default value is gone). Else
        // use the new variable
        if (!m_oldstpsuffstate.getvalue(0).empty()) {
            stringToStrings(m_oldstpsuffstate.getvalue(0), m_stopsuffvec);
        } else {
            std::set<string> ss;
            computeBasePlusMinus(ss, m_stpsuffstate.getvalue(0), 
                                 m_stpsuffstate.getvalue(1), 
                                 m_stpsuffstate.getvalue(2));
            m_stopsuffvec = vector<string>(ss.begin(), ss.end());
        }

        // Compute the special suffixes store
        delete STOPSUFFIXES;
        if ((m_stopsuffixes = new SuffixStore) == 0) {
            LOGERR("RclConfig::inStopSuffixes: out of memory\n");
            return m_stopsuffvec;
        }
        m_maxsufflen = 0;
        for (const auto& entry : m_stopsuffvec) {
            STOPSUFFIXES->insert(SfString(stringtolower(entry)));
            if (m_maxsufflen < entry.length())
                m_maxsufflen = int(entry.length());
        }
    }
    LOGDEB1("RclConfig::getStopSuffixes: ->" <<
            stringsToString(m_stopsuffvec) << endl);
    return m_stopsuffvec;
}

bool RclConfig::inStopSuffixes(const string& fni)
{
    LOGDEB2("RclConfig::inStopSuffixes(" << fni << ")\n");

    // Call getStopSuffixes() to possibly update state, ignore result
    getStopSuffixes();

    // Only need a tail as long as the longest suffix.
    int pos = MAX(0, int(fni.length() - m_maxsufflen));
    string fn(fni, pos);

    stringtolower(fn);
    SuffixStore::const_iterator it = STOPSUFFIXES->find(fn);
    if (it != STOPSUFFIXES->end()) {
        LOGDEB2("RclConfig::inStopSuffixes: Found (" << fni << ") ["  <<
                ((*it).m_str) << "]\n");
        return true;
    } else {
        LOGDEB2("RclConfig::inStopSuffixes: not found [" << fni << "]\n");
        return false;
    }
}

string RclConfig::getMimeTypeFromSuffix(const string& suff) const
{
    string mtype;
    mimemap->get(suff, mtype, m_keydir);
    return mtype;
}

string RclConfig::getSuffixFromMimeType(const string &mt) const
{
    // First try from standard data, ensuring that we can control the value
    // from the order in the configuration file.
    auto rclsuff = mime_suffixes.find(mt);
    if (rclsuff != mime_suffixes.end()) {
        return rclsuff->second;
    }
    // Try again from local data. The map is in the wrong direction,
    // have to walk it.
    vector<string> sfs = mimemap->getNames(cstr_null);
    for (const auto& suff : sfs) {
        string mt1;
        if (mimemap->get(suff, mt1, cstr_null) && !stringicmp(mt, mt1)) {
            return suff;
        }
    }
    return cstr_null;
}

/** Get list of file categories from mimeconf */
bool RclConfig::getMimeCategories(vector<string>& cats) const
{
    if (!mimeconf)
        return false;
    cats = mimeconf->getNames("categories");
    return true;
}

bool RclConfig::isMimeCategory(string& cat) const
{
    vector<string>cats;
    getMimeCategories(cats);
    for (vector<string>::iterator it = cats.begin(); it != cats.end(); it++) {
        if (!stringicmp(*it,cat))
            return true;
    }
    return false;
}

/** Get list of mime types for category from mimeconf */
bool RclConfig::getMimeCatTypes(const string& cat, vector<string>& tps) const
{
    tps.clear();
    if (!mimeconf)
        return false;
    string slist;
    if (!mimeconf->get(cat, slist, "categories"))
        return false;

    stringToStrings(slist, tps);
    return true;
}

string RclConfig::getMimeHandlerDef(const string &mtype, bool filtertypes)
{
    string hs;

    if (filtertypes) {
        if(m_rmtstate.needrecompute()) {
            m_restrictMTypes.clear();
            stringToStrings(stringtolower((const string&)m_rmtstate.getvalue()),
                            m_restrictMTypes);
        }
        if (m_xmtstate.needrecompute()) {
            m_excludeMTypes.clear();
            stringToStrings(stringtolower((const string&)m_xmtstate.getvalue()),
                            m_excludeMTypes);
        }
        if (!m_restrictMTypes.empty() && 
            !m_restrictMTypes.count(stringtolower(mtype))) {
            LOGDEB2("RclConfig::getMimeHandlerDef: not in mime type list\n");
            return hs;
        }
        if (!m_excludeMTypes.empty() && 
            m_excludeMTypes.count(stringtolower(mtype))) {
            LOGDEB2("RclConfig::getMimeHandlerDef: in excluded mime list\n");
            return hs;
        }
    }

    if (!mimeconf->get(mtype, hs, "index")) {
        LOGDEB1("getMimeHandlerDef: no handler for '" << mtype << "'\n");
    }
    return hs;
}

const vector<MDReaper>& RclConfig::getMDReapers()
{
    string hs;
    if (m_mdrstate.needrecompute()) {
        m_mdreapers.clear();
        // New value now stored in m_mdrstate.getvalue(0)
        const string& sreapers = m_mdrstate.getvalue(0);
        if (sreapers.empty())
            return m_mdreapers;
        string value;
        ConfSimple attrs;
        valueSplitAttributes(sreapers, value, attrs);
        vector<string> nmlst = attrs.getNames(cstr_null);
        for (vector<string>::const_iterator it = nmlst.begin();
             it != nmlst.end(); it++) {
            MDReaper reaper;
            reaper.fieldname = fieldCanon(*it);
            string s;
            attrs.get(*it, s);
            stringToStrings(s, reaper.cmdv);
            m_mdreapers.push_back(reaper);
        }
    }
    return m_mdreapers;
}

bool RclConfig::getGuiFilterNames(vector<string>& cats) const
{
    if (!mimeconf)
        return false;
    cats = mimeconf->getNamesShallow("guifilters");
    return true;
}

bool RclConfig::getGuiFilter(const string& catfiltername, string& frag) const
{
    frag.clear();
    if (!mimeconf)
        return false;
    if (!mimeconf->get(catfiltername, frag, "guifilters"))
        return false;
    return true;
}

bool RclConfig::valueSplitAttributes(const string& whole, string& value, 
                                     ConfSimple& attrs)
{
    /* There is currently no way to escape a semi-colon */
    string::size_type semicol0 = whole.find_first_of(";");
    value = whole.substr(0, semicol0);
    trimstring(value);
    string attrstr;
    if (semicol0 != string::npos && semicol0 < whole.size() - 1) {
        attrstr = whole.substr(semicol0+1);
    }

    // Handle additional attributes. We substitute the semi-colons
    // with newlines and use a ConfSimple
    if (!attrstr.empty()) {
        for (string::size_type i = 0; i < attrstr.size(); i++) {
            if (attrstr[i] == ';')
                attrstr[i] = '\n';
        }
        attrs.reparse(attrstr);
    } else {
        attrs.clear();
    }
    
    return true;
}

bool RclConfig::getMissingHelperDesc(string& out) const
{
    string fmiss = path_cat(getConfDir(), "missing");
    out.clear();
    if (!file_to_string(fmiss, out))
        return false;
    return true;
}

void RclConfig::storeMissingHelperDesc(const string &s)
{
    string fmiss = path_cat(getCacheDir(), "missing");
    FILE *fp = fopen(fmiss.c_str(), "w");
    if (fp) {
        if (s.size() > 0 && fwrite(s.c_str(), s.size(), 1, fp) != 1) {
            LOGERR("storeMissingHelperDesc: fwrite failed\n");
        }
        fclose(fp);
    }
}

// Read definitions for field prefixes, aliases, and hierarchy and arrange 
// things for speed (theses are used a lot during indexing)
bool RclConfig::readFieldsConfig(const string& cnferrloc)
{
    LOGDEB2("RclConfig::readFieldsConfig\n");
    m_fields = new ConfStack<ConfSimple>("fields", m_cdirs, true);
    if (m_fields == 0 || !m_fields->ok()) {
        m_reason = string("No/bad fields file in: ") + cnferrloc;
        return false;
    }

    // Build a direct map avoiding all indirections for field to
    // prefix translation
    // Add direct prefixes from the [prefixes] section
    vector<string> tps = m_fields->getNames("prefixes");
    for (const auto& fieldname : tps) {
        string val;
        m_fields->get(fieldname, val, "prefixes");
        ConfSimple attrs;
        FieldTraits ft;
        // fieldname = prefix ; attr1=val;attr2=val...
        if (!valueSplitAttributes(val, ft.pfx, attrs)) {
            LOGERR("readFieldsConfig: bad config line for ["  << fieldname <<
                   "]: [" << val << "]\n");
            return 0;
        }
        string tval;
        if (attrs.get("wdfinc", tval))
            ft.wdfinc = atoi(tval.c_str());
        if (attrs.get("boost", tval))
            ft.boost = atof(tval.c_str());
        if (attrs.get("pfxonly", tval))
            ft.pfxonly = stringToBool(tval);
        if (attrs.get("noterms", tval))
            ft.noterms = stringToBool(tval);
        m_fldtotraits[stringtolower(fieldname)] = ft;
        LOGDEB2("readFieldsConfig: ["  << fieldname << "] -> ["  << ft.pfx <<
                "] " << ft.wdfinc << " " << ft.boost << "\n");
    }

    // Values section
    tps = m_fields->getNames("values");
    for (const auto& fieldname : tps) {
        string canonic = stringtolower(fieldname); // canonic name
        string val;
        m_fields->get(fieldname, val, "values");
        ConfSimple attrs;
        string svslot;
        // fieldname = valueslot ; attr1=val;attr2=val...
        if (!valueSplitAttributes(val, svslot, attrs)) {
            LOGERR("readFieldsConfig: bad value line for ["  << fieldname <<
                   "]: [" << val << "]\n");
            return 0;
        }
        uint32_t valueslot = uint32_t(atoi(svslot.c_str()));
        if (valueslot == 0) {
            LOGERR("readFieldsConfig: found 0 value slot for [" << fieldname <<
                   "]: [" << val << "]\n");
            continue;
        }

        string tval;
        FieldTraits::ValueType valuetype{FieldTraits::STR};
        if (attrs.get("type", tval)) {
            if (tval == "string") {
                valuetype = FieldTraits::STR;
            } else if (tval == "int") {
                valuetype = FieldTraits::INT;
            } else {
                LOGERR("readFieldsConfig: bad type for value for " <<
                       fieldname << " : " << tval << endl);
                return 0;
            }
        }
        int valuelen{0};
        if (attrs.get("len", tval)) {
            valuelen = atoi(tval.c_str());
        }
        
        // Find or insert traits entry
        const auto pit =
            m_fldtotraits.insert(
                pair<string, FieldTraits>(canonic, FieldTraits())).first;
        pit->second.valueslot = valueslot;
        pit->second.valuetype = valuetype;
        pit->second.valuelen = valuelen;
    }
    
    // Add prefixes for aliases and build alias-to-canonic map while
    // we're at it. Having the aliases in the prefix map avoids an
    // additional indirection at index time.
    tps = m_fields->getNames("aliases");
    for (const auto& fieldname : tps) {
        string canonic = stringtolower(fieldname); // canonic name
        FieldTraits ft;
        const auto pit = m_fldtotraits.find(canonic);
        if (pit != m_fldtotraits.end()) {
            ft = pit->second;
        }
        string aliases;
        m_fields->get(canonic, aliases, "aliases");
        vector<string> l;
        stringToStrings(aliases, l);
        for (const auto& alias : l) {
            if (pit != m_fldtotraits.end())
                m_fldtotraits[stringtolower(alias)] = ft;
            m_aliastocanon[stringtolower(alias)] = canonic;
        }
    }

    // Query aliases map
    tps = m_fields->getNames("queryaliases");
    for (const auto& entry: tps) {
        string canonic = stringtolower(entry); // canonic name
        string aliases;
        m_fields->get(canonic, aliases, "queryaliases");
        vector<string> l;
        stringToStrings(aliases, l);
        for (const auto& alias : l) {
            m_aliastoqcanon[stringtolower(alias)] = canonic;
        }
    }

#if 0
    for (map<string, FieldTraits>::const_iterator it = m_fldtotraits.begin();
         it != m_fldtotraits.end(); it++) {
        LOGDEB("readFieldsConfig: ["  << entry << "] -> ["  << it->second.pfx <<
               "] " << it->second.wdfinc << " " << it->second.boost << "\n");
    }
#endif

    vector<string> sl = m_fields->getNames("stored");
    for (const auto& fieldname : sl) {
        m_storedFields.insert(fieldCanon(stringtolower(fieldname)));
    }

    // Extended file attribute to field translations
    vector<string>xattrs = m_fields->getNames("xattrtofields");
    for (const auto& xattr : xattrs) {
        string val;
        m_fields->get(xattr, val, "xattrtofields");
        m_xattrtofld[xattr] = val;
    }

    return true;
}

// Return specifics for field name:
bool RclConfig::getFieldTraits(const string& _fld, const FieldTraits **ftpp,
                               bool isquery) const
{
    string fld = isquery ? fieldQCanon(_fld) : fieldCanon(_fld);
    map<string, FieldTraits>::const_iterator pit = m_fldtotraits.find(fld);
    if (pit != m_fldtotraits.end()) {
        *ftpp = &pit->second;
        LOGDEB1("RclConfig::getFieldTraits: [" << _fld << "]->["  <<
                pit->second.pfx << "]\n");
        return true;
    } else {
        LOGDEB1("RclConfig::getFieldTraits: no prefix for field [" << fld <<
                "]\n");
        *ftpp = 0;
        return false;
    }
}

set<string> RclConfig::getIndexedFields() const
{
    set<string> flds;
    if (m_fields == 0)
        return flds;

    vector<string> sl = m_fields->getNames("prefixes");
    flds.insert(sl.begin(), sl.end());
    return flds;
}

string RclConfig::fieldCanon(const string& f) const
{
    string fld = stringtolower(f);
    map<string, string>::const_iterator it = m_aliastocanon.find(fld);
    if (it != m_aliastocanon.end()) {
        LOGDEB1("RclConfig::fieldCanon: [" << f << "] -> [" << it->second <<
                "]\n");
        return it->second;
    }
    LOGDEB1("RclConfig::fieldCanon: ["  << (f) << "] -> ["  << (fld) << "]\n");
    return fld;
}

string RclConfig::fieldQCanon(const string& f) const
{
    string fld = stringtolower(f);
    map<string, string>::const_iterator it = m_aliastoqcanon.find(fld);
    if (it != m_aliastoqcanon.end()) {
        LOGDEB1("RclConfig::fieldQCanon: [" << f << "] -> ["  << it->second <<
                "]\n");
        return it->second;
    }
    return fieldCanon(f);
}

vector<string> RclConfig::getFieldSectNames(const string &sk, const char* patrn)
    const
{
    if (m_fields == 0)
        return vector<string>();
    return m_fields->getNames(sk, patrn);
}

bool RclConfig::getFieldConfParam(const string &name, const string &sk, 
                                  string &value) const
{
    if (m_fields == 0)
        return false;
    return m_fields->get(name, value, sk);
}

set<string> RclConfig::getMimeViewerAllEx() const
{
    set<string> res;
    if (mimeview == 0)
        return res;

    string base, plus, minus;
    mimeview->get("xallexcepts", base, "");
    LOGDEB1("RclConfig::getMimeViewerAllEx(): base: " << s << endl);
    mimeview->get("xallexcepts+", plus, "");
    LOGDEB1("RclConfig::getMimeViewerAllEx(): plus: " << plus << endl);
    mimeview->get("xallexcepts-", minus, "");
    LOGDEB1("RclConfig::getMimeViewerAllEx(): minus: " << minus << endl);

    computeBasePlusMinus(res, base, plus, minus);
    LOGDEB1("RclConfig::getMimeViewerAllEx(): res: " << stringsToString(res)
            << endl);
    return res;
}

bool RclConfig::setMimeViewerAllEx(const set<string>& allex)
{
    if (mimeview == 0)
        return false;

    string sbase;
    mimeview->get("xallexcepts", sbase, "");

    string splus, sminus;
    setPlusMinus(sbase, allex, splus, sminus);

    if (!mimeview->set("xallexcepts-", sminus, "")) {
        m_reason = string("RclConfig:: cant set value. Readonly?");
        return false;
    }
    if (!mimeview->set("xallexcepts+", splus, "")) {
        m_reason = string("RclConfig:: cant set value. Readonly?");
        return false;
    }

    return true;
}

string RclConfig::getMimeViewerDef(const string &mtype, const string& apptag,
                                   bool useall) const
{
    LOGDEB2("RclConfig::getMimeViewerDef: mtype [" << mtype << "] apptag ["
            << apptag << "]\n");
    string hs;
    if (mimeview == 0)
        return hs;

    if (useall) {
        // Check for exception
        set<string> allex = getMimeViewerAllEx();
        bool isexcept = false;
        for (auto& it : allex) {
            vector<string> mita;
            stringToTokens(it, mita, "|");
            if ((mita.size() == 1 && apptag.empty() && mita[0] == mtype) ||
                (mita.size() == 2 && mita[1] == apptag && mita[0] == mtype)) {
                // Exception to x-all
                isexcept = true;
                break;
            }
        }

        if (isexcept == false) {
            mimeview->get("application/x-all", hs, "view");
            return hs;
        }
        // Fallthrough to normal case.
    }

    if (apptag.empty() || !mimeview->get(mtype + string("|") + apptag,
                                         hs, "view"))
        mimeview->get(mtype, hs, "view");
    return hs;
}

bool RclConfig::getMimeViewerDefs(vector<pair<string, string> >& defs) const
{
    if (mimeview == 0)
        return false;
    vector<string>tps = mimeview->getNames("view");
    for (vector<string>::const_iterator it = tps.begin(); 
         it != tps.end();it++) {
        defs.push_back(pair<string, string>(*it, getMimeViewerDef(*it, "", 0)));
    }
    return true;
}

bool RclConfig::setMimeViewerDef(const string& mt, const string& def)
{
    if (mimeview == 0)
        return false;
    bool status;
    if (!def.empty()) 
        status = mimeview->set(mt, def, "view");
    else 
        status = mimeview->erase(mt, "view");

    if (!status) {
        m_reason = string("RclConfig:: cant set value. Readonly?");
        return false;
    }
    return true;
}

bool RclConfig::mimeViewerNeedsUncomp(const string &mimetype) const
{
    string s;
    vector<string> v;
    if (mimeview != 0 && mimeview->get("nouncompforviewmts", s, "") && 
        stringToStrings(s, v) && 
        find_if(v.begin(), v.end(), StringIcmpPred(mimetype)) != v.end()) 
        return false;
    return true;
}

string RclConfig::getMimeIconPath(const string &mtype, const string &apptag)
    const
{
    string iconname;
    if (!apptag.empty())
        mimeconf->get(mtype + string("|") + apptag, iconname, "icons");
    if (iconname.empty())
        mimeconf->get(mtype, iconname, "icons");
    if (iconname.empty())
        iconname = "document";

    string iconpath;
#if defined (__FreeBSD__) && __FreeBSD_version < 500000
    // gcc 2.95 dies if we call getConfParam here ??
    if (m_conf) m_conf->get(string("iconsdir"), iconpath, m_keydir);
#else
    getConfParam("iconsdir", iconpath);
#endif

    if (iconpath.empty()) {
        iconpath = path_cat(m_datadir, "images");
    } else {
        iconpath = path_tildexpand(iconpath);
    }
    return path_cat(iconpath, iconname) + ".png";
}

// Return path defined by varname. May be absolute or relative to
// confdir, with default in confdir
string RclConfig::getConfdirPath(const char *varname, const char *dflt) const
{
    string result;
    if (!getConfParam(varname, result)) {
        result = path_cat(getConfDir(), dflt);
    } else {
        result = path_tildexpand(result);
        // If not an absolute path, compute relative to config dir
        if (!path_isabsolute(result)) {
            result = path_cat(getConfDir(), result);
        }
    }
    return path_canon(result);
}

string RclConfig::getCacheDir() const
{
    return m_cachedir.empty() ? getConfDir() : m_cachedir;
}

// Return path defined by varname. May be absolute or relative to
// confdir, with default in confdir
string RclConfig::getCachedirPath(const char *varname, const char *dflt) const
{
    string result;
    if (!getConfParam(varname, result)) {
        result = path_cat(getCacheDir(), dflt);
    } else {
        result = path_tildexpand(result);
        // If not an absolute path, compute relative to cache dir
        if (!path_isabsolute(result)) {
            result = path_cat(getCacheDir(), result);
        }
    }
    return path_canon(result);
}

string RclConfig::getDbDir() const
{
    return getCachedirPath("dbdir", "xapiandb");
}
string RclConfig::getWebcacheDir() const
{
    return getCachedirPath("webcachedir", "webcache");
}
string RclConfig::getMboxcacheDir() const
{
    return getCachedirPath("mboxcachedir", "mboxcache");
}
string RclConfig::getAspellcacheDir() const
{
    return getCachedirPath("aspellDicDir", "");
}

string RclConfig::getStopfile() const
{
    return getConfdirPath("stoplistfile", "stoplist.txt");
}

string RclConfig::getSynGroupsFile() const
{
    return getConfdirPath("syngroupsfile", "syngroups.txt");
}

// The index status file is fast changing, so it's possible to put it outside
// of the config directory (for ssds, not sure this is really useful).
// To enable being quite xdg-correct we should add a getRundirPath()
string RclConfig::getIdxStatusFile() const
{
    return getCachedirPath("idxstatusfile", "idxstatus.txt");
}
string RclConfig::getPidfile() const
{
    return path_cat(getCacheDir(), "index.pid");
}
string RclConfig::getIdxStopFile() const
{
    return path_cat(getCacheDir(), "index.stop");
}

/* Eliminate the common leaf part of file paths p1 and p2. Example: 
 * /mnt1/common/part /mnt2/common/part -> /mnt1 /mnt2. This is used
 * for computing translations for paths when the dataset has been
 * moved. Of course this could be done more efficiently than by splitting 
 * into vectors, but we don't care.*/
static string path_diffstems(const string& p1, const string& p2,
                             string& r1, string& r2)
{
    string reason;
    r1.clear();
    r2.clear();
    vector<string> v1, v2;
    stringToTokens(p1, v1, "/");
    stringToTokens(p2, v2, "/");
    unsigned int l1 = v1.size();
    unsigned int l2 = v2.size();
        
    // Search for common leaf part
    unsigned int cl = 0;
    for (; cl < MIN(l1, l2); cl++) {
        if (v1[l1-cl-1] != v2[l2-cl-1]) {
            break;
        }
    }
    //cerr << "Common length = " << cl << endl;
    if (cl == 0) {
        reason = "Input paths are empty or have no common part";
        return reason;
    }
    for (unsigned i = 0; i < l1 - cl; i++) {
        r1 += "/" + v1[i];
    }
    for (unsigned i = 0; i < l2 - cl; i++) {
        r2 += "/" + v2[i];
    }
        
    return reason;
}

void RclConfig::urlrewrite(const string& dbdir, string& url) const
{
    LOGDEB1("RclConfig::urlrewrite: dbdir [" << dbdir << "] url [" << url <<
            "]\n");

    // If orgidxconfdir is set, we assume that this index is for a
    // movable dataset, with the configuration directory stored inside
    // the dataset tree. This allows computing automatic path
    // translations if the dataset has been moved.
    string orig_confdir;
    string cur_confdir;
    string confstemorg, confstemrep;
    if (m_conf->get("orgidxconfdir", orig_confdir, "")) {
        if (!m_conf->get("curidxconfdir", cur_confdir, "")) {
            cur_confdir = m_confdir;
        }
        LOGDEB1("RclConfig::urlrewrite: orgidxconfdir: " << orig_confdir <<
                " cur_confdir " << cur_confdir << endl);
        string reason = path_diffstems(orig_confdir, cur_confdir,
                                       confstemorg, confstemrep);
        if (!reason.empty()) {
            LOGERR("urlrewrite: path_diffstems failed: " << reason <<
                   " : orig_confdir [" << orig_confdir <<
                   "] cur_confdir [" << cur_confdir << endl);
            confstemorg = confstemrep = "";
        }
    }
    
    // Do path translations exist for this index ?
    bool needptrans = true;
    if (m_ptrans == 0 || !m_ptrans->hasSubKey(dbdir)) {
        LOGDEB2("RclConfig::urlrewrite: no paths translations (m_ptrans " <<
                m_ptrans << ")\n");
        needptrans = false;
    }

    if (!needptrans && confstemorg.empty()) {
        return;
    }
    bool computeurl = false;
    
    string path = fileurltolocalpath(url);
    if (path.empty()) {
        LOGDEB2("RclConfig::urlrewrite: not file url\n");
        return;
    }
    
    // Do the movable volume thing.
    if (!confstemorg.empty() && confstemorg.size() <= path.size() &&
        !path.compare(0, confstemorg.size(), confstemorg)) {
        path = path.replace(0, confstemorg.size(), confstemrep);
        computeurl = true;
    }

    if (needptrans) {
        // For each translation check if the prefix matches the input path,
        // replace and return the result if it does.
        vector<string> opaths = m_ptrans->getNames(dbdir);
        for (const auto& opath: opaths) {
            if (opath.size() <= path.size() &&
                !path.compare(0, opath.size(), opath)) {
                string npath;
                // Key comes from getNames()=> call must succeed
                if (m_ptrans->get(opath, npath, dbdir)) { 
                    path = path_canon(path.replace(0, opath.size(), npath));
                    computeurl = true;
                }
                break;
            }
        }
    }
    if (computeurl) {
        url = path_pathtofileurl(path);
    }
}

bool RclConfig::sourceChanged() const
{
    if (m_conf && m_conf->sourceChanged())
        return true;
    if (mimemap && mimemap->sourceChanged())
        return true;
    if (mimeconf && mimeconf->sourceChanged())
        return true;
    if (mimeview && mimeview->sourceChanged())
        return true;
    if (m_fields && m_fields->sourceChanged())
        return true;
    if (m_ptrans && m_ptrans->sourceChanged())
        return true;
    return false;
}

string RclConfig::getWebQueueDir() const
{
    string webqueuedir;
    if (!getConfParam("webqueuedir", webqueuedir)) {
#ifdef _WIN32
        webqueuedir = "~/AppData/Local/RecollWebQueue";
#else
        webqueuedir = "~/.recollweb/ToIndex/";
#endif
    }
    webqueuedir = path_tildexpand(webqueuedir);
    return webqueuedir;
}

vector<string>& RclConfig::getSkippedNames()
{
    if (m_skpnstate.needrecompute()) {
        set<string> ss;
        computeBasePlusMinus(ss, m_skpnstate.getvalue(0),
                             m_skpnstate.getvalue(1), m_skpnstate.getvalue(2));
        m_skpnlist = vector<string>(ss.begin(), ss.end());
    }
    return m_skpnlist;
}

vector<string>& RclConfig::getOnlyNames()
{
    if (m_onlnstate.needrecompute()) {
        stringToStrings(m_onlnstate.getvalue(), m_onlnlist);
    }
    return m_onlnlist;
}

vector<string> RclConfig::getSkippedPaths() const
{
    vector<string> skpl;
    getConfParam("skippedPaths", &skpl);

    // Always add the dbdir and confdir to the skipped paths. This is 
    // especially important for the rt monitor which will go into a loop if we
    // don't do this.
    skpl.push_back(getDbDir());
    skpl.push_back(getConfDir());
#ifdef _WIN32
	skpl.push_back(TempFile::rcltmpdir());
#endif
    if (getCacheDir().compare(getConfDir())) {
        skpl.push_back(getCacheDir());
    }
    // And the web queue dir
    skpl.push_back(getWebQueueDir());
    for (vector<string>::iterator it = skpl.begin(); it != skpl.end(); it++) {
        *it = path_tildexpand(*it);
        *it = path_canon(*it);
    }
    sort(skpl.begin(), skpl.end());
    vector<string>::iterator uit = unique(skpl.begin(), skpl.end());
    skpl.resize(uit - skpl.begin());
    return skpl;
}

vector<string> RclConfig::getDaemSkippedPaths() const
{
    vector<string> dskpl;
    getConfParam("daemSkippedPaths", &dskpl);

    for (vector<string>::iterator it = dskpl.begin(); it != dskpl.end(); it++) {
        *it = path_tildexpand(*it);
        *it = path_canon(*it);
    }

    vector<string> skpl1 = getSkippedPaths();
    vector<string> skpl;
    if (dskpl.empty()) {
        skpl = skpl1;
    } else {
        sort(dskpl.begin(), dskpl.end());
        merge(dskpl.begin(), dskpl.end(), skpl1.begin(), skpl1.end(), 
              skpl.begin());
        vector<string>::iterator uit = unique(skpl.begin(), skpl.end());
        skpl.resize(uit - skpl.begin());
    }
    return skpl;
}


// Look up an executable filter.  We add $RECOLL_FILTERSDIR,
// and filtersdir from the config file to the PATH, then use execmd::which()
string RclConfig::findFilter(const string &icmd) const
{
    // If the path is absolute, this is it
    if (path_isabsolute(icmd))
        return icmd;

    const char *cp = getenv("PATH");
    if (!cp) //??
        cp = "";
    string PATH(cp);

    // For historical reasons: check in personal config directory
    PATH = getConfDir() + path_PATHsep() + PATH;

    string temp;
    // Prepend $datadir/filters
    temp = path_cat(m_datadir, "filters");
    PATH = temp + path_PATHsep() + PATH;
#ifdef _WIN32
    // Windows only: use the bundled Python
    temp = path_cat(m_datadir, "filters");
    temp = path_cat(temp, "python");
    PATH = temp + path_PATHsep() + PATH;
#endif
    // Prepend possible configuration parameter?
    if (getConfParam(string("filtersdir"), temp)) {
        temp = path_tildexpand(temp);
        PATH = temp + path_PATHsep() + PATH;
    }

    // Prepend possible environment variable
    if ((cp = getenv("RECOLL_FILTERSDIR"))) {
        PATH = string(cp) + path_PATHsep() + PATH;
    } 

    string cmd;
    if (ExecCmd::which(icmd, cmd, PATH.c_str())) {
        return cmd;
    } else {
        // Let the shell try to find it...
        return icmd;
    }
}

/** 
 * Return decompression command line for given mime type
 */
bool RclConfig::getUncompressor(const string &mtype, vector<string>& cmd) const
{
    string hs;

    mimeconf->get(mtype, hs, cstr_null);
    if (hs.empty())
        return false;
    vector<string> tokens;
    stringToStrings(hs, tokens);
    if (tokens.empty()) {
        LOGERR("getUncompressor: empty spec for mtype " << mtype << "\n");
        return false;
    }
    vector<string>::iterator it = tokens.begin();
    if (tokens.size() < 2)
        return false;
    if (stringlowercmp("uncompress", *it++)) 
        return false;
    cmd.clear();
    cmd.push_back(findFilter(*it));

    // Special-case python and perl on windows: we need to also locate the
    // first argument which is the script name "python somescript.py". 
    // On Unix, thanks to #!, we usually just run "somescript.py", but need
    // the same change if we ever want to use the same cmdling as windows
    if (!stringlowercmp("python", *it) || !stringlowercmp("perl", *it)) {
        it++;
        if (tokens.size() < 3) {
            LOGERR("getUncpressor: python/perl cmd: no script?. [" <<
                   mtype << "]\n");
        } else {
            *it = findFilter(*it);
        }
    } else {
        it++;
    }
    
    cmd.insert(cmd.end(), it, tokens.end());
    return true;
}

static const char blurb0[] = 
    "# The system-wide configuration files for recoll are located in:\n"
    "#   %s\n"
    "# The default configuration files are commented, you should take a look\n"
    "# at them for an explanation of what can be set (you could also take a look\n"
    "# at the manual instead).\n"
    "# Values set in this file will override the system-wide values for the file\n"
    "# with the same name in the central directory. The syntax for setting\n"
    "# values is identical.\n"
    ;
// We just use path_max to print the path to /usr/share/recoll/examples 
// inside the config file. At worse, the text is truncated (using
// snprintf). But 4096 should be enough :)
#ifndef PATH_MAX
#define MYPATHALLOC 4096
#else
#define MYPATHALLOC PATH_MAX
#endif

// Use uni2ascii -a K to generate these from the utf-8 strings
// Swedish and Danish. 
static const char swedish_ex[] = "unac_except_trans = \303\244\303\244 \303\204\303\244 \303\266\303\266 \303\226\303\266 \303\274\303\274 \303\234\303\274 \303\237ss \305\223oe \305\222oe \303\246ae \303\206ae \357\254\201fi \357\254\202fl \303\245\303\245 \303\205\303\245";
// German:
static const char german_ex[] = "unac_except_trans = \303\244\303\244 \303\204\303\244 \303\266\303\266 \303\226\303\266 \303\274\303\274 \303\234\303\274 \303\237ss \305\223oe \305\222oe \303\246ae \303\206ae \357\254\201fi \357\254\202fl";

// Create initial user config by creating commented empty files
static const char *configfiles[] = {"recoll.conf", "mimemap", "mimeconf", 
                                    "mimeview", "fields"};
static int ncffiles = sizeof(configfiles) / sizeof(char *);
bool RclConfig::initUserConfig()
{
    // Explanatory text
    const int bs = sizeof(blurb0)+MYPATHALLOC+1;
    char blurb[bs];
    string exdir = path_cat(m_datadir, "examples");
    snprintf(blurb, bs, blurb0, exdir.c_str());

    // Use protective 700 mode to create the top configuration
    // directory: documents can be reconstructed from index data.
    if (!path_exists(m_confdir) && 
        mkdir(m_confdir.c_str(), 0700) < 0) {
        m_reason += string("mkdir(") + m_confdir + ") failed: " + 
            strerror(errno);
        return false;
    }
    string lang = localelang();
    for (int i = 0; i < ncffiles; i++) {
        string dst = path_cat(m_confdir, string(configfiles[i])); 
        if (!path_exists(dst)) {
            FILE *fp = fopen(dst.c_str(), "w");
            if (fp) {
                fprintf(fp, "%s\n", blurb);
                if (!strcmp(configfiles[i], "recoll.conf")) {
                    // Add improved unac_except_trans for some languages
                    if (lang == "se" || lang == "dk" || lang == "no" || 
                        lang == "fi") {
                        fprintf(fp, "%s\n", swedish_ex);
                    } else if (lang == "de") {
                        fprintf(fp, "%s\n", german_ex);
                    }
                }
                fclose(fp);
            } else {
                m_reason += string("fopen ") + dst + ": " + strerror(errno);
                return false;
            }
        }
    }
    return true;
}

void RclConfig::zeroMe() {
    m_ok = false; 
    m_keydirgen = 0;
    m_conf = 0; 
    mimemap = 0; 
    mimeconf = 0; 
    mimeview = 0; 
    m_fields = 0;
    m_ptrans = 0;
    m_stopsuffixes = 0;
    m_maxsufflen = 0;
    initParamStale(0, 0);
}

void RclConfig::freeAll() 
{
    delete m_conf;
    delete mimemap;
    delete mimeconf; 
    delete mimeview; 
    delete m_fields;
    delete m_ptrans;
    delete STOPSUFFIXES;
    // just in case
    zeroMe();
}

void RclConfig::initFrom(const RclConfig& r)
{
    zeroMe();
    if (!(m_ok = r.m_ok))
        return;

    // Copyable fields
    m_ok = r.m_ok;
    m_reason = r.m_reason;
    m_confdir = r.m_confdir;
    m_cachedir = r.m_cachedir;
    m_datadir = r.m_datadir;
    m_keydir = r.m_keydir;
    m_keydirgen = r.m_keydirgen;
    m_cdirs = r.m_cdirs;
    m_fldtotraits = r.m_fldtotraits;
    m_aliastocanon = r.m_aliastocanon;
    m_aliastoqcanon = r.m_aliastoqcanon;
    m_storedFields = r.m_storedFields;
    m_xattrtofld = r.m_xattrtofld;
    m_maxsufflen = r.m_maxsufflen;
    m_skpnlist = r.m_skpnlist;
    m_onlnlist = r.m_onlnlist;
    m_stopsuffixes = r.m_stopsuffixes;
    m_defcharset = r.m_defcharset;
    m_restrictMTypes  = r.m_restrictMTypes;
    m_excludeMTypes = r.m_excludeMTypes;
    m_thrConf = r.m_thrConf;
    m_mdreapers = r.m_mdreapers;

    // Special treatment
    if (r.m_conf)
        m_conf = new ConfStack<ConfTree>(*(r.m_conf));
    if (r.mimemap)
        mimemap = new ConfStack<ConfTree>(*(r.mimemap));
    if (r.mimeconf)
        mimeconf = new ConfStack<ConfSimple>(*(r.mimeconf));
    if (r.mimeview)
        mimeview = new ConfStack<ConfSimple>(*(r.mimeview));
    if (r.m_fields)
        m_fields = new ConfStack<ConfSimple>(*(r.m_fields));
    if (r.m_ptrans)
        m_ptrans = new ConfSimple(*(r.m_ptrans));
    if (r.m_stopsuffixes)
        m_stopsuffixes = new SuffixStore(*((SuffixStore*)r.m_stopsuffixes));
    initParamStale(m_conf, mimemap);
}

void RclConfig::initParamStale(ConfNull *cnf, ConfNull *mimemap)
{
    m_oldstpsuffstate.init(mimemap);
    m_stpsuffstate.init(cnf);
    m_skpnstate.init(cnf);
    m_onlnstate.init(cnf);
    m_rmtstate.init(cnf);
    m_xmtstate.init(cnf);
    m_mdrstate.init(cnf);
}
