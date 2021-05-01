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
#ifndef _UNACPP_H_INCLUDED_
#define _UNACPP_H_INCLUDED_

#include <string>

#ifndef NO_NAMESPACES
using std::string;
#endif /* NO_NAMESPACES */

// A small stringified wrapper for unac.c
enum UnacOp {UNACOP_UNAC = 1, UNACOP_FOLD = 2, UNACOP_UNACFOLD = 3};
extern bool unacmaybefold(const string& in, string& out, 
			  const char *encoding, UnacOp what);

// Utility function to determine if string begins with capital
extern bool unaciscapital(const string& in);
// Utility function to determine if string has upper-case anywhere
extern bool unachasuppercase(const string& in);
// Utility function to determine if any character is accented. This
// approprialey ignores the characters from unac_except_chars which
// are really separate letters
extern bool unachasaccents(const string& in);

#endif /* _UNACPP_H_INCLUDED_ */
