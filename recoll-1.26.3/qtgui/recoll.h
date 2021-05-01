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
#ifndef _RECOLL_H_INCLUDED_
#define _RECOLL_H_INCLUDED_

#include <string>
#include <memory>

#include "rclconfig.h"
#include "rcldb.h"
#include "rclutil.h"

#include <QString>

// Misc declarations in need of sharing between the UI files

// Open the database if needed. We now force a close/open by default
extern bool maybeOpenDb(std::string &reason, bool force = true, 
			bool *maindberror = 0);

/** Retrieve configured stemming languages */
bool getStemLangs(vector<string>& langs);

extern RclConfig *theconfig;

extern TempFile *rememberTempFile(TempFile);
extern void forgetTempFile(string &fn);
extern void deleteAllTempFiles();

extern std::shared_ptr<Rcl::Db> rcldb;
extern int recollNeedsExit;
extern void startManual(const string& helpindex);

extern void applyStyleSheet(const QString&);

inline std::string qs2utf8s(const QString& qs)
{
    return std::string((const char *)qs.toUtf8());
}
inline std::string qs2u8s(const QString& qs)
{
    return std::string((const char *)qs.toUtf8());
}
inline QString u8s2qs(const std::string us)
{
    return QString::fromUtf8(us.c_str());
}

/** Specialized version of the qt file dialog. Can't use getOpenFile()
   etc. cause they hide dot files... */
extern QString myGetFileName(bool isdir, QString caption = QString(),
			     bool filenosave = false,
                             QString dirlocation = QString(),
                             QString dlftnm = QString()
    );

#endif /* _RECOLL_H_INCLUDED_ */
