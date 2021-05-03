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
#ifndef _MH_EXEC_H_INCLUDED_
#define _MH_EXEC_H_INCLUDED_

#include <string>
#include <vector>

#include "mimehandler.h"
#include "execmd.h"

class HandlerTimeout {};
    
/** 
 * Turn external document into internal one by executing an external command.
 *
 * The command to execute, and its parameters, are defined in the mimeconf
 * configuration file, and stored by mimehandler.cpp in the object when it is
 * built. This data is not reset by a clear() call.
 *
 * The output MIME type (typically text/plain or text/html) and output
 * character set are also defined in mimeconf. The default is text/html, utf-8
 *
 * The command will write the document text to stdout. Its only way to
 * set metadata is through "meta" tags if the output MIME is
 * text/html.
 *
 * As any RecollFilter, a MimeHandlerExec object can be reset
 * by calling clear(), and will stay initialised for the same mtype
 * (cmd, params etc.)
 */
class MimeHandlerExec : public RecollFilter {
public:
    ///////////////////////
    // Members not reset by clear(). params, cfgFilterOutputMtype and 
    // cfgFilterOutputCharset
    // define what I am.  missingHelper is a permanent error
    // (no use to try and execute over and over something that's not
    // here).

    // Parameters: this has been built by our creator, from config file 
    // data. We always add the file name at the end before actual execution
    std::vector<std::string> params;
    // Filter output type. The default for ext. filters is to output html, 
    // but some don't, in which case the type is defined in the config.
    std::string cfgFilterOutputMtype;
    // Output character set if the above type is not text/html. For
    // those filters, the output charset has to be known: ie set by a command
    // line option.
    std::string cfgFilterOutputCharset; 
    bool missingHelper{false};
    std::string whatHelper;
    // Resource management values

    // The filtermaxseconds default is set in the constructor by
    // querying the recoll.conf configuration variable. It can be
    // changed by the filter creation code in mimehandler.cpp if a
    // maxseconds parameter is set on the mimeconf line.
    int m_filtermaxseconds{900};
    int m_filtermaxmbytes{0};
    ////////////////

    MimeHandlerExec(RclConfig *cnf, const std::string& id);

    virtual void setmaxseconds(int seconds) {
        m_filtermaxseconds = seconds;
    }
    
    virtual bool next_document() override;
    virtual bool skip_to_document(const std::string& ipath) override;

    virtual void clear_impl() override {
        m_fn.erase(); 
        m_ipath.erase();
    }

protected:
    virtual bool set_document_file_impl(
        const std::string& mt, const std::string& file_path) override;

    std::string m_fn;
    std::string m_ipath;
    // md5 computation excluded by handler name: can't change after init
    bool m_handlernomd5{false};
    bool m_hnomd5init{false};
    // If md5 not excluded by handler name, allow/forbid depending on mime
    bool m_nomd5{false};
    
    // Set the character set field and possibly transcode text/plain
    // output.
    // 
    // @param mt the MIME type. A constant for mh_exec, but may depend on the
    //    subdocument entry for mh_execm.
    // @param charset Document character set. A constant (empty
    //      parameter) for mh_exec (we use the value defined in mimeconf),
    //      possibly sent from the command for mh_execm.
    virtual void handle_cs(const std::string& mt, 
                           const std::string& charset = std::string());

private:
    virtual void finaldetails();
};


// This is called periodically by ExeCmd when it is waiting for data,
// or when it does receive some. We may choose to interrupt the
// command.
class MEAdv : public ExecCmdAdvise {
public:
    MEAdv(int maxsecs = 900);
    // Reset start time to now
    void reset();
    void setmaxsecs(int maxsecs) {
        m_filtermaxseconds = maxsecs;
    }
    void newData(int n);
private:
    time_t m_start;
    int m_filtermaxseconds;
};

#endif /* _MH_EXEC_H_INCLUDED_ */
