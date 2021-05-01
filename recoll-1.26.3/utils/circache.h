/* Copyright (C) 2009 J.F.Dockes
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
#ifndef _circache_h_included_
#define _circache_h_included_

/**
 * A data cache implemented as a circularly managed file
 *
 * A single file is used to stored objects. The file grows to a
 * specified maximum size, then is rewritten from the start,
 * overwriting older entries.
 *
 * Data objects inside the cache each have two parts: a data segment and an
 * attribute (metadata) dictionary.
 * They are named using the same identifiers that are used inside the Recoll
 * index (the UDI).
 *
 * Inside the file. the UDIs are stored inside the entry dictionary
 * under the key "udi".
 *
 * It is assumed that the dictionary are small (they are routinely read/parsed)
 *
 */

#include <sys/types.h>
#include <stdint.h>

#include <string>

class ConfSimple;
class CirCacheInternal;

class CirCache {
public:
    CirCache(const std::string& dir);
    virtual ~CirCache();

    virtual std::string getReason();

    enum CreateFlags {CC_CRNONE = 0,
                      // Unique entries: erase older instances when same udi
                      // is stored.
                      CC_CRUNIQUE = 1,
                      // Truncate file (restart from scratch).
                      CC_CRTRUNCATE = 2
                     };
    virtual bool create(int64_t maxsize, int flags);

    enum OpMode {CC_OPREAD, CC_OPWRITE};
    virtual bool open(OpMode mode);

    virtual std::string getpath();

    // Set data to 0 if you just want the header
    virtual bool get(const std::string& udi, std::string& dic,
                     std::string *data = 0, int instance = -1);

    // Note: the dicp MUST have an udi entry
    enum PutFlags {NoCompHint = 1};
    virtual bool put(const std::string& udi, const ConfSimple *dicp,
                     const std::string& data, unsigned int flags = 0);

    virtual bool erase(const std::string& udi, bool reallyclear = false);

    /** Walk the archive.
     *
     * Maybe we'll have separate iterators one day, but this is good
     * enough for now. No put() operations should be performed while
     * using these.
     */
    /** Back to oldest */
    virtual bool rewind(bool& eof);
    /** Get entry under cursor */
    virtual bool getCurrent(std::string& udi, std::string& dic,
                            std::string *data = 0);
    /** Get current entry udi only. Udi can be empty (erased empty), caller
     * should call again */
    virtual bool getCurrentUdi(std::string& udi);
    /** Skip to next. (false && !eof) -> error, (false&&eof)->EOF. */
    virtual bool next(bool& eof);

    /* Debug. This writes the entry headers to stdout */
    virtual bool dump();

    /* Utility: append all entries from sdir to ddir. 
     * 
     * This does not need to be a member at all, just using the namespace here.
     *
     * @param ddir destination circache (must be previously created
     *     with appropriate size)
     * @param sdir source circache
     * @ret number of entries copied or -a
     */
    static int append(const std::string ddir, const std::string& sdir,
                      std::string *reason = 0);

protected:
    CirCacheInternal *m_d;
    std::string m_dir;
private:
    CirCache(const CirCache&) {}
    CirCache& operator=(const CirCache&) {
        return *this;
    }
};

#endif /* _circache_h_included_ */
