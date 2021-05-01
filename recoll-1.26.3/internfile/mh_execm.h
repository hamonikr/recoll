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
#ifndef _MH_EXECM_H_INCLUDED_
#define _MH_EXECM_H_INCLUDED_

#include "mh_exec.h"
#include "execmd.h"

/** 
 * Turn external document into internal one by executing an external filter.
 *
 * The command to execute, and its parameters, are stored in the "params" 
 * which is built in mimehandler.cpp out of data from the mimeconf file.
 *
 * This version uses persistent filters which can handle multiple requests 
 * without exiting (both multiple files and multiple documents per file), 
 * with a simple question/response protocol.
 *
 * The data is exchanged in TLV fashion, in a way that should be
 * usable in most script languages. The basic unit of data has one line 
 * with a data type and a count (both ASCII), followed by the data. A
 * 'message' is made of one or several units or tags and ends with one empty
 * line. 
 * 
 * Example from recollindex (the message begins before 'Filename' and has
 * 'Filename' and 'Ipath' tags):
 * 
Filename: 24
/my/home/mail/somefolderIpath: 2
22

<Message ends here: because of the empty line after '22'

 * 
 * Example answer, with 'Mimetype' and 'Data' tags
 * 
Mimetype: 10
text/plainData: 10
0123456789

<Message ends here because of empty line

 *        
 * This format is both extensible and reasonably easy to parse. 
 * While it's more fitted for python or perl on the script side, it
 * should even be sort of usable from the shell (e.g.: use dd to read
 * the counted data). Most alternatives would need data encoding in
 * some cases.
 *
 * Higher level dialog:
 * The C++ program is the master and sends request messages to the script. 
 * Both sides of the communication should be prepared to receive and discard 
 * unknown tags.
 * The messages normally have the following tags:
 *  - Filename: the file to process. This can be empty meaning that we 
 *      are requesting the next document in the current file.
 *  - Ipath: this will be present only if we are requesting a specific 
 *      subdocument inside a container file (typically for preview, at query 
 *      time). Absent during indexing (ipaths are generated and sent back from
 *      the script)
 *  - Mimetype: this is the mime type for the (possibly container) file. 
 *    Can be useful to filters which handle multiple types, like rclaudio.
 *      
 * The script answers with messages having the following fields:
 *   - Document: translated document data.
 *   - Ipath: ipath for the returned document. Can be used at query time to
 *     extract a specific subdocument for preview. Not present or empty for 
 *     non-container files and for the "self" document of a container.
 *   - Mimetype: mime type for the returned data.
 *     This is optional. For multi-document filters, if mimetype is
 *     not present in the answer, the ipath must be a file-name-like
 *     string which will be used to divine the mime type (this is used
 *     typically with archives like Zip or Tar). If this fails,
 *     the document will be handled as unknown type and the contents won't 
 *     be indexed. When neither ipath nor mimetype are present the default 
 *     is to attempt to treat the document as HTML.
 *   - Charset: for document types for which it makes sense, and if the filter
 *     has the information.
 *   - Eofnow: empty field: no document is returned and we're at eof.
 *   - Eofnext: empty field: file ends after the doc returned by this message.
 *   - SubdocError: no subdoc returned by this request, but file goes on.
 *   - FileError: error, stop for this file.
 */
class MimeHandlerExecMultiple : public MimeHandlerExec {
    /////////
    // Things not reset by "clear()", additionally to those in MimeHandlerExec
    ExecCmd  m_cmd;
    /////// End un-cleared stuff.

 public:
    MimeHandlerExecMultiple(RclConfig *cnf, const std::string& id) 
        : MimeHandlerExec(cnf, id) {
    }
    // No resources to clean up, the ExecCmd destructor does it.
    virtual ~MimeHandlerExecMultiple() {}

    virtual bool next_document() override;

    // skip_to and clear inherited from MimeHandlerExec

protected:
        // This is the only 2nd-level derived handler class. Use call-super.
    virtual bool set_document_file_impl(const std::string& mt,
                                        const std::string &file_path) override {
        m_filefirst = true;
        return MimeHandlerExec::set_document_file_impl(mt, file_path);
    }

private:
    bool startCmd();
    bool readDataElement(std::string& name, std::string& data);
    bool m_filefirst;
    int  m_maxmemberkb;
    MEAdv m_adv;
};

#endif /* _MH_EXECM_H_INCLUDED_ */
