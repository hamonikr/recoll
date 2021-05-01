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

#include "advsearch_w.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qframe.h>
#include <qcheckbox.h>
#include <qevent.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qmessagebox.h>
#include <QShortcut>

#include <string>
#include <map>
#include <algorithm>
using namespace std;

#include "recoll.h"
#include "rclconfig.h"
#include "log.h"
#include "searchdata.h"
#include "guiutils.h"
#include "rclhelp.h"

static const int initclausetypes[] = {1, 3, 0, 2, 5};
static const unsigned int iclausescnt = sizeof(initclausetypes) / sizeof(int);
static map<QString,QString> cat_translations;
static map<QString,QString> cat_rtranslations;

void AdvSearch::init()
{
    (void)new HelpClient(this);
    HelpClient::installMap((const char *)objectName().toUtf8(), 
			   "RCL.SEARCH.GUI.COMPLEX");

    // signals and slots connections
    connect(delFiltypPB, SIGNAL(clicked()), this, SLOT(delFiltypPB_clicked()));
    connect(searchPB, SIGNAL(clicked()), this, SLOT(runSearch()));
    connect(filterDatesCB, SIGNAL(toggled(bool)), 
	    this, SLOT(filterDatesCB_toggled(bool)));
    connect(filterSizesCB, SIGNAL(toggled(bool)), 
	    this, SLOT(filterSizesCB_toggled(bool)));
    connect(restrictFtCB, SIGNAL(toggled(bool)), 
	    this, SLOT(restrictFtCB_toggled(bool)));
    connect(restrictCtCB, SIGNAL(toggled(bool)), 
	    this, SLOT(restrictCtCB_toggled(bool)));
    connect(dismissPB, SIGNAL(clicked()), this, SLOT(close()));
    connect(browsePB, SIGNAL(clicked()), this, SLOT(browsePB_clicked()));
    connect(addFiltypPB, SIGNAL(clicked()), this, SLOT(addFiltypPB_clicked()));

    connect(delAFiltypPB, SIGNAL(clicked()), 
	    this, SLOT(delAFiltypPB_clicked()));
    connect(addAFiltypPB, SIGNAL(clicked()), 
	    this, SLOT(addAFiltypPB_clicked()));
    connect(saveFileTypesPB, SIGNAL(clicked()), 
	    this, SLOT(saveFileTypes()));
    connect(addClausePB, SIGNAL(clicked()), this, SLOT(addClause()));
    connect(delClausePB, SIGNAL(clicked()), this, SLOT(delClause()));

    new QShortcut(QKeySequence(Qt::Key_Up), this, SLOT(slotHistoryNext()));;
    new QShortcut(QKeySequence(Qt::Key_Down), this, SLOT(slotHistoryPrev()));

    conjunctCMB->insertItem(1, tr("All clauses"));
    conjunctCMB->insertItem(2, tr("Any clause"));

    // Create preconfigured clauses
    for (unsigned int i = 0; i < iclausescnt; i++) {
	addClause(initclausetypes[i], false);
    }
    // Tune initial state according to last saved
    {
	vector<SearchClauseW *>::iterator cit = m_clauseWins.begin();
        unsigned int existing = m_clauseWins.size();
	for (unsigned int i = 0; i < prefs.advSearchClauses.size(); i++) {
	    if (i < existing) {
		(*cit)->tpChange(prefs.advSearchClauses[i]);
		cit++;
	    } else {
		addClause(prefs.advSearchClauses[i], false);
	    }
	}
    }
    (*m_clauseWins.begin())->wordsLE->setFocus();

    // Initialize min/max mtime from extrem values in the index
    int minyear, maxyear;
    if (rcldb) {
	rcldb->maxYearSpan(&minyear, &maxyear);
	minDateDTE->setDisplayFormat("yyyy-MM-dd");
	maxDateDTE->setDisplayFormat("yyyy-MM-dd");
	minDateDTE->setDate(QDate(minyear, 1, 1));
	maxDateDTE->setDate(QDate(maxyear, 12, 31));
    }

    // Initialize lists of accepted and ignored mime types from config
    // and settings
    m_ignTypes = prefs.asearchIgnFilTyps;
    m_ignByCats = prefs.fileTypesByCats;
    restrictCtCB->setEnabled(false);
    restrictCtCB->setChecked(m_ignByCats);
    fillFileTypes();

    subtreeCMB->insertItems(0, prefs.asearchSubdirHist);
    subtreeCMB->setEditText("");

    // The clauseline frame is needed to force designer to accept a
    // vbox to englobe the base clauses grid and 'something else' (the
    // vbox is so that we can then insert SearchClauseWs), but we
    // don't want to see it.
    clauseline->close();

    bool calpop = 0;
    minDateDTE->setCalendarPopup(calpop);
    maxDateDTE->setCalendarPopup(calpop);

    // Translations for known categories
    cat_translations[QString::fromUtf8("texts")] = tr("text");
    cat_rtranslations[tr("texts")] = QString::fromUtf8("text"); 

    cat_translations[QString::fromUtf8("spreadsheet")] = tr("spreadsheet");
    cat_rtranslations[tr("spreadsheets")] = QString::fromUtf8("spreadsheet");

    cat_translations[QString::fromUtf8("presentation")] = tr("presentation");
    cat_rtranslations[tr("presentation")] =QString::fromUtf8("presentation");

    cat_translations[QString::fromUtf8("media")] = tr("media");
    cat_rtranslations[tr("media")] = QString::fromUtf8("media"); 

    cat_translations[QString::fromUtf8("message")] = tr("message");
    cat_rtranslations[tr("message")] = QString::fromUtf8("message"); 

    cat_translations[QString::fromUtf8("other")] = tr("other");
    cat_rtranslations[tr("other")] = QString::fromUtf8("other"); 
}

void AdvSearch::saveCnf()
{
    // Save my state
    prefs.advSearchClauses.clear(); 
    for (const auto& clause : m_clauseWins) {
	prefs.advSearchClauses.push_back(clause->sTpCMB->currentIndex());
    }
}

void AdvSearch::addClause(bool updsaved)
{
    addClause(0, updsaved);
}

void AdvSearch::addClause(int tp, bool updsaved)
{
    SearchClauseW *w = new SearchClauseW(clauseFRM);
    m_clauseWins.push_back(w);
    ((QVBoxLayout *)(clauseFRM->layout()))->addWidget(w);
    w->show();
    w->tpChange(tp);
    if (m_clauseWins.size() > iclausescnt) {
	delClausePB->setEnabled(true);
    } else {
	delClausePB->setEnabled(false);
    }
    if (updsaved) {
        saveCnf();
    }
}

void AdvSearch::delClause(bool updsaved)
{
    if (m_clauseWins.size() <= iclausescnt)
	return;
    delete m_clauseWins.back();
    m_clauseWins.pop_back();
    if (m_clauseWins.size() > iclausescnt) {
	delClausePB->setEnabled(true);
    } else {
	delClausePB->setEnabled(false);
    }
    if (updsaved) {
        saveCnf();
    }
}

void AdvSearch::delAFiltypPB_clicked()
{
    yesFiltypsLB->selectAll();
    delFiltypPB_clicked();
}

// Move selected file types from the searched to the ignored box
void AdvSearch::delFiltypPB_clicked()
{
    QList<QListWidgetItem *> items = yesFiltypsLB->selectedItems();
    for (QList<QListWidgetItem *>::iterator it = items.begin(); 
	 it != items.end(); it++) {
	int row = yesFiltypsLB->row(*it);
	QListWidgetItem *item = yesFiltypsLB->takeItem(row);
	noFiltypsLB->insertItem(0, item);
    }
    guiListsToIgnTypes();
}

// Move selected file types from the ignored to the searched box
void AdvSearch::addFiltypPB_clicked()
{
    QList<QListWidgetItem *> items = noFiltypsLB->selectedItems();
    for (QList<QListWidgetItem *>::iterator it = items.begin(); 
	 it != items.end(); it++) {
	int row = noFiltypsLB->row(*it);
	QListWidgetItem *item = noFiltypsLB->takeItem(row);
	yesFiltypsLB->insertItem(0, item);
    }
    guiListsToIgnTypes();
 }

// Compute list of ignored mime type from widget lists
void AdvSearch::guiListsToIgnTypes()
{
    yesFiltypsLB->sortItems();
    noFiltypsLB->sortItems();
    m_ignTypes.clear();
    for (int i = 0; i < noFiltypsLB->count();i++) {
	QListWidgetItem *item = noFiltypsLB->item(i);
	m_ignTypes.append(item->text());
    }
}
void AdvSearch::addAFiltypPB_clicked()
{
    noFiltypsLB->selectAll();
    addFiltypPB_clicked();
}

// Activate file type selection
void AdvSearch::restrictFtCB_toggled(bool on)
{
    restrictCtCB->setEnabled(on);
    yesFiltypsLB->setEnabled(on);
    delFiltypPB->setEnabled(on);
    addFiltypPB->setEnabled(on);
    delAFiltypPB->setEnabled(on);
    addAFiltypPB->setEnabled(on);
    noFiltypsLB->setEnabled(on);
    saveFileTypesPB->setEnabled(on);
}

// Activate file type selection
void AdvSearch::filterSizesCB_toggled(bool on)
{
    minSizeLE->setEnabled(on);
    maxSizeLE->setEnabled(on);
}
// Activate file type selection
void AdvSearch::filterDatesCB_toggled(bool on)
{
    minDateDTE->setEnabled(on);
    maxDateDTE->setEnabled(on);
}

void AdvSearch::restrictCtCB_toggled(bool on)
{
    m_ignByCats = on;
    // Only reset the list if we're enabled. Else this is init from prefs
    if (restrictCtCB->isEnabled())
	m_ignTypes.clear();
    fillFileTypes();
}

void AdvSearch::fillFileTypes()
{
    noFiltypsLB->clear();
    yesFiltypsLB->clear();
    noFiltypsLB->insertItems(0, m_ignTypes); 

    QStringList ql;
    if (m_ignByCats == false) {
	vector<string> types = theconfig->getAllMimeTypes();
	rcldb->getAllDbMimeTypes(types);
	sort(types.begin(), types.end());
	types.erase(unique(types.begin(), types.end()), types.end());
	for (vector<string>::iterator it = types.begin(); 
	     it != types.end(); it++) {
	    QString qs = QString::fromUtf8(it->c_str());
	    if (m_ignTypes.indexOf(qs) < 0)
		ql.append(qs);
	}
    } else {
	vector<string> cats;
	theconfig->getMimeCategories(cats);
	for (vector<string>::const_iterator it = cats.begin();
	     it != cats.end(); it++) {
	    map<QString, QString>::const_iterator it1;
	    QString cat;
	    if ((it1 = cat_translations.find(QString::fromUtf8(it->c_str())))
		!= cat_translations.end()) {
		cat = it1->second;
	    } else {
		cat = QString::fromUtf8(it->c_str());
	    } 
	    if (m_ignTypes.indexOf(cat) < 0)
		ql.append(cat);
	}
    }
    yesFiltypsLB->insertItems(0, ql);
}

// Save current set of ignored file types to prefs
void AdvSearch::saveFileTypes()
{
    prefs.asearchIgnFilTyps = m_ignTypes;
    prefs.fileTypesByCats = m_ignByCats;
    rwSettings(true);
}

void AdvSearch::browsePB_clicked()
{
    QString dir = myGetFileName(true);
#ifdef _WIN32
    string s = qs2utf8s(dir);
    for (string::size_type i = 0; i < s.size(); i++) {
        if (s[i] == '\\') {
            s[i] = '/';
        }
    }
    if (s.size() >= 2 && isalpha(s[0]) && s[1] == ':') {
        s.erase(1,1);
        s = string("/") + s;
    }
    dir = u8s2qs(s);
#endif
    subtreeCMB->setEditText(dir);
}

size_t AdvSearch::stringToSize(QString qsize)
{
    size_t size = size_t(-1);
    qsize.replace(QRegExp("[\\s]+"), "");
    if (!qsize.isEmpty()) {
	string csize(qs2utf8s(qsize));
	char *cp;
	size = strtoll(csize.c_str(), &cp, 10);
	if (*cp != 0) {
	    switch (*cp) {
	    case 'k': case 'K': size *= 1E3;break;
	    case 'm': case 'M': size *= 1E6;break;
	    case 'g': case 'G': size *= 1E9;break;
	    case 't': case 'T': size *= 1E12;break;
	    default: 
		QMessageBox::warning(0, "Recoll", 
			     tr("Bad multiplier suffix in size filter"));
		size = size_t(-1);
	    }
	}
    }
    return size;
}

using namespace Rcl;
void AdvSearch::runSearch()
{
    string stemLang = prefs.stemlang();
    std::shared_ptr<SearchData> sdata(new SearchData(conjunctCMB->currentIndex() == 0 ?
					     SCLT_AND : SCLT_OR, stemLang));
    bool hasclause = false;

    for (vector<SearchClauseW*>::iterator it = m_clauseWins.begin();
	 it != m_clauseWins.end(); it++) {
	SearchDataClause *cl;
	if ((cl = (*it)->getClause())) {
	    sdata->addClause(cl);
            hasclause = true;
	}
    }
    if (!hasclause)
        return;

    if (restrictFtCB->isChecked() && noFiltypsLB->count() > 0) {
	for (int i = 0; i < yesFiltypsLB->count(); i++) {
	    if (restrictCtCB->isChecked()) {
		QString qcat = yesFiltypsLB->item(i)->text();
		map<QString,QString>::const_iterator qit;
		string cat;
		if ((qit = cat_rtranslations.find(qcat)) != 
		    cat_rtranslations.end()) {
		    cat = qs2utf8s(qit->second);
		} else {
		    cat = qs2utf8s(qcat);
		}
		vector<string> types;
		theconfig->getMimeCatTypes(cat, types);
		for (vector<string>::const_iterator it = types.begin();
		     it != types.end(); it++) {
		    sdata->addFiletype(*it);
		}
	    } else {
		sdata->addFiletype(qs2utf8s(yesFiltypsLB->item(i)->text()));
	    }
	}
    }

    if (filterDatesCB->isChecked()) {
	QDate mindate = minDateDTE->date();
	QDate maxdate = maxDateDTE->date();
	DateInterval di;
	di.y1 = mindate.year();
	di.m1 = mindate.month();
	di.d1 = mindate.day();
	di.y2 = maxdate.year();
	di.m2 = maxdate.month();
	di.d2 = maxdate.day();
	sdata->setDateSpan(&di);
    }
    if (filterSizesCB->isChecked()) {
	size_t size = stringToSize(minSizeLE->text());
	sdata->setMinSize(size);
	size = stringToSize(maxSizeLE->text());
	sdata->setMaxSize(size);
    }

    if (!subtreeCMB->currentText().isEmpty()) {
	QString current = subtreeCMB->currentText();

        Rcl::SearchDataClausePath *pathclause = 
            new Rcl::SearchDataClausePath((const char*)current.toLocal8Bit(), 
                                          direxclCB->isChecked());
        if (sdata->getTp() == SCLT_AND) {
            sdata->addClause(pathclause);
        } else {
            std::shared_ptr<SearchData> 
                nsdata(new SearchData(SCLT_AND, stemLang));
            nsdata->addClause(new Rcl::SearchDataClauseSub(sdata));
            nsdata->addClause(pathclause);
            sdata = nsdata;
        }

	// Keep history clean and sorted. Maybe there would be a
	// simpler way to do this
	list<QString> entries;
	for (int i = 0; i < subtreeCMB->count(); i++) {
	    entries.push_back(subtreeCMB->itemText(i));
	}
	entries.push_back(subtreeCMB->currentText());
	entries.sort();
	entries.unique();
	LOGDEB("Subtree list now has "  << (entries.size()) << " entries\n" );
	subtreeCMB->clear();
	for (list<QString>::iterator it = entries.begin(); 
	     it != entries.end(); it++) {
	    subtreeCMB->addItem(*it);
	}
	subtreeCMB->setCurrentIndex(subtreeCMB->findText(current));
	prefs.asearchSubdirHist.clear();
	for (int index = 0; index < subtreeCMB->count(); index++)
	    prefs.asearchSubdirHist.push_back(subtreeCMB->itemText(index));
    }
    saveCnf();
    g_advshistory && g_advshistory->push(sdata);
    emit setDescription("");
    emit startSearch(sdata, false);
}


// Set up fields from existing search data, which must be compatible
// with what we can do...
void AdvSearch::fromSearch(std::shared_ptr<SearchData> sdata)
{
    if (sdata->m_tp == SCLT_OR)
	conjunctCMB->setCurrentIndex(1);
    else
	conjunctCMB->setCurrentIndex(0);

    while (sdata->m_query.size() > m_clauseWins.size()) {
	addClause();
    }

    subtreeCMB->setEditText("");
    direxclCB->setChecked(0);

    for (unsigned int i = 0; i < sdata->m_query.size(); i++) {
	// Set fields from clause
	if (sdata->m_query[i]->getTp() == SCLT_SUB) {
	    LOGERR("AdvSearch::fromSearch: SUB clause found !\n" );
	    continue;
	}
	if (sdata->m_query[i]->getTp() == SCLT_PATH) {
	    SearchDataClausePath *cs = 
		dynamic_cast<SearchDataClausePath*>(sdata->m_query[i]);
	    // We can only use one such clause. There should be only one too 
	    // if this is sfrom aved search data.
	    QString qdir = QString::fromLocal8Bit(cs->gettext().c_str());
	    subtreeCMB->setEditText(qdir);
	    direxclCB->setChecked(cs->getexclude());
	    continue;
	}
	SearchDataClauseSimple *cs = 
	    dynamic_cast<SearchDataClauseSimple*>(sdata->m_query[i]);
	m_clauseWins[i]->setFromClause(cs);
    }
    for (unsigned int i = sdata->m_query.size(); i < m_clauseWins.size(); i++) {
	m_clauseWins[i]->clear();
    }

    restrictCtCB->setChecked(0);
    if (!sdata->m_filetypes.empty()) {
	restrictFtCB_toggled(1);
	delAFiltypPB_clicked();
	for (unsigned int i = 0; i < sdata->m_filetypes.size(); i++) {
	    QString ft = QString::fromUtf8(sdata->m_filetypes[i].c_str());
	    QList<QListWidgetItem *> lst = 
		noFiltypsLB->findItems(ft, Qt::MatchExactly);
	    if (!lst.isEmpty()) {
		int row = noFiltypsLB->row(lst[0]);
		QListWidgetItem *item = noFiltypsLB->takeItem(row);
		yesFiltypsLB->insertItem(0, item);
	    }
	}
	yesFiltypsLB->sortItems();
    } else {
	addAFiltypPB_clicked();
	restrictFtCB_toggled(0);
    }

    if (sdata->m_haveDates) {
	filterDatesCB->setChecked(1);
	DateInterval &di(sdata->m_dates);
	QDate mindate(di.y1, di.m1, di.d1);
	QDate maxdate(di.y2, di.m2, di.d2);
        minDateDTE->setDate(mindate);
        maxDateDTE->setDate(maxdate);
    } else {
	filterDatesCB->setChecked(0);
	QDate date;
	minDateDTE->setDate(date);
	maxDateDTE->setDate(date);
    }

    if (sdata->m_maxSize != (size_t)-1 || sdata->m_minSize != (size_t)-1) {
	filterSizesCB->setChecked(1);
	QString sz;
	if (sdata->m_minSize != (size_t)-1) {
	    sz.setNum(sdata->m_minSize);
	    minSizeLE->setText(sz);
	} else {
	    minSizeLE->setText("");
	}
	if (sdata->m_maxSize != (size_t)-1) {
	    sz.setNum(sdata->m_maxSize);
	    maxSizeLE->setText(sz);
	} else {
	    maxSizeLE->setText("");
	}
    } else {
	filterSizesCB->setChecked(0);
	minSizeLE->setText("");
	maxSizeLE->setText("");
    }
}

void AdvSearch::slotHistoryNext()
{
    if (g_advshistory == 0)
	return;
    std::shared_ptr<Rcl::SearchData> sd = g_advshistory->getnewer();
    if (!sd)
	return;
    fromSearch(sd);
}

void AdvSearch::slotHistoryPrev()
{
    if (g_advshistory == 0)
	return;
    std::shared_ptr<Rcl::SearchData> sd = g_advshistory->getolder();
    if (!sd)
	return;
    fromSearch(sd);
}


