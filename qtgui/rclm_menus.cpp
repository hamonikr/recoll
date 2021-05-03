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

#include <iostream>

#include "rclmain_w.h"
#include "ssearch_w.h"

#include <QMenu>
#include <QPushButton>
#include <QMessageBox>

void RclMain::buildMenus()
{
    fileMenu = new QMenu();
    fileMenu->setObjectName(QString::fromUtf8("fileMenu"));
    fileMenu->setTitle(QApplication::translate("RclMainBase", "&File"));
    viewMenu = new QMenu();
    viewMenu->setObjectName(QString::fromUtf8("viewMenu"));
    viewMenu->setTitle(QApplication::translate("RclMainBase", "&View"));
    toolsMenu = new QMenu();
    toolsMenu->setObjectName(QString::fromUtf8("toolsMenu"));
    toolsMenu->setTitle(QApplication::translate("RclMainBase", "&Tools"));
    preferencesMenu = new QMenu();
    preferencesMenu->setObjectName(QString::fromUtf8("preferencesMenu"));
    preferencesMenu->setTitle(QApplication::translate("RclMainBase",
                                                      "&Preferences"));
    helpMenu = new QMenu();
    helpMenu->setObjectName(QString::fromUtf8("helpMenu"));
    helpMenu->setTitle(QApplication::translate("RclMainBase", "&Help"));
    resultsMenu = new QMenu();
    resultsMenu->setObjectName(QString::fromUtf8("resultsMenu"));
    resultsMenu->setTitle(QApplication::translate("RclMainBase", "&Results"));
    queryMenu = new QMenu();
    queryMenu->setObjectName(QString::fromUtf8("queryMenu"));
    queryMenu->setTitle(QApplication::translate("RclMainBase", "&Query"));


    fileMenu->insertAction(fileRebuildIndexAction, fileBumpIndexingAction);
    fileMenu->addAction(fileToggleIndexingAction);
    fileMenu->addAction(fileRebuildIndexAction);
    fileMenu->addAction(actionSpecial_Indexing);
    fileMenu->addSeparator();
    fileMenu->addAction(actionSave_last_query);
    fileMenu->addAction(actionLoad_saved_query);
    fileMenu->addSeparator();
    fileMenu->addAction(fileExportSSearchHistoryAction);
    fileMenu->addAction(fileEraseSearchHistoryAction);
    fileMenu->addSeparator();
    fileMenu->addAction(fileEraseDocHistoryAction);
    fileMenu->addSeparator();
    fileMenu->addAction(fileExitAction);

    viewMenu->addSeparator();
    viewMenu->addAction(toggleFullScreenAction);
    viewMenu->addAction(zoomInAction);
    viewMenu->addAction(zoomOutAction);
    
    toolsMenu->addAction(toolsDoc_HistoryAction);
    toolsMenu->addAction(toolsAdvanced_SearchAction);
    toolsMenu->addAction(toolsSpellAction);
    toolsMenu->addAction(actionQuery_Fragments);
    toolsMenu->addAction(actionWebcache_Editor);
    toolsMenu->addAction(showMissingHelpers_Action);
    toolsMenu->addAction(showActiveTypes_Action);
    toolsMenu->addAction(actionShow_index_statistics);

    preferencesMenu->addAction(queryPrefsAction);
    preferencesMenu->addSeparator();
    preferencesMenu->addAction(indexConfigAction);
    preferencesMenu->addAction(indexScheduleAction);

    queryMenu->addSection(QIcon(), tr("Simple search type"));
    sstypGroup = new QActionGroup(this);
    auto actSSAny = new QAction(tr("Any term"), this);
    
    actSSAny->setData(QVariant(SSearch::SST_ANY));
    actSSAny->setCheckable(true);
    sstypGroup->addAction(actSSAny);
    queryMenu->addAction(actSSAny);
    auto actSSAll = new QAction(tr("All terms"), this);
    actSSAll->setData(QVariant(SSearch::SST_ALL));
    actSSAll->setCheckable(true);
    sstypGroup->addAction(actSSAll);
    queryMenu->addAction(actSSAll);
    auto actSSFile = new QAction(tr("File name"), this);
    actSSFile->setData(QVariant(SSearch::SST_FNM));
    actSSFile->setCheckable(true);
    sstypGroup->addAction(actSSFile);
    queryMenu->addAction(actSSFile);
    auto actSSQuery = new QAction(tr("Query language"), this);
    actSSQuery->setData(QVariant(SSearch::SST_LANG));
    actSSQuery->setCheckable(true);
    sstypGroup->addAction(actSSQuery);
    queryMenu->addAction(actSSQuery);
    queryMenu->addSeparator();
    queryMenu->addAction(enbSynAction);
    queryMenu->addSeparator();
    queryMenu->addAction(extIdxAction);
    connect(queryMenu, SIGNAL(triggered(QAction *)), this, 
                          SLOT(onSSTypMenu(QAction *)));
    connect(sSearch->searchTypCMB, SIGNAL(currentIndexChanged(int)), this,
            SLOT(onSSTypCMB(int)));
    queryMenu->addSection(QIcon(), tr("Stemming language"));
    // Stemming language menu
    g_stringNoStem = tr("(no stemming)");
    g_stringAllStem = tr("(all languages)");
    m_idNoStem = queryMenu->addAction(g_stringNoStem);
    m_idNoStem->setCheckable(true);
    m_stemLangToId[g_stringNoStem] = m_idNoStem;
    m_idAllStem = queryMenu->addAction(g_stringAllStem);
    m_idAllStem->setCheckable(true);
    m_stemLangToId[g_stringAllStem] = m_idAllStem;
    // Can't get the stemming languages from the db at this stage as
    // db not open yet (the case where it does not even exist makes
    // things complicated). So get the languages from the config
    // instead
    vector<string> langs;
    if (!getStemLangs(langs)) {
        QMessageBox::warning(0, "Recoll", 
                             tr("error retrieving stemming languages"));
    }
    QAction *curid = prefs.queryStemLang == "ALL" ? m_idAllStem : m_idNoStem;
    QAction *id; 
    for (const auto& lang : langs) {
        QString qlang = u8s2qs(lang);
        id = queryMenu->addAction(qlang);
        id->setCheckable(true);
        m_stemLangToId[qlang] = id;
        if (prefs.queryStemLang == qlang) {
            curid = id;
        }
    }
    curid->setChecked(true);
    
    helpMenu->addAction(userManualAction);
    helpMenu->addAction(showMissingHelpers_Action);
    helpMenu->addAction(showActiveTypes_Action);
    helpMenu->addSeparator();
    helpMenu->addAction(helpAbout_RecollAction);

    resultsMenu->addAction(nextPageAction);
    resultsMenu->addAction(prevPageAction);
    resultsMenu->addAction(firstPageAction);
    resultsMenu->addSeparator();
    resultsMenu->addAction(actionSortByDateAsc);
    resultsMenu->addAction(actionSortByDateDesc);
    resultsMenu->addSeparator();
    resultsMenu->addAction(actionShowQueryDetails);
    resultsMenu->addSeparator();
    resultsMenu->addAction(actionShowResultsAsTable);
    resultsMenu->addSeparator();
    resultsMenu->addAction(actionSaveResultsAsCSV);

    MenuBar->addAction(fileMenu->menuAction());
    MenuBar->addAction(queryMenu->menuAction());
    MenuBar->addAction(resultsMenu->menuAction());
    MenuBar->addAction(viewMenu->menuAction());
    MenuBar->addAction(toolsMenu->menuAction());
    MenuBar->addAction(preferencesMenu->menuAction());
    MenuBar->addSeparator();
    MenuBar->addAction(helpMenu->menuAction());

    buttonTopMenu = new QMenu();
    buttonTopMenu->addAction(fileMenu->menuAction());
    buttonTopMenu->addAction(queryMenu->menuAction());
    buttonTopMenu->addAction(viewMenu->menuAction());
    buttonTopMenu->addAction(toolsMenu->menuAction());
    buttonTopMenu->addAction(resultsMenu->menuAction());
    buttonTopMenu->addAction(preferencesMenu->menuAction());
    buttonTopMenu->addSeparator();
    buttonTopMenu->addAction(helpMenu->menuAction());
    sSearch->menuPB->setMenu(buttonTopMenu);

    return;
}

void RclMain::onSSTypMenu(QAction *act)
{
    if (act->actionGroup() != sstypGroup) {
        return;
    }
    int id = act->data().toInt();
    sSearch->onSearchTypeChanged(id);
}

void RclMain::onSSTypCMB(int idx)
{
    QList<QAction*> ssacts = sstypGroup->actions();
    for (int i = 0; i < ssacts.size(); ++i) {
        if (ssacts.at(i)->data().toInt() == idx)
            ssacts.at(i)->setChecked(true);
    }
}
