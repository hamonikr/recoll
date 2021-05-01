/* Copyright (C) 2017 J.F.Dockes
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

#include "zlibut.h"

#include <zlib.h>

#include "log.h"

using namespace std;

static void *allocmem(
    void *cp,    /* The array to grow. may be NULL */
    int  sz,     /* Unit size in bytes */
    int  *np,    /* Pointer to current allocation number */
    int  min,    /* Number to allocate the first time */
    int  maxinc) /* Maximum increment */
{
    if (cp == 0) {
        cp = malloc(min * sz);
        *np = cp ? min : 0;
        return cp;
    }

    int inc = (*np > maxinc) ?  maxinc : *np;
    if ((cp = realloc(cp, (*np + inc) * sz)) != 0) {
        *np += inc;
    }
    return cp;
}

class ZLibUtBuf::Internal {
public:
    Internal() {}
    ~Internal() {
        if (buf && dofree) {
            free(buf);
        }
    }
    bool grow(size_t n) {
        if (!initsz)
            initsz = n;
        buf = (char *)allocmem(buf, initsz, &alloc, 1, 20);
        return nullptr != buf;
    }
    int getAlloc() {
        return alloc * initsz;
    }
    char *buf{nullptr};
    int initsz{0}; // Set to first alloc size
    int alloc{0}; // Allocation count (allocmem()). Capa is alloc*inisz
    int datacnt{0}; // Data count
    bool dofree{true}; // Does buffer belong to me ?
    friend bool inflateToBuf(void* inp, unsigned int inlen, ZLibUtBuf& buf);
};

ZLibUtBuf::ZLibUtBuf()
{
    m = new Internal;
}
ZLibUtBuf::~ZLibUtBuf()
{
    delete m;
}

char *ZLibUtBuf::getBuf() const
{
    return m->buf;
}            
char *ZLibUtBuf::takeBuf()
{
    m->dofree = false;
    return m->buf;
}
size_t ZLibUtBuf::getCnt()
{
    return m->datacnt;
}

bool inflateToBuf(const void* inp, unsigned int inlen, ZLibUtBuf& buf)
{
    LOGDEB0("inflateToBuf: inlen " << inlen << "\n");

    z_stream d_stream; /* decompression stream */

    d_stream.zalloc = (alloc_func)0;
    d_stream.zfree = (free_func)0;
    d_stream.opaque = (voidpf)0;
    d_stream.next_in  = (Bytef*)inp;
    d_stream.avail_in = inlen;
    d_stream.next_out = 0;
    d_stream.avail_out = 0;

    int err;
    if ((err = inflateInit(&d_stream)) != Z_OK) {
        LOGERR("Inflate: inflateInit: err " << err << " msg "  <<
               d_stream.msg << "\n");
        return false;
    }

    for (;;) {
        LOGDEB2("InflateToDynBuf: avail_in " << d_stream.avail_in <<
                " total_in " << d_stream.total_in << " avail_out " <<
                d_stream.avail_out << " total_out " << d_stream.total_out <<
                "\n");
        if (d_stream.avail_out == 0) {
            if (!buf.m->grow(inlen)) {
                LOGERR("Inflate: out of memory, current alloc " <<
                       buf.m->getAlloc() << "\n");
                inflateEnd(&d_stream);
                return false;
            }
            d_stream.avail_out = buf.m->getAlloc() - d_stream.total_out;
            d_stream.next_out = (Bytef*)(buf.getBuf() + d_stream.total_out);
        }
        err = inflate(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) {
            break;
        }
        if (err != Z_OK) {
            LOGERR("Inflate: error " << err << " msg " <<
                   (d_stream.msg ? d_stream.msg : "") << endl);
            inflateEnd(&d_stream);
            return false;
        }
    }
    if ((err = inflateEnd(&d_stream)) != Z_OK) {
        LOGERR("Inflate: inflateEnd error " << err << " msg " <<
               (d_stream.msg ? d_stream.msg : "") << endl);
        return false;
    }
    buf.m->datacnt = d_stream.total_out;
    LOGDEB1("inflateToBuf: ok, output size " << buf.getCnt() << endl);
    return true;
}


bool deflateToBuf(const void* inp, unsigned int inlen, ZLibUtBuf& buf)
{
    uLongf len = compressBound(static_cast<uLong>(inlen));
    // This needs cleanup: because the buffer is reused inside
    // e.g. circache, we want a minimum size in case the 1st doc size,
    // which sets the grow increment is small. It would be better to
    // let the user set a min size hint.
    if (len < 500 *1024)
        len = 500 * 1024;

    while (buf.m->getAlloc() < int(len)) {
        if (!buf.m->grow(len)) {
            LOGERR("deflateToBuf: can't get buffer for " << len << " bytes\n");
            return false;
        }
    }
    bool ret = compress((Bytef*)buf.getBuf(), &len, (Bytef*)inp,
                        static_cast<uLong>(inlen)) == Z_OK;
    buf.m->datacnt = len;
    return ret;
}
