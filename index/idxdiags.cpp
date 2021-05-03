/* Copyright (C) 2021 J.F.Dockes
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

#include "autoconfig.h"

#include <stdio.h>
#include <mutex>

#include "idxdiags.h"

static std::mutex diagmutex;

class IdxDiags::Internal {
public:
    ~Internal() {
        if (fp) {
            fclose(fp);
        }
    }
    FILE *fp{nullptr};
};

IdxDiags::IdxDiags()
{
    m = new Internal;
}

IdxDiags::~IdxDiags()
{
    delete m;
}

bool IdxDiags::flush()
{
    std::unique_lock<std::mutex> lock(diagmutex);
    if (m && m->fp) {
        return fflush(m->fp) ? false : true;
    }
    return true;
}

static IdxDiags *theInstance;

IdxDiags& IdxDiags::theDiags()
{
    if (nullptr == theInstance) {
        theInstance = new IdxDiags;
    }
    return *theInstance;
}

bool IdxDiags::init(const std::string& outpath)
{
    m->fp = fopen(outpath.c_str(), "w");
    if (nullptr == m->fp) {
        return false;
    }
    return true;
}

bool IdxDiags::record(DiagKind diag, const std::string& path, const std::string& detail)
{
    if (nullptr == m || nullptr == m->fp || (path.empty() && detail.empty())) {
        return true;
    }
    const char *skind = "Unknown";
    switch (diag) {
    case Ok: skind = "Ok";break;
    case Skipped: skind = "Skipped";break;
    case NoContentSuffix: skind = "NoContentSuffix";break;
    case MissingHelper: skind = "MissingHelper";break;
    case Error: skind = "Error";break;
    case NoHandler: skind = "NoHandler";break;
    case ExcludedMime: skind = "ExcludedMime";break;
    case NotIncludedMime: skind = "NotIncludedMime";break;
    }

    std::unique_lock<std::mutex> lock(diagmutex);
    fprintf(m->fp, "%s %s | %s\n", skind, path.c_str(), detail.c_str());
    return true;
}
