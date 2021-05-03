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
#ifndef _CRONTOOL_W_H_INCLUDED_
#define _CRONTOOL_W_H_INCLUDED_

#include "ui_crontool.h"

class QPushButton;

class CronToolW : public QDialog, public Ui::CronToolW {
    Q_OBJECT
public:
    CronToolW(QWidget * parent = 0) 
        : QDialog(parent), enableButton(0), disableButton(0) {
        setupUi(this);
        init();
    }
    QPushButton *enableButton;
    QPushButton *disableButton;

private slots:
    void enableCron();
    void disableCron();
private:
    void init();
    void changeCron(bool enable);
};


#endif /* _CRONTOOL_W_H_INCLUDED_ */
