/*
 *   Copyright 2004 J.F.Dockes
 *
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
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <mutex>
using namespace std;

#include "cstr.h"
#include "mimehandler.h"
#include "log.h"
#include "rclconfig.h"
#include "smallut.h"
#include "md5ut.h"
#include "mh_exec.h"
#include "mh_execm.h"
#include "mh_html.h"
#include "mh_mail.h"
#include "mh_mbox.h"
#include "mh_text.h"
#include "mh_symlink.h"
#include "mh_unknown.h"
#include "mh_null.h"
#include "mh_xslt.h"
#include "rcldoc.h"
#include "rclutil.h"

// Performance help: we use a pool of already known and created
// handlers. There can be several instances for a given mime type
// (think email attachment in email message: 2 rfc822 handlers are
// needed simulteanously)
static multimap<string, RecollFilter*>  o_handlers;
static list<multimap<string, RecollFilter*>::iterator> o_hlru;
typedef list<multimap<string, RecollFilter*>::iterator>::iterator hlruit_tp;

static std::mutex o_handlers_mutex;

static const unsigned int max_handlers_cache_size = 100;

/* Look for mime handler in pool */
static RecollFilter *getMimeHandlerFromCache(const string& key)
{
    std::unique_lock<std::mutex> locker(o_handlers_mutex);
    string xdigest;
    MD5HexPrint(key, xdigest);
    LOGDEB("getMimeHandlerFromCache: " << xdigest << " cache size " <<
           o_handlers.size() << "\n");

    multimap<string, RecollFilter *>::iterator it = o_handlers.find(key);
    if (it != o_handlers.end()) {
        RecollFilter *h = it->second;
        hlruit_tp it1 = find(o_hlru.begin(), o_hlru.end(), it);
        if (it1 != o_hlru.end()) {
            o_hlru.erase(it1);
        } else {
            LOGERR("getMimeHandlerFromCache: lru position not found\n");
        }
        o_handlers.erase(it);
        LOGDEB("getMimeHandlerFromCache: " << xdigest << " found size " <<
               o_handlers.size() << "\n");
        return h;
    }
    LOGDEB("getMimeHandlerFromCache: " << xdigest << " not found\n");
    return 0;
}

/* Return mime handler to pool */
void returnMimeHandler(RecollFilter *handler)
{
    typedef multimap<string, RecollFilter*>::value_type value_type;

    if (handler == 0) {
        LOGERR("returnMimeHandler: bad parameter\n");
        return;
    }
    handler->clear();

    std::unique_lock<std::mutex> locker(o_handlers_mutex);

    LOGDEB("returnMimeHandler: returning filter for " <<
           handler->get_mime_type() << " cache size " << o_handlers.size() <<
           "\n");

    // Limit pool size. The pool can grow quite big because there are
    // many filter types, each of which can be used in several copies
    // at the same time either because it occurs several times in a
    // stack (ie mail attachment to mail), or because several threads
    // are processing the same mime type at the same time.
    multimap<string, RecollFilter *>::iterator it;
    if (o_handlers.size() >= max_handlers_cache_size) {
        static int once = 1;
        if (once) {
            once = 0;
            for (it = o_handlers.begin(); it != o_handlers.end(); it++) {
                LOGDEB1("Cache full. key: " << it->first << "\n");
            }
            LOGDEB1("Cache LRU size: " << o_hlru.size() << "\n");
        }
        if (o_hlru.size() > 0) {
            it = o_hlru.back();
            o_hlru.pop_back();
            delete it->second;
            o_handlers.erase(it);
        }
    }
    it = o_handlers.insert(value_type(handler->get_id(), handler));
    o_hlru.push_front(it);
}

void clearMimeHandlerCache()
{
    LOGDEB("clearMimeHandlerCache()\n");
    multimap<string, RecollFilter *>::iterator it;
    std::unique_lock<std::mutex> locker(o_handlers_mutex);
    for (it = o_handlers.begin(); it != o_handlers.end(); it++) {
        delete it->second;
    }
    o_handlers.clear();
    TempFile::tryRemoveAgain();
}

/** For mime types set as "internal" in mimeconf: 
 * create appropriate handler object. */
static RecollFilter *mhFactory(RclConfig *config, const string &mimeOrParams,
                               bool nobuild, string& id)
{
    LOGDEB1("mhFactory(" << mimeOrParams << ")\n");
    vector<string> lparams;
    stringToStrings(mimeOrParams, lparams);
    if (lparams.empty()) {
        // ??
        return nullptr;
    }
    string lmime(lparams[0]);
    stringtolower(lmime);
    if (cstr_textplain == lmime) {
        LOGDEB2("mhFactory(" << mime << "): returning MimeHandlerText\n");
        MD5String("MimeHandlerText", id);
        return nobuild ? 0 : new MimeHandlerText(config, id);
    } else if (cstr_texthtml == lmime) {
        LOGDEB2("mhFactory(" << mime << "): returning MimeHandlerHtml\n");
        MD5String("MimeHandlerHtml", id);
        return nobuild ? 0 : new MimeHandlerHtml(config, id);
    } else if ("text/x-mail" == lmime) {
        LOGDEB2("mhFactory(" << mime << "): returning MimeHandlerMbox\n");
        MD5String("MimeHandlerMbox", id);
        return nobuild ? 0 : new MimeHandlerMbox(config, id);
    } else if ("message/rfc822" == lmime) {
        LOGDEB2("mhFactory(" << mime << "): returning MimeHandlerMail\n");
        MD5String("MimeHandlerMail", id);
        return nobuild ? 0 : new MimeHandlerMail(config, id);
    } else if ("inode/symlink" == lmime) {
        LOGDEB2("mhFactory(" << mime << "): returning MimeHandlerSymlink\n");
        MD5String("MimeHandlerSymlink", id);
        return nobuild ? 0 : new MimeHandlerSymlink(config, id);
    } else if ("application/x-zerosize" == lmime) {
        LOGDEB("mhFactory(" << lmime << "): returning MimeHandlerNull\n");
        MD5String("MimeHandlerNull", id);
        return nobuild ? 0 : new MimeHandlerNull(config, id);
    } else if (lmime.find("text/") == 0) {
        // Try to handle unknown text/xx as text/plain. This
        // only happen if the text/xx was defined as "internal" in
        // mimeconf, not at random. For programs, for example this
        // allows indexing and previewing as text/plain (no filter
        // exec) but still opening with a specific editor.
        LOGDEB2("mhFactory(" << mime << "): returning MimeHandlerText(x)\n");
        MD5String("MimeHandlerText", id);
        return nobuild ? 0 : new MimeHandlerText(config, id);
    } else if ("xsltproc" == lmime) {
        // XML Types processed with one or several xslt style sheets.
        MD5String(mimeOrParams, id);
        return nobuild ? 0 : new MimeHandlerXslt(config, id, lparams);
    } else {
        // We should not get there. It means that "internal" was set
        // as a handler in mimeconf for a mime type we actually can't
        // handle.
        LOGERR("mhFactory: mime type [" << lmime <<
               "] set as internal but unknown\n");
        MD5String("MimeHandlerUnknown", id);
        return nobuild ? 0 : new MimeHandlerUnknown(config, id);
    }
}

static const string cstr_mh_charset("charset");
/**
 * Create a filter that executes an external program or script
 * A filter def can look like:
 *      someprog -v -t " h i j";charset= xx; mimetype=yy
 * A semi-colon list of attr=value pairs may come after the exec spec.
 * This list is treated by replacing semi-colons with newlines and building
 * a confsimple. This is done quite brutally and we don't support having
 * a ';' inside a quoted string for now. Can't see a use for it.
 */
MimeHandlerExec *mhExecFactory(RclConfig *cfg, const string& mtype, string& hs,
                               bool multiple, const string& id)
{
    ConfSimple attrs;
    string cmdstr;

    if (!cfg->valueSplitAttributes(hs, cmdstr, attrs)) {
        LOGERR("mhExecFactory: bad config line for [" <<
               mtype << "]: [" << hs << "]\n");
        return 0;
    }

    // Split command name and args, and build exec object
    vector<string> cmdtoks;
    stringToStrings(cmdstr, cmdtoks);
    if (cmdtoks.empty()) {
        LOGERR("mhExecFactory: bad config line for [" << mtype <<
               "]: [" << hs << "]\n");
        return 0;
    }
    MimeHandlerExec *h = multiple ? 
        new MimeHandlerExecMultiple(cfg, id) :
        new MimeHandlerExec(cfg, id);
    vector<string>::iterator it = cmdtoks.begin();

    // Special-case python and perl on windows: we need to also locate the
    // first argument which is the script name "python somescript.py". 
    // On Unix, thanks to #!, we usually just run "somescript.py", but need
    // the same change if we ever want to use the same cmdling as windows
    if (!stringlowercmp("python", *it) || !stringlowercmp("perl", *it)) {
        if (cmdtoks.size() < 2) {
            LOGERR("mhExecFactory: python/perl cmd: no script?. [" <<
                   mtype << "]: [" << hs << "]\n");
        }
        vector<string>::iterator it1(it);
        it1++;
        *it1 = cfg->findFilter(*it1);
    }
            
    h->params.push_back(cfg->findFilter(*it++));
    h->params.insert(h->params.end(), it, cmdtoks.end());

    // Handle additional attributes. We substitute the semi-colons
    // with newlines and use a ConfSimple
    string value;
    if (attrs.get(cstr_mh_charset, value)) 
        h->cfgFilterOutputCharset = stringtolower((const string&)value);
    if (attrs.get(cstr_dj_keymt, value))
        h->cfgFilterOutputMtype = stringtolower((const string&)value);

#if 0
    string scmd;
    for (it = h->params.begin(); it != h->params.end(); it++) {
        scmd += string("[") + *it + "] ";
    }
    LOGDEB("mhExecFactory:mt [" << mtype << "] cfgmt [" <<
           h->cfgFilterOutputMtype << "] cfgcs [" <<
           h->cfgFilterOutputCharset << "] cmd: [" << scmd << "]\n");
#endif

    return h;
}

/* Get handler/filter object for given mime type: */
RecollFilter *getMimeHandler(const string &mtype, RclConfig *cfg, 
                             bool filtertypes)
{
    LOGDEB("getMimeHandler: mtype [" << mtype << "] filtertypes " <<
           filtertypes << "\n");
    RecollFilter *h = 0;

    // Get handler definition for mime type. We do this even if an
    // appropriate handler object may be in the cache.
    // This is fast, and necessary to conform to the
    // configuration, (ie: text/html might be filtered out by
    // indexedmimetypes but an html handler could still be in the
    // cache because it was needed by some other interning stack).
    string hs;
    hs = cfg->getMimeHandlerDef(mtype, filtertypes);
    string id;

    if (!hs.empty()) { 
        // Got a handler definition line
        // Break definition into type (internal/exec/execm) 
        // and name/command string 
        string::size_type b1 = hs.find_first_of(" \t");
        string handlertype = hs.substr(0, b1);
        string cmdstr;
        if (b1 != string::npos) {
            cmdstr = hs.substr(b1);
            trimstring(cmdstr);
        }
        bool internal = !stringlowercmp("internal", handlertype);
        if (internal) {
            // For internal types let the factory compute the cache id
            mhFactory(cfg, cmdstr.empty() ? mtype : cmdstr, true, id);
        } else {
            // exec/execm: use the md5 of the def line
            MD5String(hs, id);
        }

        // Do we already have a handler object in the cache ?
        h = getMimeHandlerFromCache(id);
        if (h != 0)
            goto out;

        LOGDEB2("getMimeHandler: " << mtype << " not in cache\n");
        if (internal) {
            // If there is a parameter after "internal" it's the mime
            // type to use, or the further qualifier (e.g. style sheet
            // name for xslt types). This is so that we can have bogus
            // mime types like text/x-purple-html-log (for ie:
            // specific icon) and still use the html filter on
            // them. This is partly redundant with the
            // localfields/rclaptg, but better? (and the latter will
            // probably go away at some point in the future?).
            LOGDEB2("handlertype internal, cmdstr [" << cmdstr << "]\n");
            h = mhFactory(cfg, cmdstr.empty() ? mtype : cmdstr, false, id);
            goto out;
        } else if (!stringlowercmp("dll", handlertype)) {
        } else {
            if (cmdstr.empty()) {
                LOGERR("getMimeHandler: bad line for " << mtype << ": " <<
                       hs << "\n");
                goto out;
            }
            if (!stringlowercmp("exec", handlertype)) {
                h = mhExecFactory(cfg, mtype, cmdstr, false, id);
                goto out;
            } else if (!stringlowercmp("execm", handlertype)) {
                h = mhExecFactory(cfg, mtype, cmdstr, true, id);
                goto out;
            } else {
                LOGERR("getMimeHandler: bad line for " << mtype << ": " <<
                       hs << "\n");
                goto out;
            }
        }
    } else {
        // No identified mime type, or no handler associated.
        // Unhandled files are either ignored or their name and
        // generic metadata is indexed, depending on configuration
        bool indexunknown = false;
        cfg->getConfParam("indexallfilenames", &indexunknown);
        if (indexunknown) {
            MD5String("MimeHandlerUnknown", id);
            if ((h = getMimeHandlerFromCache(id)) == 0)
                h = new MimeHandlerUnknown(cfg, id);
        }
        goto out;
    }

out:
    if (h) {
        h->set_property(RecollFilter::DEFAULT_CHARSET, cfg->getDefCharset());
        // In multithread context, and in case this handler is out
        // from the cache, it may have a config pointer belonging to
        // another thread. Fix it.
        h->setConfig(cfg);
    }
    return h;
}

/// Can this mime type be interned (according to config) ?
bool canIntern(const std::string mtype, RclConfig *cfg)
{
    if (mtype.empty())
        return false;
    string hs = cfg->getMimeHandlerDef(mtype);
    if (hs.empty())
        return false;
    return true;
}
/// Same, getting MIME from doc
bool canIntern(Rcl::Doc *doc, RclConfig *cfg)
{
    if (doc) {
        return canIntern(doc->mimetype, cfg);
    }
    return false;
}

/// Can this MIME type be opened (has viewer def) ?
bool canOpen(Rcl::Doc *doc, RclConfig *cfg) {
    if (!doc) {
        return false;
    }
    string apptag;
    doc->getmeta(Rcl::Doc::keyapptg, &apptag);
    return !cfg->getMimeViewerDef(doc->mimetype, apptag, false).empty();
}

string RecollFilter::metadataAsString()
{
    string s;
    for (const auto& ent : m_metaData) {
        if (ent.first == "content")
            continue;
        s += ent.first + "->" + ent.second + "\n";
    }
    return s;
}
