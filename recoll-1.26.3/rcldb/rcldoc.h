/* Copyright (C) 2006 J.F.Dockes
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
#ifndef _RCLDOC_H_INCLUDED_
#define _RCLDOC_H_INCLUDED_

#include <string>
#include <unordered_map>
#include <vector>

#include "smallut.h"

namespace Rcl {

/**
 * Dumb holder for document attributes and data. 
 * 
 * This is used both for indexing, where fields are filled-up by the
 * indexer prior to adding to the index, and for querying, where
 * fields are filled from data stored in the index. Not all fields are
 * in use at both index and query times, and not all field data is
 * stored at index time.
 */
class Doc {
public:
    ////////////////////////////////////////////////////////////
    // The following fields are stored into the document data record (so they
    // can be accessed after a query without fetching the actual document).
    // We indicate the routine that sets them up during indexing
    
    // Binary or url-encoded url. No transcoding: this is used to access files 
    // Index: computed by Db::add caller. 
    // Query: from doc data.
    std::string url;

    // When we do path translation for documents from external indexes, we
    // save the original path:
    std::string idxurl;
    // And the originating db. 0 is base, 1 first external etc.
    int idxi{0};

    // Internal path for multi-doc files. Ascii
    // Set by FsIndexer::processone    
    std::string ipath;

    // Mime type. Set by FileInterner::internfile
    std::string mimetype;     

    // File modification time as decimal ascii unix time
    // Set by FsIndexer::processone
    std::string fmtime;

    // Data reference date (same format). Ie: mail date
    // Possibly set by mimetype-specific handler
    // Filter::metaData["modificationdate"]
    std::string dmtime;

    // Charset we transcoded the 'text' field from (in case we want back)
    // Possibly set by handler
    std::string origcharset;  

    // A map for textual metadata like, author, keywords, abstract,
    // title.  The entries are possibly set by the mimetype-specific
    // handler. If a fieldname-to-prefix translation exists, the
    // terms in the value will be indexed with a prefix.
    // Only some predefined fields are stored in the data record:
    // "title", "keywords", "abstract", "author", but if a field name is
    // in the "stored" configuration list, it will be stored too.
    std::unordered_map<std::string, std::string> meta; 

    // Attribute for the "abstract" entry. true if it is just the top
    // of doc, not a native document attribute. Not stored directly, but
    // as an indicative prefix at the beginning of the abstract (ugly hack)
    bool syntabs{false};
    
    // File size. This is the size of the compressed file or of the
    // external containing archive.
    // Index: Set by caller prior to Db::Add. 
    // Query: Set from data record
    std::string pcbytes;       

    // Document size, ie, size of the .odt or .xls.
    // Index: Set in internfile from the filter stack
    // Query: set from data record
    std::string fbytes;

    // Doc text size. 
    // Index: from text.length(). 
    // Query: set by rcldb from index data record
    std::string dbytes;

    // Doc signature. Used for up to date checks. 
    // Index: set by Db::Add caller. Query: set from doc data.
    // This is opaque to rcldb, and could just as well be ctime, size,
    // ctime+size, md5, whatever.
    std::string sig;

    /////////////////////////////////////////////////
    // The following fields don't go to the db record, so they can't
    // be retrieved at query time

    // Main document text. This is plaintext utf-8 text to be split
    // and indexed
    std::string text; 

    /////////////////////////////////////////////////
    // Misc stuff

    int pc{0}; // relevancy percentage, used by sortseq, convenience
    unsigned long xdocid{0}; // Opaque: rcldb doc identifier.

    // Page breaks were stored during indexing.
    bool haspages{false};

    // Has children, either as content of file-level container or
    // ipath descendants.
    bool haschildren{false};

    // During indexing: only fields from extended attributes were set, no
    // doc content. Allows for faster reindexing of existing doc
    bool onlyxattr{false};

    ///////////////////////////////////////////////////////////////////

    void erase() {
        url.erase();
        idxurl.erase();
        idxi = 0;
        ipath.erase();
        mimetype.erase();
        fmtime.erase();
        dmtime.erase();
        origcharset.erase();
        meta.clear();
        syntabs = false;
        pcbytes.erase();
        fbytes.erase();
        dbytes.erase();
        sig.erase();

        text.erase();

        pc = 0;
        xdocid = 0;
        haspages = false;
        haschildren = false;
        onlyxattr = false;
    }

    // Copy ensuring no shared string data, for threading issues.
    void copyto(Doc *d) const;

    Doc() { }

    /** Get value for named field. If value pointer is 0, just test existence */
    bool getmeta(const std::string& nm, std::string *value = 0) const {
        const auto it = meta.find(nm);
        if (it != meta.end()) {
            if (value)
                *value = it->second;
            return true;
        } else {
            return false;
        }
    }

    /** Nocopy getvalue. sets pointer to entry value if exists */
    bool peekmeta(const std::string& nm, const std::string **value = 0) const {
        const auto it = meta.find(nm);
        if (it != meta.end()) {
            if (value)
                *value = &(it->second);
            return true;
        } else {
            return false;
        }
    }

    // Create entry or append text to existing entry.
    bool addmeta(const std::string& nm, const std::string& value) {
        auto mit = meta.find(nm);
        if (mit == meta.end()) {
            meta[nm] = value;
        } else if (mit->second.empty()) {
            mit->second = value;
        } else {
            // It may happen that the same attr exists several times
            // in the internfile stack. Avoid duplicating values.
            if (mit->second != value)
                mit->second += std::string(" - ") + value;
        }
        return true;
    }

    /* Is this document stored as a regular filesystem file ?
     * (as opposed to e.g. a webcache file), not a subdoc, 
     */
    bool isFsFile() {
        std::string backend;
        getmeta(keybcknd, &backend);
        if (!backend.empty() && backend.compare("FS"))
            return false;
        return true;
    }


    void dump(bool dotext=false) const;

    ////////////////////////////////////////////////////////////////
    // The official names for recoll native fields when used in a text
    // context (ie: the python interface duplicates some of the fixed
    // fields in the meta array, these are the names used). Defined in
    // rcldoc.cpp. Fields stored in the meta[] array (ie, title,
    // author), _must_ use these canonical values, not aliases. This is 
    // enforced in internfile.cpp and misc other bits of metadata-gathering 
    // code
    static const std::string keyurl; // url
    // childurl. This is set when working with the parent of the result, to hold
    // the child of interest url, typically to highlight a directory entry
    static const std::string keychildurl; 
    // file name. This is set for filesystem-level containers or
    // documents, and not inherited by subdocuments (which can get a
    // keyfn anyway from, e.g, an attachment filename value).  Subdocs
    // used to inherit the file name, but this was undesirable (you
    // usually don't want to see all subdocs when searching for the
    // file name). Instead the container file name is now set in the
    // document record but not indexed (see next entry).
    static const std::string keyfn;  
    // Container file name. This is set for all subdocuments of a
    // given top level container. It is not indexed by default but
    // stored in the document record keyfn field if this is still
    // empty when we create it, for display purposes.
    static const std::string keytcfn;
    static const std::string keyipt; // ipath
    static const std::string keytp;  // mime type
    static const std::string keyfmt; // file mtime
    static const std::string keydmt; // document mtime
    static const std::string keymt;  // mtime dmtime if set else fmtime
    static const std::string keyoc;  // original charset
    static const std::string keypcs; // document outer container size
    static const std::string keyfs;  // document size
    static const std::string keyds;  // document text size
    static const std::string keysz;  // dbytes if set else fbytes else pcbytes
    static const std::string keysig; // sig
    static const std::string keyrr;  // relevancy rating
    static const std::string keycc;  // Collapse count
    static const std::string keyabs; // abstract
    static const std::string keyau;  // author
    static const std::string keytt;  // title
    static const std::string keykw;  // keywords
    static const std::string keymd5; // file md5 checksum
    static const std::string keybcknd; // backend type when not from the fs
    // udi back from index. Only set by Rcl::Query::getdoc().
    static const std::string keyudi;
    static const std::string keyapptg; // apptag. Set from localfields (fs only)
    static const std::string keybght;  // beagle hit type ("beagleHitType")
};

extern bool docsToPaths(std::vector<Doc> &docs,std::vector<std::string> &paths);

}

#endif /* _RCLDOC_H_INCLUDED_ */
