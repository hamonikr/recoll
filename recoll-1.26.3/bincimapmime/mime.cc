/* -*- mode:c++;c-basic-offset:2 -*- */
/*  --------------------------------------------------------------------
 *  Filename:
 *    mime.cc
 *  
 *  Description:
 *    Implementation of main mime parser components
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
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

#include <string>
#include <vector>
#include <map>
#include <exception>
#include <iostream>
#ifndef NO_NAMESPACES
using namespace ::std;
#endif /* NO_NAMESPACES */


#include "mime.h"
#include "convert.h"
#include "mime-inputsource.h"

//------------------------------------------------------------------------
Binc::MimeDocument::MimeDocument(void)
{
  allIsParsed = false;
  headerIsParsed = false;
  doc_mimeSource = 0;
}

//------------------------------------------------------------------------
Binc::MimeDocument::~MimeDocument(void)
{
  delete doc_mimeSource;
  doc_mimeSource = 0;
}

//------------------------------------------------------------------------
void Binc::MimeDocument::clear(void)
{
  members.clear();
  h.clear();
  headerIsParsed = false;
  allIsParsed = false;
  delete doc_mimeSource;
  doc_mimeSource = 0;
}

//------------------------------------------------------------------------
void Binc::MimePart::clear(void)
{
  members.clear();
  h.clear();
  mimeSource = 0;
}

//------------------------------------------------------------------------
Binc::MimePart::MimePart(void)
{
  size = 0;
  messagerfc822 = false;
  multipart = false;

  nlines = 0;
  nbodylines = 0;
  mimeSource = 0;
}

//------------------------------------------------------------------------
Binc::MimePart::~MimePart(void)
{
}

//------------------------------------------------------------------------
Binc::HeaderItem::HeaderItem(void)
{
}

//------------------------------------------------------------------------
Binc::HeaderItem::HeaderItem(const string &key, const string &value)
{
  this->key = key;
  this->value = value;
}

//------------------------------------------------------------------------
Binc::Header::Header(void)
{
}

//------------------------------------------------------------------------
Binc::Header::~Header(void)
{
}

//------------------------------------------------------------------------
bool Binc::Header::getFirstHeader(const string &key, HeaderItem &dest) const
{
  string k = key;
  lowercase(k);

  for (vector<HeaderItem>::const_iterator i = content.begin();
       i != content.end(); ++i) {
    string tmp = (*i).getKey();
    lowercase(tmp);

    if (tmp == k) {
      dest = *i;
      return true;
    }
  }
  return false;
}

//------------------------------------------------------------------------
bool Binc::Header::getAllHeaders(const string &key, vector<HeaderItem> &dest) const
{
  string k = key;
  lowercase(k);

  for (vector<HeaderItem>::const_iterator i = content.begin();
       i != content.end(); ++i) {
    string tmp = (*i).getKey();
    lowercase(tmp);
    if (tmp == k)
      dest.push_back(*i);
  }

  return (dest.size() != 0);
}

//------------------------------------------------------------------------
void Binc::Header::clear(void)
{
  content.clear();
}

//------------------------------------------------------------------------
void Binc::Header::add(const string &key, const string &value)
{
  content.push_back(HeaderItem(key, value));
}
