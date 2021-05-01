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
#include "autoconfig.h"

#include <utility>
#include <memory>
#include <stdlib.h>

#include <qapplication.h>
#include <qmessagebox.h>
#include <qfiledialog.h>
#include <qshortcut.h>
#include <qtabwidget.h>
#include <qtimer.h>
#include <qstatusbar.h>
#include <qwindowdefs.h>
#include <qcheckbox.h>
#include <qfontdialog.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qaction.h>
#include <qpushbutton.h>
#include <qimage.h>
#include <qcursor.h>
#include <qevent.h>
#include <QFileSystemWatcher>
#include <QThread>
#include <QProgressDialog>
#include <QToolBar>

#include "recoll.h"
#include "log.h"
#include "mimehandler.h"
#include "pathut.h"
#include "smallut.h"
#include "advsearch_w.h"
#include "sortseq.h"
#include "uiprefs_w.h"
#include "guiutils.h"
#include "reslist.h"
#include "ssearch_w.h"
#include "internfile.h"
#include "docseqdb.h"
#include "docseqhist.h"
#include "docseqdocs.h"
#include "restable.h"
#include "firstidx.h"
#include "indexer.h"
#include "rclzg.h"
#include "snippets_w.h"
#include "fragbuts.h"
#include "systray.h"
#include "rclmain_w.h"
#include "rclhelp.h"
#include "moc_rclmain_w.cpp"

using std::pair;

QString g_stringAllStem, g_stringNoStem;

static Qt::ToolBarArea int2area(int in)
{
    switch (in) {
    case Qt::LeftToolBarArea: return Qt::LeftToolBarArea;
    case Qt::RightToolBarArea: return Qt::RightToolBarArea;
    case Qt::BottomToolBarArea: return Qt::BottomToolBarArea;
    case Qt::TopToolBarArea:
    default:
        return Qt::TopToolBarArea;
    }
}

static QString configToTitle()
{
    string confdir = path_getsimple(theconfig->getConfDir());
    // Lower-case version. This only works with the ascii part, but
    // that's ok even if there are non-ascii chars in there, because
    // we further operate only on ascii substrings.
    string lconfdir = stringtolower((const string&)confdir);

    if (!lconfdir.empty() && lconfdir[0] == '.') {
        lconfdir = lconfdir.substr(1);
        confdir = confdir.substr(1);
    }
    string::size_type pos = lconfdir.find("recoll");
    if (pos != string::npos) {
        lconfdir = lconfdir.substr(0, pos) + lconfdir.substr(pos+6);
        confdir = confdir.substr(0, pos) + confdir.substr(pos+6);
    }
    if (!confdir.empty()) {
        switch (confdir[0]) {
        case '.': case '-': case '_':
            confdir = confdir.substr(1);
            break;
        default:
            break;
        }
    }
    if (confdir.empty()) {
        confdir = "Recoll";
    } else {
        confdir = string("Recoll - ") + confdir;
    }
    return QString::fromUtf8(confdir.c_str());
}

void RclMain::init()
{
    // This is just to get the common catg strings into the message file
    static const char* catg_strings[] = {
            QT_TR_NOOP("All"), QT_TR_NOOP("media"),  QT_TR_NOOP("message"),
            QT_TR_NOOP("other"),  QT_TR_NOOP("presentation"),
            QT_TR_NOOP("spreadsheet"),  QT_TR_NOOP("text"), 
	    QT_TR_NOOP("sorted"), QT_TR_NOOP("filtered")
    };
    setWindowTitle(configToTitle());

    DocSequence::set_translations((const char *)tr("sorted").toUtf8(), 
				(const char *)tr("filtered").toUtf8());

    periodictimer = new QTimer(this);

    // idxstatus file. Make sure it exists before trying to watch it
    // (case where we're started on an older index, or if the status
    // file was deleted since indexing)
    QString idxfn = 
        QString::fromLocal8Bit(theconfig->getIdxStatusFile().c_str());
    QFile qf(idxfn);
    qf.open(QIODevice::ReadWrite);
    qf.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
    qf.close();
    m_watcher.addPath(idxfn);
    // At least some versions of qt4 don't display the status bar if
    // it's not created here.
    (void)statusBar();

    (void)new HelpClient(this);
    HelpClient::installMap((const char *)this->objectName().toUtf8(),
                           "RCL.SEARCH.GUI.SIMPLE");

    // Set the focus to the search terms entry:
    sSearch->takeFocus();

    enbSynAction->setDisabled(prefs.synFile.isEmpty());
    enbSynAction->setChecked(prefs.synFileEnable);
    
    // Stemming language menu
    g_stringNoStem = tr("(no stemming)");
    g_stringAllStem = tr("(all languages)");
    m_idNoStem = preferencesMenu->addAction(g_stringNoStem);
    m_idNoStem->setCheckable(true);
    m_stemLangToId[g_stringNoStem] = m_idNoStem;
    m_idAllStem = preferencesMenu->addAction(g_stringAllStem);
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
    for (vector<string>::const_iterator it = langs.begin(); 
	 it != langs.end(); it++) {
	QString qlang = QString::fromUtf8(it->c_str(), it->length());
	id = preferencesMenu->addAction(qlang);
	id->setCheckable(true);
	m_stemLangToId[qlang] = id;
	if (prefs.queryStemLang == qlang) {
	    curid = id;
	}
    }
    curid->setChecked(true);

    m_toolsTB = new QToolBar(tr("Tools"), this);
    m_toolsTB->setObjectName(QString::fromUtf8("m_toolsTB"));
    m_toolsTB->addAction(toolsAdvanced_SearchAction);
    m_toolsTB->addAction(toolsDoc_HistoryAction);
    m_toolsTB->addAction(toolsSpellAction);
    m_toolsTB->addAction(actionQuery_Fragments);
    this->addToolBar(int2area(prefs.toolArea), m_toolsTB);

    m_resTB = new QToolBar(tr("Results"), this);
    m_resTB->setObjectName(QString::fromUtf8("m_resTB"));
    this->addToolBar(int2area(prefs.resArea), m_resTB);

    // Document filter buttons and combobox
    // Combobox version of the document filter control
    m_filtCMB = new QComboBox(m_resTB);
    m_filtCMB->setEditable(false);
    m_filtCMB->addItem(tr("All"));
    m_filtCMB->setToolTip(tr("Document filter"));
    // Buttons version of the document filter control
    m_filtFRM = new QFrame(this);
    m_filtFRM->setObjectName(QString::fromUtf8("m_filtFRM"));
    QSizePolicy sizePolicy2(QSizePolicy::Preferred, QSizePolicy::Maximum);
    sizePolicy2.setHorizontalStretch(0);
    sizePolicy2.setVerticalStretch(0);
    sizePolicy2.setHeightForWidth(m_filtFRM->sizePolicy().hasHeightForWidth());
    m_filtFRM->setSizePolicy(sizePolicy2);
    QHBoxLayout *bgrphbox = new QHBoxLayout(m_filtFRM);
    m_filtBGRP  = new QButtonGroup(m_filtFRM);
    QRadioButton *allRDB = new QRadioButton(m_filtFRM);
    verticalLayout->insertWidget(1, m_filtFRM);
    allRDB->setObjectName(QString::fromUtf8("allRDB"));
    allRDB->setGeometry(QRect(0, 0, 45, 20));
    allRDB->setText(tr("All"));
    bgrphbox->addWidget(allRDB);
    int bgrpid = 0;
    m_filtBGRP->addButton(allRDB, bgrpid++);
    allRDB->setChecked(true);

    // Menu version of the document filter control
    m_filtMN = new QMenu(MenuBar);
    m_filtMN->setObjectName(QString::fromUtf8("m_filtMN"));
    MenuBar->insertMenu(helpMenu->menuAction(), m_filtMN);
    m_filtMN->setTitle("F&ilter");
    QActionGroup *fltag = new QActionGroup(this);
    fltag->setExclusive(true);
    QAction *act = fltag->addAction(tr("All"));
    m_filtMN->addAction(act);
    act->setCheckable(true);
    act->setData((int)0);

    // Go through the filter list and setup buttons and combobox
    vector<string> cats;
    theconfig->getGuiFilterNames(cats);
    m_catgbutvec.push_back(catg_strings[0]);
    for (vector<string>::const_iterator it = cats.begin();
	 it != cats.end(); it++) {
	QRadioButton *but = new QRadioButton(m_filtFRM);
	QString catgnm = QString::fromUtf8(it->c_str(), it->length());
	m_catgbutvec.push_back(*it);
	// We strip text before the first colon before setting the button name.
	// This is so that the user can decide the order of buttons by naming 
	// the filter,ie, a:media b:messages etc.
	QString but_txt = catgnm;
	int colon = catgnm.indexOf(':');
	if (colon != -1) {
	    but_txt = catgnm.right(catgnm.size()-(colon+1));
	}
	but->setText(tr(but_txt.toUtf8()));
        m_filtCMB->addItem(tr(but_txt.toUtf8()));
        bgrphbox->addWidget(but);
        m_filtBGRP->addButton(but, bgrpid++);
        QAction *act = fltag->addAction(tr(but_txt.toUtf8()));
        m_filtMN->addAction(act);
        act->setCheckable(true);
        act->setData((int)(m_catgbutvec.size()-1));
        m_filtMN->connect(m_filtMN, SIGNAL(triggered(QAction *)), this, 
                          SLOT(catgFilter(QAction *)));
    }
    m_filtFRM->setLayout(bgrphbox);
    connect(m_filtBGRP, SIGNAL(buttonClicked(int)),this, SLOT(catgFilter(int)));
    connect(m_filtCMB, SIGNAL(activated(int)), this, SLOT(catgFilter(int)));

    restable = new ResTable(this);
    verticalLayout->insertWidget(2, restable);
    actionShowResultsAsTable->setChecked(prefs.showResultsAsTable);
    on_actionShowResultsAsTable_toggled(prefs.showResultsAsTable);

    // A shortcut to get the focus back to the search entry. 
    QKeySequence seq("Ctrl+Shift+s");
    QShortcut *sc = new QShortcut(seq, this);
    connect(sc, SIGNAL (activated()), sSearch, SLOT (takeFocus()));
    QKeySequence seql("Ctrl+l");
    sc = new QShortcut(seql, this);
    connect(sc, SIGNAL (activated()), sSearch, SLOT (takeFocus()));

    connect(&m_watcher, SIGNAL(fileChanged(QString)),
            this, SLOT(updateIdxStatus()));

    connect(sSearch,
            SIGNAL(startSearch(std::shared_ptr<Rcl::SearchData>, bool)), 
	    this, SLOT(startSearch(std::shared_ptr<Rcl::SearchData>, bool)));
    connect(sSearch, SIGNAL(setDescription(QString)), 
	    this, SLOT(onSetDescription(QString)));
    connect(sSearch, SIGNAL(clearSearch()), 
	    this, SLOT(resetSearch()));
    connect(preferencesMenu, SIGNAL(triggered(QAction*)),
	    this, SLOT(setStemLang(QAction*)));
    connect(preferencesMenu, SIGNAL(aboutToShow()),
	    this, SLOT(adjustPrefsMenu()));
    connect(fileExitAction, SIGNAL(triggered() ), 
	    this, SLOT(fileExit() ) );
    connect(fileToggleIndexingAction, SIGNAL(triggered()), 
	    this, SLOT(toggleIndexing()));
#ifndef _WIN32
    fileMenu->insertAction(fileRebuildIndexAction, fileBumpIndexingAction);
    connect(fileBumpIndexingAction, SIGNAL(triggered()), 
	    this, SLOT(bumpIndexing()));
#endif
    connect(fileRebuildIndexAction, SIGNAL(triggered()), 
	    this, SLOT(rebuildIndex()));
    connect(fileEraseDocHistoryAction, SIGNAL(triggered()), 
	    this, SLOT(eraseDocHistory()));
    connect(fileEraseSearchHistoryAction, SIGNAL(triggered()), 
	    this, SLOT(eraseSearchHistory()));
    connect(actionSave_last_query, SIGNAL(triggered()),
	    this, SLOT(saveLastQuery()));
    connect(actionLoad_saved_query, SIGNAL(triggered()),
	    this, SLOT(loadSavedQuery()));
    connect(actionShow_index_statistics, SIGNAL(triggered()),
            this, SLOT(showIndexStatistics()));
    connect(helpAbout_RecollAction, SIGNAL(triggered()), 
	    this, SLOT(showAboutDialog()));
    connect(showMissingHelpers_Action, SIGNAL(triggered()), 
	    this, SLOT(showMissingHelpers()));
    connect(showActiveTypes_Action, SIGNAL(triggered()), 
	    this, SLOT(showActiveTypes()));
    connect(userManualAction, SIGNAL(triggered()), 
	    this, SLOT(startManual()));
    connect(toolsDoc_HistoryAction, SIGNAL(triggered()), 
	    this, SLOT(showDocHistory()));
    connect(toolsAdvanced_SearchAction, SIGNAL(triggered()), 
	    this, SLOT(showAdvSearchDialog()));
    connect(toolsSpellAction, SIGNAL(triggered()), 
	    this, SLOT(showSpellDialog()));
    connect(actionWebcache_Editor, SIGNAL(triggered()),
            this, SLOT(showWebcacheDialog()));
    connect(actionQuery_Fragments, SIGNAL(triggered()), 
	    this, SLOT(showFragButs()));
    connect(actionSpecial_Indexing, SIGNAL(triggered()), 
	    this, SLOT(showSpecIdx()));
    connect(indexConfigAction, SIGNAL(triggered()), 
	    this, SLOT(showIndexConfig()));
    connect(indexScheduleAction, SIGNAL(triggered()), 
	    this, SLOT(showIndexSched()));
    connect(queryPrefsAction, SIGNAL(triggered()), 
	    this, SLOT(showUIPrefs()));
    connect(extIdxAction, SIGNAL(triggered()), 
	    this, SLOT(showExtIdxDialog()));
    connect(enbSynAction, SIGNAL(toggled(bool)),
            this, SLOT(setSynEnabled(bool)));

    connect(toggleFullScreenAction, SIGNAL(triggered()), 
            this, SLOT(toggleFullScreen()));
    connect(actionShowQueryDetails, SIGNAL(triggered()),
	    reslist, SLOT(showQueryDetails()));
    connect(periodictimer, SIGNAL(timeout()), 
	    this, SLOT(periodic100()));

    restable->setRclMain(this, true);
    connect(actionSaveResultsAsCSV, SIGNAL(triggered()), 
	    restable, SLOT(saveAsCSV()));
    connect(this, SIGNAL(docSourceChanged(std::shared_ptr<DocSequence>)),
	    restable, SLOT(setDocSource(std::shared_ptr<DocSequence>)));
    connect(this, SIGNAL(searchReset()), 
	    restable, SLOT(resetSource()));
    connect(this, SIGNAL(resultsReady()), 
	    restable, SLOT(readDocSource()));
    connect(this, SIGNAL(sortDataChanged(DocSeqSortSpec)), 
	    restable, SLOT(onSortDataChanged(DocSeqSortSpec)));

    connect(restable->getModel(), SIGNAL(sortDataChanged(DocSeqSortSpec)),
	    this, SLOT(onSortDataChanged(DocSeqSortSpec)));

    connect(restable, SIGNAL(docPreviewClicked(int, Rcl::Doc, int)), 
	    this, SLOT(startPreview(int, Rcl::Doc, int)));
    connect(restable, SIGNAL(docExpand(Rcl::Doc)), 
	    this, SLOT(docExpand(Rcl::Doc)));
    connect(restable, SIGNAL(showSubDocs(Rcl::Doc)), 
	    this, SLOT(showSubDocs(Rcl::Doc)));
    connect(restable, SIGNAL(openWithRequested(Rcl::Doc, string)), 
	    this, SLOT(openWith(Rcl::Doc, string)));

    reslist->setRclMain(this, true);
    connect(this, SIGNAL(docSourceChanged(std::shared_ptr<DocSequence>)),
	    reslist, SLOT(setDocSource(std::shared_ptr<DocSequence>)));
    connect(firstPageAction, SIGNAL(triggered()), 
	    reslist, SLOT(resultPageFirst()));
    connect(prevPageAction, SIGNAL(triggered()), 
	    reslist, SLOT(resPageUpOrBack()));
    connect(nextPageAction, SIGNAL(triggered()),
	    reslist, SLOT(resPageDownOrNext()));
    connect(this, SIGNAL(searchReset()), 
	    reslist, SLOT(resetList()));
    connect(this, SIGNAL(resultsReady()), 
	    reslist, SLOT(readDocSource()));

    connect(reslist, SIGNAL(hasResults(int)), 
	    this, SLOT(resultCount(int)));
    connect(reslist, SIGNAL(wordSelect(QString)),
	    sSearch, SLOT(addTerm(QString)));
    connect(reslist, SIGNAL(wordReplace(const QString&, const QString&)),
	    sSearch, SLOT(onWordReplace(const QString&, const QString&)));
    connect(reslist, SIGNAL(nextPageAvailable(bool)), 
	    this, SLOT(enableNextPage(bool)));
    connect(reslist, SIGNAL(prevPageAvailable(bool)), 
	    this, SLOT(enablePrevPage(bool)));

    connect(reslist, SIGNAL(docExpand(Rcl::Doc)), 
	    this, SLOT(docExpand(Rcl::Doc)));
    connect(reslist, SIGNAL(showSnippets(Rcl::Doc)), 
	    this, SLOT(showSnippets(Rcl::Doc)));
    connect(reslist, SIGNAL(showSubDocs(Rcl::Doc)), 
	    this, SLOT(showSubDocs(Rcl::Doc)));
    connect(reslist, SIGNAL(docSaveToFileClicked(Rcl::Doc)), 
	    this, SLOT(saveDocToFile(Rcl::Doc)));
    connect(reslist, SIGNAL(editRequested(Rcl::Doc)), 
	    this, SLOT(startNativeViewer(Rcl::Doc)));
    connect(reslist, SIGNAL(openWithRequested(Rcl::Doc, string)), 
	    this, SLOT(openWith(Rcl::Doc, string)));
    connect(reslist, SIGNAL(docPreviewClicked(int, Rcl::Doc, int)), 
	    this, SLOT(startPreview(int, Rcl::Doc, int)));
    connect(reslist, SIGNAL(previewRequested(Rcl::Doc)), 
	    this, SLOT(startPreview(Rcl::Doc)));

    setFilterCtlStyle(prefs.filterCtlStyle);

    if (prefs.keepSort && prefs.sortActive) {
	m_sortspec.field = (const char *)prefs.sortField.toUtf8();
	m_sortspec.desc = prefs.sortDesc;
	onSortDataChanged(m_sortspec);
	emit sortDataChanged(m_sortspec);
    }

    enableTrayIcon(prefs.showTrayIcon);

    fileRebuildIndexAction->setEnabled(false);
    fileToggleIndexingAction->setEnabled(false);
    fileRetryFailedAction->setEnabled(false);
    // Start timer on a slow period (used for checking ^C). Will be
    // speeded up during indexing
    periodictimer->start(1000);
}

void RclMain::enableTrayIcon(bool on)
{
    on =  on && QSystemTrayIcon::isSystemTrayAvailable();
    if (on) {
        if (nullptr == m_trayicon) {
            m_trayicon = new RclTrayIcon(this, 
                                         QIcon(QString(":/images/recoll.png")));
        }
        m_trayicon->show();
    } else {
        delete m_trayicon;
        m_trayicon = 0;
    }
}

void RclMain::setSynEnabled(bool on)
{
    prefs.synFileEnable = on;
    if (uiprefs)
        uiprefs->synFileCB->setChecked(prefs.synFileEnable);
}

void RclMain::resultCount(int n)
{
    actionSortByDateAsc->setEnabled(n>0);
    actionSortByDateDesc->setEnabled(n>0);
}

void RclMain::setFilterCtlStyle(int stl)
{
    switch (stl) {
    case PrefsPack::FCS_MN:
        setupResTB(false);
        m_filtFRM->setVisible(false);
        m_filtMN->menuAction()->setVisible(true);
        break;
    case PrefsPack::FCS_CMB:
        setupResTB(true);
        m_filtFRM->setVisible(false);
        m_filtMN->menuAction()->setVisible(false);
        break;
    case PrefsPack::FCS_BT:
    default:
        setupResTB(false);
        m_filtFRM->setVisible(true);
        m_filtMN->menuAction()->setVisible(false);
    }
}

// Set up the "results" toolbox, adding the filter combobox or not depending
// on config option
void RclMain::setupResTB(bool combo)
{
    m_resTB->clear();
    m_resTB->addAction(firstPageAction);
    m_resTB->addAction(prevPageAction);
    m_resTB->addAction(nextPageAction);
    m_resTB->addSeparator();
    m_resTB->addAction(actionSortByDateAsc);
    m_resTB->addAction(actionSortByDateDesc);
    if (combo) {
        m_resTB->addSeparator();
        m_filtCMB->show();
        m_resTB->addWidget(m_filtCMB);
    } else {
        m_filtCMB->hide();
    }
    m_resTB->addSeparator();
    m_resTB->addAction(actionShowResultsAsTable);
}

// This is called by a timer right after we come up. Try to open
// the database and talk to the user if we can't
void RclMain::initDbOpen()
{
    bool nodb = false;
    string reason;
    bool maindberror;
    if (!maybeOpenDb(reason, true, &maindberror)) {
        nodb = true;
	if (maindberror) {
	    FirstIdxDialog fidia(this);
	    connect(fidia.idxconfCLB, SIGNAL(clicked()), 
		    this, SLOT(execIndexConfig()));
	    connect(fidia.idxschedCLB, SIGNAL(clicked()), 
		    this, SLOT(execIndexSched()));
	    connect(fidia.runidxPB, SIGNAL(clicked()), 
		    this, SLOT(rebuildIndex()));
	    fidia.exec();
	    // Don't open adv search or run cmd line search in this case.
	    return;
	} else {
	    QMessageBox::warning(0, "Recoll", 
				 tr("Could not open external index. Db not open. Check external indexes list."));
	}
    }

    if (prefs.startWithAdvSearchOpen)
	showAdvSearchDialog();
    // If we have something in the search entry, it comes from a
    // command line argument
    if (!nodb && sSearch->hasSearchString())
	QTimer::singleShot(0, sSearch, SLOT(startSimpleSearch()));

    if (!m_urltoview.isEmpty()) 
	viewUrl();
}

void RclMain::setStemLang(QAction *id)
{
    LOGDEB("RclMain::setStemLang(" << id << ")\n");
    // Check that the menu entry is for a stemming language change
    // (might also be "show prefs" etc.
    bool isLangId = false;
    for (map<QString, QAction*>::const_iterator it = m_stemLangToId.begin();
	 it != m_stemLangToId.end(); it++) {
	if (id == it->second)
	    isLangId = true;
    }
    if (!isLangId)
	return;

    // Set the "checked" item state for lang entries
    for (map<QString, QAction*>::const_iterator it = m_stemLangToId.begin();
	 it != m_stemLangToId.end(); it++) {
	(it->second)->setChecked(false);
    }
    id->setChecked(true);

    // Retrieve language value (also handle special cases), set prefs,
    // notify that we changed
    QString lang;
    if (id == m_idNoStem) {
	lang = "";
    } else if (id == m_idAllStem) {
	lang = "ALL";
    } else {
	lang = id->text();
    }
    prefs.queryStemLang = lang;
    LOGDEB("RclMain::setStemLang(" << id << "): lang [" <<
           qs2utf8s(prefs.queryStemLang) << "]\n");
    rwSettings(true);
    emit stemLangChanged(lang);
}

// Set the checked stemming language item before showing the prefs menu
void RclMain::setStemLang(const QString& lang)
{
    LOGDEB("RclMain::setStemLang(" << qs2utf8s(lang) << ")\n");
    QAction *id;
    if (lang == "") {
	id = m_idNoStem;
    } else if (lang == "ALL") {
	id = m_idAllStem;
    } else {
	map<QString, QAction*>::iterator it = m_stemLangToId.find(lang);
	if (it == m_stemLangToId.end()) 
	    return;
	id = it->second;
    }
    for (map<QString, QAction*>::const_iterator it = m_stemLangToId.begin();
	 it != m_stemLangToId.end(); it++) {
	(it->second)->setChecked(false);
    }
    id->setChecked(true);
}

// Prefs menu about to show
void RclMain::adjustPrefsMenu()
{
    setStemLang(prefs.queryStemLang);
}

void RclMain::showTrayMessage(const QString& text)
{
    if (m_trayicon && prefs.trayMessages)
        m_trayicon->showMessage("Recoll", text, 
                                QSystemTrayIcon::Information, 1000);
}

void RclMain::closeEvent(QCloseEvent *ev)
{
    LOGDEB("RclMain::closeEvent\n");
    if (isFullScreen()) {
        prefs.showmode = PrefsPack::SHOW_FULL;
    } else if (isMaximized()) {
        prefs.showmode = PrefsPack::SHOW_MAX;
    } else {
        prefs.showmode = PrefsPack::SHOW_NORMAL;
    }
    if (prefs.closeToTray && m_trayicon && m_trayicon->isVisible()) {
        hide();
        ev->ignore();
    } else {
        fileExit();
    }
}

void RclMain::fileExit()
{
    LOGDEB("RclMain: fileExit\n");
    // Have to do this both in closeEvent (for close to tray) and fileExit
    // (^Q, doesnt go through closeEvent)
    if (isFullScreen()) {
        prefs.showmode = PrefsPack::SHOW_FULL;
    } else if (isMaximized()) {
        prefs.showmode = PrefsPack::SHOW_MAX;
    } else {
        prefs.showmode = PrefsPack::SHOW_NORMAL;
    }
    if (m_trayicon) {
        m_trayicon->setVisible(false);
    }
    // Don't save geometry if we're currently fullscreened
    if (!isFullScreen() && !isMaximized()) {
        prefs.mainwidth = width();
        prefs.mainheight = height();
    }
    
    prefs.toolArea = toolBarArea(m_toolsTB);
    prefs.resArea = toolBarArea(m_resTB);
    restable->saveColState();

    if (prefs.ssearchTypSav) {
	prefs.ssearchTyp = sSearch->searchTypCMB->currentIndex();
    }

    rwSettings(true);

    // We should do the right thing and let exit() call all the
    // cleanup handlers. But we have few persistent resources and qt
    // exit is a great source of crashes and pita. So do our own
    // cleanup:
    deleteAllTempFiles();
    // and scram out
    _Exit(0);
}

// Start a db query and set the reslist docsource
void RclMain::startSearch(std::shared_ptr<Rcl::SearchData> sdata, bool issimple)
{
    LOGDEB("RclMain::startSearch. Indexing " << (m_idxproc?"on":"off") <<
           " Active " << m_queryActive << "\n");
    if (m_queryActive) {
	LOGDEB("startSearch: already active\n");
	return;
    }
    m_queryActive = true;
    restable->setEnabled(false);
    m_source = std::shared_ptr<DocSequence>();

    m_searchIsSimple = issimple;

    // The db may have been closed at the end of indexing
    string reason;
    // If indexing is being performed, we reopen the db at each query.
    if (!maybeOpenDb(reason, m_idxproc != 0)) {
	QMessageBox::critical(0, "Recoll", QString(reason.c_str()));
	m_queryActive = false;
        restable->setEnabled(true);
	return;
    }

    if (prefs.synFileEnable && !prefs.synFile.isEmpty()) {
        string sf = (const char *)prefs.synFile.toLocal8Bit();
        if (!rcldb->setSynGroupsFile(sf)) {
            QMessageBox::warning(0, "Recoll",
                                 tr("Can't set synonyms file (parse error?)"));
            return;
        }
    } else {
        rcldb->setSynGroupsFile("");
    }

    Rcl::Query *query = new Rcl::Query(rcldb.get());
    query->setCollapseDuplicates(prefs.collapseDuplicates);

    curPreview = 0;
    DocSequenceDb *src = 
	new DocSequenceDb(rcldb, std::shared_ptr<Rcl::Query>(query), 
			  string(tr("Query results").toUtf8()), sdata);
    src->setAbstractParams(prefs.queryBuildAbstract, 
                           prefs.queryReplaceAbstract);
    m_source = std::shared_ptr<DocSequence>(src);
    m_source->setSortSpec(m_sortspec);
    m_source->setFiltSpec(m_filtspec);

    emit docSourceChanged(m_source);
    emit sortDataChanged(m_sortspec);
    initiateQuery();
}

class QueryThread : public QThread {
    std::shared_ptr<DocSequence> m_source;
 public: 
    QueryThread(std::shared_ptr<DocSequence> source)
	: m_source(source)
    {
    }
    ~QueryThread() { }
    virtual void run() 
    {
	cnt = m_source->getResCnt();
    }
    int cnt;
};

void RclMain::initiateQuery()
{
    if (!m_source)
	return;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    QueryThread qthr(m_source);
    qthr.start();

    QProgressDialog progress(this);
    progress.setLabelText(tr("Query in progress.<br>"
			     "Due to limitations of the indexing library,<br>"
			     "cancelling will exit the program"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setRange(0,0);

    // For some reason setMinimumDuration() does not seem to work with
    // a busy dialog (range 0,0) Have to call progress.show() inside
    // the loop.
    // progress.setMinimumDuration(2000);
    // Also the multiple processEvents() seem to improve the responsiveness??
    for (int i = 0;;i++) {
	qApp->processEvents();
	if (qthr.wait(100)) {
	    break;
	}
	if (i == 20)
	    progress.show();
	qApp->processEvents();
	if (progress.wasCanceled()) {
	    // Just get out of there asap. 
	    exit(1);
	}

	qApp->processEvents();
    }

    int cnt = qthr.cnt;
    QString msg;
    if (cnt > 0) {
	QString str;
	msg = tr("Result count (est.)") + ": " + 
	    str.setNum(cnt);
    } else {
	msg = tr("No results found");
    }

    statusBar()->showMessage(msg, 0);
    QApplication::restoreOverrideCursor();
    m_queryActive = false;
    restable->setEnabled(true);
    emit(resultsReady());
}

void RclMain::resetSearch()
{
    m_source = std::shared_ptr<DocSequence>();
    emit searchReset();
}

void RclMain::onSortCtlChanged()
{
    if (m_sortspecnochange)
	return;

    LOGDEB("RclMain::onSortCtlChanged()\n");
    m_sortspec.reset();
    if (actionSortByDateAsc->isChecked()) {
	m_sortspec.field = "mtime";
	m_sortspec.desc = false;
	prefs.sortActive = true;
	prefs.sortDesc = false;
	prefs.sortField = "mtime";
    } else if (actionSortByDateDesc->isChecked()) {
	m_sortspec.field = "mtime";
	m_sortspec.desc = true;
	prefs.sortActive = true;
	prefs.sortDesc = true;
	prefs.sortField = "mtime";
    } else {
	prefs.sortActive = prefs.sortDesc = false;
	prefs.sortField = "";
    }
    if (m_source)
	m_source->setSortSpec(m_sortspec);
    emit sortDataChanged(m_sortspec);
    initiateQuery();
}

void RclMain::onSortDataChanged(DocSeqSortSpec spec)
{
    LOGDEB("RclMain::onSortDataChanged\n");
    m_sortspecnochange = true;
    if (spec.field.compare("mtime")) {
	actionSortByDateDesc->setChecked(false);
	actionSortByDateAsc->setChecked(false);
    } else {
	actionSortByDateDesc->setChecked(spec.desc);
	actionSortByDateAsc->setChecked(!spec.desc);
    }
    m_sortspecnochange = false;
    if (m_source)
	m_source->setSortSpec(spec);
    m_sortspec = spec;

    prefs.sortField = QString::fromUtf8(spec.field.c_str());
    prefs.sortDesc = spec.desc;
    prefs.sortActive = !spec.field.empty();

    initiateQuery();
}

void RclMain::on_actionShowResultsAsTable_toggled(bool on)
{
    LOGDEB("RclMain::on_actionShowResultsAsTable_toggled(" << on << ")\n");
    prefs.showResultsAsTable = on;
    displayingTable = on;
    restable->setVisible(on);
    reslist->setVisible(!on);
    actionSaveResultsAsCSV->setEnabled(on);
    static QShortcut tablefocseq(QKeySequence("Ctrl+r"), this);
    if (!on) {
	int docnum = restable->getDetailDocNumOrTopRow();
	if (docnum >= 0) {
            reslist->resultPageFor(docnum);
        }
        disconnect(&tablefocseq, SIGNAL(activated()),
                   restable, SLOT(takeFocus()));
        sSearch->takeFocus();
    } else {
	int docnum = reslist->pageFirstDocNum();
	if (docnum >= 0) {
	    restable->makeRowVisible(docnum);
	}
	nextPageAction->setEnabled(false);
	prevPageAction->setEnabled(false);
	firstPageAction->setEnabled(false);
        connect(&tablefocseq, SIGNAL(activated()), 
                restable, SLOT(takeFocus()));
    }
}

void RclMain::on_actionSortByDateAsc_toggled(bool on)
{
    LOGDEB("RclMain::on_actionSortByDateAsc_toggled(" << on << ")\n");
    if (on) {
	if (actionSortByDateDesc->isChecked()) {
	    actionSortByDateDesc->setChecked(false);
	    // Let our buddy work.
	    return;
	}
    }
    onSortCtlChanged();
}

void RclMain::on_actionSortByDateDesc_toggled(bool on)
{
    LOGDEB("RclMain::on_actionSortByDateDesc_toggled(" << on << ")\n");
    if (on) {
	if (actionSortByDateAsc->isChecked()) {
	    actionSortByDateAsc->setChecked(false);
	    // Let our buddy work.
	    return;
	}
    }
    onSortCtlChanged();
}

void RclMain::saveDocToFile(Rcl::Doc doc)
{
    QString s = 
	QFileDialog::getSaveFileName(this, //parent
				     tr("Save file"), 
				     QString::fromLocal8Bit(path_home().c_str())
	    );
    string tofile((const char *)s.toLocal8Bit());
    TempFile temp; // not used because tofile is set.
    if (!FileInterner::idocToFile(temp, tofile, theconfig, doc)) {
	QMessageBox::warning(0, "Recoll",
			     tr("Cannot extract document or create "
				"temporary file"));
	return;
    }
}

void RclMain::showSubDocs(Rcl::Doc doc)
{
    LOGDEB("RclMain::showSubDocs\n");
    string reason;
    if (!maybeOpenDb(reason)) {
	QMessageBox::critical(0, "Recoll", QString(reason.c_str()));
	return;
    }
    vector<Rcl::Doc> docs;
    if (!rcldb->getSubDocs(doc, docs)) {
	QMessageBox::warning(0, "Recoll", QString("Can't get subdocs"));
	return;
    }	
    DocSequenceDocs *src = 
	new DocSequenceDocs(rcldb, docs,
			    qs2utf8s(tr("Sub-documents and attachments")));
    src->setDescription(qs2utf8s(tr("Sub-documents and attachments")));
    std::shared_ptr<DocSequence> 
	source(new DocSource(theconfig, std::shared_ptr<DocSequence>(src)));

    ResTable *res = new ResTable();
    res->setRclMain(this, false);
    res->setDocSource(source);
    res->readDocSource();
    res->show();
}

// Search for document 'like' the selected one. We ask rcldb/xapian to find
// significant terms, and add them to the simple search entry.
void RclMain::docExpand(Rcl::Doc doc)
{
    LOGDEB("RclMain::docExpand()\n");
    if (!rcldb)
	return;
    list<string> terms;

    terms = m_source->expand(doc);
    if (terms.empty()) {
	LOGDEB("RclMain::docExpand: no terms\n");
	return;
    }
    // Do we keep the original query. I think we'd better not.
    // rcldb->expand is set to keep the original query terms instead.
    QString text;// = sSearch->queryText->currentText();
    for (list<string>::iterator it = terms.begin(); it != terms.end(); it++) {
	text += QString::fromLatin1(" \"") +
	    QString::fromUtf8((*it).c_str()) + QString::fromLatin1("\"");
    }
    // We need to insert item here, its not auto-done like when the user types
    // CR
    sSearch->setSearchString(text);
    sSearch->setAnyTermMode();
    sSearch->startSimpleSearch();
}

void RclMain::showDocHistory()
{
    LOGDEB("RclMain::showDocHistory\n");
    resetSearch();
    curPreview = 0;

    string reason;
    if (!maybeOpenDb(reason)) {
	QMessageBox::critical(0, "Recoll", QString(reason.c_str()));
	return;
    }
    // Construct a bogus SearchData structure
    std::shared_ptr<Rcl::SearchData>searchdata = 
	std::shared_ptr<Rcl::SearchData>(new Rcl::SearchData(Rcl::SCLT_AND, cstr_null));
    searchdata->setDescription((const char *)tr("History data").toUtf8());


    // If you change the title, also change it in eraseDocHistory()
    DocSequenceHistory *src = 
	new DocSequenceHistory(rcldb, g_dynconf, 
			       string(tr("Document history").toUtf8()));
    src->setDescription((const char *)tr("History data").toUtf8());
    DocSource *source = new DocSource(theconfig, std::shared_ptr<DocSequence>(src));
    m_source = std::shared_ptr<DocSequence>(source);
    m_source->setSortSpec(m_sortspec);
    m_source->setFiltSpec(m_filtspec);
    emit docSourceChanged(m_source);
    emit sortDataChanged(m_sortspec);
    initiateQuery();
}

// Erase all memory of documents viewed
void RclMain::eraseDocHistory()
{
    // Clear file storage
    if (g_dynconf)
	g_dynconf->eraseAll(docHistSubKey);
    // Clear possibly displayed history
    if (reslist->displayingHistory()) {
	showDocHistory();
    }
}

void RclMain::eraseSearchHistory()
{
    prefs.ssearchHistory.clear();
    if (sSearch)
	sSearch->clearAll();
    if (g_advshistory)
	g_advshistory->clear();
}

// Called when the uiprefs dialog is ok'd
void RclMain::setUIPrefs()
{
    if (!uiprefs)
	return;
    LOGDEB("Recollmain::setUIPrefs\n");
    reslist->setFont();
    sSearch->setPrefs();
    enbSynAction->setDisabled(prefs.synFile.isEmpty());
    enbSynAction->setChecked(prefs.synFileEnable);
}

void RclMain::enableNextPage(bool yesno)
{
    if (!displayingTable)
	nextPageAction->setEnabled(yesno);
}

void RclMain::enablePrevPage(bool yesno)
{
    if (!displayingTable) {
	prevPageAction->setEnabled(yesno);
	firstPageAction->setEnabled(yesno);
    }
}

void RclMain::onSetDescription(QString desc)
{
    m_queryDescription = desc;
}

QString RclMain::getQueryDescription()
{
    if (!m_source)
	return "";
    return m_queryDescription.isEmpty() ?
        u8s2qs(m_source->getDescription()) : m_queryDescription;
}

// Set filter, action style
void RclMain::catgFilter(QAction *act)
{
    int id = act->data().toInt();
    catgFilter(id);
}

// User pressed a filter button: set filter params in reslist
void RclMain::catgFilter(int id)
{
    LOGDEB("RclMain::catgFilter: id " << id << "\n");
    if (id < 0 || id >= int(m_catgbutvec.size()))
	return; 

    switch (prefs.filterCtlStyle) {
    case PrefsPack::FCS_MN:
        m_filtCMB->setCurrentIndex(id);
        m_filtBGRP->buttons()[id]->setChecked(true);
        break;
    case PrefsPack::FCS_CMB:
        m_filtBGRP->buttons()[id]->setChecked(true);
        m_filtMN->actions()[id]->setChecked(true);
        break;
    case PrefsPack::FCS_BT:
    default:
        m_filtCMB->setCurrentIndex(id);
        m_filtMN->actions()[id]->setChecked(true);
    }

    m_catgbutvecidx = id;
    setFiltSpec();
}

void RclMain::setFiltSpec()
{
    m_filtspec.reset();

    // "Category" buttons
    if (m_catgbutvecidx != 0)  {
	string catg = m_catgbutvec[m_catgbutvecidx];
	string frag;
	theconfig->getGuiFilter(catg, frag);
	m_filtspec.orCrit(DocSeqFiltSpec::DSFS_QLANG, frag);
    }

    // Fragments from the fragbuts buttonbox tool
    if (fragbuts) {
        vector<string> frags;
        fragbuts->getfrags(frags);
        for (vector<string>::const_iterator it = frags.begin();
             it != frags.end(); it++) {
            m_filtspec.orCrit(DocSeqFiltSpec::DSFS_QLANG, *it);
        }
    }

    if (m_source)
	m_source->setFiltSpec(m_filtspec);
    initiateQuery();
}

void RclMain::onFragmentsChanged()
{
    setFiltSpec();
}

void RclMain::toggleFullScreen()
{
    if (isFullScreen())
        showNormal();
    else
        showFullScreen();
}

void RclMain::showEvent(QShowEvent *ev)
{
    sSearch->takeFocus();
    QMainWindow::showEvent(ev);
}

void RclMain::applyStyleSheet()
{
    ::applyStyleSheet(prefs.qssFile);
}
