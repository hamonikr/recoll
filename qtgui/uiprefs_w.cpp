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

#include "safesysstat.h"

#include <string>
#include <algorithm>
#include <list>

#include <qfontdialog.h>
#include <qspinbox.h>
#include <qmessagebox.h>
#include <qvariant.h>
#include <qpushbutton.h>
#include <qtabwidget.h>
#include <qlistwidget.h>
#include <qwidget.h>
#include <qlabel.h>
#include <qspinbox.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qtextedit.h>
#include <qlist.h>
#include <QTimer>
#include <QListWidget>
#include <QSettings>
#include <QKeySequenceEdit>
#include <QKeySequence>

#include "recoll.h"
#include "guiutils.h"
#include "rclconfig.h"
#include "pathut.h"
#include "uiprefs_w.h"
#include "viewaction_w.h"
#include "log.h"
#include "editdialog.h"
#include "rclmain_w.h"
#include "ptrans_w.h"
#include "scbase.h"
#include "rclhelp.h"

void UIPrefsDialog::init()
{
    // See enum above and keep in order !
    ssearchTypCMB->addItem(tr("Any term"));
    ssearchTypCMB->addItem(tr("All terms"));
    ssearchTypCMB->addItem(tr("File name"));
    ssearchTypCMB->addItem(tr("Query language"));
    ssearchTypCMB->addItem(tr("Value from previous program exit"));

    connect(viewActionPB, SIGNAL(clicked()), this, SLOT(showViewAction()));
    connect(reslistFontPB, SIGNAL(clicked()), this, SLOT(showFontDialog()));
    connect(resetFontPB, SIGNAL(clicked()), this, SLOT(resetReslistFont()));

    connect(stylesheetPB, SIGNAL(clicked()),this, SLOT(showStylesheetDialog()));
    connect(resetSSPB, SIGNAL(clicked()), this, SLOT(resetStylesheet()));
    connect(darkSSPB, SIGNAL(clicked()), this, SLOT(setDarkMode()));
    connect(snipCssPB, SIGNAL(clicked()),this, SLOT(showSnipCssDialog()));
    connect(synFilePB, SIGNAL(clicked()),this, SLOT(showSynFileDialog()));
    connect(resetSnipCssPB, SIGNAL(clicked()), this, SLOT(resetSnipCss()));

    connect(idxLV, SIGNAL(itemSelectionChanged()),
            this, SLOT(extradDbSelectChanged()));
    connect(ptransPB, SIGNAL(clicked()),
            this, SLOT(extraDbEditPtrans()));
    connect(addExtraDbPB, SIGNAL(clicked()), 
            this, SLOT(addExtraDbPB_clicked()));
    connect(delExtraDbPB, SIGNAL(clicked()), 
            this, SLOT(delExtraDbPB_clicked()));
    connect(togExtraDbPB, SIGNAL(clicked()), 
            this, SLOT(togExtraDbPB_clicked()));
    connect(actAllExtraDbPB, SIGNAL(clicked()), 
            this, SLOT(actAllExtraDbPB_clicked()));
    connect(unacAllExtraDbPB, SIGNAL(clicked()), 
            this, SLOT(unacAllExtraDbPB_clicked()));
    connect(CLEditPara, SIGNAL(clicked()), this, SLOT(editParaFormat()));
    connect(CLEditHeader, SIGNAL(clicked()), this, SLOT(editHeaderText()));
    connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
    connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(buildAbsCB, SIGNAL(toggled(bool)), 
            replAbsCB, SLOT(setEnabled(bool)));
    connect(ssNoCompleteCB, SIGNAL(toggled(bool)), 
            ssSearchOnCompleteCB, SLOT(setDisabled(bool)));
    connect(resetscPB, SIGNAL(clicked()), this, SLOT(resetShortcuts()));

    (void)new HelpClient(this);
    HelpClient::installMap("sctab", "RCL.SEARCH.GUI.SHORTCUTS");
    
    setFromPrefs();
}

// Update dialog state from stored prefs
void UIPrefsDialog::setFromPrefs()
{
    // Most values are stored in the prefs struct. Some rarely used
    // ones go directly through the settings
    QSettings settings;

    // Entries per result page spinbox
    pageLenSB->setValue(prefs.respagesize);
    maxHistSizeSB->setValue(prefs.historysize);
    collapseDupsCB->setChecked(prefs.collapseDuplicates);
    maxHLTSB->setValue(prefs.maxhltextkbs);

    if (prefs.ssearchTypSav) {
        ssearchTypCMB->setCurrentIndex(4);
    } else {
        ssearchTypCMB->setCurrentIndex(prefs.ssearchTyp);
    }
    
    switch (prefs.filterCtlStyle) {
    case PrefsPack::FCS_MN:
        filterMN_RB->setChecked(1);
        break;
    case PrefsPack::FCS_CMB:
        filterCMB_RB->setChecked(1);
        break;
    case PrefsPack::FCS_BT:
    default:
        filterBT_RB->setChecked(1);
        break;
    }
    noBeepsCB->setChecked(prefs.noBeeps);
    ssNoCompleteCB->setChecked(prefs.ssearchNoComplete);
    ssSearchOnCompleteCB->setChecked(prefs.ssearchStartOnComplete);
    ssSearchOnCompleteCB->setEnabled(!prefs.ssearchNoComplete);
    
    syntlenSB->setValue(prefs.syntAbsLen);
    syntctxSB->setValue(prefs.syntAbsCtx);

    initStartAdvCB->setChecked(prefs.startWithAdvSearchOpen);

    keepSortCB->setChecked(prefs.keepSort);

    noToolbarsCB->setChecked(prefs.noToolbars);
    noClearSearchCB->setChecked(prefs.noClearSearch);
    noStatusBarCB->setChecked(prefs.noStatusBar);
    noMenuBarCB->setChecked(prefs.noMenuBar);
    noSSTypCMBCB->setChecked(prefs.noSSTypCMB);
    restabShowTxtNoShiftRB->setChecked(prefs.resTableTextNoShift);
    restabShowTxtShiftRB->setChecked(!prefs.resTableTextNoShift);
    resTableNoHoverMetaCB->setChecked(prefs.resTableNoHoverMeta);
    noResTableHeaderCB->setChecked(prefs.noResTableHeader);
    showResTableVHeaderCB->setChecked(prefs.showResTableVHeader);
    noRowJumpShortcutsCB->setChecked(prefs.noResTableRowJumpSC);
    showTrayIconCB->setChecked(prefs.showTrayIcon);
    if (!prefs.showTrayIcon) {
        prefs.closeToTray = false;
        prefs.trayMessages = false;
    }
    closeToTrayCB->setEnabled(showTrayIconCB->checkState());
    trayMessagesCB->setEnabled(showTrayIconCB->checkState());
    closeToTrayCB->setChecked(prefs.closeToTray);
    trayMessagesCB->setChecked(prefs.trayMessages);
    
    // See qxtconfirmationmessage. Needs to be -1 for the dialog to show.
    showTempFileWarningCB->setChecked(prefs.showTempFileWarning == -1);
    anchorTamilHackCB->setChecked(settings.value("anchorSpcHack", 0).toBool());
    previewHtmlCB->setChecked(prefs.previewHtml);
    previewActiveLinksCB->setChecked(prefs.previewActiveLinks);
    switch (prefs.previewPlainPre) {
    case PrefsPack::PP_BR:
        plainBRRB->setChecked(1);
        break;
    case PrefsPack::PP_PRE:
        plainPRERB->setChecked(1);
        break;
    case PrefsPack::PP_PREWRAP:
    default:
        plainPREWRAPRB->setChecked(1);
        break;
    }
    // Query terms color
    qtermStyleCMB->setCurrentText(prefs.qtermstyle);
    if (qtermStyleCMB->count() <=1) {
        qtermStyleCMB->addItem(prefs.qtermstyle);
        qtermStyleCMB->addItem("color: blue");
        qtermStyleCMB->addItem("color: red;background: yellow");
        qtermStyleCMB->addItem(
            "color: #dddddd; background: black; font-weight: bold");
    }    
    // Abstract snippet separator string
    abssepLE->setText(prefs.abssep);
    dateformatLE->setText(u8s2qs(prefs.reslistdateformat));

    // Result list font family and size
    reslistFontFamily = prefs.reslistfontfamily;
    reslistFontSize = prefs.reslistfontsize;
    setupReslistFontPB();

    // Style sheet
    qssFile = prefs.qssFile;
    if (qssFile.isEmpty()) {
        stylesheetPB->setText(tr("Choose"));
    } else {
        string nm = path_getsimple(qs2path(qssFile));
        stylesheetPB->setText(path2qs(nm));
    }
    darkMode = prefs.darkMode;

    snipCssFile = prefs.snipCssFile;
    if (snipCssFile.isEmpty()) {
        snipCssPB->setText(tr("Choose"));
    } else {
        string nm = path_getsimple(qs2path(snipCssFile));
        snipCssPB->setText(path2qs(nm));
    }
    snipwMaxLenSB->setValue(prefs.snipwMaxLength);
    snipwByPageCB->setChecked(prefs.snipwSortByPage);
    alwaysSnippetsCB->setChecked(prefs.alwaysSnippets);
    paraFormat = prefs.reslistformat;
    headerText = prefs.reslistheadertext;

    // Stemming language combobox
    stemLangCMB->clear();
    stemLangCMB->addItem(g_stringNoStem);
    stemLangCMB->addItem(g_stringAllStem);
    vector<string> langs;
    if (!getStemLangs(langs)) {
        QMessageBox::warning(0, "Recoll", 
                             tr("error retrieving stemming languages"));
    }
    int cur = prefs.queryStemLang == ""  ? 0 : 1;
    for (vector<string>::const_iterator it = langs.begin(); 
         it != langs.end(); it++) {
        stemLangCMB->
            addItem(QString::fromUtf8(it->c_str(), it->length()));
        if (cur == 0 && !strcmp((const char*)prefs.queryStemLang.toUtf8(), 
                                it->c_str())) {
            cur = stemLangCMB->count();
        }
    }
    stemLangCMB->setCurrentIndex(cur);

    autoPhraseCB->setChecked(prefs.ssearchAutoPhrase);
    autoPThreshSB->setValue(prefs.ssearchAutoPhraseThreshPC);

    buildAbsCB->setChecked(prefs.queryBuildAbstract);
    replAbsCB->setEnabled(prefs.queryBuildAbstract);
    replAbsCB->setChecked(prefs.queryReplaceAbstract);

    autoSuffsCB->setChecked(prefs.autoSuffsEnable);
    autoSuffsLE->setText(prefs.autoSuffs);

    synFile = prefs.synFile;
    if (synFile.isEmpty()) {
        synFileCB->setChecked(false);
        synFileCB->setEnabled(false);
        synFilePB->setText(tr("Choose"));
    } else {
        synFileCB->setChecked(prefs.synFileEnable);
        synFileCB->setEnabled(true);
        string nm = path_getsimple(qs2path(synFile));
        synFilePB->setText(path2qs(nm));
    }

    // Initialize the extra indexes listboxes
    idxLV->clear();
    for (const auto& dbdir : prefs.allExtraDbs) {
        QListWidgetItem *item = 
            new QListWidgetItem(path2qs(dbdir), idxLV);
        if (item) 
            item->setCheckState(Qt::Unchecked);
    }
    for (const auto& dbdir : prefs.activeExtraDbs) {
        auto items =
            idxLV->findItems(path2qs(dbdir), 
                             Qt::MatchFixedString|Qt::MatchCaseSensitive);
        for (auto& entry : items) {
            entry->setCheckState(Qt::Checked);
        }
    }
    idxLV->sortItems();
    readShortcuts();
    setSSButState();
}

void UIPrefsDialog::readShortcutsInternal(const QStringList& sl)
{
    shortcutsTB->setRowCount(0);
    shortcutsTB->setColumnCount(4);
    shortcutsTB->setHorizontalHeaderItem(
        0, new QTableWidgetItem(tr("Context")));
    shortcutsTB->setHorizontalHeaderItem(
        1, new QTableWidgetItem(tr("Description")));
    shortcutsTB->setHorizontalHeaderItem(
        2, new QTableWidgetItem(tr("Shortcut")));
    shortcutsTB->setHorizontalHeaderItem(
        3, new QTableWidgetItem(tr("Default")));
    int row = 0;
    m_scids.clear();
    for (int i = 0; i < sl.size();) {
        LOGDEB0("UIPrefsDialog::readShortcuts: inserting row " <<
                qs2utf8s(sl.at(i)) << " " << qs2utf8s(sl.at(i+1)) << " " <<
                qs2utf8s(sl.at(i+2)) << " " << qs2utf8s(sl.at(i+3)) << "\n");
        shortcutsTB->insertRow(row);
        m_scids.push_back(sl.at(i++));
        shortcutsTB->setItem(row, 0, new QTableWidgetItem(sl.at(i++)));
        shortcutsTB->setItem(row, 1, new QTableWidgetItem(sl.at(i++)));
        auto ed = new QKeySequenceEdit(QKeySequence(sl.at(i++)));
        shortcutsTB->setCellWidget(row, 2, ed);
        shortcutsTB->setItem(row, 3, new QTableWidgetItem(sl.at(i++)));
        row++;
    }
    shortcutsTB->resizeColumnsToContents();
    shortcutsTB->horizontalHeader()->setStretchLastSection(true);
}

void UIPrefsDialog::readShortcuts()
{
    readShortcutsInternal(SCBase::scBase().getAll());
}

void UIPrefsDialog::resetShortcuts()
{
    readShortcutsInternal(SCBase::scBase().getAllDefaults());
}

void UIPrefsDialog::storeShortcuts()
{
    SCBase& scbase = SCBase::scBase();
    QStringList slout;
    for (int row = 0; row < shortcutsTB->rowCount(); row++) {
        QString dflt = shortcutsTB->item(row, 0)->text();
        QString ctxt = shortcutsTB->item(row, 1)->text();
        auto qsce = (QKeySequenceEdit*)(shortcutsTB->cellWidget(row, 2));
        QString val = qsce->keySequence().toString();
        scbase.set(m_scids[row], dflt, ctxt, val);
    }
    scbase.store();
}

void UIPrefsDialog::setupReslistFontPB()
{
    QString s;
    if (reslistFontFamily.length() == 0) {
        reslistFontPB->setText(tr("Default QtWebkit font"));
    } else {
        reslistFontPB->setText(reslistFontFamily + "-" +
                               s.setNum(reslistFontSize));
    }
}

void UIPrefsDialog::accept()
{
    // Most values are stored in the prefs struct. Some rarely used
    // ones go directly through the settings
    QSettings settings;
    prefs.noBeeps = noBeepsCB->isChecked();
    prefs.ssearchNoComplete = ssNoCompleteCB->isChecked();
    prefs.ssearchStartOnComplete = ssSearchOnCompleteCB->isChecked();

    if (ssearchTypCMB->currentIndex() == 4) {
        prefs.ssearchTypSav = true;
        // prefs.ssearchTyp will be set from the current value when
        // exiting the program
    } else {
        prefs.ssearchTypSav = false;
        prefs.ssearchTyp = ssearchTypCMB->currentIndex();
    }

    if (filterMN_RB->isChecked()) {
        prefs.filterCtlStyle = PrefsPack::FCS_MN;
    } else if (filterCMB_RB->isChecked()) {
        prefs.filterCtlStyle = PrefsPack::FCS_CMB;
    } else {
        prefs.filterCtlStyle = PrefsPack::FCS_BT;
    }
    m_mainWindow->setFilterCtlStyle(prefs.filterCtlStyle);

    prefs.respagesize = pageLenSB->value();
    prefs.historysize = maxHistSizeSB->value();
    prefs.collapseDuplicates = collapseDupsCB->isChecked();
    prefs.maxhltextkbs = maxHLTSB->value();

    prefs.qtermstyle = qtermStyleCMB->currentText();
    prefs.abssep = abssepLE->text();
    prefs.reslistdateformat = qs2utf8s(dateformatLE->text());

    prefs.reslistfontfamily = reslistFontFamily;
    prefs.reslistfontsize = reslistFontSize;
    prefs.darkMode = darkMode;
    prefs.setupDarkCSS();
    prefs.qssFile = qssFile;
    QTimer::singleShot(0, m_mainWindow, SLOT(applyStyleSheet()));
    prefs.snipCssFile = snipCssFile;
    prefs.reslistformat =  paraFormat;
    prefs.reslistheadertext =  headerText;
    if (prefs.reslistformat.trimmed().isEmpty()) {
        prefs.reslistformat = prefs.dfltResListFormat;
        paraFormat = prefs.reslistformat;
    }
    prefs.snipwMaxLength = snipwMaxLenSB->value();
    prefs.snipwSortByPage = snipwByPageCB->isChecked();
    prefs.alwaysSnippets = alwaysSnippetsCB->isChecked();

    prefs.creslistformat = (const char*)prefs.reslistformat.toUtf8();

    if (stemLangCMB->currentIndex() == 0) {
        prefs.queryStemLang = "";
    } else if (stemLangCMB->currentIndex() == 1) {
        prefs.queryStemLang = "ALL";
    } else {
        prefs.queryStemLang = stemLangCMB->currentText();
    }
    prefs.ssearchAutoPhrase = autoPhraseCB->isChecked();
    prefs.ssearchAutoPhraseThreshPC = autoPThreshSB->value();
    prefs.queryBuildAbstract = buildAbsCB->isChecked();
    prefs.queryReplaceAbstract = buildAbsCB->isChecked() && 
        replAbsCB->isChecked();

    prefs.startWithAdvSearchOpen = initStartAdvCB->isChecked();

    prefs.keepSort = keepSortCB->isChecked();
    prefs.noToolbars = noToolbarsCB->isChecked();
    m_mainWindow->setupToolbars();
    prefs.noMenuBar = noMenuBarCB->isChecked();
    m_mainWindow->setupMenus();
    prefs.noSSTypCMB = noSSTypCMBCB->isChecked();
    prefs.resTableTextNoShift = restabShowTxtNoShiftRB->isChecked();
    prefs.resTableNoHoverMeta = resTableNoHoverMetaCB->isChecked();
    prefs.noResTableHeader = noResTableHeaderCB->isChecked();
    prefs.showResTableVHeader = showResTableVHeaderCB->isChecked();
    prefs.noResTableRowJumpSC = noRowJumpShortcutsCB->isChecked();
    prefs.noStatusBar = noStatusBarCB->isChecked();
    m_mainWindow->setupStatusBar();
    prefs.noClearSearch = noClearSearchCB->isChecked();
    m_mainWindow->sSearch->setupButtons();
    prefs.showTrayIcon = showTrayIconCB->isChecked();
    m_mainWindow->enableTrayIcon(prefs.showTrayIcon);
    prefs.closeToTray = closeToTrayCB->isChecked();
    prefs.trayMessages = trayMessagesCB->isChecked();
    // -1 is the qxtconf... predefined value to show the dialog
    prefs.showTempFileWarning = showTempFileWarningCB->isChecked() ? -1 : 1;
    settings.setValue("anchorSpcHack", anchorTamilHackCB->isChecked());
    prefs.previewHtml = previewHtmlCB->isChecked();
    prefs.previewActiveLinks = previewActiveLinksCB->isChecked();

    if (plainBRRB->isChecked()) {
        prefs.previewPlainPre = PrefsPack::PP_BR;
    } else if (plainPRERB->isChecked()) {
        prefs.previewPlainPre = PrefsPack::PP_PRE;
    } else {
        prefs.previewPlainPre = PrefsPack::PP_PREWRAP;
    }

    prefs.syntAbsLen = syntlenSB->value();
    prefs.syntAbsCtx = syntctxSB->value();

    prefs.autoSuffsEnable = autoSuffsCB->isChecked();
    prefs.autoSuffs = autoSuffsLE->text();

    prefs.synFileEnable = synFileCB->isChecked();
    prefs.synFile = synFile;
    
    prefs.allExtraDbs.clear();
    prefs.activeExtraDbs.clear();
    for (int i = 0; i < idxLV->count(); i++) {
        QListWidgetItem *item = idxLV->item(i);
        if (item) {
            prefs.allExtraDbs.push_back(qs2path(item->text()));
            if (item->checkState() == Qt::Checked) {
                prefs.activeExtraDbs.push_back(qs2path(item->text()));
            }
        }
    }

    rwSettings(true);
    storeShortcuts();
    
    string reason;
    maybeOpenDb(reason, true);
    emit uiprefsDone();
    QDialog::accept();
}

void UIPrefsDialog::editParaFormat()
{
    EditDialog dialog(this);
    dialog.setWindowTitle(tr("Result list paragraph format "
                             "(erase all to reset to default)"));
    dialog.plainTextEdit->setPlainText(paraFormat);
    int result = dialog.exec();
    if (result == QDialog::Accepted)
        paraFormat = dialog.plainTextEdit->toPlainText();
}

void UIPrefsDialog::editHeaderText()
{
    EditDialog dialog(this);
    dialog.setWindowTitle(tr("Result list header (default is empty)"));
    dialog.plainTextEdit->setPlainText(headerText);
    int result = dialog.exec();
    if (result == QDialog::Accepted)
        headerText = dialog.plainTextEdit->toPlainText();
}

void UIPrefsDialog::reject()
{
    setFromPrefs();
    QDialog::reject();
}

void UIPrefsDialog::setStemLang(const QString& lang)
{
    int cur = 0;
    if (lang == "") {
        cur = 0;
    } else if (lang == "ALL") {
        cur = 1;
    } else {
        for (int i = 1; i < stemLangCMB->count(); i++) {
            if (lang == stemLangCMB->itemText(i)) {
                cur = i;
                break;
            }
        }
    }
    stemLangCMB->setCurrentIndex(cur);
}

void UIPrefsDialog::showFontDialog()
{
    bool ok;
    QFont font;
    if (prefs.reslistfontfamily.length()) {
        font.setFamily(prefs.reslistfontfamily);
        font.setPointSize(prefs.reslistfontsize);
    }

    font = QFontDialog::getFont(&ok, font, this);
    if (ok) {
        // We used to check if the default font was set, in which case
        // we erased the preference, but this would result in letting
        // webkit make a choice of default font which it usually seems
        // to do wrong. So now always set the font. There is still a
        // way for the user to let webkit choose the default though:
        // click reset, then the font name and size will be empty.
        reslistFontFamily = font.family();
        reslistFontSize = font.pointSize();
        setupReslistFontPB();
    }
}

void UIPrefsDialog::setSSButState()
{
    darkSSPB->setEnabled(!darkMode);
    resetSSPB->setEnabled(darkMode || !qssFile.isEmpty());
    if (darkMode || qssFile.isEmpty()) {
        stylesheetPB->setText(tr("Choose QSS File"));
    } else {
        stylesheetPB->setText(path2qs(path_getsimple(qs2path(qssFile))));
    }
}

void UIPrefsDialog::showStylesheetDialog()
{
    auto newfn = myGetFileName(false, "Select stylesheet file", true);
    if (!newfn.isEmpty()) {
        qssFile = newfn;
        darkMode = false;
    }
    setSSButState();
}
void UIPrefsDialog::setDarkMode()
{
    auto fn = path_cat(path_cat(theconfig->getDatadir(), "examples"), "recoll-dark.qss");
    qssFile = u8s2qs(fn);
    darkMode = true;
    setSSButState();
}
void UIPrefsDialog::resetStylesheet()
{
    qssFile.clear();
    darkMode = false;
    setSSButState();
}

void UIPrefsDialog::showSnipCssDialog()
{
    snipCssFile = myGetFileName(false, "Select snippets window CSS file", true);
    string nm = path_getsimple(qs2path(snipCssFile));
    snipCssPB->setText(path2qs(nm));
}
void UIPrefsDialog::resetSnipCss()
{
    snipCssFile = "";
    snipCssPB->setText(tr("Choose"));
}

void UIPrefsDialog::showSynFileDialog()
{
    synFile = myGetFileName(false, "Select synonyms file", true);
    if (synFile.isEmpty()) {
        synFileCB->setChecked(false);
        synFileCB->setEnabled(false);
        synFilePB->setText(tr("Choose"));
        return;
    } else {
        synFileCB->setChecked(prefs.synFileEnable);
        synFileCB->setEnabled(true);
        string nm = path_getsimple(qs2path(synFile));
        synFilePB->setText(path2qs(nm));
    }
    string nm = path_getsimple(qs2path(synFile));
    synFilePB->setText(path2qs(nm));
}

void UIPrefsDialog::resetReslistFont()
{
    reslistFontFamily = "";
    reslistFontSize = QFont().pointSize();
    setupReslistFontPB();
}

void UIPrefsDialog::showViewAction()
{
    if (m_viewAction == 0) {
        m_viewAction = new ViewAction(0);
    } else {
        // Close and reopen, in hope that makes us visible...
        m_viewAction->close();
    }
    m_viewAction->show();
}
void UIPrefsDialog::showViewAction(const QString& mt)
{
    showViewAction();
    m_viewAction->selectMT(mt);
}

////////////////////////////////////////////
// External / extra search indexes setup

void UIPrefsDialog::extradDbSelectChanged()
{
    if (idxLV->selectedItems().size() <= 1) 
        ptransPB->setEnabled(true);
    else
        ptransPB->setEnabled(false);
}

void UIPrefsDialog::extraDbEditPtrans()
{
    string dbdir;
    if (idxLV->selectedItems().size() == 0) {
        dbdir = theconfig->getDbDir();
    } else if (idxLV->selectedItems().size() == 1) {
        QListWidgetItem *item = idxLV->selectedItems()[0];
        QString qd = item->data(Qt::DisplayRole).toString();
        dbdir = qs2path(qd);
    } else {
        QMessageBox::warning(
            0, "Recoll", tr("At most one index should be selected"));
        return;
    }
    dbdir = path_canon(dbdir);
    EditTrans *etrans = new EditTrans(dbdir, this);
    etrans->show();
}

void UIPrefsDialog::togExtraDbPB_clicked()
{
    for (int i = 0; i < idxLV->count(); i++) {
        QListWidgetItem *item = idxLV->item(i);
        if (item->isSelected()) {
            if (item->checkState() == Qt::Checked) {
                item->setCheckState(Qt::Unchecked);
            } else {
                item->setCheckState(Qt::Checked);
            }
        }
    }
}
void UIPrefsDialog::actAllExtraDbPB_clicked()
{
    for (int i = 0; i < idxLV->count(); i++) {
        QListWidgetItem *item = idxLV->item(i);
        item->setCheckState(Qt::Checked);
    }
}
void UIPrefsDialog::unacAllExtraDbPB_clicked()
{
    for (int i = 0; i < idxLV->count(); i++) {
        QListWidgetItem *item = idxLV->item(i);
        item->setCheckState(Qt::Unchecked);
    }
}

void UIPrefsDialog::delExtraDbPB_clicked()
{
    QList<QListWidgetItem *> items = idxLV->selectedItems();
    for (QList<QListWidgetItem *>::iterator it = items.begin(); 
         it != items.end(); it++) {
        delete *it;
    }
}

void UIPrefsDialog::on_showTrayIconCB_clicked()
{
    if (!showTrayIconCB->checkState()) {
        closeToTrayCB->setChecked(false);
        trayMessagesCB->setChecked(false);
    }
    closeToTrayCB->setEnabled(showTrayIconCB->checkState());
    trayMessagesCB->setEnabled(showTrayIconCB->checkState());
}

/** 
 * Browse to add another index.
 * We do a textual comparison to check for duplicates, except for
 * the main db for which we check inode numbers. 
 */
void UIPrefsDialog::addExtraDbPB_clicked()
{
    QString input = myGetFileName(true, 
                                  tr("Select recoll config directory or "
                                     "xapian index directory "
                                     "(e.g.: /home/me/.recoll or "
                                     "/home/me/.recoll/xapiandb)"));

    if (input.isEmpty())
        return;
    string dbdir = qs2path(input);
    if (path_exists(path_cat(dbdir, "recoll.conf"))) {
        // Chosen dir is config dir.
        RclConfig conf(&dbdir);
        dbdir = conf.getDbDir();
        if (dbdir.empty()) {
            QMessageBox::warning(
                0, "Recoll", tr("The selected directory looks like a Recoll "
                                "configuration directory but the configuration "
                                "could not be read"));
            return;
        }
    }

    LOGDEB("ExtraDbDial: got: ["  << (dbdir) << "]\n" );
    bool stripped;
    if (!Rcl::Db::testDbDir(dbdir, &stripped)) {
        QMessageBox::warning(0, "Recoll", tr("The selected directory does not "
                                             "appear to be a Xapian index"));
        return;
    }
    if (o_index_stripchars != stripped) {
        QMessageBox::warning(0, "Recoll", 
                             tr("Cant add index with different case/diacritics"
                                " stripping option"));
        return;
    }
    if (path_samefile(dbdir, theconfig->getDbDir())) {
        QMessageBox::warning(0, "Recoll", tr("This is the main/local index!"));
        return;
    }

    for (int i = 0; i < idxLV->count(); i++) {
        QListWidgetItem *item = idxLV->item(i);
        string existingdir = qs2path(item->text());
        if (path_samefile(dbdir, existingdir)) {
            QMessageBox::warning(
                0, "Recoll", tr("The selected directory is already in the "
                                "index list"));
            return;
        }
    }

    QListWidgetItem *item = new QListWidgetItem(path2qs(dbdir), idxLV);
    item->setCheckState(Qt::Checked);
    idxLV->sortItems();
}
