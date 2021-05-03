/* Copyright (C) 2017-2019 J.F.Dockes
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
#ifndef _DLIB_H_INCLUDED_
#define _DLIB_H_INCLUDED_

/** Dynamic library functions */

#include <string>

extern void *dlib_open(const std::string& libname, int flags = 0);
extern void *dlib_sym(void *handle, const char *name);
extern void dlib_close(void *handle);
extern const char *dlib_error();

#endif /* _DLIB_H_INCLUDED_ */
