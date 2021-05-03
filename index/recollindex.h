/* Copyright (C) 2009 J.F.Dockes
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
#ifndef _recollindex_h_included_
#define _recollindex_h_included_
#include <list>
#include <string>

/** Helper methods in recollindex.cpp for initial checks/setup to index 
 * a list of files (either from the monitor or the command line) */
class RclConfig;
extern bool indexfiles(RclConfig *config, std::list<std::string> &filenames);
extern bool purgefiles(RclConfig *config, std::list<std::string> &filenames);
extern bool createAuxDbs(RclConfig *config);

/** 
 * Helper method for executing the recoll-we (new WebExtensions plugin) helper
 * script. This moves files from the browser download directory (only
 * place where the browser accepts to create them), to the web queue
 * dir). This keeps the c++ code compatible with old and new addon.
 * The script is executed before a batch pass, or from time to time in
 *  the monitor, if web processing is enabled.
 */
extern bool runWebFilesMoverScript(RclConfig *);

extern int stopindexing;

// Try to explain what went wrong...
extern void addIdxReason(std::string who, std::string reason);

class ReExec;
extern ReExec *o_reexec;
extern std::string thisprog;

#endif /* _recollindex_h_included_ */
