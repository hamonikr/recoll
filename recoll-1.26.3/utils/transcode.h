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
#ifndef _TRANSCODE_H_INCLUDED_
#define _TRANSCODE_H_INCLUDED_
/** 
 * 
 */
#include <string>
/**
 * c++ized interface to iconv
 *
 * @param in input string
 * @param out output string
 * @param icode input encoding
 * @param ocode input encoding
 * @param ecnt (output) number of transcoding errors
 * @return true if transcoding succeeded, even with errors. False for global
 *     errors like unknown charset names
 */
extern bool transcode(const std::string &in, std::string &out, 
		      const std::string &icode,
		      const std::string &ocode, 
		      int *ecnt = 0);

#ifdef _WIN32
extern bool wchartoutf8(const wchar_t *in, std::string& out);
extern bool utf8towchar(const std::string& in, wchar_t *out, size_t obytescap);
#endif

#endif /* _TRANSCODE_H_INCLUDED_ */
