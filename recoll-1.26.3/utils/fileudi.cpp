/* Copyright (C) 2005 J.F.Dockes
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
#ifndef TEST_FILEUDI
#include "autoconfig.h"

#include <stdio.h>
#include <cstdlib>

#include "fileudi.h"
#include "md5.h"
#include "base64.h"

using std::string;

// Size of the hashed result (base64 of 16 bytes of md5, minus 2 pad chars)
#define HASHLEN 22

// Convert longish paths by truncating and appending hash of path
// The full length of the base64-encoded (minus pad) of the md5 is 22 chars
// We append this to the truncated path
void pathHash(const std::string &path, std::string &phash, unsigned int maxlen)
{
    if (maxlen < HASHLEN) {
	fprintf(stderr, "pathHash: internal error: requested len too small\n");
	abort();
    }

    if (path.length() <= maxlen) {
	phash = path;
	return;
    }

    // Compute the md5
    unsigned char chash[16];
    MD5_CTX ctx;
    MD5Init(&ctx);
    MD5Update(&ctx, (const unsigned char *)(path.c_str()+maxlen-HASHLEN), 
	      path.length() - (maxlen - HASHLEN));
    MD5Final(chash, &ctx);

    // Encode it to ascii. This shouldn't be strictly necessary as
    // xapian terms can be binary
    string hash;
    base64_encode(string((char *)chash, 16), hash);
    // We happen to know there will be 2 pad chars in there, that we
    // don't need as this won't ever be decoded. Resulting length is 22
    hash.resize(hash.length() - 2);

    // Truncate path and append hash
    phash = path.substr(0, maxlen - HASHLEN) + hash;
}


// Maximum length for path/unique terms stored for each document. We truncate
// longer paths and uniquize them by appending a hashed value. This
// is done to avoid xapian max term length limitations, not
// to gain space (we gain very little even with very short maxlens
// like 30). The xapian max key length seems to be around 250.
// The value for PATHHASHLEN includes the length of the hash part.
#define PATHHASHLEN 150

// Compute the unique term used to link documents to their file-system source:
// Hashed path + possible internal path
void make_udi(const string& fn, const string& ipath, string &udi)
{
    string s(fn);
    // Note that we append a "|" in all cases. Historical, could be removed
    s.append("|");
    s.append(ipath);
    pathHash(s, udi, PATHHASHLEN);
    return;
}

#else // TEST_FILEUDI
#include <stdio.h>
#include <string>
#include "fileudi.h"

using namespace std;

int main(int argc, char **argv)
{
    string path="/usr/lib/toto.cpp";
    string ipath = "1:2:3:4:5:10";
    string udi;
    make_udi(path, ipath, udi);
    printf("udi [%s]\n", udi.c_str());
    path = "/some/much/too/looooooooooooooong/path/bla/bla/bla"
	"/looooooooooooooong/path/bla/bla/bla/llllllllllllllllll"
	"/looooooooooooooong/path/bla/bla/bla/llllllllllllllllll";
    ipath = "1:2:3:4:5:10"
	"1:2:3:4:5:10"
	"1:2:3:4:5:10";
    make_udi(path, ipath, udi);
    printf("udi [%s]\n", udi.c_str());
}
#endif // TEST_FILEUDI
