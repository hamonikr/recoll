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
#ifndef _BASE64_H_INCLUDED_
#define _BASE64_H_INCLUDED_
#include <string>

void base64_encode(const std::string& in, std::string& out);
bool base64_decode(const std::string& in, std::string& out);
inline std::string base64_encode(const std::string& in)
{
    std::string o;
    base64_encode(in, o);
    return o;
}
inline std::string base64_decode(const std::string& in)
{
    std::string o;
    if (base64_decode(in, o))
	return o;
    return std::string();
}

#endif /* _BASE64_H_INCLUDED_ */
