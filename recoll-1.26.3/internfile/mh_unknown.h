/* Copyright (C) 2004 J.F.Dockes
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
#ifndef _MH_UNKNOWN_H_INCLUDED_
#define _MH_UNKNOWN_H_INCLUDED_

#include <string>

#include "cstr.h"
#include "mimehandler.h"

/**
 * Handler for files with no content handler: does nothing.
 *
 */
class MimeHandlerUnknown : public RecollFilter {
 public:
    MimeHandlerUnknown(RclConfig *cnf, const string& id) 
	: RecollFilter(cnf, id) {
    }
    virtual ~MimeHandlerUnknown() {}
    virtual bool is_data_input_ok(DataInput input) const {
        return true;
    }
    
    virtual bool next_document() {
	if (m_havedoc == false)
	    return false;
	m_havedoc = false; 
	m_metaData[cstr_dj_keycontent] = cstr_null;
	m_metaData[cstr_dj_keymt] = cstr_textplain;
	return true;
    }
    virtual bool is_unknown() {return true;}
};

#endif /* _MH_UNKNOWN_H_INCLUDED_ */
