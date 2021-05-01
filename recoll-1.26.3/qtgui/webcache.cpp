/* Copyright (C) 2016 J.F.Dockes
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
#include <iostream>
#include <memory>
#include <unordered_map>

#include <QDebug>
#include <QSettings>
#include <QCloseEvent>
#include <QShortcut>
#include <QMenu>
#include <QClipboard>
#include <QTimer>
#include <QMessageBox>

#include "recoll.h"
#include "webcache.h"
#include "webstore.h"
#include "circache.h"
#include "conftree.h"
#include "rclmain_w.h"
#include "smallut.h"

using namespace std;

class CEnt {
public:
    CEnt(const string& ud, const string& ur, const string& mt)
        : udi(ud), url(ur), mimetype(mt) {
    }
    string udi;
    string url;
    string mimetype;
};

class WebcacheModelInternal {
public:
    std::shared_ptr<WebStore> cache;
    vector<CEnt> all;
    vector<CEnt> disp;
};

WebcacheModel::WebcacheModel(QObject *parent)
    : QAbstractTableModel(parent), m(new WebcacheModelInternal())
{
    //qDebug() << "WebcacheModel::WebcacheModel()";
    reload();
}
WebcacheModel::~WebcacheModel()
{
    delete m;
}

void WebcacheModel::reload()
{
    m->cache =
        std::shared_ptr<WebStore>(new WebStore(theconfig));
    m->all.clear();
    m->disp.clear();
    
    if (m->cache) {
        bool eof;
        m->cache->cc()->rewind(eof);
        while (!eof) {
            string udi, sdic;
            m->cache->cc()->getCurrent(udi, sdic);
            ConfSimple dic(sdic);
            string mime, url;
            dic.get("mimetype", mime);
            dic.get("url", url);
            if (!udi.empty()) {
                m->all.push_back(CEnt(udi, url, mime));
                m->disp.push_back(CEnt(udi, url, mime));
            }
            if (!m->cache->cc()->next(eof))
                break;
        }
    }
    emit dataChanged(createIndex(0,0), createIndex(1, m->all.size()));
}

bool WebcacheModel::deleteIdx(unsigned int idx)
{
    if (idx > m->disp.size() || !m->cache)
        return false;
    return m->cache->cc()->erase(m->disp[idx].udi, true);
}

string WebcacheModel::getURL(unsigned int idx)
{
    if (idx > m->disp.size() || !m->cache)
        return string();
    return m->disp[idx].url;
}

int WebcacheModel::rowCount(const QModelIndex&) const
{
    //qDebug() << "WebcacheModel::rowCount(): " << m->disp.size();
    return int(m->disp.size());
}

int WebcacheModel::columnCount(const QModelIndex&) const
{
    //qDebug() << "WebcacheModel::columnCount()";
    return 2;
}

QVariant WebcacheModel::headerData (int col, Qt::Orientation orientation, 
                                    int role) const
{
    // qDebug() << "WebcacheModel::headerData()";
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QVariant();
    }
    switch (col) {
    case 0: return QVariant(tr("MIME"));
    case 1: return QVariant(tr("Url"));
    default: return QVariant();
    }
}

QVariant WebcacheModel::data(const QModelIndex& index, int role) const
{
    //qDebug() << "WebcacheModel::data()";
    Q_UNUSED(index);
    if (role != Qt::DisplayRole) {
        return QVariant();
    }
    int row = index.row();
    if (row < 0 || row >= int(m->disp.size())) {
        return QVariant();
    }

    const string& mime = m->disp[row].mimetype;
    const string& url = m->disp[row].url;
    
    switch (index.column()) {
    case 0: return QVariant(QString::fromUtf8(mime.c_str()));
    case 1: return QVariant(QString::fromUtf8(url.c_str()));
    default: return QVariant();
    }
}

void WebcacheModel::setSearchFilter(const QString& _txt)
{
    SimpleRegexp re(qs2utf8s(_txt), SimpleRegexp::SRE_NOSUB);
    
    m->disp.clear();
    for (unsigned int i = 0; i < m->all.size(); i++) {
        if (re(m->all[i].url)) {
            m->disp.push_back(m->all[i]);
        } else {
            //qDebug() << "match failed. exp" << _txt << "data" <<
            // m->all[i].url.c_str();
        }
    }
    emit dataChanged(createIndex(0,0), createIndex(1, m->all.size()));
}

static const int ROWHEIGHTPAD = 2;
static const char *cwnm = "/Recoll/prefs/webcachecolw";
static const char *wwnm = "/Recoll/prefs/webcachew";
static const char *whnm = "/Recoll/prefs/webcacheh";
static const QKeySequence closeKS(Qt::ControlModifier+Qt::Key_W);

WebcacheEdit::WebcacheEdit(RclMain *parent)
    : QDialog(parent), m_recoll(parent), m_modified(false)
{
    //qDebug() << "WebcacheEdit::WebcacheEdit()";
    setupUi(this);
    m_model = new WebcacheModel(this);
    tableview->setModel(m_model);
    tableview->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableview->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tableview->setContextMenuPolicy(Qt::CustomContextMenu);
    tableview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    QSettings settings;
    QStringList wl;
    wl = settings.value(cwnm).toStringList();
    QHeaderView *header = tableview->horizontalHeader();
    if (header) {
	if (int(wl.size()) == header->count()) {
	    for (int i = 0; i < header->count(); i++) {
		header->resizeSection(i, wl[i].toInt());
	    }
	}
    }
    connect(header, SIGNAL(sectionResized(int,int,int)),
            this, SLOT(saveColState()));

    header = tableview->verticalHeader();
    if (header) {
	header->setDefaultSectionSize(QApplication::fontMetrics().height() + 
				      ROWHEIGHTPAD);
    }

    int width = settings.value(wwnm, 0).toInt();
    int height = settings.value(whnm, 0).toInt();
    if (width && height) {
        resize(QSize(width, height));
    }

    connect(searchLE, SIGNAL(textEdited(const QString&)),
            m_model, SLOT(setSearchFilter(const QString&)));
    connect(new QShortcut(closeKS, this), SIGNAL (activated()), 
            this, SLOT (close()));
    connect(tableview, SIGNAL(customContextMenuRequested(const QPoint&)),
	    this, SLOT(createPopupMenu(const QPoint&)));

}

void WebcacheEdit::createPopupMenu(const QPoint& pos)
{
    int selsz = tableview->selectionModel()->selectedRows().size();
    if (selsz <= 0) {
        return;
    }
    QMenu *popup = new QMenu(this);
    if (selsz == 1) {
        popup->addAction(tr("Copy URL"), this, SLOT(copyURL()));
    }
    if (m_recoll) {
        RclMain::IndexerState ixstate = m_recoll->indexerState();
        switch (ixstate) {
        case RclMain::IXST_UNKNOWN:
            QMessageBox::warning(0, "Recoll",
                                 tr("Unknown indexer state. "
                                    "Can't edit webcache file."));
            break;
        case RclMain::IXST_RUNNINGMINE:
        case RclMain::IXST_RUNNINGNOTMINE:
            QMessageBox::warning(0, "Recoll",
                                 tr("Indexer is running. "
                                    "Can't edit webcache file."));
            break;
        case RclMain::IXST_NOTRUNNING:
            popup->addAction(tr("Delete selection"),
                             this, SLOT(deleteSelected()));
            break;
        }
    }
        
    popup->popup(tableview->mapToGlobal(pos));
}

void WebcacheEdit::deleteSelected()
{
    QModelIndexList selection = tableview->selectionModel()->selectedRows();
    for (int i = 0; i < selection.size(); i++) {
        if (m_model->deleteIdx(selection[i].row())) {
            m_modified = true;
        }
    }
    m_model->reload();
    m_model->setSearchFilter(searchLE->text());
    tableview->clearSelection();
}

void WebcacheEdit::copyURL()
{
    QModelIndexList selection = tableview->selectionModel()->selectedRows();
    if (selection.size() != 1)
        return;
    string url = m_model->getURL(selection[0].row());
    if (!url.empty()) {
        url =  url_encode(url, 7);
        QApplication::clipboard()->setText(url.c_str(), 
                                           QClipboard::Selection);
        QApplication::clipboard()->setText(url.c_str(), 
                                           QClipboard::Clipboard);
    }
}

void WebcacheEdit::saveColState()
{
    //qDebug() << "void WebcacheEdit::saveColState()";
    QHeaderView *header = tableview->horizontalHeader();
    QStringList newwidths;
    for (int vi = 0; vi < header->count(); vi++) {
	int li = header->logicalIndex(vi);
	newwidths.push_back(lltodecstr(header->sectionSize(li)).c_str());
    }
    
    QSettings settings;
    settings.setValue(cwnm, newwidths);
}

void WebcacheEdit::closeEvent(QCloseEvent *event)
{
    if (m_modified) {
        QMessageBox::information(0, "Recoll",
                                 tr("Webcache was modified, you will need "
                                    "to run the indexer after closing this "
                                    "window."));
    }
    if (!isFullScreen()) {
        QSettings settings;
        settings.setValue(wwnm, width());
        settings.setValue(whnm, height());
    }
    event->accept();
}
