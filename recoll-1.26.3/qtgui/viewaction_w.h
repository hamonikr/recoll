/* Copyright (C) 2006-2019 J.F.Dockes
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
#ifndef _VIEWACTION_W_H_INCLUDED_
#define _VIEWACTION_W_H_INCLUDED_

#include <qdialog.h>

#include "ui_viewaction.h"

class QDialog;
class QMouseEvent;
class QTableWidget;

class ViewAction : public QDialog, public Ui::ViewActionBase
{
    Q_OBJECT

public:
    ViewAction(QWidget* parent = 0) 
        : QDialog(parent) {
        setupUi(this);
        init();
    }
    ~ViewAction() {}
    void selectMT(const QString& mt);

public slots:
    virtual void editActions();
    virtual void onCurrentItemChanged(QTableWidgetItem *, QTableWidgetItem *);
    virtual void onUseDesktopCBToggled(int);
    virtual void onSetExceptCBToggled(int);
    virtual void onSelSameClicked();
private:
    virtual void init();
    virtual void fillLists();
};

#endif /* _VIEWACTION_W_H_INCLUDED_ */
