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
#ifndef _PLAINTORICH_H_INCLUDED_
#define _PLAINTORICH_H_INCLUDED_

#include <string>
#include <list>

#include "hldata.h"
#include "cstr.h"

/** 
 * A class for highlighting search results. Overridable methods allow
 * for different styles. We can handle plain text or html input. In the latter
 * case, we may fail to highligt term groups if they are mixed with HTML 
 * tags (ex: firstterm <b>2ndterm</b>).
 */
class PlainToRich {
public:
    virtual ~PlainToRich() {}

    void set_inputhtml(bool v) {
        m_inputhtml = v;
    }
    void set_activatelinks(bool v) {
        m_activatelinks = v;
    }

    /**
     * Transform plain text for highlighting search terms, ie in the
     * preview window or result list entries.
     *
     * The actual tags used for highlighting and anchoring are
     * determined by deriving from this class which handles the searching for
     * terms and groups, but there is an assumption that the output will be
     * html-like: we escape characters like < or &
     * 
     * Finding the search terms is relatively complicated because of
     * phrase/near searches, which need group highlights. As a matter
     * of simplification, we handle "phrase" as "near", not filtering
     * on word order.
     *
     * @param in    raw text out of internfile.
     * @param out   rich text output, divided in chunks (to help our caller
     *   avoid inserting half tags into textedit which doesnt like it)
     * @param in hdata terms and groups to be highlighted. These are
     *   lowercase and unaccented.
     * @param chunksize max size of chunks in output list
     */
    virtual bool plaintorich(const std::string &in, std::list<std::string> &out,
                             const HighlightData& hdata,
                             int chunksize = 50000
        );

    /* Overridable output methods for headers, highlighting and marking tags */

    virtual std::string header() {
        return cstr_null;
    }

    /** Return match prefix (e.g.: <div class="match">). 
        @param groupidx the index into hdata.groups */
    virtual std::string startMatch(unsigned int) {
        return cstr_null;
    }

    /** Return data for end of match area (e.g.: </div>). */
    virtual std::string endMatch() {
        return cstr_null;
    }

    virtual std::string startChunk() {
        return cstr_null;
    }

protected:
    bool m_inputhtml{false};
    // Use <br> to break plain text lines (else caller has used a <pre> tag)
    bool m_eolbr{false}; 
    const HighlightData *m_hdata{0};
    bool m_activatelinks{false};
};

#endif /* _PLAINTORICH_H_INCLUDED_ */
