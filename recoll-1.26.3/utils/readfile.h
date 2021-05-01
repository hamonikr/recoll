/* Copyright (C) 2004 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef _READFILE_H_INCLUDED_
#define _READFILE_H_INCLUDED_

#include <sys/types.h>

#include <string>

class FileScanUpstream;

/** Data sink for the file reader. */
class FileScanDo {
public:
    virtual ~FileScanDo() {}
    /* Initialize and allocate. 
     * @param size if set, lower bound of data size.
     * @param reason[output] set to error message in case of error.
     * @return false for error (file_scan will return), true if ok.
     */
    virtual bool init(int64_t size, std::string *reason) = 0;
    /* Process chunk of data
     * @param buf  the data buffer.
     * @param cnt byte count.
     * @param reason[output] set to error message in case of error.
     * @return false for error (file_scan will return), true if ok.
     */
    virtual bool data(const char *buf, int cnt, std::string *reason) = 0;
    
    virtual void setUpstream(FileScanUpstream*) {}
};

/** Open and read file, calling the FileScanDo data() method for each chunk.
 *
 * @param filename File name. Use empty value for stdin

 * @param doer the data processor. The init() method will be called
 * initially witht a lower bound of the data size (may be used to
 * reserve a buffer), or with a 0 size if nothing is known about the
 * size. The data() method will be called for every chunk of data
 * read. 
 * @param offs Start offset. If not zero, will disable decompression 
 *             (set to -1 to start at 0 with no decompression).
 * @param cnt Max bytes in output. Set cnt to -1 for no limit.
 * @param[output] md5p If not null, points to a string to store the hex ascii 
 *     md5 of the uncompressed data.
 * @param[output] reason If not null, points to a string for storing an 
 *     error message if the return value is false.
 * @return true if the operation ended normally, else false.
 */
bool file_scan(const std::string& fn, FileScanDo* doer, int64_t startoffs,
               int64_t cnttoread, std::string *reason
#ifdef READFILE_ENABLE_MD5
               , std::string *md5p
#endif
    );

/** Same as above, not offset/cnt/md5 */
bool file_scan(const std::string& filename, FileScanDo* doer,
               std::string *reason);

/** Same as file_scan, from a memory buffer. No libz processing */
bool string_scan(const char *data, size_t cnt, FileScanDo* doer, 
                 std::string *reason
#ifdef READFILE_ENABLE_MD5
                 , std::string *md5p
#endif
    );

#if defined(READFILE_ENABLE_MINIZ)
/* Process a zip archive member */
bool file_scan(const std::string& filename, const std::string& membername,
               FileScanDo* doer, std::string *reason);
bool string_scan(const char* data, size_t cnt, const std::string& membername,
                 FileScanDo* doer, std::string *reason);
#endif

/**
 * Read file into string.
 * @return true for ok, false else
 */
bool file_to_string(const std::string& filename, std::string& data,
                    std::string *reason = 0);

/** Read file chunk into string. Set cnt to -1 for going to
 * eof, offs to -1 for going from the start without decompression */
bool file_to_string(const std::string& filename, std::string& data,
                    int64_t offs, size_t cnt, std::string *reason = 0);


#endif /* _READFILE_H_INCLUDED_ */
