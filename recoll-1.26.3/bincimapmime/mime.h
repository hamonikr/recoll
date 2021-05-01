/* -*- mode:c++;c-basic-offset:2 -*- */
/*  --------------------------------------------------------------------
 *  Filename:
 *    src/parsers/mime/mime.h
 *  
 *  Description:
 *    Declaration of main mime parser components
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
#ifndef mime_h_included
#define mime_h_included
#include <string>
#include <vector>
#include <map>
#include <stdio.h>

namespace Binc {

  class MimeInputSource;


  //---------------------------------------------------------------------- 
  class HeaderItem {
  private:
    mutable std::string key;
    mutable std::string value;

  public:
    inline const std::string &getKey(void) const { return key; }
    inline const std::string &getValue(void) const { return value; }

    //--
    HeaderItem(void);
    HeaderItem(const std::string &key, const std::string &value);
  };

  //---------------------------------------------------------------------- 
  class Header {
  private:
    mutable std::vector<HeaderItem> content;

  public:
    bool getFirstHeader(const std::string &key, HeaderItem &dest) const;
    bool getAllHeaders(const std::string &key, std::vector<HeaderItem> &dest) const;
    void add(const std::string &name, const std::string &content);
    void clear(void);

    //--
    Header(void);
    ~Header(void);
  };

  //----------------------------------------------------------------------
  class IODevice;
  class MimeDocument;
  class MimePart {
  protected:
  public:
    mutable bool multipart;
    mutable bool messagerfc822;
    mutable std::string subtype;
    mutable std::string boundary;

    mutable unsigned int headerstartoffsetcrlf;
    mutable unsigned int headerlength;

    mutable unsigned int bodystartoffsetcrlf;
    mutable unsigned int bodylength;
    mutable unsigned int nlines;
    mutable unsigned int nbodylines;
    mutable unsigned int size;

  public:
    enum FetchType {
      FetchBody,
      FetchHeader,
      FetchMime
    };

    mutable Header h;

    mutable std::vector<MimePart> members;

    inline const std::string &getSubType(void) const { return subtype; }
    inline bool isMultipart(void) const { return multipart; }
    inline bool isMessageRFC822(void) const { return messagerfc822; }
    inline unsigned int getSize(void) const { return bodylength; }
    inline unsigned int getNofLines(void) const { return nlines; }
    inline unsigned int getNofBodyLines(void) const { return nbodylines; }
    inline unsigned int getBodyLength(void) const { return bodylength; }
    inline unsigned int getBodyStartOffset(void) const { return bodystartoffsetcrlf; }

    void printBody(Binc::IODevice &output, unsigned int startoffset, unsigned int length) const;
      void getBody(std::string& s, unsigned int startoffset, unsigned int length) const;
    virtual void clear(void);

    virtual int doParseOnlyHeader(MimeInputSource *ms, 
				  const std::string &toboundary);
    virtual int doParseFull(MimeInputSource *ms, 
			    const std::string &toboundary, int &boundarysize);

    MimePart(void);
    virtual ~MimePart(void);

  private:
    MimeInputSource *mimeSource;

    bool parseOneHeaderLine(Binc::Header *header, unsigned int *nlines);

    bool skipUntilBoundary(const std::string &delimiter,
			   unsigned int *nlines, bool *eof);
    inline void postBoundaryProcessing(bool *eof,
				       unsigned int *nlines,
				       int *boundarysize,
				       bool *foundendofpart);
      void parseMultipart(const std::string &boundary,
			   const std::string &toboundary,
			   bool *eof,
			   unsigned int *nlines,
			   int *boundarysize,
			   bool *foundendofpart,
			   unsigned int *bodylength,
			  std::vector<Binc::MimePart> *members);
      void parseSinglePart(const std::string &toboundary,
			    int *boundarysize,
			    unsigned int *nbodylines,
			    unsigned int *nlines,
			    bool *eof, bool *foundendofpart,
			   unsigned int *bodylength);
    void parseHeader(Binc::Header *header, unsigned int *nlines);
    void analyzeHeader(Binc::Header *header, bool *multipart,
		       bool *messagerfc822, std::string *subtype,
		       std::string *boundary);
    void parseMessageRFC822(std::vector<Binc::MimePart> *members,
			    bool *foundendofpart,
			    unsigned int *bodylength,
			    unsigned int *nbodylines,
			    const std::string &toboundary);
  };

  //----------------------------------------------------------------------
  class MimeDocument : public MimePart {
  public:
    MimeDocument(void);
    ~MimeDocument(void);

    void parseOnlyHeader(int fd);
    void parseFull(int fd);
    void parseOnlyHeader(std::istream& s);
    void parseFull(std::istream& s);

    void clear(void);
    
    bool isHeaderParsed(void) const 
    {
      return headerIsParsed; 
    }
    bool isAllParsed(void) const 
    { 
      return allIsParsed; 
    }

  private:
    bool headerIsParsed;
    bool allIsParsed;
    MimeInputSource *doc_mimeSource;
  };

};

#endif
