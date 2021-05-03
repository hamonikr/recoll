#ifndef _closefrom_h_included_
#define _closefrom_h_included_
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

/* Close all descriptors >=  fd */
extern int libclf_closefrom(int fd);

/* Retrieve max open fd. This might be the actual max open one (not
   thread-safe) or a system max value. */
extern int libclf_maxfd(int flags=0);

#endif /* _closefrom_h_included_ */
