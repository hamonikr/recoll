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
#ifndef RCLMAIN_W_H
#define RCLMAIN_W_H
#include "autoconfig.h"

#include <qvariant.h>
#include <qmainwindow.h>
#include <QFileSystemWatcher>

#include "sortseq.h"
#include "preview_w.h"
#include "recoll.h"
#include "advsearch_w.h"
#include "uiprefs_w.h"
#include "rcldb.h"
#include "searchdata.h"
#include "spell_w.h"
#include <memory>
#include "pathut.h"
#include "guiutils.h"
#include "rclutil.h"

class SnippetsW;
class IdxSchedW;
class ExecCmd;
class Preview;
class ResTable;
class CronToolW;
class WinSchedToolW;
class RTIToolW;
class FragButs;
class SpecIdxW;
class WebcacheEdit;
class ConfIndexW;
class RclTrayIcon;

#include "ui_rclmain.h"


class RclMain : public QMainWindow, public Ui::RclMainBase {
    Q_OBJECT;

public:
    RclMain(QWidget * parent = 0)
        : QMainWindow(parent) {
        setupUi(this);
        init();
    }
    ~RclMain() {}

    QString getQueryDescription();

    /** This is only called from main() to set an URL to be displayed (using
    recoll as a doc extracter for embedded docs */
    virtual void setUrlToView(const QString& u) {
        m_urltoview = u;
    }
    /** Same usage: actually display the current urltoview */
    virtual void viewUrl();

    bool lastSearchSimple() {
        return m_searchIsSimple;
    }

    // Takes copies of the args instead of refs. Lazy and safe.
    void newDupsW(const Rcl::Doc doc, const std::vector<Rcl::Doc> dups);

    enum  IndexerState {IXST_UNKNOWN, IXST_NOTRUNNING,
                        IXST_RUNNINGMINE, IXST_RUNNINGNOTMINE};

    IndexerState indexerState() const {
        return m_indexerState;
    }
    void enableTrayIcon(bool onoff);
                            
public slots:
    virtual void fileExit();
    virtual void periodic100();
    virtual void toggleIndexing();
    virtual void bumpIndexing();
    virtual void rebuildIndex();
    virtual void specialIndex();
    virtual void startSearch(std::shared_ptr<Rcl::SearchData> sdata,
                             bool issimple);
    virtual void previewClosed(Preview *w);
    virtual void showAdvSearchDialog();
    virtual void showSpellDialog();
    virtual void showWebcacheDialog();
    virtual void showIndexStatistics();
    virtual void showFragButs();
    virtual void showSpecIdx();
    virtual void showAboutDialog();
    virtual void showMissingHelpers();
    virtual void showActiveTypes();
    virtual void startManual();
    virtual void startManual(const string&);
    virtual void showDocHistory();
    virtual void showExtIdxDialog();
    virtual void setSynEnabled(bool);
    virtual void showUIPrefs();
    virtual void showIndexConfig();
    virtual void execIndexConfig();
    virtual void showCronTool();
    virtual void execCronTool();
    virtual void showRTITool();
    virtual void execRTITool();
    virtual void showIndexSched();
    virtual void execIndexSched();
    virtual void setUIPrefs();
    virtual void enableNextPage(bool);
    virtual void enablePrevPage(bool);
    virtual void docExpand(Rcl::Doc);
    virtual void showSubDocs(Rcl::Doc);
    virtual void showSnippets(Rcl::Doc);
    virtual void startPreview(int docnum, Rcl::Doc doc, int keymods);
    virtual void startPreview(Rcl::Doc);
    virtual void startNativeViewer(Rcl::Doc, int pagenum = -1,
                                  QString term = QString());
    virtual void openWith(Rcl::Doc, string);
    virtual void saveDocToFile(Rcl::Doc);
    virtual void previewNextInTab(Preview *, int sid, int docnum);
    virtual void previewPrevInTab(Preview *, int sid, int docnum);
    virtual void previewExposed(Preview *, int sid, int docnum);
    virtual void resetSearch();
    virtual void eraseDocHistory();
    virtual void eraseSearchHistory();
    virtual void saveLastQuery();
    virtual void loadSavedQuery();
    virtual void setStemLang(QAction *id);
    virtual void adjustPrefsMenu();
    virtual void catgFilter(int);
    virtual void catgFilter(QAction *);
    virtual void onFragmentsChanged();
    virtual void initDbOpen();
    virtual void toggleFullScreen();
    virtual void on_actionSortByDateAsc_toggled(bool on);
    virtual void on_actionSortByDateDesc_toggled(bool on);
    virtual void on_actionShowResultsAsTable_toggled(bool on);
    virtual void onSortDataChanged(DocSeqSortSpec);
    virtual void resultCount(int);
    virtual void applyStyleSheet();
    virtual void setFilterCtlStyle(int stl);
    virtual void showTrayMessage(const QString& text);
    virtual void onSetDescription(QString);
                                          
private slots:
    virtual void updateIdxStatus();
    virtual void onWebcacheDestroyed(QObject *);
signals:
    void docSourceChanged(std::shared_ptr<DocSequence>);
    void stemLangChanged(const QString& lang);
    void sortDataChanged(DocSeqSortSpec);
    void resultsReady();
    void searchReset();

protected:
    virtual void closeEvent(QCloseEvent *);
    virtual void showEvent(QShowEvent *);


private:
    SnippetsW      *m_snippets{0};
    Preview        *curPreview{0};
    AdvSearch      *asearchform{0};
    UIPrefsDialog  *uiprefs{0};
    ConfIndexW     *indexConfig{0};
    IdxSchedW      *indexSched{0};
#ifdef _WIN32
    WinSchedToolW  *cronTool{0};
#else
    CronToolW      *cronTool{0};
#endif
    RTIToolW       *rtiTool{0};
    SpellW         *spellform{0};
    FragButs       *fragbuts{0};
    SpecIdxW       *specidx{0};
    QTimer         *periodictimer{0};
    WebcacheEdit   *webcache{0};
    ResTable       *restable{0};
    bool            displayingTable{false};
    QAction        *m_idNoStem{0};
    QAction        *m_idAllStem{0};
    QToolBar       *m_toolsTB{0};
    QToolBar       *m_resTB{0};
    QFrame         *m_filtFRM{0};
    QComboBox      *m_filtCMB{0};
    QButtonGroup   *m_filtBGRP{0};
    QMenu          *m_filtMN{0};
    QFileSystemWatcher m_watcher;
    vector<ExecCmd*>  m_viewers;
    ExecCmd          *m_idxproc{0}; // Indexing process
    bool             m_idxkilled{false}; // Killed my process
    TempFile        *m_idxreasontmp{nullptr};
    map<QString, QAction*> m_stemLangToId;
    vector<string>    m_catgbutvec;
    int               m_catgbutvecidx{0};
    DocSeqFiltSpec    m_filtspec;
    bool              m_sortspecnochange{false};
    DocSeqSortSpec    m_sortspec;
    std::shared_ptr<DocSequence> m_source;
    IndexerState      m_indexerState{IXST_UNKNOWN};
    bool              m_queryActive{false};
    bool              m_firstIndexing{false};
    // Last search was started from simple
    bool              m_searchIsSimple{false};
    // This is set to the query string by ssearch, and to empty by
    // advsearch, and used for the Preview window title. If empty, we
    // use the Xapian Query string.
    QString           m_queryDescription;
    // If set on init, will be displayed either through ext app, or
    // preview (if no ext app set)
    QString          m_urltoview;
    RclTrayIcon     *m_trayicon{0};
    // We sometimes take the indexer lock (e.g.: when editing the webcache)
    Pidfile         *m_pidfile{0};
    
    virtual void init();
    virtual void setupResTB(bool combo);
    virtual void previewPrevOrNextInTab(Preview *, int sid, int docnum,
                                        bool next);
    // flags may contain ExecCmd::EXF_xx values
    virtual void execViewer(const map<string, string>& subs, bool enterHistory,
                            const string& execpath, const vector<string>& lcmd,
                            const string& cmd, Rcl::Doc doc, int flags=0);
    virtual void setStemLang(const QString& lang);
    virtual void onSortCtlChanged();
    virtual void showIndexConfig(bool modal);
    virtual void showIndexSched(bool modal);
    virtual void showCronTool(bool modal);
    virtual void showRTITool(bool modal);
    virtual void updateIdxForDocs(vector<Rcl::Doc>&);
    virtual void initiateQuery();
    virtual bool containerUpToDate(Rcl::Doc& doc);
    virtual void setFiltSpec();
    virtual bool checkIdxPaths();
};

#endif // RCLMAIN_W_H
