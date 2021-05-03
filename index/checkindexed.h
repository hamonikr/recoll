/* Copyright (C) 2021 J.F.Dockes
 *
 * License: GPL 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _CHECKINDEXED_H_INCLUDED_
#define _CHECKINDEXED_H_INCLUDED_

#include <vector>
#include <string>

class RclConfig;

// Diagnostic routine. Reads paths from stdin (one per line) if filepaths is empty.
// For each path, check that the file is indexed, print back its path
// with an ERROR or ABSENT prefix if it's not
extern bool checkindexed(RclConfig *conf, const std::vector<std::string>& filepaths);

#endif /* _CHECKINDEXED_H_INCLUDED_ */
