/* Copyright (C) 2006-2020 J.F.Dockes 
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
#ifndef _RESPOPUP_H_INCLUDED_
#define _RESPOPUP_H_INCLUDED_
#include "autoconfig.h"

class RclMain;
namespace ResultPopup {
enum Options {showExpand = 0x1, showSubs = 0x2, isMain = 0x3,
              showSaveOne = 0x4, showSaveSel = 0x8};
extern QMenu *create(QWidget *me, int opts,  
                     std::shared_ptr<DocSequence> source,
                     Rcl::Doc& doc);
extern Rcl::Doc getParent(std::shared_ptr<DocSequence> source,
                          Rcl::Doc& doc);
extern Rcl::Doc getFolder(Rcl::Doc& doc);
extern void copyFN(const Rcl::Doc &doc);
extern void copyPath(const Rcl::Doc &doc);
extern void copyURL(const Rcl::Doc &doc);
extern void copyText(Rcl::Doc &doc, RclMain *rclmain=nullptr);
};

#endif /* _RESPOPUP_H_INCLUDED_ */
