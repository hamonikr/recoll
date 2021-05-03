/* Copyright (C) 2005 J.F.Dockes
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
#include <time.h>

#include <qevent.h>
#include <qwidget.h>
#include <QTabWidget>

#include "recoll.h"
#include "rclhelp.h"
#include "log.h"

map<string, string> HelpClient::helpmap;

void HelpClient::installMap(string wname, string section)
{
    helpmap[wname] = section;
}

HelpClient::HelpClient(QObject *parent, const char *)
    : QObject(parent)
{
    parent->installEventFilter(this);
}
    
bool HelpClient::eventFilter(QObject *obj, QEvent *event)
{
    static time_t last_start;
    if (event->type() != QEvent::KeyPress &&
        event->type() != QEvent::ShortcutOverride) {
        return false;
    }
    QKeyEvent *ke = static_cast<QKeyEvent *>(event);
    if (ke->key() != Qt::Key_F1 && ke->key() != Qt::Key_Help) {
        return false;
    }

    if (!obj->isWidgetType()) {
        return false;
    }

    QWidget *widget = dynamic_cast<QWidget *>(obj);
    if (nullptr == widget) {
        // ??
        return false;
    }
    widget = widget->focusWidget();
    auto it = helpmap.end();
    std::string onm;
    while (widget) {
        QTabWidget *tw = dynamic_cast<QTabWidget*>(widget);
        if (nullptr != tw) {
            onm = qs2utf8s(tw->currentWidget()->objectName());
            LOGDEB1("HelpClient: tab name " << onm << "\n");
            it = helpmap.find(onm);
            if (it != helpmap.end())
                break;
        }                
        onm = qs2utf8s(widget->objectName());
        LOGDEB1("HelpClient: object name " << onm << "\n");
        it = helpmap.find(onm);
        if (it != helpmap.end())
            break;
        widget = widget->parentWidget();
    }
    if (time(0) - last_start > 5) {
        last_start = time(0);
        if (it != helpmap.end()) {
            LOGDEB("HelpClient::eventFilter: " << it->first << "->"  << it->second << "\n");
            startManual(it->second);
        } else {
            LOGDEB("HelpClient::eventFilter: no help section\n");
            startManual("");
        }
    }

    return true;
}

