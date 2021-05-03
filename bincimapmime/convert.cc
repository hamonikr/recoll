/* -*- mode:c++;c-basic-offset:2 -*- */
/*  --------------------------------------------------------------------
 *  Filename:
 *    convert.cc
 *  
 *  Description:
 *    Implementation of miscellaneous convertion functions.
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
#include "convert.h"
#include <string>

#ifndef NO_NAMESPACES
using namespace ::std;
using namespace Binc;
#endif /* NO_NAMESPACES */

//------------------------------------------------------------------------
BincStream::BincStream(void)
{
}

//------------------------------------------------------------------------
BincStream::~BincStream(void)
{
  clear();
}

//------------------------------------------------------------------------
string BincStream::popString(std::string::size_type size)
{
  if (size > nstr.length())
    size = nstr.length();
  string tmp = nstr.substr(0, size);
  nstr = nstr.substr(size);
  return tmp;
}

//------------------------------------------------------------------------
char BincStream::popChar(void)
{
  if (nstr.length() == 0)
    return '\0';

  char c = nstr[0];
  nstr = nstr.substr(1);
  return c;
}

//------------------------------------------------------------------------
void BincStream::unpopChar(char c)
{
  nstr = c + nstr;
}

//------------------------------------------------------------------------
void BincStream::unpopStr(const string &s)
{
  nstr = s + nstr;
}

//------------------------------------------------------------------------
const string &BincStream::str(void) const
{
  return nstr;
}

//------------------------------------------------------------------------
void BincStream::clear(void)
{
  nstr.clear();
}

//------------------------------------------------------------------------
unsigned int BincStream::getSize(void) const
{
  return (unsigned int) nstr.length();
}

//------------------------------------------------------------------------
BincStream &BincStream::operator << (std::ostream&(*)(std::ostream&))
{
  nstr += "\r\n";
  return *this;
}

//------------------------------------------------------------------------
BincStream &BincStream::operator << (const string &t)
{
  nstr += t;
  return *this;
}

//------------------------------------------------------------------------
BincStream &BincStream::operator << (int t)
{
  nstr += toString(t);
  return *this;
}

//------------------------------------------------------------------------
BincStream &BincStream::operator << (unsigned int t)
{
  nstr += toString(t);
  return *this;
}

//------------------------------------------------------------------------
BincStream &BincStream::operator << (char t)
{
  nstr += t;
  return *this;
}
