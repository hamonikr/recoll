/* Copyright (C) 2012 J.F.Dockes
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
#ifndef _STRMATCHER_H_INCLUDED_
#define _STRMATCHER_H_INCLUDED_

#include <string>
#include "smallut.h"

// Encapsulating simple wildcard/regexp string matching.

// Matcher class. Interface to either wildcard or regexp yes/no matcher
class StrMatcher {
public:
    StrMatcher(const std::string& exp) 
        : m_sexp(exp) {}
    virtual ~StrMatcher() {};
    virtual bool match(const std::string &val) const = 0;
    virtual std::string::size_type baseprefixlen() const = 0;
    virtual bool setExp(const std::string& newexp) {
	m_sexp = newexp;
	return true;
    }
    virtual bool ok() const {
	return true;
    }
    virtual const std::string& exp() const {
	return m_sexp;
    }
    virtual StrMatcher *clone() const = 0;
    const std::string& getreason() const {
	return m_reason;
    }
protected:
    std::string m_sexp;
    std::string m_reason;
};

class StrWildMatcher : public StrMatcher {
public:
    StrWildMatcher(const std::string& exp)
        : StrMatcher(exp) {}
    virtual ~StrWildMatcher() {}
    virtual bool match(const std::string& val) const override;
    virtual std::string::size_type baseprefixlen() const override;
    virtual StrWildMatcher *clone() const override {
	return new StrWildMatcher(m_sexp);
    }
};

class StrRegexpMatcher : public StrMatcher {
public:
    StrRegexpMatcher(const std::string& exp);
    virtual bool setExp(const std::string& newexp) override;
    virtual ~StrRegexpMatcher() {};
    virtual bool match(const std::string& val) const override;
    virtual std::string::size_type baseprefixlen() const override;
    virtual bool ok() const override;
    virtual StrRegexpMatcher *clone() const override {
	return new StrRegexpMatcher(m_sexp);
    }
private:
    SimpleRegexp m_re;
};

#endif /* _STRMATCHER_H_INCLUDED_ */
