/* Copyright (C) 2006-2021 J.F.Dockes
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
#ifndef _RCLASPELL_H_INCLUDED_
#define _RCLASPELL_H_INCLUDED_

/* autoconfig.h must be included before this file */
#ifdef RCL_USE_ASPELL

/**
 * Aspell speller interface class.
 *
 * Aspell is used to let the user find about spelling variations that may 
 * exist in the document set for a given word.
 * A specific aspell dictionary is created out of all the terms in the 
 * xapian index, and we then use it to expand a term to spelling neighbours.
 */

#include <string>
#include <vector>

#include "rclconfig.h"
#include "rcldb.h"

class AspellData;

class Aspell {
public:
    Aspell(const RclConfig *cnf);
    ~Aspell();

    /** Check health */
    bool ok() const;

    /** Find the aspell command and shared library, init function pointers */
    bool init(std::string &reason); 

    /**  Build dictionary out of index term list. This is done at the end
     * of an indexing pass. */
    bool buildDict(Rcl::Db &db, std::string &reason);

    /** Return a list of possible expansions for a given word */
    bool suggest(Rcl::Db &db, const std::string& term, 
                 std::vector<std::string> &suggestions, std::string &reason);
    
private:
    std::string dicPath();
    const RclConfig  *m_config;
    std::string      m_lang;
    AspellData *m_data{nullptr};

    bool make_speller(std::string& reason);
};

#endif /* RCL_USE_ASPELL */
#endif /* _RCLASPELL_H_INCLUDED_ */
