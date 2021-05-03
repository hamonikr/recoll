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
#ifndef _PTRANS_W_H_INCLUDED_
#define _PTRANS_W_H_INCLUDED_

#include <string>

#include <qvariant.h>
#include <qdialog.h>

#include "ui_ptrans.h"

class QTableWidgetItem;

class EditTrans : public QDialog, public Ui::EditTransBase
{
    Q_OBJECT

public:
    EditTrans(const std::string& dbdir, QWidget* parent = 0)
        : QDialog(parent) {
        setupUi(this);
        init(dbdir);
    }

public slots:
    virtual void onItemDoubleClicked(QTableWidgetItem *);
    virtual void on_savePB_clicked();
    virtual void on_addPB_clicked();
    virtual void on_delPB_clicked();
    virtual void on_transTW_itemSelectionChanged();
private:
    virtual void init(const std::string& dbdir);
    std::string m_dbdir;
};

#endif /* _PTRANS_W_H_INCLUDED_ */
