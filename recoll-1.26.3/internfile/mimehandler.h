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
#ifndef _MIMEHANDLER_H_INCLUDED_
#define _MIMEHANDLER_H_INCLUDED_
#include "autoconfig.h"

#include <stdio.h>
#include <stdint.h>
#include <string>

#include "Filter.h"
#include "cstr.h"
#include "smallut.h"

class RclConfig;

class RecollFilter : public Dijon::Filter {
public:
    RecollFilter(RclConfig *config, const std::string& id)
	: m_config(config), m_id(id) {
    }
    virtual ~RecollFilter() {}

    virtual void setConfig(RclConfig *config) {
	m_config = config;
    }

    virtual bool set_property(Properties p, const std::string &v) {
	switch (p) {
	case DJF_UDI: 
	    m_udi = v;
	    break;
	case DEFAULT_CHARSET: 
	    m_dfltInputCharset = v;
	    break;
	case OPERATING_MODE: 
	    if (!v.empty() && v[0] == 'v') 
		m_forPreview = true; 
	    else 
		m_forPreview = false;
	    break;
	}
	return true;
    }

    // We don't use this for now
    virtual bool set_document_uri(const std::string& mtype, 
				  const std::string &) {
	m_mimeType = mtype;
	return false;
    }

    virtual bool set_document_file(const std::string& mtype, 
				   const std::string &file_path) {
	m_mimeType = mtype;
	return set_document_file_impl(mtype, file_path);
    }

    virtual bool set_document_string(const std::string& mtype, 
				     const std::string &contents) {
	m_mimeType = mtype;
	return set_document_string_impl(mtype, contents);
    }
    
    virtual bool set_document_data(const std::string& mtype, 
				   const char *cp, size_t sz) 
    {
	return set_document_string(mtype, std::string(cp, sz));
    }

    virtual void set_docsize(int64_t size) {
	m_docsize = size;
    }

    virtual int64_t get_docsize() const {
	return m_docsize;
    }

    virtual bool has_documents() const {
        return m_havedoc;
    }

    // Most doc types are single-doc
    virtual bool skip_to_document(const std::string& s) {
	if (s.empty())
	    return true;
	return false;
    }

    virtual bool is_data_input_ok(DataInput input) const {
	if (input == DOCUMENT_FILE_NAME)
	    return true;
	return false;
    }

    virtual std::string get_error() const {
	return m_reason;
    }

    virtual const std::string& get_id() const {
	return m_id;
    }

    // Classes which need to do local work in clear() need
    // to implement clear_impl()
    virtual void clear() final {
        clear_impl();
	Dijon::Filter::clear();
	m_forPreview = m_havedoc = false;
	m_dfltInputCharset.clear();
	m_reason.clear();
    }
    virtual void clear_impl() {}
    
    // This only makes sense if the contents are currently txt/plain
    // It converts from keyorigcharset to UTF-8 and sets keycharset.
    bool txtdcode(const std::string& who);

    std::string metadataAsString();
protected:

    // We provide default implementation as not all handlers need both methods
    virtual bool set_document_file_impl(const std::string&,
                                        const std::string&) {
        return m_havedoc = true;
    }

    virtual bool set_document_string_impl(const std::string&,
                                          const std::string&) {
        return m_havedoc = true;
    }
    
    bool preview() {
        return m_forPreview;
    }

    RclConfig *m_config;
    bool   m_forPreview{false};
    std::string m_dfltInputCharset;
    std::string m_reason;
    bool   m_havedoc{false};
    std::string m_udi; // May be set by creator as a hint
    // m_id is and md5 of the filter definition line (from mimeconf) and
    // is used when fetching/returning filters to / from the cache.
    std::string m_id;
    int64_t m_docsize{0}; // Size of the top document
};

/**
 * Return indexing handler object for the given mime type. The returned 
 * pointer should be passed to returnMimeHandler() for recycling, after use.
 * @param mtyp input mime type, ie text/plain
 * @param cfg  the recoll config object to be used
 * @param filtertypes decide if we should restrict to types in 
 *     indexedmimetypes (if this is set at all).
 */
extern RecollFilter *getMimeHandler(const std::string &mtyp, RclConfig *cfg,
                                    bool filtertypes);

/// Free up filter for reuse (you can also delete it)
extern void returnMimeHandler(RecollFilter *);

/// Clean up cache at the end of an indexing pass. For people who use
/// the GUI to index: avoid all those filter processes forever hanging
/// off recoll.
extern void clearMimeHandlerCache();

namespace Rcl {
    class Doc;
}
/// Can this mime type be interned ?
extern bool canIntern(const std::string mimetype, RclConfig *cfg);
/// Same, getting MIME from doc
extern bool canIntern(Rcl::Doc *doc, RclConfig *cfg);
/// Can this MIME type be opened (has viewer def) ?
extern bool canOpen(Rcl::Doc *doc, RclConfig *cfg);

#endif /* _MIMEHANDLER_H_INCLUDED_ */
