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
#ifndef _ADVSEARCH_W_H_INCLUDED_
#define _ADVSEARCH_W_H_INCLUDED_
#include "autoconfig.h"

#include <vector>

#include <QVariant>
#include <QDialog>

#include "searchclause_w.h"
#include "recoll.h"
#include <memory>
#include "searchdata.h"
#include "advshist.h"

class QDialog;

#include "ui_advsearch.h"

class AdvSearch : public QDialog, public Ui::AdvSearchBase
{
    Q_OBJECT

public:
    AdvSearch(QDialog* parent = 0) 
	: QDialog(parent)
    {
	setupUi(this);
	init();
    }

public slots:
    virtual void delFiltypPB_clicked();
    virtual void delAFiltypPB_clicked();
    virtual void addFiltypPB_clicked();
    virtual void addAFiltypPB_clicked();
    virtual void guiListsToIgnTypes();
    virtual void filterDatesCB_toggled(bool);
    virtual void filterSizesCB_toggled(bool);
    virtual void restrictFtCB_toggled(bool);
    virtual void restrictCtCB_toggled(bool);
    virtual void runSearch();
    virtual void fromSearch(std::shared_ptr<Rcl::SearchData> sdata);
    virtual void browsePB_clicked();
    virtual void saveFileTypes();
    virtual void delClause(bool updsaved=true);
    virtual void addClause(bool updsaved=true);
    virtual void addClause(int, bool updsaved=true);
    virtual void slotHistoryNext();
    virtual void slotHistoryPrev();

signals:
    void startSearch(std::shared_ptr<Rcl::SearchData>, bool);
    void setDescription(QString);
    
private:
    virtual void init();
    std::vector<SearchClauseW *> m_clauseWins;
    QStringList                m_ignTypes;
    bool                       m_ignByCats;
    void saveCnf();
    void fillFileTypes();
    size_t stringToSize(QString);
};


#endif /* _ADVSEARCH_W_H_INCLUDED_ */
