/* Copyright (C) 2017-2019 J.F.Dockes
 *
 * License: GPL 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifdef BUILDING_RECOLL
#include "autoconfig.h"
#else
#include "config.h"
#endif

#include "dlib.h"

#include "pathut.h"
#include "smallut.h"

#ifdef _WIN32
#include "safewindows.h"
#elif defined(HAVE_DLOPEN)
#include <dlfcn.h>
#else
#error dlib.cpp not ported on this system
#endif

void *dlib_open(const std::string& libname, int flags)
{
#ifdef _WIN32
    return LoadLibraryA(libname.c_str());
#elif defined(HAVE_DLOPEN)
    return dlopen(libname.c_str(), RTLD_LAZY);
#else
    return nullptr;
#endif
}

void *dlib_sym(void *handle, const char *name)
{
#ifdef _WIN32
    return (void *)::GetProcAddress((HMODULE)handle, name);
#elif defined(HAVE_DLOPEN)
    return dlsym(handle, name);
#else
    return nullptr;
#endif
}

void dlib_close(void *handle)
{
#ifdef _WIN32
    ::FreeLibrary((HMODULE)handle);
#elif defined(HAVE_DLOPEN)
    dlclose(handle);
#endif
}

const char *dlib_error()
{
#ifdef _WIN32
    int error = GetLastError();
    static std::string errorstring;
    errorstring = std::string("dlopen/dlsym error: ") + lltodecstr(error);
    return errorstring.c_str();
#elif defined(HAVE_DLOPEN)
    return dlerror();
#else
    return "??? dlib not ported";
#endif
}    
