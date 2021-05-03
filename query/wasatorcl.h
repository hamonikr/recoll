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

#ifndef _WASATORCL_H_INCLUDED_
#define _WASATORCL_H_INCLUDED_

#include <string>
#include <memory>

namespace Rcl {
    class SearchData;
}
class RclConfig;

extern std::shared_ptr<Rcl::SearchData>wasaStringToRcl(
    const RclConfig *, const std::string& stemlang,
    const std::string& query, std::string &reason,
    const std::string& autosuffs = "");

#endif /* _WASATORCL_H_INCLUDED_ */
