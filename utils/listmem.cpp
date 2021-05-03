#include "listmem.h"

#include <stdlib.h>
#include <string.h>
#include <iomanip>

using namespace std;

/*
 * Functions to list a memory buffer:
 */

/* Turn byte into Hexadecimal ascii representation */
static char *hexa(unsigned int i)
{
    int j;
    static char asc[3];

    asc[0] = (i >> 4) & 0x0f;
    asc[1] = i & 0x0f;
    asc[2] = 0;
    for (j = 0; j < 2; j++)
        if (asc[j] > 9) {
            asc[j] += 55;
        } else {
            asc[j] += 48;
        }
    return (asc);
}

static void swap16(unsigned char *d, const unsigned char *s, int n)
{
    if (n & 1) {
        n >>= 1;
        n++;
    } else {
        n >>= 1;
    }
    while (n--) {
        int i;
        i = 2 * n;
        d[i] = s[i + 1];
        d[i + 1] = s[i];
    }
}

static void swap32(unsigned char *d, const unsigned char *s, int n)
{
    if (n & 3) {
        n >>= 2;
        n++;
    } else {
        n >>= 2;
    }
    while (n--) {
        int i;
        i = 4 * n;
        d[i] = s[i + 3];
        d[i + 1] = s[i + 2];
        d[i + 2] = s[i + 1];
        d[i + 3] = s[i];
    }
}

/* Turn byte buffer into hexadecimal representation */
void charbuftohex(int len, unsigned char *dt, int maxlen, char *str)
{
    int i;
    char *bf;

    for (i = 0, bf = str; i < len; i++) {
        char *cp;
        if (bf - str >= maxlen - 4) {
            break;
        }
        cp = hexa((unsigned int)dt[i]);
        *bf++ = *cp++;
        *bf++ = *cp++;
        *bf++ = ' ';
    }
    *bf++ = 0;
}

void listmem(ostream& os, const void *_ptr, int siz, int adr, int opts)
{
    const unsigned char *ptr = (const unsigned char *)_ptr;
    int             i, j, c;
    char            lastlisted[16];
    int             alreadysame = 0;
    int         oneout = 0;
    unsigned char  *mpt;

    if (opts & (LISTMEM_SWAP16 | LISTMEM_SWAP32)) {
        if ((mpt = (unsigned char *)malloc(siz + 4)) == NULL) {
            os << "OUT OF MEMORY\n";
            return;
        }
        if (opts & LISTMEM_SWAP16) {
            swap16(mpt, ptr, siz);
        } else if (opts & LISTMEM_SWAP32) {
            swap32(mpt, ptr, siz);
        }
    } else {
        mpt = (unsigned char *)ptr;
    }

    for (i = 0; i < siz; i += 16) {
        /* Check for same data (only print first line in this case) */
        if (oneout != 0  &&
                siz - i >= 16 && memcmp(lastlisted, mpt + i, 16) == 0) {
            if (alreadysame == 0) {
                os << "*\n";
                alreadysame = 1;
            }
            continue;
        }
        alreadysame = 0;
        /* Line header */
        os << std::setw(4) << i + adr << " ";

        /* Hexadecimal representation */
        for (j = 0; j < 16; j++) {
            if ((i + j) < siz) {
                os << hexa(mpt[i + j]) << ((j & 1) ? " " : "");
            } else {
                os << "  " << ((j & 1) ? " " : "");
            }
        }
        os << "  ";

        /* Also print ascii for values that fit */
        for (j = 0; j < 16; j++) {
            if ((i + j) < siz) {
                c = mpt[i + j];
                if (c >= 0x20 && c <= 0x7f) {
                    os << char(c);
                } else {
                    os << ".";
                }
            } else {
                os << " ";
            }
        }
        os << "\n";
        memcpy(lastlisted, mpt + i, 16);
        oneout = 1;
    }
    if (mpt != ptr) {
        free(mpt);
    }
}
