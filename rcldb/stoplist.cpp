/* Copyright (C) 2007 J.F.Dockes
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

#include "log.h"
#include "readfile.h"
#include "unacpp.h"
#include "smallut.h"
#include "stoplist.h"

namespace Rcl 
{

bool StopList::setFile(const string &filename)
{
    m_stops.clear();
    string stoptext, reason;
    if (!file_to_string(filename, stoptext, &reason)) {
        LOGDEB0("StopList::StopList: file_to_string(" << filename <<
                ") failed: " << reason << "\n");
        return false;
    }
    set<string> stops;
    stringToStrings(stoptext, stops);
    for (set<string>::iterator it = stops.begin(); 
         it != stops.end(); it++) {
        string dterm;
        unacmaybefold(*it, dterm, "UTF-8", UNACOP_UNACFOLD);
        m_stops.insert(dterm);
    }

    return true;
}

// Most sites will have an empty stop list. We try to optimize the
// empty set case as much as possible. empty() is probably sligtly
// faster than find() in this case.
bool StopList::isStop(const string &term) const
{
    return m_stops.empty() ? false : m_stops.find(term) != m_stops.end();
}

}


