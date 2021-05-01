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

#include <string>
#include <iostream>
#include <mutex>

#include <errno.h>
#include <iconv.h>
#include <wchar.h>

#include "transcode.h"
#include "log.h"

using namespace std;

// We gain approximately 25% exec time for word at a time conversions by
// caching the iconv_open thing. 
//
// We may also lose some concurrency on multiproc because of the
// necessary locking, but we only have one processing-intensive
// possible thread for now (the indexing one), so this is probably not
// an issue (and could be worked around with a slightly more
// sohisticated approach).
#define ICONV_CACHE_OPEN

bool transcode(const string &in, string &out, const string &icode,
               const string &ocode, int *ecnt)
{
    LOGDEB2("Transcode: " << icode << " -> " << ocode << "\n");
#ifdef ICONV_CACHE_OPEN
    static iconv_t ic = (iconv_t)-1;
    static string cachedicode;
    static string cachedocode;
    static std::mutex o_cachediconv_mutex;
    std::unique_lock<std::mutex> lock(o_cachediconv_mutex);
#else 
    iconv_t ic;
#endif
    bool ret = false;
    const int OBSIZ = 8192;
    char obuf[OBSIZ], *op;
    bool icopen = false;
    int mecnt = 0;
    out.erase();
    size_t isiz = in.length();
    out.reserve(isiz);
    const char *ip = in.c_str();

#ifdef ICONV_CACHE_OPEN
    if (cachedicode.compare(icode) || cachedocode.compare(ocode)) {
        if (ic != (iconv_t)-1) {
            iconv_close(ic);
            ic = (iconv_t)-1;
        }
#endif
        if((ic = iconv_open(ocode.c_str(), icode.c_str())) == (iconv_t)-1) {
            out = string("iconv_open failed for ") + icode
                + " -> " + ocode;
#ifdef ICONV_CACHE_OPEN
            cachedicode.erase();
            cachedocode.erase();
#endif
            goto error;
        }

#ifdef ICONV_CACHE_OPEN
        cachedicode.assign(icode);
        cachedocode.assign(ocode);
    }
#endif

    icopen = true;

    while (isiz > 0) {
        size_t osiz;
        op = obuf;
        osiz = OBSIZ;

        if(iconv(ic, (ICONV_CONST char **)&ip, &isiz, &op, &osiz) == (size_t)-1
           && errno != E2BIG) {
#if 0
            out.erase();
            out = string("iconv failed for ") + icode + " -> " + ocode +
                " : " + strerror(errno);
#endif
            if (errno == EILSEQ) {
                LOGDEB1("transcode:iconv: bad input seq.: shift, retry\n");
                LOGDEB1(" Input consumed " << ip - in << " output produced " <<
                        out.length() + OBSIZ - osiz << "\n");
                out.append(obuf, OBSIZ - osiz);
                out += "?";
                mecnt++;
                ip++;isiz--;
                continue;
            }
            // Normally only EINVAL is possible here: incomplete
            // multibyte sequence at the end. This is not fatal. Any
            // other is supposedly impossible, we return an error
            if (errno == EINVAL)
                goto out;
            else
                goto error;
        }

        out.append(obuf, OBSIZ - osiz);
    }

#ifndef ICONV_CACHE_OPEN
    icopen = false;
    if(iconv_close(ic) == -1) {
        out.erase();
        out = string("iconv_close failed for ") + icode + " -> " + ocode;
        goto error;
    }
#endif

out:
    ret = true;

error:

    if (icopen) {
#ifndef ICONV_CACHE_OPEN
        iconv_close(ic);
#else
        // Just reset conversion
        iconv(ic, 0, 0, 0, 0);
#endif
    }

    if (mecnt)
        LOGDEB("transcode: [" << icode << "]->[" << ocode << "] " <<
               mecnt << " errors\n");
    if (ecnt)
        *ecnt = mecnt;
    return ret;
}

bool wchartoutf8(const wchar_t *in, std::string& out)
{
    static iconv_t ic = (iconv_t)-1;
    if (ic == (iconv_t)-1) {
        if((ic = iconv_open("UTF-8", "WCHAR_T")) == (iconv_t)-1) {
            LOGERR("wchartoutf8: iconv_open failed\n");
            return false;
        }
    }
    const int OBSIZ = 8192;
    char obuf[OBSIZ], *op;
    out.erase();
    size_t isiz = 2 * wcslen(in);
    out.reserve(isiz);
    const char *ip = (const char *)in;

    while (isiz > 0) {
        size_t osiz;
        op = obuf;
        osiz = OBSIZ;

        if(iconv(ic, (ICONV_CONST char **)&ip, &isiz, &op, &osiz) == (size_t)-1
           && errno != E2BIG) {
            LOGERR("wchartoutf8: iconv error, errno: " << errno << endl);
            return false;
        }
        out.append(obuf, OBSIZ - osiz);
    }
    return true;
}

bool utf8towchar(const std::string& in, wchar_t *out, size_t obytescap)
{
    static iconv_t ic = (iconv_t)-1;
    if (ic == (iconv_t)-1) {
        if((ic = iconv_open("WCHAR_T", "UTF-8")) == (iconv_t)-1) {
            LOGERR("utf8towchar: iconv_open failed\n");
            return false;
        }
    }
    size_t isiz = in.size();
    const char *ip = in.c_str();
    size_t osiz = (size_t)obytescap-2;
    char *op = (char *)out;
    if (iconv(ic, (ICONV_CONST char **)&ip, &isiz, &op, &osiz) == (size_t)-1) {
        LOGERR("utf8towchar: iconv error, errno: " << errno << endl);
        return false;
    }
    *op++ = 0;
    *op = 0;
    return true;
}
