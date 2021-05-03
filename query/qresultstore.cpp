/* Copyright (C) 2017-2020 J.F.Dockes
 *
 * License: GPL 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "qresultstore.h"

#include <string>
#include <iostream>
#include <map>
#include <vector>

#include <string.h>

#include "rcldoc.h"
#include "rclquery.h"

namespace Rcl {

class QResultStore::Internal {
public:
    bool testentry(const std::pair<std::string,std::string>& entry) {
        return !entry.second.empty() &&
            (isinc ? fieldspec.find(entry.first) != fieldspec.end() :
             fieldspec.find(entry.first) == fieldspec.end());
    }
    
    std::map<std::string, int> keyidx;
    // Notes: offsets[0] is always 0, not really useful, simpler this
    // way. Also could use simple C array instead of c++ vector...
    struct docoffs {
        ~docoffs() {
            free(base);
        }
        char *base{nullptr};
        std::vector<int> offsets;
    };
    std::vector<struct docoffs> docs;
    std::set<std::string> fieldspec;
    bool isinc{false};
};

QResultStore::QResultStore()
{
    m = new Internal;
}
QResultStore::~QResultStore()
{
    delete m;
}

// For reference : Fields normally excluded by uprcl:         
// {"author", "ipath", "rcludi", "relevancyrating", "sig", "abstract", "caption",
//  "filename",  "origcharset", "sig"};


bool QResultStore::storeQuery(Rcl::Query& query, std::set<std::string> fldspec,
    bool isinc)
{
    m->fieldspec = fldspec;
    m->isinc = isinc;
    
    /////////////
    // Enumerate all existing keys and assign array indexes for
    // them. Count documents while we are at it.
    m->keyidx = {{"url",0},
                 {"mimetype", 1},
                 {"fmtime", 2},
                 {"dmtime", 3},
                 {"fbytes", 4},
                 {"dbytes", 5}
    };

    int count = 0;
    for (;;count++) {
        Rcl::Doc doc;
        if (!query.getDoc(count, doc, false)) {
            break;
        }
        for (const auto& entry : doc.meta) {
            if (m->testentry(entry)) {
                auto it = m->keyidx.find(entry.first);
                if (it == m->keyidx.end()) {
                    int idx = m->keyidx.size();
                    m->keyidx.insert({entry.first, idx});
                };
            }
        }
    }

    ///////
    // Populate the main array with doc-equivalent structures.
    
    m->docs.resize(count);
    
    for (int i = 0; i < count; i++) {
        Rcl::Doc doc;
        if (!query.getDoc(i, doc, false)) {
            break;
        }
        auto& vdoc = m->docs[i];
        vdoc.offsets.resize(m->keyidx.size());
        int nbytes = 
            doc.url.size() + 1 +
            doc.mimetype.size() + 1 +
            doc.fmtime.size() + 1 +
            doc.dmtime.size() + 1 +
            doc.fbytes.size() + 1 +
            doc.dbytes.size() + 1;
        for (const auto& entry : doc.meta) {
            if (m->testentry(entry)) {
                if (m->keyidx.find(entry.first) == m->keyidx.end()) {
                    continue;
                }
                nbytes += entry.second.size() + 1;
            }
        }

        char *cp = (char*)malloc(nbytes);
        if (nullptr == cp) {
            abort();
        }

#define STRINGCPCOPY(CHARP, S) do { \
            memcpy(CHARP, S.c_str(), S.size()+1); \
            CHARP += S.size()+1; \
        } while (false);

        vdoc.base = cp;
        vdoc.offsets[0] = cp - vdoc.base;
        STRINGCPCOPY(cp, doc.url);
        vdoc.offsets[1] = cp - vdoc.base;
        STRINGCPCOPY(cp, doc.mimetype);
        vdoc.offsets[2] = cp - vdoc.base;
        STRINGCPCOPY(cp, doc.fmtime);
        vdoc.offsets[3] = cp - vdoc.base;
        STRINGCPCOPY(cp, doc.dmtime);
        vdoc.offsets[4] = cp - vdoc.base;
        STRINGCPCOPY(cp, doc.fbytes);
        vdoc.offsets[5] = cp - vdoc.base;
        STRINGCPCOPY(cp, doc.dbytes);
        for (const auto& entry : doc.meta) {
            if (m->testentry(entry)) {
                auto it = m->keyidx.find(entry.first);
                if (it == m->keyidx.end()) {
                    std::cerr << "Unknown key: " << entry.first << "\n";
                }
                if (it->second <= 5) {
                    // Already done ! Storing another address would be
                    // wasteful and crash when freeing...
                    continue;
                }
                vdoc.offsets[it->second] = cp - vdoc.base;
                STRINGCPCOPY(cp, entry.second);
            }
        }
        // Point all empty entries to the final null byte
        for (unsigned int i = 1; i < vdoc.offsets.size(); i++) {
            if (vdoc.offsets[i] == 0) {
                vdoc.offsets[i] = cp - 1 - vdoc.base;
            }
        }
    }
    return true;
}

int QResultStore::getCount()
{
    return int(m->docs.size());
}

const char *QResultStore::fieldValue(int docindex, const std::string& fldname)
{
    if (docindex < 0 || docindex >= int(m->docs.size())) {
        return nullptr;
    }
    auto& vdoc = m->docs[docindex];

    auto it = m->keyidx.find(fldname);
    if (it == m->keyidx.end() ||
        it->second < 0 || it->second >= int(vdoc.offsets.size())) {
        return nullptr;
    }
    return vdoc.base + vdoc.offsets[it->second];
}

} // namespace Rcl
