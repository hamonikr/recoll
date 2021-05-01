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
#include "autoconfig.h"

#include <iostream>

#include "wasatorcl.h"
#include "wasaparserdriver.h"
#include "searchdata.h"
#include "log.h"

#define YYDEBUG 1

// bison-generated file
#include "wasaparse.hpp"

using namespace std;
using namespace Rcl;


void
yy::parser::error (const location_type& l, const std::string& m)
{
    d->setreason(m);
}


SearchData *wasaStringToRcl(const RclConfig *config,
                                 const std::string& stemlang,
                                 const std::string& query, string &reason,
                                 const std::string& autosuffs)
{
    WasaParserDriver d(config, stemlang, autosuffs);
    SearchData *sd = d.parse(query);
    if (!sd) 
        reason = d.getreason();
    return sd;
}

WasaParserDriver::WasaParserDriver(const RclConfig *c, const std::string sl, 
                                   const std::string& as)
    : m_stemlang(sl), m_autosuffs(as), m_config(c),
      m_index(0), m_result(0), m_haveDates(false), 
      m_maxSize((size_t)-1), m_minSize((size_t)-1)
{

}

WasaParserDriver::~WasaParserDriver()
{
}

SearchData *WasaParserDriver::parse(const std::string& in)
{
    m_input = in;
    m_index = 0;
    delete m_result;
    m_result = 0;
    m_returns = stack<int>();

    yy::parser parser(this);
    parser.set_debug_level(0);

    if (parser.parse() != 0) {
        delete m_result;
        m_result = 0;
    }

    if (m_result == 0)
        return m_result;

    // Set the top level filters (types, dates, size)
    for (vector<string>::const_iterator it = m_filetypes.begin();
         it != m_filetypes.end(); it++) {
        m_result->addFiletype(*it);
    }
    for (vector<string>::const_iterator it = m_nfiletypes.begin();
         it != m_nfiletypes.end(); it++) {
        m_result->remFiletype(*it);
    }
    if (m_haveDates) {
        m_result->setDateSpan(&m_dates);
    }
    if (m_minSize != (size_t)-1) {
        m_result->setMinSize(m_minSize);
    }
    if (m_maxSize != (size_t)-1) {
        m_result->setMaxSize(m_maxSize);
    }
    //if (m_result)  m_result->dump(cout);
    return m_result;
}

int WasaParserDriver::GETCHAR()
{
    if (!m_returns.empty()) {
        int c = m_returns.top();
        m_returns.pop();
        return c;
    }
    if (m_index < m_input.size())
        return m_input[m_index++];
    return 0;
}
void WasaParserDriver::UNGETCHAR(int c)
{
    m_returns.push(c);
}

// Add clause to query, handling special pseudo-clauses for size/date
// etc. (mostly determined on field name).
bool WasaParserDriver::addClause(SearchData *sd, 
                                 SearchDataClauseSimple* cl)
{
    if (cl->getfield().empty()) {
        // Simple clause with empty field spec.
        // Possibly change terms found in the "autosuffs" list into "ext"
        // field queries
        if (!m_autosuffs.empty()) {
            vector<string> asfv;
            if (stringToStrings(m_autosuffs, asfv)) {
                if (find_if(asfv.begin(), asfv.end(), 
                            StringIcmpPred(cl->gettext())) != asfv.end()) {
                    cl->setfield("ext");
                    cl->addModifier(SearchDataClause::SDCM_NOSTEMMING);
                }
            }
        }
        return sd->addClause(cl);
    }

    const string& ofld = cl->getfield();
    string fld = stringtolower(ofld);

    // MIME types and categories
    if (!fld.compare("mime") || !fld.compare("format")) {
        if (cl->getexclude()) {
            m_nfiletypes.push_back(cl->gettext());
        } else {
            m_filetypes.push_back(cl->gettext());
        }
        delete cl;
        return false;
    } 

    if (!fld.compare("rclcat") || !fld.compare("type")) {
        vector<string> mtypes;
        if (m_config && m_config->getMimeCatTypes(cl->gettext(), mtypes)) {
            for (vector<string>::iterator mit = mtypes.begin();
                 mit != mtypes.end(); mit++) {
                if (cl->getexclude()) {
                    m_nfiletypes.push_back(*mit);
                } else {
                    m_filetypes.push_back(*mit);
                }
            }
        }
        delete cl;
        return false;
    }

    // Handle "date" spec
    if (!fld.compare("date")) {
        DateInterval di;
        if (!parsedateinterval(cl->gettext(), &di)) {
            LOGERR("Bad date interval format: "  << (cl->gettext()) << "\n" );
            m_reason = "Bad date interval format";
            delete cl;
            return false;
        }
        LOGDEB("addClause:: date span:  " << di.y1 << "-" << di.m1 << "-"
               << di.d1 << "/" << di.y2 << "-" << di.m2 << "-" << di.d2 << "\n");
        m_haveDates = true;
        m_dates = di;
        delete cl;
        return false;
    } 

    // Handle "size" spec
    if (!fld.compare("size")) {
        char *cp;
        size_t size = strtoll(cl->gettext().c_str(), &cp, 10);
        if (*cp != 0) {
            switch (*cp) {
            case 'k': case 'K': size *= 1000;break;
            case 'm': case 'M': size *= 1000*1000;break;
            case 'g': case 'G': size *= 1000*1000*1000;break;
            case 't': case 'T': size *= size_t(1000)*1000*1000*1000;break;
            default: 
                m_reason = string("Bad multiplier suffix: ") + *cp;
                delete cl;
                return false;
            }
        }

        SearchDataClause::Relation rel = cl->getrel();

        delete cl;

        switch (rel) {
        case SearchDataClause::REL_EQUALS:
            m_maxSize = m_minSize = size;
            break;
        case SearchDataClause::REL_LT:
        case SearchDataClause::REL_LTE:
            m_maxSize = size;
            break;
        case SearchDataClause::REL_GT: 
        case SearchDataClause::REL_GTE:
            m_minSize = size;
            break;
        default:
            m_reason = "Bad relation operator with size query. Use > < or =";
            return false;
        }
        return false;
    }

    if (!fld.compare("dir")) {
        // dir filtering special case
        SearchDataClausePath *nclause = 
            new SearchDataClausePath(cl->gettext(), cl->getexclude());
        delete cl;
        return sd->addClause(nclause);
    }

    if (cl->getTp() == SCLT_OR || cl->getTp() == SCLT_AND) {
        // If this is a normal clause and the term has commas or
        // slashes inside, take it as a list, turn the slashes/commas
        // to spaces, leave unquoted. Otherwise, this would end up as
        // a phrase query. This is a handy way to enter multiple terms
        // to be searched inside a field. We interpret ',' as AND, and
        // '/' as OR. No mixes allowed and ',' wins.
        SClType tp = SCLT_FILENAME;// impossible value
        string ns = neutchars(cl->gettext(), ",");
        if (ns.compare(cl->gettext())) {
            // had ','
            tp = SCLT_AND;
        } else {
            ns = neutchars(cl->gettext(), "/");
            if (ns.compare(cl->gettext())) {
                // had not ',' but has '/'
                tp = SCLT_OR;
            }
        }

        if (tp != SCLT_FILENAME) {
            SearchDataClauseSimple *ncl = 
                new SearchDataClauseSimple(tp, ns, ofld);
            delete cl;
            return sd->addClause(ncl);
        }
    }
    return sd->addClause(cl);
}

