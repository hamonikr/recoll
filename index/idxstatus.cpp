/* Copyright (C) 2017-2021 J.F.Dockes
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

#include <mutex>

#include "chrono.h"
#include "conftree.h"
#include "idxstatus.h"
#include "log.h"
#include "rclconfig.h"
#include "x11mon.h"

// Global stop request flag. This is checked in a number of place in the
// indexing routines.
int stopindexing;

void readIdxStatus(RclConfig *config, DbIxStatus &status)
{
    ConfSimple cs(config->getIdxStatusFile().c_str(), 1);
    status.phase = DbIxStatus::Phase(cs.getInt("phase", 0));
    cs.get("fn", status.fn);
    status.docsdone = (int)cs.getInt("docsdone", 0);
    status.filesdone = (int)cs.getInt("filesdone", 0);
    status.fileerrors = (int)cs.getInt("fileerrors", 0);
    status.dbtotdocs = (int)cs.getInt("dbtotdocs", 0);
    status.totfiles = (int)cs.getInt("totfiles", 0);
    status.hasmonitor = cs.getBool("hasmonitor", false);
}

// Receive status updates from the ongoing indexing operation
// Also check for an interrupt request and return the info to caller which
// should subsequently orderly terminate what it is doing.
class DbIxStatusUpdater::Internal {
public:
#ifdef IDX_THREADS
    std::mutex m_mutex;
#endif
    Internal(const RclConfig *config, bool nox11mon)
        : m_file(config->getIdxStatusFile().c_str()), m_stopfilename(config->getIdxStopFile()),
          nox11monitor(nox11mon) {
        // The total number of files included in the index is actually
        // difficult to compute from the index itself. For display
        // purposes, we save it in the status file from indexing to
        // indexing (mostly...)
        string stf;
        if (m_file.get("totfiles", stf)) {
            status.totfiles = atoi(stf.c_str());
        }
    }
    
    virtual bool update() {
        if (status.dbtotdocs < status.docsdone)
            status.dbtotdocs = status.docsdone;
        // Update the status file. Avoid doing it too often. Always do
        // it at the end (status DONE)
        if (status.phase == DbIxStatus::DBIXS_DONE || 
            status.phase != m_prevphase || m_chron.millis() > 300) {
            if (status.totfiles < status.filesdone || status.phase == DbIxStatus::DBIXS_DONE) {
                status.totfiles = status.filesdone;
            }
            m_prevphase = status.phase;
            m_chron.restart();
            m_file.holdWrites(true);
            m_file.set("phase", int(status.phase));
            m_file.set("docsdone", status.docsdone);
            m_file.set("filesdone", status.filesdone);
            m_file.set("fileerrors", status.fileerrors);
            m_file.set("dbtotdocs", status.dbtotdocs);
            m_file.set("totfiles", status.totfiles);
            m_file.set("fn", status.fn);
            m_file.set("hasmonitor", status.hasmonitor);
            m_file.holdWrites(false);
        }
        if (path_exists(m_stopfilename)) {
            LOGINF("recollindex: asking indexer to stop because " << m_stopfilename << " exists\n");
            path_unlink(m_stopfilename);
            stopindexing = true;
        }
        if (stopindexing) {
            return false;
        }

#ifndef DISABLE_X11MON
        // If we are in the monitor, we also need to check X11 status
        // during the initial indexing pass (else the user could log
        // out and the indexing would go on, not good (ie: if the user
        // logs in again, the new recollindex will fail).
        if (status.hasmonitor && !nox11monitor && !x11IsAlive()) {
            LOGDEB("X11 session went away during initial indexing pass\n");
            stopindexing = true;
            return false;
        }
#endif
        return true;
    }

    DbIxStatus status;
    ConfSimple m_file;
    string m_stopfilename;
    Chrono m_chron;
    bool nox11monitor{false};
    DbIxStatus::Phase m_prevphase{DbIxStatus::DBIXS_NONE};
};


DbIxStatusUpdater::DbIxStatusUpdater(const RclConfig *config, bool nox11monitor) {
    m = new Internal(config, nox11monitor);
}

void DbIxStatusUpdater::setMonitor(bool onoff)
{
    m->status.hasmonitor = onoff;
}

void DbIxStatusUpdater::setDbTotDocs(int totdocs)
{
#ifdef IDX_THREADS
    std::unique_lock<std::mutex>  lock(m->m_mutex);
#endif
    m->status.dbtotdocs = totdocs;
}

bool DbIxStatusUpdater::update(DbIxStatus::Phase phase, const string& fn, int incr) {
#ifdef IDX_THREADS
    std::unique_lock<std::mutex>  lock(m->m_mutex);
#endif

    // We don't change a FLUSH status except if the new status is NONE
    // (recollindex init or rcldb after commit(). Else, the flush status maybe
    // overwritten by a "file updated" status and not be displayed
    if (phase == DbIxStatus::DBIXS_NONE || m->status.phase != DbIxStatus::DBIXS_FLUSH)
        m->status.phase = phase;
    m->status.fn = fn;
    if (incr & IncrDocsDone)
        m->status.docsdone++;
    if (incr & IncrFilesDone)
        m->status.filesdone++;
    if (incr & IncrFileErrors)
        m->status.fileerrors++;
    return m->update();
}

static DbIxStatusUpdater *updater;

DbIxStatusUpdater *statusUpdater(RclConfig *config, bool nox11mon)
{
    if (updater) {
        return updater;
    }
    return (updater = new DbIxStatusUpdater(config, nox11mon));
}
