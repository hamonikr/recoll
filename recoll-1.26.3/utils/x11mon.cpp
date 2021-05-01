#ifndef TEST_X11MON
/* Copyright (C) 2006 J.F.Dockes */
// Poll state of X11 connectibility (to detect end of user session).
#include "autoconfig.h"
#ifndef DISABLE_X11MON
#include <stdio.h>
#include <X11/Xlib.h>
#include <signal.h>
#include <setjmp.h>

#define DODEBUG
#ifdef DODEBUG
#define DEBUG(X) fprintf X
#else
#define DEBUG(X) fprintf X
#endif

static Display *m_display;
static bool m_ok;
static jmp_buf env;

static int errorHandler(Display *, XErrorEvent*)
{
    DEBUG((stderr, "x11mon: error handler: Got X11 error\n"));
    m_ok = false;
    return 0;
}
static int ioErrorHandler(Display *)
{
    DEBUG((stderr, "x11mon: error handler: Got X11 IO error\n"));
    m_ok = false;
    m_display = 0;
    longjmp(env, 1);
}

bool x11IsAlive()
{
    // Xlib always exits on IO errors. Need a setjmp to avoid this (will jump
    // from IO error handler instead of returning).
    if (setjmp(env)) {
	DEBUG((stderr, "x11IsAlive: Long jump\n"));
	return false;
    }
    if (m_display == 0) {
	signal(SIGPIPE, SIG_IGN);
	XSetErrorHandler(errorHandler);
	XSetIOErrorHandler(ioErrorHandler);
	if ((m_display = XOpenDisplay(0)) == 0) {
	    DEBUG((stderr, "x11IsAlive: cant connect\n"));
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
#else
bool x11IsAlive() 
{
    return true;
}
#endif /* DISABLE_X11MON */

#else

// Test driver

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "x11mon.h"

int main(int argc, char **argv)
{
    for (;;) {
	if (!x11IsAlive()) {
	    fprintf(stderr, "x11IsAlive failed\n");
	} else {
	    fprintf(stderr, "x11IsAlive Ok\n");
	}
	sleep(1);
    }
}
#endif
