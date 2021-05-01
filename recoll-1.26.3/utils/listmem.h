#ifndef _LISTMEM_H_INCLUDED_
#define _LISTMEM_H_INCLUDED_
#include <ostream>

enum ListmemOpts {LISTMEM_SWAP16 = 1, LISTMEM_SWAP32 = 2};

/// @param startadr starting value for offset listings on the right
extern void listmem(std::ostream&, const void *ptr, int sz,
                    int startadr = 0, int opts = 0);

#endif /* _LISTMEM_H_INCLUDED_ */
