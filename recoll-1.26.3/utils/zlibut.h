#ifndef _ZLIBUT_H_INCLUDED_
#define _ZLIBUT_H_INCLUDED_

#include <sys/types.h>

class ZLibUtBuf {
public:
    ZLibUtBuf();
    ~ZLibUtBuf();
    char *getBuf() const;
    char *takeBuf();
    size_t getCnt();

    class Internal;
    Internal *m;
};

bool inflateToBuf(const void* inp, unsigned int inlen, ZLibUtBuf& buf);
bool deflateToBuf(const void* inp, unsigned int inlen, ZLibUtBuf& buf);

#endif /* _ZLIBUT_H_INCLUDED_ */
