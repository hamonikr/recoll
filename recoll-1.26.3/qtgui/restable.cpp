/*
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

#include "restable.h"

#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#include <algorithm>
#include <memory>

#include <Qt>
#include <QShortcut>
#include <QAbstractTableModel>
#include <QSettings>
#include <QMenu>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QPainter>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QKeyEvent>

#include "recoll.h"
#include "docseq.h"
#include "log.h"
#include "guiutils.h"
#include "reslistpager.h"
#include "reslist.h"
#include "rclconfig.h"
#include "plaintorich.h"
#include "indexer.h"
#include "respopup.h"
#include "rclmain_w.h"
#include "multisave.h"
#include "appformime.h"
#include "transcode.h"

static const QKeySequence quitKeySeq("Ctrl+q");
static const QKeySequence closeKeySeq("Ctrl+w");

// Compensate for the default and somewhat bizarre vertical placement
// of text in cells
static const int ROWHEIGHTPAD = 2;
static const int TEXTINCELLVTRANS = -4;

static PlainToRichQtReslist g_hiliter;

//////////////////////////////////////////////////////////////////////////
// Restable "pager". We use it to print details for a document in the 
// detail area
///
class ResTablePager : public ResListPager {
public:
    ResTablePager(ResTable *p)
        : ResListPager(1), m_parent(p) 
        {}
    virtual bool append(const string& data, int idx, const Rcl::Doc& doc);
    virtual string trans(const string& in);
    virtual const string &parFormat();
    virtual string absSep() {return (const char *)(prefs.abssep.toUtf8());}
    virtual string iconUrl(RclConfig *, Rcl::Doc& doc);
private:
    ResTable *m_parent;
};

bool ResTablePager::append(const string& data, int, const Rcl::Doc&)
{
    m_parent->m_detail->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
    m_parent->m_detail->textCursor().insertBlock();
    m_parent->m_detail->insertHtml(u8s2qs(data));

//    LOGDEB("RESTABLEPAGER::APPEND: data : " << data << std::endl);
//    m_parent->m_detail->setHtml(u8s2qs(data));
    return true;
}

string ResTablePager::trans(const string& in)
{
    return string((const char*)ResList::tr(in.c_str()).toUtf8());
}

const string& ResTablePager::parFormat()
{
    return prefs.creslistformat;
}

string ResTablePager::iconUrl(RclConfig *config, Rcl::Doc& doc)
{
    if (doc.ipath.empty()) {
        vector<Rcl::Doc> docs;
        docs.push_back(doc);
        vector<string> paths;
        Rcl::docsToPaths(docs, paths);
        if (!paths.empty()) {
            string path;
            if (thumbPathForUrl(cstr_fileu + paths[0], 128, path)) {
                return cstr_fileu + path;
            }
        }
    }
    return ResListPager::iconUrl(config, doc);
}

/////////////////////////////////////////////////////////////////////////////
/// Detail text area methods

ResTableDetailArea::ResTableDetailArea(ResTable* parent)
    : QTextBrowser(parent), m_table(parent)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(createPopupMenu(const QPoint&)));
}

void ResTableDetailArea::createPopupMenu(const QPoint& pos)
{
    if (m_table && m_table->m_model && m_table->m_detaildocnum >= 0) {
        int opts = m_table->m_ismainres ? ResultPopup::showExpand : 0;
        opts |= ResultPopup::showSaveOne;
        QMenu *popup = ResultPopup::create(m_table, opts, 
                                           m_table->m_model->getDocSource(),
                                           m_table->m_detaildoc);
        popup->popup(mapToGlobal(pos));
    }
}

//////////////////////////////////////////////////////////////////////////////
//// Data model methods
////

// Routines used to extract named data from an Rcl::Doc. The basic one
// just uses the meta map. Others (ie: the date ones) need to do a
// little processing
static string gengetter(const string& fld, const Rcl::Doc& doc)
{
    const auto it = doc.meta.find(fld);
    if (it == doc.meta.end()) {
        return string();
    }
    return it->second;
}

static string sizegetter(const string& fld, const Rcl::Doc& doc)
{
    const auto it = doc.meta.find(fld);
    if (it == doc.meta.end()) {
        return string();
    }
    int64_t size = atoll(it->second.c_str());
    return displayableBytes(size) + " (" + it->second + ")";
}

static string dategetter(const string&, const Rcl::Doc& doc)
{
    string sdate;
    if (!doc.dmtime.empty() || !doc.fmtime.empty()) {
        char datebuf[100];
        datebuf[0] = 0;
        time_t mtime = doc.dmtime.empty() ?
            atoll(doc.fmtime.c_str()) : atoll(doc.dmtime.c_str());
        struct tm *tm = localtime(&mtime);
        strftime(datebuf, 99, "%Y-%m-%d", tm);
        transcode(datebuf, sdate, RclConfig::getLocaleCharset(), "UTF-8");
    }
    return sdate;
}

static string datetimegetter(const string&, const Rcl::Doc& doc)
{
    char datebuf[100];
    datebuf[0] = 0;
    if (!doc.dmtime.empty() || !doc.fmtime.empty()) {
        time_t mtime = doc.dmtime.empty() ?
            atoll(doc.fmtime.c_str()) : atoll(doc.dmtime.c_str());
        struct tm *tm = localtime(&mtime);
        strftime(datebuf, 99, prefs.creslistdateformat.c_str(), tm);
    }
    return datebuf;
}

// Static map to translate from internal column names to displayable ones
map<string, QString> RecollModel::o_displayableFields;

FieldGetter *RecollModel::chooseGetter(const string& field)
{
    if (!stringlowercmp("date", field))
        return dategetter;
    else if (!stringlowercmp("datetime", field))
        return datetimegetter;
    else if (!stringlowercmp("bytes", field.substr(1)))
        return sizegetter;
    else
        return gengetter;
}

string RecollModel::baseField(const string& field)
{
    if (!stringlowercmp("date", field) || !stringlowercmp("datetime", field))
        return "mtime";
    else
        return field;
}

RecollModel::RecollModel(const QStringList fields, ResTable *tb,
                         QObject *parent)
    : QAbstractTableModel(parent), m_table(tb), m_ignoreSort(false)
{
    // Initialize the translated map for column headers
    o_displayableFields["abstract"] = tr("Abstract");
    o_displayableFields["author"] = tr("Author");
    o_displayableFields["dbytes"] = tr("Document size");
    o_displayableFields["dmtime"] = tr("Document date");
    o_displayableFields["fbytes"] = tr("File size");
    o_displayableFields["filename"] = tr("File name");
    o_displayableFields["fmtime"] = tr("File date");
    o_displayableFields["ipath"] = tr("Ipath");
    o_displayableFields["keywords"] = tr("Keywords");
    o_displayableFields["mtype"] = tr("MIME type");
    o_displayableFields["origcharset"] = tr("Original character set");
    o_displayableFields["relevancyrating"] = tr("Relevancy rating");
    o_displayableFields["title"] = tr("Title");
    o_displayableFields["url"] = tr("URL");
    o_displayableFields["mtime"] = tr("Mtime");
    o_displayableFields["date"] = tr("Date");
    o_displayableFields["datetime"] = tr("Date and time");

    // Add dynamic "stored" fields to the full column list. This
    // could be protected to be done only once, but it's no real
    // problem
    if (theconfig) {
        const set<string>& stored = theconfig->getStoredFields();
        for (set<string>::const_iterator it = stored.begin(); 
             it != stored.end(); it++) {
            if (o_displayableFields.find(*it) == o_displayableFields.end()) {
                o_displayableFields[*it] = QString::fromUtf8(it->c_str());
            }
        }
    }

    // Construct the actual list of column names
    for (QStringList::const_iterator it = fields.begin(); 
         it != fields.end(); it++) {
        m_fields.push_back((const char *)(it->toUtf8()));
        m_getters.push_back(chooseGetter(m_fields.back()));
    }

    g_hiliter.set_inputhtml(false);
}

int RecollModel::rowCount(const QModelIndex&) const
{
    LOGDEB2("RecollModel::rowCount\n");
    if (!m_source)
        return 0;
    return m_source->getResCnt();
}

int RecollModel::columnCount(const QModelIndex&) const
{
    LOGDEB2("RecollModel::columnCount\n");
    return m_fields.size();
}

void RecollModel::readDocSource()
{
    LOGDEB("RecollModel::readDocSource()\n");
    beginResetModel();
    endResetModel();
}

void RecollModel::setDocSource(std::shared_ptr<DocSequence> nsource)
{
    LOGDEB("RecollModel::setDocSource\n");
    if (!nsource) {
        m_source = std::shared_ptr<DocSequence>();
    } else {
        // We used to allocate a new DocSource here instead of sharing
        // the input, but I can't see why.
        m_source = nsource;
        m_hdata.clear();
    }
}

void RecollModel::deleteColumn(int col)
{
    if (col > 0 && col < int(m_fields.size())) {
        vector<string>::iterator it = m_fields.begin();
        it += col;
        m_fields.erase(it);
        vector<FieldGetter*>::iterator it1 = m_getters.begin();
        it1 += col;
        m_getters.erase(it1);
        readDocSource();
    }
}

void RecollModel::addColumn(int col, const string& field)
{
    LOGDEB("AddColumn: col " << col << " fld ["  << field << "]\n");
    if (col >= 0 && col < int(m_fields.size())) {
        col++;
        vector<string>::iterator it = m_fields.begin();
        vector<FieldGetter*>::iterator it1 = m_getters.begin();
        if (col) {
            it += col;
            it1 += col;
        }
        m_fields.insert(it, field);
        m_getters.insert(it1, chooseGetter(field));
        readDocSource();
    }
}

QVariant RecollModel::headerData(int idx, Qt::Orientation orientation, 
                                 int role) const
{
    LOGDEB2("RecollModel::headerData: idx " << idx << " orientation " <<
            (orientation == Qt::Vertical ? "vertical":"horizontal") <<
            " role " << role << "\n");
    if (orientation == Qt::Vertical && role == Qt::DisplayRole) {
        return idx;
    }
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole &&
        idx < int(m_fields.size())) {
        map<string, QString>::const_iterator it = 
            o_displayableFields.find(m_fields[idx]);
        if (it == o_displayableFields.end())
            return QString::fromUtf8(m_fields[idx].c_str());
        else 
            return it->second;
    }
    return QVariant();
}

QVariant RecollModel::data(const QModelIndex& index, int role) const
{
    LOGDEB2("RecollModel::data: row " << index.row() << " col " <<
            index.column() << " role " << role << "\n");
    if (!m_source || role != Qt::DisplayRole || !index.isValid() ||
        index.column() >= int(m_fields.size())) {
        return QVariant();
    }

    Rcl::Doc doc;
    if (!m_source->getDoc(index.row(), doc)) {
        return QVariant();
    }

    string colname = m_fields[index.column()];

    string data = m_getters[index.column()](colname, doc);

#ifndef _WIN32
    // Special case url, because it may not be utf-8. URL-encode in this case.
    // Not on windows, where we always read the paths as Unicode.
    if (!colname.compare("url")) {
        int ecnt;
        string data1;
        if (!transcode(data, data1, "UTF-8", "UTF-8", &ecnt) || ecnt > 0) {
            data = url_encode(data);
        }
    }
#endif

    list<string> lr;
    g_hiliter.plaintorich(data, lr, m_hdata);
    return QString::fromUtf8(lr.front().c_str());
}

void RecollModel::saveAsCSV(FILE *fp)
{
    if (!m_source)
        return;

    int cols = columnCount();
    int rows = rowCount();
    vector<string> tokens;

    for (int col = 0; col < cols; col++) {
        QString qs = headerData(col, Qt::Horizontal,Qt::DisplayRole).toString();
        tokens.push_back((const char *)qs.toUtf8());
    }
    string csv;
    stringsToCSV(tokens, csv);
    fprintf(fp, "%s\n", csv.c_str());
    tokens.clear();

    for (int row = 0; row < rows; row++) {
        Rcl::Doc doc;
        if (!m_source->getDoc(row, doc)) {
            continue;
        }
        for (int col = 0; col < cols; col++) {
            tokens.push_back(m_getters[col](m_fields[col], doc));
        }
        stringsToCSV(tokens, csv);
        fprintf(fp, "%s\n", csv.c_str());
        tokens.clear();
    }
}

// This gets called when the column headers are clicked
void RecollModel::sort(int column, Qt::SortOrder order)
{
    if (m_ignoreSort) {
        return;
    }
    LOGDEB("RecollModel::sort(" << column << ", " << order << ")\n");
    
    DocSeqSortSpec spec;
    if (column >= 0 && column < int(m_fields.size())) {
        spec.field = m_fields[column];
        if (!stringlowercmp("relevancyrating", spec.field) &&
            order != Qt::AscendingOrder) {
            QMessageBox::warning(0, "Recoll", 
                                 tr("Can't sort by inverse relevance"));
            QTimer::singleShot(0, m_table, SLOT(resetSort()));
            return;
        }
        if (!stringlowercmp("date", spec.field) || 
            !stringlowercmp("datetime", spec.field))
            spec.field = "mtime";
        spec.desc = order == Qt::AscendingOrder ? false : true;
    }
    emit sortDataChanged(spec);
}

/////////////////////////// 
// ResTable panel methods

// We use a custom delegate to display the cells because the base
// tableview's can't handle rich text to highlight the match terms
class ResTableDelegate: public QStyledItemDelegate {
public:
    ResTableDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

    // We might want to optimize by passing the data to the base
    // method if the text does not contain any term matches. Would
    // need a modif to plaintorich to return the match count (easy),
    // and a way to pass an indicator from data(), a bit more
    // difficult. Anyway, the display seems fast enough as is.
    void paint(QPainter *painter, const QStyleOptionViewItem &option, 
               const QModelIndex &index) const
        {
            QStyleOptionViewItem opt = option;
            initStyleOption(&opt, index);
            QVariant value = index.data(Qt::DisplayRole);
            if (value.isValid() && !value.isNull()) {
                QString text = value.toString();
                if (!text.isEmpty()) {
                    QTextDocument document;
                    painter->save();
                    if (opt.state & QStyle::State_Selected) {
                        painter->fillRect(opt.rect, opt.palette.highlight());
                        // Set the foreground color. The pen approach does
                        // not seem to work, probably it's reset by the
                        // textdocument. Couldn't use
                        // setdefaultstylesheet() either. the div thing is
                        // an ugly hack. Works for now
#if 0
                        QPen pen = painter->pen();
                        pen.setBrush(opt.palette.brush(QPalette::HighlightedText));
                        painter->setPen(pen);
#else
                        text = QString::fromUtf8("<div style='color: white'> ") + 
                            text + QString::fromUtf8("</div>");
#endif
                    } 
                    painter->setClipRect(option.rect);
                    QPoint where = option.rect.topLeft();
                    where.ry() += TEXTINCELLVTRANS;
                    painter->translate(where);
                    document.setHtml(text);
                    document.drawContents(painter);
                    painter->restore();
                    return;
                } 
            }
            QStyledItemDelegate::paint(painter, option, index);
        }
};

void ResTable::init()
{
    if (!(m_model = new RecollModel(prefs.restableFields, this)))
        return;
    tableView->setModel(m_model);
    tableView->setMouseTracking(true);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setItemDelegate(new ResTableDelegate(this));
    tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    new QShortcut(QKeySequence("Ctrl+o"), this, SLOT(menuEdit()));
    new QShortcut(QKeySequence("Ctrl+Shift+o"), this, SLOT(menuEditAndQuit()));
    new QShortcut(QKeySequence("Ctrl+d"), this, SLOT(menuPreview()));
    new QShortcut(QKeySequence("Ctrl+e"), this, SLOT(menuShowSnippets()));

    connect(tableView, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(createPopupMenu(const QPoint&)));

    QHeaderView *header = tableView->horizontalHeader();
    if (header) {
        if (int(prefs.restableColWidths.size()) == header->count()) {
            for (int i = 0; i < header->count(); i++) {
                header->resizeSection(i, prefs.restableColWidths[i]);
            }
        }
        header->setSortIndicatorShown(true);
        header->setSortIndicator(-1, Qt::AscendingOrder);
        header->setContextMenuPolicy(Qt::CustomContextMenu);
        header->setStretchLastSection(1);
        connect(header, SIGNAL(sectionResized(int,int,int)),
                this, SLOT(saveColState()));
        connect(header, SIGNAL(customContextMenuRequested(const QPoint&)),
                this, SLOT(createHeaderPopupMenu(const QPoint&)));
    }
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    header->setSectionsMovable(true);
#else
    header->setMovable(true);
#endif

    header = tableView->verticalHeader();
    if (header) {
        header->setDefaultSectionSize(QApplication::fontMetrics().height() + 
                                      ROWHEIGHTPAD);
    }

    QShortcut *sc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(sc, SIGNAL(activated()), tableView->selectionModel(), SLOT(clear()));
    connect(tableView->selectionModel(), 
            SIGNAL(currentChanged(const QModelIndex&, const QModelIndex &)),
            this, SLOT(onTableView_currentChanged(const QModelIndex&)));
    connect(tableView, SIGNAL(doubleClicked(const QModelIndex&)), 
            this, SLOT(onDoubleClick(const QModelIndex&)));

    m_pager = new ResTablePager(this);
    m_pager->setHighLighter(&g_hiliter);

    QSettings settings;
    QVariant saved = settings.value("resTableSplitterSizes");
    if (saved != QVariant()) {
        splitter->restoreState(saved.toByteArray());
    } else {
        QList<int> sizes;
        sizes << 355 << 125;
        splitter->setSizes(sizes);
    }

    delete textBrowser;
    m_detail = new ResTableDetailArea(this);
    m_detail->setReadOnly(true);
    m_detail->setUndoRedoEnabled(false);
    m_detail->setOpenLinks(false);
    // signals and slots connections
    connect(m_detail, SIGNAL(anchorClicked(const QUrl &)), 
            this, SLOT(linkWasClicked(const QUrl &)));
    splitter->addWidget(m_detail);
    splitter->setOrientation(Qt::Vertical);
    installEventFilter(this);
}

bool ResTable::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Enter) || (key->key() == Qt::Key_Return)) {
            menuEdit();
            return true;
        } else {
            return QObject::eventFilter(obj, event);
        }
    } else {
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void ResTable::setRclMain(RclMain *m, bool ismain) 
{
    m_rclmain = m;
    m_ismainres = ismain;

    // We allow single selection only in the main table because this
    // may have a mix of file-level docs and subdocs and multisave
    // only works for subdocs
    if (m_ismainres)
        tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    else
        tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    if (!m_ismainres) {
        // don't set this shortcut when we are a child of main, would
        // be duplicate/ambiguous
        connect(new QShortcut(quitKeySeq, this), SIGNAL(activated()),
                m_rclmain, SLOT (fileExit()));
    }

    new QShortcut(closeKeySeq, this, SLOT (close()));
    connect(this, SIGNAL(previewRequested(Rcl::Doc)), 
            m_rclmain, SLOT(startPreview(Rcl::Doc)));
    connect(this, SIGNAL(editRequested(Rcl::Doc)), 
            m_rclmain, SLOT(startNativeViewer(Rcl::Doc)));
    connect(this, SIGNAL(docSaveToFileClicked(Rcl::Doc)), 
            m_rclmain, SLOT(saveDocToFile(Rcl::Doc)));
    connect(this, SIGNAL(showSnippets(Rcl::Doc)), 
            m_rclmain, SLOT(showSnippets(Rcl::Doc)));
}

int ResTable::getDetailDocNumOrTopRow()
{
    if (m_detaildocnum >= 0)
        return m_detaildocnum;
    QModelIndex modelIndex = tableView->indexAt(QPoint(0, 0));
    return modelIndex.row();
}

void ResTable::makeRowVisible(int row)
{
    LOGDEB("ResTable::showRow(" << row << ")\n");
    QModelIndex modelIndex = m_model->index(row, 0);
    tableView->scrollTo(modelIndex, QAbstractItemView::PositionAtTop);
    tableView->selectionModel()->clear();
    m_detail->clear();
    m_detaildocnum = -1;
}

// This is called by rclmain_w prior to exiting
void ResTable::saveColState()
{
    if (!m_ismainres)
        return;
    QSettings settings;
    settings.setValue("resTableSplitterSizes", splitter->saveState());

    QHeaderView *header = tableView->horizontalHeader();
    const vector<string>& vf = m_model->getFields();
    if (!header) {
        LOGERR("ResTable::saveColState: no table header ??\n");
        return;
    }

    // Remember the current column order. Walk in visual order and
    // create new list
    QStringList newfields;
    vector<int> newwidths;
    for (int vi = 0; vi < header->count(); vi++) {
        int li = header->logicalIndex(vi);
        if (li < 0 || li >= int(vf.size())) {
            LOGERR("saveColState: logical index beyond list size!\n");
            continue;
        }
        newfields.push_back(QString::fromUtf8(vf[li].c_str()));
        newwidths.push_back(header->sectionSize(li));
    }
    prefs.restableFields = newfields;
    prefs.restableColWidths = newwidths;
}

void ResTable::onTableView_currentChanged(const QModelIndex& index)
{
    LOGDEB2("ResTable::onTableView_currentChanged(" << index.row() << ", " <<
            index.column() << ")\n");

    if (!m_model || !m_model->getDocSource())
        return;
    Rcl::Doc doc;
    if (m_model->getDocSource()->getDoc(index.row(), doc)) {
        m_detail->clear();
        m_detaildocnum = index.row();
        m_detaildoc = doc;
        m_pager->displayDoc(theconfig, index.row(), m_detaildoc, 
                            m_model->m_hdata);
        emit(detailDocChanged(doc, m_model->getDocSource()));
    } else {
        m_detaildocnum = -1;
    }
}

void ResTable::on_tableView_entered(const QModelIndex& index)
{
    LOGDEB2("ResTable::on_tableView_entered(" << index.row() << ", "  <<
            index.column() << ")\n");
    if (!tableView->selectionModel()->hasSelection())
        onTableView_currentChanged(index);
}

void ResTable::takeFocus()
{
//    LOGDEB("resTable: take focus\n");
    tableView->setFocus(Qt::ShortcutFocusReason);
}

void ResTable::setDocSource(std::shared_ptr<DocSequence> nsource)
{
    LOGDEB("ResTable::setDocSource\n");
    if (m_model)
        m_model->setDocSource(nsource);
    if (m_pager)
        m_pager->setDocSource(nsource, 0);
    if (m_detail)
        m_detail->clear();
    m_detaildocnum = -1;
}

void ResTable::resetSource()
{
    LOGDEB("ResTable::resetSource\n");
    setDocSource(std::shared_ptr<DocSequence>());
    readDocSource();
}

void ResTable::saveAsCSV()
{
    LOGDEB("ResTable::saveAsCSV\n");
    if (!m_model)
        return;
    QString s = 
        QFileDialog::getSaveFileName(this, //parent
                                     tr("Save table to CSV file"),
                                     QString::fromLocal8Bit(path_home().c_str())
            );
    if (s.isEmpty())
        return;
    const char *tofile = s.toLocal8Bit();
    FILE *fp = fopen(tofile, "w");
    if (fp == 0) {
        QMessageBox::warning(0, "Recoll", 
                             tr("Can't open/create file: ") + s);
        return;
    }
    m_model->saveAsCSV(fp);
    fclose(fp);
}

// This is called when the sort order is changed from another widget
void ResTable::onSortDataChanged(DocSeqSortSpec spec)
{
    LOGDEB("ResTable::onSortDataChanged: [" << spec.field << "] desc " <<
           spec.desc << "\n");
    QHeaderView *header = tableView->horizontalHeader();
    if (!header || !m_model)
        return;

    // Check if the specified field actually matches one of columns
    // and set indicator
    m_model->setIgnoreSort(true);
    bool matched = false;
    const vector<string> fields = m_model->getFields();
    for (unsigned int i = 0; i < fields.size(); i++) {
        if (!spec.field.compare(m_model->baseField(fields[i]))) {
            header->setSortIndicator(i, spec.desc ? 
                                     Qt::DescendingOrder : Qt::AscendingOrder);
            matched = true;
        }
    }
    if (!matched)
        header->setSortIndicator(-1, Qt::AscendingOrder);
    m_model->setIgnoreSort(false);
}

void ResTable::resetSort()
{
    LOGDEB("ResTable::resetSort()\n");
    QHeaderView *header = tableView->horizontalHeader();
    if (header)
        header->setSortIndicator(-1, Qt::AscendingOrder); 
    // the model's sort slot is not called by qt in this case (qt 4.7)
    if (m_model)
        m_model->sort(-1, Qt::AscendingOrder);
}

void ResTable::readDocSource(bool resetPos)
{
    LOGDEB("ResTable::readDocSource("  << resetPos << ")\n");
    if (resetPos)
        tableView->verticalScrollBar()->setSliderPosition(0);

    if (m_model->m_source) {
        m_model->m_source->getTerms(m_model->m_hdata);
    } else {
        m_model->m_hdata.clear();
    }
    m_model->readDocSource();
    m_detail->clear();
    m_detaildocnum = -1;
}

void ResTable::linkWasClicked(const QUrl &url)
{
    if (m_detaildocnum < 0) {
        return;
    }
    QString s = url.toString();
    const char *ascurl = s.toUtf8();
    LOGDEB("ResTable::linkWasClicked: [" << ascurl << "]\n");

    int i = atoi(ascurl+1) -1;
    int what = ascurl[0];
    switch (what) {
        // Open abstract/snippets window
    case 'A':
        if (m_detaildocnum >= 0)
            emit(showSnippets(m_detaildoc));
        break;
    case 'D':
    {
        vector<Rcl::Doc> dups;
        if (m_detaildocnum >= 0 && m_rclmain && 
            m_model->getDocSource()->docDups(m_detaildoc, dups)) {
            m_rclmain->newDupsW(m_detaildoc, dups);
        }
    }
    break;

    // Open parent folder
    case 'F':
    {
        emit editRequested(ResultPopup::getParent(std::shared_ptr<DocSequence>(),
                                                  m_detaildoc));
    }
    break;

    case 'P': 
    case 'E': 
    {
        if (what == 'P') {
            if (m_ismainres) {
                emit docPreviewClicked(i, m_detaildoc, 0);
            }  else {
                emit previewRequested(m_detaildoc);
            }
        } else {
            emit editRequested(m_detaildoc);
        }
    }
    break;

    // Run script. Link format Rnn|Script Name
    case 'R':
    {
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
            menuOpenWith(&act);
        }
    }
    break;

    default: 
        LOGERR("ResTable::linkWasClicked: bad link [" << ascurl << "]\n");
        break;// ?? 
    }
}

void ResTable::onDoubleClick(const QModelIndex& index)
{
    if (!m_model || !m_model->getDocSource())
        return;
    Rcl::Doc doc;
    if (m_model->getDocSource()->getDoc(index.row(), doc)) {
        if (m_detaildocnum != index.row()) {
            m_detail->clear();
            m_detaildocnum = index.row();
            m_pager->displayDoc(theconfig, index.row(), m_detaildoc, 
                                m_model->m_hdata);
        }
        m_detaildoc = doc;
        if (m_detaildocnum >= 0) 
            emit editRequested(m_detaildoc);
    } else {
        m_detaildocnum = -1;
    }
}

void ResTable::createPopupMenu(const QPoint& pos)
{
    LOGDEB("ResTable::createPopupMenu: m_detaildocnum " << m_detaildocnum <<
           "\n");
    if (m_detaildocnum >= 0 && m_model) {
        int opts = m_ismainres? ResultPopup::isMain : 0;
    
        int selsz = tableView->selectionModel()->selectedRows().size();

        if (selsz == 1) {
            opts |= ResultPopup::showSaveOne;
        } else if (selsz > 1 && !m_ismainres) {
            // We don't show save multiple for the main list because not all 
            // docs are necessary subdocs and multisave only works with those.
            opts |= ResultPopup::showSaveSel;
        }
        QMenu *popup = ResultPopup::create(this, opts, m_model->getDocSource(),
                                           m_detaildoc);
        popup->popup(mapToGlobal(pos));
    }
}

void ResTable::menuPreview()
{
    if (m_detaildocnum >= 0) {
        if (m_ismainres) {
            emit docPreviewClicked(m_detaildocnum, m_detaildoc, 0);
        } else {
            emit previewRequested(m_detaildoc);
        }
    }
}

void ResTable::menuSaveToFile()
{
    if (m_detaildocnum >= 0) 
        emit docSaveToFileClicked(m_detaildoc);
}

void ResTable::menuSaveSelection()
{
    if (m_model == 0 || !m_model->getDocSource())
        return;

    QModelIndexList indexl = tableView->selectionModel()->selectedRows();
    vector<Rcl::Doc> v;
    for (int i = 0; i < indexl.size(); i++) {
        Rcl::Doc doc;
        if (m_model->getDocSource()->getDoc(indexl[i].row(), doc))
            v.push_back(doc);
    }
    if (v.size() == 0) {
        return;
    } else if (v.size() == 1) {
        emit docSaveToFileClicked(v[0]);
    } else {
        multiSave(this, v);
    }
}

void ResTable::menuPreviewParent()
{
    if (m_detaildocnum >= 0 && m_model &&  
        m_model->getDocSource()) {
        Rcl::Doc pdoc = ResultPopup::getParent(m_model->getDocSource(), 
                                               m_detaildoc);
        if (pdoc.mimetype == "inode/directory") {
            emit editRequested(pdoc);
        } else {
            emit previewRequested(pdoc);
        }
    }
}

void ResTable::menuOpenParent()
{
    if (m_detaildocnum >= 0 && m_model && m_model->getDocSource())
        emit editRequested(
            ResultPopup::getParent(m_model->getDocSource(), m_detaildoc));
}

void ResTable::menuEdit()
{
    if (m_detaildocnum >= 0) 
        emit editRequested(m_detaildoc);
}
void ResTable::menuEditAndQuit()
{
    if (m_detaildocnum >= 0) {
        emit editRequested(m_detaildoc);
        m_rclmain->fileExit();
    }
}
void ResTable::menuOpenWith(QAction *act)
{
    if (act == 0)
        return;
    string cmd = qs2utf8s(act->data().toString());
    if (m_detaildocnum >= 0) 
        emit openWithRequested(m_detaildoc, cmd);
}

void ResTable::menuCopyFN()
{
    if (m_detaildocnum >= 0) 
        ResultPopup::copyFN(m_detaildoc);
}

void ResTable::menuCopyURL()
{
    if (m_detaildocnum >= 0) 
        ResultPopup::copyURL(m_detaildoc);
}

void ResTable::menuExpand()
{
    if (m_detaildocnum >= 0) 
        emit docExpand(m_detaildoc);
}

void ResTable::menuShowSnippets()
{
    if (m_detaildocnum >= 0)
        emit showSnippets(m_detaildoc);
}

void ResTable::menuShowSubDocs()
{
    if (m_detaildocnum >= 0)
        emit showSubDocs(m_detaildoc);
}

void ResTable::createHeaderPopupMenu(const QPoint& pos)
{
    LOGDEB("ResTable::createHeaderPopupMenu(" << pos.x() << ", " <<
           pos.y() << ")\n");
    QHeaderView *header = tableView->horizontalHeader();
    if (!header || !m_model)
        return;

    m_popcolumn = header->logicalIndexAt(pos);
    if (m_popcolumn < 0)
        return;

    const map<string, QString>& allfields = m_model->getAllFields();
    const vector<string>& fields = m_model->getFields();
    QMenu *popup = new QMenu(this);

    popup->addAction(tr("&Reset sort"), this, SLOT(resetSort()));
    popup->addSeparator();

    popup->addAction(tr("&Save as CSV"), this, SLOT(saveAsCSV()));
    popup->addSeparator();

    popup->addAction(tr("&Delete column"), this, SLOT(deleteColumn()));
    popup->addSeparator();

    QAction *act;
    for (map<string, QString>::const_iterator it = allfields.begin();
         it != allfields.end(); it++) {
        if (std::find(fields.begin(), fields.end(), it->first) != fields.end())
            continue;
        act = new QAction(tr("Add \"%1\" column").arg(it->second), popup);
        act->setData(QString::fromUtf8(it->first.c_str()));
        connect(act, SIGNAL(triggered(bool)), this , SLOT(addColumn()));
        popup->addAction(act);
    }
    popup->popup(mapToGlobal(pos));
}

void ResTable::deleteColumn()
{
    if (m_model)
        m_model->deleteColumn(m_popcolumn);
}

void ResTable::addColumn()
{
    if (!m_model)
        return;
    QAction *action = (QAction *)sender();
    LOGDEB("addColumn: text " << qs2utf8s(action->text()) << ", data " <<
           qs2utf8s(action->data().toString()) << "\n");
    m_model->addColumn(m_popcolumn, qs2utf8s(action->data().toString()));
}

