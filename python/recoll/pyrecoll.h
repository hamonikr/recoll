/* Copyright (C) 2012-2020 J.F.Dockes
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

/* Shared definitions for pyrecoll.cpp and pyrclextract.cpp */

#include <Python.h>

#include <memory>
#include <string>

class RclConfig;
namespace Rcl {
class Doc;
class Query;
};

typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
    Rcl::Doc *doc;
    /* Each doc object has a pointer to the global config, for convenience */
    std::shared_ptr<RclConfig> rclconfig; 
} recoll_DocObject;

struct recoll_DbObject;

typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
    Rcl::Query *query;
    int         next; // Index of result to be fetched next or -1 if uninit
    int         rowcount; // Number of records returned by last execute
    std::string      *sortfield; // Need to allocate in here, main program is C.
    int         ascending;
    int         arraysize; // Default size for fetchmany
    recoll_DbObject* connection;
    bool        fetchtext;
} recoll_QueryObject;

extern PyTypeObject recoll_DocType;
extern PyTypeObject recoll_QueryType;
extern PyTypeObject rclx_ExtractorType;
extern PyTypeObject recoll_QResultStoreType;
extern PyTypeObject recoll_QRSDocType;

extern int pys2cpps(PyObject *pyval, std::string& out);

#endif // _PYRECOLL_H_INCLUDED_
