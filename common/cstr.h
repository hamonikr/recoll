/* Copyright (C) 2011-2018 J.F.Dockes
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

#ifndef _CSTR_H_INCLUDED_
#define _CSTR_H_INCLUDED_

// recoll mostly uses STL strings. In many places we had automatic
// conversion from a C string to an STL one. This costs, and can
// become significant if used often.
//
// This file and the associated .cpp file declares/defines constant
// strings used in the program. Strings are candidates for a move here
// when they are used in a fast loop or are shared.

#include <string>

// The following slightly hacky preprocessing directives and the
// companion code in the cpp file looks complicated, but it just
// ensures that we only have to write the strings once to get the
// extern declaration and the definition.
#ifdef RCLIN_CSTR_CPPFILE
#undef DEF_CSTR
#define DEF_CSTR(NM, STR) const std::string cstr_##NM(STR)
#else
#define DEF_CSTR(NM, STR) extern const std::string cstr_##NM
#endif

DEF_CSTR(caption, "caption");
DEF_CSTR(colon, ":");
DEF_CSTR(dmtime, "dmtime");
DEF_CSTR(dquote, "\"");
DEF_CSTR(fbytes, "fbytes");
DEF_CSTR(fileu, "file://");
DEF_CSTR(fmtime, "fmtime");
DEF_CSTR(iso_8859_1, "ISO-8859-1");
DEF_CSTR(utf8, "UTF-8");
DEF_CSTR(cp1252, "CP1252");
DEF_CSTR(minwilds, "*?[");
DEF_CSTR(newline, "\n");
DEF_CSTR(null, "");
DEF_CSTR(plus, "+");
DEF_CSTR(textplain, "text/plain");
DEF_CSTR(texthtml, "text/html");
DEF_CSTR(url, "url");
// Marker for HTML format fields
DEF_CSTR(fldhtm, "\007");
// Characters that can -begin- a wildcard or regexp expression. 
DEF_CSTR(wildSpecStChars, "*?[");
DEF_CSTR(regSpecStChars, "(.[{");

// Values used as keys inside Dijon::Filter::metaData[].

// The document data.
DEF_CSTR(dj_keycontent, "content");

// These fields go from the topmost handler (text/plain) into the
// Rcl::Doc::meta, possibly with some massaging. 
DEF_CSTR(dj_keyanc, "rclanc");
DEF_CSTR(dj_keyorigcharset, "origcharset");
DEF_CSTR(dj_keyds, "description");
DEF_CSTR(dj_keyabstract, "abstract");

// Built or inherited along the handler stack, then copied to doc
DEF_CSTR(dj_keyipath, "ipath");
DEF_CSTR(dj_keyfn, "filename");
DEF_CSTR(dj_keyauthor, "author");
DEF_CSTR(dj_keymd, "modificationdate");
// charset and mimetype are explicitly blocked from going into the doc meta
DEF_CSTR(dj_keycharset, "charset");
DEF_CSTR(dj_keymt, "mimetype");

// All other meta fields are directly copied from
// Dijon::Filter::metaData to Rcl::Doc::meta. The defininitions which
// follow are just for well-known names, with no particular processing
// in internfile.
DEF_CSTR(dj_keytitle, "title");
DEF_CSTR(dj_keyrecipient, "recipient");
DEF_CSTR(dj_keymsgid, "msgid");
DEF_CSTR(dj_keymd5, "md5");


#endif /* _CSTR_H_INCLUDED_ */
