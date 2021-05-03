/* Copyright (C) 2005-2020 J.F.Dockes
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
#include <QAction>
#include <QMenu>

#include "systray.h"
#include "rclmain_w.h"
#include "log.h"

void RclTrayIcon::init()
{
    QAction *restoreAction = new QAction(tr("Restore"), this);
    QAction *quitAction = new QAction(tr("Quit"), this);
     
    connect(restoreAction, SIGNAL(triggered()), this, SLOT(onRestore()));
    connect(quitAction, SIGNAL(triggered()), m_mainw, SLOT(fileExit()));
    QMenu *trayIconMenu = new QMenu(0);
    trayIconMenu->addAction(restoreAction);
    trayIconMenu->addAction(quitAction);
    setContextMenu(trayIconMenu);

    connect(this, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(onActivated(QSystemTrayIcon::ActivationReason)));
}

void RclTrayIcon::onRestore()
{
    // Hide and show to restore on current desktop
    m_mainw->hide();
    switch (prefs.showmode) {
    case PrefsPack::SHOW_NORMAL: m_mainw->show(); break;
    case PrefsPack::SHOW_MAX: m_mainw->showMaximized(); break;
    case PrefsPack::SHOW_FULL: m_mainw->showFullScreen(); break;
    }
}

void RclTrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    LOGDEB("RclTrayIcon::onActivated: reason " << reason << std::endl);
    switch (reason) {
    case QSystemTrayIcon::DoubleClick:
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::MiddleClick:
        onRestore();
        break;
    default:
        return;
    }
}
