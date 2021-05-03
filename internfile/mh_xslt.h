/* Copyright (C) 2018 J.F.Dockes
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
#ifndef _MH_XSLT_H_INCLUDED_
#define _MH_XSLT_H_INCLUDED_

#include <string>

#include "mimehandler.h"

class MimeHandlerXslt : public RecollFilter {
 public:
    MimeHandlerXslt(RclConfig *cnf, const std::string& id,
                    const std::vector<std::string>& params);
    virtual ~MimeHandlerXslt();

    virtual bool next_document() override;
    virtual void clear_impl() override;

    virtual bool is_data_input_ok(DataInput input) const override {
        return (input == DOCUMENT_FILE_NAME || input == DOCUMENT_STRING);
    }

protected:
    virtual bool set_document_file_impl(const std::string& mt, 
                                        const std::string& file_path) override;
    virtual bool set_document_string_impl(const std::string& mt,
                                          const std::string& data) override;

    class Internal;
private:
    Internal *m{nullptr};
};


#endif /* _MH_XSLT_H_INCLUDED_ */
