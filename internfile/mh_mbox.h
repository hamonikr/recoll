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
#ifndef _MBOX_H_INCLUDED_
#define _MBOX_H_INCLUDED_

#include <string>
#include <vector>
#include <inttypes.h>

#include "mimehandler.h"

/** 
 * Translate a mail folder file into internal documents (also works
 * for maildir files). This has to keep state while parsing a mail folder
 * file. 
 */
class MimeHandlerMbox : public RecollFilter {
public:
    MimeHandlerMbox(RclConfig *cnf, const std::string& id);
    virtual ~MimeHandlerMbox();
    virtual bool next_document() override;
    virtual bool skip_to_document(const std::string& ipath) override;
    virtual void clear_impl() override;

protected:
    virtual bool set_document_file_impl(const std::string&,
                                        const std::string&) override;

    class Internal;
private:
    Internal *m{nullptr};
};

#endif /* _MBOX_H_INCLUDED_ */
