/* Copyright (C) 2012 J.F.Dockes
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

#include <string>
#include <vector>
#include <sstream>

#if defined(USING_WEBKIT)
#  include <QWebSettings>
#  include <QWebFrame>
#  include <QUrl>
#  define QWEBSETTINGS QWebSettings
#  define QWEBPAGE QWebPage
#elif defined(USING_WEBENGINE)
// Notes for WebEngine
// - All links must begin with http:// for acceptNavigationRequest to be
//   called. 
// - The links passed to acceptNav.. have the host part 
//   lowercased -> we change S0 to http://h/S0, not http://S0
#  include <QWebEnginePage>
#  include <QWebEngineSettings>
#  include <QtWebEngineWidgets>
#  define QWEBSETTINGS QWebEngineSettings
#  define QWEBPAGE QWebEnginePage
#else
#include <QTextBrowser>
#endif

#include <QShortcut>

#include "log.h"
#include "recoll.h"
#include "snippets_w.h"
#include "guiutils.h"
#include "rcldb.h"
#include "rclhelp.h"
#include "plaintorich.h"

using namespace std;

#if defined(USING_WEBKIT)
#define browser ((QWebView*)browserw)
#elif defined(USING_WEBENGINE)
#define browser ((QWebEngineView*)browserw)
#else
#define browser ((QTextBrowser*)browserw)
#endif

class PlainToRichQtSnippets : public PlainToRich {
public:
    virtual string startMatch(unsigned int) {
        return string("<span class='rclmatch' style='")
            + qs2utf8s(prefs.qtermstyle) + string("'>");
    }
    virtual string endMatch() {
        return string("</span>");
    }
};
static PlainToRichQtSnippets g_hiliter;

void SnippetsW::init()
{
    m_sortingByPage = prefs.snipwSortByPage;
    QPushButton *searchButton = new QPushButton(tr("Search"));
    searchButton->setAutoDefault(false);
    buttonBox->addButton(searchButton, QDialogButtonBox::ActionRole);
//    setWindowFlags(Qt::WindowStaysOnTopHint);
    searchFM->hide();

    new QShortcut(QKeySequence::Find, this, SLOT(slotEditFind()));
    new QShortcut(QKeySequence(Qt::Key_Slash), this, SLOT(slotEditFind()));
    new QShortcut(QKeySequence(Qt::Key_Escape), searchFM, SLOT(hide()));
    new QShortcut(QKeySequence::FindNext, this, SLOT(slotEditFindNext()));
    new QShortcut(QKeySequence(Qt::Key_F3), this, SLOT(slotEditFindNext()));
    new QShortcut(QKeySequence::FindPrevious, this, 
                  SLOT(slotEditFindPrevious()));
    new QShortcut(QKeySequence(Qt::SHIFT + Qt::Key_F3), 
                  this, SLOT(slotEditFindPrevious()));

    QPushButton *closeButton = buttonBox->button(QDialogButtonBox::Close);
    if (closeButton)
        connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
    connect(searchButton, SIGNAL(clicked()), this, SLOT(slotEditFind()));
    connect(searchLE, SIGNAL(textChanged(const QString&)), 
            this, SLOT(slotSearchTextChanged(const QString&)));
    connect(nextPB, SIGNAL(clicked()), this, SLOT(slotEditFindNext()));
    connect(prevPB, SIGNAL(clicked()), this, SLOT(slotEditFindPrevious()));


    // Get rid of the placeholder widget created from the .ui
    delete browserw;
#if defined(USING_WEBKIT)
    browserw = new QWebView(this);
    verticalLayout->insertWidget(0, browserw);
    browser->setUrl(QUrl(QString::fromUtf8("about:blank")));
    connect(browser, SIGNAL(linkClicked(const QUrl &)), 
            this, SLOT(onLinkClicked(const QUrl &)));
    browser->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    browser->page()->currentFrame()->setScrollBarPolicy(Qt::Horizontal,
                                                        Qt::ScrollBarAlwaysOff);
    QWEBSETTINGS *ws = browser->page()->settings();
    if (prefs.reslistfontfamily != "") {
        ws->setFontFamily(QWEBSETTINGS::StandardFont, prefs.reslistfontfamily);
        ws->setFontSize(QWEBSETTINGS::DefaultFontSize, prefs.reslistfontsize);
    }
    if (!prefs.snipCssFile.isEmpty())
        ws->setUserStyleSheetUrl(QUrl::fromLocalFile(prefs.snipCssFile));
    browserw->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(browserw, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(createPopupMenu(const QPoint&)));
#elif defined(USING_WEBENGINE)
    browserw = new QWebEngineView(this);
    verticalLayout->insertWidget(0, browserw);
    browser->setPage(new SnipWebPage(this));
    QWEBSETTINGS *ws = browser->page()->settings();
    if (prefs.reslistfontfamily != "") {
        ws->setFontFamily(QWEBSETTINGS::StandardFont, prefs.reslistfontfamily);
        ws->setFontSize(QWEBSETTINGS::DefaultFontSize, prefs.reslistfontsize);
    }
    // Stylesheet TBD
    browserw->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(browserw, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(createPopupMenu(const QPoint&)));
#else
    browserw = new QTextBrowser(this);
    verticalLayout->insertWidget(0, browserw);
    connect(browser, SIGNAL(anchorClicked(const QUrl &)), 
            this, SLOT(onLinkClicked(const QUrl &)));
    browser->setReadOnly(true);
    browser->setUndoRedoEnabled(false);
    browser->setOpenLinks(false);
    browser->setTabChangesFocus(true);
    if (prefs.reslistfontfamily.length()) {
        QFont nfont(prefs.reslistfontfamily, prefs.reslistfontsize);
        browser->setFont(nfont);
    } else {
        browser->setFont(QFont());
    }
#endif
}

void SnippetsW::createPopupMenu(const QPoint& pos)
{
    QMenu *popup = new QMenu(this);
    if (m_sortingByPage) {
        popup->addAction(tr("Sort By Relevance"), this,
                         SLOT(reloadByRelevance()));
    } else {
        popup->addAction(tr("Sort By Page"), this, SLOT(reloadByPage()));
    }
    popup->popup(mapToGlobal(pos));
}

void SnippetsW::reloadByRelevance()
{
    m_sortingByPage = false;
    onSetDoc(m_doc, m_source);
}
void SnippetsW::reloadByPage()
{
    m_sortingByPage = true;
    onSetDoc(m_doc, m_source);
}

void SnippetsW::onSetDoc(Rcl::Doc doc, std::shared_ptr<DocSequence> source)
{
    m_doc = doc;
    m_source = source;
    if (!source)
        return;

    // Make title out of file name if none yet
    string titleOrFilename;
    string utf8fn;
    m_doc.getmeta(Rcl::Doc::keytt, &titleOrFilename);
    m_doc.getmeta(Rcl::Doc::keyfn, &utf8fn);
    if (titleOrFilename.empty()) {
        titleOrFilename = utf8fn;
    }
    QString title("Recoll - Snippets");
    if (!titleOrFilename.empty()) {
        title += QString(" : ") + QString::fromUtf8(titleOrFilename.c_str());
    }
    setWindowTitle(title);

    vector<Rcl::Snippet> vpabs;
    source->getAbstract(m_doc, vpabs, prefs.snipwMaxLength, m_sortingByPage);

    HighlightData hdata;
    source->getTerms(hdata);

    ostringstream oss;
    oss << 
        "<html><head>"
        "<meta http-equiv=\"content-type\" "
        "content=\"text/html; charset=utf-8\">";

    oss << "<style type=\"text/css\">\nbody,table,select,input {\n";
    oss << "color: " + qs2utf8s(prefs.fontcolor) + ";\n";
    oss << "}\n</style>\n";
    oss << qs2utf8s(prefs.reslistheadertext);

    oss << 
        "</head>"
        "<body>"
        "<table class=\"snippets\">"
        ;

    g_hiliter.set_inputhtml(false);
    bool nomatch = true;

    for (const auto& snippet : vpabs) {
        if (snippet.page == -1) {
            oss << "<tr><td colspan=\"2\">" << 
                snippet.snippet << "</td></tr>" << endl;
            continue;
        }
        list<string> lr;
        if (!g_hiliter.plaintorich(snippet.snippet, lr, hdata)) {
            LOGDEB1("No match for [" << snippet.snippet << "]\n");
            continue;
        }
        nomatch = false;
        oss << "<tr><td>";
        if (snippet.page > 0) {
            oss << "<a href=\"http://h/P" << snippet.page << "T" <<
                snippet.term << "\">" 
                << "P.&nbsp;" << snippet.page << "</a>";
        }
        oss << "</td><td>" << lr.front().c_str() << "</td></tr>" << endl;
    }
    oss << "</table>" << endl;
    if (nomatch) {
        oss.str("<html><head></head><body>\n");
        oss << qs2utf8s(tr("<p>Sorry, no exact match was found within limits. "
                           "Probably the document is very big and the snippets "
                           "generator got lost in a maze...</p>"));
    }
    oss << "\n</body></html>";
#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
    browser->setHtml(QString::fromUtf8(oss.str().c_str()));
#else
    browser->insertHtml(QString::fromUtf8(oss.str().c_str()));
    browser->moveCursor (QTextCursor::Start);
    browser->ensureCursorVisible();
#endif
    raise();
}

void SnippetsW::slotEditFind()
{
    searchFM->show();
    searchLE->selectAll();
    searchLE->setFocus();
}

void SnippetsW::slotEditFindNext()
{
    if (!searchFM->isVisible())
        slotEditFind();

#if defined(USING_WEBKIT)  || defined(USING_WEBENGINE)
    browser->findText(searchLE->text());
#else
    browser->find(searchLE->text());
#endif

}

void SnippetsW::slotEditFindPrevious()
{
    if (!searchFM->isVisible())
        slotEditFind();

#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
    browser->findText(searchLE->text(), QWEBPAGE::FindBackward);
#else
    browser->find(searchLE->text(), QTextDocument::FindBackward);
#endif
}

void SnippetsW::slotSearchTextChanged(const QString& txt)
{
#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
    browser->findText(txt);
#else
    // Cursor thing is so that we don't go to the next occurrence with
    // each character, but rather try to extend the current match
    QTextCursor cursor = browser->textCursor();
    cursor.setPosition(cursor.anchor(), QTextCursor::KeepAnchor);
    browser->setTextCursor(cursor);
    browser->find(txt, 0);
#endif
}

void SnippetsW::onLinkClicked(const QUrl &url)
{
    string ascurl = qs2u8s(url.toString()).substr(9);
    LOGDEB("Snippets::onLinkClicked: [" << ascurl << "]\n");

    if (ascurl.size() > 3) {
        int what = ascurl[0];
        switch (what) {
        case 'P': 
        {
            string::size_type numpos = ascurl.find_first_of("0123456789");
            if (numpos == string::npos)
                return;
            int page = atoi(ascurl.c_str() + numpos);
            string::size_type termpos = ascurl.find_first_of("T");
            string term;
            if (termpos != string::npos)
                term = ascurl.substr(termpos+1);
            emit startNativeViewer(m_doc, page, 
                                   QString::fromUtf8(term.c_str()));
            return;
        }
        }
    }
    LOGERR("Snippets::onLinkClicked: bad link [" << ascurl << "]\n");
}
