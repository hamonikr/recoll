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

#include "autoconfig.h"
#include "strmatcher.h"

#include <stdio.h>
#include <sys/types.h>
#include <fnmatch.h>

#include <string>

#include "cstr.h"
#include "log.h"
#include "pathut.h"

using namespace std;

bool StrWildMatcher::match(const string& val) const
{
    LOGDEB2("StrWildMatcher::match ["<< m_sexp<< "] against [" << val << "]\n");
    int ret = fnmatch(m_sexp.c_str(), val.c_str(), FNM_NOESCAPE);
    switch (ret) {
    case 0: return true;
    case FNM_NOMATCH: return false;
    default:
	LOGINFO("StrWildMatcher::match:err: e [" << m_sexp << "] s [" << val
                << "] (" << url_encode(val) << ") ret " << ret << "\n");
	return false;
    }
}

string::size_type StrWildMatcher::baseprefixlen() const
{
    return m_sexp.find_first_of(cstr_wildSpecStChars);
}

StrRegexpMatcher::StrRegexpMatcher(const string& exp)
    : StrMatcher(exp),
      m_re(exp, SimpleRegexp::SRE_NOSUB)
{
}

bool StrRegexpMatcher::setExp(const string& exp)
{
    m_re = SimpleRegexp(exp, SimpleRegexp::SRE_NOSUB);
    return m_re.ok();
}

bool StrRegexpMatcher::match(const string& val) const
{
    if (!m_re.ok()) 
	return false;
    return m_re(val);
}

string::size_type StrRegexpMatcher::baseprefixlen() const
{
    return m_sexp.find_first_of(cstr_regSpecStChars);
}

bool StrRegexpMatcher::ok() const
{
    return m_re.ok();
}

