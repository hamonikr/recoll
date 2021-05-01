/* Copyright (C) 2006 J.F.Dockes
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
#ifndef _RCLMON_H_INCLUDED_
#define _RCLMON_H_INCLUDED_
#include "autoconfig.h"
#ifdef RCL_MONITOR

/**
 * Definitions for the real-time monitoring recoll. 
 * We're interested in file modifications, deletions and renaming.
 * We use two threads, one to receive events from the source, the
 * other to perform adequate processing.
 *
 * The two threads communicate through an event buffer which is
 * actually a hash map indexed by file path for easy coalescing of
 * multiple events to the same file.
 */
#include <time.h>
#include <string>
#include <map>
#include <mutex>

#include "rclconfig.h"

#ifndef NO_NAMESPACES
using std::string;
using std::multimap;
#endif

/**
 * Monitoring event: something changed in the filesystem
 */
class RclMonEvent {
 public: 
    enum EvType {RCLEVT_NONE= 0, RCLEVT_MODIFY=1, RCLEVT_DELETE=2, 
		 RCLEVT_DIRCREATE=3, RCLEVT_ISDIR=0x10};
    string m_path;
    // Type and flags
    int  m_etyp;

    ///// For fast changing files: minimum time interval before reindex
    // Minimum interval (from config)
    int    m_itvsecs;
    // Don't process this entry before:
    time_t m_minclock; 
    // Changed since put in purgatory after reindex
    bool   m_needidx;

    RclMonEvent() : m_etyp(RCLEVT_NONE),
		    m_itvsecs(0), m_minclock(0), m_needidx(false) {}
    EvType evtype() {return EvType(m_etyp & 0xf);}
    int evflags() {return m_etyp & 0xf0;}
};

enum RclMonitorOption {RCLMON_NONE=0, RCLMON_NOFORK=1, RCLMON_NOX11=2,
		       RCLMON_NOCONFCHECK=4};

/**
 * Monitoring event queue. This is the shared object between the main thread 
 * (which does the actual indexing work), and the monitoring thread which 
 * receives events from FAM / inotify / etc.
 */
class RclEQData;
class RclMonEventQueue {
 public:
    RclMonEventQueue();
    ~RclMonEventQueue();
    /** Wait for event or timeout. Returns with the queue locked */
    std::unique_lock<std::mutex> wait(int secs = -1, bool *timedout = 0);
    /** Add event. */
    bool pushEvent(const RclMonEvent &ev);
    /** To all threads: end processing */
    void setTerminate(); 
    bool ok();
    bool empty();
    RclMonEvent pop();
    void setopts(int opts);

    // Convenience function for initially communicating config to mon thr
    void setConfig(RclConfig *conf);
    RclConfig *getConfig();

 private:
    RclEQData *m_data;
};

/** Start monitoring on the topdirs described in conf */
extern bool startMonitor(RclConfig *conf, int flags);

/** Main routine for the event receiving thread */
extern void *rclMonRcvRun(void *);

// Specific debug macro for monitor synchronization events
#define MONDEB LOGDEB2


#endif // RCL_MONITOR
#endif /* _RCLMON_H_INCLUDED_ */
