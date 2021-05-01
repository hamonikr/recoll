/* Copyright (C) 2004-2018 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef _EXECMD_H_INCLUDED_
#define _EXECMD_H_INCLUDED_

#include <string>
#include <vector>
#include <stack>
#include <sys/types.h>

/**
 * Callback function object to advise of new data arrival, or just periodic
 * heartbeat if cnt is 0.
 *
 * To interrupt the command, the code using ExecCmd should either
 * raise an exception inside newData() (and catch it in doexec's caller), or
 * call ExecCmd::setKill()
 *
 */
class ExecCmdAdvise {
public:
    virtual ~ExecCmdAdvise() {}
    virtual void newData(int cnt) = 0;
};

/**
 * Callback function object to get more input data. Data has to be provided
 * into the initial input string, set it to empty to signify eof.
 */
class ExecCmdProvide {
public:
    virtual ~ExecCmdProvide() {}
    virtual void newData() = 0;
};

/**
 * Execute command possibly taking both input and output (will do
 * asynchronous io as appropriate for things to work).
 *
 * Input to the command can be provided either once in a parameter to doexec
 * or provided in chunks by setting a callback which will be called to
 * request new data. In this case, the 'input' parameter to doexec may be
 * empty (but not null)
 *
 * Output from the command is normally returned in a single string, but a
 * callback can be set to be called whenever new data arrives, in which case
 * it is permissible to consume the data and erase the string.
 *
 * Note that SIGPIPE should be ignored and SIGCLD blocked when calling doexec,
 * else things might fail randomly. (This is not done inside the class because
 * of concerns with multithreaded programs).
 *
 */
class ExecCmd {
public:
    // Use vfork instead of fork. Our vfork usage is multithread-compatible as
    // far as I can see, but just in case...
    static void useVfork(bool on);

    /**
     * Add/replace environment variable before executing command. This must
     * be called before doexec() to have an effect (possibly multiple
     * times for several variables).
     * @param envassign an environment assignment string ("name=value")
     */
    void putenv(const std::string& envassign);
    void putenv(const std::string& name, const std::string& value);

    /**
     * Try to set a limit on child process vm size. This will use
     * setrlimit() and RLIMIT_AS/VMEM if available. Parameter is in
     * units of 2**10. Must be called before starting the command, default
     * is inherit from parent.
     */
    void setrlimit_as(int mbytes);

    /**
     * Set function objects to call whenever new data is available or on
     * select timeout. The data itself is stored in the output string.
     * Must be set before calling doexec.
     */
    void setAdvise(ExecCmdAdvise *adv);
    /*
     * Set function object to call whenever new data is needed. The
     * data should be stored in the input string. Must be set before
     * calling doexec()
     */
    void setProvide(ExecCmdProvide *p);

    /**
     * Set select timeout in milliseconds. The default is 1 S.
     * This is NOT a time after which an error will occur, but the period of
     * the calls to the advise routine (which normally checks for cancellation).
     */
    void setTimeout(int mS);

    /**
     * Set destination for stderr data. The default is to let it alone (will
     * usually go to the terminal or to wherever the desktop messages go).
     * There is currently no option to put stderr data into a program variable
     * If the parameter can't be opened for writing, the command's
     * stderr will be closed.
     */
    void setStderr(const std::string& stderrFile);

    /**
     * Set kill wait timeout. This is the maximum time we'll wait for
     * the command after sending a SIGTERM, before sending a SIGKILL.

     * @param mS the maximum number of mS to wait. Note that values
     *    below 1000 mS make no sense as the program will sleep for
     *    longer time before retrying the waitpid(). Use -1 for
     *    forever (bad idea), 0 for absolutely no pity.
     */
     void setKillTimeout(int mS);

    /**
     * Execute command.
     *
     * Both input and output can be specified, and asynchronous
     * io (select-based) is used to prevent blocking. This will not
     * work if input and output need to be synchronized (ie: Q/A), but
     * works ok for filtering.
     * The function is exception-safe. In case an exception occurs in the
     * advise callback, fds and pids will be cleaned-up properly.
     *
     * @param cmd the program to execute. This must be an absolute file name
     *   or exist in the PATH.
     * @param args the argument vector (NOT including argv[0]).
     * @param input Input to send TO the command.
     * @param output Output FROM the command.
     * @return the exec output status (0 if ok), or -1
     */
    int doexec(const std::string& cmd, const std::vector<std::string>& args,
               const std::string *input = 0,
               std::string *output = 0);

    /** Same as doexec but cmd and args in one vector */
    int doexec1(const std::vector<std::string>& args,
                const std::string *input = 0,
                std::string *output = 0) {
        if (args.empty()) {
            return -1;
        }
        return doexec(args[0],
                      std::vector<std::string>(args.begin() + 1, args.end()),
                      input, output);
    }

    /*
     * The next four methods can be used when a Q/A dialog needs to be
     * performed with the command
     */
    int startExec(const std::string& cmd, const std::vector<std::string>& args,
                  bool has_input, bool has_output);
    int send(const std::string& data);
    int receive(std::string& data, int cnt = -1);

    /** Read line. Will call back periodically to check for cancellation */
    int getline(std::string& data);

    /** Read line. Timeout after timeosecs seconds */
    int getline(std::string& data, int timeosecs);

    int wait();
    /** Wait with WNOHANG set.
    @return true if process exited, false else.
    @param O: status, the wait(2) call's status value */
    bool maybereap(int *status);

    pid_t getChildPid();

    /**
     * Cancel/kill command. This can be called from another thread or
     * from the advise callback, which could also raise an exception
     * to accomplish the same thing. In the owner thread, any I/O loop
     * will exit at the next iteration, and the process will be waited for.
     */
    void setKill();

    /**
     * Get rid of current process (become ready for start). This will signal
     * politely the process to stop, wait a moment, then terminate it. This
     * is a blocking call.
     */
    void zapChild();

    /**
     * Request process termination (SIGTERM or equivalent). This returns
     * immediately
     */
    bool requestChildExit();

    enum ExFlags {EXF_NONE,
                  // Only does anything on windows. Used when starting
                  // a viewer. The default is to hide the window,
                  // because it avoids windows appearing and
                  // disappearing when executing stuff for previewing
                  EXF_SHOWWINDOW = 1,
                  // Windows only: show maximized
                  EXF_MAXIMIZED = 2,
                 };
    ExecCmd(int flags = 0);
    ~ExecCmd();

    /**
     * Utility routine: check if/where a command is found according to the
     * current PATH (or the specified one
     * @param cmd command name
     * @param exe on return, executable path name if found
     * @param path exec seach path to use instead of getenv(PATH)
     * @return true if found
     */
    static bool which(const std::string& cmd, std::string& exe, const char* path = 0);

    /**
     * Execute command and return stdout output in a string
     * @param cmd input: command and args
     * @param out output: what the command printed
     * @return true if exec status was 0
     */
    static bool backtick(const std::vector<std::string> cmd, std::string& out);

    class Internal;
private:
    Internal *m;
    /* Copyconst and assignment are private and forbidden */
    ExecCmd(const ExecCmd&) {}
    ExecCmd& operator=(const ExecCmd&) {
        return *this;
    };
};


/**
 * Rexecute self process with the same arguments.
 *
 * Note that there are some limitations:
 *  - argv[0] has to be valid: an executable name which will be found in
 *    the path when exec is called in the initial working directory. This is
 *    by no means guaranteed. The shells do this, but argv[0] could be an
 *    arbitrary string.
 *  - The initial working directory must be found and remain valid.
 *  - We don't try to do anything with fd 0,1,2. If they were changed by the
 *    program, their initial meaning won't be the same as at the moment of the
 *    initial invocation.
 *  - We don't restore the signals. Signals set to be blocked
 *    or ignored by the program will remain ignored even if this was not their
 *    initial state.
 *  - The environment is also not restored.
 *  - Others system aspects ?
 *  - Other program state: application-dependant. Any external cleanup
 *    (temp files etc.) must be performed by the application. ReExec()
 *    duplicates the atexit() function to make this easier, but the
 *    ReExec().atexit() calls must be done explicitly, this is not automatic
 *
 * In short, this is usable in reasonably controlled situations and if there
 * are no security issues involved, but this does not perform miracles.
 */
class ReExec {
public:
    ReExec() {}
    ReExec(int argc, char *argv[]);
    void init(int argc, char *argv[]);
    int atexit(void (*function)(void)) {
        m_atexitfuncs.push(function);
        return 0;
    }
    void reexec();
    const std::string& getreason() {
        return m_reason;
    }
    // Insert new args into the initial argv. idx designates the place
    // before which the new args are inserted (the default of 1
    // inserts after argv[0] which would probably be an appropriate
    // place for additional options)
    void insertArgs(const std::vector<std::string>& args, int idx = 1);
    void removeArg(const std::string& arg);
private:
    std::vector<std::string> m_argv;
    std::string m_curdir;
    int    m_cfd;
    std::string m_reason;
    std::stack<void (*)(void)> m_atexitfuncs;
};

#endif /* _EXECMD_H_INCLUDED_ */
