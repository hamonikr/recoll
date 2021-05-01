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
#ifndef _MIME_H_INCLUDED_
#define _MIME_H_INCLUDED_
/*
  Mime definitions RFC to 4-9-2006:

  2045 Multipurpose Internet Mail Extensions (MIME) Part One: Format of
  Internet Message Bodies. N. Freed, N. Borenstein. November 1996.
  (Format: TXT=72932 bytes) (Obsoletes RFC1521, RFC1522, RFC1590)
  (Updated by RFC2184, RFC2231) (Status: DRAFT STANDARD)

  2046 Multipurpose Internet Mail Extensions (MIME) Part Two: Media
  Types. N. Freed, N. Borenstein. November 1996. (Format: TXT=105854
  bytes) (Obsoletes RFC1521, RFC1522, RFC1590) (Updated by RFC2646,
  RFC3798) (Status: DRAFT STANDARD)

  2047 MIME (Multipurpose Internet Mail Extensions) Part Three: Message
  Header Extensions for Non-ASCII Text. K. Moore. November 1996.
  (Format: TXT=33262 bytes) (Obsoletes RFC1521, RFC1522, RFC1590)
  (Updated by RFC2184, RFC2231) (Status: DRAFT STANDARD)

  2183 Communicating Presentation Information in Internet Messages: The
  Content-Disposition Header Field. R. Troost, S. Dorner, K. Moore,
  Ed.. August 1997. (Format: TXT=23150 bytes) (Updates RFC1806)
  (Updated by RFC2184, RFC2231) (Status: PROPOSED STANDARD)

  2231 MIME Parameter Value and Encoded Word Extensions: Character Sets,
  Languages, and Continuations. N. Freed, K. Moore. November 1997.
  (Format: TXT=19280 bytes) (Obsoletes RFC2184) (Updates RFC2045,
  RFC2047, RFC2183) (Status: PROPOSED STANDARD)
*/


#include <time.h>

#include <string>
#include <map>

#include "base64.h"

/** A class to represent a MIME header value with parameters */
class MimeHeaderValue {
public:
    std::string value;
    std::map<std::string, std::string> params;
};

/** 
 * Parse MIME Content-type and Content-disposition value
 *
 * @param in the input string should be like: value; pn1=pv1; pn2=pv2. 
 *   Example: text/plain; charset="iso-8859-1" 
 */
extern bool parseMimeHeaderValue(const std::string& in, MimeHeaderValue& psd);

/** 
 * Quoted Printable decoding. 
 *
 * Doubles up as rfc2231 decoder, with the help of the hence the @param esc 
 * parameter.
 * RFC2045 Quoted Printable uses '=' , RFC2331 uses '%'. The two encodings are
 * otherwise similar.
 */
extern bool qp_decode(const std::string& in, std::string &out, char esc = '=');

/** Decode an Internet mail field value encoded according to rfc2047 
 *
 * Example input:  Some words =?iso-8859-1?Q?RE=A0=3A_Smoke_Tests?= more input
 * 
 * Note that MIME parameter values are explicitly NOT to be encoded with
 * this encoding which is only for headers like Subject:, To:. But it
 * is sometimes used anyway...
 * 
 * @param in input string, ascii with rfc2047 markup
 * @return out output string encoded in utf-8
 */
extern bool rfc2047_decode(const std::string& in, std::string &out);


/** Decode RFC2822 date to unix time (gmt secs from 1970)
 *
 * @param dt date string (the part after Date: )
 * @return unix time
 */
time_t rfc2822DateToUxTime(const std::string& dt);

#endif /* _MIME_H_INCLUDED_ */
