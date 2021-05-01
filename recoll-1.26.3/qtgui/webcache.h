/* Copyright (C) 2016 J.F.Dockes
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
#ifndef _WEBCACHE_H_INCLUDED_
#define _WEBCACHE_H_INCLUDED_
#include "autoconfig.h"

#include <vector>
#include <string>
#include <memory>

#include "ui_webcache.h"

#include <QAbstractTableModel>

class WebcacheModelInternal;
class QCloseEvent;

class WebcacheModel : public QAbstractTableModel {
    Q_OBJECT;

public:
    WebcacheModel(QObject *parent = 0);
    ~WebcacheModel();

    // Reimplemented methods
    virtual int rowCount (const QModelIndex& = QModelIndex()) const;
    virtual int columnCount(const QModelIndex& = QModelIndex()) const;
    virtual QVariant headerData (int col, Qt::Orientation orientation, 
				 int role = Qt::DisplayRole) const;
    virtual QVariant data(const QModelIndex& index, 
			   int role = Qt::DisplayRole ) const;
    bool deleteIdx(unsigned int idx);
    std::string getURL(unsigned int idx);

public slots:
    void setSearchFilter(const QString&);
    void reload();

private:
    WebcacheModelInternal *m;
};

class RclMain;

class WebcacheEdit : public QDialog, public Ui::Webcache {
    Q_OBJECT;

public:
    WebcacheEdit(RclMain *parent);
public slots:
    void saveColState();
    void createPopupMenu(const QPoint&);
    void deleteSelected();
    void copyURL();
protected:
    void closeEvent(QCloseEvent *);
private:
    WebcacheModel *m_model;
    RclMain *m_recoll;
    bool m_modified;
};


#endif /* _WEBCACHE_H_INCLUDED_ */
