/* Copyright (C) 2004-2019 J.F.Dockes
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

#include "utf8fn.h"
#include "rclconfig.h"
#include "transcode.h"
#include "log.h"

using namespace std;

string compute_utf8fn(const RclConfig *config, const string& ifn, bool simple)
{
    string lfn(simple ? path_getsimple(ifn) : ifn);
#ifdef _WIN32
    PRETEND_USE(config);
    // On windows file names are read as UTF16 wchar_t and converted to UTF-8
    // while scanning directories
    return lfn;
#else
    string charset = config->getDefCharset(true);
    string utf8fn; 
    int ercnt;
    if (!transcode(lfn, utf8fn, charset, "UTF-8", &ercnt)) {
        LOGERR("compute_utf8fn: fn transcode failure from ["  << charset <<
               "] to UTF-8 for: [" << lfn << "]\n");
    } else if (ercnt) {
        LOGDEB("compute_utf8fn: "  << ercnt << " transcode errors from [" <<
               charset << "] to UTF-8 for: ["  << lfn << "]\n");
    }
    LOGDEB1("compute_utf8fn: transcoded from ["  << lfn << "] to ["  <<
            utf8fn << "] ("  << charset << "->"  << "UTF-8)\n");
    return utf8fn;
#endif
}
