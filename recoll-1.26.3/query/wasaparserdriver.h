/* Copyright (C) 2006 J.F.Dockes
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
#ifndef _WASAPARSERDRIVER_H_INCLUDED_
#define _WASAPARSERDRIVER_H_INCLUDED_

#include <string>
#include <stack>
#include <vector>

#include "smallut.h"

class WasaParserDriver;
namespace Rcl {
    class SearchData;
    class SearchDataClauseSimple;
}
namespace yy {
    class parser;
}

class RclConfig;

class WasaParserDriver {
public:
    
    WasaParserDriver(const RclConfig *c, const std::string sl, 
                     const std::string& as);
    ~WasaParserDriver();
    
    Rcl::SearchData *parse(const std::string&);
    bool addClause(Rcl::SearchData *sd, Rcl::SearchDataClauseSimple* cl);

    int GETCHAR();
    void UNGETCHAR(int c);

    std::string& qualifiers() {
        return m_qualifiers;
    }
    void setreason(const std::string& reason) {
        m_reason = reason;
    }
    const std::string& getreason() const {
        return m_reason;
    }
    
private:
    friend class yy::parser;

    std::string m_stemlang;
    std::string m_autosuffs;
    const RclConfig  *m_config;

    // input string.
    std::string m_input;
    // Current position in m_input
    unsigned int m_index;
    // Characters pushed-back, ready for next getchar.
    std::stack<int> m_returns;
    // Result, set by parser.
    Rcl::SearchData *m_result;

    // Storage for top level filters
    std::vector<std::string>  m_filetypes; 
    std::vector<std::string>  m_nfiletypes;
    bool                      m_haveDates;
    DateInterval              m_dates; // Restrict to date interval
    size_t                    m_maxSize;
    size_t                    m_minSize;

    std::string m_reason;

    // Let the quoted string reader store qualifiers in there, simpler
    // than handling this in the parser, because their nature is
    // determined by the absence of white space after the closing
    // dquote. e.g "some term"abc. We could avoid this by making white
    // space a token.
    std::string m_qualifiers;
};


#endif /* _WASAPARSERDRIVER_H_INCLUDED_ */
