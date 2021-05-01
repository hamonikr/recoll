/* Copyright (C) 2017-2019 J.F.Dockes
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
#include <string>

using std::string;

void utf8truncate(std::string& s, int maxlen)
{
    if (s.size() <= string::size_type(maxlen)) {
        return;
    }
    Utf8Iter iter(s);
    string::size_type pos = 0;
    while (iter++ != string::npos)
        if (iter.getBpos() < string::size_type(maxlen)) {
            pos = iter.getBpos();
        }

    s.erase(pos);
}
