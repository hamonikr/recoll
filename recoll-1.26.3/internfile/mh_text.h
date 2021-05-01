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
#ifndef _MH_TEXT_H_INCLUDED_
#define _MH_TEXT_H_INCLUDED_

#include <sys/types.h>
#include <stdint.h>

#include <string>

#include "mimehandler.h"

/**
 * Handler for text/plain files. 
 *
 * Maybe try to guess charset, or use default, then transcode to utf8
 */
class MimeHandlerText : public RecollFilter {
public:
    MimeHandlerText(RclConfig *cnf, const std::string& id) 
        : RecollFilter(cnf, id), m_paging(false), m_offs(0), m_pagesz(0) {
    }
    virtual ~MimeHandlerText() {}

    virtual bool is_data_input_ok(DataInput input) const override {
        if (input == DOCUMENT_FILE_NAME || input == DOCUMENT_STRING)
            return true;
        return false;
    }
    virtual bool next_document() override;
    virtual bool skip_to_document(const std::string& s) override;
    virtual void clear_impl() override {
        m_paging = false;
        m_text.clear(); 
        m_fn.clear();
        m_offs = 0;
        m_pagesz = 0;
        m_charsetfromxattr.clear();
    }
    
protected:
    virtual bool set_document_file_impl(const std::string& mt,
                                        const std::string &file_path) override;
    virtual bool set_document_string_impl(const std::string&,
                                          const std::string&) override;

private:
    bool   m_paging{false};
    std::string m_text;
    std::string m_fn;
    int64_t  m_offs{0}; // Offset of next read in file if we're paging
    size_t m_pagesz{0};
    std::string m_charsetfromxattr; 

    bool readnext();
};

#endif /* _MH_TEXT_H_INCLUDED_ */
