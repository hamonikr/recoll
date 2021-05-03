/* Copyright (C) 2015-2021 J.F.Dockes
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

#ifndef _SYNGROUPS_H_INCLUDED_
#define _SYNGROUPS_H_INCLUDED_

#include <string>
#include <vector>
#include <set>

// Manage synonym groups. This is very different from stemming and
// case/diac expansion because there is no reference form: all terms
// in a group are equivalent.
class SynGroups {
public:
    SynGroups();
    ~SynGroups();
    SynGroups(const SynGroups&) = delete;
    SynGroups& operator=(const SynGroups&) = delete;
    SynGroups(const SynGroups&&) = delete;
    SynGroups& operator=(const SynGroups&&) = delete;

    bool setfile(const std::string& fname);
    std::vector<std::string> getgroup(const std::string& term) const;
    const std::set<std::string>& getmultiwords() const;
    size_t getmultiwordsmaxlength() const;
    const std::string& getpath() const;
    bool ok() const;
private:
    class Internal;
    Internal *m;
};

#endif /* _SYNGROUPS_H_INCLUDED_ */
