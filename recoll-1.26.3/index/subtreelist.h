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

#ifndef _SUBTREELIST_H_INCLUDED_
#define _SUBTREELIST_H_INCLUDED_

#include <vector>
#include <string>

class RclConfig;

// This queries the database with a pure directory-filter query, to
// retrieve all the entries below the specified path. This is used by
// the real time indexer to purge entries when a top directory is
// renamed. This is really convoluted, I'd like a better way.
extern bool subtreelist(RclConfig *config, const string& top, 
			std::vector<std::string>& paths); 

#endif /* _SUBTREELIST_H_INCLUDED_ */
