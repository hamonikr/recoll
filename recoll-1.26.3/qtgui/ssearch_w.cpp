/* Copyright (C) 2006 J.F.Dockes
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

#include <sstream>
#include <set>
#include <vector>
#include <string>
#include <memory>

#include <qapplication.h>
#include <qinputdialog.h>
#include <qvariant.h>
#include <qpushbutton.h>
#include <qcombobox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qmessagebox.h>
#include <qevent.h>
#include <QCompleter>
#include <QAbstractItemView>
#include <QAbstractListModel>
#include <QModelIndex>
#include <QTimer>
#include <QListView>

#include "log.h"
#include "guiutils.h"
#include "searchdata.h"
#include "ssearch_w.h"
#include "textsplit.h"
#include "wasatorcl.h"
#include "rclhelp.h"
#include "xmltosd.h"
#include "smallut.h"
#include "rcldb.h"
#include "recoll.h"

using namespace std;

// Max search history matches displayed in completer
static const int maxhistmatch = 10;
// Max db term matches fetched from the index
static const int maxdbtermmatch = 20;
// Visible rows for the completer listview
static const int completervisibleitems = 20;

void RclCompleterModel::init()
{
    if (!clockPixmap.load(":/images/clock.png") ||
        !interroPixmap.load(":/images/interro.png")) {
        LOGERR("SSearch: pixmap loading failed\n");
    }
}

int RclCompleterModel::rowCount(const QModelIndex &) const
{
    LOGDEB1("RclCompleterModel::rowCount: " << currentlist.size() << "\n");
    return currentlist.size();
}

QVariant RclCompleterModel::data(const QModelIndex &index, int role) const
{
    LOGDEB1("RclCompleterModel::data: row: " << index.row() << " role " <<
            role << "\n");
    if (role != Qt::DisplayRole && role != Qt::EditRole &&
        role != Qt::DecorationRole) {
        return QVariant();
    }
    if (index.row() < 0 || index.row() >= int(currentlist.size())) {
        return QVariant();
    }

    if (role == Qt::DecorationRole) {
        LOGDEB1("RclCompleterModel::data: returning pixmap\n");
        return index.row() < firstfromindex ? QVariant(clockPixmap) :
            QVariant(interroPixmap);
    } else {
        LOGDEB1("RclCompleterModel::data: return: " <<
                qs2u8s(currentlist[index.row()]) << endl);
        return QVariant(currentlist[index.row()]);
    }
}

void RclCompleterModel::onPartialWord(
    int tp, const QString& _qtext, const QString& qpartial)
{
    string partial = qs2u8s(qpartial);
    QString qtext = _qtext.trimmed();
    bool onlyspace = qtext.isEmpty();
    LOGDEB1("RclCompleterModel::onPartialWord: [" << partial << "] onlyspace "<<
            onlyspace << "\n");
    
    currentlist.clear();
    beginResetModel();
    if ((prefs.ssearchNoComplete && !onlyspace) || tp == SSearch::SST_FNM) {
        // Nocomplete: only look at history by entering space
        // Filename: no completion for now. We'd need to termatch with
        // the right prefix?
        endResetModel();
        return;
    }

    int histmatch = 0;
    // Look for matches between the full entry and the search history
    // (anywhere in the string)
    for (int i = 0; i < prefs.ssearchHistory.count(); i++) {
        LOGDEB1("[" << qs2u8s(prefs.ssearchHistory[i]) << "] contains [" <<
                qs2u8s(qtext) << "] ?\n");
        // If there is current text, only show a limited count of
        // matching entries, else show the full history.
        if (onlyspace ||
            prefs.ssearchHistory[i].contains(qtext, Qt::CaseInsensitive)) {
            currentlist.push_back(prefs.ssearchHistory[i]);
            if (!onlyspace && ++histmatch >= maxhistmatch)
                break;
        }
    }
    firstfromindex = currentlist.size();

    // Look for Recoll terms beginning with the partial word
    if (!qpartial.trimmed().isEmpty()) {
        Rcl::TermMatchResult rclmatches;
        if (!rcldb->termMatch(Rcl::Db::ET_WILD, string(),
                              partial + "*", rclmatches, maxdbtermmatch)) {
            LOGDEB1("RclCompleterModel: termMatch failed: [" << partial + "*" <<
                    "]\n");
        } else {
            LOGDEB1("RclCompleterModel: termMatch cnt: " <<
                    rclmatches.entries.size() << endl);
        }
        for (const auto& entry : rclmatches.entries) {
            LOGDEB1("RclCompleterModel: match " << entry.term << endl);
            string data = entry.term;
            currentlist.push_back(u8s2qs(data));
        }
    }
    endResetModel();
    QTimer::singleShot(0, m_parent, SLOT(onCompleterShown()));
}

void SSearch::init()
{
    // See enum in .h and keep in order !
    searchTypCMB->addItem(tr("Any term"));
    searchTypCMB->addItem(tr("All terms"));
    searchTypCMB->addItem(tr("File name"));
    searchTypCMB->addItem(tr("Query language"));
    
    connect(queryText, SIGNAL(returnPressed()), this, SLOT(startSimpleSearch()));
    connect(queryText, SIGNAL(textChanged(const QString&)),
            this, SLOT(searchTextChanged(const QString&)));
    connect(queryText, SIGNAL(textEdited(const QString&)),
            this, SLOT(searchTextEdited(const QString&)));
    connect(clearqPB, SIGNAL(clicked()), queryText, SLOT(clear()));
    connect(searchPB, SIGNAL(clicked()), this, SLOT(startSimpleSearch()));
    connect(searchTypCMB, SIGNAL(activated(int)), this,
            SLOT(searchTypeChanged(int)));

    m_completermodel = new RclCompleterModel(this);
    m_completer = new QCompleter(m_completermodel, this);
    m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    m_completer->setFilterMode(Qt::MatchContains);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setMaxVisibleItems(completervisibleitems);
    queryText->setCompleter(m_completer);
    m_completer->popup()->installEventFilter(this);
    queryText->installEventFilter(this);
    connect(this, SIGNAL(partialWord(int, const QString&, const QString&)),
            m_completermodel,
            SLOT(onPartialWord(int,const QString&,const QString&)));
    connect(m_completer, SIGNAL(activated(const QString&)), this,
            SLOT(onCompletionActivated(const QString&)));
    connect(historyPB, SIGNAL(clicked()), this, SLOT(onHistoryClicked()));
}

void SSearch::takeFocus()
{
    LOGDEB1("SSearch: take focus\n");
    queryText->setFocus(Qt::ShortcutFocusReason);
    // If the focus was already in the search entry, the text is not selected.
    // Do it for consistency
    queryText->selectAll();
}

QString SSearch::currentText()
{
    return queryText->text();
}

void SSearch::clearAll()
{
    queryText->clear();
}

void SSearch::onCompleterShown()
{
    LOGDEB("SSearch::onCompleterShown\n");
    QCompleter *completer = queryText->completer();
    if (!completer) {
        LOGDEB0("SSearch::onCompleterShown: no completer\n");
        return;
    }
    QAbstractItemView *popup =  completer->popup();
    if (!popup) {
        LOGDEB0("SSearch::onCompleterShown: no popup\n");
        return;
    }
    QVariant data = popup->model()->data(popup->currentIndex());
    if (!data.isValid()) {
        LOGDEB0("SSearch::onCompleterShown: data not valid\n");
        return;
    }
    // Test if the completer text begins with the current input.
    QString text = data.toString();
    if (!text.lastIndexOf(queryText->text()) == 0) {
        return;
    }
    
    LOGDEB0("SSearch::onCompleterShown:" <<  
            " current [" << qs2utf8s(currentText()) <<
            "] saved [" << qs2utf8s(m_savedEditText) <<
            "] popup [" << qs2utf8s(text) << "]\n");

    // We append the completion part to the end of the current input,
    // line, and select it so that the user has a clear indication of
    // what will happen if they type Enter.
    int pos = queryText->cursorPosition();
    int len = text.size() - currentText().size();
    queryText->setText(text);
    queryText->setCursorPosition(pos);
    queryText->setSelection(pos, len);
}

// This is to avoid that if the user types Backspace or Del while we
// have inserted / selected the current completion, the lineedit text
// goes back to what it was, the completion fires, and it looks like
// nothing was typed. So we disable the completion if a
bool SSearch::eventFilter(QObject *target, QEvent *event)
{
    Q_UNUSED(target);
    LOGDEB1("SSearch::eventFilter: event\n");
    if (event->type() != QEvent::KeyPress) {
        return false;
    }
    LOGDEB1("SSearch::eventFilter: KeyPress event. Target " << target <<
            " popup "<<m_completer->popup() << " lineedit "<<queryText<< "\n");

    QKeyEvent *keyEvent = (QKeyEvent *)event;
    if (keyEvent->key() == Qt::Key_Backspace) {
        LOGDEB("SSearch::eventFilter: backspace\n");
        queryText->setCompleter(nullptr);
        queryText->backspace();
        return true;
    } else if (keyEvent->key()==Qt::Key_Delete) {
        LOGDEB("SSearch::eventFilter: delete\n");
        queryText->setCompleter(nullptr);
        queryText->del();
        return true;
    } else {
        if (nullptr == queryText->completer()) {
            queryText->setCompleter(m_completer);
        }
    }        
    return false;
}

// onCompletionActivated() is called when an entry is selected in the
// popup, but the edit text is going to be replaced in any case if
// there is a current match (we can't prevent it in the signal). If
// there is no match (e.g. the user clicked the history button and
// selected an entry), the query text will not be set.
// So:
//  - We set the query text to the popup activation value in all cases
//  - We schedule a callback to set the text to what we want (which is the
//    concatenation of the user entry before the current partial word and the
//    pop up data.
//  - Note that a history click will replace a current partial word,
//    so that the effect is different if there is a space at the end
//    of the entry or not: pure concatenation vs replacement of the
//    last (partial) word.
void SSearch::restoreText()
{
    LOGDEB("SSearch::restoreText: savedEdit: " << qs2u8s(m_savedEditText) <<
           endl);
    if (!m_savedEditText.trimmed().isEmpty()) {
        // If the popup text begins with the saved text, just let it replace
        if (currentText().lastIndexOf(m_savedEditText) != 0) {
            queryText->setText(m_savedEditText.trimmed() + " " + currentText());
        }
        m_savedEditText = "";
    }        
    queryText->setFocus();
    if (prefs.ssearchStartOnComplete) {
        QTimer::singleShot(0, this, SLOT(startSimpleSearch()));
    }
}
void SSearch::onCompletionActivated(const QString& text)
{
    LOGDEB("SSearch::onCompletionActivated: queryText [" <<
           qs2u8s(currentText()) << "] text [" << qs2u8s(text) << "]\n");
    queryText->setText(text);
    QTimer::singleShot(0, this, SLOT(restoreText()));
}

void SSearch::onHistoryClicked()
{
    if (m_completermodel) {
        queryText->setCompleter(m_completer);
        m_completermodel->onPartialWord(SST_LANG, "", "");
        queryText->completer()->complete();
    }
}

void SSearch::searchTextEdited(const QString& text)
{
    LOGDEB1("SSearch::searchTextEdited: text [" << qs2u8s(text) << "]\n");
    QString pword;
    int cs = getPartialWord(pword);
    int tp = searchTypCMB->currentIndex();
    
    m_savedEditText = text.left(cs);
    LOGDEB1("SSearch::searchTextEdited: cs " <<cs<<" pword ["<< qs2u8s(pword) <<
            "] savedEditText [" << qs2u8s(m_savedEditText) << "]\n");
    if (cs >= 0) {
        emit partialWord(tp, currentText(), pword);
    } else {
        emit partialWord(tp, currentText(), " ");
    }
}

void SSearch::searchTextChanged(const QString& text)
{
    LOGDEB1("SSearch::searchTextChanged: text [" << qs2u8s(text) << "]\n");

    if (text.isEmpty()) {
        searchPB->setEnabled(false);
        clearqPB->setEnabled(false);
        queryText->setFocus();
        emit clearSearch();
    } else {
        searchPB->setEnabled(true);
        clearqPB->setEnabled(true);
    }
}

void SSearch::searchTypeChanged(int typ)
{
    LOGDEB1("Search type now " << typ << "\n");
    // Adjust context help
    if (typ == SST_LANG) {
        HelpClient::installMap((const char *)this->objectName().toUtf8(), 
                               "RCL.SEARCH.LANG");
    } else {
        HelpClient::installMap((const char *)this->objectName().toUtf8(), 
                               "RCL.SEARCH.GUI.SIMPLE");
    }
    // Also fix tooltips
    switch (typ) {
    case SST_LANG:
        queryText->setToolTip(
            // Do not modify the text here, test with the
            // sshelp/qhelp.html file and a browser, then use
            // sshelp/helphtmltoc.sh to turn to code and insert here
            tr("<html><head><style>") +
            tr("table, th, td {") +
            tr("border: 1px solid black;") +
            tr("border-collapse: collapse;") +
            tr("}") +
            tr("th,td {") +
            tr("text-align: center;") +
            tr("}") +
            tr("</style></head><body>") +
            tr("<p>Query language cheat-sheet. In doubt: click <b>Show Query</b>.&nbsp;") +
            tr("You should really look at the manual (F1)</p>") +
            tr("<table border='1' cellspacing='0'>") +
            tr("<tr><th>What</th><th>Examples</th>") +
            tr("<tr><td>And</td><td>one two&nbsp;&nbsp;&nbsp;one AND two&nbsp;&nbsp;&nbsp;one && two</td></tr>") +
            tr("<tr><td>Or</td><td>one OR two&nbsp;&nbsp;&nbsp;one || two</td></tr>") +
            tr("<tr><td>Complex boolean. OR has priority, use parentheses&nbsp;") +
            tr("where needed</td><td>(one AND two) OR three</td></tr>") +
            tr("<tr><td>Not</td><td>-term</td></tr>") +
            tr("<tr><td>Phrase</td><td>\"pride and prejudice\"</td></tr>") +
            tr("<tr><td>Ordered proximity (slack=1)</td><td>\"pride prejudice\"o1</td></tr>") +
            tr("<tr><td>Unordered proximity (slack=1)</td><td>\"prejudice pride\"po1</td></tr>") +
            tr("<tr><td>Unordered prox. (default slack=10)</td><td>\"prejudice&nbsp;pride\"p</td></tr>") +
            tr("<tr><td>No stem expansion: capitalize</td><td>Floor</td></tr>") +
            tr("<tr><td>Field-specific</td><td>author:austen&nbsp;&nbsp;title:prejudice</td></tr>") +
            tr("<tr><td>AND inside field (no order)</td><td>author:jane,austen</td></tr>") +
            tr("<tr><td>OR inside field</td><td>author:austen/bronte</td></tr>") +
            tr("<tr><td>Field names</td><td>title/subject/caption&nbsp;&nbsp;author/from<br>recipient/to&nbsp;&nbsp;filename&nbsp;&nbsp;ext</td></tr>") +
            tr("<tr><td>Directory path filter</td><td>dir:/home/me&nbsp;&nbsp;dir:doc</td></tr>") +
            tr("<tr><td>MIME type filter</td><td>mime:text/plain mime:video/*</td></tr>") +
            tr("<tr><td>Date intervals</td><td>date:2018-01-01/2018-31-12<br>") +
            tr("date:2018&nbsp;&nbsp;date:2018-01-01/P12M</td></tr>") +
            tr("<tr><td>Size</td><td>size&gt;100k size&lt;1M</td></tr>") +
            tr("</table></body></html>")
            );
        break;
    case SST_FNM:
        queryText->setToolTip(tr("Enter file name wildcard expression."));
        break;
    case SST_ANY:
    case SST_ALL:
    default:
        queryText->setToolTip(tr("Enter search terms here."));
    }
}

void SSearch::startSimpleSearch()
{
    if (queryText->completer() && queryText->completer()->popup()->isVisible()) {
        return;
    }
    string u8 = qs2u8s(queryText->text());
    trimstring(u8);
    if (u8.length() == 0)
        return;

    if (!startSimpleSearch(u8))
        return;

    // Search terms history.
    // New text at the front and erase any older identical entry
    QString txt = currentText().trimmed();
    if (txt.isEmpty())
        return;
    if (prefs.historysize) {
        prefs.ssearchHistory.insert(0, txt);
        prefs.ssearchHistory.removeDuplicates();
    }
    if (prefs.historysize >= 0) {
        for (int i = (int)prefs.ssearchHistory.count();
             i > prefs.historysize; i--) {
            prefs.ssearchHistory.removeLast();
        }
    }
}

void SSearch::setPrefs()
{
}

string SSearch::asXML()
{
    return m_xml;
}

bool SSearch::startSimpleSearch(const string& u8, int maxexp)
{
    LOGDEB("SSearch::startSimpleSearch(" << u8 << ")\n");
    string stemlang = prefs.stemlang();

    ostringstream xml;
    xml << "<SD type='ssearch'>\n";
    xml << "  <SL>" << stemlang << "</SL>\n";
    xml << "  <T>" << base64_encode(u8) << "</T>\n";

    SSearchType tp = (SSearchType)searchTypCMB->currentIndex();
    Rcl::SearchData *sdata = 0;

    if (tp == SST_LANG) {
        xml << "  <SM>QL</SM>\n";
        string reason;
        if (prefs.autoSuffsEnable) {
            sdata = wasaStringToRcl(theconfig, stemlang, u8, reason, 
                                    (const char *)prefs.autoSuffs.toUtf8());
            if (!prefs.autoSuffs.isEmpty()) {
                xml <<  "  <AS>" << qs2u8s(prefs.autoSuffs) << "</AS>\n";
            }
        } else {
            sdata = wasaStringToRcl(theconfig, stemlang, u8, reason);
        }
        if (sdata == 0) {
            QMessageBox::warning(0, "Recoll", tr("Bad query string") + ": " +
                                 QString::fromUtf8(reason.c_str()));
            return false;
        }
    } else {
        sdata = new Rcl::SearchData(Rcl::SCLT_OR, stemlang);
        if (sdata == 0) {
            QMessageBox::warning(0, "Recoll", tr("Out of memory"));
            return false;
        }
        Rcl::SearchDataClause *clp = 0;
        if (tp == SST_FNM) {
            xml << "  <SM>FN</SM>\n";
            clp = new Rcl::SearchDataClauseFilename(u8);
        } else {
            // ANY or ALL, several words.
            if (tp == SST_ANY) {
                xml << "  <SM>OR</SM>\n";
                clp = new Rcl::SearchDataClauseSimple(Rcl::SCLT_OR, u8);
            } else {
                xml << "  <SM>AND</SM>\n";
                clp = new Rcl::SearchDataClauseSimple(Rcl::SCLT_AND, u8);
            }
        }
        sdata->addClause(clp);
    }

    if (prefs.ssearchAutoPhrase && rcldb) {
        xml << "  <AP/>\n";
        sdata->maybeAddAutoPhrase(*rcldb, 
                                  prefs.ssearchAutoPhraseThreshPC / 100.0);
    }
    if (maxexp != -1) {
        sdata->setMaxExpand(maxexp);
    }

    for (const auto& dbdir : prefs.activeExtraDbs) {
        xml << "  <EX>" << base64_encode(dbdir) << "</EX>";
    }

    xml << "</SD>\n";
    m_xml = xml.str();
    LOGDEB("SSearch::startSimpleSearch:xml:[" << m_xml << "]\n");

    std::shared_ptr<Rcl::SearchData> rsdata(sdata);
    emit setDescription(u8s2qs(u8));
    emit startSearch(rsdata, true);
    return true;
}

bool SSearch::fromXML(const SSearchDef& fxml)
{
    string asString;
    set<string> cur;
    set<string> stored;

    // Retrieve current list of stemlangs. prefs returns a
    // space-separated list Warn if stored differs from current,
    // but don't change the latter.
    stringToStrings(prefs.stemlang(), cur);
    stored = set<string>(fxml.stemlangs.begin(), fxml.stemlangs.end());
    stringsToString(fxml.stemlangs, asString);
    if (cur != stored) {
        QMessageBox::warning(
            0, "Recoll", tr("Stemming languages for stored query: ") + 
            QString::fromUtf8(asString.c_str()) + 
            tr(" differ from current preferences (kept)"));
    }

    // Same for autosuffs
    stringToStrings(qs2u8s(prefs.autoSuffs), cur);
    stored = set<string>(fxml.autosuffs.begin(), fxml.autosuffs.end());
    stringsToString(fxml.stemlangs, asString);
    if (cur != stored) {
        QMessageBox::warning(
            0, "Recoll", tr("Auto suffixes for stored query: ") + 
            QString::fromUtf8(asString.c_str()) + 
            tr(" differ from current preferences (kept)"));
    }

    cur = set<string>(prefs.activeExtraDbs.begin(), prefs.activeExtraDbs.end());
    stored = set<string>(fxml.extindexes.begin(), fxml.extindexes.end());
    stringsToString(fxml.extindexes, asString);
    if (cur != stored) {
        QMessageBox::warning(
            0, "Recoll", tr("External indexes for stored query: ") + 
            QString::fromUtf8(asString.c_str()) + 
            tr(" differ from current preferences (kept)"));
    }

    if (prefs.ssearchAutoPhrase && !fxml.autophrase) {
        QMessageBox::warning(
            0, "Recoll", 
            tr("Autophrase is set but it was unset for stored query"));
    } else if (!prefs.ssearchAutoPhrase && fxml.autophrase) {
        QMessageBox::warning(
            0, "Recoll", 
            tr("Autophrase is unset but it was set for stored query"));
    }
    setSearchString(QString::fromUtf8(fxml.text.c_str()));
    // We used to use prefs.ssearchTyp here. Not too sure why?
    // Minimize user surprise factor ? Anyway it seems cleaner to
    // restore the saved search type
    searchTypCMB->setCurrentIndex(fxml.mode);
    return true;
}

void SSearch::setSearchString(const QString& txt)
{
    queryText->setText(txt);
}

bool SSearch::hasSearchString()
{
    return !currentText().isEmpty();
}

// Add term to simple search. Term comes out of double-click in
// reslist or preview. 
// It would probably be better to cleanup in preview.ui.h and
// reslist.cpp and do the proper html stuff in the latter case
// (which is different because it format is explicit richtext
// instead of auto as for preview, needed because it's built by
// fragments?).
static const char* punct = " \t()<>\"'[]{}!^*.,:;\n\r";
void SSearch::addTerm(QString term)
{
    LOGDEB("SSearch::AddTerm: [" << qs2u8s(term) << "]\n");
    string t = (const char *)term.toUtf8();
    string::size_type pos = t.find_last_not_of(punct);
    if (pos == string::npos)
        return;
    t = t.substr(0, pos+1);
    pos = t.find_first_not_of(punct);
    if (pos != string::npos)
        t = t.substr(pos);
    if (t.empty())
        return;
    term = QString::fromUtf8(t.c_str());

    QString text = currentText();
    text += QString::fromLatin1(" ") + term;
    queryText->setText(text);
}

void SSearch::onWordReplace(const QString& o, const QString& n)
{
    LOGDEB("SSearch::onWordReplace: o [" << qs2u8s(o) << "] n [" <<
           qs2u8s(n) << "]\n");
    QString txt = currentText();
    QRegExp exp = QRegExp(QString("\\b") + o + QString("\\b"));
    exp.setCaseSensitivity(Qt::CaseInsensitive);
    txt.replace(exp, n);
    queryText->setText(txt);
    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers ();
    if (mods == Qt::NoModifier)
        startSimpleSearch();
}

void SSearch::setAnyTermMode()
{
    searchTypCMB->setCurrentIndex(SST_ANY);
}

// If text does not end with space, return last (partial) word and >0
// else return -1
int SSearch::getPartialWord(QString& word)
{
    // Extract last word in text
    QString txt = currentText();
    if (txt.isEmpty()) {
        return -1;
    }
    int lstidx = txt.size()-1;

    // If the input ends with a space or dquote (phrase input), or
    // dquote+qualifiers, no partial word.
    if (txt[lstidx] == ' ') {
        return -1;
    }
    int cs = txt.lastIndexOf("\"");
    if (cs > 0) {
        bool dquoteToEndNoSpace{true};
        for (int i = cs; i <= lstidx; i++) {
            if (txt[i] == ' ') {
                dquoteToEndNoSpace = false;
                break;
            }
        }
        if (dquoteToEndNoSpace) {
            return -1;
        }
    }

    cs = txt.lastIndexOf(" ");
    if (cs < 0)
        cs = 0;
    else
        cs++;
    word = txt.right(txt.size() - cs);
    return cs;
}
