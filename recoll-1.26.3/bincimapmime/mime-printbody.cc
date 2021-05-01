/* -*- mode:c++;c-basic-offset:2 -*- */
/*  --------------------------------------------------------------------
 *  Filename:
 *    mime-printbody.cc
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

#include "mime.h"
#include "mime-utils.h"
#include "mime-inputsource.h"

#include <string>

using namespace ::std;

void Binc::MimePart::getBody(string &s,
			     unsigned int startoffset,
			     unsigned int length) const
{
  mimeSource->reset();
  mimeSource->seek(bodystartoffsetcrlf + startoffset);
  s.reserve(length);
  if (startoffset + length > bodylength)
    length = bodylength - startoffset;

  char c = '\0';
  for (unsigned int i = 0; i < length; ++i) {
    if (!mimeSource->getChar(&c))
      break;

    s += (char)c;
  }
}
