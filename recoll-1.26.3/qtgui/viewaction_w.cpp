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
#include "autoconfig.h"

#include "viewaction_w.h"

#include <vector>
#include <utility>
#include <string>

#include <QMessageBox>

#include "recoll.h"
#include "log.h"
#include "guiutils.h"

using namespace std;


void ViewAction::init()
{
    selSamePB->setEnabled(false);
    connect(closePB, SIGNAL(clicked()), this, SLOT(close()));
    connect(chgActPB, SIGNAL(clicked()), this, SLOT(editActions()));
    connect(actionsLV,
            SIGNAL(currentItemChanged(QTableWidgetItem *, QTableWidgetItem *)),
            this,
            SLOT(onCurrentItemChanged(QTableWidgetItem *, QTableWidgetItem *)));
    useDesktopCB->setChecked(prefs.useDesktopOpen);
    onUseDesktopCBToggled(prefs.useDesktopOpen);
    connect(useDesktopCB, SIGNAL(stateChanged(int)), 
            this, SLOT(onUseDesktopCBToggled(int)));
    connect(setExceptCB, SIGNAL(stateChanged(int)), 
            this, SLOT(onSetExceptCBToggled(int)));
    connect(selSamePB, SIGNAL(clicked()),
            this, SLOT(onSelSameClicked()));
    resize(QSize(640, 480).expandedTo(minimumSizeHint()));
}
        
void ViewAction::onUseDesktopCBToggled(int onoff)
{
    prefs.useDesktopOpen = onoff != 0;
    fillLists();
    setExceptCB->setEnabled(prefs.useDesktopOpen);
}

void ViewAction::onSetExceptCBToggled(int onoff)
{
    newActionLE->setEnabled(onoff != 0);
}

void ViewAction::fillLists()
{
    currentLBL->clear();
    actionsLV->clear();
    actionsLV->verticalHeader()->setDefaultSectionSize(20); 
    vector<pair<string, string> > defs;
    theconfig->getMimeViewerDefs(defs);
    actionsLV->setRowCount(defs.size());

    set<string> viewerXs;
    if (prefs.useDesktopOpen) {
        viewerXs = theconfig->getMimeViewerAllEx();
    }

    int row = 0;
    for (const auto& def : defs) {
        actionsLV->setItem(row, 0, new QTableWidgetItem(u8s2qs(def.first)));
        if (!prefs.useDesktopOpen ||
            viewerXs.find(def.first) != viewerXs.end()) {
            actionsLV->setItem(row, 1, new QTableWidgetItem(u8s2qs(def.second)));
        } else {
            actionsLV->setItem(
                row, 1, new QTableWidgetItem(tr("Desktop Default")));
        }
        row++;
    }
    QStringList labels(tr("MIME type"));
    labels.push_back(tr("Command"));
    actionsLV->setHorizontalHeaderLabels(labels);
}

void ViewAction::selectMT(const QString& mt)
{
    actionsLV->clearSelection();
    QList<QTableWidgetItem *>items = 
        actionsLV->findItems(mt, Qt::MatchFixedString|Qt::MatchCaseSensitive);
    for (QList<QTableWidgetItem *>::iterator it = items.begin();
         it != items.end(); it++) {
        (*it)->setSelected(true);
        actionsLV->setCurrentItem(*it, QItemSelectionModel::Columns);
    }
}

void ViewAction::onSelSameClicked()
{
    actionsLV->clearSelection();
    QString value = currentLBL->text();
    if (value.isEmpty())
        return;
    string action = qs2utf8s(value);
    LOGDEB1("ViewAction::onSelSameClicked: value: " << action << endl);

    vector<pair<string, string> > defs;
    theconfig->getMimeViewerDefs(defs);
    for (const auto& def : defs) {
        if (def.second == action) {
            QList<QTableWidgetItem *>items = actionsLV->findItems(
                u8s2qs(def.first), Qt::MatchFixedString|Qt::MatchCaseSensitive);
            for (QList<QTableWidgetItem *>::iterator it = items.begin();
                 it != items.end(); it++) {
                (*it)->setSelected(true);
                actionsLV->item((*it)->row(), 1)->setSelected(true);
            }
        }
    }
}

void ViewAction::onCurrentItemChanged(QTableWidgetItem *item, QTableWidgetItem *)
{
    currentLBL->clear();
    selSamePB->setEnabled(false);
    if (nullptr == item) {
        return;
    }
    QTableWidgetItem *item0 = actionsLV->item(item->row(), 0);
    string mtype = qs2utf8s(item0->text());

    vector<pair<string, string> > defs;
    theconfig->getMimeViewerDefs(defs);
    for (const auto& def : defs) {
        if (def.first == mtype) {
            currentLBL->setText(u8s2qs(def.second));
            selSamePB->setEnabled(true);
            return;
        }
    }
}

void ViewAction::editActions()
{
    QString action0;
    int except0 = -1;

    set<string> viewerXs = theconfig->getMimeViewerAllEx();
    vector<string> mtypes;
    bool dowarnmultiple = true;
    for (int row = 0; row < actionsLV->rowCount(); row++) {
        QTableWidgetItem *item0 = actionsLV->item(row, 0);
        if (!item0->isSelected())
            continue;
        string mtype = qs2utf8s(item0->text());
        mtypes.push_back(mtype);
        QTableWidgetItem *item1 = actionsLV->item(row, 1);
        QString action = item1->text();
        bool except = viewerXs.find(mtype) != viewerXs.end();
        if (action0.isEmpty()) {
            action0 = action;
            except0 = except;
        } else {
            if ((action != action0 || except != except0) && dowarnmultiple) {
                switch (QMessageBox::warning(0, "Recoll",
                                             tr("Changing entries with "
                                                "different current values"),
                                             "Continue",
                                             "Cancel",
                                             0, 0, 1)) {
                case 0: dowarnmultiple = false; break;
                case 1: return;
                }
            }
        }
    }

    if (action0.isEmpty())
        return;
    string sact = qs2utf8s(newActionLE->text());
    if (!sact.empty()) {
        trimstring(sact);
#ifdef _WIN32
        path_slashize(sact);
#endif
    }
    for (const auto& entry : mtypes) {
        auto xit = viewerXs.find(entry);
        if (setExceptCB->isChecked()) {
            if (xit == viewerXs.end()) {
                viewerXs.insert(entry);
            }
        } else {
            if (xit != viewerXs.end()) {
                viewerXs.erase(xit);
            }
        }
        // An empty action will restore the default (erase from
        // topmost conftree)
        theconfig->setMimeViewerDef(entry, sact);
    }

    theconfig->setMimeViewerAllEx(viewerXs);
    fillLists();
}
