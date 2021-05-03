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
#ifndef _WEBQUEUEFETCHER_H_INCLUDED_
#define _WEBQUEUEFETCHER_H_INCLUDED_

#include "fetcher.h"

/** 
 * The WEB queue cache fetcher: 
 */
class WQDocFetcher : public DocFetcher{
    virtual bool fetch(RclConfig* cnf, const Rcl::Doc& idoc, RawDoc& out);
    virtual bool makesig(RclConfig* cnf, const Rcl::Doc& idoc,
                         std::string& sig);
    virtual ~WQDocFetcher() {}
};

#endif /* _WEBQUEUEFETCHER_H_INCLUDED_ */
