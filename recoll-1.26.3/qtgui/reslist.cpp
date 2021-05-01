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

#include <time.h>
#include <stdlib.h>

#include <memory>

#include <qapplication.h>
#include <qvariant.h>
#include <qevent.h>
#include <qmenu.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qtimer.h>
#include <qmessagebox.h>
#include <qimage.h>
#include <qscrollbar.h>
#include <QTextBlock>
#include <QShortcut>

#include "log.h"
#include "smallut.h"
#include "recoll.h"
#include "guiutils.h"
#include "pathut.h"
#include "docseq.h"
#include "pathut.h"
#include "mimehandler.h"
#include "plaintorich.h"
#include "internfile.h"
#include "indexer.h"
#include "snippets_w.h"
#include "listdialog.h"
#include "reslist.h"
#include "moc_reslist.cpp"
#include "rclhelp.h"
#include "appformime.h"
#include "respopup.h"
#include "reslistpager.h"

static const QKeySequence quitKeySeq("Ctrl+q");
static const QKeySequence closeKeySeq("Ctrl+w");

#if defined(USING_WEBKIT)
# include <QWebFrame>
# include <QWebElement>
# include <QWebSettings>
# define QWEBSETTINGS QWebSettings
#elif defined(USING_WEBENGINE)
// Notes for WebEngine
// - All links must begin with http:// for acceptNavigationRequest to be
//   called. 
// - The links passed to acceptNav.. have the host part 
//   lowercased -> we change S0 to http://localhost/S0, not http://S0
# include <QWebEnginePage>
# include <QWebEngineSettings>
# include <QtWebEngineWidgets>
# define QWEBSETTINGS QWebEngineSettings
#endif

#ifdef USING_WEBENGINE
// This script saves the location details when a mouse button is
// clicked. This is for replacing data provided by Webkit QWebElement
// on a right-click as QT WebEngine does not have an equivalent service.
static const string locdetailscript(R"raw(
var locDetails = '';
function saveLoc(ev) 
{
    el = ev.target;
    locDetails = '';
    while (el && el.attributes && !el.attributes.getNamedItem("rcldocnum")) {
        el = el.parentNode;
    }
    rcldocnum = el.attributes.getNamedItem("rcldocnum");
    if (rcldocnum) {
        rcldocnumvalue = rcldocnum.value;
    } else {
        rcldocnumvalue = "";
    }
    if (el && el.attributes) {
        locDetails = 'rcldocnum = ' + rcldocnumvalue
    }
}
)raw");

bool RclWebPage::acceptNavigationRequest(const QUrl& url, 
                                         NavigationType tp, 
                                         bool isMainFrame)
{ 
    Q_UNUSED(isMainFrame);
    LOGDEB0("QWebEnginePage::acceptNavigationRequest. Type: " <<
            tp << " isMainFrame " << isMainFrame << std::endl);
    if (tp == QWebEnginePage::NavigationTypeLinkClicked) {
        m_reslist->onLinkClicked(url);
        return false;
    } else {
        return true;
    }
}
#endif // WEBENGINE


// Decide if we set font family and style with a css section in the
// html <head> or with qwebsettings setfont... calls.  We currently do
// it with websettings because this gives an instant redisplay, and
// the css has a tendancy to not find some system fonts. Otoh,
// SetFontSize() needs a strange offset of 3, not needed with css.
#undef SETFONT_WITH_HEADSTYLE

class QtGuiResListPager : public ResListPager {
public:
    QtGuiResListPager(ResList *p, int ps) 
        : ResListPager(ps), m_reslist(p) 
        {}
    virtual bool append(const string& data);
    virtual bool append(const string& data, int idx, const Rcl::Doc& doc);
    virtual string trans(const string& in);
    virtual string detailsLink();
    virtual const string &parFormat();
    virtual const string &dateFormat();
    virtual string nextUrl();
    virtual string prevUrl();
    virtual string headerContent();
    virtual void suggest(const vector<string>uterms, 
                         map<string, vector<string> >& sugg);
    virtual string absSep() {return (const char *)(prefs.abssep.toUtf8());}
    virtual string iconUrl(RclConfig *, Rcl::Doc& doc);
#ifdef USING_WEBENGINE
    virtual string linkPrefix() override {return "http://localhost/";} 
#endif
private:
    ResList *m_reslist;
};

#if 0
FILE *fp;
void logdata(const char *data)
{
    if (fp == 0)
        fp = fopen("/tmp/recolltoto.html", "a");
    if (fp)
        fprintf(fp, "%s", data);
}
#else
#define logdata(X)
#endif

//////////////////////////////
// /// QtGuiResListPager methods:
bool QtGuiResListPager::append(const string& data)
{
    LOGDEB2("QtGuiReslistPager::appendString   : " << data << "\n");
    logdata(data.c_str());
    m_reslist->append(QString::fromUtf8(data.c_str()));
    return true;
}

bool QtGuiResListPager::append(const string& data, int docnum, 
                               const Rcl::Doc&)
{
    LOGDEB2("QtGuiReslistPager::appendDoc: blockCount " <<
            m_reslist->document()->blockCount() << ", " << data << "\n");
    logdata(data.c_str());
#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
    QString sdoc = QString(
        "<div class=\"rclresult\" id=\"%1\" rcldocnum=\"%1\">").arg(docnum);
    m_reslist->append(sdoc);
    m_reslist->append(QString::fromUtf8(data.c_str()));
    m_reslist->append("</div>");
#else
    int blkcnt0 = m_reslist->document()->blockCount();
    m_reslist->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
    m_reslist->textCursor().insertBlock();
    m_reslist->insertHtml(QString::fromUtf8(data.c_str()));
    m_reslist->moveCursor(QTextCursor::Start, QTextCursor::MoveAnchor);
    m_reslist->ensureCursorVisible();
    int blkcnt1 = m_reslist->document()->blockCount();
    for (int block = blkcnt0; block < blkcnt1; block++) {
        m_reslist->m_pageParaToReldocnums[block] = docnum;
    }
#endif
    return true;
}

string QtGuiResListPager::trans(const string& in)
{
    return string((const char*)ResList::tr(in.c_str()).toUtf8());
}

string QtGuiResListPager::detailsLink()
{
    string chunk = string("<a href=\"") + linkPrefix() + "H-1\">";
    chunk += trans("(show query)");
    chunk += "</a>";
    return chunk;
}

const string& QtGuiResListPager::parFormat()
{
    return prefs.creslistformat;
}
const string& QtGuiResListPager::dateFormat()
{
    return prefs.creslistdateformat;
}

string QtGuiResListPager::nextUrl()
{
    return "n-1";
}

string QtGuiResListPager::prevUrl()
{
    return "p-1";
}

string QtGuiResListPager::headerContent() 
{
    string out;

    out = "<style type=\"text/css\">\nbody,table,select,input {\n";
#ifdef SETFONT_WITH_HEADSTYLE
    char ftsz[30];
    sprintf(ftsz, "%d", prefs.reslistfontsize);
    out += string("font-family: \"") + qs2utf8s(prefs.reslistfontfamily)
        + "\";\n";
    out += string("font-size: ") + ftsz + "pt;\n";
#endif
    out += string("color: ") + qs2utf8s(prefs.fontcolor) + ";\n";
    out += string("}\n</style>\n");
#if defined(USING_WEBENGINE)
    out += "<script type=\"text/javascript\">\n";
    out += locdetailscript;
    out += "</script>\n";
#endif
    out += qs2utf8s(prefs.reslistheadertext);
    return out;
}

void QtGuiResListPager::suggest(const vector<string>uterms, 
                                map<string, vector<string> >& sugg)
{
    sugg.clear();
    bool issimple = m_reslist && m_reslist->m_rclmain && 
        m_reslist->m_rclmain->lastSearchSimple();

    for (const auto& uit : uterms) {
        vector<string> tsuggs;

        // If the term is in the dictionary, Aspell::suggest won't
        // list alternatives. In fact we may want to check the
        // frequencies and propose something anyway if a possible
        // variation is much more common (as google does) ?
        if (!rcldb->getSpellingSuggestions(uit, tsuggs)) {
            continue;
        }
        // We should check that the term stems differently from the
        // base word (else it's not useful to expand the search). Or
        // is it ? This should depend if stemming is turned on or not

        if (!tsuggs.empty()) {
            sugg[uit] = vector<string>(tsuggs.begin(), tsuggs.end());
            if (sugg[uit].size() > 5)
                sugg[uit].resize(5);
            // Set up the links as a <href="Sold|new">. 
            for (auto& it : sugg[uit]) {
                if (issimple) {
                    it = string("<a href=\"") + linkPrefix() + "S" + uit +
                        "|" + it + "\">" + it + "</a>";
                }
            }
        }
    }
}

string QtGuiResListPager::iconUrl(RclConfig *config, Rcl::Doc& doc)
{
    if (doc.ipath.empty()) {
        vector<Rcl::Doc> docs;
        docs.push_back(doc);
        vector<string> paths;
        Rcl::docsToPaths(docs, paths);
        if (!paths.empty()) {
            string path;
            LOGDEB2("ResList::iconUrl: source path [" << paths[0] << "]\n");
            if (thumbPathForUrl(cstr_fileu + paths[0], 128, path)) {
                LOGDEB2("ResList::iconUrl: icon path [" << path << "]\n");
                return cstr_fileu + path;
            } else {
                LOGDEB2("ResList::iconUrl: no icon: path [" << path << "]\n");
            }
        } else {
            LOGDEB("ResList::iconUrl: docsToPaths failed\n");
        }
    }
    return ResListPager::iconUrl(config, doc);
}

/////// /////// End reslistpager methods

string PlainToRichQtReslist::startMatch(unsigned int idx)
{
    (void)idx;
#if 0
    if (m_hdata) {
        string s1, s2;
        stringsToString<vector<string> >(m_hdata->groups[idx], s1); 
        stringsToString<vector<string> >(
            m_hdata->ugroups[m_hdata->grpsugidx[idx]], s2);
        LOGDEB2("Reslist startmatch: group " << s1 << " user group " <<
                s2 << "\n");
    }
#endif                
    return string("<span class='rclmatch' style='")
        + qs2utf8s(prefs.qtermstyle) + string("'>");
}

string PlainToRichQtReslist::endMatch()
{
    return string("</span>");
}

static PlainToRichQtReslist g_hiliter;

/////////////////////////////////////

ResList::ResList(QWidget* parent, const char* name)
    : RESLIST_PARENTCLASS(parent)
{
    if (!name)
        setObjectName("resList");
    else 
        setObjectName(name);
    
#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
    setPage(new RclWebPage(this));
#ifdef USING_WEBKIT
    LOGDEB("Reslist: using Webkit\n");
    page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    // signals and slots connections
    connect(this, SIGNAL(linkClicked(const QUrl &)), 
            this, SLOT(onLinkClicked(const QUrl &)));
#else
    LOGDEB("Reslist: using Webengine\n");
#endif
    settings()->setAttribute(QWEBSETTINGS::JavascriptEnabled, true);
#else
    LOGDEB("Reslist: using QTextBrowser\n");
    setReadOnly(true);
    setUndoRedoEnabled(false);
    setOpenLinks(false);
    setTabChangesFocus(true);
    // signals and slots connections
    connect(this, SIGNAL(anchorClicked(const QUrl &)), 
            this, SLOT(onLinkClicked(const QUrl &)));
#endif

    setFont();
    languageChange();

    (void)new HelpClient(this);
    HelpClient::installMap(qs2utf8s(this->objectName()),
                           "RCL.SEARCH.GUI.RESLIST");

#if 0
    // See comments in "highlighted
    connect(this, SIGNAL(highlighted(const QString &)), 
            this, SLOT(highlighted(const QString &)));
#endif

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(createPopupMenu(const QPoint&)));

    m_pager = new QtGuiResListPager(this, prefs.respagesize);
    m_pager->setHighLighter(&g_hiliter);
}

ResList::~ResList()
{
    // These have to exist somewhere for translations to work
#ifdef __GNUC__
    __attribute__((unused))
#endif
        static const char* strings[] = {
        QT_TR_NOOP("<p><b>No results found</b><br>"),
        QT_TR_NOOP("Documents"),
        QT_TR_NOOP("out of at least"),
        QT_TR_NOOP("for"),
        QT_TR_NOOP("Previous"),
        QT_TR_NOOP("Next"),
        QT_TR_NOOP("Unavailable document"),
        QT_TR_NOOP("Preview"),
        QT_TR_NOOP("Open"),
        QT_TR_NOOP("Snippets"),
        QT_TR_NOOP("(show query)"),
        QT_TR_NOOP("<p><i>Alternate spellings (accents suppressed): </i>"),
        QT_TR_NOOP("<p><i>Alternate spellings: </i>"),
    };
}

void ResList::setRclMain(RclMain *m, bool ismain) 
{
    m_rclmain = m;
    m_ismainres = ismain;
    if (!m_ismainres) {
        connect(new QShortcut(closeKeySeq, this), SIGNAL (activated()), 
                this, SLOT (close()));
        connect(new QShortcut(quitKeySeq, this), SIGNAL (activated()), 
                m_rclmain, SLOT (fileExit()));
        connect(this, SIGNAL(previewRequested(Rcl::Doc)), 
                m_rclmain, SLOT(startPreview(Rcl::Doc)));
        connect(this, SIGNAL(docSaveToFileClicked(Rcl::Doc)), 
                m_rclmain, SLOT(saveDocToFile(Rcl::Doc)));
        connect(this, SIGNAL(editRequested(Rcl::Doc)), 
                m_rclmain, SLOT(startNativeViewer(Rcl::Doc)));
    }
}

void ResList::runStoredJS()
{
    runJS(m_js);
    m_js.clear();
}

void ResList::runJS(const QString& js)
{
#if defined(USING_WEBKIT)
    page()->mainFrame()->evaluateJavaScript(js);
#elif defined(USING_WEBENGINE)
    page()->runJavaScript(js);
#else
    Q_UNUSED(js);
#endif
}

void ResList::setFont()
{
#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
#  ifndef SETFONT_WITH_HEADSTYLE
    if (prefs.reslistfontfamily.length()) {
        // For some reason there is (12-2014) an offset of 3 between what
        // we request from webkit and what we get.
        settings()->setFontSize(QWEBSETTINGS::DefaultFontSize, 
                                prefs.reslistfontsize + 3);
        settings()->setFontFamily(QWEBSETTINGS::StandardFont, 
                                  prefs.reslistfontfamily);
    } else {
        settings()->resetFontSize(QWEBSETTINGS::DefaultFontSize);
        settings()->resetFontFamily(QWEBSETTINGS::StandardFont);
    }
# endif
#else
    if (prefs.reslistfontfamily.length()) {
        QFont nfont(prefs.reslistfontfamily, prefs.reslistfontsize);
        QTextBrowser::setFont(nfont);
    } else {
        QTextBrowser::setFont(QFont());
    }
#endif
}

int ResList::newListId()
{
    static int id;
    return ++id;
}

void ResList::setDocSource(std::shared_ptr<DocSequence> nsource)
{
    LOGDEB("ResList::setDocSource()\n");
    m_source = std::shared_ptr<DocSequence>(new DocSource(theconfig, nsource));
    if (m_pager)
        m_pager->setDocSource(m_source);
}

// A query was executed, or the filtering/sorting parameters changed,
// re-read the results.
void ResList::readDocSource()
{
    LOGDEB("ResList::readDocSource()\n");
    resetView();
    if (!m_source)
        return;
    m_listId = newListId();

    // Reset the page size in case the preference was changed
    m_pager->setPageSize(prefs.respagesize);
    m_pager->setDocSource(m_source);
    resultPageNext();
    emit hasResults(m_source->getResCnt());
}

void ResList::resetList() 
{
    LOGDEB("ResList::resetList()\n");
    setDocSource(std::shared_ptr<DocSequence>());
    resetView();
}

void ResList::resetView() 
{
    m_curPvDoc = -1;
    // There should be a progress bar for long searches but there isn't 
    // We really want the old result list to go away, otherwise, for a
    // slow search, the user will wonder if anything happened. The
    // following helps making sure that the textedit is really
    // blank. Else, there are often icons or text left around
#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
    m_text = "";
    setHtml("<html><body></body></html>");
#else
    m_pageParaToReldocnums.clear();
    clear();
    QTextBrowser::append(".");
    clear();
#endif

}

bool ResList::displayingHistory()
{
    // We want to reset the displayed history if it is currently
    // shown. Using the title value is an ugly hack
    string htstring = string((const char *)tr("Document history").toUtf8());
    if (!m_source || m_source->title().empty())
        return false;
    return m_source->title().find(htstring) == 0;
}

void ResList::languageChange()
{
    setWindowTitle(tr("Result list"));
}

#if !defined(USING_WEBKIT) && !defined(USING_WEBENGINE)
// Get document number from text block number
int ResList::docnumfromparnum(int block)
{
    if (m_pager->pageNumber() < 0)
        return -1;

    // Try to find the first number < input and actually in the map
    // (result blocks can be made of several text blocks)
    std::map<int,int>::iterator it;
    do {
        it = m_pageParaToReldocnums.find(block);
        if (it != m_pageParaToReldocnums.end())
            return pageFirstDocNum() + it->second;
    } while (--block >= 0);
    return -1;
}

// Get range of paragraph numbers which make up the result for document number
pair<int,int> ResList::parnumfromdocnum(int docnum)
{
    LOGDEB("parnumfromdocnum: docnum " << docnum << "\n");
    if (m_pager->pageNumber() < 0) {
        LOGDEB("parnumfromdocnum: no page return -1,-1\n");
        return pair<int,int>(-1,-1);
    }
    int winfirst = pageFirstDocNum();
    if (docnum - winfirst < 0) {
        LOGDEB("parnumfromdocnum: docnum " << docnum << " < winfirst " <<
               winfirst << " return -1,-1\n");
        return pair<int,int>(-1,-1);
    }
    docnum -= winfirst;
    for (std::map<int,int>::iterator it = m_pageParaToReldocnums.begin();
         it != m_pageParaToReldocnums.end(); it++) {
        if (docnum == it->second) {
            int first = it->first;
            int last = first+1;
            std::map<int,int>::iterator it1;
            while ((it1 = m_pageParaToReldocnums.find(last)) != 
                   m_pageParaToReldocnums.end() && it1->second == docnum) {
                last++;
            }
            LOGDEB("parnumfromdocnum: return " << first << "," << last << "\n");
            return pair<int,int>(first, last);
        }
    }
    LOGDEB("parnumfromdocnum: not found return -1,-1\n");
    return pair<int,int>(-1,-1);
}
#endif // TEXTBROWSER

// Return doc from current or adjacent result pages. We can get called
// for a document not in the current page if the user browses through
// results inside a result window (with shift-arrow). This can only
// result in a one-page change.
bool ResList::getDoc(int docnum, Rcl::Doc &doc)
{
    LOGDEB("ResList::getDoc: docnum " << docnum << " winfirst " <<
           pageFirstDocNum() << "\n");
    int winfirst = pageFirstDocNum();
    int winlast = m_pager->pageLastDocNum();
    if (docnum < 0 ||  winfirst < 0 || winlast < 0)
        return false;

    // Is docnum in current page ? Then all Ok
    if (docnum >= winfirst && docnum <= winlast) {
        return m_pager->getDoc(docnum, doc);
    }

    // Else we accept to page down or up but not further
    if (docnum < winfirst && docnum >= winfirst - prefs.respagesize) {
        resultPageBack();
    } else if (docnum < winlast + 1 + prefs.respagesize) {
        resultPageNext();
    }
    winfirst = pageFirstDocNum();
    winlast = m_pager->pageLastDocNum();
    if (docnum >= winfirst && docnum <= winlast) {
        return m_pager->getDoc(docnum, doc);
    }
    return false;
}

void ResList::keyPressEvent(QKeyEvent * e)
{
    if ((e->modifiers() & Qt::ShiftModifier)) {
        if (e->key() == Qt::Key_PageUp) {
            // Shift-PageUp -> first page of results
            resultPageFirst();
            return;
        } 
    } else {
        if (e->key() == Qt::Key_PageUp || e->key() == Qt::Key_Backspace) {
            resPageUpOrBack();
            return;
        } else if (e->key() == Qt::Key_PageDown || e->key() == Qt::Key_Space) {
            resPageDownOrNext();
            return;
        }
    }
    RESLIST_PARENTCLASS::keyPressEvent(e);
}

void ResList::mouseReleaseEvent(QMouseEvent *e)
{
    m_lstClckMod = 0;
    if (e->modifiers() & Qt::ControlModifier) {
        m_lstClckMod |= Qt::ControlModifier;
    } 
    if (e->modifiers() & Qt::ShiftModifier) {
        m_lstClckMod |= Qt::ShiftModifier;
    }
    RESLIST_PARENTCLASS::mouseReleaseEvent(e);
}

void ResList::highlighted(const QString& )
{
    // This is supposedly called when a link is preactivated (hover or tab
    // traversal, but is not actually called for tabs. We would have liked to
    // give some kind of visual feedback for tab traversal
}

// Page Up/Down: we don't try to check if current paragraph is last or
// first. We just page up/down and check if viewport moved. If it did,
// fair enough, else we go to next/previous result page.
void ResList::resPageUpOrBack()
{
#if defined(USING_WEBKIT)
    if (scrollIsAtTop()) {
        resultPageBack();
    } else {
        page()->mainFrame()->scroll(0, -int(0.9*geometry().height()));
    }
    setupArrows();
#elif defined(USING_WEBENGINE)
    if (scrollIsAtTop()) {
        resultPageBack();
    } else {
        QString js = "window.scrollBy(" + 
            QString::number(0) + ", " +
            QString::number(-int(0.9*geometry().height())) + ");";
        runJS(js);
    }
    setupArrows();
#else
    int vpos = verticalScrollBar()->value();
    verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
    if (vpos == verticalScrollBar()->value())
        resultPageBack();
#endif
}

void ResList::resPageDownOrNext()
{
#if defined(USING_WEBKIT)
    if (scrollIsAtBottom()) {
        resultPageNext();
    } else {
        page()->mainFrame()->scroll(0, int(0.9*geometry().height()));
    }
    setupArrows();
#elif defined(USING_WEBENGINE)
    if (scrollIsAtBottom()) {
        resultPageNext();
    } else {
        QString js = "window.scrollBy(" + 
            QString::number(0) + ", " +
            QString::number(int(0.9*geometry().height())) + ");";
        runJS(js);
    }
    setupArrows();
#else
    int vpos = verticalScrollBar()->value();
    verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    LOGDEB("ResList::resPageDownOrNext: vpos before " << vpos << ", after "
           << verticalScrollBar()->value() << "\n");
    if (vpos == verticalScrollBar()->value()) 
        resultPageNext();
#endif
}

void ResList::setupArrows()
{
    emit prevPageAvailable(m_pager->hasPrev() || !scrollIsAtTop());
    emit nextPageAvailable(m_pager->hasNext() || !scrollIsAtBottom());
}

bool ResList::scrollIsAtBottom()
{
#if defined(USING_WEBKIT)
    QWebFrame *frame = page()->mainFrame();
    bool ret;
    if (!frame || frame->scrollBarGeometry(Qt::Vertical).isEmpty()) {
        ret = true;
    } else {
        int max = frame->scrollBarMaximum(Qt::Vertical);
        int cur = frame->scrollBarValue(Qt::Vertical); 
        ret = (max != 0) && (cur == max);
        LOGDEB2("Scrollatbottom: cur " << cur << " max " << max << "\n");
    }
    LOGDEB2("scrollIsAtBottom: returning " << ret << "\n");
    return ret;
#elif defined(USING_WEBENGINE)
    QSize css = page()->contentsSize().toSize();
    QSize wss = size();
    QPoint sp = page()->scrollPosition().toPoint();
    LOGDEB1("atBottom: contents W " << css.width() << " H " << css.height() << 
            " widget W " << wss.width() << " Y " << wss.height() << 
            " scroll X " << sp.x() << " Y " << sp.y() << "\n");
    // This seems to work but it's mysterious as points and pixels
    // should not be the same
    return wss.height() + sp.y() >= css.height() - 10;
#else
    return false;
#endif
}

bool ResList::scrollIsAtTop()
{
#if defined(USING_WEBKIT)
    QWebFrame *frame = page()->mainFrame();
    bool ret;
    if (!frame || frame->scrollBarGeometry(Qt::Vertical).isEmpty()) {
        ret = true;
    } else {
        int cur = frame->scrollBarValue(Qt::Vertical);
        int min = frame->scrollBarMinimum(Qt::Vertical);
        LOGDEB("Scrollattop: cur " << cur << " min " << min << "\n");
        ret = (cur == min);
    }
    LOGDEB2("scrollIsAtTop: returning " << ret << "\n");
    return ret;
#elif defined(USING_WEBENGINE)
    return page()->scrollPosition().toPoint().ry() == 0;
#else
    return false;
#endif
}

// Show previous page of results. We just set the current number back
// 2 pages and show next page.
void ResList::resultPageBack()
{
    if (m_pager->hasPrev()) {
        m_pager->resultPageBack();
        displayPage();
    }
}

// Go to the first page
void ResList::resultPageFirst()
{
    // In case the preference was changed
    m_pager->setPageSize(prefs.respagesize);
    m_pager->resultPageFirst();
    displayPage();
}

// Fill up result list window with next screen of hits
void ResList::resultPageNext()
{
    if (m_pager->hasNext()) {
        m_pager->resultPageNext();
        displayPage();
    }
}

void ResList::resultPageFor(int docnum)
{
    m_pager->resultPageFor(docnum);
    displayPage();
}

void ResList::append(const QString &text)
{
    LOGDEB2("QtGuiReslistPager::appendQString : " << qs2utf8s(text) << "\n");
#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
    m_text += text;
#else
    QTextBrowser::append(text);
#endif
}

void ResList::displayPage()
{
    resetView();

    m_pager->displayPage(theconfig);

#if defined(USING_WEBENGINE) || defined(USING_WEBKIT)
    setHtml(m_text);
#endif

#if defined(USING_WEBENGINE)
    // Have to delay running this. Alternative would be to set it as
    // onload on the body element in the html, like upplay does, but
    // this would need an ennoying reslistpager modification.
    m_js = "elt=document.getElementsByTagName('body')[0];"
        "elt.addEventListener('contextmenu', saveLoc);";
    QTimer::singleShot(200, this, SLOT(runStoredJS()));
#endif

    LOGDEB0("ResList::displayPg: hasNext " << m_pager->hasNext() <<
            " atBot " << scrollIsAtBottom() << " hasPrev " <<
            m_pager->hasPrev() << " at Top " << scrollIsAtTop() << " \n");
    setupArrows();

    // Possibly color paragraph of current preview if any
    previewExposed(m_curPvDoc);
}

// Color paragraph (if any) of currently visible preview
void ResList::previewExposed(int docnum)
{
    LOGDEB("ResList::previewExposed: doc " << docnum << "\n");

    // Possibly erase old one to white
    if (m_curPvDoc != -1) {
#if defined(USING_WEBKIT)
        QString sel = 
            QString("div[rcldocnum=\"%1\"]").arg(m_curPvDoc - pageFirstDocNum());
        LOGDEB2("Searching for element, selector: [" << qs2utf8s(sel) << "]\n");
        QWebElement elt = page()->mainFrame()->findFirstElement(sel);
        if (!elt.isNull()) {
            LOGDEB2("Found\n");
            elt.removeAttribute("style");
        } else {
            LOGDEB2("Not Found\n");
        }
#elif defined(USING_WEBENGINE)
        QString js = QString(
            "elt=document.getElementById('%1');"
            "if (elt){elt.removeAttribute('style');}"
            ).arg(m_curPvDoc - pageFirstDocNum());
        runJS(js);
#else
        pair<int,int> blockrange = parnumfromdocnum(m_curPvDoc);
        if (blockrange.first != -1) {
            for (int blockn = blockrange.first;
                 blockn < blockrange.second; blockn++) {
                QTextBlock block = document()->findBlockByNumber(blockn);
                QTextCursor cursor(block);
                QTextBlockFormat format = cursor.blockFormat();
                format.clearBackground();
                cursor.setBlockFormat(format);
            }
        }
#endif
        m_curPvDoc = -1;
    }

    // Set background for active preview's doc entry
    m_curPvDoc = docnum;

#if defined(USING_WEBKIT)
    QString sel = 
        QString("div[rcldocnum=\"%1\"]").arg(docnum - pageFirstDocNum());
    LOGDEB2("Searching for element, selector: [" << qs2utf8s(sel) << "]\n");
    QWebElement elt = page()->mainFrame()->findFirstElement(sel);
    if (!elt.isNull()) {
        LOGDEB2("Found\n");
        elt.setAttribute("style", "background: LightBlue;}");
    } else {
        LOGDEB2("Not Found\n");
    }
#elif defined(USING_WEBENGINE)
    QString js = QString(
        "elt=document.getElementById('%1');"
        "if(elt){elt.setAttribute('style', 'background: LightBlue');}"
        ).arg(docnum - pageFirstDocNum());
    runJS(js);
#else
    pair<int,int>  blockrange = parnumfromdocnum(docnum);

    // Maybe docnum is -1 or not in this window, 
    if (blockrange.first < 0)
        return;
    // Color the new active paragraph
    QColor color("LightBlue");
    for (int blockn = blockrange.first+1;
         blockn < blockrange.second; blockn++) {
        QTextBlock block = document()->findBlockByNumber(blockn);
        QTextCursor cursor(block);
        QTextBlockFormat format;
        format.setBackground(QBrush(color));
        cursor.mergeBlockFormat(format);
        setTextCursor(cursor);
        ensureCursorVisible();
    }
#endif
}

// Double click in res list: add selection to simple search
void ResList::mouseDoubleClickEvent(QMouseEvent *event)
{
    RESLIST_PARENTCLASS::mouseDoubleClickEvent(event);
#if defined(USING_WEBKIT) 
    emit(wordSelect(selectedText()));
#elif defined(USING_WEBENGINE)
    // webengineview does not have such an event function, and
    // reimplementing event() itself is not useful (tried) as it does
    // not get mouse clicks. We'd need javascript to do this, but it's
    // not that useful, so left aside for now.
#else
    if (textCursor().hasSelection())
        emit(wordSelect(textCursor().selectedText()));
#endif
}

void ResList::showQueryDetails()
{
    if (!m_source)
        return;
    string oq = breakIntoLines(m_source->getDescription(), 100, 50);
    QString str;
    QString desc = tr("Result count (est.)") + ": " + 
        str.setNum(m_source->getResCnt()) + "<br>";
    desc += tr("Query details") + ": " + QString::fromUtf8(oq.c_str());
    QMessageBox::information(this, tr("Query details"), desc);
}

void ResList::onLinkClicked(const QUrl &qurl)
{
    // qt5: url.toString() does not accept FullyDecoded, but that's what we
    // want. e.g. Suggestions links are like Sterm|spelling which we
    // receive as Sterm%7CSpelling
    string strurl = url_decode(qs2utf8s(qurl.toString()));
    
    LOGDEB1("ResList::onLinkClicked: [" << strurl << "] prefix " <<
            m_pager->linkPrefix() << "\n");
    strurl = strurl.substr(m_pager->linkPrefix().size());

    int what = strurl[0];
    switch (what) {

        // Open abstract/snippets window
    case 'A':
    {
        if (!m_source) 
            return;
        int i = atoi(strurl.c_str()+1) - 1;
        Rcl::Doc doc;
        if (!getDoc(i, doc)) {
            LOGERR("ResList::onLinkClicked: can't get doc for " << i << "\n");
            return;
        }
        emit(showSnippets(doc));
    }
    break;

    // Show duplicates
    case 'D':
    {
        if (!m_source) 
            return;
        int i = atoi(strurl.c_str()+1) - 1;
        Rcl::Doc doc;
        if (!getDoc(i, doc)) {
            LOGERR("ResList::onLinkClicked: can't get doc for " << i << "\n");
            return;
        }
        vector<Rcl::Doc> dups;
        if (m_source->docDups(doc, dups) && m_rclmain) {
            m_rclmain->newDupsW(doc, dups);
        }
    }
    break;

    // Open parent folder
    case 'F':
    {
        int i = atoi(strurl.c_str()+1) - 1;
        Rcl::Doc doc;
        if (!getDoc(i, doc)) {
            LOGERR("ResList::onLinkClicked: can't get doc for " << i << "\n");
            return;
        }
        emit editRequested(ResultPopup::getParent(std::shared_ptr<DocSequence>(),
                                                  doc));
    }
    break;

    // Show query details
    case 'h':
    case 'H':
    {
        showQueryDetails();
        break;
    }

    // Preview and edit
    case 'P': 
    case 'E': 
    {
        int i = atoi(strurl.c_str()+1) - 1;
        Rcl::Doc doc;
        if (!getDoc(i, doc)) {
            LOGERR("ResList::onLinkClicked: can't get doc for " << i << "\n");
            return;
        }
        if (what == 'P') {
            if (m_ismainres) {
                emit docPreviewClicked(i, doc, m_lstClckMod);
            } else {
                emit previewRequested(doc);
            }
        } else {
            emit editRequested(doc);
        }
    }
    break;

    // Next/prev page
    case 'n':
        resultPageNext();
        break;
    case 'p':
        resultPageBack();
        break;

        // Run script. Link format Rnn|Script Name
    case 'R':
    {
        int i = atoi(strurl.c_str() + 1) - 1;
        QString s = qurl.toString();
        int bar = s.indexOf("|");
        if (bar == -1 || bar >= s.size()-1)
            break;
        string cmdname = qs2utf8s(s.right(s.size() - (bar + 1)));
        DesktopDb ddb(path_cat(theconfig->getConfDir(), "scripts"));
        DesktopDb::AppDef app;
        if (ddb.appByName(cmdname, app)) {
            QAction act(QString::fromUtf8(app.name.c_str()), this);
            QVariant v(QString::fromUtf8(app.command.c_str()));
            act.setData(v);
            m_popDoc = i;
            menuOpenWith(&act);
        }
    }
    break;

    // Spelling: replacement suggestion clicked
    case 'S':
    {
        string s;
        if (!strurl.empty())
            s = strurl.substr(1);
        string::size_type bar = s.find_first_of("|");
        if (bar != string::npos && bar < s.size() - 1) {
            string o = s.substr(0, bar);
            string n = s.substr(bar+1);
            LOGDEB2("Emitting wordreplace " << o << " -> " << n << std::endl);
            emit wordReplace(u8s2qs(o), u8s2qs(n));
        }
    }
    break;

    default: 
        LOGERR("ResList::onLinkClicked: bad link [" << strurl.substr(0,20) << "]\n");
        break;// ?? 
    }
}

void ResList::onPopupJsDone(const QVariant &jr)
{
    QString qs(jr.toString());
    LOGDEB("onPopupJsDone: parameter: " << qs2utf8s(qs) << "\n");
    QStringList qsl = qs.split("\n", QString::SkipEmptyParts);
    for (int i = 0 ; i < qsl.size(); i++) {
        int eq = qsl[i].indexOf("=");
        if (eq > 0) {
            QString nm = qsl[i].left(eq).trimmed();
            QString value = qsl[i].right(qsl[i].size() - (eq+1)).trimmed();
            if (!nm.compare("rcldocnum")) {
                m_popDoc = atoi(qs2utf8s(value).c_str());
            } else {
                LOGERR("onPopupJsDone: unknown key: " << qs2utf8s(nm) << "\n");
            }
        }
    }
    doCreatePopupMenu();
}

void ResList::createPopupMenu(const QPoint& pos)
{
    LOGDEB("ResList::createPopupMenu(" << pos.x() << ", " << pos.y() << ")\n");
    m_popDoc = -1;
    m_popPos = pos;
#if defined(USING_WEBKIT)
    QWebHitTestResult htr = page()->mainFrame()->hitTestContent(pos);
    if (htr.isNull())
        return;
    QWebElement el = htr.enclosingBlockElement();
    while (!el.isNull() && !el.hasAttribute("rcldocnum"))
        el = el.parent();
    if (el.isNull())
        return;
    QString snum = el.attribute("rcldocnum");
    m_popDoc = pageFirstDocNum() + snum.toInt();
#elif defined(USING_WEBENGINE)
    QString js("window.locDetails;");
    RclWebPage *mypage = dynamic_cast<RclWebPage*>(page());
    mypage->runJavaScript(js, [this](const QVariant &v) {onPopupJsDone(v);});
#else
    QTextCursor cursor = cursorForPosition(pos);
    int blocknum = cursor.blockNumber();
    LOGDEB("ResList::createPopupMenu(): block " << blocknum << "\n");
    m_popDoc = docnumfromparnum(blocknum);
#endif
    doCreatePopupMenu();
}

void ResList::doCreatePopupMenu()
{
    if (m_popDoc < 0) 
        return;
    Rcl::Doc doc;
    if (!getDoc(m_popDoc, doc))
        return;

    int options =  ResultPopup::showSaveOne;
    if (m_ismainres)
        options |= ResultPopup::isMain;
    QMenu *popup = ResultPopup::create(this, options, m_source, doc);
    popup->popup(mapToGlobal(m_popPos));
}

void ResList::menuPreview()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc)) {
        if (m_ismainres) {
            emit docPreviewClicked(m_popDoc, doc, 0);
        } else {
            emit previewRequested(doc);
        }
    }
}

void ResList::menuSaveToFile()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        emit docSaveToFileClicked(doc);
}

void ResList::menuPreviewParent()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc) && m_source)  {
        Rcl::Doc pdoc = ResultPopup::getParent(m_source, doc);
        if (pdoc.mimetype == "inode/directory") {
            emit editRequested(pdoc);
        } else {
            emit previewRequested(pdoc);
        }
    }
}

void ResList::menuOpenParent()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc) && m_source) 
        emit editRequested(ResultPopup::getParent(m_source, doc));
}

void ResList::menuShowSnippets()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        emit showSnippets(doc);
}

void ResList::menuShowSubDocs()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc)) 
        emit showSubDocs(doc);
}

void ResList::menuEdit()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        emit editRequested(doc);
}
void ResList::menuOpenWith(QAction *act)
{
    if (act == 0)
        return;
    string cmd = qs2utf8s(act->data().toString());
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        emit openWithRequested(doc, cmd);
}

void ResList::menuCopyFN()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        ResultPopup::copyFN(doc);
}

void ResList::menuCopyURL()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        ResultPopup::copyURL(doc);
}

void ResList::menuExpand()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        emit docExpand(doc);
}

int ResList::pageFirstDocNum()
{
    return m_pager->pageFirstDocNum();
}
