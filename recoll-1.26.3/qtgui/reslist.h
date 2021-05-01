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

#ifndef _RESLIST_H_INCLUDED_
#define _RESLIST_H_INCLUDED_
#include "autoconfig.h"

#include <map>
#include <QPoint>

#include "plaintorich.h"

#if defined(USING_WEBENGINE)
#  include <QWebEngineView>
#  define RESLIST_PARENTCLASS QWebEngineView
#elif defined(USING_WEBKIT)
#  include <QWebView>
#  define RESLIST_PARENTCLASS QWebView
#else
#  include <QTextBrowser>
#  define RESLIST_PARENTCLASS QTextBrowser
#endif

class RclMain;
class QtGuiResListPager;
class QEvent;
namespace Rcl {
class Doc;
}

/**
 * Display a list of document records. The data can be out of the history 
 * manager or from an index query, both abstracted as a DocSequence. 
 */
class ResList : public RESLIST_PARENTCLASS
{
    Q_OBJECT;

    friend class QtGuiResListPager;
public:
    ResList(QWidget* parent = 0, const char* name = 0);
    virtual ~ResList();
    
    // Return document for given docnum. We mostly act as an
    // intermediary to the docseq here, but this has also the
    // side-effect of making the entry current (visible and
    // highlighted), and only works if the num is inside the current
    // page or its immediate neighbours.
    bool getDoc(int docnum, Rcl::Doc &);
    bool displayingHistory();
    int listId() const {return m_listId;}
    int pageFirstDocNum();
    void setFont();
    void setRclMain(RclMain *m, bool ismain);

public slots:
    virtual void setDocSource(std::shared_ptr<DocSequence> nsource);
    virtual void resetList();     // Erase current list
    virtual void resPageUpOrBack(); // Page up pressed
    virtual void resPageDownOrNext(); // Page down pressed
    virtual void resultPageBack(); // Previous page of results
    virtual void resultPageFirst(); // First page of results
    virtual void resultPageNext(); // Next (or first) page of results
    virtual void resultPageFor(int docnum); // Page containing docnum
    virtual void menuPreview();
    virtual void menuSaveToFile();
    virtual void menuEdit();
    virtual void menuOpenWith(QAction *);
    virtual void menuCopyFN();
    virtual void menuCopyURL();
    virtual void menuExpand();
    virtual void menuPreviewParent();
    virtual void menuOpenParent();
    virtual void menuShowSnippets();
    virtual void menuShowSubDocs();
    virtual void previewExposed(int);
    virtual void append(const QString &text);
    virtual void readDocSource();
    virtual void highlighted(const QString& link);
    virtual void createPopupMenu(const QPoint& pos);
    virtual void showQueryDetails();
        
signals:
    void nextPageAvailable(bool);
    void prevPageAvailable(bool);
    void docPreviewClicked(int, Rcl::Doc, int);
    void docSaveToFileClicked(Rcl::Doc);
    void previewRequested(Rcl::Doc);
    void showSnippets(Rcl::Doc);
    void showSubDocs(Rcl::Doc);
    void editRequested(Rcl::Doc);
    void openWithRequested(Rcl::Doc, string cmd);
    void docExpand(Rcl::Doc);
    void wordSelect(QString);
    void wordReplace(const QString&, const QString&);
    void hasResults(int);

protected:
    void keyPressEvent(QKeyEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);
    void mouseDoubleClickEvent(QMouseEvent*);

public slots:
    virtual void onLinkClicked(const QUrl &);
    virtual void onPopupJsDone(const QVariant&);
    void runJS(const QString& js);
    void runStoredJS();
protected slots:
    virtual void languageChange();

private:
    QtGuiResListPager  *m_pager{0};
    std::shared_ptr<DocSequence> m_source;
    int        m_popDoc{-1}; // Docnum for the popup menu.
    QPoint     m_popPos;
    int        m_curPvDoc{-1};// Docnum for current preview
    int        m_lstClckMod{0}; // Last click modifier. 
    int        m_listId{0}; // query Id for matching with preview windows
#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
    // Webview makes it more difficult to append text incrementally,
    // so we store the page and display it when done.
    QString    m_text;
#else
    // Translate from textedit paragraph number to relative
    // docnum. Built while we insert text into the qtextedit
    std::map<int,int>  m_pageParaToReldocnums;
    virtual int docnumfromparnum(int);
    virtual std::pair<int,int> parnumfromdocnum(int);
#endif
    QString m_js;
    RclMain   *m_rclmain{0};
    bool m_ismainres{true};

    void doCreatePopupMenu();
    virtual void displayPage();
    static int newListId();
    void resetView();
    bool scrollIsAtTop();
    bool scrollIsAtBottom();
    void setupArrows();
};

#ifdef USING_WEBENGINE

// Subclass the page to hijack the link clicks
class RclWebPage : public QWebEnginePage {
    Q_OBJECT

public:
    RclWebPage(ResList *parent) 
        : QWebEnginePage((QWidget *)parent), m_reslist(parent) {}

protected:
    virtual bool acceptNavigationRequest(
        const QUrl& url, NavigationType tp, bool isMainFrame);
private:
    ResList *m_reslist;
};

#else // Using Qt Webkit

#define RclWebPage QWebPage

#endif

class PlainToRichQtReslist : public PlainToRich {
public:
    virtual string startMatch(unsigned int idx);
    virtual string endMatch();
};

#endif /* _RESLIST_H_INCLUDED_ */
