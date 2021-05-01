/* Copyright (C) 2004 J.F.Dockes
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
#ifndef _MIMETYPE_H_INCLUDED_
#define _MIMETYPE_H_INCLUDED_

#include "safesysstat.h"
#include <string>

class RclConfig;

/**
 * Try to determine a mime type for file. 
 *
 * If stp is not null, this may imply more than matching the suffix,
 * the name must be usable to actually access file data.
 * @param filename file/path name to use
 * @param stp if not null use st_mode bits for directories etc.
 * @param cfg recoll config
 * @param usfc Use system's 'file' command as last resort (or not)
 */
std::string mimetype(const std::string &filename, const struct stat *stp,
                     RclConfig *cfg, bool usfc);


#endif /* _MIMETYPE_H_INCLUDED_ */
