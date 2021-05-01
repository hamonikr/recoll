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
#include "autoconfig.h"

#include "webqueue.h"

#include <string.h>
#include <errno.h>
#include "safesysstat.h"
#include "safeunistd.h"

#include "cstr.h"
#include "pathut.h"
#include "rclutil.h"
#include "log.h"
#include "fstreewalk.h"
#include "webstore.h"
#include "circache.h"
#include "smallut.h"
#include "fileudi.h"
#include "internfile.h"
#include "wipedir.h"
#include "indexer.h"
#include "readfile.h"
#include "conftree.h"
#include "transcode.h"
#include "cancelcheck.h"

#include <vector>
#include <fstream>

using namespace std;

#define DOTFILEPREFIX "_"

// The browser plugin creates a file named .xxx (where xxx is the name
// for the main file in the queue), to hold external metadata (http or
// created by the plugin).  This class reads the .xxx, dotfile, and turns
// it into an Rcl::Doc holder
class WebQueueDotFile {
public:
    WebQueueDotFile(RclConfig *conf, const string& fn)
        : m_conf(conf), m_fn(fn)
        {}

    // Read input line, strip it of eol and return as c++ string
    bool readLine(ifstream& input, string& line) {
        static const int LL = 2048;
        char cline[LL]; 
        cline[0] = 0;
        input.getline(cline, LL-1);
        if (!input.good()) {
            if (input.bad()) {
                LOGERR("WebQueueDotFileRead: input.bad()\n");
            }
            return false;
        }
        int ll = strlen(cline);
        while (ll > 0 && (cline[ll-1] == '\n' || cline[ll-1] == '\r')) {
            cline[ll-1] = 0;
            ll--;
        }
        line.assign(cline, ll);
        LOGDEB2("WebQueueDotFile:readLine: [" << line << "]\n");
        return true;
    }

    // Process a Web queue dot file and set interesting stuff in the doc
    bool toDoc(Rcl::Doc& doc) {
        string line;
        ifstream input;

        input.open(m_fn.c_str(), ios::in);
        if (!input.good()) {
            LOGERR("WebQueueDotFile: open failed for [" << m_fn << "]\n");
            return false;
        }

        // Read the 3 first lines: 
        // - url
        // - hit type: we only know about Bookmark and WebHistory for now
        // - content-type.
        if (!readLine(input, line))
            return false;
        doc.url = line;
        if (!readLine(input, line))
            return false;
        doc.meta[Rcl::Doc::keybght] = line;
        if (!readLine(input, line))
            return false;
        doc.mimetype = line;

        // We set the bookmarks mtype as html (the text is empty
        // anyway), so that the html viewer will be called on 'Open'
        bool isbookmark = false;
        if (!stringlowercmp("bookmark", doc.meta[Rcl::Doc::keybght])) {
            isbookmark = true;
            doc.mimetype = "text/html";
        }

        string confstr;
        string ss(" ");
        // Read the rest: fields and keywords. We do a little
        // massaging of the input lines, then use a ConfSimple to
        // parse, and finally insert the key/value pairs into the doc
        // meta[] array
        for (;;) {
            if (!readLine(input, line)) {
                // Eof hopefully
                break;
            }
            if (line.find("t:") != 0)
                continue;
            line = line.substr(2);
            confstr += line + "\n";
        }
        ConfSimple fields(confstr, 1);
        vector<string> names = fields.getNames(cstr_null);
        for (vector<string>::iterator it = names.begin();
             it != names.end(); it++) {
            string value;
            fields.get(*it, value, cstr_null);
            if (!value.compare("undefined") || !value.compare("null"))
                continue;

            string *valuep = &value;
            string cvalue;
            if (isbookmark) {
                // It appears that bookmarks are stored in the users'
                // locale charset (not too sure). No idea what to do
                // for other types, would have to check the plugin.
                string charset = m_conf->getDefCharset(true);
                transcode(value, cvalue, charset,  "UTF-8"); 
                valuep = &cvalue;
            }
                
            string caname = m_conf->fieldCanon(*it);
            doc.meta[caname].append(ss + *valuep);
        }

        // Finally build the confsimple that we will save to the
        // cache, from the doc fields. This could also be done in
        // parallel with the doc.meta build above, but simpler this
        // way.  We need it because not all interesting doc fields are
        // in the meta array (ie: mimetype, url), and we want
        // something homogenous and easy to save.
        for (const auto& entry : doc.meta) {
            m_fields.set(entry.first, entry.second, cstr_null);
        }
        m_fields.set(cstr_url, doc.url, cstr_null);
        m_fields.set(cstr_bgc_mimetype, doc.mimetype, cstr_null);

        return true;
    }

    RclConfig *m_conf;
    ConfSimple m_fields;
    string m_fn;
};

// Initialize. Compute paths and create a temporary directory that will be
// used by internfile()
WebQueueIndexer::WebQueueIndexer(RclConfig *cnf, Rcl::Db *db,
                                 DbIxStatusUpdater *updfunc)
    : m_config(cnf), m_db(db), m_cache(0), m_updater(updfunc), 
      m_nocacheindex(false)
{
    m_queuedir = m_config->getWebQueueDir();
    path_catslash(m_queuedir);
    m_cache = new WebStore(cnf);
}

WebQueueIndexer::~WebQueueIndexer()
{
    LOGDEB("WebQueueIndexer::~\n");
    deleteZ(m_cache);
}

// Index document stored in the cache. 
bool WebQueueIndexer::indexFromCache(const string& udi)
{
    if (!m_db)
        return false;

    CancelCheck::instance().checkCancel();

    Rcl::Doc dotdoc;
    string data;
    string hittype;

    if (!m_cache || !m_cache->getFromCache(udi, dotdoc, data, &hittype)) {
        LOGERR("WebQueueIndexer::indexFromCache: cache failed\n");
        return false;
    }

    if (hittype.empty()) {
        LOGERR("WebQueueIndexer::index: cc entry has no hit type\n");
        return false;
    }
        
    if (!stringlowercmp("bookmark", hittype)) {
        // Just index the dotdoc
        dotdoc.meta[Rcl::Doc::keybcknd] = "BGL";
        return m_db->addOrUpdate(udi, cstr_null, dotdoc);
    } else {
        Rcl::Doc doc;
        FileInterner interner(data, m_config, 
                              FileInterner::FIF_doUseInputMimetype,
                              dotdoc.mimetype);
        FileInterner::Status fis;
        try {
            fis = interner.internfile(doc);
        } catch (CancelExcept) {
            LOGERR("WebQueueIndexer: interrupted\n");
            return false;
        }
        if (fis != FileInterner::FIDone) {
            LOGERR("WebQueueIndexer: bad status from internfile\n");
            return false;
        }

        doc.mimetype = dotdoc.mimetype;
        doc.fmtime = dotdoc.fmtime;
        doc.url = dotdoc.url;
        doc.pcbytes = dotdoc.pcbytes;
        doc.sig.clear();
        doc.meta[Rcl::Doc::keybcknd] = "BGL";
        return m_db->addOrUpdate(udi, cstr_null, doc);
    }
}

void WebQueueIndexer::updstatus(const string& udi)
{
    if (m_updater) {
        ++(m_updater->status.docsdone);
        if (m_updater->status.dbtotdocs < m_updater->status.docsdone)
            m_updater->status.dbtotdocs = m_updater->status.docsdone;
        m_updater->status.fn = udi;
        m_updater->update();
    }
}

bool WebQueueIndexer::index()
{
    if (!m_db)
        return false;
    LOGDEB("WebQueueIndexer::processqueue: [" << m_queuedir << "]\n");
    m_config->setKeyDir(m_queuedir);
    if (!path_makepath(m_queuedir, 0700)) {
        LOGERR("WebQueueIndexer:: can't create queuedir [" << m_queuedir <<
               "] errno " << errno << "\n");
        return false;
    }
    if (!m_cache || !m_cache->cc()) {
        LOGERR("WebQueueIndexer: cache initialization failed\n");
        return false;
    }
    CirCache *cc = m_cache->cc();
    // First check/index files found in the cache. If the index was reset,
    // this actually does work, else it sets the existence flags (avoid
    // purging). We don't do this when called from indexFiles
    if (!m_nocacheindex) {
        bool eof;
        if (!cc->rewind(eof)) {
            // rewind can return eof if the cache is empty
            if (!eof)
                return false;
        }
        int nentries = 0;
        do {
            string udi;
            if (!cc->getCurrentUdi(udi)) {
                LOGERR("WebQueueIndexer:: cache file damaged\n");
                break;
            }
            if (udi.empty())
                continue;
            if (m_db->needUpdate(udi, cstr_null)) {
                try {
                    // indexFromCache does a CirCache::get(). We could
                    // arrange to use a getCurrent() instead, would be more 
                    // efficient
                    indexFromCache(udi);
                    updstatus(udi);
                } catch (CancelExcept) {
                    LOGERR("WebQueueIndexer: interrupted\n");
                    return false;
                }
            }
            nentries++;
        } while (cc->next(eof));
    }

    // Finally index the queue
    FsTreeWalker walker(FsTreeWalker::FtwNoRecurse);
    walker.addSkippedName(DOTFILEPREFIX "*");
    FsTreeWalker::Status status = walker.walk(m_queuedir, *this);
    LOGDEB("WebQueueIndexer::processqueue: done: status " << status << "\n");
    return true;
}

// Index a list of files (sent by the real time monitor)
bool WebQueueIndexer::indexFiles(list<string>& files)
{
    LOGDEB("WebQueueIndexer::indexFiles\n");

    if (!m_db) {
        LOGERR("WebQueueIndexer::indexfiles no db??\n");
        return false;
    }
    for (list<string>::iterator it = files.begin(); it != files.end();) {
        if (it->empty()) {//??
            it++; continue;
        }
        string father = path_getfather(*it);
        if (father.compare(m_queuedir)) {
            LOGDEB("WebQueueIndexer::indexfiles: skipping [" << *it << "] (nq)\n");
            it++; continue;
        }
        // Pb: we are often called with the dot file, before the
        // normal file exists, and sometimes never called for the
        // normal file afterwards (ie for bookmarks where the normal
        // file is empty). So we perform a normal queue run at the end
        // of the function to catch older stuff. Still this is not
        // perfect, sometimes some files will not be indexed before
        // the next run.
        string fn = path_getsimple(*it);
        if (fn.empty() || fn.at(0) == '.') {
            it++; continue;
        }
        struct stat st;
        if (path_fileprops(*it, &st) != 0) {
            LOGERR("WebQueueIndexer::indexfiles: cant stat [" << *it << "]\n");
            it++; continue;
        }
        if (!S_ISREG(st.st_mode)) {
            LOGDEB("WebQueueIndexer::indexfiles: skipping [" << *it <<
                   "] (nr)\n");
            it++; continue;
        }

        processone(*it, &st, FsTreeWalker::FtwRegular);
        it = files.erase(it);
    }
    m_nocacheindex = true;
    index();
    // Note: no need to reset nocacheindex, we're in the monitor now
    return true;
}

FsTreeWalker::Status 
WebQueueIndexer::processone(const string &path,
                            const struct stat *stp,
                            FsTreeWalker::CbFlag flg)
{
    if (!m_db) //??
        return FsTreeWalker::FtwError;

    bool dounlink = false;

    if (flg != FsTreeWalker::FtwRegular) 
        return FsTreeWalker::FtwOk;

    string dotpath = path_cat(path_getfather(path), 
                              string(DOTFILEPREFIX) + path_getsimple(path));
    LOGDEB("WebQueueIndexer: prc1: [" << path << "]\n");

    WebQueueDotFile dotfile(m_config, dotpath);
    Rcl::Doc dotdoc;
    string udi, udipath;
    if (!dotfile.toDoc(dotdoc))
        goto out;
    //dotdoc.dump(1);

    // Have to use the hit type for the udi, because the same url can exist
    // as a bookmark or a page.
    udipath = path_cat(dotdoc.meta[Rcl::Doc::keybght], url_gpath(dotdoc.url));
    make_udi(udipath, cstr_null, udi);

    LOGDEB("WebQueueIndexer: prc1: udi [" << udi << "]\n");
    char ascdate[30];
    sprintf(ascdate, "%ld", long(stp->st_mtime));

    if (!stringlowercmp("bookmark", dotdoc.meta[Rcl::Doc::keybght])) {
        // For bookmarks, we just index the doc that was built from the
        // metadata.
        if (dotdoc.fmtime.empty())
            dotdoc.fmtime = ascdate;

        dotdoc.pcbytes = lltodecstr(stp->st_size);

        // Document signature for up to date checks: none. 
        dotdoc.sig.clear();
        
        dotdoc.meta[Rcl::Doc::keybcknd] = "BGL";
        if (!m_db->addOrUpdate(udi, cstr_null, dotdoc)) 
            return FsTreeWalker::FtwError;

    } else {
        Rcl::Doc doc;
        // Store the dotdoc fields in the future doc. In case someone wants
        // to use fields generated by the browser plugin like inurl
        doc.meta = dotdoc.meta;

        FileInterner interner(path, stp, m_config,
                              FileInterner::FIF_doUseInputMimetype,
                              &dotdoc.mimetype);
        FileInterner::Status fis;
        try {
            fis = interner.internfile(doc);
        } catch (CancelExcept) {
            LOGERR("WebQueueIndexer: interrupted\n");
            goto out;
        }
        if (fis != FileInterner::FIDone && fis != FileInterner::FIAgain) {
            LOGERR("WebQueueIndexer: bad status from internfile\n");
            // TOBEDONE: internfile can return FIAgain here if it is
            // paging a big text file, we should loop. Means we're
            // only indexing the first page for text/plain files
            // bigger than the page size (dlft: 1MB) for now.
            goto out;
        }

        if (doc.fmtime.empty())
            doc.fmtime = ascdate;
        dotdoc.fmtime = doc.fmtime;

        doc.pcbytes = lltodecstr(stp->st_size);
        // Document signature for up to date checks: none. 
        doc.sig.clear();
        doc.url = dotdoc.url;

        doc.meta[Rcl::Doc::keybcknd] = "BGL";
        if (!m_db->addOrUpdate(udi, cstr_null, doc)) 
            return FsTreeWalker::FtwError;
    }

    // Copy to cache
    {
        // doc fields not in meta, needing saving to the cache
        dotfile.m_fields.set("fmtime", dotdoc.fmtime, cstr_null);
        // fbytes is used for historical reasons, should be pcbytes, but makes
        // no sense to change.
        dotfile.m_fields.set(cstr_fbytes, dotdoc.pcbytes, cstr_null);
        dotfile.m_fields.set("udi", udi, cstr_null);
        string fdata;
        file_to_string(path, fdata);
        if (!m_cache || !m_cache->cc()) {
            LOGERR("WebQueueIndexer: cache initialization failed\n");
            goto out;
        }
        if (!m_cache->cc()->put(udi, &dotfile.m_fields, fdata, 0)) {
            LOGERR("WebQueueIndexer::prc1: cache_put failed; " <<
                   m_cache->cc()->getReason() << "\n");
            goto out;
        }
    }
    updstatus(udi);
    dounlink = true;
out:
    if (dounlink) {
        if (unlink(path.c_str())) {
            LOGSYSERR("WebQueueIndexer::processone", "unlink", path);
        }
        if (unlink(dotpath.c_str())) {
            LOGSYSERR("WebQueueIndexer::processone", "unlink", dotpath);
        }
    }
    return FsTreeWalker::FtwOk;
}
