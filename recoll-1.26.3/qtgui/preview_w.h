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
#ifndef _PREVIEW_W_H_INCLUDED_
#define _PREVIEW_W_H_INCLUDED_
#include "autoconfig.h"

// Always use a qtextbrowser for now, there is no compelling reason to
// switch to webkit here
#if 1 || defined(RESLIST_TEXTBROWSER)
#define PREVIEW_TEXTBROWSER
#endif

#include <stdio.h>

#include <memory>

#include <QComboBox>
#include <qvariant.h>
#include <qwidget.h>

#ifdef PREVIEW_TEXTBROWSER
#include <QTextBrowser>
#define PREVIEW_PARENTCLASS QTextBrowser
#else
#include <QtWebKit/QWebView>
#define PREVIEW_PARENTCLASS QWebView
#endif
#include <qimage.h>

#include "rcldb.h"
#include "plaintorich.h"
#include "rclmain_w.h"

#include "ui_preview.h"

class QTabWidget;
class QLabel;
class QPushButton;
class QCheckBox;
class Preview;
class PlainToRichQtPreview;
class QUrl;
class RclMain;

class PreviewTextEdit : public PREVIEW_PARENTCLASS {
    Q_OBJECT;
public:
    PreviewTextEdit(QWidget* parent, const char* name, Preview *pv);
    void moveToAnchor(const QString& name);
    enum DspType {PTE_DSPTXT, PTE_DSPFLDS, PTE_DSPIMG};

public slots:
    virtual void displayFields();
    virtual void displayText();
    virtual void displayImage();
    virtual void print();
    virtual void createPopupMenu(const QPoint& pos);
    void onAnchorClicked(const QUrl& url);

    friend class Preview;

protected:
    void mouseDoubleClickEvent(QMouseEvent *);

private:
    Preview *m_preview;
    std::shared_ptr<PlainToRichQtPreview> m_plaintorich;
    
    bool   m_dspflds;
    string m_url; // filename for this tab
    string m_ipath; // Internal doc path inside file
    int    m_docnum;  // Index of doc in db search results.

    // doc out of internfile (previous fields come from the index) with
    // main text erased (for space).
    Rcl::Doc m_fdoc; 

    // The input doc out of the index/query list
    Rcl::Doc m_dbdoc; 

    // Saved rich (or plain actually) text: the textedit seems to
    // sometimes (but not always) return its text stripped of tags, so
    // this is needed (for printing for example)
    QString  m_richtxt;
    Qt::TextFormat m_format;

    // Temporary file name (possibly, if displaying image). The
    // TempFile itself is kept inside main.cpp (because that's where
    // signal cleanup happens), but we use its name to ask for release
    // when the tab is closed.
    string m_tmpfilename;
    QImage m_image;
    DspType m_curdsp;
};


class Preview : public QDialog, public Ui::Preview {
    Q_OBJECT

public:

    Preview(RclMain *m, int sid, // Search Id
            const HighlightData& hdata) // Search terms etc. for highlighting
        : m_rclmain(m), m_searchId(sid), m_hData(hdata) {
        setupUi(this);
        init();
    }

    virtual void closeEvent(QCloseEvent *e);
    virtual bool eventFilter(QObject *target, QEvent *event);

    /** 
     * Arrange for the document to be displayed either by exposing the tab 
     * if already loaded, or by creating a new tab and loading it.
     * @para docnum is used to link back to the result list (to highlight 
     *   paragraph when tab exposed etc.
     */
    virtual bool makeDocCurrent(const Rcl::Doc& idoc, int docnum, 
                                bool sametab = false);
    void emitWordSelect(QString);
    friend class PreviewTextEdit;

public slots:
    // Search stuff
    virtual void searchTextChanged(const QString& text);
    virtual void doSearch(const QString& str, bool next, bool reverse,
                          bool wo = false);
    virtual void nextPressed();
    virtual void prevPressed();

    // Tabs management
    virtual void currentChanged(int);
    virtual void closeCurrentTab();
    virtual void closeTab(int index);
    virtual void emitShowNext();
    virtual void emitShowPrev();

    virtual void emitSaveDocToFile();
    virtual void emitEditRequested();
    virtual void togglePlainPre();

signals:
    void previewClosed(Preview *);
    void wordSelect(QString);
    void showNext(Preview *w, int sid, int docnum);
    void showPrev(Preview *w, int sid, int docnum);
    void previewExposed(Preview *w, int sid, int docnum);
    void printCurrentPreviewRequest();
    void saveDocToFile(Rcl::Doc);
    void editRequested(Rcl::Doc);

private:
    RclMain *m_rclmain;
    // Identifier of search in main window. This is used to check that
    // we make sense when requesting the next document when browsing
    // successive search results in a tab.
    int           m_searchId; 

    bool          m_dynSearchActive{false};
    // Index value the search text comes from. -1 if text was edited
    int           m_searchTextFromIndex{0};

    bool          m_canBeep{true};
    bool          m_loading{false};
    HighlightData m_hData;
    bool          m_justCreated{true}; // First tab create is different

    void init();
    virtual void setCurTabProps(const Rcl::Doc& doc, int docnum);
    virtual PreviewTextEdit *editor(int);
    virtual PreviewTextEdit *currentEditor();
    virtual PreviewTextEdit *addEditorTab();
    virtual bool loadDocInCurrentTab(const Rcl::Doc& idoc, int dnm);
};

#endif /* _PREVIEW_W_H_INCLUDED_ */
