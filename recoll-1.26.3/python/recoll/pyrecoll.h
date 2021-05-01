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
#ifndef _PYRECOLL_H_INCLUDED_
#define _PYRECOLL_H_INCLUDED_

#include <Python.h>

#include <memory>

class RclConfig;

typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
    Rcl::Doc *doc;
    /* Each doc object has a pointer to the global config, for convenience */
    std::shared_ptr<RclConfig> rclconfig; 
} recoll_DocObject;

#define PYRECOLL_PACKAGE "recoll."

#endif // _PYRECOLL_H_INCLUDED_
