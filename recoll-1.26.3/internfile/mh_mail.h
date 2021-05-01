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
#ifndef _MAIL_H_INCLUDED_
#define _MAIL_H_INCLUDED_

#include <sstream>
#include <vector>
#include <map>

#include "mimehandler.h"

namespace Binc {
class MimeDocument;
class MimePart;
}

class MHMailAttach;

/** 
 * Process a mail message (rfc822) into internal documents.
 */
class MimeHandlerMail : public RecollFilter {
public:
    MimeHandlerMail(RclConfig *cnf, const std::string &id);
    virtual ~MimeHandlerMail();
    virtual bool is_data_input_ok(DataInput input) const override {
        return (input == DOCUMENT_FILE_NAME || input == DOCUMENT_STRING);
    }
    virtual bool next_document() override;
    virtual bool skip_to_document(const std::string& ipath) override;
    virtual void clear_impl() override;

protected:
    virtual bool set_document_file_impl(const std::string& mt,
                                        const std::string& file_path) override;
    virtual bool set_document_string_impl(const std::string& mt,
                                          const std::string& data) override;

private:
    bool processMsg(Binc::MimePart *doc, int depth);
    void walkmime(Binc::MimePart* doc, int depth);
    bool processAttach();
    Binc::MimeDocument     *m_bincdoc;
    int                     m_fd;
    std::stringstream      *m_stream;

    // Current index in parts. starts at -1 for self, then index into
    // attachments
    int                     m_idx; 
    // Start of actual text (after the reprinted headers. This is for 
    // generating a semi-meaningful "abstract")
    std::string::size_type       m_startoftext; 
    std::string                  m_subject; 
    std::vector<MHMailAttach *>  m_attachments;
    // Additional headers to be processed as per config + field name translation
    std::map<std::string, std::string>      m_addProcdHdrs; 
};

class MHMailAttach {
public:
    std::string m_contentType;
    std::string m_filename;
    std::string m_charset;
    std::string m_contentTransferEncoding;
    Binc::MimePart *m_part;
};

#endif /* _MAIL_H_INCLUDED_ */
