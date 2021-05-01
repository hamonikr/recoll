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

#include <stdio.h>

#include <algorithm>
#include <list>
#include <map>
#include <string>

#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qcombobox.h>
#include <QTableWidget>
#include <QHeaderView>
#include <QClipboard>
#include <QKeyEvent>

#include "log.h"
#include "recoll.h"
#include "spell_w.h"
#include "guiutils.h"
#include "rcldb.h"
#include "searchdata.h"
#include "rclquery.h"
#include "rclhelp.h"
#include "wasatorcl.h"
#include "execmd.h"
#include "indexer.h"
#include "fstreewalk.h"

using std::list;
using std::multimap;
using std::string;

inline bool wordlessMode(SpellW::comboboxchoice v)
{
    return (v == SpellW::TYPECMB_STATS || v == SpellW::TYPECMB_FAILED);
}

void SpellW::init()
{
    m_c2t.clear();
    expTypeCMB->addItem(tr("Wildcards"));
    m_c2t.push_back(TYPECMB_WILD);
    expTypeCMB->addItem(tr("Regexp"));
    m_c2t.push_back(TYPECMB_REG);
    expTypeCMB->addItem(tr("Stem expansion"));
    m_c2t.push_back(TYPECMB_STEM);
    expTypeCMB->addItem(tr("Spelling/Phonetic"));
    m_c2t.push_back(TYPECMB_SPELL);
    expTypeCMB->addItem(tr("Show index statistics"));
    m_c2t.push_back(TYPECMB_STATS);
    expTypeCMB->addItem(tr("List files which could not be indexed (slow)"));
    m_c2t.push_back(TYPECMB_FAILED);

    // Stemming language combobox
    stemLangCMB->clear();
    vector<string> langs;
    if (!getStemLangs(langs)) {
	QMessageBox::warning(0, "Recoll", 
			     tr("error retrieving stemming languages"));
    }
    for (vector<string>::const_iterator it = langs.begin(); 
	 it != langs.end(); it++) {
	stemLangCMB->addItem(u8s2qs(*it));
    }

    (void)new HelpClient(this);
    HelpClient::installMap((const char *)this->objectName().toUtf8(), 
			   "RCL.SEARCH.GUI.TERMEXPLORER");

    // signals and slots connections
    connect(baseWordLE, SIGNAL(textChanged(const QString&)), 
	    this, SLOT(wordChanged(const QString&)));
    connect(baseWordLE, SIGNAL(returnPressed()), this, SLOT(doExpand()));
    connect(expandPB, SIGNAL(clicked()), this, SLOT(doExpand()));
    connect(dismissPB, SIGNAL(clicked()), this, SLOT(close()));
    connect(expTypeCMB, SIGNAL(activated(int)), this, SLOT(onModeChanged(int)));

    resTW->setShowGrid(0);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    resTW->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
#else
    resTW->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
#endif
    resTW->verticalHeader()->setDefaultSectionSize(20); 
    connect(resTW,
	   SIGNAL(cellDoubleClicked(int, int)),
            this, SLOT(textDoubleClicked(int, int)));

    resTW->setColumnWidth(0, 200);
    resTW->setColumnWidth(1, 150);
    resTW->installEventFilter(this);

    int idx = cmbIdx((comboboxchoice)prefs.termMatchType);
    expTypeCMB->setCurrentIndex(idx);
    onModeChanged(idx);
}

int SpellW::cmbIdx(comboboxchoice mode)
{
    vector<comboboxchoice>::const_iterator it = 
	std::find(m_c2t.begin(), m_c2t.end(), mode);
    if (it == m_c2t.end())
	it = m_c2t.begin();
    return it - m_c2t.begin();
}

static const int maxexpand = 10000;

/* Expand term according to current mode */
void SpellW::doExpand()
{
    int idx = expTypeCMB->currentIndex();
    if (idx < 0 || idx >= int(m_c2t.size()))
	idx = 0;
    comboboxchoice mode = m_c2t[idx];

    // Can't clear qt4 table widget: resets column headers too
    resTW->setRowCount(0);
    if (baseWordLE->text().isEmpty() && !wordlessMode(mode)) 
	return;

    string reason;
    if (!maybeOpenDb(reason)) {
	QMessageBox::critical(0, "Recoll", QString(reason.c_str()));
	LOGDEB("SpellW::doExpand: db error: "  << (reason) << "\n" );
	return;
    }

    int mt;
    switch(mode) {
    case TYPECMB_WILD: mt = Rcl::Db::ET_WILD; break;
    case TYPECMB_REG: mt = Rcl::Db::ET_REGEXP; break;
    case TYPECMB_STEM: mt = Rcl::Db::ET_STEM; break;
    default: mt = Rcl::Db::ET_WILD;
    }
    if (caseSensCB->isChecked()) {
	mt |= Rcl::Db::ET_CASESENS;
    }
    if (diacSensCB->isChecked()) {
	mt |= Rcl::Db::ET_DIACSENS;
    }
    Rcl::TermMatchResult res;
    string expr = string((const char *)baseWordLE->text().toUtf8());
    Rcl::DbStats dbs;
    rcldb->dbStats(dbs, false);

    switch (mode) {
    case TYPECMB_WILD: 
    default:
    case TYPECMB_REG:
    case TYPECMB_STEM:
    {
	string l_stemlang = qs2utf8s(stemLangCMB->currentText());

	if (!rcldb->termMatch(mt, l_stemlang, expr, res, maxexpand)) {
	    LOGERR("SpellW::doExpand:rcldb::termMatch failed\n" );
	    return;
	}
        statsLBL->setText(tr("Index: %1 documents, average length %2 terms."
			     "%3 results")
                          .arg(dbs.dbdoccount).arg(dbs.dbavgdoclen, 0, 'f', 0)
			  .arg(res.entries.size()));
    }
        
    break;

    case TYPECMB_SPELL: 
    {
	LOGDEB("SpellW::doExpand: spelling [" << expr << "]\n" );
	vector<string> suggs;
	if (!rcldb->getSpellingSuggestions(expr, suggs)) {
	    QMessageBox::warning(0, "Recoll", tr("Spell expansion error. "));
	}
	for (const auto& it : suggs) {
	    res.entries.push_back(Rcl::TermMatchEntry(it));
        }
        statsLBL->setText(tr("%1 results").arg(res.entries.size()));
    }
    break;

    case TYPECMB_STATS: 
    {
	showStats();
	return;
    }
    break;
    case TYPECMB_FAILED:
    {
	showFailed();
	return;
    }
    break;
    }

    if (res.entries.empty()) {
        resTW->setItem(0, 0, new QTableWidgetItem(tr("No expansion found")));
    } else {
        int row = 0;

	if (maxexpand > 0 && int(res.entries.size()) >= maxexpand) {
	    resTW->setRowCount(row + 1);
	    resTW->setSpan(row, 0, 1, 2);
	    resTW->setItem(row++, 0, 
			   new QTableWidgetItem(
			       tr("List was truncated alphabetically, "
				  "some frequent "))); 
	    resTW->setRowCount(row + 1);
	    resTW->setSpan(row, 0, 1, 2);
	    resTW->setItem(row++, 0, new QTableWidgetItem(
			       tr("terms may be missing. "
				  "Try using a longer root.")));
	    resTW->setRowCount(row + 1);
	    resTW->setItem(row++, 0, new QTableWidgetItem(""));
	}

	for (vector<Rcl::TermMatchEntry>::iterator it = res.entries.begin(); 
	     it != res.entries.end(); it++) {
	    LOGDEB2("SpellW::expand: " << it->wcf << " [" << it->term << "]\n");
	    char num[30];
	    if (it->wcf)
		sprintf(num, "%d / %d",  it->docs, it->wcf);
	    else
		num[0] = 0;
	    resTW->setRowCount(row+1);
            resTW->setItem(row, 0, new QTableWidgetItem(u8s2qs(it->term)));
            resTW->setItem(row++, 1, 
                             new QTableWidgetItem(QString::fromUtf8(num)));
	}
    }
}

void SpellW::showStats()
{
    statsLBL->setText("");
    int row = 0;

    Rcl::DbStats res;
    if (!rcldb->dbStats(res, false)) {
	LOGERR("SpellW::doExpand:rcldb::dbStats failed\n" );
	return;
    }

    resTW->setRowCount(row+1);
    resTW->setItem(row, 0,
		   new QTableWidgetItem(tr("Number of documents")));
    resTW->setItem(row++, 1, new QTableWidgetItem(
		       QString::number(res.dbdoccount)));

    resTW->setRowCount(row+1);
    resTW->setItem(row, 0,
		   new QTableWidgetItem(tr("Average terms per document")));
    resTW->setItem(row++, 1, new QTableWidgetItem(
		       QString::number(res.dbavgdoclen, 'f', 0)));

    resTW->setRowCount(row+1);
    resTW->setItem(row, 0,
		   new QTableWidgetItem(tr("Smallest document length (terms)")));
    resTW->setItem(row++, 1, new QTableWidgetItem(
		       QString::number(res.mindoclen)));

    resTW->setRowCount(row+1);
    resTW->setItem(row, 0,
		   new QTableWidgetItem(tr("Longest document length (terms)")));
    resTW->setItem(row++, 1, new QTableWidgetItem(
		       QString::number(res.maxdoclen)));

    if (!theconfig)
	return;

    ConfSimple cs(theconfig->getIdxStatusFile().c_str(), 1);
    DbIxStatus st;
    cs.get("fn", st.fn);
    cs.get("docsdone", &st.docsdone);
    cs.get("filesdone", &st.filesdone);
    cs.get("fileerrors", &st.fileerrors);
    cs.get("dbtotdocs", &st.dbtotdocs);
    cs.get("totfiles", &st.totfiles);

    resTW->setRowCount(row+1);
    resTW->setItem(row, 0,
		   new QTableWidgetItem(tr("Results from last indexing:")));
    resTW->setItem(row++, 1, new QTableWidgetItem(""));
    resTW->setRowCount(row+1);
    resTW->setItem(row, 0,
		   new QTableWidgetItem(tr("  Documents created/updated")));
    resTW->setItem(row++, 1,
                   new QTableWidgetItem(QString::number(st.docsdone)));
    resTW->setRowCount(row+1);
    resTW->setItem(row, 0,
		   new QTableWidgetItem(tr("  Files tested")));
    resTW->setItem(row++, 1,
                   new QTableWidgetItem(QString::number(st.filesdone)));
    resTW->setRowCount(row+1);
    resTW->setItem(row, 0,
		   new QTableWidgetItem(tr("  Unindexed files")));
    resTW->setItem(row++, 1,
                   new QTableWidgetItem(QString::number(st.fileerrors)));

    baseWordLE->setText(QString::fromLocal8Bit(theconfig->getDbDir().c_str()));

    int64_t dbkbytes = fsTreeBytes(theconfig->getDbDir()) / 1024;
    if (dbkbytes < 0) {
	dbkbytes = 0;
    }
    resTW->setRowCount(row+1);
    resTW->setItem(row, 0,
		   new QTableWidgetItem(tr("Database directory size")));
    resTW->setItem(row++, 1, new QTableWidgetItem(
		       u8s2qs(displayableBytes(dbkbytes*1024))));

    vector<string> allmimetypes = theconfig->getAllMimeTypes();
    multimap<int, string> mtbycnt;
    for (vector<string>::const_iterator it = allmimetypes.begin();
	 it != allmimetypes.end(); it++) {
	string reason;
	string q = string("mime:") + *it;
	Rcl::SearchData *sd = wasaStringToRcl(theconfig, "", q, reason);
	std::shared_ptr<Rcl::SearchData> rq(sd);
	Rcl::Query query(rcldb.get());
	if (!query.setQuery(rq)) {
	    LOGERR("Query setup failed: "  << (query.getReason()) << "" );
	    return;
	}
	int cnt = query.getResCnt();
	mtbycnt.insert(pair<int,string>(cnt,*it));
    }
    resTW->setRowCount(row+1);
    resTW->setItem(row, 0, new QTableWidgetItem(tr("MIME types:")));
    resTW->setItem(row++, 1, new QTableWidgetItem(""));

    for (multimap<int, string>::const_reverse_iterator it = mtbycnt.rbegin();
	 it != mtbycnt.rend(); it++) {
	resTW->setRowCount(row+1);
	resTW->setItem(row, 0, new QTableWidgetItem(QString("    ") +
                                                    u8s2qs(it->second)));
	resTW->setItem(row++, 1, new QTableWidgetItem(
			   QString::number(it->first)));
    }
}

void SpellW::showFailed()
{
    statsLBL->setText("");
    int row = 0;

    Rcl::DbStats res;
    if (!rcldb->dbStats(res, true)) {
	LOGERR("SpellW::doExpand:rcldb::dbStats failed\n" );
	return;
    }
    for (auto entry : res.failedurls) {
	resTW->setRowCount(row+1);
	resTW->setItem(row, 0, new QTableWidgetItem(u8s2qs(entry)));
	resTW->setItem(row++, 1, new QTableWidgetItem(""));
    }
}

void SpellW::wordChanged(const QString &text)
{
    if (text.isEmpty()) {
	expandPB->setEnabled(false);
        resTW->setRowCount(0);
    } else {
	expandPB->setEnabled(true);
    }
}

void SpellW::textDoubleClicked() {}
void SpellW::textDoubleClicked(int row, int)
{
    QTableWidgetItem *item = resTW->item(row, 0);
    if (item)
        emit(wordSelect(item->text()));
}

void SpellW::onModeChanged(int idx)
{
    if (idx < 0 || idx > int(m_c2t.size()))
	return;
    comboboxchoice mode = m_c2t[idx];
    setModeCommon(mode);
}

void SpellW::setMode(comboboxchoice mode)
{
    expTypeCMB->setCurrentIndex(cmbIdx(mode));
    setModeCommon(mode);
}

void SpellW::setModeCommon(comboboxchoice mode)
{
    if (wordlessMode(m_prevmode)) {
        baseWordLE->setText("");
    }
    m_prevmode = mode;
    resTW->setRowCount(0);
    if (o_index_stripchars) {
	caseSensCB->setEnabled(false);
	diacSensCB->setEnabled(false);
    } else {
	caseSensCB->setEnabled(true);
	diacSensCB->setEnabled(true);
    }
   
    if (mode == TYPECMB_STEM) {
	stemLangCMB->setEnabled(true);
	diacSensCB->setChecked(false);
	diacSensCB->setEnabled(false);
	caseSensCB->setChecked(false);
	caseSensCB->setEnabled(false);
    } else {
	stemLangCMB->setEnabled(false);
    }

    if (wordlessMode(mode)) {
	baseWordLE->setEnabled(false);
	QStringList labels(tr("Item"));
	labels.push_back(tr("Value"));
	resTW->setHorizontalHeaderLabels(labels);
	diacSensCB->setEnabled(false);
	caseSensCB->setEnabled(false);
	doExpand();
    } else {
	baseWordLE->setEnabled(true);
	QStringList labels(tr("Term"));
	labels.push_back(tr("Doc. / Tot."));
	resTW->setHorizontalHeaderLabels(labels);
	prefs.termMatchType = mode;
    }
}

void SpellW::copy()
{
  QItemSelectionModel * selection = resTW->selectionModel();
  QModelIndexList indexes = selection->selectedIndexes();

  if(indexes.size() < 1)
    return;

  // QModelIndex::operator < sorts first by row, then by column. 
  // this is what we need
  std::sort(indexes.begin(), indexes.end());

  // You need a pair of indexes to find the row changes
  QModelIndex previous = indexes.first();
  indexes.removeFirst();
  QString selected_text;
  QModelIndex current;
  Q_FOREACH(current, indexes)
  {
    QVariant data = resTW->model()->data(previous);
    QString text = data.toString();
    // At this point `text` contains the text in one cell
    selected_text.append(text);
    // If you are at the start of the row the row number of the previous index
    // isn't the same.  Text is followed by a row separator, which is a newline.
    if (current.row() != previous.row())
    {
      selected_text.append(QLatin1Char('\n'));
    }
    // Otherwise it's the same row, so append a column separator, which is a tab.
    else
    {
      selected_text.append(QLatin1Char('\t'));
    }
    previous = current;
  }

  // add last element
  selected_text.append(resTW->model()->data(current).toString());
  selected_text.append(QLatin1Char('\n'));
  qApp->clipboard()->setText(selected_text, QClipboard::Selection);
  qApp->clipboard()->setText(selected_text, QClipboard::Clipboard);
}


bool SpellW::eventFilter(QObject *target, QEvent *event)
{
    if (event->type() != QEvent::KeyPress ||
	(target != resTW && target != resTW->viewport())) 
	return false;

    QKeyEvent *keyEvent = (QKeyEvent *)event;
    if(keyEvent->matches(QKeySequence::Copy) )
    {
	copy();
	return true;
    }
    return false;
}

