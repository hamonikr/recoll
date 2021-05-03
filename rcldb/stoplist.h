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
#ifndef _STOPLIST_H_INCLUDED_
#define _STOPLIST_H_INCLUDED_

#include <set>
#include <string>

#ifndef NO_NAMESPACES
using std::set;
using std::string;
namespace Rcl 
{
#endif

/**
 * A StopList is just a bunch of strings read from a file. 
 *
 * Some of the string may contain whitespace (that's for experimentation with 
 * stop n-grams), so we take care of dquotes while reading the file. We also 
 * lowercase and remove accents. The source file should be utf-8.
 */
class StopList {
public:
    StopList() {}
    StopList(const string &filename) {setFile(filename);}
    virtual ~StopList() {}

    bool setFile(const string &filename);
    bool isStop(const string &term) const;
    bool hasStops() const {return !m_stops.empty();}

private:
    set<string> m_stops;
};

#ifndef NO_NAMESPACES
}
#endif

#endif /* _STOPLIST_H_INCLUDED_ */
