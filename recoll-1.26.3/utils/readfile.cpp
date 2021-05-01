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
#ifdef BUILDING_RECOLL
#include "autoconfig.h"
#else
#include "config.h"
#endif

#include "readfile.h"

#include <errno.h>
#include <sys/types.h>

#ifdef _WIN32
#include "safefcntl.h"
#include "safesysstat.h"
#include "safeunistd.h"
#include "transcode.h"
#define OPEN _wopen

#else
#define O_BINARY 0
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#define OPEN open

#endif

#include <string>

#include "smallut.h"
#include "pathut.h"

#ifdef READFILE_ENABLE_MD5
#include "md5.h"
#endif

#ifdef MDU_INCLUDE_LOG
#include MDU_INCLUDE_LOG
#else
#include "log.h"
#endif

using namespace std;

///////////////
// Implementation of basic interface: read whole file to memory buffer
class FileToString : public FileScanDo {
public:
    FileToString(string& data) : m_data(data) {}

    // Note: the fstat() + reserve() (in init()) calls divide cpu
    // usage almost by 2 on both linux i586 and macosx (compared to
    // just append()) Also tried a version with mmap, but it's
    // actually slower on the mac and not faster on linux.
    virtual bool init(int64_t size, string *reason) {
        if (size > 0) {
            m_data.reserve(size);
        }
        return true;
    }
    virtual bool data(const char *buf, int cnt, string *reason) {
        try {
            m_data.append(buf, cnt);
        } catch (...) {
            catstrerror(reason, "append", errno);
            return false;
        }
        return true;
    }

    string& m_data;
};

bool file_to_string(const string& fn, string& data, int64_t offs, size_t cnt,
                    string *reason)
{
    FileToString accum(data);
    return file_scan(fn, &accum, offs, cnt, reason
#ifdef READFILE_ENABLE_MD5
                     , nullptr
#endif
        );
}

bool file_to_string(const string& fn, string& data, string *reason)
{
    return file_to_string(fn, data, 0, size_t(-1), reason);
}


/////////////
//  Callback/filtering interface

// Abstract class base for both source (origin) and filter
// (midstream). Both have a downstream
class FileScanUpstream {
public:
    virtual void setDownstream(FileScanDo *down) {
        m_down = down;
    }
    virtual FileScanDo *out() {
        return m_down;
    }
protected:        
    FileScanDo *m_down{nullptr};
};

// Source element.
class FileScanSource : public FileScanUpstream {
public:
    FileScanSource(FileScanDo *down) {
        setDownstream(down);
    }
    virtual bool scan() = 0;
};

// Inside element of a transformation pipe. The idea is that elements
// which don't recognize the data get themselves out of the pipe
// (pop()). Typically, only one of the decompression modules
// (e.g. gzip/bzip2/xz...) would remain. For now there is only gzip,
// it pops itself if the data does not have the right magic number
class FileScanFilter : public FileScanDo, public FileScanUpstream {
public:
    virtual void insertAtSink(FileScanDo *sink, FileScanUpstream *upstream) {
        setDownstream(sink);
        if (m_down) {
            m_down->setUpstream(this);
        }
        setUpstream(upstream);
        if (m_up) {
            m_up->setDownstream(this);
        }
    }

    // Remove myself from the pipe. 
    virtual void pop() {
        if (m_down) {
            m_down->setUpstream(m_up);
        }
        if (m_up) {
            m_up->setDownstream(m_down);
        }
    }

    virtual void setUpstream(FileScanUpstream *up) override {
        m_up = up;
    }

private:
    FileScanUpstream *m_up{nullptr};
};


#if defined(READFILE_ENABLE_ZLIB)
#include <zlib.h>

class GzFilter : public FileScanFilter {
public:
    virtual ~GzFilter() {
        if (m_initdone) {
            inflateEnd(&m_stream);
        }
    }

    virtual bool init(int64_t size, string *reason) override {
        LOGDEB1("GzFilter::init\n");
        if (out()) {
            return out()->init(size, reason);
        }
        return true;
    }

    virtual bool data(const char *buf, int cnt, string *reason) override {
        LOGDEB1("GzFilter::data: cnt " << cnt << endl);

        int error;
        m_stream.next_in = (Bytef*)buf;
        m_stream.avail_in = cnt;
        
        if (m_initdone == false) {
            // We do not support a first read cnt < 2. This quite
            // probably can't happen with a compressed file (size>2)
            // except if we're reading a tty which is improbable. So
            // assume this is a regular file.
            const unsigned char *ubuf = (const unsigned char *)buf;
            if ((cnt < 2) || ubuf[0] != 0x1f || ubuf[1] != 0x8b) {
                LOGDEB1("GzFilter::data: not gzip. out() is " << out() << "\n");
                pop();
                if (out()) {
                    return out()->data(buf, cnt, reason);
                } else {
                    return false;
                }
            }
            m_stream.opaque = nullptr;
            m_stream.zalloc = alloc_func;
            m_stream.zfree = free_func;
            m_stream.next_out = (Bytef*)m_obuf;
            m_stream.avail_out = m_obs;
            if ((error = inflateInit2(&m_stream, 15+32)) != Z_OK) {
                LOGERR("inflateInit2 error: " << error << endl);
                if (reason) {
                    *reason += " Zlib inflateinit failed";
                    if (m_stream.msg && *m_stream.msg) {
                        *reason += string(": ") + m_stream.msg;
                    }
                }
                return false;
            }
            m_initdone = true;
        }
        
        while (m_stream.avail_in != 0) {
            m_stream.next_out = (Bytef*)m_obuf;
            m_stream.avail_out = m_obs;
            if ((error = inflate(&m_stream, Z_SYNC_FLUSH)) < Z_OK) {
                LOGERR("inflate error: " << error << endl);
                if (reason) {
                    *reason += " Zlib inflate failed";
                    if (m_stream.msg && *m_stream.msg) {
                        *reason += string(": ") + m_stream.msg;
                    }
                }
                return false;
            }
            if (out() &&
                !out()->data(m_obuf, m_obs - m_stream.avail_out, reason)) {
                return false;
            }
        }
        return true;
    }
    
    static voidpf alloc_func(voidpf opaque, uInt items, uInt size) {
        return malloc(items * size);
    }
    static void free_func(voidpf opaque, voidpf address) {
        free(address);
    }

    bool m_initdone{false};
    z_stream m_stream;
    char m_obuf[10000];
    const int m_obs{10000};
};
#endif // GZ

#ifdef READFILE_ENABLE_MD5

class FileScanMd5 : public FileScanFilter {
public:
    FileScanMd5(string& d) : digest(d) {}
    virtual bool init(int64_t size, string *reason) override {
        LOGDEB1("FileScanMd5: init\n");
	MD5Init(&ctx);
        if (out()) {
            return out()->init(size, reason);
        }
	return true;
    }
    virtual bool data(const char *buf, int cnt, string *reason) override {
        LOGDEB1("FileScanMd5: data. cnt " << cnt << endl);
	MD5Update(&ctx, (const unsigned char*)buf, cnt);
        if (out() && !out()->data(buf, cnt, reason)) {
            return false;
        }
	return true;
    }
    bool finish() {
        LOGDEB1("FileScanMd5: finish\n");
        MD5Final(digest, &ctx);
        return true;
    }
    string &digest;
    MD5_CTX ctx;
};
#endif // MD5

// Source taking data from a regular file
class FileScanSourceFile : public FileScanSource {
public:
    FileScanSourceFile(FileScanDo *next, const string& fn, int64_t startoffs,
                       int64_t cnttoread, string *reason)
        : FileScanSource(next), m_fn(fn), m_startoffs(startoffs),
          m_cnttoread(cnttoread), m_reason(reason) { }

    virtual bool scan() {
        LOGDEB1("FileScanSourceFile: reading " << m_fn << " offs " <<
               m_startoffs<< " cnt " << m_cnttoread << " out " << out() << endl);
        const int RDBUFSZ = 8192;
        bool ret = false;
        bool noclosing = true;
        int fd = 0;
        struct stat st;
        // Initialize st_size: if fn.empty() , the fstat() call won't happen.
        st.st_size = 0;

        // If we have a file name, open it, else use stdin.
        if (!m_fn.empty()) {
            SYSPATH(m_fn, realpath);
            fd = OPEN(realpath, O_RDONLY | O_BINARY);
            if (fd < 0 || fstat(fd, &st) < 0) {
                catstrerror(m_reason, "open/stat", errno);
                return false;
            }
            noclosing = false;
        }

#if defined O_NOATIME && O_NOATIME != 0
        if (fcntl(fd, F_SETFL, O_NOATIME) < 0) {
            // perror("fcntl");
        }
#endif
        if (out()) {
            if (m_cnttoread != -1 && m_cnttoread) {
                out()->init(m_cnttoread + 1, m_reason);
            } else if (st.st_size > 0) {
                out()->init(st.st_size + 1, m_reason);
            } else {
                out()->init(0, m_reason);
            }
        }

        int64_t curoffs = 0;
        if (m_startoffs > 0 && !m_fn.empty()) {
            if (lseek(fd, m_startoffs, SEEK_SET) != m_startoffs) {
                catstrerror(m_reason, "lseek", errno);
                return false;
            }
            curoffs = m_startoffs;
        }

        char buf[RDBUFSZ];
        int64_t totread = 0;
        for (;;) {
            size_t toread = RDBUFSZ;
            if (m_startoffs > 0 && curoffs < m_startoffs) {
                toread = size_t(MIN(RDBUFSZ, m_startoffs - curoffs));
            }

            if (m_cnttoread != -1) {
                toread = MIN(toread, (uint64_t)(m_cnttoread - totread));
            }
            ssize_t n = static_cast<ssize_t>(read(fd, buf, toread));
            if (n < 0) {
                catstrerror(m_reason, "read", errno);
                goto out;
            }
            if (n == 0) {
                break;
            }
            curoffs += n;
            if (curoffs - n < m_startoffs) {
                continue;
            }
            if (!out()->data(buf, n, m_reason)) {
                goto out;
            }
            totread += n;
            if (m_cnttoread > 0 && totread >= m_cnttoread) {
                break;
            }
        }

        ret = true;
    out:
        if (fd >= 0 && !noclosing) {
            close(fd);
        }
        return ret;
    }
    
protected:
    string m_fn;
    int64_t m_startoffs;
    int64_t m_cnttoread;
    string *m_reason;
};


#if defined(READFILE_ENABLE_MINIZ)
#include "miniz.h"

// Source taking data from a ZIP archive member
class FileScanSourceZip : public FileScanSource {
public:
    FileScanSourceZip(FileScanDo *next, const string& fn,
                      const string& member, string *reason)
        : FileScanSource(next), m_fn(fn), m_member(member),
          m_reason(reason) {}

    FileScanSourceZip(const char *data, size_t cnt, FileScanDo *next,
                      const string& member, string *reason)
        : FileScanSource(next), m_data(data), m_cnt(cnt), m_member(member),
          m_reason(reason) {}

    virtual bool scan() {
        bool ret = false;
        mz_zip_archive zip;
        mz_zip_zero_struct(&zip);
        void *opaque = this;

        bool ret1;
        if (m_fn.empty()) {
            ret1 = mz_zip_reader_init_mem(&zip, m_data, m_cnt, 0);
        } else {
            SYSPATH(m_fn, realpath);
            ret1 = mz_zip_reader_init_file(&zip, realpath, 0);
        }
        if (!ret1) {
            if (m_reason) {
                *m_reason += "mz_zip_reader_init_xx() failed: ";
                *m_reason +=
                    string(mz_zip_get_error_string(zip.m_last_error));
            }
            return false;
        }

        mz_uint32 file_index;
        if (mz_zip_reader_locate_file_v2(&zip, m_member.c_str(), NULL, 0,
                                         &file_index) < 0) {
            if (m_reason) {
                *m_reason += "mz_zip_reader_locate_file() failed: ";
                *m_reason += string(mz_zip_get_error_string(zip.m_last_error));
            }
            goto out;
        }

        mz_zip_archive_file_stat zstat;
        if (!mz_zip_reader_file_stat(&zip, file_index, &zstat)) {
            if (m_reason) {
                *m_reason += "mz_zip_reader_file_stat() failed: ";
                *m_reason += string(mz_zip_get_error_string(zip.m_last_error));
            }
            goto out;
        }
        if (out()) {
            if (!out()->init(zstat.m_uncomp_size, m_reason)) {
                goto out;
            }
        }
                
        if (!mz_zip_reader_extract_to_callback(
                &zip, file_index, write_cb, opaque, 0)) {
            if (m_reason) {
                *m_reason += "mz_zip_reader_extract_to_callback() failed: ";
                *m_reason += string(mz_zip_get_error_string(zip.m_last_error));
            }
            goto out;
        }
        
        ret = true;
    out:
        mz_zip_reader_end(&zip);
        return ret;
    }

    static size_t write_cb(void *pOpaque, mz_uint64 file_ofs,
                           const void *pBuf, size_t n) {
        const char *cp = (const char*)pBuf;
        LOGDEB1("write_cb: ofs " << file_ofs << " cnt " << n << " data: " <<
                string(cp, n) << endl);
        FileScanSourceZip *ths = (FileScanSourceZip *)pOpaque;
        if (ths->out()) {
            if (!ths->out()->data(cp, n, ths->m_reason)) {
                return (size_t)-1;
            }
        }
        return n;
    }
    
protected:
    const char *m_data;
    size_t m_cnt;
    string m_fn;
    string m_member;
    string *m_reason;
};

bool file_scan(const std::string& filename, const std::string& membername,
               FileScanDo* doer, std::string *reason)
{
    if (membername.empty()) {
        return file_scan(filename, doer, 0, -1, reason
#ifdef READFILE_ENABLE_MD5
, nullptr
#endif
            );
    } else {
            FileScanSourceZip source(doer, filename, membername, reason);
            return source.scan();
    }
}

bool string_scan(const char *data, size_t cnt, const std::string& membername,
                 FileScanDo* doer, std::string *reason)
{
    if (membername.empty()) {
        return string_scan(data, cnt, doer, reason
#ifdef READFILE_ENABLE_MD5
, nullptr
#endif
            );                           
    } else {
        FileScanSourceZip source(data, cnt, doer, membername, reason);
        return source.scan();
    }
}

#endif // READFILE_ENABLE_ZIP

bool file_scan(const string& fn, FileScanDo* doer, int64_t startoffs,
               int64_t cnttoread, string *reason
#ifdef READFILE_ENABLE_MD5
               , string *md5p
#endif
    )
{
    LOGDEB1("file_scan: doer " << doer << endl);
#if defined(READFILE_ENABLE_ZLIB)
    bool nodecomp = startoffs != 0;
#endif
    if (startoffs < 0) {
        startoffs = 0;
    }
    
    FileScanSourceFile source(doer, fn, startoffs, cnttoread, reason);
    FileScanUpstream *up = &source;
    up = up;
    
#if defined(READFILE_ENABLE_ZLIB)
    GzFilter gzfilter;
    if (!nodecomp) {
        gzfilter.insertAtSink(doer, up);
        up = &gzfilter;
    }
#endif

#ifdef READFILE_ENABLE_MD5
    // We compute the MD5 on the uncompressed data, so insert this
    // right at the source (after the decompressor).
    string digest;
    FileScanMd5 md5filter(digest);
    if (md5p) {
        md5filter.insertAtSink(doer, up);
        up = &md5filter;
    }
#endif
    
    bool ret = source.scan();

#ifdef READFILE_ENABLE_MD5
    if (md5p) {
        md5filter.finish();
        MD5HexPrint(digest, *md5p);
    }
#endif
    return ret;
}

bool file_scan(const string& fn, FileScanDo* doer, string *reason)
{
    return file_scan(fn, doer, 0, -1, reason
#ifdef READFILE_ENABLE_MD5
, nullptr
#endif
        );                           
}


class FileScanSourceBuffer : public FileScanSource {
public:
    FileScanSourceBuffer(FileScanDo *next, const char *data, size_t cnt,
                         string *reason)
        : FileScanSource(next), m_data(data), m_cnt(cnt), m_reason(reason) {}

    virtual bool scan() {
        if (out()) {
            if (!out()->init(m_cnt, m_reason)) {
                return false;
            }
            return out()->data(m_data, m_cnt, m_reason);
        } else {
            return true;
        }
    }
    
protected:
    const char *m_data{nullptr};
    size_t m_cnt{0};
    string *m_reason{nullptr};
};

bool string_scan(const char *data, size_t cnt, FileScanDo* doer,
                 std::string *reason
#ifdef READFILE_ENABLE_MD5
                 , std::string *md5p
#endif
    )
{
    FileScanSourceBuffer source(doer, data, cnt, reason);
    FileScanUpstream *up = &source;
    up = up;
    
#ifdef READFILE_ENABLE_MD5
    string digest;
    FileScanMd5 md5filter(digest);
    if (md5p) {
        md5filter.insertAtSink(doer, up);
        up = &md5filter;
    }
#endif
    
    bool ret = source.scan();

#ifdef READFILE_ENABLE_MD5
    if (md5p) {
        md5filter.finish();
        MD5HexPrint(digest, *md5p);
    }
#endif
    return ret;
}

