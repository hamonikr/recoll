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

// Poll state of X11 connectibility (to detect end of user session).

#include "autoconfig.h"

#ifndef DISABLE_X11MON

#include "x11mon.h"

#include <stdio.h>
#include <X11/Xlib.h>
#include <signal.h>
#include <setjmp.h>

#include "log.h"

static Display *m_display;
static bool m_ok;
static jmp_buf env;

static int errorHandler(Display *, XErrorEvent*)
{
    LOGERR("x11mon: error handler: Got X11 error\n");
    m_ok = false;
    return 0;
}

static int ioErrorHandler(Display *)
{
    LOGERR("x11mon: error handler: Got X11 IO error\n");
    m_ok = false;
    m_display = 0;
    longjmp(env, 1);
}

bool x11IsAlive()
{
    // Xlib always exits on IO errors. Need a setjmp to avoid this (will jump
    // from IO error handler instead of returning).
    if (setjmp(env)) {
        LOGDEB("x11IsAlive: got long jump: X11 error\n");
        return false;
    }
    if (m_display == 0) {
        signal(SIGPIPE, SIG_IGN);
        XSetErrorHandler(errorHandler);
        XSetIOErrorHandler(ioErrorHandler);
        if ((m_display = XOpenDisplay(0)) == 0) {
            LOGERR("x11IsAlive: cant connect\n");
            m_ok = false;
            return false;
        }
    }
    m_ok = true;
    bool sync= XSynchronize(m_display, true);
    XNoOp(m_display);
    XSynchronize(m_display, sync);
    return m_ok;
}

#else // DISABLE_X11MON->

bool x11IsAlive() 
{
    return true;
}

#endif /* DISABLE_X11MON */
