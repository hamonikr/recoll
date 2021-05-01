 /* -*- mode:c++;c-basic-offset:2 -*- */
/*  --------------------------------------------------------------------
 *  Filename:
 *    mime-parsefull.cc
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
#include "mime-utils.h"
#include "mime-inputsource.h"
#include "convert.h"

// #define MPF
#ifdef MPF
#define MPFDEB(X) fprintf X
#else
#define MPFDEB(X)
#endif

//------------------------------------------------------------------------
void Binc::MimeDocument::parseFull(int fd)
{
  if (allIsParsed)
    return;

  allIsParsed = true;

  delete doc_mimeSource;
  doc_mimeSource = new MimeInputSource(fd);

  headerstartoffsetcrlf = 0;
  headerlength = 0;
  bodystartoffsetcrlf = 0;
  bodylength = 0;
  size = 0;
  messagerfc822 = false;
  multipart = false;

  int bsize = 0;
  string bound;
  doParseFull(doc_mimeSource, bound, bsize);

  // eat any trailing junk to get the correct size
  char c;
  while (doc_mimeSource->getChar(&c));

  size = doc_mimeSource->getOffset();
}

void Binc::MimeDocument::parseFull(istream& s)
{
  if (allIsParsed)
    return;

  allIsParsed = true;

  delete doc_mimeSource;
  doc_mimeSource = new MimeInputSourceStream(s);

  headerstartoffsetcrlf = 0;
  headerlength = 0;
  bodystartoffsetcrlf = 0;
  bodylength = 0;
  size = 0;
  messagerfc822 = false;
  multipart = false;

  int bsize = 0;
  string bound;
  doParseFull(doc_mimeSource, bound, bsize);

  // eat any trailing junk to get the correct size
  char c;
  while (doc_mimeSource->getChar(&c));

  size = doc_mimeSource->getOffset();
}

//------------------------------------------------------------------------
bool Binc::MimePart::parseOneHeaderLine(Binc::Header *header, 
					unsigned int *nlines)
{
  using namespace ::Binc;
  char c;
  bool eof = false;
  char cqueue[4];
  string name;
  string content;

  while (mimeSource->getChar(&c)) {
    // If we encounter a \r before we got to the first ':', then
    // rewind back to the start of the line and assume we're at the
    // start of the body.
    if (c == '\r') {
      for (int i = 0; i < (int) name.length() + 1; ++i)
	mimeSource->ungetChar();
      return false;
    }

    // A colon marks the end of the header name
    if (c == ':') break;

    // Otherwise add to the header name
    name += c;
  }

  cqueue[0] = '\0';
  cqueue[1] = '\0';
  cqueue[2] = '\0';
  cqueue[3] = '\0';

  // Read until the end of the header.
  bool endOfHeaders = false;
  while (!endOfHeaders) {
    if (!mimeSource->getChar(&c)) {
      eof = true;
      break;
    }

    if (c == '\n') ++*nlines;

    for (int i = 0; i < 3; ++i)
      cqueue[i] = cqueue[i + 1];
    cqueue[3] = c;

    if (strncmp(cqueue, "\r\n\r\n", 4) == 0) {
      endOfHeaders = true;
      break;
    }

    // If the last character was a newline, and the first now is not
    // whitespace, then rewind one character and store the current
    // key,value pair.
    if (cqueue[2] == '\n' && c != ' ' && c != '\t') {
      if (content.length() > 2)
	content.resize(content.length() - 2);

      trim(content);
      header->add(name, content);

      if (c != '\r') {
	mimeSource->ungetChar();
	if (c == '\n') --*nlines;
	return true;
      }
	
      mimeSource->getChar(&c);
      return false;
    }

    content += c;
  }

  if (name != "") {
    if (content.length() > 2)
      content.resize(content.length() - 2);
    header->add(name, content);
  }

  return !(eof || endOfHeaders);
}

//------------------------------------------------------------------------
void Binc::MimePart::parseHeader(Binc::Header *header, unsigned int *nlines)
{
  while (parseOneHeaderLine(header, nlines))
  { }
}

//------------------------------------------------------------------------
void Binc::MimePart::analyzeHeader(Binc::Header *header, bool *multipart,
				   bool *messagerfc822, string *subtype,
				   string *boundary)
{
  using namespace ::Binc;

  // Do simple parsing of headers to determine the
  // type of message (multipart,messagerfc822 etc)
  HeaderItem ctype;
  if (header->getFirstHeader("content-type", ctype)) {
    vector<string> types;
    split(ctype.getValue(), ";", types);

    if (types.size() > 0) {
      // first element should describe content type
      string tmp = types[0];
      trim(tmp);
      vector<string> v;
      split(tmp, "/", v);
      string key, value;

      key = (v.size() > 0) ? v[0] : "text";
      value = (v.size() > 1) ? v[1] : "plain";
      lowercase(key);

      if (key == "multipart") {
	*multipart = true;
	lowercase(value);
	*subtype = value;
      } else if (key == "message") {
	lowercase(value);
	if (value == "rfc822")
	  *messagerfc822 = true;
      }
    }

    for (vector<string>::const_iterator i = types.begin();
	 i != types.end(); ++i) {
      string element = *i;
      trim(element);

      if (element.find("=") != string::npos) {
	string::size_type pos = element.find('=');
	string key = element.substr(0, pos);
	string value = element.substr(pos + 1);
	
	lowercase(key);
	trim(key);

	if (key == "boundary") {
	  trim(value, " \"");
	  *boundary = value;
	}
      }
    }
  }
}

void Binc::MimePart::parseMessageRFC822(vector<Binc::MimePart> *members,
					bool *foundendofpart,
					unsigned int *bodylength,
					unsigned int *nbodylines,
					const string &toboundary)
{
  using namespace ::Binc;

  // message rfc822 means a completely enclosed mime document. we
  // call the parser recursively, and pass on the boundary string
  // that we got. when parse() finds this boundary, it returns 0. if
  // it finds the end boundary (boundary + "--"), it returns != 0.
  MimePart m;

  unsigned int bodystartoffsetcrlf = mimeSource->getOffset();
    
  // parsefull returns the number of bytes that need to be removed
  // from the body because of the terminating boundary string.
  int bsize = 0;
  if (m.doParseFull(mimeSource, toboundary, bsize))
    *foundendofpart = true;

  // make sure bodylength doesn't overflow    
  *bodylength = mimeSource->getOffset();
  if (*bodylength >= bodystartoffsetcrlf) {
    *bodylength -= bodystartoffsetcrlf;
    if (*bodylength >= (unsigned int) bsize) {
      *bodylength -= (unsigned int) bsize;
    } else {
      *bodylength = 0;
    }
  } else {
    *bodylength = 0;
  }

  *nbodylines += m.getNofLines();

  members->push_back(m);
}

bool Binc::MimePart::skipUntilBoundary(const string &delimiter,
				       unsigned int *nlines, bool *eof)
{
  string::size_type endpos = delimiter.length();
  char *delimiterqueue = 0;
  string::size_type delimiterpos = 0;
  const char *delimiterStr = delimiter.c_str();
  if (delimiter != "") {
    delimiterqueue = new char[endpos];
    memset(delimiterqueue, 0, endpos);
  }

  // first, skip to the first delimiter string. Anything between the
  // header and the first delimiter string is simply ignored (it's
  // usually a text message intended for non-mime clients)
  char c;

  bool foundBoundary = false;
  for (;;) {    
    if (!mimeSource->getChar(&c)) {
      *eof = true;
      break;
    }

    if (c == '\n')
      ++*nlines;

    // if there is no delimiter, we just read until the end of the
    // file.
    if (!delimiterqueue)
      continue;

    delimiterqueue[delimiterpos++] = c;
    if (delimiterpos ==  endpos)
      delimiterpos = 0;
      
    if (compareStringToQueue(delimiterStr, delimiterqueue,
			     delimiterpos, int(endpos))) {
      foundBoundary = true;
      break;
    }
  }

  delete [] delimiterqueue;
  delimiterqueue = 0;

  return foundBoundary;
}

// JFD: Things we do after finding a boundary (something like CRLF--somestring)
// Need to see if this is a final one (with an additional -- at the end),
// and need to check if it is immediately followed by another boundary 
// (in this case, we give up our final CRLF in its favour)
inline void Binc::MimePart::postBoundaryProcessing(bool *eof,
						   unsigned int *nlines,
						   int *boundarysize,
						   bool *foundendofpart)
{
    // Read two more characters. This may be CRLF, it may be "--" and
    // it may be any other two characters.
    char a = '\0';
    if (!mimeSource->getChar(&a))
      *eof = true;
    if (a == '\n')
      ++*nlines; 

    char b = '\0';
    if (!mimeSource->getChar(&b))
      *eof = true;
    if (b == '\n')
      ++*nlines;
    
    // If eof, we're done here
    if (*eof)
      return;

    // If we find two dashes after the boundary, then this is the end
    // of boundary marker, and we need to get 2 more chars
    if (a == '-' && b == '-') {
      *foundendofpart = true;
      *boundarysize += 2;
	
      if (!mimeSource->getChar(&a))
	*eof = true;
      if (a == '\n')
	++*nlines; 
	
      if (!mimeSource->getChar(&b))
	*eof = true;
      if (b == '\n')
	++*nlines;
    }

    // If the boundary is followed by CRLF, we need to handle the
    // special case where another boundary line follows
    // immediately. In this case we consider the CRLF to be part of
    // the NEXT boundary.
    if (a == '\r' && b == '\n') {
      // Get 2 more
      if (!mimeSource->getChar(&a) || !mimeSource->getChar(&b)) {
	*eof = true; 
      } else if (a == '-' && b == '-') {
	MPFDEB((stderr, "BINC: consecutive delimiters, giving up CRLF\n"));
	mimeSource->ungetChar();
	mimeSource->ungetChar();
	mimeSource->ungetChar();
	mimeSource->ungetChar();
      } else {
	// We unget the 2 chars, and keep our crlf (increasing our own size)
	MPFDEB((stderr, "BINC: keeping my CRLF\n"));
	mimeSource->ungetChar();
	mimeSource->ungetChar();
	*boundarysize += 2;
      }

    } else {
      // Boundary string not followed by CRLF, don't read more and let
      // others skip the rest. Note that this is allowed but quite uncommon
      mimeSource->ungetChar();
      mimeSource->ungetChar();
    }
}

void Binc::MimePart::parseMultipart(const string &boundary,
				    const string &toboundary,
				    bool *eof,
				    unsigned int *nlines,
				    int *boundarysize,
				    bool *foundendofpart,
				    unsigned int *bodylength,
				    vector<Binc::MimePart> *members)
{
  MPFDEB((stderr, "BINC: ParseMultipart: boundary [%s], toboundary[%s]\n", 
	  boundary.c_str(),
	  toboundary.c_str()));
  using namespace ::Binc;
  unsigned int bodystartoffsetcrlf = mimeSource->getOffset();

  // multipart parsing starts with skipping to the first
  // boundary. then we call parse() for all parts. the last parse()
  // command will return a code indicating that it found the last
  // boundary of this multipart. Note that the first boundary does
  // not have to start with CRLF.
  string delimiter = "--" + boundary;

  skipUntilBoundary(delimiter, nlines, eof);

  if (!eof)
    *boundarysize = int(delimiter.size());

  postBoundaryProcessing(eof, nlines, boundarysize, foundendofpart);

  // read all mime parts.
  if (!*foundendofpart && !*eof) {
    bool quit = false;
    do {
      MimePart m;

      // If parseFull returns != 0, then it encountered the multipart's
      // final boundary.
      int bsize = 0;
      if (m.doParseFull(mimeSource, boundary, bsize)) {
	quit = true;
	*boundarysize = bsize;
      }

      members->push_back(m);

    } while (!quit);
  }

  if (!*foundendofpart && !*eof) {
    // multipart parsing starts with skipping to the first
    // boundary. then we call parse() for all parts. the last parse()
    // command will return a code indicating that it found the last
    // boundary of this multipart. Note that the first boundary does
    // not have to start with CRLF.
    string delimiter = "\r\n--" + toboundary;
    skipUntilBoundary(delimiter, nlines, eof);

    if (!*eof)
      *boundarysize = int(delimiter.size());

    postBoundaryProcessing(eof, nlines, boundarysize, foundendofpart);
  }

  // make sure bodylength doesn't overflow    
  *bodylength = mimeSource->getOffset();
  if (*bodylength >= bodystartoffsetcrlf) {
    *bodylength -= bodystartoffsetcrlf;
    if (*bodylength >= (unsigned int) *boundarysize) {
      *bodylength -= (unsigned int) *boundarysize;
    } else {
      *bodylength = 0;
    }
  } else {
    *bodylength = 0;
  }
  MPFDEB((stderr, "BINC: ParseMultipart return\n"));
}

void Binc::MimePart::parseSinglePart(const string &toboundary,
			    int *boundarysize,
			    unsigned int *nbodylines,
			    unsigned int *nlines,
			    bool *eof, bool *foundendofpart,
			    unsigned int *bodylength)
{
  MPFDEB((stderr, "BINC: parseSinglePart, boundary [%s]\n", 
	  toboundary.c_str()));
  using namespace ::Binc;
  unsigned int bodystartoffsetcrlf = mimeSource->getOffset();

  // If toboundary is empty, then we read until the end of the
  // file. Otherwise we will read until we encounter toboundary.
  string _toboundary; 
  if (toboundary != "") {
    _toboundary = "\r\n--";
    _toboundary += toboundary;
  }

  //  if (skipUntilBoundary(_toboundary, nlines, eof))
  //    *boundarysize = _toboundary.length();

  char *boundaryqueue = 0;
  size_t endpos = _toboundary.length();
  if (toboundary != "") {
    boundaryqueue = new char[endpos];
    memset(boundaryqueue, 0, endpos);
  }

  *boundarysize = 0;

  const char *_toboundaryStr = _toboundary.c_str();
  string line;
  bool toboundaryIsEmpty = (toboundary == "");
  char c;
  string::size_type boundarypos = 0;
  while (mimeSource->getChar(&c)) {
    if (c == '\n') { ++*nbodylines; ++*nlines; }

    if (toboundaryIsEmpty)
      continue;

    // find boundary
    boundaryqueue[boundarypos++] = c;
    if (boundarypos == endpos)
      boundarypos = 0;
      
    if (compareStringToQueue(_toboundaryStr, boundaryqueue,
			     boundarypos, int(endpos))) {
      *boundarysize = static_cast<int>(_toboundary.length());
      break;
    }
  }

  delete [] boundaryqueue;

  if (toboundary != "") {
    postBoundaryProcessing(eof, nlines, boundarysize, foundendofpart);
  } else {
    // Recoll: in the case of a multipart body with a null
    // boundary (probably illegal but wtf), eof was not set and
    // multipart went into a loop until bad alloc.
    *eof = true;
  }

  // make sure bodylength doesn't overflow    
  *bodylength = mimeSource->getOffset();
  if (*bodylength >= bodystartoffsetcrlf) {
    *bodylength -= bodystartoffsetcrlf;
    if (*bodylength >= (unsigned int) *boundarysize) {
      *bodylength -= (unsigned int) *boundarysize;
    } else {
      *bodylength = 0;
    }
  } else {
    *bodylength = 0;
  }
  MPFDEB((stderr, "BINC: parseSimple ret: bodylength %d, boundarysize %d\n",
	  *bodylength, *boundarysize));
}

//------------------------------------------------------------------------
int Binc::MimePart::doParseFull(MimeInputSource *ms, const string &toboundary,
				int &boundarysize)
{
  MPFDEB((stderr, "BINC: doParsefull, toboundary[%s]\n", toboundary.c_str()));
  mimeSource = ms;
  headerstartoffsetcrlf = mimeSource->getOffset();

  // Parse the header of this mime part.
  parseHeader(&h, &nlines);

  // Headerlength includes the seperating CRLF. Body starts after the
  // CRLF.
  headerlength = mimeSource->getOffset() - headerstartoffsetcrlf;
  bodystartoffsetcrlf = mimeSource->getOffset();
  MPFDEB((stderr, "BINC: doParsefull, bodystartoffsetcrlf %d\n", bodystartoffsetcrlf));
  bodylength = 0;

  // Determine the type of mime part by looking at fields in the
  // header.
  analyzeHeader(&h, &multipart, &messagerfc822, &subtype, &boundary);

  bool eof = false;
  bool foundendofpart = false;

  if (messagerfc822) {
    parseMessageRFC822(&members, &foundendofpart, &bodylength,
		       &nbodylines, toboundary);

  } else if (multipart) {
    parseMultipart(boundary, toboundary, &eof, &nlines, &boundarysize,
		   &foundendofpart, &bodylength,
		   &members);
  } else {
    parseSinglePart(toboundary, &boundarysize, &nbodylines, &nlines,
		    &eof, &foundendofpart, &bodylength);
  }

  MPFDEB((stderr, "BINC: doParsefull ret, toboundary[%s]\n", toboundary.c_str()));
  return (eof || foundendofpart) ? 1 : 0;
}
