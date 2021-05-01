/* Copyright (C) 2017-2018 J.F.Dockes
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


#include "idxstatus.h"

#include "rclconfig.h"
#include "conftree.h"

void readIdxStatus(RclConfig *config, DbIxStatus &status)
{
    ConfSimple cs(config->getIdxStatusFile().c_str(), 1);
    string val;
    cs.get("phase", val);
    status.phase = DbIxStatus::Phase(atoi(val.c_str()));
    cs.get("fn", status.fn);
    cs.get("docsdone", &status.docsdone);
    cs.get("filesdone", &status.filesdone);
    cs.get("fileerrors", &status.fileerrors);
    cs.get("dbtotdocs", &status.dbtotdocs);
    cs.get("totfiles", &status.totfiles);
    string shm("0");
    cs.get("hasmonitor", shm);
    status.hasmonitor = stringToBool(shm);
}
