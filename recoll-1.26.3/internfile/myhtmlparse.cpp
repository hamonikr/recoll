/* This file was copied from omega-0.8.5->1.2.6 and modified */

/* myhtmlparse.cc: subclass of HtmlParser for extracting text
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
#include <time.h>
#ifdef _WIN32
// Local implementation in windows directory
#include "strptime.h" 
#endif
#include <stdio.h>
#include <algorithm>
#include <cstring>

#include "cstr.h"
#include "myhtmlparse.h"
#include "indextext.h" // for lowercase_term()
#include "mimeparse.h"
#include "smallut.h"
#include "cancelcheck.h"
#include "log.h"
#include "transcode.h"

static const string cstr_html_charset("charset");
static const string cstr_html_content("content");

inline static bool
p_notdigit(char c)
{
    return !isdigit(static_cast<unsigned char>(c));
}

inline static bool
p_notxdigit(char c)
{
    return !isxdigit(static_cast<unsigned char>(c));
}

inline static bool
p_notalnum(char c)
{
    return !isalnum(static_cast<unsigned char>(c));
}

/*
 * The following array was taken from Estraier. Estraier was
 * written by Mikio Hirabayashi. 
 *                Copyright (C) 2003-2004 Mikio Hirabayashi
 * The version where this comes from 
 * is covered by the GNU licence, as this file.*/
static const char *epairs[] = {
    /* basic symbols */
    "amp", "&", "lt", "<", "gt", ">", "quot", "\"", "apos", "'",
    /* ISO-8859-1 */
    "nbsp", "\xc2\xa0", "iexcl", "\xc2\xa1", "cent", "\xc2\xa2",
    "pound", "\xc2\xa3", "curren", "\xc2\xa4", "yen", "\xc2\xa5",
    "brvbar", "\xc2\xa6", "sect", "\xc2\xa7", "uml", "\xc2\xa8",
    "copy", "\xc2\xa9", "ordf", "\xc2\xaa", "laquo", "\xc2\xab",
    "not", "\xc2\xac", "shy", "\xc2\xad", "reg", "\xc2\xae",
    "macr", "\xc2\xaf", "deg", "\xc2\xb0", "plusmn", "\xc2\xb1",
    "sup2", "\xc2\xb2", "sup3", "\xc2\xb3", "acute", "\xc2\xb4",
    "micro", "\xc2\xb5", "para", "\xc2\xb6", "middot", "\xc2\xb7",
    "cedil", "\xc2\xb8", "sup1", "\xc2\xb9", "ordm", "\xc2\xba",
    "raquo", "\xc2\xbb", "frac14", "\xc2\xbc", "frac12", "\xc2\xbd",
    "frac34", "\xc2\xbe", "iquest", "\xc2\xbf", "Agrave", "\xc3\x80",
    "Aacute", "\xc3\x81", "Acirc", "\xc3\x82", "Atilde", "\xc3\x83",
    "Auml", "\xc3\x84", "Aring", "\xc3\x85", "AElig", "\xc3\x86",
    "Ccedil", "\xc3\x87", "Egrave", "\xc3\x88", "Eacute", "\xc3\x89",
    "Ecirc", "\xc3\x8a", "Euml", "\xc3\x8b", "Igrave", "\xc3\x8c",
    "Iacute", "\xc3\x8d", "Icirc", "\xc3\x8e", "Iuml", "\xc3\x8f",
    "ETH", "\xc3\x90", "Ntilde", "\xc3\x91", "Ograve", "\xc3\x92",
    "Oacute", "\xc3\x93", "Ocirc", "\xc3\x94", "Otilde", "\xc3\x95",
    "Ouml", "\xc3\x96", "times", "\xc3\x97", "Oslash", "\xc3\x98",
    "Ugrave", "\xc3\x99", "Uacute", "\xc3\x9a", "Ucirc", "\xc3\x9b",
    "Uuml", "\xc3\x9c", "Yacute", "\xc3\x9d", "THORN", "\xc3\x9e",
    "szlig", "\xc3\x9f", "agrave", "\xc3\xa0", "aacute", "\xc3\xa1",
    "acirc", "\xc3\xa2", "atilde", "\xc3\xa3", "auml", "\xc3\xa4",
    "aring", "\xc3\xa5", "aelig", "\xc3\xa6", "ccedil", "\xc3\xa7",
    "egrave", "\xc3\xa8", "eacute", "\xc3\xa9", "ecirc", "\xc3\xaa",
    "euml", "\xc3\xab", "igrave", "\xc3\xac", "iacute", "\xc3\xad",
    "icirc", "\xc3\xae", "iuml", "\xc3\xaf", "eth", "\xc3\xb0",
    "ntilde", "\xc3\xb1", "ograve", "\xc3\xb2", "oacute", "\xc3\xb3",
    "ocirc", "\xc3\xb4", "otilde", "\xc3\xb5", "ouml", "\xc3\xb6",
    "divide", "\xc3\xb7", "oslash", "\xc3\xb8", "ugrave", "\xc3\xb9",
    "uacute", "\xc3\xba", "ucirc", "\xc3\xbb", "uuml", "\xc3\xbc",
    "yacute", "\xc3\xbd", "thorn", "\xc3\xbe", "yuml", "\xc3\xbf",
    /* ISO-10646 */
    "fnof", "\xc6\x92", "Alpha", "\xce\x91", "Beta", "\xce\x92",
    "Gamma", "\xce\x93", "Delta", "\xce\x94", "Epsilon", "\xce\x95",
    "Zeta", "\xce\x96", "Eta", "\xce\x97", "Theta", "\xce\x98",
    "Iota", "\xce\x99", "Kappa", "\xce\x9a", "Lambda", "\xce\x9b",
    "Mu", "\xce\x9c", "Nu", "\xce\x9d", "Xi", "\xce\x9e",
    "Omicron", "\xce\x9f", "Pi", "\xce\xa0", "Rho", "\xce\xa1",
    "Sigma", "\xce\xa3", "Tau", "\xce\xa4", "Upsilon", "\xce\xa5",
    "Phi", "\xce\xa6", "Chi", "\xce\xa7", "Psi", "\xce\xa8",
    "Omega", "\xce\xa9", "alpha", "\xce\xb1", "beta", "\xce\xb2",
    "gamma", "\xce\xb3", "delta", "\xce\xb4", "epsilon", "\xce\xb5",
    "zeta", "\xce\xb6", "eta", "\xce\xb7", "theta", "\xce\xb8",
    "iota", "\xce\xb9", "kappa", "\xce\xba", "lambda", "\xce\xbb",
    "mu", "\xce\xbc", "nu", "\xce\xbd", "xi", "\xce\xbe",
    "omicron", "\xce\xbf", "pi", "\xcf\x80", "rho", "\xcf\x81",
    "sigmaf", "\xcf\x82", "sigma", "\xcf\x83", "tau", "\xcf\x84",
    "upsilon", "\xcf\x85", "phi", "\xcf\x86", "chi", "\xcf\x87",
    "psi", "\xcf\x88", "omega", "\xcf\x89", "thetasym", "\xcf\x91",
    "upsih", "\xcf\x92", "piv", "\xcf\x96", "bull", "\xe2\x80\xa2",
    "hellip", "\xe2\x80\xa6", "prime", "\xe2\x80\xb2", "Prime", "\xe2\x80\xb3",
    "oline", "\xe2\x80\xbe", "frasl", "\xe2\x81\x84", "weierp", "\xe2\x84\x98",
    "image", "\xe2\x84\x91", "real", "\xe2\x84\x9c", "trade", "\xe2\x84\xa2",
    "alefsym", "\xe2\x84\xb5", "larr", "\xe2\x86\x90", "uarr", "\xe2\x86\x91",
    "rarr", "\xe2\x86\x92", "darr", "\xe2\x86\x93", "harr", "\xe2\x86\x94",
    "crarr", "\xe2\x86\xb5", "lArr", "\xe2\x87\x90", "uArr", "\xe2\x87\x91",
    "rArr", "\xe2\x87\x92", "dArr", "\xe2\x87\x93", "hArr", "\xe2\x87\x94",
    "forall", "\xe2\x88\x80", "part", "\xe2\x88\x82", "exist", "\xe2\x88\x83",
    "empty", "\xe2\x88\x85", "nabla", "\xe2\x88\x87", "isin", "\xe2\x88\x88",
    "notin", "\xe2\x88\x89", "ni", "\xe2\x88\x8b", "prod", "\xe2\x88\x8f",
    "sum", "\xe2\x88\x91", "minus", "\xe2\x88\x92", "lowast", "\xe2\x88\x97",
    "radic", "\xe2\x88\x9a", "prop", "\xe2\x88\x9d", "infin", "\xe2\x88\x9e",
    "ang", "\xe2\x88\xa0", "and", "\xe2\x88\xa7", "or", "\xe2\x88\xa8",
    "cap", "\xe2\x88\xa9", "cup", "\xe2\x88\xaa", "int", "\xe2\x88\xab",
    "there4", "\xe2\x88\xb4", "sim", "\xe2\x88\xbc", "cong", "\xe2\x89\x85",
    "asymp", "\xe2\x89\x88", "ne", "\xe2\x89\xa0", "equiv", "\xe2\x89\xa1",
    "le", "\xe2\x89\xa4", "ge", "\xe2\x89\xa5", "sub", "\xe2\x8a\x82",
    "sup", "\xe2\x8a\x83", "nsub", "\xe2\x8a\x84", "sube", "\xe2\x8a\x86",
    "supe", "\xe2\x8a\x87", "oplus", "\xe2\x8a\x95", "otimes", "\xe2\x8a\x97",
    "perp", "\xe2\x8a\xa5", "sdot", "\xe2\x8b\x85", "lceil", "\xe2\x8c\x88",
    "rceil", "\xe2\x8c\x89", "lfloor", "\xe2\x8c\x8a", "rfloor", "\xe2\x8c\x8b",
    "lang", "\xe2\x8c\xa9", "rang", "\xe2\x8c\xaa", "loz", "\xe2\x97\x8a",
    "spades", "\xe2\x99\xa0", "clubs", "\xe2\x99\xa3", "hearts", "\xe2\x99\xa5",
    "diams", "\xe2\x99\xa6", "OElig", "\xc5\x92", "oelig", "\xc5\x93",
    "Scaron", "\xc5\xa0", "scaron", "\xc5\xa1", "Yuml", "\xc5\xb8",
    "circ", "\xcb\x86", "tilde", "\xcb\x9c", "ensp", "\xe2\x80\x82",
    "emsp", "\xe2\x80\x83", "thinsp", "\xe2\x80\x89", "zwnj", "\xe2\x80\x8c",
    "zwj", "\xe2\x80\x8d", "lrm", "\xe2\x80\x8e", "rlm", "\xe2\x80\x8f",
    "ndash", "\xe2\x80\x93", "mdash", "\xe2\x80\x94", "lsquo", "\xe2\x80\x98",
    "rsquo", "\xe2\x80\x99", "sbquo", "\xe2\x80\x9a", "ldquo", "\xe2\x80\x9c",
    "rdquo", "\xe2\x80\x9d", "bdquo", "\xe2\x80\x9e", "dagger", "\xe2\x80\xa0",
    "Dagger", "\xe2\x80\xa1", "permil", "\xe2\x80\xb0", "lsaquo", "\xe2\x80\xb9",
    "rsaquo", "\xe2\x80\xba", "euro", "\xe2\x82\xac",
    NULL, NULL
};
map<string, string> my_named_ents;
class NamedEntsInitializer {
public:
    NamedEntsInitializer()
    {
	for (int i = 0;;) {
	    const char *ent;
	    const char *val;
	    ent = epairs[i++];
	    if (ent == 0) 
		break;
	    val = epairs[i++];
	    if (val == 0) 
		break;
	    my_named_ents[string(ent)] = val;
	}
    }
};
static NamedEntsInitializer namedEntsInitializerInstance;

MyHtmlParser::MyHtmlParser()
    : in_script_tag(false),
      in_style_tag(false),
      in_pre_tag(false),
      in_title_tag(false),
      pending_space(false),
      indexing_allowed(true)
{
    // The default html document charset is iso-8859-1. We'll update
    // this value from the encoding tag if found. Actually use cp1252 which
    // is a superset
    charset = "CP1252";
}

void MyHtmlParser::decode_entities(string &s)
{
    LOGDEB2("MyHtmlParser::decode_entities\n" );
    // This has no meaning whatsoever if the character encoding is unknown,
    // so don't do it. If charset known, caller has converted text to utf-8, 
    // and this is also how we translate entities
    //    if (tocharset != "utf-8")
    //    	return;

    // We need a const_iterator version of s.end() - otherwise the
    // find() and find_if() templates don't work...
    string::const_iterator amp = s.begin(), s_end = s.end();
    while ((amp = find(amp, s_end, '&')) != s_end) {
	unsigned int val = 0;
	string::const_iterator end, p = amp + 1;
	string subs;
	if (p != s_end && *p == '#') {
	    p++;
	    if (p != s_end && (*p == 'x' || *p == 'X')) {
		// hex
		p++;
		end = find_if(p, s_end, p_notxdigit);
		sscanf(s.substr(p - s.begin(), end - p).c_str(), "%x", &val);
	    } else {
		// number
		end = find_if(p, s_end, p_notdigit);
		val = atoi(s.substr(p - s.begin(), end - p).c_str());
	    }
	} else {
	    end = find_if(p, s_end, p_notalnum);
	    string code = s.substr(p - s.begin(), end - p);
	    map<string, string>::const_iterator i;
	    i = my_named_ents.find(code);
	    if (i != my_named_ents.end()) 
		subs = i->second;
	}

	if (end < s_end && *end == ';') 
	    end++;
	
	if (val) {
	    // The code is the code position for a unicode char. We need
	    // to translate it to an utf-8 string.
	    string utf16be;
	    utf16be += char(val / 256);
	    utf16be += char(val % 256);
	    transcode(utf16be, subs, "UTF-16BE", "UTF-8");
	} 

	if (subs.length() > 0) {
	    string::size_type amp_pos = amp - s.begin();
	    s.replace(amp_pos, end - amp, subs);
	    s_end = s.end();
	    // We've modified the string, so the iterators are no longer
	    // valid...
	    amp = s.begin() + amp_pos + subs.length();
	} else {
	    amp = end;
	}
    }
}

// Compress whitespace and suppress newlines
// Note that we independently add some newlines to the output text in the
// tag processing code. Like this, the preview looks a bit more like what a
// browser would display.
// We keep whitespace inside <pre> tags
void
MyHtmlParser::process_text(const string &text)
{
    LOGDEB2("process_text: title "  << (in_title_tag) << " script "  << (in_script_tag) << " style "  << (in_style_tag) << " pre "  << (in_pre_tag) << " pending_space "  << (pending_space) << " txt ["  << (text) << "]\n" );
    CancelCheck::instance().checkCancel();

    if (!in_script_tag && !in_style_tag) {
	if (in_title_tag) {
	    titledump += text;
	} else if (!in_pre_tag) {
	    string::size_type b = 0;
	    bool only_space = true;
	    while ((b = text.find_first_not_of(WHITESPACE, b)) != string::npos) {
		only_space = false;
		// If space specifically needed or chunk begins with
		// whitespace, add exactly one space
		if (pending_space || b != 0) {
			dump += ' ';
		}
		pending_space = true;
		string::size_type e = text.find_first_of(WHITESPACE, b);
		if (e == string::npos) {
		    dump += text.substr(b);
		    pending_space = false;
		    break;
		}
		dump += text.substr(b, e - b);
		b = e + 1;
	    }
	    if (only_space)
		pending_space = true;
	} else {
	    if (pending_space)
		dump += ' ';
	    dump += text;
	}
    }
}

bool
MyHtmlParser::opening_tag(const string &tag)
{
    LOGDEB2("opening_tag: ["  << (tag) << "]\n" );
#if 0
    cout << "TAG: " << tag << ": " << endl;
    map<string, string>::const_iterator x;
    for (x = p.begin(); x != p.end(); x++) {
	cout << "  " << x->first << " -> '" << x->second << "'" << endl;
    }
#endif
    if (tag.empty()) return true;
    switch (tag[0]) {
	case 'a':
	    if (tag == "address") pending_space = true;
	    break;
	case 'b':
	    // body: some bad docs have several opening body tags and
	    // even text before the body is displayed by Opera and
	    // Firefox.  We used to reset the dump each time we saw a
	    // body tag, but I can't see any reason to do so.

	    if (tag == "blockquote" || tag == "br") {
		dump += '\n';
		pending_space = true;
	    }
	    break;
	case 'c':
	    if (tag == "center") pending_space = true;
	    break;
	case 'd':
	    if (tag == "dd" || tag == "dir" || tag == "div" || tag == "dl" ||
		tag == "dt") pending_space = true;
	    if (tag == "dt")
		dump += '\n';
	    break;
	case 'e':
	    if (tag == "embed") pending_space = true;
	    break;
	case 'f':
	    if (tag == "fieldset" || tag == "form") pending_space = true;
	    break;
	case 'h':
	    // hr, and h1, ..., h6
	    if (tag.length() == 2 && strchr("r123456", tag[1])) {
		dump += '\n';
		pending_space = true;
	    }
	    break;
	case 'i':
	    if (tag == "iframe" || tag == "img" || tag == "isindex" ||
		tag == "input") pending_space = true;
	    break;
	case 'k':
	    if (tag == "keygen") pending_space = true;
	    break;
	case 'l':
	    if (tag == "legend" || tag == "li" || tag == "listing") {
		dump += '\n';
		pending_space = true;
	    }
	    break;
	case 'm':
	    if (tag == "meta") {
		string content;
		if (get_parameter(cstr_html_content, content)) {
		    string name;
		    if (get_parameter("name", name)) {
			lowercase_term(name);
			if (name == "date") {
			    // Specific to Recoll filters.
			    decode_entities(content);
			    struct tm tm;
                            memset(&tm, 0, sizeof(tm));
			    if (strptime(content.c_str(), 
					 " %Y-%m-%d %H:%M:%S ", &tm) ||
				strptime(content.c_str(), 
					 "%Y-%m-%dT%H:%M:%S", &tm)
				) {
				char ascuxtime[100];
				sprintf(ascuxtime, "%ld", (long)mktime(&tm));
				dmtime = ascuxtime;
			    }
			} else if (name == "robots") {
			} else {
			    string markup;
			    bool ishtml = false;
			    if (get_parameter("markup", markup)) {
				if (!stringlowercmp("html", markup)) {
				    ishtml = true;
				}
			    }
			    if (!meta[name].empty())
				meta[name] += ' ';
			    decode_entities(content);
			    meta[name] += content;
			    if (ishtml && 
				meta[name].compare(0, cstr_fldhtm.size(), 
						   cstr_fldhtm)) {
				meta[name].insert(0, cstr_fldhtm);
			    }
			}
		    } 
		    string hdr;
		    if (get_parameter("http-equiv", hdr)) {
			lowercase_term(hdr);
			if (hdr == "content-type") {
			    MimeHeaderValue p;
			    parseMimeHeaderValue(content, p);
			    map<string, string>::const_iterator k;
			    if ((k = p.params.find(cstr_html_charset)) != 
				p.params.end()) {
				charset = k->second;
				if (!charset.empty() && 
				    !samecharset(charset, fromcharset)) {
				    LOGDEB1("Doc http-equiv charset '"  << (charset) << "' differs from dir deflt '"  << (fromcharset) << "'\n" );
				    throw false;
				}
			    }
			}
		    }
		}
		string newcharset;
		if (get_parameter(cstr_html_charset, newcharset)) {
		    // HTML5 added: <meta charset="...">
		    lowercase_term(newcharset);
		    charset = newcharset;
		    if (!charset.empty() && 
			!samecharset(charset, fromcharset)) {
			LOGDEB1("Doc html5 charset '"  << (charset) << "' differs from dir deflt '"  << (fromcharset) << "'\n" );
			throw false;
		    }
		}
		break;
	    } else if (tag == "marquee" || tag == "menu" || tag == "multicol")
		pending_space = true;
	    break;
	case 'o':
	    if (tag == "ol" || tag == "option") pending_space = true;
	    break;
	case 'p':
	    if (tag == "p" || tag == "plaintext") {
		dump += '\n';
		pending_space = true;
	    } else if (tag == "pre") {
		in_pre_tag = true;
		dump += '\n';
		pending_space = true;
	    }
	    break;
	case 'q':
	    if (tag == "q") pending_space = true;
	    break;
	case 's':
	    if (tag == "style") {
		in_style_tag = true;
		break;
	    } else if (tag == "script") {
		in_script_tag = true;
		break;
	    } else if (tag == "select") 
		pending_space = true;
	    break;
	case 't':
	    if (tag == "table" || tag == "td" || tag == "textarea" ||
		tag == "th") {
		pending_space = true;
	    } else if (tag == "title") {
		in_title_tag = true;
	    }
	    break;
	case 'u':
	    if (tag == "ul") pending_space = true;
	    break;
	case 'x':
	    if (tag == "xmp") pending_space = true;
	    break;
    }
    return true;
}

bool
MyHtmlParser::closing_tag(const string &tag)
{
    LOGDEB2("closing_tag: ["  << (tag) << "]\n" );
    if (tag.empty()) return true;
    switch (tag[0]) {
	case 'a':
	    if (tag == "address") pending_space = true;
	    break;
	case 'b':
	    // body: We used to signal and end of doc here by returning
	    // false but the browsers just ignore body and html
	    // closing tags if there is further text, so it seems right
	    // to do the same

	    if (tag == "blockquote" || tag == "br") pending_space = true;
	    break;
	case 'c':
	    if (tag == "center") pending_space = true;
	    break;
	case 'd':
	    if (tag == "dd" || tag == "dir" || tag == "div" || tag == "dl" ||
		tag == "dt") pending_space = true;
	    break;
	case 'f':
	    if (tag == "fieldset" || tag == "form") pending_space = true;
	    break;
	case 'h':
	    // hr, and h1, ..., h6
	    if (tag.length() == 2 && strchr("r123456", tag[1]))
		pending_space = true;
	    break;
	case 'i':
	    if (tag == "iframe") pending_space = true;
	    break;
	case 'l':
	    if (tag == "legend" || tag == "li" || tag == "listing")
		pending_space = true;
	    break;
	case 'm':
	    if (tag == "marquee" || tag == "menu") pending_space = true;
	    break;
	case 'o':
	    if (tag == "ol" || tag == "option") pending_space = true;
	    break;
	case 'p':
	    if (tag == "p") {
		pending_space = true;
	    } else if  (tag == "pre") {
		pending_space = true;
		in_pre_tag = false;
	    }
	    break;
	case 'q':
	    if (tag == "q") pending_space = true;
	    break;
	case 's':
	    if (tag == "style") {
		in_style_tag = false;
		break;
	    }
	    if (tag == "script") {
		in_script_tag = false;
		break;
	    }
	    if (tag == "select") pending_space = true;
	    break;
	case 't':
	    if (tag == "title") {
		in_title_tag = false;
		if (meta.find("title") == meta.end()|| meta["title"].empty()) {
		    meta["title"] = titledump;
		    titledump.clear();
		}
		break;
	    }
	    if (tag == "table" || tag == "td" || tag == "textarea" ||
		tag == "th") pending_space = true;
	    break;
	case 'u':
	    if (tag == "ul") pending_space = true;
	    break;
	case 'x':
	    if (tag == "xmp") pending_space = true;
	    break;
    }
    return true;
}

// This gets called when hitting eof. 
// We used to do: 
//    > If the <body> is open, do
//    > something with the text (that is, don't throw up). Else, things are
//    > too weird, throw an error. We don't get called if the parser finds
//    > a closing body tag (exception gets thrown by closing_tag())
// But we don't throw any more. Whatever text we've extracted up to now is
// better than nothing.
void
MyHtmlParser::do_eof()
{
}

