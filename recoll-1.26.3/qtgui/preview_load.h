/* Copyright (C) 2015 J.F.Dockes
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
#ifndef _PVW_LOAD_H_INCLUDED_
#define _PVW_LOAD_H_INCLUDED_

#include <string>

#include <QThread>

#include "rcldoc.h"
#include "pathut.h"
#include "rclutil.h"
#include "rclconfig.h"
#include "internfile.h"

/* 
 * A thread to perform the file reading / format conversion work for preview
 */
class LoadThread : public QThread {

    Q_OBJECT;

public: 
    LoadThread(RclConfig *conf,
               const Rcl::Doc& idoc, bool pvhtml, QObject *parent = 0);

    virtual ~LoadThread() {
    }

    virtual void run();

public:
    // The results are returned through public members.
    int status;
    Rcl::Doc fdoc;
    TempFile tmpimg;
    std::string missing;
    FileInterner::ErrorPossibleCause explain{FileInterner::InternfileOther};

private:
    Rcl::Doc m_idoc;
    bool m_previewHtml;
    RclConfig m_config;
};


#endif /* _PVW_LOAD_H_INCLUDED_ */
