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

#ifndef TEST_MIMEPARSE
#include "autoconfig.h"

#include <string>
#include <vector>

#include <ctype.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <cstdlib>
#include <cstring>

#include "mimeparse.h"
#include "base64.h"
#include "transcode.h"
#include "smallut.h"

using namespace std;

//#define DEBUG_MIMEPARSE 
#ifdef DEBUG_MIMEPARSE
#define DPRINT(X) fprintf X
#else
#define DPRINT(X)
#endif

// Parsing a header value. Only content-type and content-disposition
// have parameters, but others are compatible with content-type
// syntax, only, parameters are not used. So we can parse all like:
//
//    headertype: value [; paramname=paramvalue] ...
//
// Value and paramvalues can be quoted strings, and there can be
// comments too. Note that RFC2047 is explicitly forbidden for
// parameter values (RFC2231 must be used), but I have seen it used
// anyway (ie: thunderbird 1.0)
//
// Ref: RFC2045/6/7 (MIME) RFC2183/2231 (content-disposition and encodings)



/** Decode a MIME parameter value encoded according to rfc2231
 *
 * Example input withs input charset == "":  
 *     [iso-8859-1'french'RE%A0%3A_Smoke_Tests%20bla]
 * Or (if charset is set) : RE%A0%3A_Smoke_Tests%20bla
 *
 * @param in input string, ascii with rfc2231 markup
 * @param out output string
 * @param charset if empty: decode string like 'charset'lang'more%20stuff,
 *      else just do the %XX part
 * @return out output string encoded in utf-8
 */
bool rfc2231_decode(const string &in, string &out, string &charset)
{
    string::size_type pos1, pos2=0;

    if (charset.empty()) {
        if ((pos1 = in.find("'")) == string::npos)
            return false;
        charset = in.substr(0, pos1);
        // fprintf(stderr, "Charset: [%s]\n", charset.c_str());
        pos1++;

        if ((pos2 = in.find("'", pos1)) == string::npos)
            return false;
        // We have no use for lang for now
        // string lang = in.substr(pos1, pos2-pos1); 
        // fprintf(stderr, "Lang: [%s]\n", lang.c_str());
        pos2++;
    }

    string raw;
    qp_decode(in.substr(pos2), raw, '%');
    // fprintf(stderr, "raw [%s]\n", raw.c_str());
    if (!transcode(raw, out, charset, "UTF-8"))
        return false;
    return true;
}


/////////////////////////////////////////
/// Decoding of MIME fields values and parameters

// The lexical token returned by find_next_token
class Lexical {
public:
    enum kind {none, token, separator};
    kind   what;
    string value;
    string error;
    char quote;
    Lexical() : what(none), quote(0) {}
    void reset() {what = none; value.erase(); error.erase();quote = 0;}
};

// Skip mime comment. This must be called with in[start] == '('
static string::size_type 
skip_comment(const string &in, string::size_type start, Lexical &lex)
{
    int commentlevel = 0;
    for (; start < in.size(); start++) {
        if (in[start] == '\\') {
            // Skip escaped char. 
            if (start+1 < in.size()) {
                start++;
                continue;
            } else {
                lex.error.append("\\ at end of string ");
                return in.size();
            }
        }
        if (in[start] == '(')
            commentlevel++;
        if (in[start] == ')') {
            if (--commentlevel == 0)
                break;
        }
    }
    if (start == in.size() && commentlevel != 0) {
        lex.error.append("Unclosed comment ");
        return in.size();
    }
    return start;
}

// Skip initial whitespace and (possibly nested) comments. 
static string::size_type 
skip_whitespace_and_comment(const string &in, string::size_type start, 
                            Lexical &lex)
{
    while (1) {
        if ((start = in.find_first_not_of(" \t\r\n", start)) == string::npos)
            return in.size();
        if (in[start] == '(') {
            if ((start = skip_comment(in, start, lex)) == string::npos)
                return string::npos;
        } else {
            break;
        }
    }
    return start;
}

/// Find next token in mime header value string. 
/// @return the next starting position in string, string::npos for error 
/// @param in the input string
/// @param start the starting position
/// @param lex  the returned token and its description
/// @param delims separators we should look for
static string::size_type 
find_next_token(const string &in, string::size_type start, 
                Lexical &lex, string delims = ";=")
{
    char oquot, cquot;

    start = skip_whitespace_and_comment(in, start, lex);
    if (start == string::npos || start == in.size())
        return in.size();

    // Begins with separator ? return it.
    string::size_type delimi = delims.find_first_of(in[start]);
    if (delimi != string::npos) {
        lex.what = Lexical::separator;
        lex.value = delims[delimi];
        return start+1;
    }

    // Check for start of quoted string
    oquot = in[start];
    switch (oquot) {
    case '<': cquot = '>';break;
    case '"': cquot = '"';break;
    default: cquot = 0; break;
    }

    if (cquot != 0) {
        // Quoted string parsing
        string::size_type end;
        start++; // Skip quote character
        for (end = start;end < in.size() && in[end] != cquot; end++) {
            if (in[end] == '\\') {
                // Skip escaped char. 
                if (end+1 < in.size()) {
                    end++;
                } else {
                    // backslash at end of string: error
                    lex.error.append("\\ at end of string ");
                    return string::npos;
                }
            }
        }
        if (end == in.size()) {
            // Found end of string before closing quote character: error
            lex.error.append("Unclosed quoted string ");
            return string::npos;
        }
        lex.what = Lexical::token;
        lex.value = in.substr(start, end-start);
        lex.quote = oquot;
        return ++end;
    } else {
        string::size_type end = in.find_first_of(delims + "\r\n \t(", start);
        lex.what = Lexical::token;
        lex.quote = 0;
        if (end == string::npos) {
            end = in.size();
            lex.value = in.substr(start);
        } else {
            lex.value = in.substr(start, end-start);
        }
        return end;
    }
}

// Classes for handling rfc2231 value continuations
class Chunk {
public:
    Chunk() : decode(false) {}
    bool decode;
    string value;
};
class Chunks {
public:
    vector<Chunk> chunks;
};

void stringtolower(string &out, const string& in)
{
    for (string::size_type i = 0; i < in.size(); i++)
        out.append(1, char(tolower(in[i])));
}

// Parse MIME field value. Should look like:
//  somevalue ; param1=val1;param2=val2
bool parseMimeHeaderValue(const string& value, MimeHeaderValue& parsed)
{
    parsed.value.erase();
    parsed.params.clear();

    Lexical lex;
    string::size_type start = 0;

    // Get the field value
    start = find_next_token(value, start, lex);
    if (start == string::npos || lex.what != Lexical::token) 
        return false;
    parsed.value = lex.value;

    map<string, string> rawparams;
    // Look for parameters
    for (;;) {
        string paramname, paramvalue;
        lex.reset();
        start = find_next_token(value, start, lex);
        if (start == value.size())
            break;
        if (start == string::npos) {
            //fprintf(stderr, "Find_next_token error(1)\n");
            return false;
        }
        if (lex.what == Lexical::separator && lex.value[0] == ';')
            continue;
        if (lex.what != Lexical::token) 
            return false;
        stringtolower(paramname, lex.value);

        start = find_next_token(value, start, lex);
        if (start == string::npos || lex.what != Lexical::separator || 
            lex.value[0] != '=') {
            //fprintf(stderr, "Find_next_token error (2)\n");
            return false;
        }

        start = find_next_token(value, start, lex);
        if (start == string::npos || lex.what != Lexical::token) {
            //fprintf(stderr, "Parameter has no value!");
            return false;
        }
        paramvalue = lex.value;
        rawparams[paramname] = paramvalue;
        //fprintf(stderr, "RAW: name [%s], value [%s]\n", paramname.c_str(),
        //              paramvalue.c_str());
    }
    //    fprintf(stderr, "Number of raw params %d\n", rawparams.size());

    // RFC2231 handling: 
    // - if a parameter name ends in * it must be decoded 
    // - If a parameter name looks line name*ii[*] it is a
    //   partial value, and must be concatenated with other such.
    
    map<string, Chunks> chunks;
    for (map<string, string>::const_iterator it = rawparams.begin(); 
         it != rawparams.end(); it++) {
        string nm = it->first;
        //      fprintf(stderr, "NM: [%s]\n", nm.c_str());
        if (nm.empty()) // ??
            continue;

        Chunk chunk;
        if (nm[nm.length()-1] == '*') {
            nm.erase(nm.length() - 1);
            chunk.decode = true;
        } else
            chunk.decode = false;
        //      fprintf(stderr, "NM1: [%s]\n", nm.c_str());

        chunk.value = it->second;

        // Look for another asterisk in nm. If none, assign index 0
        string::size_type aster;
        int idx = 0;
        if ((aster = nm.rfind("*")) != string::npos) {
            string num = nm.substr(aster+1);
            //fprintf(stderr, "NUM: [%s]\n", num.c_str());
            nm.erase(aster);
            idx = atoi(num.c_str());
        }
        Chunks empty;
        if (chunks.find(nm) == chunks.end())
            chunks[nm] = empty;
        chunks[nm].chunks.resize(idx+1);
        chunks[nm].chunks[idx] = chunk;
        //fprintf(stderr, "CHNKS: nm [%s], idx %d, decode %d, value [%s]\n", 
        // nm.c_str(), idx, int(chunk.decode), chunk.value.c_str());
    }

    // For each parameter name, concatenate its chunks and possibly
    // decode Note that we pass the whole concatenated string to
    // decoding if the first chunk indicates that decoding is needed,
    // which is not right because there might be uncoded chunks
    // according to the rfc.
    for (map<string, Chunks>::const_iterator it = chunks.begin(); 
         it != chunks.end(); it++) {
        if (it->second.chunks.empty())
            continue;
        string nm = it->first;
        // Create the name entry
        if (parsed.params.find(nm) == parsed.params.end())
            parsed.params[nm].clear();
        // Concatenate all chunks and decode the whole if the first one needs
        // to. Yes, this is not quite right.
        string value;
        for (vector<Chunk>::const_iterator vi = it->second.chunks.begin();
             vi != it->second.chunks.end(); vi++) {
            value += vi->value;
        }
        if (it->second.chunks[0].decode) {
            string charset;
            rfc2231_decode(value, parsed.params[nm], charset);
        } else {
            // rfc2047 MUST NOT but IS used by some agents
            rfc2047_decode(value, parsed.params[nm]);
        }
        //fprintf(stderr, "FINAL: nm [%s], value [%s]\n", 
        //nm.c_str(), parsed.params[nm].c_str());
    }
    
    return true;
}

// Decode a string encoded with quoted-printable encoding. 
// we reuse the code for rfc2231 % encoding, even if the eol
// processing is not useful in this case
bool qp_decode(const string& in, string &out, char esc) 
{
    out.reserve(in.length());
    string::size_type ii;
    for (ii = 0; ii < in.length(); ii++) {
        if (in[ii] == esc) {
            ii++; // Skip '=' or '%'
            if(ii >= in.length() - 1) { // Need at least 2 more chars
                break;
            } else if (in[ii] == '\r' && in[ii+1] == '\n') { // Soft nl, skip
                ii++;
            } else if (in[ii] != '\n' && in[ii] != '\r') { // decode
                char c = in[ii];
                char co;
                if(c >= 'A' && c <= 'F') {
                    co = char((c - 'A' + 10) * 16);
                } else if (c >= 'a' && c <= 'f') {
                    co = char((c - 'a' + 10) * 16);
                } else if (c >= '0' && c <= '9') {
                    co = char((c - '0') * 16);
                } else {
                    return false;
                }
                if(++ii >= in.length()) 
                    break;
                c = in[ii];
                if (c >= 'A' && c <= 'F') {
                    co += char(c - 'A' + 10);
                } else if (c >= 'a' && c <= 'f') {
                    co += char(c - 'a' + 10);
                } else if (c >= '0' && c <= '9') {
                    co += char(c - '0');
                } else {
                    return false;
                }
                out += co;
            }
        } else {
            out += in[ii];
        }
    }
    return true;
}

// Decode an word encoded as quoted printable or base 64
static bool rfc2047_decodeParsed(const std::string& charset, 
                                 const std::string& encoding, 
                                 const std::string& value, 
                                 std::string &utf8)
{
    DPRINT((stderr, "DecodeParsed: charset [%s] enc [%s] val [%s]\n",
            charset.c_str(), encoding.c_str(), value.c_str()));
    utf8.clear();

    string decoded;
    if (!stringlowercmp("b", encoding)) {
        if (!base64_decode(value, decoded))
            return false;
        DPRINT((stderr, "FromB64: [%s]\n", decoded.c_str()));
    } else if (!stringlowercmp("q", encoding)) {
        if (!qp_decode(value, decoded))
            return false;
        // Need to translate _ to ' ' here
        string temp;
        for (string::size_type pos = 0; pos < decoded.length(); pos++)
            if (decoded[pos] == '_')
                temp += ' ';
            else 
                temp += decoded[pos];
        decoded = temp;
        DPRINT((stderr, "FromQP: [%s]\n", decoded.c_str()));
    } else {
        DPRINT((stderr, "Bad encoding [%s]\n", encoding.c_str()));
        return false;
    }

    if (!transcode(decoded, utf8, charset, "UTF-8")) {
        DPRINT((stderr, "Transcode failed\n"));
        return false;
    }
    return true;
}

// Parse a mail header value encoded according to RFC2047. 
// This is not supposed to be used for MIME parameter values, but it
// happens.
// Bugs: 
//    - We should turn off decoding while inside quoted strings
//
typedef enum  {rfc2047ready, rfc2047open_eq, 
               rfc2047charset, rfc2047encoding, 
               rfc2047value, rfc2047close_q} Rfc2047States;

bool rfc2047_decode(const std::string& in, std::string &out) 
{
    DPRINT((stderr, "rfc2047_decode: [%s]\n", in.c_str()));

    Rfc2047States state = rfc2047ready;
    string encoding, charset, value, utf8;

    out.clear();

    for (string::size_type ii = 0; ii < in.length(); ii++) {
        char ch = in[ii];
        switch (state) {
        case rfc2047ready: 
        {
            DPRINT((stderr, "STATE: ready, ch %c\n", ch));
            switch (ch) {
                // Whitespace: stay ready
            case ' ': case '\t': value += ch;break;
                // '=' -> forward to next state
            case '=': state = rfc2047open_eq; break;
                DPRINT((stderr, "STATE: open_eq\n"));
                // Other: go back to sleep
            default: value += ch; state = rfc2047ready;
            }
        }
        break;
        case rfc2047open_eq: 
        {
            DPRINT((stderr, "STATE: open_eq, ch %c\n", ch));
            switch (ch) {
            case '?': 
            {
                // Transcode current (unencoded part) value:
                // we sometimes find 8-bit chars in
                // there. Interpret as Iso8859.
                if (value.length() > 0) {
                    transcode(value, utf8, "ISO-8859-1", "UTF-8");
                    out += utf8;
                    value.clear();
                }
                state = rfc2047charset; 
            }
            break;
            default: state = rfc2047ready; out += '='; out += ch;break;
            }
        } 
        break;
        case rfc2047charset: 
        {
            DPRINT((stderr, "STATE: charset, ch %c\n", ch));
            switch (ch) {
            case '?': state = rfc2047encoding; break;
            default: charset += ch; break;
            }
        } 
        break;
        case rfc2047encoding: 
        {
            DPRINT((stderr, "STATE: encoding, ch %c\n", ch));
            switch (ch) {
            case '?': state = rfc2047value; break;
            default: encoding += ch; break;
            }
        }
        break;
        case rfc2047value: 
        {
            DPRINT((stderr, "STATE: value, ch %c\n", ch));
            switch (ch) {
            case '?': state = rfc2047close_q; break;
            default: value += ch;break;
            }
        }
        break;
        case rfc2047close_q: 
        {
            DPRINT((stderr, "STATE: close_q, ch %c\n", ch));
            switch (ch) {
            case '=': 
            {
                DPRINT((stderr, "End of encoded area. Charset %s, Encoding %s\n", charset.c_str(), encoding.c_str()));
                string utf8;
                state = rfc2047ready; 
                if (!rfc2047_decodeParsed(charset, encoding, value, 
                                          utf8)) {
                    return false;
                }
                out += utf8;
                charset.clear();
                encoding.clear();
                value.clear();
            }
            break;
            default: state = rfc2047value; value += '?';value += ch;break;
            }
        }
        break;
        default: // ??
            DPRINT((stderr, "STATE: default ?? ch %c\n", ch));
            return false;
        }
    }

    if (value.length() > 0) {
        transcode(value, utf8, "CP1252", "UTF-8");
        out += utf8;
        value.clear();
    }
    if (state != rfc2047ready) 
        return false;
    return true;
}

#define DEBUGDATE 0
#if DEBUGDATE
#define DATEDEB(X) fprintf X
#else
#define DATEDEB(X)
#endif

// Convert rfc822 date to unix time. A date string normally looks like:
//  Mon, 3 Jul 2006 09:51:58 +0200
// But there are many close common variations
// And also hopeless things like: Fri Nov  3 13:13:33 2006
time_t rfc2822DateToUxTime(const string& dt)
{
    // Strip everything up to first comma if any, we don't need weekday,
    // then break into tokens
    vector<string> toks;
    string::size_type idx;
    if ((idx = dt.find_first_of(",")) != string::npos) {
        if (idx == dt.length() - 1) {
            DATEDEB((stderr, "Bad rfc822 date format (short1): [%s]\n", 
                     dt.c_str()));
            return (time_t)-1;
        }
        string date = dt.substr(idx+1);
        stringToTokens(date, toks, " \t:");
    } else {
        // No comma. Enter strangeland
        stringToTokens(dt, toks, " \t:");
        // Test for date like: Sun Nov 19 06:18:41 2006
        //                      0   1  2   3 4  5  6
        // and change to:      19 Nov 2006 06:18:41
        if (toks.size() == 7) {
            if (toks[0].length() == 3 &&
                toks[0].find_first_of("0123456789") == string::npos) {
                swap(toks[0], toks[2]);
                swap(toks[6], toks[2]);
                toks.pop_back();
            }
        }
    }

#if DEBUGDATE
    for (list<string>::iterator it = toks.begin(); it != toks.end(); it++) {
        DATEDEB((stderr, "[%s] ", it->c_str()));
    }
    DATEDEB((stderr, "\n"));
#endif

    if (toks.size() < 6) {
        DATEDEB((stderr, "Bad rfc822 date format (toks cnt): [%s]\n", 
                 dt.c_str()));
        return (time_t)-1;
    }

    if (toks.size() == 6) {
        // Probably no timezone, sometimes happens
        toks.push_back("+0000");
    }

    struct tm tm;
    memset(&tm, 0, sizeof(tm));

    // Load struct tm with appropriate tokens, possibly converting
    // when needed

    vector<string>::iterator it = toks.begin();

    // Day of month: no conversion needed
    tm.tm_mday = atoi(it->c_str());
    it++;

    // Month. Only Jan-Dec are legal. January, February do happen
    // though. Convert to 0-11
    if (*it == "Jan" || *it == "January") tm.tm_mon = 0; else if
        (*it == "Feb" || *it == "February") tm.tm_mon = 1; else if
        (*it == "Mar" || *it == "March") tm.tm_mon = 2; else if
        (*it == "Apr" || *it == "April") tm.tm_mon = 3; else if
        (*it == "May") tm.tm_mon = 4; else if
        (*it == "Jun" || *it == "June") tm.tm_mon = 5; else if
        (*it == "Jul" || *it == "July") tm.tm_mon = 6; else if
        (*it == "Aug" || *it == "August") tm.tm_mon = 7; else if
        (*it == "Sep" || *it == "September") tm.tm_mon = 8; else if
        (*it == "Oct" || *it == "October") tm.tm_mon = 9; else if
        (*it == "Nov" || *it == "November") tm.tm_mon = 10; else if
        (*it == "Dec" || *it == "December") tm.tm_mon = 11; else {
        DATEDEB((stderr, "Bad rfc822 date format (month): [%s]\n", 
                 dt.c_str()));
        return (time_t)-1;
    }
    it++;

    // Year. Struct tm counts from 1900. 2 char years are quite rare
    // but do happen. I've seen 00 happen so count small values from 2000
    tm.tm_year = atoi(it->c_str());
    if (it->length() == 2) {
        if (tm.tm_year < 10)
            tm.tm_year += 2000;
        else
            tm.tm_year += 1900;
    }
    if (tm.tm_year > 1900)
        tm.tm_year -= 1900;
    it++;

    // Hour minute second need no adjustments
    tm.tm_hour = atoi(it->c_str()); it++;
    tm.tm_min  = atoi(it->c_str()); it++;
    tm.tm_sec  = atoi(it->c_str()); it++;       


    // Timezone is supposed to be either +-XYZT or a zone name
    int zonesecs = 0;
    if (it->length() < 1) {
        DATEDEB((stderr, "Bad rfc822 date format (zlen): [%s]\n", dt.c_str()));
        return (time_t)-1;
    }
    if (it->at(0) == '-' || it->at(0) == '+') {
        // Note that +xy:zt (instead of +xyzt) sometimes happen, we
        // may want to process it one day
        if (it->length() < 5) {
            DATEDEB((stderr, "Bad rfc822 date format (zlen1): [%s]\n", 
                     dt.c_str()));
            goto nozone;
        }
        zonesecs = 3600*((it->at(1)-'0') * 10 + it->at(2)-'0')+ 
            (it->at(3)-'0')*10 + it->at(4)-'0';
        zonesecs = it->at(0) == '+' ? -1 * zonesecs : zonesecs;
    } else {
        int hours;
        if (*it == "A") hours= 1; else if (*it == "B") hours= 2; 
        else if (*it == "C") hours= 3; else if (*it == "D") hours= 4; 
        else if (*it == "E") hours= 5; else if (*it == "F") hours= 6;
        else if (*it == "G") hours= 7; else if (*it == "H") hours= 8; 
        else if (*it == "I") hours= 9; else if (*it == "K") hours= 10;
        else if (*it == "L") hours= 11; else if (*it == "M") hours= 12; 
        else if (*it == "N") hours= -1; else if (*it == "O") hours= -2; 
        else if (*it == "P") hours= -3; else if (*it == "Q") hours= -4; 
        else if (*it == "R") hours= -5; else if (*it == "S") hours= -6; 
        else if (*it == "T") hours= -7; else if (*it == "U") hours= -8; 
        else if (*it == "V") hours= -9; else if (*it == "W") hours= -10;
        else if (*it == "X") hours= -11; else if (*it == "Y") hours= -12;
        else if (*it == "Z") hours=  0; else if  (*it == "UT") hours= 0; 
        else if (*it == "GMT") hours= 0; else if (*it == "EST") hours= 5;
        else if (*it == "EDT") hours= 4; else if (*it == "CST") hours= 6;
        else if (*it == "CDT") hours= 5; else if (*it == "MST") hours= 7;
        else if (*it == "MDT") hours= 6; else if (*it == "PST") hours= 8;
        else if (*it == "PDT") hours= 7; 
        // Non standard names
        // Standard Time (or Irish Summer Time?) is actually +5.5
        else if (*it == "CET") hours= -1; else if (*it == "JST") hours= -9; 
        else if (*it == "IST") hours= -5; else if (*it == "WET") hours= 0; 
        else if (*it == "MET") hours= -1; 
        else {
            DATEDEB((stderr, "Bad rfc822 date format (zname): [%s]\n", 
                     dt.c_str()));
            // Forget tz
            goto nozone;
        }
        zonesecs = 3600 * hours;
    }
    DATEDEB((stderr, "Tz: [%s] -> %d\n", it->c_str(), zonesecs));
nozone:

    // Compute the UTC Unix time value
#ifndef sun
    time_t tim = timegm(&tm);
#else
    // No timegm on Sun. Use mktime, then correct for local timezone
    time_t tim = mktime(&tm);
    // altzone and timezone hold the difference in seconds between UTC
    // and local. They are negative for places east of greenwich
    // 
    // mktime takes our buffer to be local time, so it adds timezone
    // to the conversion result (if timezone is < 0 it's currently
    // earlier in greenwhich). 
    //
    // We have to substract it back (hey! hopefully! maybe we have to
    // add it). Who can really know?
    tim -= timezone;
#endif

    // And add in the correction from the email's Tz
    tim += zonesecs;

    DATEDEB((stderr, "Date: %s  uxtime %ld \n", ctime(&tim), tim));
    return tim;
}

#else 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <string>
#include "mimeparse.h"
#include "readfile.h"


using namespace std;
extern bool rfc2231_decode(const string& in, string& out, string& charset); 
extern time_t rfc2822DateToUxTime(const string& date);
static const char *thisprog;

static char usage [] =
    "-p: header value and parameter test\n"
    "-q: qp decoding\n"
    "-b: base64\n"
    "-7: rfc2047\n"
    "-1: rfc2331\n"
    "-t: date time\n"
    "  \n\n"
    ;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_p     0x2 
#define OPT_q     0x4 
#define OPT_b     0x8
#define OPT_7     0x10
#define OPT_1     0x20
#define OPT_t     0x40
int
main(int argc, const char **argv)
{
    int count = 10;
    
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            /* Cas du "adb - core" */
            Usage();
        while (**argv)
            switch (*(*argv)++) {
            case 'p':   op_flags |= OPT_p; break;
            case 'q':   op_flags |= OPT_q; break;
            case 'b':   op_flags |= OPT_b; break;
            case '1':   op_flags |= OPT_1; break;
            case '7':   op_flags |= OPT_7; break;
            case 't':   op_flags |= OPT_t; break;
            default: Usage();   break;
            }
    b1: argc--; argv++;
    }

    if (argc != 0)
        Usage();

    if (op_flags & OPT_p) {
        // Mime header value and parameters extraction
        const char *tr[] = {
            "text/html;charset = UTF-8 ; otherparam=garb; \n"
            "QUOTEDPARAM=\"quoted value\"",

            "text/plain; charset=ASCII\r\n name=\"809D3016_5691DPS_5.2.LIC\"",

            "application/x-stuff;"
            "title*0*=us-ascii'en'This%20is%20even%20more%20;"
            "title*1*=%2A%2A%2Afun%2A%2A%2A%20;"
            "title*2=\"isn't it!\"",

            // The following are all invalid, trying to crash the parser...
            "",
            // This does not parse because of whitespace in the value.
            " complete garbage;",
            // This parses, but only the first word gets into the value
            " some value",
            " word ;",  ";",  "=",  "; = ",  "a;=\"toto tutu\"=", ";;;;a=b",
        };
      
        for (unsigned int i = 0; i < sizeof(tr) / sizeof(char *); i++) {
            MimeHeaderValue parsed;
            if (!parseMimeHeaderValue(tr[i], parsed)) {
                fprintf(stderr, "PARSE ERROR for [%s]\n", tr[i]);
                continue;
            }
            printf("Field value: [%s]\n", parsed.value.c_str());
            map<string, string>::iterator it;
            for (it = parsed.params.begin();it != parsed.params.end();it++) {
                if (it == parsed.params.begin())
                    printf("Parameters:\n");
                printf("  [%s] = [%s]\n", it->first.c_str(), it->second.c_str());
            }
        }

    } else if (op_flags & OPT_q) {
        // Quoted printable stuff
        const char *qp = 
            "=41=68 =e0 boire=\r\n continue 1ere\ndeuxieme\n\r3eme "
            "agrave is: '=E0' probable skipped decode error: =\n"
            "Actual decode error =xx this wont show";

        string out;
        if (!qp_decode(string(qp), out)) {
            fprintf(stderr, "qp_decode returned error\n");
        }
        printf("Decoded: '%s'\n", out.c_str());
    } else if (op_flags & OPT_b) {
        // Base64
        //'C'est à boire qu'il nous faut éviter l'excès.'
        //'Deuxième ligne'
        //'Troisième ligne'
        //'Et la fin (pas de nl). '
        const char *b64 = 
            "Qydlc3Qg4CBib2lyZSBxdSdpbCBub3VzIGZhdXQg6XZpdGVyIGwnZXhj6HMuCkRldXhp6G1l\r\n"
            "IGxpZ25lClRyb2lzaehtZSBsaWduZQpFdCBsYSBmaW4gKHBhcyBkZSBubCkuIA==\r\n";

        string out;
        if (!base64_decode(string(b64), out)) {
            fprintf(stderr, "base64_decode returned error\n");
            exit(1);
        }
        printf("Decoded: [%s]\n", out.c_str());
#if 0
        string coded, decoded;
        const char *fname = "/tmp/recoll_decodefail";
        if (!file_to_string(fname, coded)) {
            fprintf(stderr, "Cant read %s\n", fname);
            exit(1);
        }
    
        if (!base64_decode(coded, decoded)) {
            fprintf(stderr, "base64_decode returned error\n");
            exit(1);
        }
        printf("Decoded: [%s]\n", decoded.c_str());
#endif

    } else if (op_flags & (OPT_7|OPT_1)) {
        // rfc2047
        char line [1024];
        string out;
        bool res;
        while (fgets(line, 1023, stdin)) {
            int l = strlen(line);
            if (l == 0)
                continue;
            line[l-1] = 0;
            fprintf(stderr, "Line: [%s]\n", line);
            string charset;
            if (op_flags & OPT_7) {
                res = rfc2047_decode(line, out);
            } else {
                res = rfc2231_decode(line, out, charset);
            }
            if (res)
                fprintf(stderr, "Out:  [%s] cs %s\n", out.c_str(), charset.c_str());
            else
                fprintf(stderr, "Decoding failed\n");
        }
    } else if (op_flags & OPT_t) {
        time_t t;
        
        const char *dates[] = {
            " Wed, 13 Sep 2006 11:40:26 -0700 (PDT)",
            " Mon, 3 Jul 2006 09:51:58 +0200",
            " Wed, 13 Sep 2006 08:19:48 GMT-07:00",
            " Wed, 13 Sep 2006 11:40:26 -0700 (PDT)",
            " Sat, 23 Dec 89 19:27:12 EST",
            "   13 Jan 90 08:23:29 GMT"};

        for (unsigned int i = 0; i <sizeof(dates) / sizeof(char *); i++) {
            t = rfc2822DateToUxTime(dates[i]);
            struct tm *tm = localtime(&t);
            char datebuf[100];
            strftime(datebuf, 99, "&nbsp;%Y-%m-%d&nbsp;%H:%M:%S %z", tm);
            printf("[%s] -> [%s]\n", dates[i], datebuf);
        }
        printf("Enter date:\n");
        char line [1024];
        while (fgets(line, 1023, stdin)) {
            int l = strlen(line);
            if (l == 0) continue;
            line[l-1] = 0;
            t = rfc2822DateToUxTime(line);
            struct tm *tm = localtime(&t);
            char datebuf[100];
            strftime(datebuf, 99, "&nbsp;%Y-%m-%d&nbsp;%H:%M:%S %z", tm);
            printf("[%s] -> [%s]\n", line, datebuf);
        }


    }
    exit(0);
}

#endif // TEST_MIMEPARSE
