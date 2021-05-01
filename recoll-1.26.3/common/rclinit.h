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
#ifndef _RCLINIT_H_INCLUDED_
#define _RCLINIT_H_INCLUDED_

#include <string>

class RclConfig;
/**
 * Initialize by reading configuration, opening log file, etc.
 *
 * This must be called from the main thread before starting any others. It sets
 * up the global signal handling. other threads must call recoll_threadinit()
 * when starting.
 *
 * @param flags   misc modifiers. These are currently only used to customize
 *      the log file and verbosity.
 * @param cleanup function to call before exiting (atexit)
 * @param sigcleanup function to call on terminal signal (INT/HUP...) This
 *       should typically set a flag which tells the program (recoll,
 *       recollindex etc.. to exit as soon as possible (after closing the db,
 *       etc.). cleanup will then be called by exit().
 * @param reason in case of error: output string explaining things
 * @param argcnf Configuration directory name from the command line (overriding
 *               default and environment
 * @return the parsed configuration.
 */
enum RclInitFlags {RCLINIT_NONE = 0, RCLINIT_DAEMON = 1, RCLINIT_IDX = 2,
                   RCLINIT_PYTHON = 4};
// Kinds of termination requests, in addition to the normal signal
// values. Passed as type int to sigcleanup() when it is not invoked
// directly as a sig handler. Note that because of the existence of
// sigset_t, we are pretty sure that no signals can have a high value
enum RclSigKind {
    // System resume from sleep
    RCLSIG_RESUME = 1002};
                 
extern RclConfig *recollinit(int flags,
                             void (*cleanup)(void), void (*sigcleanup)(int),
                             std::string& reason, const std::string *argcnf = 0);

// Threads need to call this to block signals.
// The main thread handles all signals.
extern void recoll_threadinit();

// Check if main thread
extern bool recoll_ismainthread();

// Should be called while exiting asap when critical cleanup (db
// close) has been performed. Only useful for the indexer (writes to
// the db), and only actually does something on Windows.
extern void recoll_exitready();

#endif /* _RCLINIT_H_INCLUDED_ */
