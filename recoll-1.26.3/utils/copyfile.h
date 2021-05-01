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
#ifndef _COPYFILE_H_INCLUDED_
#define _COPYFILE_H_INCLUDED_

#include <string>

enum CopyfileFlags {COPYFILE_NONE = 0, 
                    COPYFILE_NOERRUNLINK = 1,
                    COPYFILE_EXCL = 2,
};

/** Copy src to dst. 
 *
 * We destroy an existing dst except if COPYFILE_EXCL is set (or we if
 * have no permission...).
 * A partially copied dst is normally removed, except if COPYFILE_NOERRUNLINK 
 * is set.
 */
extern bool copyfile(const char *src, const char *dst, std::string &reason,
		     int flags = 0);

/** Save c++ string to file */
extern bool stringtofile(const std::string& dt, const char *dst, 
                         std::string& reason, int flags = 0);

/** Try to rename src. If this fails (different devices) copy then unlink src */
extern bool renameormove(const char *src, const char *dst, std::string &reason);

#endif /* _COPYFILE_H_INCLUDED_ */
