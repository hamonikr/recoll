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

#ifndef _IDXDIAGS_H_INCLUDED_
#define _IDXDIAGS_H_INCLUDED_

#include <string>

class IdxDiags {
public:
    enum DiagKind {Ok, Skipped, NoContentSuffix, MissingHelper, Error, NoHandler,
                   ExcludedMime, NotIncludedMime};

    // Retrieve a reference to the single instance.
    static IdxDiags& theDiags();

    // Initialize, setting the output file path. outpath will be truncated.
    // No locking: this must be called from the main thread, before going multithread.
    // If init is never called, further calls to record() or flush() will be noops.
    bool init(const std::string& outpath);

    // Record a reason for a document not to be indexed. 
    bool record(DiagKind diag, const std::string& path, const std::string& detail = std::string());
    bool flush();
    
    class Internal;
private:
    Internal *m;
    IdxDiags();
    ~IdxDiags();
};

#endif /* _IDXDIAGS_H_INCLUDED_ */
