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

#ifndef _CHRONO_H_INCLUDED_
#define _CHRONO_H_INCLUDED_

#include <time.h>

/** Easy interface to measuring time intervals */
class Chrono {
public:
    /** Initialize, setting the origin time */
    Chrono();

    /** Re-store current time and return mS since init or last call */
    long restart();
    /** Re-store current time and return uS since init or last call */
    long urestart();

    /** Snapshot current time to static storage */
    static void refnow();

    /** Return interval value in various units.
     *
     * If frozen is set this gives the time since the last refnow call
     * (this is to allow for using one actual system call to get
       values from many chrono objects, like when examining timeouts
       in a queue
     */
    long long nanos(bool frozen = false);
    long micros(bool frozen = false);
    long millis(bool frozen = false);
    float secs(bool frozen = false);

    /** Return the absolute value of the current origin */
    long long amicros() const;

    struct TimeSpec {
        time_t tv_sec; /* Time in seconds */
        long   tv_nsec; /* And nanoseconds (< 10E9) */
    };

private:
    TimeSpec m_orig;
    static TimeSpec o_now;
};

#endif /* _CHRONO_H_INCLUDED_ */
