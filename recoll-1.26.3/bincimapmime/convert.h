/* -*- mode:c++;c-basic-offset:2 -*- */
/*  --------------------------------------------------------------------
 *  Filename:
 *    src/util/convert.h
 *  
 *  Description:
 *    Declaration of miscellaneous convertion functions.
 *  --------------------------------------------------------------------
 *  Copyright 2002-2005 Andreas Aardal Hanssen
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *  --------------------------------------------------------------------
 */
#ifndef convert_h_included
#define convert_h_included
#include <stddef.h>
#include <string>
#include <vector>
#include <iomanip>
#include <iostream>

#include <stdio.h>
#include <cstdlib>
#include <cstring>

namespace Binc {

  //----------------------------------------------------------------------
  inline std::string toString(int i_in)
  {
    char intbuf[16];
    snprintf(intbuf, sizeof(intbuf), "%d", i_in);
    return std::string(intbuf);
  }

  //----------------------------------------------------------------------
  inline std::string toString(unsigned int i_in)
  {
    char intbuf[16];
    snprintf(intbuf, sizeof(intbuf), "%u", i_in);
    return std::string(intbuf);
  }

  //----------------------------------------------------------------------
  inline std::string toString(unsigned long i_in)
  {
    char longbuf[40];
    snprintf(longbuf, sizeof(longbuf), "%lu", i_in);
    return std::string(longbuf);
  }

  //----------------------------------------------------------------------
  inline std::string toString(const char *i_in)
  {
    return std::string(i_in);
  }

  //----------------------------------------------------------------------
  inline int atoi(const std::string &s_in)
  {
    return ::atoi(s_in.c_str());
  }

  //----------------------------------------------------------------------
  inline std::string toHex(const std::string &s)
  {
    const char hexchars[] = "0123456789abcdef";
    std::string tmp;
    for (std::string::const_iterator i = s.begin(); i != s.end(); ++i) {
      unsigned char c = (unsigned char)*i;
      tmp += hexchars[((c & 0xf0) >> 4)];
      tmp += hexchars[c & 0x0f];
    }
    
    return tmp;
  }

  //----------------------------------------------------------------------
  inline std::string fromHex(const std::string &s)
  {
    const char hexchars[] = "0123456789abcdef";
    std::string tmp;
    for (std::string::const_iterator i = s.begin();
	 i != s.end() && i + 1 != s.end(); i += 2) {
      ptrdiff_t n;
      unsigned char c = *i;
      unsigned char d = *(i + 1);
      
      const char *t;
      if ((t = strchr(hexchars, c)) == 0)
	return "out of range";
      n = (t - hexchars) << 4;
      
      
      if ((t = strchr(hexchars, d)) == 0)
	return "out of range";
      n += (t - hexchars);
      
      if (n >= 0 && n <= 255)
	tmp += (char) n;
      else
	return "out of range";
    }
    
    return tmp;
  }
  
  //----------------------------------------------------------------------
  inline std::string toImapString(const std::string &s_in)
  {
    for (std::string::const_iterator i = s_in.begin(); i != s_in.end(); ++i) {
      unsigned char c = (unsigned char)*i;
      if (c <= 31 || c >= 127 || c == '\"' || c == '\\')
	return "{" + toString((unsigned long)s_in.length()) + "}\r\n" + s_in;
    }
    
    return "\"" + s_in + "\"";
  }

  //----------------------------------------------------------------------
  inline void uppercase(std::string &input)
  {
    for (std::string::iterator i = input.begin(); i != input.end(); ++i)
      *i = toupper(*i);
  }

  //----------------------------------------------------------------------
  inline void lowercase(std::string &input)
  {
    for (std::string::iterator i = input.begin(); i != input.end(); ++i)
      *i = tolower(*i);
  }

  //----------------------------------------------------------------------
  inline void chomp(std::string &s_in, const std::string &chars = " \t\r\n")
  {
    std::string::size_type n = s_in.length();
    while (n > 1 && chars.find(s_in[n - 1]) != std::string::npos)
      s_in.resize(n-- - 1);
  }

  //----------------------------------------------------------------------
  inline void trim(std::string &s_in, const std::string &chars = " \t\r\n")
  {
    while (s_in != "" && chars.find(s_in[0]) != std::string::npos)
      s_in = s_in.substr(1);
    chomp(s_in, chars);
  }

  //----------------------------------------------------------------------
  inline const std::string unfold(const std::string &a, 
				  bool removecomment = true)
  {
    std::string tmp;
    bool incomment = false;
    bool inquotes = false;
    for (std::string::const_iterator i = a.begin(); i != a.end(); ++i) {
      unsigned char c = (unsigned char)*i;
      if (!inquotes && removecomment) {
	if (c == '(') {
	  incomment = true; 
	  tmp += " ";
	} else if (c == ')') {
	  incomment = false; 
	} else if (c != 0x0a && c != 0x0d) {
	  tmp += *i;
        }
      } else if (c != 0x0a && c != 0x0d) {
	tmp += *i;
      }

      if (!incomment) {
        if (*i == '\"') 
          inquotes = !inquotes;
      }
    }

    trim(tmp);
    return tmp;
  }
  
  //----------------------------------------------------------------------
  inline void split(const std::string &s_in, const std::string &delim, 
	     std::vector<std::string> &dest, bool skipempty = true)
  {
    std::string token;
    for (std::string::const_iterator i = s_in.begin(); i != s_in.end(); ++i) {
      if (delim.find(*i) != std::string::npos) {
	if (!skipempty || token != "")
	  dest.push_back(token);
	token.clear();
      } else
	token += *i;
    }

    if (token != "")
      dest.push_back(token);
  }

  //----------------------------------------------------------------------
  inline void splitAddr(const std::string &s_in,
			std::vector<std::string> &dest, bool skipempty = true)
  {
    static const std::string delim = ",";
    std::string token;
    bool inquote = false;
    for (std::string::const_iterator i = s_in.begin(); i != s_in.end(); ++i) {
      if (inquote && *i == '\"') inquote = false;
      else if (!inquote && *i == '\"') inquote = true;

      if (!inquote && delim.find(*i) != std::string::npos) {
	if (!skipempty || token != "")
	  dest.push_back(token);
	token.clear();
      } else
	token += *i;
    }
    if (token != "")
      dest.push_back(token);
  }

  //----------------------------------------------------------------------
  inline std::string toCanonMailbox(const std::string &s_in)
  {
    if (s_in.find("..") != std::string::npos) return std::string();

    if (s_in.length() >= 5) {
      std::string a = s_in.substr(0, 5);
      uppercase(a);
      return a == "INBOX" ?
	a + (s_in.length() > 5 ? s_in.substr(5) : std::string()) : s_in;
    }
    
    return s_in;
  }

  //------------------------------------------------------------------------
  inline std::string toRegex(const std::string &s_in, char delimiter)
  {
    std::string regex = "^";
    for (std::string::const_iterator i = s_in.begin(); i != s_in.end(); ++i) {
      if (*i == '.' || *i == '[' || *i == ']' || *i == '{' || *i == '}' ||
	  *i == '(' || *i == ')' || *i == '^' || *i == '$' || *i == '?' ||
	  *i == '+' || *i == '\\') {
	regex += "\\";
	regex += *i;
      } else if (*i == '*')
	regex += ".*?";
       else if (*i == '%') {
        regex += "(\\";
        regex += delimiter;
        regex += "){0,1}";
	regex += "[^\\";
	regex += delimiter;
	regex += "]*?";
      } else regex += *i;
    }
    
    if (regex[regex.length() - 1] == '?')
      regex[regex.length() - 1] = '$';
    else
      regex += "$";

    return regex;
  }

  //------------------------------------------------------------------------
  class BincStream {
  private:
    std::string nstr;

  public:

    //--
    BincStream &operator << (std::ostream&(*)(std::ostream&));
    BincStream &operator << (const std::string &t);
    BincStream &operator << (unsigned int t);
    BincStream &operator << (int t);
    BincStream &operator << (char t);

    //--
    std::string popString(std::string::size_type size);

    //--
    char popChar(void);
    void unpopChar(char c);
    void unpopStr(const std::string &s);

    //--
    const std::string &str(void) const;

    //--
    unsigned int getSize(void) const;

    //--
    void clear(void);

    //--
    BincStream(void);
    ~BincStream(void);
  };
}

#endif
