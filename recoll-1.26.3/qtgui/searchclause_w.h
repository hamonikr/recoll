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
#ifndef SEARCHCLAUSE_H
#define SEARCHCLAUSE_H
// A class for entry of a search clause: type (OR/AND/etc.), distance
// for PHRASE or NEAR, and text

#include <qvariant.h>
#include <qwidget.h>
#include "searchdata.h"

class QVBoxLayout;
class QHBoxLayout;
class QComboBox;
class QSpinBox;
class QLineEdit;

class SearchClauseW : public QWidget
{
    Q_OBJECT

public:
    SearchClauseW(QWidget* parent = 0);
    ~SearchClauseW();
    Rcl::SearchDataClause *getClause();
    void setFromClause(Rcl::SearchDataClauseSimple *cl);
    void clear();

    QComboBox* sTpCMB;
    QComboBox* fldCMB;
    QSpinBox*  proxSlackSB;
    QLineEdit* wordsLE;

public slots:
    virtual void tpChange(int);
protected slots:
    virtual void languageChange();
};

#endif // SEARCHCLAUSE_H
