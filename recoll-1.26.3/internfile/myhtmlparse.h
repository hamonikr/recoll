/* This file was copied from omega-0.8.5 and modified */

/* myhtmlparse.h: subclass of HtmlParser for extracting text
 *
 * ----START-LICENCE----
 * Copyright 1999,2000,2001 BrightStation PLC
 * Copyright 2002,2003,2004 Olly Betts
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 * -----END-LICENCE-----
 */
#include <map>
using std::map;

#include "htmlparse.h"

// FIXME: Should we include \xa0 which is non-breaking space in iso-8859-1, but
// not in all charsets and perhaps spans of all \xa0 should become a single
// \xa0?
#define WHITESPACE " \t\n\r"

class MyHtmlParser : public HtmlParser {
 public:
    bool in_script_tag;
    bool in_style_tag;
    bool in_pre_tag;
    bool in_title_tag;
    bool pending_space;
    map<string,string> meta;
    string dump, dmtime, titledump;
    // This is the charset our caller thinks the doc used (initially
    // comes from the environment/configuration, used as source for
    // conversion to utf-8)
    string fromcharset; 
    // This is the charset it was supposedly converted to (always
    // utf-8 in fact, except if conversion utterly failed)
    string tocharset; 
    // charset is declared by HtmlParser. It is the charset from the
    // document: default, then from html or xml header.
    // string charset; 

    bool indexing_allowed;

    void process_text(const string &text);
    bool opening_tag(const string &tag);
    bool closing_tag(const string &tag);
    void do_eof();
    void decode_entities(string &s);
    void reset_charsets() {fromcharset = tocharset = "";}
    void set_charsets(const string& f, const string& t) 
    {
	fromcharset = f;
	tocharset = t;
    }
    // Return charset as determined from html
    const string& get_charset() {return charset;}

    MyHtmlParser();
};
