/* Copyright (C) 2013 J.F.Dockes
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
#ifndef _UNCOMP_H_INCLUDED_
#define _UNCOMP_H_INCLUDED_

#include <vector>
#include <string>
#include <mutex>

#include "pathut.h"
#include "rclutil.h"

/// Uncompression script interface.
class Uncomp {
public:
    explicit Uncomp(bool docache = false);
    ~Uncomp();

    /** Uncompress the input file into a temporary one, by executing the
     * script given as input. 
     * Return the path to the uncompressed file (which is inside a 
     * temporary directory).
     */
    bool uncompressfile(const std::string& ifn, 
			const std::vector<std::string>& cmdv,
			std::string& tfile);
    static void clearcache();
    
private:
    TempDir *m_dir{0};
    std::string   m_tfile;
    std::string   m_srcpath;
    bool m_docache;

    class UncompCache {
    public:
	UncompCache() {}
	~UncompCache() {
	    delete m_dir;
	}
        std::mutex m_lock;
	TempDir *m_dir{0};
	std::string   m_tfile;
	std::string   m_srcpath;
    };
    static UncompCache o_cache;
};

#endif /* _UNCOMP_H_INCLUDED_ */
