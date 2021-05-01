/* Copyright (C) 2002 J.F. Dockes
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

// Wrapper classes for the socket interface

#ifdef BUILDING_RECOLL
#include "autoconfig.h"
#else
#include "config.h"
#endif

#include "netcon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#ifdef _AIX
#include <strings.h>
#endif // _AIX

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_KQUEUE
#include <sys/types.h>
#include <sys/event.h>
#include <vector>
#endif

#include <map>

#ifdef MDU_INCLUDE_LOG
#include MDU_INCLUDE_LOG
#else
#include "log.h"
#endif

using namespace std;

#ifndef SOCKLEN_T
#define SOCKLEN_T socklen_t
#endif

// Size of path buffer in sockaddr_un (AF_UNIX socket
// addr). Mysteriously it is 108 (explicit value) under linux, no
// define accessible. Let's take a little margin as it appears that
// some systems use 92. I believe we could also malloc a variable size
// struct but why bother.
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 90
#endif

// Need &one, &zero for setsockopt...
static const int one = 1;
static const int zero = 0;

#ifndef LOGSYSERR
#define LOGSYSERR(who, call, spar)                                \
    LOGERR(who << ": "  << call << "("  << spar << ") errno " <<  \
           errno << " ("  << strerror(errno) << ")\n")
#endif
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef freeZ
#define freeZ(X) if (X) {free(X);X=0;}
#endif

#define MILLIS(OLD, NEW) ( (uint64_t((NEW).tv_sec) - (OLD).tv_sec) * 1000 + \
                            ((NEW).tv_usec - (OLD).tv_usec) / 1000 )

// Static method
// Simplified interface to 'select()'. Only use one fd, for either
// reading or writing. This is only used when not using the
// selectloop() style of network i/o.
// Note that timeo == 0 does NOT mean wait forever but no wait at all.
int Netcon::select1(int fd, int timeo, int write)
{
    int ret;
    struct timeval tv;
    fd_set rd;
    tv.tv_sec = timeo;
    tv.tv_usec =  0;
    FD_ZERO(&rd);
    FD_SET(fd, &rd);
    if (write) {
        ret = select(fd + 1, 0, &rd, 0, &tv);
    } else {
        ret = select(fd + 1, &rd, 0, 0, &tv);
    }
    if (!FD_ISSET(fd, &rd)) {
        LOGDEB2("Netcon::select1: fd " << fd << " timeout\n");
    }
    return ret;
}


///////////////////////////////////////////
// SelectLoop

class SelectLoop::Internal {
public:
    Internal() {
#ifdef HAVE_KQUEUE
        if ((kq = kqueue()) == -1) {
            LOGSYSERR("Netcon::selectloop", "kqueue", "");
        }
#endif
    }

    ~Internal() {
#ifdef HAVE_KQUEUE
        if (kq >= 0)
            close(kq);
#endif
    }
    
    // Set by client callback to tell selectloop to return.
    bool selectloopDoReturn{false};
    int  selectloopReturnValue{0};
    int  placetostart{0};

    // Map of NetconP indexed by fd
    map<int, NetconP> polldata;
#ifdef HAVE_KQUEUE
    int kq{-1};
#endif
    // The last time we did the periodic thing. Initialized by setperiodic()
    struct timeval lasthdlcall;

    // The call back function and its parameter
    int (*periodichandler)(void *){0};
    void *periodicparam{0};
    // The periodic interval
    int periodicmillis{0};

    void periodictimeout(struct timeval *tv);
    void periodictimeout(struct timespec *ts);
    int maybecallperiodic();
    int setselevents(int fd, int events);
    int setselevents(NetconP& con, int events);
};

SelectLoop::SelectLoop()
{
    m = new Internal;
}

SelectLoop::~SelectLoop()
{
    delete m;
}

void SelectLoop::loopReturn(int value)
{
    m->selectloopDoReturn = true;
    m->selectloopReturnValue = value;
}
        
void SelectLoop::setperiodichandler(int (*handler)(void *), void *p, int ms)
{
    m->periodichandler = handler;
    m->periodicparam = p;
    m->periodicmillis = ms;
    if (m->periodicmillis > 0) {
        gettimeofday(&m->lasthdlcall, 0);
    }
}

// Compute the appropriate timeout so that the select call returns in
// time to call the periodic routine.
void SelectLoop::Internal::periodictimeout(struct timeval *tv)
{
    // If periodic not set, the select call times out and we loop
    // after a very long time (we'd need to pass NULL to select for an
    // infinite wait, and I'm too lazy to handle it)
    if (periodicmillis <= 0) {
        tv->tv_sec = 10000;
        tv->tv_usec = 0;
        return;
    }

    struct timeval mtv;
    gettimeofday(&mtv, 0);
    int millis = periodicmillis - MILLIS(lasthdlcall, mtv);
    
    // millis <= 0 means we should have already done the thing. *dont* set the
    // tv to 0, which means no timeout at all !
    if (millis <= 0) {
        millis = 1;
    }
    tv->tv_sec = millis / 1000;
    tv->tv_usec = (millis % 1000) * 1000;
}

void SelectLoop::Internal::periodictimeout(struct timespec *ts)
{
    struct timeval tv;
    periodictimeout(&tv);
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000;
}


// Check if it's time to call the handler. selectloop will return to
// caller if either we or the handler return 0
int SelectLoop::Internal::maybecallperiodic()
{
    if (periodicmillis <= 0) {
        return 1;
    }

    struct timeval mtv;
    gettimeofday(&mtv, 0);
    int millis = periodicmillis - MILLIS(lasthdlcall, mtv);

    if (millis <= 0) {
        lasthdlcall = mtv;
        if (periodichandler) {
            return periodichandler(periodicparam);
        } else {
            return 0;
        }
    }
    return 1;
}

#ifndef HAVE_KQUEUE

int SelectLoop::doLoop()
{
    for (;;) {
        if (m->selectloopDoReturn) {
            m->selectloopDoReturn = false;
            LOGDEB("Netcon::selectloop: returning on request\n");
            return m->selectloopReturnValue;
        }

        int nfds;
        fd_set rd, wd;
        FD_ZERO(&rd);
        FD_ZERO(&wd);

        // Walk the netcon map and set up the read and write fd_sets
        // for select()
        nfds = 0;
        for (auto& entry : m->polldata) {
            NetconP& pll = entry.second;
            int fd  = entry.first;
            LOGDEB2("Selectloop: fd " << fd << " flags 0x"  <<
                    pll->m_wantedEvents << "\n");
            if (pll->m_wantedEvents & Netcon::NETCONPOLL_READ) {
                FD_SET(fd, &rd);
                nfds = MAX(nfds, fd + 1);
            }
            if (pll->m_wantedEvents & Netcon::NETCONPOLL_WRITE) {
                FD_SET(fd, &wd);
                nfds = MAX(nfds, fd + 1);
            }
        }

        if (nfds == 0) {
            // This should never happen in a server as we should at least
            // always monitor the main listening server socket. For a
            // client, it's up to client code to avoid or process this
            // condition.

            // Just in case there would still be open fds in there
            // (with no r/w flags set). Should not be needed, but safer
            m->polldata.clear();
            LOGDEB1("Netcon::selectloop: no fds\n");
            return 0;
        }

        LOGDEB2("Netcon::selectloop: selecting, nfds = " << nfds << "\n");

        // Compute the next timeout according to what might need to be
        // done apart from waiting for data
        struct timeval tv;
        m->periodictimeout(&tv);
        // Wait for something to happen
        int ret = select(nfds, &rd, &wd, 0, &tv);
        LOGDEB2("Netcon::selectloop: nfds " << nfds <<
                " select returns " << ret << "\n");
        if (ret < 0) {
            LOGSYSERR("Netcon::selectloop", "select", "");
            return -1;
        }
        if (m->periodicmillis > 0 && m->maybecallperiodic() <= 0) {
            return 1;
        }

        // Timeout, do it again.
        if (ret == 0) {
            continue;
        }

        // Select returned > 0: at least one fd must be ready. Sweep the fd
        // table and act on the ready ones.
        // We don't start the fd sweep at 0, else some fds would be advantaged.
        // Note that we do an fd sweep, not a map sweep. This is
        // inefficient because the fd array may be very sparse. Otoh, the
        // map may change between 2 sweeps, so that we'd have to be smart
        // with the iterator. As the cost per unused fd is low (just 2 bit
        // flag tests), we keep it like this for now
        if (m->placetostart >= nfds) {
            m->placetostart = 0;
        }
        int i, fd;
        int activefds = 0;
        for (i = 0, fd = m->placetostart; i < nfds; i++, fd++) {
            if (fd >= nfds) {
                fd = 0;
            }

            int canread = FD_ISSET(fd, &rd);
            int canwrite = FD_ISSET(fd, &wd);
            bool none = !canread && !canwrite;
            LOGDEB2("Netcon::selectloop: fd " << fd << " "  << 
                    (none ? "blocked" : "can") << " "  << 
                    (canread ? "read" : "") << " "  << 
                    (canwrite ? "write" : "") << "\n");
            if (none) {
                continue;
            }

            auto it = m->polldata.find(fd);
            if (it == m->polldata.end()) {
                // This should never happen, because we only set our
                // own fds in the mask !
                LOGERR("Netcon::selectloop: fd "  << fd << " not found\n");
                continue;
            }
            activefds++;
            // Next start will be one beyond last serviced (modulo nfds)
            m->placetostart = fd + 1;

            NetconP& pll = it->second;
            if (canread && pll->cando(Netcon::NETCONPOLL_READ) <= 0) {
                pll->m_wantedEvents &= ~Netcon::NETCONPOLL_READ;
            }
            if (canwrite && pll->cando(Netcon::NETCONPOLL_WRITE) <= 0) {
                pll->m_wantedEvents &= ~Netcon::NETCONPOLL_WRITE;
            }
            if (!(pll->m_wantedEvents &
                  (Netcon::NETCONPOLL_WRITE | Netcon::NETCONPOLL_READ))) {
                LOGDEB0("Netcon::selectloop: fd " << it->first << " has 0x"
                        << it->second->m_wantedEvents << " mask, erasing\n");
                m->polldata.erase(it);
            }
        } // fd sweep

        if (ret > 0 && activefds != ret) {
            LOGERR("Select returned " << ret << " not equal to " <<
                   activefds << " active fd found\n");
            return -1;
        }
    } // forever loop
    LOGERR("SelectLoop::doLoop: got out of loop !\n");
    return -1;
}

#else // -> Using kqueue: use select()

int SelectLoop::doLoop()
{
    for (;;) {
        if (m->selectloopDoReturn) {
            m->selectloopDoReturn = false;
            LOGDEB("Netcon::selectloop: returning on request\n");
            return m->selectloopReturnValue;
        }

        // Check that we do have something to wait for.
        int nfds = 0;
        for (auto& entry : m->polldata) {
            NetconP& pll = entry.second;
            if (pll->m_wantedEvents & Netcon::NETCONPOLL_READ) {
                nfds++;
            } else if (pll->m_wantedEvents & Netcon::NETCONPOLL_WRITE) {
                nfds++;
            }
        }
        if (nfds == 0) {
            // This should never happen in a server as we should at least
            // always monitor the main listening server socket. For a
            // client, it's up to client code to avoid or process this
            // condition.

            // Just in case there would still be open fds in there
            // (with no r/w flags set). Should not be needed, but safer
            m->polldata.clear();
            LOGDEB1("Netcon::selectloop: no fds\n");
            return 0;
        }

        // Compute the next timeout according to what might need to be
        // done apart from waiting for data
        struct timespec ts;
        m->periodictimeout(&ts);
        // Wait for something to happen
        vector<struct kevent> events;
        events.resize(nfds);
        LOGDEB1("Netcon::selectloop: kevent(), nfds = " << nfds << "\n");
        int ret = kevent(m->kq, 0, 0, &events[0], events.size(), &ts);
        LOGDEB1("Netcon::selectloop: nfds " << nfds <<
                " kevent returns " << ret << "\n");
        if (ret < 0) {
            LOGSYSERR("Netcon::selectloop", "kevent", "");
            return -1;
        }
        if (m->periodicmillis > 0 && m->maybecallperiodic() <= 0) {
            return 1;
        }
        if (ret == 0) {
            // Timeout, do it again.
            continue;
        }
 
        for (int i = 0; i < ret; i++) {
            struct kevent& ev = events[i];
            if (ev.flags & EV_ERROR) {
                LOGSYSERR("Netcon::selectLoop", "kevent", "");
                LOGERR("Netcon::selectLoop: event error: " <<
                       strerror(ev.data));
                return -1;
            }
            int canread = ev.filter == EVFILT_READ;
            int canwrite = ev.filter == EVFILT_WRITE; 
            bool none = !canread && !canwrite;
            LOGDEB1("Netcon::selectloop: fd " << int(ev.ident) << " "  << 
                    (none ? "blocked" : "can") << " "  << 
                    (canread ? "read" : "") << " "  << 
                    (canwrite ? "write" : "") << "\n");
            if (none) {
                LOGERR("Kevent returned unknown filter " << ev.filter <<endl);
                continue;
            }

            auto it = m->polldata.find(int(ev.ident));
            if (it == m->polldata.end()) {
                LOGERR("Netcon::selectloop: fd " << int(ev.ident) <<
                       " not found\n");
                continue;
            }
            NetconP& pll = it->second;
            if (canread && pll->cando(Netcon::NETCONPOLL_READ) <= 0) {
                pll->setselevents(pll->getselevents() &
                                  ~Netcon::NETCONPOLL_READ);
            }
            if (canwrite && pll->cando(Netcon::NETCONPOLL_WRITE) <= 0) {
                pll->setselevents(pll->getselevents() &
                                  ~Netcon::NETCONPOLL_WRITE);
            }
            if (!(pll->getselevents() &
                  (Netcon::NETCONPOLL_WRITE | Netcon::NETCONPOLL_READ))) {
                LOGDEB0("Netcon::selectloop: fd " << it->first << " has 0x"
                        << it->second->getselevents() << " mask, erasing\n");
                m->polldata.erase(it);
            }
        } // fd sweep

    } // forever loop
    LOGERR("SelectLoop::doLoop: got out of loop !\n");
    return -1;
}

#endif // kqueue version

int SelectLoop::Internal::setselevents(int fd, int events)
{
#ifdef HAVE_KQUEUE
    auto it = polldata.find(fd);
    if (it == polldata.end()) {
        return -1;
    }
    return setselevents(it->second, events);
#endif
    return 0;
}

int SelectLoop::Internal::setselevents(NetconP& con, int events)
{
#ifdef HAVE_KQUEUE
    struct kevent event;
    if (events & Netcon::NETCONPOLL_READ) {
        EV_SET(&event, con->m_fd, EVFILT_READ, EV_ADD, 0, 0, 0);
        if(kevent(kq, &event, 1, 0, 0, 0) < 0) {
            LOGSYSERR("SelectLoop::addselcon", "kevent", "");
            return -1;
        }
    } else {
        EV_SET(&event, con->m_fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
        kevent(kq, &event, 1, 0, 0, 0);
    }
    if (events & Netcon::NETCONPOLL_WRITE) {
        EV_SET(&event, con->m_fd, EVFILT_WRITE, EV_ADD, 0, 0, 0);
        if(kevent(kq, &event, 1, 0, 0, 0) < 0) {
            LOGSYSERR("SelectLoop::addselcon", "kevent", "");
            return -1;
        }
    } else {
        EV_SET(&event, con->m_fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
        kevent(kq, &event, 1, 0, 0, 0);
    }
#endif
    return 0;
}

// Add a connection to the monitored set. This can be used to change
// the event flags too (won't add duplicates)
int SelectLoop::addselcon(NetconP con, int events)
{
    if (!con) {
        return -1;
    }
    LOGDEB1("Netcon::addselcon: fd " << con->m_fd << "\n");
    con->set_nonblock(1);
    con->m_wantedEvents = events;
    m->polldata[con->m_fd] = con;
    con->setloop(this);
    return m->setselevents(con, events);
}

// Remove a connection from the monitored set.
int SelectLoop::remselcon(NetconP con)
{
    if (!con) {
        return -1;
    }
    LOGDEB1("Netcon::remselcon: fd " << con->m_fd << "\n");
    m->setselevents(con, 0);
    auto it = m->polldata.find(con->m_fd);
    if (it == m->polldata.end()) {
        LOGDEB1("Netcon::remselcon: con not found for fd " << 
                con->m_fd << "\n");
        return -1;
    }
    con->setloop(0);
    m->polldata.erase(it);
    return 0;
}

//////////////////////////////////////////////////////////
// Base class (Netcon) methods
Netcon::~Netcon()
{
    closeconn();
    if (m_peer) {
        free(m_peer);
        m_peer = 0;
    }
}

void Netcon::closeconn()
{
    if (m_ownfd && m_fd >= 0) {
        close(m_fd);
    }
    m_fd = -1;
    m_ownfd = true;
}

char *Netcon::sterror()
{
    return strerror(errno);
}

void Netcon::setpeer(const char *hostname)
{
    if (m_peer) {
        free(m_peer);
    }
    m_peer = strdup(hostname);
}

int Netcon::settcpnodelay(int on)
{
    LOGDEB2("Netcon::settcpnodelay\n");
    if (m_fd < 0) {
        LOGERR("Netcon::settcpnodelay: connection not opened\n");
        return -1;
    }
    char *cp = on ? (char *)&one : (char *)&zero;
    if (setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, cp, sizeof(one)) < 0) {
        LOGSYSERR("NetconCli::settcpnodelay", "setsockopt", "TCP_NODELAY");
        return -1;
    }
    return 0;
}


// Set/reset non-blocking flag on fd
int Netcon::set_nonblock(int onoff)
{
    int  flags = fcntl(m_fd, F_GETFL, 0);
    if (flags != -1)   {
        int newflags = onoff ? flags | O_NONBLOCK : flags & ~O_NONBLOCK;
        if (newflags != flags)
            if (fcntl(m_fd, F_SETFL, newflags) < 0) {
                return -1;
            }
    }
    return flags;
}

int Netcon::setselevents(int events)
{
    m_wantedEvents = events;
    if (m_loop) {
        m_loop->m->setselevents(m_fd, events);
    }
    return m_wantedEvents;
}
        
/////////////////////////////////////////////////////////////////////
// Data socket (NetconData) methods

NetconData::NetconData(bool cancellable)
    : m_buf(0), m_bufbase(0), m_bufbytes(0), m_bufsize(0), m_wkfds{-1,-1}
{
    if (cancellable) {
        if (pipe(m_wkfds) < 0) {
            LOGSYSERR("NetconData::NetconData", "pipe", "");
            m_wkfds[0] = m_wkfds[1] = -1;
        }
        LOGDEB2("NetconData:: m_wkfds[0] " << m_wkfds[0] << " m_wkfds[1] " <<
               m_wkfds[1] << endl);
        for (int i = 0; i < 2; i++) {
            int flags = fcntl(m_wkfds[i], F_GETFL, 0);
            fcntl(m_wkfds[i], F_SETFL, flags | O_NONBLOCK);
        }
    }
}

NetconData::~NetconData()
{
    freeZ(m_buf);
    m_bufbase = 0;
    m_bufbytes = m_bufsize = 0;
    for (int i = 0; i < 2; i++) {
        if (m_wkfds[i] >= 0) {
            close(m_wkfds[i]);
        }
    }
}

int NetconData::send(const char *buf, int cnt, int expedited)
{
    LOGDEB2("NetconData::send: fd " << m_fd << " cnt " << cnt <<
            " expe " << expedited << "\n");
    int flag = 0;
    if (m_fd < 0) {
        LOGERR("NetconData::send: connection not opened\n");
        return -1;
    }
    if (expedited) {
        LOGDEB2("NetconData::send: expedited data, count " <<cnt << " bytes\n");
        flag = MSG_OOB;
    }
    int ret;
    // There is a bug in the uthread version of sendto() in FreeBSD at
    // least up to 2.2.7, so avoid using it when possible
    if (flag) {
        ret = ::send(m_fd, buf, cnt, flag);
    } else {
        ret = ::write(m_fd, buf, cnt);
    }

    // Note: byte count may be different from cnt if fd is non-blocking
    if (ret < 0) {
        char fdcbuf[20];
        sprintf(fdcbuf, "%d", m_fd);
        LOGSYSERR("NetconData::send", "send", fdcbuf);
    }
    return ret;
}

void NetconData::cancelReceive()
{
    if (m_wkfds[1] >= 0) {
        LOGDEB2("NetconData::cancelReceive: writing to " << m_wkfds[1] << endl);
        // We can't do a thing about the ::write return value, the
        // following nonsense is for cancelling warnings
        int ret = ::write(m_wkfds[1], "!", 1);
        ret = ret;
    }
}

// Receive at most cnt bytes (maybe less)
int NetconData::receive(char *buf, int cnt, int timeo)
{
    LOGDEB2("NetconData::receive: cnt " << cnt << " timeo "  << timeo <<
            " m_buf 0x" << m_buf << " m_bufbytes " << m_bufbytes << "\n");

    if (m_fd < 0) {
        LOGERR("NetconData::receive: connection not opened\n");
        return -1;
    }

    int fromibuf = 0;
    // Get whatever might have been left in the buffer by a previous
    // getline, except if we're called to fill the buffer of course
    if (m_buf && m_bufbytes > 0 && (buf < m_buf || buf > m_buf + m_bufsize)) {
        fromibuf = MIN(m_bufbytes, cnt);
        memcpy(buf, m_bufbase, fromibuf);
        m_bufbytes -= fromibuf;
        m_bufbase += fromibuf;
        cnt -= fromibuf;
        LOGDEB2("NetconData::receive: got " << fromibuf << " from mbuf\n");
        if (cnt <= 0) {
            return fromibuf;
        }
    }

    if (timeo > 0) {
        struct timeval tv;
        tv.tv_sec = timeo;
        tv.tv_usec =  0;
        fd_set rd;
        FD_ZERO(&rd);
        FD_SET(m_fd, &rd);
        bool cancellable = (m_wkfds[0] >= 0);
        if (cancellable) {
            LOGDEB2("NetconData::receive: cancel fd " << m_wkfds[0] << endl);
            FD_SET(m_wkfds[0], &rd);
        }
        int nfds = MAX(m_fd, m_wkfds[0]) + 1;

        int ret = select(nfds, &rd, 0, 0, &tv);
        LOGDEB2("NetconData::receive: select returned " << ret << endl);
        
        if (cancellable && FD_ISSET(m_wkfds[0], &rd)) {
            char b[100];
            // We can't do a thing about the return value, the
            // following nonsense is for cancelling warnings
            int ret = ::read(m_wkfds[0], b, 100);
            ret = ret;
            return Cancelled;
        }

        if (!FD_ISSET(m_fd, &rd)) {
            m_didtimo = 1;
            return TimeoutOrError;
        }

        if (ret < 0) {
            LOGSYSERR("NetconData::receive", "select", "");
            m_didtimo = 0;
            return TimeoutOrError;
        }
    }

    m_didtimo = 0;
    if ((cnt = read(m_fd, buf + fromibuf, cnt)) < 0) {
        LOGSYSERR("NetconData::receive", "read", m_fd);
        return -1;
    }
    LOGDEB2("NetconData::receive: normal return, fromibuf " << fromibuf <<
            " cnt "  << cnt << "\n");
    return fromibuf + cnt;
}

// Receive exactly cnt bytes (except for timeout)
int NetconData::doreceive(char *buf, int cnt, int timeo)
{
    int got, cur;
    LOGDEB2("Netcon::doreceive: cnt " << cnt << ", timeo " << timeo << "\n");
    cur = 0;
    while (cnt > cur) {
        got = receive(buf, cnt - cur, timeo);
        LOGDEB2("Netcon::doreceive: got " << got << "\n");
        if (got < 0) {
            return got;
        }
        if (got == 0) {
            return cur;
        }
        cur += got;
        buf += got;
    }
    return cur;
}

// Read data until cnt-1 characters are read or a newline is found. Add
// null char at end of buffer and return.
// As we don't know where the newline will be and it would be inefficient to
// read a character at a time, we use a buffer
// Unlike fgets, we return an integer status:
// >0: number of characters returned, not including the final 0
//  0: EOF reached, no chars transferred
// -1: error
static const int defbufsize = 200;
int NetconData::getline(char *buf, int cnt, int timeo)
{
    LOGDEB2("NetconData::getline: cnt " << cnt << ", timeo " << 
            timeo << "\n");
    if (m_buf == 0) {
        if ((m_buf = (char *)malloc(defbufsize)) == 0) {
            LOGSYSERR("NetconData::getline: Out of mem", "malloc", "");
            return -1;
        }
        m_bufsize = defbufsize;
        m_bufbase = m_buf;
        m_bufbytes = 0;
    }

    char *cp = buf;
    for (;;) {
        // Transfer from buffer. Have to take a lot of care to keep counts and
        // pointers consistant in all end cases
        int maxtransf = MIN(m_bufbytes, cnt - 1);
        int nn = maxtransf;
        LOGDEB2("Before loop, bufbytes " << m_bufbytes << ", maxtransf " <<
                maxtransf << ", nn: " << nn << "\n");
        for (nn = maxtransf; nn > 0;) {
            // This is not pretty but we want nn to be decremented for
            // each byte copied (even newline), and not become -1 if
            // we go to the end. Better ways welcome!
            nn--;
            if ((*cp++ = *m_bufbase++) == '\n') {
                break;
            }
        }
        // Update counts
        maxtransf -= nn; // Actual count transferred
        m_bufbytes -= maxtransf;
        cnt -= maxtransf;
        LOGDEB2("After transfer: actual transf " << maxtransf << " cnt " << 
                cnt << ", m_bufbytes " << m_bufbytes << "\n");

        // Finished ?
        if (cnt <= 1 || (cp > buf && cp[-1] == '\n')) {
            *cp = 0;
            return cp - buf;
        }

        // Transfer from net
        m_bufbase = m_buf;
        m_bufbytes = receive(m_buf, m_bufsize, timeo);
        if (m_bufbytes == 0) {
            // EOF
            *cp = 0;
            return cp - buf;
        }
        if (m_bufbytes < 0) {
            m_bufbytes = 0;
            *cp = 0;
            return -1;
        }
    }
}

// Called when selectloop detects that data can be read or written on
// the connection. The user callback would normally have been set
// up. If it is, call it and return. Else, perform housecleaning: read
// and discard.
int NetconData::cando(Netcon::Event reason)
{
    LOGDEB2("NetconData::cando\n");
    if (m_user) {
        return m_user->data(this, reason);
    }

    // No user callback. Clean up by ourselves
    if (reason & NETCONPOLL_READ) {
#define BS 200
        char buf[BS];
        int n;
        if ((n = receive(buf, BS)) < 0) {
            LOGSYSERR("NetconData::cando", "receive", "");
            return -1;
        }
        if (n == 0) {
            // EOF
            return 0;
        }
    }
    m_wantedEvents &= ~NETCONPOLL_WRITE;
    return 1;
}

///////////////////////////////////////////////////////////////////////
// Methods for a client connection (NetconCli)
int NetconCli::openconn(const char *host, unsigned int port, int timeo)
{
    int ret = -1;
    LOGDEB2("Netconcli::openconn: host " << host << ", port "  << port << "\n");

    closeconn();

    struct sockaddr *saddr;
    socklen_t addrsize;

    struct sockaddr_in ip_addr;
    struct sockaddr_un unix_addr;
    if (host[0] != '/') {
        memset(&ip_addr, 0, sizeof(ip_addr));
        ip_addr.sin_family = AF_INET;
        ip_addr.sin_port = htons(port);

        // Server name may be host name or IP address
        int addr;
        if ((addr = inet_addr(host)) != -1) {
            memcpy(&ip_addr.sin_addr, &addr, sizeof(addr));
        } else {
            struct hostent *hp;
            if ((hp = gethostbyname(host)) == 0) {
                LOGERR("NetconCli::openconn: gethostbyname(" << host << 
                       ") failed\n");
                return -1;
            }
            memcpy(&ip_addr.sin_addr, hp->h_addr, hp->h_length);
        }

        if ((m_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            LOGSYSERR("NetconCli::openconn", "socket", "");
            return -1;
        }
        addrsize = sizeof(ip_addr);
        saddr = (sockaddr*)&ip_addr;
    } else {
        memset(&unix_addr, 0, sizeof(unix_addr));
        unix_addr.sun_family = AF_UNIX;
        if (strlen(host) > UNIX_PATH_MAX - 1) {
            LOGERR("NetconCli::openconn: name too long: " << host << "\n");
            return -1;
        }
        strcpy(unix_addr.sun_path, host);

        if ((m_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
            LOGSYSERR("NetconCli::openconn", "socket", "");
            return -1;
        }
        addrsize = sizeof(unix_addr);
        saddr = (sockaddr*)&unix_addr;
    }
    if (timeo > 0) {
        set_nonblock(1);
    }

    if (connect(m_fd, saddr, addrsize) < 0) {
        if (timeo > 0) {
            if (errno != EINPROGRESS) {
                goto out;
            }
            if (select1(m_fd, timeo, 1) == 1) {
                goto connectok;
            }
        }
        if (m_silentconnectfailure == 0) {
            LOGSYSERR("NetconCli", "connect", "");
        }
        goto out;
    }
connectok:
    if (timeo > 0) {
        set_nonblock(0);
    }

    LOGDEB2("NetconCli::connect: setting keepalive\n");
    if (setsockopt(m_fd, SOL_SOCKET, SO_KEEPALIVE,
                   (char *)&one, sizeof(one)) < 0) {
        LOGSYSERR("NetconCli::connect", "setsockopt", "KEEPALIVE");
    }
    setpeer(host);
    LOGDEB2("NetconCli::openconn: connection opened ok\n");
    ret = 0;
out:
    if (ret < 0) {
        closeconn();
    }
    return ret;
}

// Same as previous, but get the port number from services
int NetconCli::openconn(const char *host, const char *serv, int timeo)
{
    LOGDEB2("Netconcli::openconn: host " << host << ", serv " << serv << "\n");

    if (host[0]  != '/') {
        struct servent *sp;
        if ((sp = getservbyname(serv, "tcp")) == 0) {
            LOGERR("NetconCli::openconn: getservbyname failed for " << serv 
                   << "\n");
            return -1;
        }
        // Callee expects the port number in host byte order
        return openconn(host, ntohs(sp->s_port), timeo);
    } else {
        return openconn(host, (unsigned int)0, timeo);
    }
}


int NetconCli::setconn(int fd)
{
    LOGDEB2("Netconcli::setconn: fd " << fd << "\n");
    closeconn();

    m_fd = fd;
    m_ownfd = false;
    setpeer("");

    return 0;
}

///////////////////////////////////////////////////////////////////////
// Methods for the main (listening) server connection

NetconServLis::~NetconServLis()
{
#ifdef NETCON_ACCESSCONTROL
    freeZ(okaddrs.intarray);
    freeZ(okmasks.intarray);
#endif
}

#if 0
// code for dumping a struct servent
static void dump_servent(struct servent *servp)
{
    fprintf(stderr, "Official name %s\n", servp->s_name);
    for (char **cpp = servp->s_aliases; *cpp; cpp++) {
        fprintf(stderr, "Nickname %s\n", *cpp);
    }
    fprintf(stderr, "Port %d\n", (int)ntohs((short)servp->s_port));
    fprintf(stderr, "Proto %s\n", servp->s_proto);
}
#endif

// Set up service.
int NetconServLis::openservice(const char *serv, int backlog)
{
    int port;
    struct servent  *servp;
    if (!serv) {
        LOGERR("NetconServLis::openservice: null serv??\n");
        return -1;
    }
    LOGDEB1("NetconServLis::openservice: serv " << serv << "\n");
#ifdef NETCON_ACCESSCONTROL
    if (initperms(serv) < 0) {
        return -1;
    }
#endif

    m_serv = serv;
    if (serv[0] != '/') {
        if ((servp = getservbyname(serv, "tcp")) == 0) {
            LOGERR("NetconServLis::openservice: getservbyname failed for " << 
                   serv << "\n");
            return -1;
        }
        port = (int)ntohs((short)servp->s_port);
        return openservice(port, backlog);
    } else {
        if (strlen(serv) > UNIX_PATH_MAX - 1) {
            LOGERR("NetconServLis::openservice: too long for AF_UNIX: " << 
                   serv << "\n");
            return -1;
        }
        int ret = -1;
        struct sockaddr_un  addr;
        if ((m_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
            LOGSYSERR("NetconServLis", "socket", "");
            return -1;
        }
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, serv);

        if (::bind(m_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            LOGSYSERR("NetconServLis", "bind", "");
            goto out;
        }
        if (listen(m_fd, backlog) < 0) {
            LOGSYSERR("NetconServLis", "listen", "");
            goto out;
        }

        LOGDEB1("NetconServLis::openservice: service opened ok\n");
        ret = 0;
out:
        if (ret < 0 && m_fd >= 0) {
            close(m_fd);
            m_fd = -1;
        }
        return ret;
    }
}

// Port is a natural host integer value
int NetconServLis::openservice(int port, int backlog)
{
    LOGDEB1("NetconServLis::openservice: port " << port << "\n");
#ifdef NETCON_ACCESSCONTROL
    if (initperms(port) < 0) {
        return -1;
    }
#endif
    int ret = -1;
    struct sockaddr_in  ipaddr;
    if ((m_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOGSYSERR("NetconServLis", "socket", "");
        return -1;
    }
    (void) setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one));
#ifdef SO_REUSEPORT
    (void) setsockopt(m_fd, SOL_SOCKET, SO_REUSEPORT, (char *)&one, sizeof(one));
#endif /*SO_REUSEPORT*/
    memset(&ipaddr, 0, sizeof(ipaddr));
    ipaddr.sin_family = AF_INET;
    ipaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    ipaddr.sin_port = htons((short)port);
    if (::bind(m_fd, (struct sockaddr *)&ipaddr, sizeof(ipaddr)) < 0) {
        LOGSYSERR("NetconServLis", "bind", "");
        goto out;
    }
    if (listen(m_fd, backlog) < 0) {
        LOGSYSERR("NetconServLis", "listen", "");
        goto out;
    }

    LOGDEB1("NetconServLis::openservice: service opened ok\n");
    ret = 0;
out:
    if (ret < 0 && m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
    return ret;
}

#ifdef NETCON_ACCESSCONTROL
int NetconServLis::initperms(int port)
{
    if (permsinit) {
        return 0;
    }

    char sport[30];
    sprintf(sport, "%d", port);
    return initperms(sport);
}

// Get authorized address lists from parameter file. This is disabled for now
int NetconServLis::initperms(const char *serv)
{
    if (permsinit) {
        return 0;
    }

    if (serv == 0 || *serv == 0 || strlen(serv) > 80) {
        LOGERR("NetconServLis::initperms: bad service name " << serv << "\n");
        return -1;
    }

    char keyname[100];
    sprintf(keyname, "%s_okaddrs", serv);
    if (genparams->getparam(keyname, &okaddrs, 1) < 0) {
        serv = "default";
        sprintf(keyname, "%s_okaddrs", serv);
        if (genparams->getparam(keyname, &okaddrs) < 0) {
            LOGERR("NetconServLis::initperms: no okaddrs found in config file\n");
            return -1;
        }
    }
    sprintf(keyname, "%s_okmasks", serv);
    if (genparams->getparam(keyname, &okmasks)) {
        LOGERR("NetconServLis::initperms: okmasks not found\n");
        return -1;
    }
    if (okaddrs.len == 0 || okmasks.len == 0) {
        LOGERR("NetconServLis::initperms: len 0 for okmasks or okaddrs\n");
        return -1;
    }

    permsinit = 1;
    return 0;
}
#endif /* NETCON_ACCESSCONTROL */

// Sample cando routine for server master connection: delete newly
// accepted connection. What else ?
// This is to be overriden by a derived class method for an application
// using the selectloop thing
int  NetconServLis::cando(Netcon::Event reason)
{
    delete accept();
    return 1;
}

NetconServCon *
NetconServLis::accept(int timeo)
{
    LOGDEB("NetconServLis::accept\n");

    if (timeo > 0) {
        int ret = select1(m_fd, timeo);
        if (ret == 0) {
            LOGDEB2("NetconServLis::accept timed out\n");
            m_didtimo = 1;
            return 0;
        }
        if (ret < 0) {
            LOGSYSERR("NetconServLis::accept", "select", "");
            return 0;
        }
    }
    m_didtimo = 0;

    NetconServCon *con = 0;
    int newfd = -1;
    struct sockaddr_in who;
    struct sockaddr_un uwho;
    if (m_serv.empty() || m_serv[0] != '/') {
        SOCKLEN_T clilen = (SOCKLEN_T)sizeof(who);
        if ((newfd = ::accept(m_fd, (struct sockaddr *)&who, &clilen)) < 0) {
            LOGSYSERR("NetconServCon::accept", "accept", "");
            goto out;
        }
#ifdef NETCON_ACCESSCONTROL
        if (checkperms(&who, clilen) < 0) {
            goto out;
        }
#endif
    } else {
        SOCKLEN_T clilen = (SOCKLEN_T)sizeof(uwho);
        if ((newfd = ::accept(m_fd, (struct sockaddr *)&uwho, &clilen)) < 0) {
            LOGSYSERR("NetconServCon::accept", "accept", "");
            goto out;
        }
    }

    con = new NetconServCon(newfd);
    if (con == 0) {
        LOGERR("NetconServLis::accept: new NetconServCon failed\n");
        goto out;
    }

    // Retrieve peer's host name. Errors are non fatal
    if (m_serv.empty() || m_serv[0] != '/') {
        struct hostent *hp;
        if ((hp = gethostbyaddr((char *) & (who.sin_addr),
                                sizeof(struct in_addr), AF_INET)) == 0) {
            LOGERR("NetconServLis::accept: gethostbyaddr failed for addr 0x" <<
                   who.sin_addr.s_addr << "\n");
            con->setpeer(inet_ntoa(who.sin_addr));
        } else {
            con->setpeer(hp->h_name);
        }
    } else {
        con->setpeer(m_serv.c_str());
    }

    LOGDEB2("NetconServLis::accept: setting keepalive\n");
    if (setsockopt(newfd, SOL_SOCKET, SO_KEEPALIVE,
                   (char *)&one, sizeof(one)) < 0) {
        LOGSYSERR("NetconServLis::accept", "setsockopt", "KEEPALIVE");
    }
    LOGDEB2("NetconServLis::accept: got connect from " << con->getpeer() << 
            "\n");

out:
    if (con == 0 && newfd >= 0) {
        close(newfd);
    }
    return con;
}

#ifdef NETCON_ACCESSCONTROL
int
NetconServLis::checkperms(void *cl, int)
{
    // If okmasks and addrs were not initialized, the default is allow to all
    if (okmasks.len <= 0 || okaddrs.len <= 0) {
        return 0;
    }

    struct sockaddr *addr = (struct sockaddr *)cl;
    unsigned long ip_addr;

    if (addr->sa_family != AF_INET) {
        LOGERR("NetconServLis::checkperms: connection from non-INET addr !\n");
        return -1;
    }

    ip_addr = ntohl(((struct sockaddr_in *)addr)->sin_addr.s_addr);
    LOGDEB2("checkperms: ip_addr: 0x" << ip_addr << "\n");
    for (int i = 0; i < okaddrs.len; i++) {
        unsigned int mask;
        if (i < okmasks.len) {
            mask = okmasks.intarray[i];
        } else {
            mask = okmasks.intarray[okmasks.len - 1];
        }
        LOGDEB2("checkperms: trying okaddr 0x" << okaddrs.intarray[i] <<
                ", mask 0x" << mask << "\n");
        if ((ip_addr & mask) == (okaddrs.intarray[i] & mask)) {
            return (0);
        }
    }
    LOGERR("NetconServLis::checkperm: connection from bad address 0x" <<
           ip_addr << "\n");
    return -1;
}
#endif /* NETCON_ACCESSCONTROL */
