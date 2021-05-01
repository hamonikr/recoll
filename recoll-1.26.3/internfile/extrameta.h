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
#ifndef _REAPXATTRS_H_INCLUDED_
#define _REAPXATTRS_H_INCLUDED_

#include "autoconfig.h"

/** Extended attributes processing helper functions */

#include <map>
#include <string>

class RclConfig;
namespace Rcl {class Doc;};

/** Read external attributes, possibly ignore some or change the names
   according to the fields configuration */
extern void reapXAttrs(const RclConfig* config, const std::string& path, 
		       std::map<std::string, std::string>& xfields);

/** Turn the pre-processed extended file attributes into doc fields */
extern void docFieldsFromXattrs(
    RclConfig *cfg, const std::map<std::string, std::string>& xfields, 
    Rcl::Doc& doc);

/** Get metadata by executing commands */
extern void reapMetaCmds(RclConfig* config, const std::string& path, 
			 std::map<std::string, std::string>& xfields);

/** Turn the pre-processed ext cmd metadata into doc fields */
extern void docFieldsFromMetaCmds(
    RclConfig *cfg, const std::map<std::string, std::string>& xfields, 
    Rcl::Doc& doc);

#endif /* _REAPXATTRS_H_INCLUDED_ */
