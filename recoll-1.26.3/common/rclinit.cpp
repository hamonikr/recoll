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
#include "autoconfig.h"

#include <stdio.h>
#ifdef _WIN32
#include "safewindows.h"
#endif
#include <signal.h>
#include <locale.h>
#include <cstdlib>
#if !defined(PUTENV_ARG_CONST)
#include <string.h>
#endif

#include <thread>

#include "log.h"
#include "rclconfig.h"
#include "rclinit.h"
#include "pathut.h"
#include "rclutil.h"
#include "unac.h"
#include "smallut.h"
#include "execmd.h"
#include "textsplit.h"
#include "rcldb.h"

std::thread::id mainthread_id;

// Signal etc. processing. We want to be able to properly close the
// index if we are currently writing to it.
//
// This is active if the sigcleanup parameter to recollinit is set,
// which only recollindex does. We arrange for the handler to be
// called when process termination is requested either by the system
// or a user keyboard intr.
//
// The recollindex handler just sets a global termination flag (plus
// the cancelcheck thing), which are tested in all timeout loops
// etc. When the flag is seen, the main thread processing returns, and
// recollindex calls exit().
//
// The other parameter, to recollinit(), cleanup, is set as an
// atexit() routine, it does the job of actually signalling the
// workers to stop and tidy up. It's automagically called by exit().

#ifndef _WIN32
static void siglogreopen(int)
{
    if (recoll_ismainthread())
        Logger::getTheLog("")->reopen("");
}

// We would like to block SIGCHLD globally, but we can't because
// QT uses it. Have to block it inside execmd.cpp
static const int catchedSigs[] = {SIGINT, SIGQUIT, SIGTERM, SIGUSR1, SIGUSR2};
void initAsyncSigs(void (*sigcleanup)(int))
{
    // We ignore SIGPIPE always. All pieces of code which can write to a pipe
    // must check write() return values.
    signal(SIGPIPE, SIG_IGN);

    // Install app signal handler
    if (sigcleanup) {
	struct sigaction action;
	action.sa_handler = sigcleanup;
	action.sa_flags = 0;
	sigemptyset(&action.sa_mask);
	for (unsigned int i = 0; i < sizeof(catchedSigs) / sizeof(int); i++)
	    if (signal(catchedSigs[i], SIG_IGN) != SIG_IGN) {
		if (sigaction(catchedSigs[i], &action, 0) < 0) {
		    perror("Sigaction failed");
		}
	    }
    }

    // Install log rotate sig handler
    {
	struct sigaction action;
	action.sa_handler = siglogreopen;
	action.sa_flags = 0;
	sigemptyset(&action.sa_mask);
	if (signal(SIGHUP, SIG_IGN) != SIG_IGN) {
	    if (sigaction(SIGHUP, &action, 0) < 0) {
		perror("Sigaction failed");
	    }
	}
    }
}
void recoll_exitready()
{
}

#else // _WIN32 ->

// Windows signals etc.
//
// ^C can be caught by the signal() emulation, but not ^Break
// apparently, which is why we use the native approach too
//
// When a keyboard interrupt occurs, windows creates a thread inside
// the process and calls the handler. The process exits when the
// handler returns or after at most 10S
//
// This should also work, with different signals (CTRL_LOGOFF_EVENT,
// CTRL_SHUTDOWN_EVENT) when the user exits or the system shuts down).
//
// Unfortunately, this is not the end of the story. It seems that in
// recent Windows version "some kinds" of apps will not reliably
// receive the signals. "Some kind" is variably defined, for example a
// simple test program works when built with vs 2015, but not
// mingw. See the following discussion thread for tentative
// explanations, it seems that importing or not from user32.dll is the
// determining factor.
// https://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/abf09824-4e4c-4f2c-ae1e-5981f06c9c6e/windows-7-console-application-has-no-way-of-trapping-logoffshutdown-event?forum=windowscompatibility
// In any case, it appears that the only reliable way to be advised of
// system shutdown or user exit is to create an "invisible window" and
// process window messages, which we now do.

static void (*l_sigcleanup)(int);
static HANDLE eWorkFinished = INVALID_HANDLE_VALUE;

static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    LOGDEB("CtrlHandler\n" );
    if (l_sigcleanup == 0)
        return FALSE;

    switch(fdwCtrlType) { 
    case CTRL_C_EVENT: 
    case CTRL_CLOSE_EVENT: 
    case CTRL_BREAK_EVENT: 
    case CTRL_LOGOFF_EVENT: 
    case CTRL_SHUTDOWN_EVENT:
    {
        l_sigcleanup(SIGINT);
        LOGDEB0("CtrlHandler: waiting for exit ready\n" );
	DWORD res = WaitForSingleObject(eWorkFinished, INFINITE);
	if (res != WAIT_OBJECT_0) {
            LOGERR("CtrlHandler: exit ack wait failed\n" );
	}
        LOGDEB0("CtrlHandler: got exit ready event, exiting\n" );
        return TRUE;
    }
    default: 
        return FALSE; 
    } 
} 

LRESULT CALLBACK MainWndProc(HWND hwnd , UINT msg , WPARAM wParam,
                             LPARAM lParam)
{
    switch (msg) {
    case WM_POWERBROADCAST:
    {
        LOGDEB("MainWndProc: got powerbroadcast message\n");
        // This gets specific processing because we want to check the
        // state of topdirs on resuming indexing (in case a mounted
        // volume went away).
        if (l_sigcleanup) {
            if (wParam == PBT_APMRESUMEAUTOMATIC ||
                wParam == PBT_APMRESUMESUSPEND) {
                l_sigcleanup(RCLSIG_RESUME);
            }
        }
    }
    break;
    case WM_QUERYENDSESSION:
    case WM_ENDSESSION:
    case WM_DESTROY:
    case WM_CLOSE:
    {
        if (l_sigcleanup) {
            l_sigcleanup(SIGINT);
            LOGDEB("MainWndProc: got end message, waiting for work finished\n");
            DWORD res = WaitForSingleObject(eWorkFinished, INFINITE);
            if (res != WAIT_OBJECT_0) {
                LOGERR("MainWndProc: exit ack wait failed\n" );
            }
        }
        LOGDEB("MainWindowProc: got exit ready event, exiting\n" );
    }
    break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return TRUE;
}

bool CreateInvisibleWindow()
{
    HWND hwnd;
    WNDCLASS wc = {0,0,0,0,0,0,0,0,0,0};

    wc.lpfnWndProc = (WNDPROC)MainWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = LoadIcon(GetModuleHandle(NULL), L"TestWClass");
    wc.lpszClassName = L"TestWClass";
    RegisterClass(&wc);

    hwnd =
        CreateWindowEx(0, L"TestWClass", L"TestWClass", WS_OVERLAPPEDWINDOW,
                       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                       CW_USEDEFAULT, (HWND) NULL, (HMENU) NULL,
                       GetModuleHandle(NULL), (LPVOID) NULL);
    if (!hwnd) {
        return FALSE;
    }
    return TRUE;
}

DWORD WINAPI RunInvisibleWindowThread(LPVOID lpParam)
{
    MSG msg;
    CreateInvisibleWindow();
    while (GetMessage(&msg, (HWND) NULL , 0 , 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

static const int catchedSigs[] = {SIGINT, SIGTERM};
void initAsyncSigs(void (*sigcleanup)(int))
{
    DWORD tid;
    // Install app signal handler
    if (sigcleanup) {
        l_sigcleanup = sigcleanup;
	for (unsigned int i = 0; i < sizeof(catchedSigs) / sizeof(int); i++) {
	    if (signal(catchedSigs[i], SIG_IGN) != SIG_IGN) {
		signal(catchedSigs[i], sigcleanup);
	    }
        }
    }

	CreateThread(NULL, 0, RunInvisibleWindowThread, NULL, 0, &tid);
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
    eWorkFinished = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (eWorkFinished == INVALID_HANDLE_VALUE) {
        LOGERR("initAsyncSigs: error creating exitready event\n" );
    }
}
void recoll_exitready()
{
    LOGDEB("recoll_exitready()\n" );
    if (!SetEvent(eWorkFinished)) {
        LOGERR("recoll_exitready: SetEvent failed\n" );
    }
}

#endif

RclConfig *recollinit(int flags, 
		      void (*cleanup)(void), void (*sigcleanup)(int), 
		      string &reason, const string *argcnf)
{
    if (cleanup)
	atexit(cleanup);

#ifdef MACPORTS
    // Apple keeps changing the way to set the environment (PATH) for
    // a desktop app (started by launchd or whatever). Life is too
    // short.
    const char *cp = getenv("PATH");
    if (!cp) //??
        cp = "";
    string PATH(cp);
    PATH = string("/opt/local/bin/") + ":" + PATH;
    setenv("PATH", PATH.c_str(), 1);
#endif
    
    // Make sure the locale is set. This is only for converting file names 
    // to utf8 for indexing.
    setlocale(LC_CTYPE, "");

    // Initially log to stderr, at error level.
    Logger::getTheLog("")->setLogLevel(Logger::LLERR);

    initAsyncSigs(sigcleanup);
    
    RclConfig *config = new RclConfig(argcnf);
    if (!config || !config->ok()) {
	reason = "Configuration could not be built:\n";
	if (config)
	    reason += config->getReason();
	else
	    reason += "Out of memory ?";
	return 0;
    }

    TextSplit::staticConfInit(config);
    
    // Retrieve the log file name and level. Daemon and batch indexing
    // processes may use specific values, else fall back on common
    // ones.
    string logfilename, loglevel;
    if (flags & RCLINIT_DAEMON) {
	config->getConfParam(string("daemlogfilename"), logfilename);
	config->getConfParam(string("daemloglevel"), loglevel);
    }
    if (flags & RCLINIT_IDX) {
        if (logfilename.empty()) {
            config->getConfParam(string("idxlogfilename"), logfilename);
        }
        if (loglevel.empty()) {
            config->getConfParam(string("idxloglevel"), loglevel);
        }
    }
    if (flags & RCLINIT_PYTHON) {
        if (logfilename.empty()) {
            config->getConfParam(string("pylogfilename"), logfilename);
        }
        if (loglevel.empty()) {
            config->getConfParam(string("pyloglevel"), loglevel);
        }
    }

    if (logfilename.empty())
	config->getConfParam(string("logfilename"), logfilename);
    if (loglevel.empty())
	config->getConfParam(string("loglevel"), loglevel);

    // Initialize logging
    if (!logfilename.empty()) {
	logfilename = path_tildexpand(logfilename);
	// If not an absolute path or stderr, compute relative to config dir.
	if (!path_isabsolute(logfilename) &&
            logfilename.compare("stderr")) {
	    logfilename = path_cat(config->getConfDir(), logfilename);
	}
        Logger::getTheLog("")->reopen(logfilename);
    }
    if (!loglevel.empty()) {
	int lev = atoi(loglevel.c_str());
        Logger::getTheLog("")->setLogLevel(Logger::LogLevel(lev));
    }
    LOGINF(Rcl::version_string() << " [" << config->getConfDir() << "]\n");

    // Make sure the locale charset is initialized (so that multiple
    // threads don't try to do it at once).
    config->getDefCharset();

    mainthread_id = std::this_thread::get_id();

    // Init smallut and pathut static values
    pathut_init_mt();
    smallut_init_mt();
    rclutil_init_mt();
    
    // Init execmd.h static PATH and PATHELT splitting
    {string bogus;
        ExecCmd::which("nosuchcmd", bogus);
    }
    
    // Init Unac translation exceptions
    string unacex;
    if (config->getConfParam("unac_except_trans", unacex) && !unacex.empty()) 
	unac_set_except_translations(unacex.c_str());

#ifndef IDX_THREADS
    ExecCmd::useVfork(true);
#else
    // Keep threads init behind log init, but make sure it's done before
    // we do the vfork choice ! The latter is not used any more actually, 
    // we always use vfork except if forbidden by config.
    if ((flags & RCLINIT_IDX)) {
        config->initThrConf();
    }

    bool novfork;
    config->getConfParam("novfork", &novfork);
    if (novfork) {
	LOGDEB0("rclinit: will use fork() for starting commands\n" );
        ExecCmd::useVfork(false);
    } else {
	LOGDEB0("rclinit: will use vfork() for starting commands\n" );
	ExecCmd::useVfork(true);
    }
#endif

    int flushmb;
    if (config->getConfParam("idxflushmb", &flushmb) && flushmb > 0) {
	LOGDEB1("rclinit: idxflushmb=" << flushmb <<
                ", set XAPIAN_FLUSH_THRESHOLD to 10E6\n");
	static const char *cp = "XAPIAN_FLUSH_THRESHOLD=1000000";
#ifdef PUTENV_ARG_CONST
	::putenv(cp);
#else
	::putenv(strdup(cp));
#endif
    }

    return config;
}

// Signals are handled by the main thread. All others should call this
// routine to block possible signals
void recoll_threadinit()
{
#ifndef _WIN32
    sigset_t sset;
    sigemptyset(&sset);

    for (unsigned int i = 0; i < sizeof(catchedSigs) / sizeof(int); i++)
	sigaddset(&sset, catchedSigs[i]);
    sigaddset(&sset, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &sset, 0);
#else
    // Not sure that this is needed at all or correct under windows.
    for (unsigned int i = 0; i < sizeof(catchedSigs) / sizeof(int); i++) {
        if (signal(catchedSigs[i], SIG_IGN) != SIG_IGN) {
            signal(catchedSigs[i], SIG_IGN);
        }
    }
#endif
}

bool recoll_ismainthread()
{
    return std::this_thread::get_id() == mainthread_id;
}


