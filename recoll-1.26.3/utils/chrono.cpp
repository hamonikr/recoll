/* Copyright (C) 2014 J.F.Dockes
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
#ifndef TEST_CHRONO
#include "autoconfig.h"

#include <time.h>
#include <iostream>

#include "chrono.h"

using namespace std;

#ifndef CLOCK_REALTIME
typedef int clockid_t;
#define CLOCK_REALTIME 1
#endif


#define SECONDS(TS1, TS2)                             \
    (float((TS2).tv_sec - (TS1).tv_sec) +             \
     float((TS2).tv_nsec - (TS1).tv_nsec) * 1e-9)

#define MILLIS(TS1, TS2)                                        \
    ((long long)((TS2).tv_sec - (TS1).tv_sec) * 1000LL +        \
     ((TS2).tv_nsec - (TS1).tv_nsec) / 1000000)

#define MICROS(TS1, TS2)                                          \
    ((long long)((TS2).tv_sec - (TS1).tv_sec) * 1000000LL +       \
     ((TS2).tv_nsec - (TS1).tv_nsec) / 1000)

#define NANOS(TS1, TS2)                                           \
    ((long long)((TS2).tv_sec - (TS1).tv_sec) * 1000000000LL +    \
     ((TS2).tv_nsec - (TS1).tv_nsec))



// Using clock_gettime() is nice because it gives us ns res and it helps with
// computing threads work times, but it's also a pita because it forces linking
// with -lrt. So keep it non-default, special development only.
// #define USE_CLOCK_GETTIME

// And wont' bother with gettime() on these.
#if defined(__APPLE__) || defined(_WIN32)
#undef USE_CLOCK_GETTIME
#endif

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdint.h> // portable: uint64_t   MSVC: __int64 

// MSVC defines this in winsock2.h!?
typedef struct timeval {
    long tv_sec;
    long tv_usec;
} timeval;

int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
    // Note: some broken versions only have 8 trailing zero's, the
    // correct epoch has 9 trailing zero's
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
    tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
    return 0;
}
#else // -> Not _WIN32
#ifndef USE_CLOCK_GETTIME
// Using gettimeofday then, needs struct timeval
#include <sys/time.h>
#endif
#endif



// We use gettimeofday instead of clock_gettime for now and get only
// uS resolution, because clock_gettime is more configuration trouble
// than it's worth
static void gettime(int
#ifdef USE_CLOCK_GETTIME
                    clk_id
#endif
                    , Chrono::TimeSpec *ts)
{
#ifdef USE_CLOCK_GETTIME
    struct timespec mts;
    clock_gettime(clk_id, &mts);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
#else
    struct timeval tv;
    gettimeofday(&tv, 0);
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000;
#endif
}
///// End system interface

// Note: this not protected against multithread access and not
// reentrant, but this is mostly debug code, and it won't crash, just
// show bad results. Also the frozen thing is not used that much
Chrono::TimeSpec Chrono::o_now;

void Chrono::refnow()
{
    gettime(CLOCK_REALTIME, &o_now);
}

Chrono::Chrono()
{
    restart();
}

// Reset and return value before rest in milliseconds
long Chrono::restart()
{
    TimeSpec now;
    gettime(CLOCK_REALTIME, &now);
    long ret = MILLIS(m_orig, now);
    m_orig = now;
    return ret;
}

long Chrono::urestart()
{
    TimeSpec now;
    gettime(CLOCK_REALTIME, &now);
    long ret = MICROS(m_orig, now);
    m_orig = now;
    return ret;
}

// Get current timer value, milliseconds
long Chrono::millis(bool frozen)
{
    if (frozen) {
        return MILLIS(m_orig, o_now);
    } else {
        TimeSpec now;
        gettime(CLOCK_REALTIME, &now);
        return MILLIS(m_orig, now);
    }
}

//
long Chrono::micros(bool frozen)
{
    if (frozen) {
        return MICROS(m_orig, o_now);
    } else {
        TimeSpec now;
        gettime(CLOCK_REALTIME, &now);
        return MICROS(m_orig, now);
    }
}

long long Chrono::amicros() const
{
    TimeSpec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    return MICROS(ts, m_orig);
}

//
long long Chrono::nanos(bool frozen)
{
    if (frozen) {
        return NANOS(m_orig, o_now);
    } else {
        TimeSpec now;
        gettime(CLOCK_REALTIME, &now);
        return NANOS(m_orig, now);
    }
}

float Chrono::secs(bool frozen)
{
    if (frozen) {
        return SECONDS(m_orig, o_now);
    } else {
        TimeSpec now;
        gettime(CLOCK_REALTIME, &now);
        return SECONDS(m_orig, now);
    }
}

#else

///////////////////// test driver


#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include <iostream>

#include "chrono.h"

using namespace std;

static char *thisprog;
static void
Usage(void)
{
    fprintf(stderr, "Usage : %s \n", thisprog);
    exit(1);
}

Chrono achrono;
Chrono rchrono;

void
showsecs(long msecs)
{
    fprintf(stderr, "%3.5f S", ((float)msecs) / 1000.0);
}

void
sigint(int sig)
{
    signal(SIGINT, sigint);
    signal(SIGQUIT, sigint);

    fprintf(stderr, "Absolute interval: ");
    showsecs(achrono.millis());
    fprintf(stderr, ". Relative interval: ");
    showsecs(rchrono.restart());
    cerr <<  " Abs micros: " << achrono.amicros() <<
        " Relabs micros: " << rchrono.amicros() - 1430477861905884LL
         << endl;
    fprintf(stderr, ".\n");
    if (sig == SIGQUIT) {
        exit(0);
    }
}

int main(int argc, char **argv)
{

    thisprog = argv[0];
    argc--;
    argv++;

    if (argc != 0) {
        Usage();
    }

    for (int i = 0; i < 50000000; i++);

    cerr << "Start secs: " << achrono.secs() << endl;

    fprintf(stderr, "Type ^C for intermediate result, ^\\ to stop\n");
    signal(SIGINT, sigint);
    signal(SIGQUIT, sigint);
    achrono.restart();
    rchrono.restart();
    while (1) {
        pause();
    }
}

#endif /*TEST_CHRONO*/
