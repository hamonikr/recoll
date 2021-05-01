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

#include <QMessageBox>
#include <QShortcut>

#include "log.h"
#include "internfile.h"
#include "rclzg.h"
#include "rclmain_w.h"

static const QKeySequence quitKeySeq("Ctrl+q");

// If a preview (toplevel) window gets closed by the user, we need to
// clean up because there is no way to reopen it. And check the case
// where the current one is closed
void RclMain::previewClosed(Preview *w)
{
    LOGDEB("RclMain::previewClosed(" << w << ")\n");
    if (w == curPreview) {
        LOGDEB("Active preview closed\n");
        curPreview = 0;
    } else {
        LOGDEB("Old preview closed\n");
    }
}

// Document up to date check. The main problem we try to solve is
// displaying the wrong message from a compacted mail folder.
//
// Also we should re-run the query after updating the index because
// the ipaths may be wrong in the current result list. For now, the
// user does this by clicking search again once the indexing is done
//
// We only do this for the main index, else jump and prey (cant update
// anyway, even the makesig() call might not make sense for our base
// config)
bool RclMain::containerUpToDate(Rcl::Doc& doc)
{
    static bool ignore_out_of_date_preview = false;

    // If ipath is empty, we decide we don't care. Also, we need an index, 
    if (ignore_out_of_date_preview || doc.ipath.empty() || rcldb == 0)
        return true;

    string udi;
    doc.getmeta(Rcl::Doc::keyudi, &udi);
    if (udi.empty()) {
        // Whatever...
        return true;
    }

    string sig;
    if (!FileInterner::makesig(theconfig, doc, sig)) {
        QMessageBox::warning(0, "Recoll", tr("Can't access file: ") + 
                             QString::fromLocal8Bit(doc.url.c_str()));
        // Let's try the preview anyway...
        return true;
    }

    if (!rcldb->needUpdate(udi, sig)) {
        // Alles ist in ordnung
        return true;
    }

    // Top level (container) document, for checking for indexing error
    string ctsig = "+";
    Rcl::Doc ctdoc;
    if (rcldb->getContainerDoc(doc, ctdoc)) {
        ctdoc.getmeta(Rcl::Doc::keysig, &ctsig);
    }
    
    // We can only run indexing on the main index (dbidx 0)
    bool ismainidx = rcldb->fromMainIndex(doc);
    // Indexer already running?
    bool ixnotact = (m_indexerState == IXST_NOTRUNNING);

    QString msg = tr("Index not up to date for this file.<br>");
    if (ctsig.back() == '+') {
        msg += tr("<em>Also, it seems that the last index update for the file "
                  "failed.</em><br/>");
    }
    if (ixnotact && ismainidx) {
        msg += tr("Click Ok to try to update the "
                  "index for this file. You will need to "
                  "run the query again when indexing is done.<br>");
    } else if (ismainidx) {
        msg += tr("The indexer is running so things should "
                  "improve when it's done. ");
    } else if (ixnotact) {
        // Not main index
        msg += tr("The document belongs to an external index "
                  "which I can't update. ");
    }
    msg += tr("Click Cancel to return to the list.<br>"
              "Click Ignore to show the preview anyway (and remember for "
              "this session). There is a risk of showing the wrong entry.<br/>");

    QMessageBox::StandardButtons bts = 
        QMessageBox::Ignore | QMessageBox::Cancel;

    if (ixnotact &&ismainidx)
        bts |= QMessageBox::Ok;

    int rep = 
        QMessageBox::warning(0, tr("Warning"), msg, bts,
                             (ixnotact && ismainidx) ? 
                             QMessageBox::Cancel : QMessageBox::NoButton);

    if (m_indexerState == IXST_NOTRUNNING && rep == QMessageBox::Ok) {
        LOGDEB("Requesting index update for " << doc.url << "\n");
        vector<Rcl::Doc> docs(1, doc);
        updateIdxForDocs(docs);
    }
    if (rep == QMessageBox::Ignore) {
        ignore_out_of_date_preview = true;
        return true;
    } else {
        return false;
    }
}

/** 
 * Open a preview window for a given document, or load it into new tab of 
 * existing window.
 *
 * @param docnum db query index
 * @param mod keyboards modifiers like ControlButton, ShiftButton
 */
void RclMain::startPreview(int docnum, Rcl::Doc doc, int mod)
{
    LOGDEB("startPreview(" << docnum << ", doc, " << mod << ")\n");

    if (!containerUpToDate(doc))
        return;

    // Do the zeitgeist thing
    zg_send_event(ZGSEND_PREVIEW, doc);

    if (mod & Qt::ShiftModifier) {
        // User wants new preview window
        curPreview = 0;
    }
    if (curPreview == 0) {
        HighlightData hdata;
        m_source->getTerms(hdata);
        curPreview = new Preview(this, reslist->listId(), hdata);

        if (curPreview == 0) {
            QMessageBox::warning(0, tr("Warning"), 
                                 tr("Can't create preview window"),
                                 QMessageBox::Ok, 
                                 QMessageBox::NoButton);
            return;
        }
        connect(new QShortcut(quitKeySeq, curPreview), SIGNAL (activated()), 
                this, SLOT (fileExit()));
        connect(curPreview, SIGNAL(previewClosed(Preview *)), 
                this, SLOT(previewClosed(Preview *)));
        connect(curPreview, SIGNAL(wordSelect(QString)),
                sSearch, SLOT(addTerm(QString)));
        connect(curPreview, SIGNAL(showNext(Preview *, int, int)),
                this, SLOT(previewNextInTab(Preview *, int, int)));
        connect(curPreview, SIGNAL(showPrev(Preview *, int, int)),
                this, SLOT(previewPrevInTab(Preview *, int, int)));
        connect(curPreview, SIGNAL(previewExposed(Preview *, int, int)),
                this, SLOT(previewExposed(Preview *, int, int)));
        connect(curPreview, SIGNAL(saveDocToFile(Rcl::Doc)), 
                this, SLOT(saveDocToFile(Rcl::Doc)));
        connect(curPreview, SIGNAL(editRequested(Rcl::Doc)), 
                this, SLOT(startNativeViewer(Rcl::Doc)));
        curPreview->setWindowTitle(getQueryDescription());
        curPreview->show();
    } 
    curPreview->makeDocCurrent(doc, docnum);
}

/** 
 * Open a preview window for a given document, no linking to result list
 *
 * This is used to show ie parent documents, which have no corresponding
 * entry in the result list.
 * 
 */
void RclMain::startPreview(Rcl::Doc doc)
{
    Preview *preview = new Preview(this, 0, HighlightData());
    if (preview == 0) {
        QMessageBox::warning(0, tr("Warning"), 
                             tr("Can't create preview window"),
                             QMessageBox::Ok, 
                             QMessageBox::NoButton);
        return;
    }
    connect(new QShortcut(quitKeySeq, preview), SIGNAL (activated()), 
            this, SLOT (fileExit()));
    connect(preview, SIGNAL(wordSelect(QString)),
            sSearch, SLOT(addTerm(QString)));
    // Do the zeitgeist thing
    zg_send_event(ZGSEND_PREVIEW, doc);
    preview->show();
    preview->makeDocCurrent(doc, 0);
}

// Show next document from result list in current preview tab
void RclMain::previewNextInTab(Preview * w, int sid, int docnum)
{
    previewPrevOrNextInTab(w, sid, docnum, true);
}

// Show previous document from result list in current preview tab
void RclMain::previewPrevInTab(Preview * w, int sid, int docnum)
{
    previewPrevOrNextInTab(w, sid, docnum, false);
}

// Combined next/prev from result list in current preview tab
void RclMain::previewPrevOrNextInTab(Preview * w, int sid, int docnum, bool nxt)
{
    LOGDEB("RclMain::previewNextInTab  sid " << sid << " docnum " << docnum <<
           ", listId " << reslist->listId() << "\n");

    if (w == 0) // ??
        return;

    if (sid != reslist->listId()) {
        QMessageBox::warning(0, "Recoll", 
                             tr("This search is not active any more"));
        return;
    }

    if (nxt)
        docnum++;
    else 
        docnum--;
    if (docnum < 0 || !m_source || docnum >= m_source->getResCnt()) {
        if (!prefs.noBeeps) {
            LOGDEB("Beeping\n");
            QApplication::beep();
        } else {
            LOGDEB("Not beeping because nobeep is set\n");
        }
        return;
    }

    Rcl::Doc doc;
    if (!reslist->getDoc(docnum, doc)) {
        QMessageBox::warning(0, "Recoll", 
                             tr("Cannot retrieve document info from database"));
        return;
    }
        
    w->makeDocCurrent(doc, docnum, true);
}

// Preview tab exposed: if the preview comes from the currently
// displayed result list, tell reslist (to color the paragraph)
void RclMain::previewExposed(Preview *, int sid, int docnum)
{
    LOGDEB2("RclMain::previewExposed: sid " << sid << " docnum " << docnum <<
            ", m_sid " << reslist->listId() << "\n");
    if (sid != reslist->listId()) {
        return;
    }
    reslist->previewExposed(docnum);
}
