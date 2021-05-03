/* Copyright (C) 2017-2020 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "utf8iter.h"

#include <algorithm>
#include <unordered_set>
#include <iostream>

using namespace std;

void utf8truncate(string& s, int maxlen, int flags, const string& ellipsis,
                  const string& ws)
{
    if (s.size() <= string::size_type(maxlen)) {
        return;
    }
    unordered_set<int> wss;
    if (flags & UTF8T_ATWORD) {
        Utf8Iter iter(ws);
        for (; !iter.eof(); iter++) {
            unsigned int c = *iter;
            wss.insert(c);
        }
    }

    if (flags & UTF8T_ELLIPSIS) {
        size_t ellen = utf8len(ellipsis);
        maxlen = std::max(0, maxlen - int(ellen));
    }

    Utf8Iter iter(s);
    string::size_type pos = 0;
    string::size_type lastwspos = 0;
    for (; !iter.eof(); iter++) {
        unsigned int c = *iter;
        if (iter.getBpos() < string::size_type(maxlen)) {
            pos = iter.getBpos() + iter.getBlen();
            if ((flags & UTF8T_ATWORD) && wss.find(c) != wss.end()) {
                lastwspos = pos;
            }
        } else {
            break;
        }
    }

    if (flags & UTF8T_ATWORD) {
        s.erase(lastwspos);
        for (;;) {
            Utf8Iter iter(s);
            unsigned int c = 0;
            for (; !iter.eof(); iter++) {
                c = *iter;
                pos = iter.getBpos();
            }
            if (wss.find(c) == wss.end()) {
                break;
            }
            s.erase(pos);
        }
    } else {
        s.erase(pos);
    }

    if (flags & UTF8T_ELLIPSIS) {
        s += ellipsis;
    }
}

size_t utf8len(const string& s)
{
    size_t len = 0;
    Utf8Iter iter(s);
    while (iter++ != string::npos) {
        len++;
    }
    return len;
}

static const std::string replchar{"\xef\xbf\xbd"};

// Check utf-8 encoding, replacing errors with the ? char above
int utf8check(const std::string& in, bool fixit, std::string *out, int maxrepl)
{
    int cnt = 0;
    Utf8Iter it(in);
    for (;!it.eof(); it++) {
        if (it.error()) {
            if (!fixit) {
                return -1;
            }
            *out += replchar;
            ++cnt;
            for (; cnt < maxrepl; cnt++) {
                it.retryfurther();
                if (it.eof())
                    return cnt;
                if (!it.error())
                    break;
                *out += replchar;
            }
            if (it.error()) {
                return -1;
            }
        }
        // We have reached a good char and eof is false
        if (fixit) {
            it.appendchartostring(*out);
        }
    }
    return cnt;
}
