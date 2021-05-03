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
#ifndef _FILEUDI_H_INCLUDED_
#define _FILEUDI_H_INCLUDED_
#include <string>
using std::string;

// Unique Document Ids for the file-based indexer (main Recoll
// indexer).  Document Ids are built from a concatenation of the file
// path and the internal path (ie: email number inside
// folder/attachment number/etc.)  As the size of Xapian terms is
// limited, the Id path is truncated to a maximum length, and completed
// by a hash of the remainder (including the ipath)

extern void make_udi(const string& fn, const string& ipath, string &udi);

#endif /* _FILEUDI_H_INCLUDED_ */
