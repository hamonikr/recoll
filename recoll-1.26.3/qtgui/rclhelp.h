/*
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
#ifndef RCLHELP_H
#define RCLHELP_H
#include <qobject.h>

#include <string>
#include <map>
using std::map;
using std::string;

class HelpClient : public QObject
{
    Q_OBJECT
public:
    HelpClient(QObject *parent, const char *name = 0);

    // Install mapping from widget name to manual section
    static void installMap(string wname, string section);

protected:
    bool eventFilter(QObject *obj, QEvent *event);
    static map<string, string> helpmap;
};

#endif // RCLHELP_H
