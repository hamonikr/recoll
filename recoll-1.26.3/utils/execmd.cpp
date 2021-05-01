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
#ifdef BUILDING_RECOLL
#include "autoconfig.h"
#else
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <string.h>

#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#ifdef HAVE_SPAWN_H
#ifndef __USE_GNU
#define __USE_GNU
#define undef__USE_GNU
#endif
#include <spawn.h>
#ifdef undef__USE_GNU
#undef __USE_GNU
#endif
#endif

#include "execmd.h"
#include "netcon.h"
#include "closefrom.h"
#include "smallut.h"
#ifdef MDU_INCLUDE_LOG
#include MDU_INCLUDE_LOG
#else
#include "log.h"
#endif

using namespace std;

extern char **environ;

class ExecCmd::Internal {
public:
    Internal() {
        sigemptyset(&m_blkcld);
    }

    static bool      o_useVfork;

    vector<string>   m_env;
    ExecCmdAdvise   *m_advise{0};
    ExecCmdProvide  *m_provide{0};
    bool             m_killRequest{false};
    int              m_timeoutMs{1000};
    int              m_killTimeoutMs{2000};
    int              m_rlimit_as_mbytes{0};
    string           m_stderrFile;
    // Pipe for data going to the command
    int              m_pipein[2]{-1,-1};
    std::shared_ptr<NetconCli> m_tocmd;
    // Pipe for data coming out
    int              m_pipeout[2]{-1,-1};
    std::shared_ptr<NetconCli> m_fromcmd;
    // Subprocess id
    pid_t            m_pid{-1};
    // Saved sigmask
    sigset_t         m_blkcld;

    // Reset internal state indicators. Any resources should have been
    // previously freed
    void reset() {
        m_killRequest = false;
        m_pipein[0] = m_pipein[1] = m_pipeout[0] = m_pipeout[1] = -1;
        m_pid = -1;
        sigemptyset(&m_blkcld);
    }
    // Child process code
    inline void dochild(const std::string& cmd, const char **argv,
                        const char **envv, bool has_input, bool has_output);
};
bool ExecCmd::Internal::o_useVfork{false};

ExecCmd::ExecCmd(int)
{
    m = new Internal();
    if (m) {
        m->reset();
    }
}
void ExecCmd::setAdvise(ExecCmdAdvise *adv)
{
    m->m_advise = adv;
}
void ExecCmd::setProvide(ExecCmdProvide *p)
{
    m->m_provide = p;
}
void ExecCmd::setTimeout(int mS)
{
    if (mS > 30) {
        m->m_timeoutMs = mS;
    }
}
void ExecCmd::setKillTimeout(int mS)
{
    m->m_killTimeoutMs = mS;
}
void ExecCmd::setStderr(const std::string& stderrFile)
{
    m->m_stderrFile = stderrFile;
}
pid_t ExecCmd::getChildPid()
{
    return m->m_pid;
}
void ExecCmd::setKill()
{
    m->m_killRequest = true;
}
void ExecCmd::zapChild()
{
    setKill();
    (void)wait();
}

bool ExecCmd::requestChildExit()
{
    if (m->m_pid > 0) {
        if (kill(m->m_pid, SIGTERM) == 0) {
            return true;
        }
    }
    return false;
}

/* From FreeBSD's which command */
static bool exec_is_there(const char *candidate)
{
    struct stat fin;

    /* XXX work around access(2) false positives for superuser */
    if (access(candidate, X_OK) == 0 &&
            stat(candidate, &fin) == 0 &&
            S_ISREG(fin.st_mode) &&
            (getuid() != 0 ||
             (fin.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0)) {
        return true;
    }
    return false;
}

bool ExecCmd::which(const string& cmd, string& exepath, const char* path)
{
    if (cmd.empty()) {
        return false;
    }
    if (cmd[0] == '/') {
        if (exec_is_there(cmd.c_str())) {
            exepath = cmd;
            return true;
        } else {
            return false;
        }
    }

    const char *pp;
    if (path) {
        pp = path;
    } else {
        pp = getenv("PATH");
    }
    if (pp == 0) {
        return false;
    }

    vector<string> pels;
    stringToTokens(pp, pels, ":");
    for (vector<string>::iterator it = pels.begin(); it != pels.end(); it++) {
        if (it->empty()) {
            *it = ".";
        }
        string candidate = (it->empty() ? string(".") : *it) + "/" + cmd;
        if (exec_is_there(candidate.c_str())) {
            exepath = candidate;
            return true;
        }
    }
    return false;
}

void ExecCmd::useVfork(bool on)
{
    // Just in case: there are competent people who believe that the
    // dynamic linker can sometimes deadlock if execve() is resolved
    // inside the vfork/exec window. Make sure it's done now. If "/" is
    // an executable file, we have a problem.
    const char *argv[] = {"/", 0};
    execve("/", (char *const *)argv, environ);
    Internal::o_useVfork  = on;
}

void ExecCmd::putenv(const string& ea)
{
    m->m_env.push_back(ea);
}

void  ExecCmd::putenv(const string& name, const string& value)
{
    string ea = name + "=" + value;
    putenv(ea);
}

static void msleep(int millis)
{
    struct timespec spec;
    spec.tv_sec = millis / 1000;
    spec.tv_nsec = (millis % 1000) * 1000000;
    nanosleep(&spec, 0);
}

/** A resource manager to ensure that execcmd cleans up if an exception is
 *  raised in the callback, or at different places on errors occurring
 *  during method executions */
class ExecCmdRsrc {
public:
    ExecCmdRsrc(ExecCmd::Internal *parent)
        : m_parent(parent), m_active(true) {
    }
    void inactivate() {
        m_active = false;
    }
    ~ExecCmdRsrc() {
        if (!m_active || !m_parent) {
            return;
        }
        LOGDEB1("~ExecCmdRsrc: working. mypid: " << getpid() << "\n");

        // Better to close the descs first in case the child is waiting in read
        if (m_parent->m_pipein[0] >= 0) {
            close(m_parent->m_pipein[0]);
        }
        if (m_parent->m_pipein[1] >= 0) {
            close(m_parent->m_pipein[1]);
        }
        if (m_parent->m_pipeout[0] >= 0) {
            close(m_parent->m_pipeout[0]);
        }
        if (m_parent->m_pipeout[1] >= 0) {
            close(m_parent->m_pipeout[1]);
        }

        // It's apparently possible for m_pid to be > 0 and getpgid to fail. In
        // this case, we have to conclude that the child process does
        // not exist. Not too sure what causes this, but the previous code
        // definitely tried to call killpg(-1,) from time to time.
        pid_t grp;
        if (m_parent->m_pid > 0 && (grp = getpgid(m_parent->m_pid)) > 0) {
            LOGDEB("ExecCmd: pid " << m_parent->m_pid << " killpg(" << grp <<
                   ", SIGTERM)\n");
            int ret = killpg(grp, SIGTERM);
            if (ret == 0) {
                int ms_slept{0};
                for (int i = 0; ; i++) {
                    int tosleep = i == 0 ? 5 : (i == 1 ? 100 : 1000);
                    msleep(tosleep);
                    ms_slept += tosleep;
                    int status;
                    (void)waitpid(m_parent->m_pid, &status, WNOHANG);
                    if (kill(m_parent->m_pid, 0) != 0) {
                        break;
                    }
                    // killtimeout == -1 -> never KILL
                    if (m_parent->m_killTimeoutMs >= 0 &&
                        ms_slept >= m_parent->m_killTimeoutMs) {
                        LOGDEB("ExecCmd: killpg(" << grp << ", SIGKILL)\n");
                        killpg(grp, SIGKILL);
                        (void)waitpid(m_parent->m_pid, &status, WNOHANG);
                        break;
                    }
                }
            } else {
                LOGERR("ExecCmd: error killing process group " << (grp) <<
                       ": " << errno << "\n");
            }
        }
        m_parent->m_tocmd.reset();
        m_parent->m_fromcmd.reset();
        pthread_sigmask(SIG_UNBLOCK, &m_parent->m_blkcld, 0);
        m_parent->reset();
    }
private:
    ExecCmd::Internal *m_parent{nullptr};
    bool    m_active{false};
};

ExecCmd::~ExecCmd()
{
    if (m) {
        ExecCmdRsrc r(m);
    }
    if (m) {
        delete m;
        m = nullptr;
    }
}

// In child process. Set up pipes and exec command.
// This must not return. _exit() on error.
// *** This can be called after a vfork, so no modification of the
//     process memory at all is allowed ***
// The LOGXX calls should not be there, but they occur only after "impossible"
// errors, which we would most definitely want to have a hint about.
//
// Note that any of the LOGXX calls could block on a mutex set in the
// father process, so that only absolutely exceptional conditions,
// should be logged, for debugging and post-mortem purposes
// If one of the calls block, the problem manifests itself by 20mn
// (filter timeout) of looping on "ExecCmd::doexec: selectloop
// returned 1', because the father is waiting on the read descriptor
inline void ExecCmd::Internal::dochild(const string& cmd, const char **argv,
                                       const char **envv,
                                       bool has_input, bool has_output)
{
    // Start our own process group
    if (setpgid(0, 0)) {
        LOGINFO("ExecCmd::DOCHILD: setpgid(0, 0) failed: errno " << errno <<
                "\n");
    }

    // Restore SIGTERM to default. Really, signal handling should be
    // specified when creating the execmd, there might be other
    // signals to reset. Resetting SIGTERM helps Recoll get rid of its
    // filter children for now though. To be fixed one day...
    // Note that resetting to SIG_DFL is a portable use of
    // signal(). No need for sigaction() here.

    // There is supposedely a risk of problems if another thread was
    // calling a signal-affecting function when vfork was called. This
    // seems acceptable though as no self-respecting thread is going
    // to mess with the global process signal disposition.

    if (signal(SIGTERM, SIG_DFL) == SIG_ERR) {
        //LOGERR("ExecCmd::DOCHILD: signal() failed, errno " << errno << "\n");
    }
    sigset_t sset;
    sigfillset(&sset);
    pthread_sigmask(SIG_UNBLOCK, &sset, 0);
    sigprocmask(SIG_UNBLOCK, &sset, 0);

#ifdef HAVE_SETRLIMIT
#if defined RLIMIT_AS || defined RLIMIT_VMEM || defined RLIMIT_DATA
    if (m_rlimit_as_mbytes > 2000 && sizeof(rlim_t) < 8) {
        // Impossible limit, don't use it
        m_rlimit_as_mbytes = 0;
    }
    if (m_rlimit_as_mbytes > 0) {
        struct rlimit ram_limit = {
            static_cast<rlim_t>(m_rlimit_as_mbytes * 1024 * 1024),
            RLIM_INFINITY
        };
        int resource;

        // RLIMIT_AS and RLIMIT_VMEM are usually synonyms when VMEM is
        // defined. RLIMIT_AS is Posix. Both don't really do what we
        // want, because they count e.g. shared lib mappings, which we
        // don't really care about.
        // RLIMIT_DATA only limits the data segment. Modern mallocs
        // use mmap and will not be bound. (Otoh if we only have this,
        // we're probably not modern).
        // So we're unsatisfied either way.
#ifdef RLIMIT_AS
        resource = RLIMIT_AS;
#elif defined RLIMIT_VMEM
        resource = RLIMIT_VMEM;
#else
        resource = RLIMIT_DATA;
#endif
        setrlimit(resource, &ram_limit);
    }
#endif
#endif // have_setrlimit

    if (has_input) {
        close(m_pipein[1]);
        if (m_pipein[0] != 0) {
            dup2(m_pipein[0], 0);
            close(m_pipein[0]);
        }
    }
    if (has_output) {
        close(m_pipeout[0]);
        if (m_pipeout[1] != 1) {
            if (dup2(m_pipeout[1], 1) < 0) {
                LOGERR("ExecCmd::DOCHILD: dup2() failed. errno " <<
                       errno << "\n");
            }
            if (close(m_pipeout[1]) < 0) {
                LOGERR("ExecCmd::DOCHILD: close() failed. errno " <<
                       errno << "\n");
            }
        }
    }
    // Do we need to redirect stderr ?
    if (!m_stderrFile.empty()) {
        int fd = open(m_stderrFile.c_str(), O_WRONLY | O_CREAT
#ifdef O_APPEND
                      | O_APPEND
#endif
                      , 0600);
        if (fd < 0) {
            close(2);
        } else {
            if (fd != 2) {
                dup2(fd, 2);
            }
            lseek(2, 0, 2);
        }
    }

    // Close all descriptors except 0,1,2
    libclf_closefrom(3);

    execve(cmd.c_str(), (char *const*)argv, (char *const*)envv);
    // Hu ho. This should never have happened as we checked the
    // existence of the executable before calling dochild... Until we
    // did this check, this was the chief cause of LOG mutex deadlock
    LOGERR("ExecCmd::DOCHILD: execve(" << cmd << ") failed. errno " <<
           errno << "\n");
    _exit(127);
}

void ExecCmd::setrlimit_as(int mbytes)
{
    m->m_rlimit_as_mbytes = mbytes;
}

int ExecCmd::startExec(const string& cmd, const vector<string>& args,
                       bool has_input, bool has_output)
{
    {
        // Debug and logging
        string command = cmd + " ";
        for (vector<string>::const_iterator it = args.begin();
                it != args.end(); it++) {
            command += "{" + *it + "} ";
        }
        LOGDEB("ExecCmd::startExec: (" << has_input << "|" << has_output <<
               ") " << command << "\n");
    }

    // The resource manager ensures resources are freed if we return early
    ExecCmdRsrc e(m);

    if (has_input && pipe(m->m_pipein) < 0) {
        LOGERR("ExecCmd::startExec: pipe(2) failed. errno " << errno << "\n" );
        return -1;
    }
    if (has_output && pipe(m->m_pipeout) < 0) {
        LOGERR("ExecCmd::startExec: pipe(2) failed. errno " << errno << "\n");
        return -1;
    }


//////////// vfork setup section
    // We do here things that we could/should do after a fork(), but
    // not a vfork(). Does no harm to do it here in both cases, except
    // that it needs cleanup (as compared to doing it just before
    // exec()).

    // Allocate arg vector (2 more for arg0 + final 0)
    typedef const char *Ccharp;
    Ccharp *argv;
    argv = (Ccharp *)malloc((args.size() + 2) * sizeof(char *));
    if (argv == 0) {
        LOGERR("ExecCmd::doexec: malloc() failed. errno " << errno << "\n");
        return -1;
    }
    // Fill up argv
    argv[0] = cmd.c_str();
    int i = 1;
    vector<string>::const_iterator it;
    for (it = args.begin(); it != args.end(); it++) {
        argv[i++] = it->c_str();
    }
    argv[i] = 0;

    // Environment. We first merge our environment and the specified
    // variables in a map<string,string>, overriding existing values,
    // then generate an appropriate char*[]
    Ccharp *envv;
    map<string, string> envmap;
    for (int i = 0; environ[i] != 0; i++) {
        string entry(environ[i]);
        string::size_type eqpos = entry.find_first_of("=");
        if (eqpos == string::npos) {
            continue;
        }
        envmap[entry.substr(0, eqpos)] = entry.substr(eqpos+1);
    }
    for (const auto& entry : m->m_env) {
        string::size_type eqpos = entry.find_first_of("=");
        if (eqpos == string::npos) {
            continue;
        }
        envmap[entry.substr(0, eqpos)] = entry.substr(eqpos+1);
    }        

    // Allocate space for the array + string storage in one block.
    unsigned int allocsize = (envmap.size() + 2) * sizeof(char *);
    for (const auto& it : envmap) {
        allocsize += it.first.size() + 1 + it.second.size() + 1;
    }
    envv = (Ccharp *)malloc(allocsize);
    if (envv == 0) {
        LOGERR("ExecCmd::doexec: malloc() failed. errno " << errno << "\n");
        free(argv);
        return -1;
    }
    // Copy to new env array
    i = 0;
    char *cp = ((char *)envv) + (envmap.size() + 2) * sizeof(char *);
    for (const auto& it : envmap) {
        strcpy(cp, (it.first + "=" + it.second).c_str());
        envv[i++] = cp;
        cp += it.first.size() + 1 + it.second.size() + 1;
    }
    envv[i++] = 0;

    // As we are going to use execve, not execvp, do the PATH thing.
    string exe;
    if (!which(cmd, exe)) {
        LOGERR("ExecCmd::startExec: " << cmd << " not found\n");
        free(argv);
        free(envv);
        return 127 << 8;
    }
//////////////////////////////// End vfork child prepare section.

#if HAVE_POSIX_SPAWN && USE_POSIX_SPAWN
    // Note that posix_spawn provides no way to setrlimit() the child.
    {
        posix_spawnattr_t attrs;
        posix_spawnattr_init(&attrs);
        short flags;
        posix_spawnattr_getflags(&attrs, &flags);

        flags |=  POSIX_SPAWN_USEVFORK;

        posix_spawnattr_setpgroup(&attrs, 0);
        flags |= POSIX_SPAWN_SETPGROUP;

        sigset_t sset;
        sigemptyset(&sset);
        posix_spawnattr_setsigmask(&attrs, &sset);
        flags |= POSIX_SPAWN_SETSIGMASK;

        sigemptyset(&sset);
        sigaddset(&sset, SIGTERM);
        posix_spawnattr_setsigdefault(&attrs, &sset);
        flags |= POSIX_SPAWN_SETSIGDEF;

        posix_spawnattr_setflags(&attrs, flags);

        posix_spawn_file_actions_t facts;
        posix_spawn_file_actions_init(&facts);

        if (has_input) {
            posix_spawn_file_actions_addclose(&facts, m->m_pipein[1]);
            if (m->m_pipein[0] != 0) {
                posix_spawn_file_actions_adddup2(&facts, m->m_pipein[0], 0);
                posix_spawn_file_actions_addclose(&facts, m->m_pipein[0]);
            }
        }
        if (has_output) {
            posix_spawn_file_actions_addclose(&facts, m->m_pipeout[0]);
            if (m->m_pipeout[1] != 1) {
                posix_spawn_file_actions_adddup2(&facts, m->m_pipeout[1], 1);
                posix_spawn_file_actions_addclose(&facts, m->m_pipeout[1]);
            }
        }

        // Do we need to redirect stderr ?
        if (!m->m_stderrFile.empty()) {
            int oflags = O_WRONLY | O_CREAT;
#ifdef O_APPEND
            oflags |= O_APPEND;
#endif
            posix_spawn_file_actions_addopen(&facts, 2, m->m_stderrFile.c_str(),
                                             oflags, 0600);
        }
        LOGDEB1("using SPAWN\n");

        // posix_spawn() does not have any standard way to ask for
        // calling closefrom(). Afaik there is a solaris extension for this,
        // but let's just add all fds
        for (int i = 3; i < libclf_maxfd(); i++) {
            posix_spawn_file_actions_addclose(&facts, i);
        }

        int ret = posix_spawn(&m->m_pid, exe.c_str(), &facts, &attrs,
                              (char *const *)argv, (char *const *)envv);
        posix_spawnattr_destroy(&attrs);
        posix_spawn_file_actions_destroy(&facts);
        if (ret) {
            LOGERR("ExecCmd::startExec: posix_spawn() failed. errno " << ret <<
                   "\n");
            return -1;
        }
    }

#else
    if (Internal::o_useVfork) {
        LOGDEB1("using VFORK\n");
        m->m_pid = vfork();
    } else {
        LOGDEB1("using FORK\n");
        m->m_pid = fork();
    }
    if (m->m_pid < 0) {
        LOGERR("ExecCmd::startExec: fork(2) failed. errno " << errno << "\n");
        return -1;
    }
    if (m->m_pid == 0) {
        // e.inactivate() is not needed. As we do not return, the call
        // stack won't be unwound and destructors of local objects
        // won't be called.
        m->dochild(exe, argv, envv, has_input, has_output);
        // dochild does not return. Just in case...
        _exit(1);
    }
#endif

    // Father process

////////////////////
    // Vfork cleanup section
    free(argv);
    free(envv);
///////////////////

    // Set the process group for the child. This is also done in the
    // child process see wikipedia(Process_group)
    if (setpgid(m->m_pid, m->m_pid)) {
        // This can fail with EACCES if the son has already done execve
        // (linux at least)
        LOGDEB2("ExecCmd: father setpgid(son)(" << m->m_pid << "," <<
                m->m_pid << ") errno " << errno << " (ok)\n");
    }

    sigemptyset(&m->m_blkcld);
    sigaddset(&m->m_blkcld, SIGCHLD);
    pthread_sigmask(SIG_BLOCK, &m->m_blkcld, 0);

    if (has_input) {
        close(m->m_pipein[0]);
        m->m_pipein[0] = -1;
        NetconCli *iclicon = new NetconCli();
        iclicon->setconn(m->m_pipein[1]);
        m->m_tocmd = std::shared_ptr<NetconCli>(iclicon);
    }
    if (has_output) {
        close(m->m_pipeout[1]);
        m->m_pipeout[1] = -1;
        NetconCli *oclicon = new NetconCli();
        oclicon->setconn(m->m_pipeout[0]);
        m->m_fromcmd = std::shared_ptr<NetconCli>(oclicon);
    }

    /* Don't want to undo what we just did ! */
    e.inactivate();

    return 0;
}

// Netcon callback. Send data to the command's input
class ExecWriter : public NetconWorker {
public:
    ExecWriter(const string *input, ExecCmdProvide *provide,
               ExecCmd::Internal *parent)
        : m_cmd(parent), m_input(input), m_cnt(0), m_provide(provide) {
    }
    void shutdown() {
        close(m_cmd->m_pipein[1]);
        m_cmd->m_pipein[1] = -1;
        m_cmd->m_tocmd.reset();
    }
    virtual int data(NetconData *con, Netcon::Event reason) {
        if (!m_input) {
            return -1;
        }
        LOGDEB1("ExecWriter: input m_cnt " << m_cnt << " input length " <<
                m_input->length() << "\n");
        if (m_cnt >= m_input->length()) {
            // Fd ready for more but we got none. Try to get data, else
            // shutdown;
            if (!m_provide) {
                shutdown();
                return 0;
            }
            m_provide->newData();
            if (m_input->empty()) {
                shutdown();
                return 0;
            } else {
                // Ready with new buffer, reset use count
                m_cnt = 0;
            }
            LOGDEB2("ExecWriter: provide m_cnt " << m_cnt <<
                    " input length " << m_input->length() << "\n");
        }
        int ret = con->send(m_input->c_str() + m_cnt,
                            m_input->length() - m_cnt);
        LOGDEB2("ExecWriter: wrote " << (ret) << " to command\n");
        if (ret <= 0) {
            LOGERR("ExecWriter: data: can't write\n");
            return -1;
        }
        m_cnt += ret;
        return ret;
    }
private:
    ExecCmd::Internal *m_cmd;
    const string   *m_input;
    unsigned int    m_cnt; // Current offset inside m_input
    ExecCmdProvide *m_provide;
};

// Netcon callback. Get data from the command output.
class ExecReader : public NetconWorker {
public:
    ExecReader(string *output, ExecCmdAdvise *advise)
        : m_output(output), m_advise(advise) {
    }
    virtual int data(NetconData *con, Netcon::Event reason) {
        char buf[8192];
        int n = con->receive(buf, 8192);
        LOGDEB1("ExecReader: got " << (n) << " from command\n");
        if (n < 0) {
            LOGERR("ExecCmd::doexec: receive failed. errno " << errno << "\n");
        } else if (n > 0) {
            m_output->append(buf, n);
            if (m_advise) {
                m_advise->newData(n);
            }
        } // else n == 0, just return
        return n;
    }
private:
    string        *m_output;
    ExecCmdAdvise *m_advise;
};


int ExecCmd::doexec(const string& cmd, const vector<string>& args,
                    const string *input, string *output)
{
    int status = startExec(cmd, args, input != 0, output != 0);
    if (status) {
        return status;
    }

    // Cleanup in case we return early
    ExecCmdRsrc e(m);
    SelectLoop myloop;
    int ret = 0;
    if (input || output) {
        // Setup output
        if (output) {
            NetconCli *oclicon = m->m_fromcmd.get();
            if (!oclicon) {
                LOGERR("ExecCmd::doexec: no connection from command\n");
                return -1;
            }
            oclicon->setcallback(std::shared_ptr<NetconWorker>
                                 (new ExecReader(output, m->m_advise)));
            myloop.addselcon(m->m_fromcmd, Netcon::NETCONPOLL_READ);
            // Give up ownership
            m->m_fromcmd.reset();
        }
        // Setup input
        if (input) {
            NetconCli *iclicon = m->m_tocmd.get();
            if (!iclicon) {
                LOGERR("ExecCmd::doexec: no connection from command\n");
                return -1;
            }
            iclicon->setcallback(std::shared_ptr<NetconWorker>
                                 (new ExecWriter(input, m->m_provide, m)));
            myloop.addselcon(m->m_tocmd, Netcon::NETCONPOLL_WRITE);
            // Give up ownership
            m->m_tocmd.reset();
        }

        // Do the actual reading/writing/waiting
        myloop.setperiodichandler(0, 0, m->m_timeoutMs);
        while ((ret = myloop.doLoop()) > 0) {
            LOGDEB("ExecCmd::doexec: selectloop returned " << (ret) << "\n");
            if (m->m_advise) {
                m->m_advise->newData(0);
            }
            if (m->m_killRequest) {
                LOGINFO("ExecCmd::doexec: cancel request\n");
                break;
            }
        }
        LOGDEB0("ExecCmd::doexec: selectloop returned " << (ret) << "\n");
        // Check for interrupt request: we won't want to waitpid()
        if (m->m_advise) {
            m->m_advise->newData(0);
        }

        // The netcons don't take ownership of the fds: we have to close them
        // (have to do it before wait, this may be the signal the child is
        // waiting for exiting).
        if (input) {
            close(m->m_pipein[1]);
            m->m_pipein[1] = -1;
        }
        if (output) {
            close(m->m_pipeout[0]);
            m->m_pipeout[0] = -1;
        }
    }

    // Normal return: deactivate cleaner, wait() will do the cleanup
    e.inactivate();

    int ret1 = ExecCmd::wait();
    if (ret) {
        return -1;
    }
    return ret1;
}

int ExecCmd::send(const string& data)
{
    NetconCli *con = m->m_tocmd.get();
    if (con == 0) {
        LOGERR("ExecCmd::send: outpipe is closed\n");
        return -1;
    }
    unsigned int nwritten = 0;
    while (nwritten < data.length()) {
        if (m->m_killRequest) {
            break;
        }
        int n = con->send(data.c_str() + nwritten, data.length() - nwritten);
        if (n < 0) {
            LOGERR("ExecCmd::send: send failed\n");
            return -1;
        }
        nwritten += n;
    }
    return nwritten;
}

int ExecCmd::receive(string& data, int cnt)
{
    NetconCli *con = m->m_fromcmd.get();
    if (con == 0) {
        LOGERR("ExecCmd::receive: inpipe is closed\n");
        return -1;
    }
    const int BS = 4096;
    char buf[BS];
    int ntot = 0;
    do {
        int toread = cnt > 0 ? MIN(cnt - ntot, BS) : BS;
        int n = con->receive(buf, toread);
        if (n < 0) {
            LOGERR("ExecCmd::receive: error\n");
            return -1;
        } else if (n > 0) {
            ntot += n;
            data.append(buf, n);
        } else {
            LOGDEB("ExecCmd::receive: got 0\n");
            break;
        }
    } while (cnt > 0 && ntot < cnt);
    return ntot;
}

int ExecCmd::getline(string& data)
{
    NetconCli *con = m->m_fromcmd.get();
    if (con == 0) {
        LOGERR("ExecCmd::receive: inpipe is closed\n");
        return -1;
    }
    const int BS = 1024;
    char buf[BS];
    int timeosecs = m->m_timeoutMs / 1000;
    if (timeosecs == 0) {
        timeosecs = 1;
    }

    // Note that we only go once through here, except in case of
    // timeout, which is why I think that the goto is more expressive
    // than a loop
again:
    int n = con->getline(buf, BS, timeosecs);
    if (n < 0) {
        if (con->timedout()) {
            LOGDEB0("ExecCmd::getline: select timeout, report and retry\n");
            if (m->m_advise) {
                m->m_advise->newData(0);
            }
            goto again;
        }
        LOGERR("ExecCmd::getline: error\n");
    } else if (n > 0) {
        data.append(buf, n);
    } else {
        LOGDEB("ExecCmd::getline: got 0\n");
    }
    return n;
}

class GetlineWatchdog : public ExecCmdAdvise {
public:
    GetlineWatchdog(int secs) : m_secs(secs), tstart(time(0)) {}
    void newData(int cnt) {
        if (time(0) - tstart >= m_secs) {
            throw std::runtime_error("getline timeout");
        }
    }
    int m_secs;
    time_t tstart;
};

int ExecCmd::getline(string& data, int timeosecs)
{
    GetlineWatchdog gwd(timeosecs);
    setAdvise(&gwd);
    try {
        return getline(data);
    } catch (...) {
        return -1;
    }
}


// Wait for command status and clean up all resources.
// We would like to avoid blocking here too, but there is no simple
// way to do this. The 2 possible approaches would be to:
//  - Use signals (alarm), waitpid() is interruptible. but signals and
//    threads... This would need a specialized thread, inter-thread comms etc.
//  - Use an intermediary process when starting the command. The
//    process forks a timer process, and the real command, then calls
//    a blocking waitpid on all at the end, and is guaranteed to get
//    at least the timer process status, thus yielding a select()
//    equivalent. This is bad too, because the timeout is on the whole
//    exec, not just the wait
// Just calling waitpid() with WNOHANG with a sleep() between tries
// does not work: the first waitpid() usually comes too early and
// reaps nothing, resulting in almost always one sleep() or more.
//
// So no timeout here. This has not been a problem in practise inside recoll.
// In case of need, using a semi-busy loop with short sleeps
// increasing from a few mS might work without creating too much
// overhead.
int ExecCmd::wait()
{
    ExecCmdRsrc e(m);
    int status = -1;
    if (!m->m_killRequest && m->m_pid > 0) {
        if (waitpid(m->m_pid, &status, 0) < 0) {
            LOGERR("ExecCmd::waitpid: returned -1 errno " << errno << "\n");
            status = -1;
        }
        LOGDEB("ExecCmd::wait: got status 0x" << (status) << "\n");
        m->m_pid = -1;
    }
    // Let the ExecCmdRsrc cleanup, it will do the killing/waiting if needed
    return status;
}

bool ExecCmd::maybereap(int *status)
{
    ExecCmdRsrc e(m);
    *status = -1;

    if (m->m_pid <= 0) {
        // Already waited for ??
        return true;
    }

    pid_t pid = waitpid(m->m_pid, status, WNOHANG);
    if (pid < 0) {
        LOGERR("ExecCmd::maybereap: returned -1 errno " << errno << "\n");
        m->m_pid = -1;
        return true;
    } else if (pid == 0) {
        LOGDEB1("ExecCmd::maybereap: not exited yet\n");
        e.inactivate();
        return false;
    } else {
        LOGDEB("ExecCmd::maybereap: got status 0x" << (status) << "\n");
        m->m_pid = -1;
        return true;
    }
}

// Static
bool ExecCmd::backtick(const vector<string> cmd, string& out)
{
    if (cmd.empty()) {
        LOGERR("ExecCmd::backtick: empty command\n");
        return false;
    }
    vector<string>::const_iterator it = cmd.begin();
    it++;
    vector<string> args(it, cmd.end());
    ExecCmd mexec;
    int status = mexec.doexec(*cmd.begin(), args, 0, &out);
    return status == 0;
}

/// ReExec class methods ///////////////////////////////////////////////////
ReExec::ReExec(int argc, char *args[])
{
    init(argc, args);
}

void ReExec::init(int argc, char *args[])
{
    for (int i = 0; i < argc; i++) {
        m_argv.push_back(args[i]);
    }
    m_cfd = open(".", 0);
    char *cd = getcwd(0, 0);
    if (cd) {
        m_curdir = cd;
    }
    free(cd);
}

void ReExec::insertArgs(const vector<string>& args, int idx)
{
    vector<string>::iterator it, cit;
    unsigned int cmpoffset = (unsigned int) - 1;

    if (idx == -1 || string::size_type(idx) >= m_argv.size()) {
        it = m_argv.end();
        if (m_argv.size() >= args.size()) {
            cmpoffset = m_argv.size() - args.size();
        }
    } else {
        it = m_argv.begin() + idx;
        if (idx + args.size() <= m_argv.size()) {
            cmpoffset = idx;
        }
    }

    // Check that the option is not already there
    if (cmpoffset != (unsigned int) - 1) {
        bool allsame = true;
        for (unsigned int i = 0; i < args.size(); i++) {
            if (m_argv[cmpoffset + i] != args[i]) {
                allsame = false;
                break;
            }
        }
        if (allsame) {
            return;
        }
    }

    m_argv.insert(it, args.begin(), args.end());
}

void ReExec::removeArg(const string& arg)
{
    for (vector<string>::iterator it = m_argv.begin();
            it != m_argv.end(); it++) {
        if (*it == arg) {
            it = m_argv.erase(it);
        }
    }
}

// Reexecute myself, as close as possible to the initial exec
void ReExec::reexec()
{

#if 0
    char *cwd;
    cwd = getcwd(0, 0);
    FILE *fp = stdout; //fopen("/tmp/exectrace", "w");
    if (fp) {
        fprintf(fp, "reexec: pwd: [%s] args: ", cwd ? cwd : "getcwd failed");
        for (vector<string>::const_iterator it = m_argv.begin();
                it != m_argv.end(); it++) {
            fprintf(fp, "[%s] ", it->c_str());
        }
        fprintf(fp, "\n");
    }
#endif

    // Execute the atexit funcs
    while (!m_atexitfuncs.empty()) {
        (m_atexitfuncs.top())();
        m_atexitfuncs.pop();
    }

    // Try to get back to the initial working directory
    if (m_cfd < 0 || fchdir(m_cfd) < 0) {
        LOGINFO("ReExec::reexec: fchdir failed, trying chdir\n");
        if (!m_curdir.empty() && chdir(m_curdir.c_str())) {
            LOGERR("ReExec::reexec: chdir failed\n");
        }
    }

    // Close all descriptors except 0,1,2
    libclf_closefrom(3);

    // Allocate arg vector (1 more for final 0)
    typedef const char *Ccharp;
    Ccharp *argv;
    argv = (Ccharp *)malloc((m_argv.size() + 1) * sizeof(char *));
    if (argv == 0) {
        LOGERR("ExecCmd::doexec: malloc() failed. errno " << errno << "\n");
        return;
    }

    // Fill up argv
    int i = 0;
    vector<string>::const_iterator it;
    for (it = m_argv.begin(); it != m_argv.end(); it++) {
        argv[i++] = it->c_str();
    }
    argv[i] = 0;
    execvp(m_argv[0].c_str(), (char *const*)argv);
}
