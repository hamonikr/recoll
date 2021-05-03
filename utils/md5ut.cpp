/* Copyright (C) 2015 J.F.Dockes
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
#include <string.h>

#include "md5ut.h"
#include "readfile.h"

using namespace std;

// Quite incredibly if this class is named FileScanMd5 like the
// different one in readfile.cpp, the vtables get mixed up and mh_xslt
// crashes while calling a virtual function (gcc 6.3 and 7.3)
class FileScanMd5loc : public FileScanDo {
public:
    FileScanMd5loc(string& d) : digest(d) {}
    virtual bool init(int64_t, string *)
    {
    MD5Init(&ctx);
    return true;
    }
    virtual bool data(const char *buf, int cnt, string*)
    {
    MD5Update(&ctx, (const unsigned char*)buf, cnt);
    return true;
    }
    string &digest;
    MD5_CTX ctx;
};

bool MD5File(const string& filename, string &digest, string *reason)
{
    FileScanMd5loc md5er(digest);
    if (!file_scan(filename, &md5er, reason))
    return false;
    // We happen to know that digest and md5er.digest are the same object
    MD5Final(md5er.digest, &md5er.ctx);
    return true;
}
