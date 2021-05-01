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
#include "autoconfig.h"

#include <stdio.h>

#include <vector>
#include <utility>
#include <string>

using namespace std;

#include <qpushbutton.h>
#include <qtimer.h>

#include <qlistwidget.h>

#include <qmessagebox.h>
#include <qinputdialog.h>
#include <qlayout.h>

#include "recoll.h"
#include "log.h"
#include "guiutils.h"
#include "conftree.h"

#include "ptrans_w.h"

void EditTrans::init(const string& dbdir)
{
    m_dbdir = path_canon(dbdir);
    connect(transTW, SIGNAL(itemDoubleClicked(QTableWidgetItem *)),
	    this, SLOT(onItemDoubleClicked(QTableWidgetItem *)));
    connect(cancelPB, SIGNAL(clicked()), this, SLOT(close()));

    QString lab = whatIdxLA->text();
    lab.append(QString::fromLocal8Bit(m_dbdir.c_str()));
    whatIdxLA->setText(lab);

    QStringList labels(tr("Source path"));
    labels.push_back(tr("Local path"));
    transTW->setHorizontalHeaderLabels(labels);

    ConfSimple *conftrans = theconfig->getPTrans();
    if (!conftrans)
	return;

    int row = 0;
    vector<string> opaths = conftrans->getNames(m_dbdir);
    for (vector<string>::const_iterator it = opaths.begin(); 
	 it != opaths.end(); it++) {
	transTW->setRowCount(row+1);
	transTW->setItem(row, 0, new QTableWidgetItem(
			     QString::fromLocal8Bit(it->c_str())));
	string npath;
	conftrans->get(*it, npath, m_dbdir);
	transTW->setItem(row, 1, new QTableWidgetItem(
			     QString::fromLocal8Bit(npath.c_str())));
	row++;
    }

    resize(QSize(640, 300).expandedTo(minimumSizeHint()));
}

void EditTrans::onItemDoubleClicked(QTableWidgetItem *item)
{
    transTW->editItem(item);
}

void EditTrans::on_savePB_clicked()
{
    ConfSimple *conftrans = theconfig->getPTrans();
    if (!conftrans) {
	QMessageBox::warning(0, "Recoll", tr("Config error"));
	return;
    }
    conftrans->holdWrites(true);
    conftrans->eraseKey(m_dbdir);

    for (int row = 0; row < transTW->rowCount(); row++) {
	QTableWidgetItem *item0 = transTW->item(row, 0);
	string from = path_canon((const char *)item0->text().toLocal8Bit());
	QTableWidgetItem *item1 = transTW->item(row, 1);
	string to = path_canon((const char*)item1->text().toLocal8Bit());
	conftrans->set(from, to, m_dbdir);
    }
    conftrans->holdWrites(false);
    // The rcldb does not use the same configuration object, but a
    // copy. Force a reopen, this is quick.
    string reason;
    maybeOpenDb(reason, true);
    close();
}

void EditTrans::on_addPB_clicked()
{
    transTW->setRowCount(transTW->rowCount()+1);
    int row = transTW->rowCount()-1;
    transTW->setItem(row, 0, new QTableWidgetItem(tr("Original path")));
    transTW->setItem(row, 1, new QTableWidgetItem(tr("Local path")));
    transTW->editItem(transTW->item(row, 0));
}

void EditTrans::on_delPB_clicked()
{
    QModelIndexList indexes = transTW->selectionModel()->selectedIndexes();
    vector<int> rows;
    for (int i = 0; i < indexes.size(); i++) {
	rows.push_back(indexes.at(i).row());
    }
    sort(rows.begin(), rows.end());
    rows.resize(unique(rows.begin(), rows.end()) - rows.begin());
    for (int i = rows.size()-1; i >= 0; i--) {
	transTW->removeRow(rows[i]);
    }
}

void EditTrans::on_transTW_itemSelectionChanged()
{
    QModelIndexList indexes = transTW->selectionModel()->selectedIndexes();
    if(indexes.size() < 1)
	delPB->setEnabled(0);
    else 
	delPB->setEnabled(1);
}

