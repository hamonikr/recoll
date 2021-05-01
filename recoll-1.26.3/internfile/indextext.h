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
#ifndef _INDEXTEXT_H_INCLUDED_
#define _INDEXTEXT_H_INCLUDED_
/* Note: this only exists to help with using myhtmlparse.cc */

#include <string>

// lets hope that the charset includes ascii values...
static inline void
lowercase_term(std::string &term)
{
    std::string::iterator i = term.begin();
    while (i != term.end()) {
	if (*i >= 'A' && *i <= 'Z')
	    *i = *i + 'a' - 'A';
        i++;
    }
}

#endif /* _INDEXTEXT_H_INCLUDED_ */
