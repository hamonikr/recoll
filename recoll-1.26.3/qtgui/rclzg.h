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
#ifndef _RCLZG_H_INCLUDED_
#define _RCLZG_H_INCLUDED_

#include "rcldoc.h"

enum ZgSendType {ZGSEND_PREVIEW, ZGSEND_OPEN};

#ifndef USE_ZEITGEIST
inline void zg_send_event(ZgSendType, const Rcl::Doc&){}
#else 
extern void zg_send_event(ZgSendType tp, const Rcl::Doc& doc);
#endif

#endif // _RCLZG_H_INCLUDED_
