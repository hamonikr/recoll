/* Copyright (C) 2012 J.F.Dockes
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
#ifndef _EXPANSIONDBS_H_INCLUDED_
#define _EXPANSIONDBS_H_INCLUDED_

#include <string>
#include <vector>

#include <xapian.h>

#include "unacpp.h"
#include "synfamily.h"

/** Specialization and overall creation code for the term expansion mechanism
 * defined in synfamily.h
 */
namespace Rcl {

/** A Capitals/Diacritics removal functor for using with
 * XapComputableSynFamMember. The input term transformation always uses
 * UNACFOLD. Post-expansion filtering uses either UNAC or FOLD 
 */
class SynTermTransUnac : public SynTermTrans {
public:
    /** Constructor
     * @param op defines if we remove diacritics, case or both 
     */
    SynTermTransUnac(UnacOp op)
    : m_op(op)
    {
    }
    virtual std::string name() {
        std::string nm("Unac: ");
        if (m_op & UNACOP_UNAC)
            nm  += "UNAC ";
        if (m_op & UNACOP_FOLD)
            nm  += "FOLD ";
        return nm;
    }
    virtual std::string operator()(const std::string& in)
    {
	string out;
	unacmaybefold(in, out, "UTF-8", m_op);
	LOGDEB2("SynTermTransUnac("  << (int(m_op)) << "): in ["  << (in) << "] out ["  << (out) << "]\n" );
	return out;
    }
    UnacOp m_op;
};

/** Walk the Xapian term list and create all the expansion dbs in one go. */
extern bool createExpansionDbs(Xapian::WritableDatabase& wdb, 
			       const std::vector<std::string>& langs);
}

#endif /* _EXPANSIONDBS_H_INCLUDED_ */

