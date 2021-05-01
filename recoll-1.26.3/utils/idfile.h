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
#ifndef _IDFILE_H_INCLUDED_
#define _IDFILE_H_INCLUDED_

#include <string>

// Look at data inside file or string, and return mime type or empty string. 
//
// The system's file utility does a bad job on mail folders. idFile
// only looks for mail file types for now, but this may change

extern std::string idFile(const char *fn);
extern std::string idFileMem(const std::string& data);

#endif /* _IDFILE_H_INCLUDED_ */
