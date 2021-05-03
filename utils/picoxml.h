/* Copyright (C) 2016 J.F.Dockes
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     (1) Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 
 *     (2) Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.  
 *     
 *     (3)The name of the author may not be used to
 *     endorse or promote products derived from this software without
 *     specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.  
**********************************************************/

#ifndef _PICOXML_H_INCLUDED_
#define _PICOXML_H_INCLUDED_

/** 
 * PicoXMLParser: a single include file parser for an XML-like, but
 * restricted language, adequate for config files, not for arbitrary
 * externally generated data.
 * 
 *  - The code depends on nothing but the C++ standard library
 *  - The input to the parser is a single c++ string. Does not deal with
 *    input in several pieces or files.
 *  - SAX mode only. You have access to the tag stack. I've always
 *    found DOM mode less usable.
 *  - Checks for proper tag nesting and not much else.
 *  - ! No CDATA
 *  - ! Attributes should really really not contain XML special chars.
 *
 * A typical input would be like the following (you can add XML
 * declarations, whitespace and newlines to taste).
 *
 * <top>top chrs1<sub attr="attrval">sub chrs</sub>top chrs2 <emptyelt /></top>
 *
 * Usage: subclass PicoXMLParser, overriding the methods in the
 *  "protected:" section (look there for more details), call the
 * constructor with your input, then call parse().
 */

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iostream>
#include <algorithm>

// Expat compat
typedef char XML_Char;

class PicoXMLParser {
public:
    PicoXMLParser(const std::string& input)
        : m_in(input), m_pos(0) {}

    virtual ~PicoXMLParser() {}

    virtual bool parse() {
        return _parse();
    }
    virtual bool Parse() {
        return _parse();
    }

    virtual std::string getReason() {
        return m_reason.str();
    }
        
protected:

    /* Methods to be overriden */

    /** 
     * Tag open handler.
     * @param tagname the tag name 
     * @param attrs a map of attribute name/value pairs
     */
    virtual void startElement(
        const std::string& /* nm */,
        const std::map<std::string, std::string>& /* attrs */) {}
    /** Expatmm compat. We don't support attributes with this at the moment */
    virtual void StartElement(const XML_Char *, const XML_Char **) {}

    /**
     * Tag close handler. 
     * You should probably have been accumulating text and stuff since
     * the tag opening.
     * @param tagname the tag name.
     */
    virtual void endElement(const std::string& /* nm */) {}
    /** Expatmm compat */
    virtual void EndElement(const XML_Char * /* nm */) {}

    /**
     * Non-tag data handler.
     * @param data the data.
     */
    virtual void characterData(const std::string& /*data*/) {}
    /** Expatmm compat */
    virtual void CharacterData(const XML_Char *, int) {}

    /** 
     * Return current tag name stack. Deprecated, use m_path.
     * This does not include the current (bottom) tag.
     * Attributes are not kept in there, you'll have to do this yourself.
     * @return a const ref to a vector of tag names.
     */
    virtual const std::vector<std::string>& tagStack() {
        return m_tagstack;
    }

    /** 
     * Current element stack, including the bottom one
     * Each entry includes the attributes and the starting character offset.
     * The stack includes the last element (the one open is called for).
     */
    class StackEl {
    public:
        StackEl(const std::string& nm) : name(nm) {}
        std::string name;
        std::string::size_type start_index;
        std::map<std::string,std::string> attributes;
        std::string data; // Derived class usage
    };
    std::vector<StackEl> m_path;

private:
    const std::string& m_in;
    std::string::size_type m_pos{0};
    std::stringstream m_reason;
    std::vector<std::string> m_tagstack;

    void _startelem(const std::string& tagname,
                    const std::map<std::string, std::string>& attrs, bool empty)
    {
        m_path.push_back(StackEl(tagname));
        StackEl& lastelt = m_path.back();
        lastelt.start_index = m_pos;
        lastelt.attributes = attrs;

        startElement(tagname, attrs);
        StartElement(tagname.c_str(), nullptr);

        m_tagstack.push_back(tagname);      // Compat
        if (empty) {
            _endelem(tagname);
        }
    }

    void _endelem(const std::string& tagname)
    {
        m_tagstack.pop_back();
        endElement(tagname);
        EndElement(tagname.c_str());
        m_path.pop_back();
    }

    bool _parse() {
        // skip initial whitespace and XML decl. On success, returns with
        // current pos on first tag '<'
        if (!skipDecl()) {
            return false;
        }
        if (nomore()) {
            // empty file
            return true;
        }
        
        for (;;) {
            // Current char is '<' and the next char is not '?'
            //std::cerr<< "m_pos "<< m_pos<<" char "<< m_in[m_pos]<<std::endl;
            // skipComment also processes 
            if (!skipComment()) {
                return false;
            }
            if (nomore()) {
                if (!m_tagstack.empty()) {
                    m_reason << "EOF hit inside open element";
                    return false;
                }
                return true;
            }
            m_pos++;
            if (nomore()) {
                m_reason << "EOF within tag";
                return false;
            }
            std::string::size_type spos = m_pos;
            int isendtag = m_in[m_pos] == '/' ? 1 : 0;

            skipStr(">");
            if (m_pos == std::string::npos || m_pos <= spos + 1) {
                m_reason << "Empty tag or EOF inside tag. pos " << spos;
                return false;
            }

            int emptyel = m_in[m_pos-2] == '/' ? 1 : 0;
            if (emptyel && isendtag) {
                m_reason << "Bad tag </xx/> at cpos " << spos;
                return false;
            }
                    
            std::string tag =
                m_in.substr(spos + isendtag,
                            m_pos - (spos + 1 + isendtag + emptyel));
            //std::cerr << "TAG NAME [" << tag << "]\n";
            trimtag(tag);
            std::map<std::string, std::string> attrs;
            if (!parseattrs(tag, attrs)) {
                return false;
            }
            if (isendtag) {
                if (m_tagstack.empty() || tag.compare(m_tagstack.back())) {
                    m_reason << "Closing not open tag " << tag <<
                        " at cpos " << m_pos;
                    return false;
                }
                _endelem(tag);
            } else {
                _startelem(tag, attrs, emptyel);
            }
            spos = m_pos;
            if (!_chardata()) {
                return false;
            }
        }
        return false;
    }

    bool _chardata() {
        std::string::size_type spos = m_pos;
        m_pos = m_in.find("<", m_pos);
        if (nomore()) {
            return true;
        }
        if (m_pos != spos) {
            std::string data{unQuote(m_in.substr(spos, m_pos - spos))};
            characterData(data);
            CharacterData(data.c_str(), data.size());
        }
        return true;
    }
    
    bool nomore(int sz = 0) const {
        return m_pos == std::string::npos || m_pos >= m_in.size() - sz;
    }
    bool skipWS(const std::string& in, std::string::size_type& pos) {
        if (pos == std::string::npos)
            return false;
        pos = in.find_first_not_of(" \t\n\r", pos);
        return pos != std::string::npos;
    }
    bool skipStr(const std::string& str) {
        if (m_pos == std::string::npos)
            return false;
        m_pos = m_in.find(str, m_pos);
        if (m_pos != std::string::npos)
            m_pos += str.size();
        return m_pos != std::string::npos;
    }
    int peek(int sz = 0) const {
        if (nomore(sz))
            return -1;
        return m_in[m_pos + 1 + sz];
    }
    void trimtag(std::string& tagname) {
        std::string::size_type trimpos = tagname.find_last_not_of(" \t\n\r");
        if (trimpos != std::string::npos) {
            tagname = tagname.substr(0, trimpos+1);
        }
    }

    bool skipDecl() {
        for (;;) {
            if (!skipWS(m_in, m_pos)) {
                m_reason << "EOF during initial ws skip";
                return true;
            }
            if (m_in[m_pos] != '<') {
                m_reason << "EOF file does not begin with decl/tag: m_pos " <<
                    m_pos << " char [" << m_in[m_pos] << "]\n";
                return false;
            }
            if (peek() == '?') {
                if (!skipStr("?>")) {
                    m_reason << "EOF while looking for end of xml decl";
                    return false;
                }
            } else {
                break;
            }
        }
        return true;
    }

    bool skipComment() {
        if (nomore()) {
            return true;
        }
        if (m_in[m_pos] != '<') {
            m_reason << "Internal error: skipComment called with wrong "
                "start: m_pos " <<
                m_pos << " char [" << m_in[m_pos] << "]\n";
            return false;
        }
        if (peek() == '!' && peek(1) == '-' && peek(2) == '-') {
            if (!skipStr("-->")) {
                m_reason << "EOF while looking for end of XML comment";
                return false;
            }
            // Process possible characters until next tag
            return _chardata();
        }
        return true;
    }
    
    bool parseattrs(std::string& tag,
                    std::map<std::string, std::string>& attrs) {
        //std::cerr << "parseattrs: [" << tag << "]\n";
        attrs.clear();
        std::string::size_type spos = tag.find_first_of(" \t\n\r");
        if (spos == std::string::npos)
            return true;
        std::string tagname = tag.substr(0, spos);
        //std::cerr << "tag name [" << tagname << "] pos " << spos << "\n";
        skipWS(tag, spos);

        for (;;) {
            //std::cerr << "top of loop [" << tag.substr(spos) << "]\n";
            std::string::size_type epos = tag.find_first_of(" \t\n\r=", spos);
            if (epos == std::string::npos) {
                m_reason << "Bad attributes syntax at cpos " << m_pos + epos;
                return false;
            }
            std::string attrnm = tag.substr(spos, epos - spos);
            if (attrnm.empty()) {
                m_reason << "Empty attribute name ?? at cpos " << m_pos + epos;
                return false;
            }
            //std::cerr << "attr name [" << attrnm << "]\n";
            skipWS(tag, epos);
            if (epos == std::string::npos || epos == tag.size() - 1 ||
                tag[epos] != '=') {
                m_reason <<"Missing equal sign or value at cpos " << m_pos+epos;
                return false;
            }
            epos++;
            skipWS(tag, epos);
            char qc{0};
            if ((tag[epos] != '"' && tag[epos] != '\'') ||
                epos == tag.size() - 1) {
                m_reason << "Missing quote or value at cpos " << m_pos+epos;
                return false;
            }
            qc = tag[epos];
            spos = epos + 1;
            epos = tag.find_first_of(qc, spos);
            if (epos == std::string::npos) {
                m_reason << "Missing closing quote at cpos " << m_pos+spos;
                return false;
            }
            attrs[attrnm] = tag.substr(spos, epos - spos);
            //std::cerr << "attr value [" << attrs[attrnm] << "]\n";
            if (epos == tag.size() - 1) {
                break;
            }
            epos++;
            skipWS(tag, epos);
            if (epos == tag.size() - 1) {
                break;
            }
            spos = epos;
        }
        tag = tagname;
        return true;
    }

    std::string unQuote(const std::string &s) {
        static const std::string e_quot{"quot"};
        static const std::string e_amp{"amp"};
        static const std::string e_apos{"apos"};
        static const std::string e_lt{"lt"};
        static const std::string e_gt{"gt"};

        std::string out;
        out.reserve(s.size());
        std::string::const_iterator it = s.begin();
        while (it != s.end()) {
            if (*it != '&') {
                out += *it;
                it++;
                continue;
            }
            if (it == s.end()) {
                // Unexpected
                break;
            }
            it++;
            std::string code;
            while (it != s.end() && *it != ';') {
                code += *it;
                it++;
            }
            if (it == s.end()) {
                // Unexpected
                break;
            }
            it++;
            if (code == e_quot) {
                out += '"';
            } else if (code == e_amp) {
                out += '&';
            } else if (code == e_apos) {
                out += '\'';
            } else if (code == e_lt) {
                out += '<';
            } else if (code == e_gt) {
                out += '>';
            }
        }
        return out;
    }
};
#endif /* _PICOXML_H_INCLUDED_ */
