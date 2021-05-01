/* Copyright (C) 2005-2019 J.F.Dockes
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

#include <list>
#include <utility>

#include <qmessagebox.h>
#include <qthread.h>
#include <qvariant.h>
#include <qpushbutton.h>
#include <qtabwidget.h>
#include <qprinter.h>
#include <qprintdialog.h>
#include <qscrollbar.h>
#include <qmenu.h>
#include <qtextedit.h>
#include <qtextbrowser.h>
#include <qprogressdialog.h>
#include <qevent.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <qimage.h>
#include <qurl.h>
#include <QShortcut>
#include <QTimer>

#include "log.h"
#include "pathut.h"
#include "internfile.h"
#include "recoll.h"
#include "smallut.h"
#include "chrono.h"
#include "wipedir.h"
#include "cancelcheck.h"
#include "preview_w.h"
#include "guiutils.h"
#include "docseqhist.h"
#include "rclhelp.h"
#include "preview_load.h"
#include "preview_plaintorich.h"
#include "rclmain_w.h"

static const QKeySequence closeKS(Qt::Key_Escape);
static const QKeySequence nextDocInTabKS(Qt::ShiftModifier+Qt::Key_Down);
static const QKeySequence prevDocInTabKS(Qt::ShiftModifier+Qt::Key_Up);
static const QKeySequence closeTabKS(Qt::ControlModifier+Qt::Key_W);
static const QKeySequence printTabKS(Qt::ControlModifier+Qt::Key_P);

// Make an attempt at trimming wildcard exprs at both ends of string
static void trimwildcards(string& elt)
{
    if (elt.empty())
        return;
    string::size_type initsize;
    do {
        initsize = elt.size();
        // Trim wildcard chars. 
        trimstring(elt, " *?");
        // Trim wildcard char classes 
        if (elt.size() && elt.back() == ']') {
            string::size_type offs = elt.find_last_of("[");
            if (offs != string::npos) {
                elt = elt.substr(0, offs);
                if (elt.size() && elt.back() == '[') {
                    elt.erase(elt.end()-1);
                }
            }
        }
        if (elt.size() && elt.front() == '[') {
            string::size_type offs = elt.find_first_of("]");
            if (offs != string::npos) {
                elt.erase(0, offs+1);
            }
        }
    } while (elt.size() && elt.size() != initsize);
}

void Preview::init()
{
    LOGDEB("Preview::init\n");
    setAttribute(Qt::WA_DeleteOnClose);
    
    // Create the first tab (the tab widget is created with one
    // initial tab for ease of use in designer, we remove it).
    addEditorTab();
    pvTab->removeTab(0);

    for (const auto& ugroup : m_hData.ugroups) {
        QString s;
        for (auto elt : ugroup) {
            trimwildcards(elt);
            if (!elt.empty()) {
                if (!s.isEmpty()) {
                    s.append(" ");
                }
                s.append(u8s2qs(elt));
            }
        }
        s = s.trimmed();
        searchTextCMB->addItem(s);
    }
    searchTextCMB->setCompleter(0);

    if (prefs.pvwidth > 100) {
        resize(prefs.pvwidth, prefs.pvheight);
    } else {
        resize(QSize(640, 480).expandedTo(minimumSizeHint()));
    }

    (void)new HelpClient(this);
    HelpClient::installMap((const char *)objectName().toUtf8(), 
                           "RCL.SEARCH.GUI.PREVIEW");

    // signals and slots connections
    connect(searchTextCMB, SIGNAL(editTextChanged(const QString&)), 
            this, SLOT(searchTextChanged(const QString&)));
    connect(nextPB, SIGNAL(clicked()), this, SLOT(nextPressed()));
    connect(prevPB, SIGNAL(clicked()), this, SLOT(prevPressed()));
    connect(clearPB, SIGNAL(clicked()), searchTextCMB, SLOT(clearEditText()));
    connect(editPB, SIGNAL(clicked()), this, SLOT(emitEditRequested()));
    connect(pvTab, SIGNAL(currentChanged(int)), this, SLOT(currentChanged(int)));
    connect(pvTab, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));

    connect(new QShortcut(closeKS, this), SIGNAL (activated()), 
            this, SLOT (close()));
    connect(new QShortcut(nextDocInTabKS, this), SIGNAL (activated()), 
            this, SLOT (emitShowNext()));
    connect(nextInTabPB, SIGNAL (clicked()), this, SLOT (emitShowNext()));
    connect(new QShortcut(prevDocInTabKS, this), SIGNAL (activated()), 
            this, SLOT (emitShowPrev()));
    connect(prevInTabPB, SIGNAL (clicked()), this, SLOT (emitShowPrev()));
    connect(new QShortcut(closeTabKS, this), SIGNAL (activated()), 
            this, SLOT (closeCurrentTab()));
    connect(new QShortcut(printTabKS, this), SIGNAL (activated()), 
            this, SIGNAL (printCurrentPreviewRequest()));

    currentChanged(pvTab->currentIndex());
}

void Preview::emitShowNext()
{
    if (m_loading)
        return;
    PreviewTextEdit *edit = currentEditor();
    if (edit) {
        emit(showNext(this, m_searchId, edit->m_docnum));
    }
}

void Preview::emitShowPrev()
{
    if (m_loading)
        return;
    PreviewTextEdit *edit = currentEditor();
    if (edit) {
        emit(showPrev(this, m_searchId, edit->m_docnum));
    }
}

void Preview::closeEvent(QCloseEvent *e)
{
    LOGDEB("Preview::closeEvent. m_loading " << m_loading << "\n");
    if (m_loading) {
        CancelCheck::instance().setCancel();
        e->ignore();
        return;
    }
    prefs.pvwidth = width();
    prefs.pvheight = height();

    /* Release all temporary files (but maybe none is actually set) */
    for (int i = 0; i < pvTab->count(); i++) {
        PreviewTextEdit *edit = editor(i);
        if (edit) {
            forgetTempFile(edit->m_tmpfilename);
        }
    }
    emit previewExposed(this, m_searchId, -1);
    emit previewClosed(this);
    QWidget::closeEvent(e);
}

extern const char *eventTypeToStr(int tp);

bool Preview::eventFilter(QObject *target, QEvent *event)
{
    if (event->type() != QEvent::KeyPress) {
#if 0
        LOGDEB("Preview::eventFilter(): " << eventTypeToStr(event->type()) <<
               "\n");
        if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *mev = (QMouseEvent *)event;
            LOGDEB("Mouse: GlobalY " << mev->globalY() << " y " << mev->y() <<
                   "\n");
        }
#endif
        return false;
    }

    PreviewTextEdit *edit = currentEditor();
    QKeyEvent *keyEvent = (QKeyEvent *)event;

    if (m_dynSearchActive) {
        if (keyEvent->key() == Qt::Key_F3) {
            LOGDEB2("Preview::eventFilter: got F3\n");
            doSearch(searchTextCMB->currentText(), true, 
                     (keyEvent->modifiers() & Qt::ShiftModifier) != 0);
            return true;
        }
        if (target != searchTextCMB)
            return QApplication::sendEvent(searchTextCMB, event);
    } else {
        if (edit && 
            (target == edit || target == edit->viewport())) {
            if (keyEvent->key() == Qt::Key_Slash ||
                (keyEvent->key() == Qt::Key_F &&
                 (keyEvent->modifiers() & Qt::ControlModifier))) {
                LOGDEB2("Preview::eventFilter: got / or C-F\n");
                searchTextCMB->setFocus();
                m_dynSearchActive = true;
                return true;
            } else if (keyEvent->key() == Qt::Key_Space) {
                LOGDEB2("Preview::eventFilter: got Space\n");
                int value = edit->verticalScrollBar()->value();
                value += edit->verticalScrollBar()->pageStep();
                edit->verticalScrollBar()->setValue(value);
                return true;
            } else if (keyEvent->key() == Qt::Key_Backspace) {
                LOGDEB2("Preview::eventFilter: got Backspace\n");
                int value = edit->verticalScrollBar()->value();
                value -= edit->verticalScrollBar()->pageStep();
                edit->verticalScrollBar()->setValue(value);
                return true;
            }
        }
    }

    return false;
}

void Preview::searchTextChanged(const QString & text)
{
    LOGDEB("Preview::searchTextChanged:(" << qs2utf8s(text) << ") current: ("<<
            qs2utf8s(searchTextCMB->currentText()) << ") currentindex " <<
            searchTextCMB->currentIndex() << "\n");
    if (!searchTextCMB->itemText(searchTextCMB->currentIndex()).compare(text)) {
        // Then we assume that the text was set by selecting in the
        // combobox There does not seem to be another way to
        // discriminate select and hand edit. Note that the
        // activated() signal is called *after* the editTextChanged()
        // one, so it is useless.
        m_searchTextFromIndex = searchTextCMB->currentIndex();
        doSearch("", false, false);
    } else {
        m_searchTextFromIndex = -1;
        if (text.isEmpty()) {
            m_dynSearchActive = false;
            clearPB->setEnabled(false);
        } else {
            m_dynSearchActive = true;
            clearPB->setEnabled(true);
            doSearch(text, false, false);
        }
    }
}

void Preview::emitSaveDocToFile()
{
    PreviewTextEdit *ce = currentEditor();
    if (ce && !ce->m_dbdoc.url.empty()) {
        emit saveDocToFile(ce->m_dbdoc);
    }
}

void Preview::emitEditRequested()
{
    PreviewTextEdit *ce = currentEditor();
    if (ce && !ce->m_dbdoc.url.empty()) {
        emit editRequested(ce->m_dbdoc);
    }
}

// Perform text search. If next is true, we look for the next match of the
// current search, trying to advance and possibly wrapping around. If next is
// false, the search string has been modified, we search for the new string, 
// starting from the current position
void Preview::doSearch(const QString &_text, bool next, bool reverse, 
                       bool wordOnly)
{
    LOGDEB("Preview::doSearch: text [" << qs2utf8s(_text) << "] idx " <<
           m_searchTextFromIndex << " next " << next << " rev " << reverse <<
           " word " << wordOnly << "\n");
    QString text = _text;

    bool matchCase = casematchCB->isChecked();
    PreviewTextEdit *edit = currentEditor();
    if (edit == 0) {
        // ??
        return;
    }

    if (text.isEmpty() || m_searchTextFromIndex != -1) {
        if (!edit->m_plaintorich->haveAnchors()) {
            LOGDEB("NO ANCHORS\n");
            return;
        }
        // The combobox indices are equal to the search ugroup indices
        // in hldata, that's how we built the list.
        if (reverse) {
            edit->m_plaintorich->prevAnchorNum(m_searchTextFromIndex);
        } else {
            edit->m_plaintorich->nextAnchorNum(m_searchTextFromIndex);
        }
        QString aname = edit->m_plaintorich->curAnchorName();
        LOGDEB("Calling scrollToAnchor(" << qs2utf8s(aname) << ")\n");
        edit->scrollToAnchor(aname);
        // Position the cursor approximately at the anchor (top of
        // viewport) so that searches start from here
        QTextCursor cursor = edit->cursorForPosition(QPoint(0, 0));
        edit->setTextCursor(cursor);
        return;
    }

    // If next is false, the user added characters to the current
    // search string.  We need to reset the cursor position to the
    // start of the previous match, else incremental search is going
    // to look for the next occurrence instead of trying to lenghten
    // the current match
    if (!next) {
        QTextCursor cursor = edit->textCursor();
        cursor.setPosition(cursor.anchor(), QTextCursor::KeepAnchor);
        edit->setTextCursor(cursor);
    }
    Chrono chron;
    LOGDEB("Preview::doSearch: first find call\n");
    QTextDocument::FindFlags flags = 0;
    if (reverse)
        flags |= QTextDocument::FindBackward;
    if (wordOnly)
        flags |= QTextDocument::FindWholeWords;
    if (matchCase)
        flags |= QTextDocument::FindCaseSensitively;
    bool found = edit->find(text, flags);
    LOGDEB("Preview::doSearch: first find call return: found " << found <<
           " " << chron.secs() << " S\n");
    // If not found, try to wrap around. 
    if (!found) { 
        LOGDEB("Preview::doSearch: wrapping around\n");
        if (reverse) {
            edit->moveCursor (QTextCursor::End);
        } else {
            edit->moveCursor (QTextCursor::Start);
        }
        LOGDEB("Preview::doSearch: 2nd find call\n");
        chron.restart();
        found = edit->find(text, flags);
        LOGDEB("Preview::doSearch: 2nd find call return found " << found <<
               " " << chron.secs() << " S\n");
    }

    if (found) {
        m_canBeep = true;
    } else {
        if (m_canBeep && !prefs.noBeeps)
            QApplication::beep();
        m_canBeep = false;
    }
    LOGDEB("Preview::doSearch: return\n");
}

void Preview::nextPressed()
{
    LOGDEB2("Preview::nextPressed\n");
    doSearch(searchTextCMB->currentText(), true, false);
}

void Preview::prevPressed()
{
    LOGDEB2("Preview::prevPressed\n");
    doSearch(searchTextCMB->currentText(), true, true);
}

// Called when user clicks on tab
void Preview::currentChanged(int index)
{
    LOGDEB2("PreviewTextEdit::currentChanged\n");
    PreviewTextEdit *edit = editor(index);
    LOGDEB1("Preview::currentChanged(). Editor: " << edit << "\n");
    
    if (edit == 0) {
        LOGERR("Editor child not found\n");
        return;
    }
    edit->setFocus();

    editPB->setEnabled(canOpen(&edit->m_dbdoc, theconfig));

    // Disconnect the print signal and reconnect it to the current editor
    LOGDEB1("Disconnecting reconnecting print signal\n");
    disconnect(this, SIGNAL(printCurrentPreviewRequest()), 0, 0);
    connect(this, SIGNAL(printCurrentPreviewRequest()), edit, SLOT(print()));
    edit->installEventFilter(this);
    edit->viewport()->installEventFilter(this);
    searchTextCMB->installEventFilter(this);
    emit(previewExposed(this, m_searchId, edit->m_docnum));
}

void Preview::closeCurrentTab()
{
    LOGDEB1("Preview::closeCurrentTab: m_loading " << m_loading << "\n");
    if (m_loading) {
        CancelCheck::instance().setCancel();
        return;
    }
    closeTab(pvTab->currentIndex());
}

void Preview::closeTab(int index)
{
    LOGDEB1("Preview::closeTab: m_loading " << m_loading << "\n");
    if (m_loading) {
        CancelCheck::instance().setCancel();
        return;
    }
    PreviewTextEdit *edit = editor(index);
    if (edit)
        forgetTempFile(edit->m_tmpfilename);
    if (pvTab->count() > 1) {
        pvTab->removeTab(index);
    } else {
        close();
    }
}

PreviewTextEdit *Preview::editor(int index)
{
    return dynamic_cast<PreviewTextEdit*>(pvTab->widget(index));
}

PreviewTextEdit *Preview::currentEditor()
{
    LOGDEB2("Preview::currentEditor()\n");
    return editor(pvTab->currentIndex());
}

PreviewTextEdit *Preview::addEditorTab()
{
    LOGDEB1("PreviewTextEdit::addEditorTab()\n");
    PreviewTextEdit *editor = new PreviewTextEdit(pvTab, "pvEdit", this);
    editor->setReadOnly(true);
    editor->setUndoRedoEnabled(false );
    pvTab->addTab(editor, "Tab");
    pvTab->setCurrentIndex(pvTab->count() - 1);
    return editor;
}

void Preview::setCurTabProps(const Rcl::Doc &doc, int docnum)
{
    LOGDEB1("Preview::setCurTabProps\n");
    QString title;
    string ctitle;
    if (doc.getmeta(Rcl::Doc::keytt, &ctitle) && !ctitle.empty()) {
        title = QString::fromUtf8(ctitle.c_str(), ctitle.length());
    } else {
        title = QString::fromLocal8Bit(path_getsimple(doc.url).c_str());
    }
    if (title.length() > 20) {
        title = title.left(10) + "..." + title.right(10);
    }
    int curidx = pvTab->currentIndex();
    pvTab->setTabText(curidx, title);

    char datebuf[100];
    datebuf[0] = 0;
    if (!doc.fmtime.empty() || !doc.dmtime.empty()) {
        time_t mtime = doc.dmtime.empty() ? 
            atoll(doc.fmtime.c_str()) : atoll(doc.dmtime.c_str());
        struct tm *tm = localtime(&mtime);
        strftime(datebuf, 99, "%Y-%m-%d %H:%M:%S", tm);
    }
    LOGDEB("Doc.url: [" << doc.url << "]\n");
    string url;
    printableUrl(theconfig->getDefCharset(), doc.url, url);
    string tiptxt = url + string("\n");
    tiptxt += doc.mimetype + " " + string(datebuf) + "\n";
    if (!ctitle.empty())
        tiptxt += ctitle + "\n";
    pvTab->setTabToolTip(curidx,
                         QString::fromUtf8(tiptxt.c_str(), tiptxt.length()));

    PreviewTextEdit *e = currentEditor();
    if (e) {
        e->m_url = doc.url;
        e->m_ipath = doc.ipath;
        e->m_docnum = docnum;
    }
}

bool Preview::makeDocCurrent(const Rcl::Doc& doc, int docnum, bool sametab)
{
    LOGDEB("Preview::makeDocCurrent: " << doc.url << "\n");

    if (m_loading) {
        LOGERR("Already loading\n");
        return false;
    }

    /* Check if we already have this page */
    for (int i = 0; i < pvTab->count(); i++) {
        PreviewTextEdit *edit =  editor(i);
        if (edit && !edit->m_url.compare(doc.url) && 
            !edit->m_ipath.compare(doc.ipath)) {
            pvTab->setCurrentIndex(i);
            return true;
        }
    }

    // if just created the first tab was created during init
    if (!sametab && !m_justCreated && !addEditorTab()) {
        return false;
    }
    m_justCreated = false;
    if (!loadDocInCurrentTab(doc, docnum)) {
        closeCurrentTab();
        return false;
    }
    raise();
    return true;
}

void Preview::togglePlainPre()
{
    switch (prefs.previewPlainPre) {
    case PrefsPack::PP_BR:
        prefs.previewPlainPre = PrefsPack::PP_PRE;
        break;
    case PrefsPack::PP_PRE:
        prefs.previewPlainPre = PrefsPack::PP_BR;
        break;
    case PrefsPack::PP_PREWRAP:
    default:
        prefs.previewPlainPre = PrefsPack::PP_PRE;
        break;
    }
    
    PreviewTextEdit *editor = currentEditor();
    if (editor)
        loadDocInCurrentTab(editor->m_dbdoc, editor->m_docnum);
}

void Preview::emitWordSelect(QString word)
{
    emit(wordSelect(word));
}

/*
  Code for loading a file into an editor window. The operations that
  we call have no provision to indicate progression, and it would be
  complicated or impossible to modify them to do so (Ie: for external 
  format converters).

  We implement a complicated and ugly mechanism based on threads to indicate 
  to the user that the app is doing things: lengthy operations are done in 
  threads and we update a progress indicator while they proceed (but we have 
  no estimate of their total duration).
  
  It might be possible, but complicated (need modifications in
  handler) to implement a kind of bucket brigade, to have the
  beginning of the text displayed faster
*/


// Insert into editor by chunks so that the top becomes visible
// earlier for big texts. This provokes some artifacts (adds empty line),
// so we can't set it too low.
#define CHUNKL 500*1000

// Make sure we don't ever reenter loadDocInCurrentTab: note that I
// don't think it's actually possible, this must be the result of a
// misguided debug session.
class LoadGuard {
    bool *m_bp;
public:
    LoadGuard(bool *bp) {m_bp = bp ; *m_bp = true;}
    ~LoadGuard() {*m_bp = false; CancelCheck::instance().setCancel(false);}
};

bool Preview::loadDocInCurrentTab(const Rcl::Doc &idoc, int docnum)
{
    LOGDEB1("Preview::loadDocInCurrentTab()\n");

    LoadGuard guard(&m_loading);
    CancelCheck::instance().setCancel(false);

    setCurTabProps(idoc, docnum);

    QString msg = QString("Loading: %1 (size %2 bytes)")
        .arg(QString::fromLocal8Bit(idoc.url.c_str()))
        .arg(QString::fromUtf8(idoc.fbytes.c_str()));

    QProgressDialog progress(msg, tr("Cancel"), 0, 0, this);
    progress.setMinimumDuration(2000);
    QEventLoop loop;
    QTimer tT;
    tT.setSingleShot(true);
    connect(&tT, SIGNAL(timeout()), &loop, SLOT(quit()));

    ////////////////////////////////////////////////////////////////////////
    // Load and convert document
    // idoc came out of the index data (main text and some fields missing). 
    // fdoc is the complete one what we are going to extract from storage.
    LoadThread lthr(theconfig, idoc, prefs.previewHtml, this);
    connect(&lthr, SIGNAL(finished()), &loop, SLOT(quit()));

    lthr.start();
    for (int i = 0;;i++) {
        tT.start(1000); 
        loop.exec();
        if (lthr.isFinished())
            break;
        if (progress.wasCanceled()) {
            CancelCheck::instance().setCancel();
        }
        if (i == 1)
            progress.show();
    }

    LOGDEB("loadDocInCurrentTab: after file load: cancel " <<
           CancelCheck::instance().cancelState() << " status " << lthr.status <<
           " text length " << lthr.fdoc.text.length() << "\n");

    if (CancelCheck::instance().cancelState())
        return false;
    if (lthr.status != 0) {
        bool canGetRawText = rcldb && rcldb->storesDocText();
        QString explain;
        if (!lthr.missing.empty()) {
            explain = QString::fromUtf8("<br>") +
                tr("Missing helper program: ") +
                QString::fromLocal8Bit(lthr.missing.c_str());
            QMessageBox::warning(0, "Recoll",
                                 tr("Can't turn doc into internal "
                                    "representation for ") +
                                 lthr.fdoc.mimetype.c_str() + explain);
        } else {
            if (progress.wasCanceled()) {
                QMessageBox::warning(0, "Recoll", tr("Canceled"));
            } else {
                progress.reset();
                // Note that we can't easily check for a readable file
                // because it's possible that only a region is locked
                // (e.g. on Windows for an ost file the first block is
                // readable even if Outlook is running).
                QString msg;
                switch (lthr.explain) {
                case FileInterner::FetchMissing:
                    msg = tr("Error loading the document: file missing.");
                    break;
                case FileInterner::FetchPerm:
                    msg = tr("Error loading the document: no permission.");
                    break;
                case FileInterner::FetchNoBackend:
                    msg =
                        tr("Error loading: backend not configured.");
                    break;
                case FileInterner::InternfileOther:
#ifdef _WIN32
                    msg = tr("Error loading the document: "
                             "other handler error<br>"
                             "Maybe the application is locking the file ?");
#else
                    msg = tr("Error loading the document: other handler error.");
#endif
                    break;
                }
                if (canGetRawText) {
                    msg += tr("<br>Attempting to display from stored text.");
                }
                QMessageBox::warning(0, "Recoll", msg);
            }
        }


        if (canGetRawText) {
            lthr.fdoc = idoc;
            if (!rcldb->getDocRawText(lthr.fdoc)) {
                QMessageBox::warning(0, "Recoll",
                                     tr("Could not fetch stored text"));
                progress.close();
                return false;
            }
        } else {
            progress.close();
        }
    }
    // Reset config just in case.
    theconfig->setKeyDir("");

    ////////////////////////////////////////////////////////////////////////
    // Create preview text: highlight search terms
    // We don't do the highlighting for very big texts: too long. We
    // should at least do special char escaping, in case a '&' or '<'
    // somehow slipped through previous processing.
    bool highlightTerms = lthr.fdoc.text.length() < 
        (unsigned long)prefs.maxhltextmbs * 1024 * 1024;

    // Final text is produced in chunks so that we can display the top
    // while still inserting at bottom
    PreviewTextEdit *editor = currentEditor();

    editor->m_plaintorich->clear();

    // For an actual html file, if we want to have the images and
    // style loaded in the preview, we need to set the search
    // path. Not too sure this is a good idea as I find them rather
    // distracting when looking for text, esp. with qtextedit
    // relatively limited html support (text sometimes get hidden by
    // images).
#if 0
    string path = fileurltolocalpath(idoc.url);
    if (!path.empty()) {
        path = path_getfather(path);
        QStringList paths(QString::fromLocal8Bit(path.c_str()));
        editor->setSearchPaths(paths);
    }
#endif

    editor->setHtml("");
    editor->m_format = Qt::RichText;
    bool inputishtml = !lthr.fdoc.mimetype.compare("text/html");
    QStringList qrichlst;
    editor->m_plaintorich->set_activatelinks(prefs.previewActiveLinks);
    
#if 1
    if (highlightTerms) {
        progress.setLabelText(tr("Creating preview text"));
        qApp->processEvents();

        if (inputishtml) {
            LOGDEB1("Preview: got html " << lthr.fdoc.text << "\n");
            editor->m_plaintorich->set_inputhtml(true);
        } else {
            LOGDEB1("Preview: got plain " << lthr.fdoc.text << "\n");
            editor->m_plaintorich->set_inputhtml(false);
        }

        ToRichThread rthr(lthr.fdoc.text, m_hData, editor->m_plaintorich,
                          qrichlst, this);
        connect(&rthr, SIGNAL(finished()), &loop, SLOT(quit()));
        rthr.start();

        for (;;) {
            tT.start(1000); 
            loop.exec();
            if (rthr.isFinished())
                break;
            if (progress.wasCanceled()) {
                CancelCheck::instance().setCancel();
            }
        }

        // Conversion to rich text done
        if (CancelCheck::instance().cancelState()) {
            if (qrichlst.size() == 0 || qrichlst.front().size() == 0) {
                // We can't call closeCurrentTab here as it might delete
                // the object which would be a nasty surprise to our
                // caller.
                return false;
            } else {
                qrichlst.back() += "<b>Cancelled !</b>";
            }
        }
    } else {
        LOGDEB("Preview: no highlighting, loading " << lthr.fdoc.text.size() <<
               " bytes\n");
        // No plaintorich() call.  In this case, either the text is
        // html and the html quoting is hopefully correct, or it's
        // plain-text and there is no need to escape special
        // characters. We'd still want to split in chunks (so that the
        // top is displayed faster), but we must not cut tags, and
        // it's too difficult on html. For text we do the splitting on
        // a QString to avoid utf8 issues.
        QString qr = QString::fromUtf8(lthr.fdoc.text.c_str(),
                                       lthr.fdoc.text.length());
        int l = 0;
        if (inputishtml) {
            qrichlst.push_back(qr);
        } else {
            editor->setPlainText("");
            editor->m_format = Qt::PlainText;
            for (int pos = 0; pos < (int)qr.length(); pos += l) {
                l = MIN(CHUNKL, qr.length() - pos);
                qrichlst.push_back(qr.mid(pos, l));
            }
        }
    }
#else // For testing qtextedit bugs...
    highlightTerms = true;
    const char *textlist[] =
        {
            "Du plain text avec un\n <termtag>termtag</termtag> fin de ligne:",
            "texte apres le tag\n",
        };
    const int listl = sizeof(textlist) / sizeof(char*);
    for (int i = 0 ; i < listl ; i++)
        qrichlst.push_back(QString::fromUtf8(textlist[i]));
#endif


    ///////////////////////////////////////////////////////////
    // Load text into editor window.
    progress.setLabelText(tr("Loading preview text into editor"));
    qApp->processEvents();
    for (QStringList::iterator it = qrichlst.begin();
         it != qrichlst.end(); it++) {
        qApp->processEvents();

        editor->append(*it);
        // We need to save the rich text for printing, the editor does
        // not do it consistently for us.
        editor->m_richtxt.append(*it);

        if (progress.wasCanceled()) {
            editor->append("<b>Cancelled !</b>");
            LOGDEB("loadDocInCurrentTab: cancelled in editor load\n");
            break;
        }
    }

    progress.close();
    editor->m_curdsp = PreviewTextEdit::PTE_DSPTXT;

    ////////////////////////////////////////////////////////////////////////
    // Finishing steps

    // Maybe the text was actually empty ? Switch to fields then. Else free-up 
    // the text memory in the loaded document. We still have a copy of the text
    // in editor->m_richtxt
    bool textempty = lthr.fdoc.text.empty();
    if (!textempty)
        lthr.fdoc.text.clear(); 
    editor->m_fdoc = lthr.fdoc;
    editor->m_dbdoc = idoc;
    editPB->setEnabled(canOpen(&editor->m_dbdoc, theconfig));
    if (textempty)
        editor->displayFields();

    // If this is an image, display it instead of the text.
    if (!idoc.mimetype.compare(0, 6, "image/")) {
        string fn = fileurltolocalpath(idoc.url);
        theconfig->setKeyDir(fn.empty() ? "" : path_getfather(fn));

        // We want a real file, so if this comes from data or we have
        // an ipath, create it.
        if (fn.empty() || !idoc.ipath.empty()) {
            TempFile temp = lthr.tmpimg;
            if (temp.ok()) {
                LOGDEB1("Preview: load: got temp file from internfile\n");
            } else if (!FileInterner::idocToFile(temp, string(), 
                                                 theconfig, idoc)) {
                temp = TempFile(); // just in case.
            }
            if (temp.ok()) {
                rememberTempFile(temp);
                fn = temp.filename();
                editor->m_tmpfilename = fn;
            } else {
                editor->m_tmpfilename.erase();
                fn.erase();
            }
        }

        if (!fn.empty()) {
            editor->m_image = QImage(fn.c_str());
            if (!editor->m_image.isNull())
                editor->displayImage();
        }
    }


    // Position the editor so that the first search term is visible
    if (searchTextCMB->currentText().length() != 0) {
        // If there is a current search string, perform the search.
        // Do not beep for an automatic search, this is ennoying.
        m_canBeep = false;
        doSearch(searchTextCMB->currentText(), true, false);
    } else {
        // Position to the first query term
        if (editor->m_plaintorich->haveAnchors()) {
            QString aname = editor->m_plaintorich->curAnchorName();
            LOGDEB2("Call movetoanchor(" << qs2utf8s(aname) << ")\n");
            editor->scrollToAnchor(aname);
            // Position the cursor approximately at the anchor (top of
            // viewport) so that searches start from here
            QTextCursor cursor = editor->cursorForPosition(QPoint(0, 0));
            editor->setTextCursor(cursor);
        }
    }


    // Enter document in document history
    historyEnterDoc(rcldb.get(), g_dynconf, idoc);

    editor->setFocus();
    emit(previewExposed(this, m_searchId, docnum));
    LOGDEB("loadDocInCurrentTab: returning true\n");
    return true;
}

PreviewTextEdit::PreviewTextEdit(QWidget* parent, const char* nm, Preview *pv) 
    : QTextBrowser(parent), m_preview(pv),
      m_plaintorich(new PlainToRichQtPreview()),
      m_dspflds(false), m_docnum(-1) 
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setObjectName(nm);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(createPopupMenu(const QPoint&)));
    connect(this, SIGNAL(anchorClicked(const QUrl &)),
            this, SLOT(onAnchorClicked(const QUrl&)));
    setOpenExternalLinks(false);
    setOpenLinks(false);
}

void PreviewTextEdit::onAnchorClicked(const QUrl& url)
{
    LOGDEB("PreviewTextEdit::onAnchorClicked: " << qs2utf8s(url.toString())
           << std::endl);
    if (prefs.previewActiveLinks && m_preview->m_rclmain) {
        Rcl::Doc doc;
        doc.url = qs2utf8s(url.toString()).c_str();
        doc.mimetype = "text/html";
        m_preview->m_rclmain->startNativeViewer(doc);
    }
}

void PreviewTextEdit::createPopupMenu(const QPoint& pos)
{
    LOGDEB1("PreviewTextEdit::createPopupMenu()\n");
    QMenu *popup = new QMenu(this);
    switch (m_curdsp) {
    case PTE_DSPTXT:
        popup->addAction(tr("Show fields"), this, SLOT(displayFields()));
        if (!m_image.isNull())
            popup->addAction(tr("Show image"), this, SLOT(displayImage()));
        break;
    case PTE_DSPFLDS:
        popup->addAction(tr("Show main text"), this, SLOT(displayText()));
        if (!m_image.isNull())
            popup->addAction(tr("Show image"), this, SLOT(displayImage()));
        break;
    case PTE_DSPIMG:
    default:
        popup->addAction(tr("Show fields"), this, SLOT(displayFields()));
        popup->addAction(tr("Show main text"), this, SLOT(displayText()));
        break;
    }
    popup->addAction(tr("Select All"), this, SLOT(selectAll()));
    popup->addAction(tr("Copy"), this, SLOT(copy()));
    popup->addAction(tr("Print"), this, SLOT(print()));
    if (prefs.previewPlainPre) {
        popup->addAction(tr("Fold lines"), m_preview, SLOT(togglePlainPre()));
    } else {
        popup->addAction(tr("Preserve indentation"), 
                         m_preview, SLOT(togglePlainPre()));
    }
    if (!m_dbdoc.url.empty()) {
        popup->addAction(tr("Save document to file"), 
                         m_preview, SLOT(emitSaveDocToFile()));
        if (canOpen(&m_dbdoc, theconfig)) {
            popup->addAction(tr("Open document"), 
                             m_preview, SLOT(emitEditRequested()));
        }
    }
    popup->popup(mapToGlobal(pos));
}

// Display main text
void PreviewTextEdit::displayText()
{
    LOGDEB1("PreviewTextEdit::displayText()\n");
    if (m_format == Qt::PlainText)
        setPlainText(m_richtxt);
    else
        setHtml(m_richtxt);
    m_curdsp = PTE_DSPTXT;
}

// Display field values
void PreviewTextEdit::displayFields()
{
    LOGDEB1("PreviewTextEdit::displayFields()\n");

    QString txt = "<html><head></head><body>\n";
    txt += "<b>" + QString::fromLocal8Bit(m_url.c_str());
    if (!m_ipath.empty())
        txt += "|" + QString::fromUtf8(m_ipath.c_str());
    txt += "</b><br><br>";
    txt += "<dl>\n";
    for (const auto& entry: m_fdoc.meta) {
        if (!entry.second.empty())
            txt += "<dt>" + QString::fromUtf8(entry.first.c_str()) + "</dt> " 
                + "<dd>" + QString::fromUtf8(escapeHtml(entry.second).c_str()) 
                + "</dd>\n";
    }
    txt += "</dl></body></html>";
    setHtml(txt);
    m_curdsp = PTE_DSPFLDS;
}

void PreviewTextEdit::displayImage()
{
    LOGDEB1("PreviewTextEdit::displayImage()\n");
    if (m_image.isNull())
        displayText();

    setPlainText("");
    if (m_image.width() > width() || 
        m_image.height() > height()) {
        m_image = m_image.scaled(width(), height(), Qt::KeepAspectRatio);
    }
    document()->addResource(QTextDocument::ImageResource, QUrl("image"), 
                            m_image);
    QTextCursor cursor = textCursor();
    cursor.insertImage("image");
    m_curdsp = PTE_DSPIMG;
}

void PreviewTextEdit::mouseDoubleClickEvent(QMouseEvent *event)
{
    LOGDEB2("PreviewTextEdit::mouseDoubleClickEvent\n");
    QTextEdit::mouseDoubleClickEvent(event);
    if (textCursor().hasSelection() && m_preview)
        m_preview->emitWordSelect(textCursor().selectedText());
}

void PreviewTextEdit::print()
{
    LOGDEB("PreviewTextEdit::print\n");
    if (!m_preview)
        return;
        
#ifndef QT_NO_PRINTER
    QPrinter printer;
    QPrintDialog *dialog = new QPrintDialog(&printer, this);
    dialog->setWindowTitle(tr("Print Current Preview"));
    if (dialog->exec() != QDialog::Accepted)
        return;
    QTextEdit::print(&printer);
#endif
}
