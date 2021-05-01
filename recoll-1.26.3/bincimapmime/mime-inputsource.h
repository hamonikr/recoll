/* -*- mode:c++;c-basic-offset:2 -*- */
/*  --------------------------------------------------------------------
 *  Filename:
 *    src/mime-inputsource.h
 *  
 *  Description:
 *    The base class of the MIME input source
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
#ifndef mime_inputsource_h_included
#define mime_inputsource_h_included
#include "autoconfig.h"
// Data source for MIME parser

// Note about large files: we might want to change the unsigned int
// used for offsets into an off_t for intellectual satisfaction, but
// in the context of recoll, we could only get into trouble if a
// *single message* exceeded 2GB, which seems rather unlikely. When
// parsing a mailbox files, we read each message in memory and use the
// stream input source (from a memory buffer, no file offsets). When
// parsing a raw message file, it's only one message.

#include <string.h>
#include "safeunistd.h"

#include <iostream>

namespace Binc {

  class MimeInputSource {
  public:
    // Note that we do NOT take ownership of fd, won't close it on delete
    inline MimeInputSource(int fd, unsigned int start = 0);
    virtual inline ~MimeInputSource(void);

    virtual inline ssize_t fillRaw(char *raw, size_t nbytes);
    virtual inline void reset(void);

    virtual inline bool fillInputBuffer(void);
    inline void seek(unsigned int offset);
    inline bool getChar(char *c);
    inline void ungetChar(void);
    inline int getFileDescriptor(void) const;

    inline unsigned int getOffset(void) const;

  private:
    int fd;
    char data[16384];
    unsigned int offset;
    unsigned int tail;
    unsigned int head;
    unsigned int start;
    char lastChar;
  };

  inline MimeInputSource::MimeInputSource(int fd, unsigned int start)
  {
    this->fd = fd;
    this->start = start;
    offset = 0;
    tail = 0;
    head = 0;
    lastChar = '\0';
    memset(data, '\0', sizeof(data));

    seek(start);
  }

  inline MimeInputSource::~MimeInputSource(void)
  {
  }

  inline ssize_t MimeInputSource::fillRaw(char *raw, size_t nbytes)
  {
      return read(fd, raw, nbytes);
  }

  inline bool MimeInputSource::fillInputBuffer(void)
  {
    char raw[4096];
    ssize_t nbytes = fillRaw(raw, 4096);
    if (nbytes <= 0) {
      // FIXME: If ferror(crlffile) we should log this.
      return false;
    }

    for (ssize_t i = 0; i < nbytes; ++i) {
      const char c = raw[i];
      if (c == '\r') {
	if (lastChar == '\r') {
	  data[tail++ & (0x4000-1)] = '\r';
	  data[tail++ & (0x4000-1)] = '\n';
	}
      } else if (c == '\n') {
	data[tail++ & (0x4000-1)] = '\r';
	data[tail++ & (0x4000-1)] = '\n';
      } else {
	if (lastChar == '\r') {
	  data[tail++ & (0x4000-1)] = '\r';
	  data[tail++ & (0x4000-1)] = '\n';
	}

	data[tail++ & (0x4000-1)] = c;
      }
      
      lastChar = c;
    }

    return true;
  }

  inline void MimeInputSource::reset(void)
  {
    offset = head = tail = 0;
    lastChar = '\0';

    if (fd != -1)
      lseek(fd, 0, SEEK_SET);
  }

  inline void MimeInputSource::seek(unsigned int seekToOffset)
  {
    if (offset > seekToOffset)
      reset();
   
    char c;
    int n = 0;
    while (seekToOffset > offset) {
      if (!getChar(&c))
	break;
      ++n;
    }
  }

  inline bool MimeInputSource::getChar(char *c)
  {
    if (head == tail && !fillInputBuffer())
      return false;

    *c = data[head++ & (0x4000-1)];
    ++offset;
    return true;
  }

  inline void MimeInputSource::ungetChar()
  {
    --head;
    --offset;
  }

  inline int MimeInputSource::getFileDescriptor(void) const
  {
    return fd;
  }

  inline unsigned int MimeInputSource::getOffset(void) const
  {
    return offset;
  }

    ///////////////////////////////////
    class MimeInputSourceStream : public MimeInputSource {
  public:
    inline MimeInputSourceStream(istream& s, unsigned int start = 0);
    virtual inline ssize_t fillRaw(char *raw, size_t nb);
    virtual inline void reset(void);
  private:
      istream& s;
  };

  inline MimeInputSourceStream::MimeInputSourceStream(istream& si, 
						      unsigned int start)
      : MimeInputSource(-1, start), s(si)
  {
  }

  inline ssize_t MimeInputSourceStream::fillRaw(char *raw, size_t nb)
  {
    // Why can't streams tell how many characters were actually read
    // when hitting eof ?
    std::streampos st = s.tellg();
    s.seekg(0, ios::end);
    std::streampos lst = s.tellg();
    s.seekg(st);
    size_t nbytes = size_t(lst - st);
    if (nbytes > nb) {
	nbytes = nb;
    }
    if (nbytes <= 0) {
	return (ssize_t)-1;
    }

    s.read(raw, nbytes);
    return static_cast<ssize_t>(nbytes);
  }

  inline void MimeInputSourceStream::reset(void)
  {
      MimeInputSource::reset();
      s.seekg(0);
  }

}

#endif
