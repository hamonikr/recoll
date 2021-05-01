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
#ifndef _SPECIDX_W_H_INCLUDED_
#define _SPECIDX_W_H_INCLUDED_

#include <vector>
#include <string>

#include "ui_specialindex.h"

class QPushButton;

class SpecIdxW : public QDialog, public Ui::SpecIdxW {
    Q_OBJECT

public:

    SpecIdxW(QWidget * parent = 0) 
	: QDialog(parent)
    {
	setupUi(this);
        selPatsLE->setEnabled(false);
        connect(browsePB, SIGNAL(clicked()), this, SLOT(onBrowsePB_clicked()));
        connect(targLE, SIGNAL(textChanged(const QString&)), 
                this, SLOT(onTargLE_textChanged(const QString&)));
    }
    bool noRetryFailed();
    bool eraseFirst();
    std::vector<std::string> selpatterns();
    std::string toptarg();

public slots:

    void onTargLE_textChanged(const QString&);
    void onBrowsePB_clicked();
};


#endif /* _SPECIDX_W_H_INCLUDED_ */
