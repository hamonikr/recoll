/* Copyright (C) 2005-2020 J.F.Dockes
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
#include <fstream>

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
#include <QClipboard>
#include <QToolTip>

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
#include "scbase.h"

static const QKeySequence quitKeySeq("Ctrl+q");
static const QKeySequence closeKeySeq("Ctrl+w");

// Compensate for the default and somewhat bizarre vertical placement
// of text in cells
static const int ROWHEIGHTPAD = 2;
static const int TEXTINCELLVTRANS = -4;

// Adjust font size from prefs, display is slightly different here
static const int fsadjustdetail = 1;
static const int fsadjusttable = 1;

static PlainToRichQtReslist g_hiliter;

static const char *settingskey_fieldlist="/Recoll/prefs/query/restableFields";
static const char *settingskey_fieldwiths="/Recoll/prefs/query/restableWidths";
static const char *settingskey_splittersizes="resTableSplitterSizes";

//////////////////////////////////////////////////////////////////////////
// Restable "pager". We use it to print details for a document in the
// detail area
///
class ResTablePager : public ResListPager {
public:
    ResTablePager(ResTable *p)
        : ResListPager(1, prefs.alwaysSnippets), m_parent(p)
        {}
    virtual bool append(const string& data) override;
    virtual bool flush() override;
    virtual string trans(const string& in) override;
    virtual const string &parFormat() override;
    virtual string absSep() override {
        return (const char *)(prefs.abssep.toUtf8());}
    virtual string headerContent() override {
        return qs2utf8s(prefs.darkreslistheadertext) + qs2utf8s(prefs.reslistheadertext);
    }
private:
    ResTable *m_parent;
    string m_data;
};

bool ResTablePager::append(const string& data)
{
    m_data += data;
    return true;
}

bool ResTablePager::flush()
{
#ifdef helps_discoverability_of_shiftclick_but_is_ennoying
    QString msg = QApplication::translate(
        "ResTable", "Use Shift+click to display the text instead.");
    if (!prefs.resTableTextNoShift) {
        m_data += std::string("<p>") + qs2utf8s(msg) + "</p>";
    }
#endif
    m_parent->m_detail->setHtml(u8s2qs(m_data));
    m_data = "";
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

void ResTableDetailArea::setFont()
{
    int fs = prefs.reslistfontsize;
    // fs shows slightly bigger in qtextbrowser? adjust.
    if (prefs.reslistfontsize > fsadjustdetail) {
        fs -= fsadjustdetail;
    }
    if (prefs.reslistfontfamily != "") {
        QFont nfont(prefs.reslistfontfamily, fs);
        QTextBrowser::setFont(nfont);
    } else {
        QFont font;
        font.setPointSize(fs);
        QTextBrowser::setFont(font);
    }
}

void ResTableDetailArea::init()
{
    setFont();
    QTextBrowser::setHtml("");
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
        time_t mtime = doc.dmtime.empty() ?
            atoll(doc.fmtime.c_str()) : atoll(doc.dmtime.c_str());
        struct tm *tm = localtime(&mtime);
        sdate = utf8datestring("%Y-%m-%d", tm);
    }
    return sdate;
}

static string datetimegetter(const string&, const Rcl::Doc& doc)
{
    string datebuf;
    if (!doc.dmtime.empty() || !doc.fmtime.empty()) {
        time_t mtime = doc.dmtime.empty() ?
            atoll(doc.fmtime.c_str()) : atoll(doc.dmtime.c_str());
        struct tm *tm = localtime(&mtime);
        // Can't use reslistdateformat because it's html (&nbsp; etc.)
        datebuf = utf8datestring("%Y-%m-%d %H:%M:%S", tm);
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
    o_displayableFields["mtime"] = tr("Date");
    o_displayableFields["date"] = tr("Date");
    o_displayableFields["datetime"] = tr("Date and time");

    // Add dynamic "stored" fields to the full column list. This
    // could be protected to be done only once, but it's no real
    // problem
    if (theconfig) {
        const auto& stored = theconfig->getStoredFields();
        for (const auto& field : stored) {
            if (o_displayableFields.find(field) == o_displayableFields.end()) {
                o_displayableFields[field] = u8s2qs(field);
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
    m_rowforcachedoc = -1;
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

QString RecollModel::displayableField(const std::string& in)
{
    const auto it = o_displayableFields.find(in);
    return (it == o_displayableFields.end()) ? u8s2qs(in) : it->second;
}

QVariant RecollModel::headerData(int idx, Qt::Orientation orientation,
                                 int role) const
{
    LOGDEB2("RecollModel::headerData: idx " << idx << " orientation " <<
            (orientation == Qt::Vertical ? "vertical":"horizontal") <<
            " role " << role << "\n");
    if (orientation == Qt::Vertical && role == Qt::DisplayRole) {
        if (idx < 26) {
            return QString("%1/%2").arg(idx).arg(char('a'+idx));
        } else {
            return idx;
        }
    }
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole &&
        idx < int(m_fields.size())) {
        return displayableField(m_fields[idx]);
    }
    return QVariant();
}

QVariant RecollModel::data(const QModelIndex& index, int role) const
{
    LOGDEB2("RecollModel::data: row " << index.row() << " col " <<
            index.column() << " role " << role << "\n");

    // The font is actually set in the custom delegate, but we need
    // this to adjust the row height (there is probably a better way
    // to do it in the delegate?)
    if (role == Qt::FontRole && prefs.reslistfontsize > 0) {
        if (m_reslfntszforcached != prefs.reslistfontsize) {
            m_reslfntszforcached = prefs.reslistfontsize;
            m_table->setDefRowHeight();
            m_cachedfont = m_table->font();
            int fs = prefs.reslistfontsize <= fsadjusttable ?
                prefs.reslistfontsize: prefs.reslistfontsize - fsadjusttable;
            if (fs > 0)
                m_cachedfont.setPointSize(fs);
        }
        return m_cachedfont;
    }

    if (!m_source || role != Qt::DisplayRole || !index.isValid() ||
        index.column() >= int(m_fields.size())) {
        return QVariant();
    }

    if (m_rowforcachedoc != index.row()) {
        m_rowforcachedoc = index.row();
        m_cachedoc = Rcl::Doc();
        if (!m_source->getDoc(index.row(), m_cachedoc)) {
            return QVariant();
        }
    }

    string colname = m_fields[index.column()];

    string data = m_getters[index.column()](colname, m_cachedoc);

#ifndef _WIN32
    // Special case url, because it may not be utf-8. URL-encode in this case.
    // Not on windows, where we always read the paths as Unicode.
    if (!colname.compare("url")) {
        int ecnt;
        string data1;
        if (!transcode(data, data1, "UTF-8", "UTF-8", &ecnt) || ecnt > 0) {
            data = url_encode(data, 7);
        }
    }
#endif

    list<string> lr;
    g_hiliter.plaintorich(data, lr, m_hdata);
    return u8s2qs(lr.front());
}

void RecollModel::saveAsCSV(std::fstream& fp)
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
    fp << csv << "\n";
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
        fp << csv << "\n";
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
               const QModelIndex &index) const {

        QVariant value = index.data(Qt::DisplayRole);
        QString text;
        if (value.isValid() && !value.isNull()) {
            text = value.toString();
        }
        if (text.isEmpty()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        painter->save();

        /* As we draw with a text document, not the normal tableview
           painter, we need to retrieve the appropriate colors and set
           them as HTML styles. */
        QString color = opt.palette.color(QPalette::Base).name();
        QString textcolor = opt.palette.color(QPalette::Text).name();
        QString selcolor = opt.palette.color(QPalette::Highlight).name();
        QString seltextcolor =
            opt.palette.color(QPalette::HighlightedText).name();
        QString fstyle;
        if (prefs.reslistfontsize > 0) {
            int fs = prefs.reslistfontsize <= fsadjusttable ?
                prefs.reslistfontsize : prefs.reslistfontsize - fsadjusttable;
            fstyle = QString("font-size: %1pt").arg(fs);
        }
        QString ntxt("<div style='");
        ntxt += " color:";
        ntxt += (opt.state & QStyle::State_Selected)? seltextcolor:textcolor;
        ntxt += ";";
        ntxt += " background:";
        ntxt += (opt.state & QStyle::State_Selected)? selcolor:color;
        ntxt += ";";
        ntxt += fstyle;
        ntxt += QString("'>") + text + QString("</div>");
        text.swap(ntxt);
        
        painter->setClipRect(opt.rect);
        QPoint where = option.rect.topLeft();
        where.ry() += TEXTINCELLVTRANS;
        painter->translate(where);
        QTextDocument document;
        document.setHtml(text);
        document.drawContents(painter);
        painter->restore();
    }
};

void ResTable::setDefRowHeight()
{
    QHeaderView *header = tableView->verticalHeader();
    if (header) {
        // Don't do this: it forces a query on the whole model (all
        // docs) to compute the height. No idea why this was needed,
        // things seem to work ok without it. The row height does not
        // shrink when the font is reduced, but I'm not sure that it
        // worked before.
//        header->setSectionResizeMode(QHeaderView::ResizeToContents);
        // Compute ourselves instead, for one row.
        QFont font = tableView->font();
        int fs = prefs.reslistfontsize <= fsadjusttable ?
            prefs.reslistfontsize : prefs.reslistfontsize - fsadjusttable;
        if (fs > 0)
            font.setPointSize(fs);
        QFontMetrics fm(font);
        header->setDefaultSectionSize(fm.height() + ROWHEIGHTPAD);
        header->setSectionResizeMode(QHeaderView::Fixed);
    }
}

void ResTable::init()
{
    QSettings settings;
    auto restableFields = settings.value(settingskey_fieldlist).toStringList();
    if (restableFields.empty()) {
        restableFields.push_back("date");
        restableFields.push_back("title");
        restableFields.push_back("filename");
        restableFields.push_back("author");
        restableFields.push_back("url");
    }
    if (!(m_model = new RecollModel(restableFields, this)))
        return;
    tableView->setModel(m_model);
    tableView->setMouseTracking(true);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setItemDelegate(new ResTableDelegate(this));
    tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    tableView->setAlternatingRowColors(true);
    
    onNewShortcuts();
    connect(&SCBase::scBase(), SIGNAL(shortcutsChanged()),
            this, SLOT(onNewShortcuts()));

    auto sc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(sc, SIGNAL(activated()),
            tableView->selectionModel(), SLOT(clear()));
    
    connect(tableView, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(createPopupMenu(const QPoint&)));

    QHeaderView *header = tableView->horizontalHeader();
    if (header) {
        QString qw = settings.value(settingskey_fieldwiths).toString();
        vector<string> vw;
        stringToStrings(qs2utf8s(qw), vw);
        vector<int> restableColWidths;
        for (const auto& w : vw) {
            restableColWidths.push_back(atoi(w.c_str()));
        }
        if (int(restableColWidths.size()) == header->count()) {
            for (int i = 0; i < header->count(); i++) {
                header->resizeSection(i, restableColWidths[i]);
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
    setDefRowHeight();

    connect(tableView->selectionModel(),
            SIGNAL(currentChanged(const QModelIndex&, const QModelIndex &)),
            this, SLOT(onTableView_currentChanged(const QModelIndex&)));
    connect(tableView, SIGNAL(doubleClicked(const QModelIndex&)),
            this, SLOT(onDoubleClick(const QModelIndex&)));
    connect(tableView, SIGNAL(clicked(const QModelIndex&)),
            this, SLOT(onClicked(const QModelIndex&)));

    m_pager = new ResTablePager(this);
    m_pager->setHighLighter(&g_hiliter);

    deleteZ(textBrowser);
    m_detail = new ResTableDetailArea(this);
    m_detail->setReadOnly(true);
    m_detail->setUndoRedoEnabled(false);
    m_detail->setOpenLinks(false);
    m_detail->init();
    // signals and slots connections
    connect(m_detail, SIGNAL(anchorClicked(const QUrl &)),
            this, SLOT(linkWasClicked(const QUrl &)));
    splitter->addWidget(m_detail);
    splitter->setOrientation(Qt::Vertical);
    QVariant saved = settings.value(settingskey_splittersizes);
    if (saved != QVariant()) {
        splitter->restoreState(saved.toByteArray());
    } else {
        QList<int> sizes;
        sizes << 355 << 125;
        splitter->setSizes(sizes);
    }
    installEventFilter(this);
    onUiPrefsChanged();
}

void ResTable::onNewShortcuts()
{
    if (prefs.noResTableRowJumpSC) {
        for (auto& lnk : m_rowlinks) {
            delete lnk;
        }
        m_rowlinks.clear();
        for (auto& sc : m_rowsc) {
            delete sc;
        }
        m_rowsc.clear();
    } else if (m_rowlinks.empty()) {
        // Set "go to row" accelerator shortcuts. letter or digit for 0-9,
        // then letter up to 25
        std::function<void(int)> setrow =
            std::bind(&ResTable::setCurrentRowFromKbd, this, std::placeholders::_1);
        for (int i = 0; i <= 25; i++) {
            auto qs = QString("Ctrl+Shift+%1").arg(char('a'+i));
            auto sc = new QShortcut(QKeySequence(qs2utf8s(qs).c_str()), this);
            m_rowlinks.push_back(new SCData(this, setrow, i));
            m_rowsc.push_back(sc);
            connect(sc, SIGNAL(activated()), m_rowlinks.back(), SLOT(activate()));
            if (i > 9)
                continue;
            qs = QString("Ctrl+%1").arg(i);
            sc = new QShortcut(QKeySequence(qs2utf8s(qs).c_str()), this);
            m_rowsc.push_back(sc);
            m_rowlinks.push_back(new SCData(this, setrow, i));
            connect(sc, SIGNAL(activated()), m_rowlinks.back(), SLOT(activate()));
        }
    }
    SETSHORTCUT(this, "restable:704", tr("Result Table"),
                tr("Open current result document"),"Ctrl+O", m_opensc, menuEdit);
    SETSHORTCUT(this, "restable:706", tr("Result Table"),
                tr("Open current result and quit"),
                "Ctrl+Shift+O", m_openquitsc, menuEditAndQuit);
    SETSHORTCUT(this, "restable:709", tr("Result Table"), tr("Preview"),
                "Ctrl+D", m_previewsc, menuPreview);
    SETSHORTCUT(this, "restable:711", tr("Result Table"), tr("Show snippets"),
                "Ctrl+E", m_showsnipssc, menuShowSnippets);
    SETSHORTCUT(this, "restable:713", tr("Result Table"), tr("Show header"),
                "Ctrl+H", m_showheadersc, toggleHeader);
    SETSHORTCUT(this, "restable:715", tr("Result Table"),
                tr("Show vertical header"),
                "Ctrl+V", m_showvheadersc, toggleVHeader);
    SETSHORTCUT(this, "restable:718", tr("Result Table"),
                tr("Copy current result text to clipboard"),
                "Ctrl+G", m_copycurtextsc, menuCopyText);
    std::vector<QShortcut*> scps={
        m_opensc, m_openquitsc, m_previewsc, m_showsnipssc, m_showheadersc,
        m_showvheadersc, m_copycurtextsc};
    for (auto& scp : scps) {
        scp->setContext(Qt::WidgetWithChildrenShortcut);
    }
}

bool ResTable::eventFilter(QObject*, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Enter) || (key->key() == Qt::Key_Return)) {
            menuEdit();
            return true;
        }
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

void ResTable::toggleHeader()
{
    if (tableView->horizontalHeader()->isVisible()) {
        prefs.noResTableHeader = true;
        tableView->horizontalHeader()->hide();
    } else {
        prefs.noResTableHeader = false;
        tableView->horizontalHeader()->show();
    }
}

void ResTable::toggleVHeader()
{
    if (tableView->verticalHeader()->isVisible()) {
        prefs.showResTableVHeader = false;
        tableView->verticalHeader()->hide();
    } else {
        prefs.showResTableVHeader = true;
        tableView->verticalHeader()->show();
    }
}

void ResTable::onUiPrefsChanged()
{
    if (m_detail) {
        m_detail->setFont();
    }        
    auto index = tableView->indexAt(QPoint(0, 0));
    // There may be a better way to force repainting all visible rows
    // with the possibly new font, but this works...
    tableView->setAlternatingRowColors(false);
    tableView->setAlternatingRowColors(true);
    makeRowVisible(index.row());
    if (prefs.noResTableHeader) {
        tableView->horizontalHeader()->hide();
    } else {
        tableView->horizontalHeader()->show();
    }
    if (prefs.showResTableVHeader) {
        tableView->verticalHeader()->show();
    } else {
        tableView->verticalHeader()->hide();
    }
}

void ResTable::setCurrentRowFromKbd(int row)
{
    LOGDEB1("setCurrentRowFromKbd: " << row << "\n");
    m_rowchangefromkbd = true;
    tableView->setFocus(Qt::ShortcutFocusReason);

    // After calling setCurrentIndex(), currentChanged() gets called
    // twice, once with row 0 and no selection, once with the actual
    // target row and selection set. It uses this fact to discriminate
    // this from hovering. For some reason, when row is zero, there is
    // only one call. So, in this case, we first select row 1, and
    // this so pretty hack gets things working
    if (row == 0) {
        tableView->selectionModel()->setCurrentIndex(
            m_model->index(1, 0),
            QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows);
    }
    tableView->selectionModel()->setCurrentIndex(
        m_model->index(row, 0),
        QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows);
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
    m_detail->init();
    m_detaildocnum = -1;
}

// This is called by rclmain_w prior to exiting
void ResTable::saveColState()
{
    if (!m_ismainres)
        return;
    QSettings settings;
    settings.setValue(settingskey_splittersizes, splitter->saveState());

    QHeaderView *header = tableView->horizontalHeader();
    const vector<string>& vf = m_model->getFields();
    if (!header) {
        LOGERR("ResTable::saveColState: no table header ??\n");
        return;
    }

    // Remember the current column order. Walk in visual order and
    // create new list
    QStringList newfields;
    QString newwidths;
    for (int vi = 0; vi < header->count(); vi++) {
        int li = header->logicalIndex(vi);
        if (li < 0 || li >= int(vf.size())) {
            LOGERR("saveColState: logical index beyond list size!\n");
            continue;
        }
        newfields.push_back(u8s2qs(vf[li]));
        newwidths += QString().setNum(header->sectionSize(li)) + QString(" ");
    }
    settings.setValue(settingskey_fieldlist, newfields);
    settings.setValue(settingskey_fieldwiths, newwidths);
}

void ResTable::onTableView_currentChanged(const QModelIndex& index)
{
    bool hasselection = tableView->selectionModel()->hasSelection();
    LOGDEB2("ResTable::onTableView_currentChanged(" << index.row() << ", " <<
        index.column() << ") from kbd " << m_rowchangefromkbd  << " hasselection " <<
            hasselection << "\n");

    if (!m_model || !m_model->getDocSource())
        return;
    Rcl::Doc doc;
    if (m_model->getDocSource()->getDoc(index.row(), doc)) {
        m_detail->init();
        m_detaildocnum = index.row();
        m_detaildoc = doc;
        bool isShift = (QApplication::keyboardModifiers() & Qt::ShiftModifier);
        bool showtext{false};
        bool showmeta{false};
        if (m_rowchangefromkbd) {
            // Ctrl+... jump to row. Show text/meta as for simple click
            if (hasselection) {
                // When getting here from ctrl+... we get called twice, once with row 0
                // and no selection, once with the actual row and selection. Only
                // reset fromkbd and set showtext in the second case.
                m_rowchangefromkbd = false;
                showtext = prefs.resTableTextNoShift;
            }
        } else {
            // Mouse click. Show text or meta depending on shift key. Never show text when hovering
            // (no selection).
            showtext = hasselection && (isShift ^ prefs.resTableTextNoShift);
        }
        if (!showtext) {
            showmeta = hasselection || !prefs.resTableNoHoverMeta;
        }
        if (showtext && rcldb->getDocRawText(m_detaildoc)) {
            m_detail->setPlainText(u8s2qs(m_detaildoc.text));
        }  else if (showmeta) {
            m_pager->displaySingleDoc(theconfig, m_detaildocnum, m_detaildoc, m_model->m_hdata);
        }
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
        m_detail->init();
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
    QString s = QFileDialog::getSaveFileName(
        this, tr("Save table to CSV file"), path2qs(path_home()));
    if (s.isEmpty())
        return;
    std::string tofile = qs2path(s);
    std::fstream fp;
    if (!path_streamopen(tofile, std::ios::out|std::ios::trunc,fp)) {
        QMessageBox::warning(0, "Recoll",
                             tr("Can't open/create file: ") + s);
        return;
    }
    m_model->saveAsCSV(fp);
    fp.close();
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
    m_detail->init();
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

    int docseqnum = atoi(ascurl+1) -1;
    if (m_detaildocnum != docseqnum) {
        //? Really we should abort...
        LOGERR("ResTable::linkWasClicked: m_detaildocnum != docseqnum !\n");
        return;
    }
    
    int what = ascurl[0];
    switch (what) {
        // Open abstract/snippets window
    case 'A':
        emit(showSnippets(m_detaildoc));
        break;
    case 'D':
    {
        vector<Rcl::Doc> dups;
        if (m_rclmain && m_model->getDocSource()->docDups(m_detaildoc, dups)) {
            m_rclmain->newDupsW(m_detaildoc, dups);
        }
    }
    break;

    // Open parent folder
    case 'F':
    {
        emit editRequested(ResultPopup::getFolder(m_detaildoc));
    }
    break;

    case 'P':
    case 'E':
    {
        if (what == 'P') {
            if (m_ismainres) {
                emit docPreviewClicked(docseqnum, m_detaildoc, 0);
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

void ResTable::onClicked(const QModelIndex& index)
{
    // If the current row is the one clicked, currentChanged is not
    // called so that we would not do the text display if we did not
    // call it from here
    m_rowchangefromkbd = false;
    if (index.row() == m_detaildocnum) {
        onTableView_currentChanged(index);
    }
}

void ResTable::onDoubleClick(const QModelIndex& index)
{
    m_rowchangefromkbd = false;
    if (!m_model || !m_model->getDocSource())
        return;
    Rcl::Doc doc;
    if (m_model->getDocSource()->getDoc(index.row(), doc)) {
        if (m_detaildocnum != index.row()) {
            m_detail->init();
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
    if (m_detaildocnum >= 0 && m_model && m_model->getDocSource()) {
        Rcl::Doc pdoc =
            ResultPopup::getParent(m_model->getDocSource(), m_detaildoc);
        if (!pdoc.url.empty()) {
            emit editRequested(pdoc);
        }
    }
}

void ResTable::menuOpenFolder()
{
    if (m_detaildocnum >= 0) {
        Rcl::Doc pdoc = ResultPopup::getFolder(m_detaildoc);
        if (!pdoc.url.empty()) {
            emit editRequested(pdoc);
        }
    }
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

void ResTable::menuCopyPath()
{
    if (m_detaildocnum >= 0)
        ResultPopup::copyPath(m_detaildoc);
}

void ResTable::menuCopyURL()
{
    if (m_detaildocnum >= 0)
        ResultPopup::copyURL(m_detaildoc);
}

void ResTable::menuCopyText()
{
    if (m_detaildocnum >= 0 && rcldb) {
        ResultPopup::copyText(m_detaildoc, m_rclmain);
        if (m_rclmain) {
            auto msg = tr("%1 bytes copied to clipboard").arg(m_detaildoc.text.size());
            // Feedback was requested: tray messages are too ennoying, not
            // everybody displays the status bar, and the tool tip only
            // works when the copy is triggered through a shortcut (else,
            // it appears that the mouse event cancels it and it's not
            // shown). So let's do status bar if visible else tooltip.
            //  Menu trigger with no status bar -> no feedback...

            // rclmain->showTrayMessage(msg);
            if (m_rclmain->statusBar()->isVisible()) {
                m_rclmain->statusBar()->showMessage(msg, 1000);
            } else {
                int x = tableView->columnViewportPosition(0) + tableView->width() / 2 ;
                int y = tableView->rowViewportPosition(m_detaildocnum);
                QPoint pos = tableView->mapToGlobal(QPoint(x,y));
                QToolTip::showText(pos, msg);
                QTimer::singleShot(1500, m_rclmain, SLOT(hideToolTip()));
            }
        }

    }
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
    for (const auto& field : allfields) {
        if (std::find(fields.begin(), fields.end(), field.first) != fields.end())
            continue;
        act = new QAction(tr("Add \"%1\" column").arg(field.second), popup);
        act->setData(u8s2qs(field.first));
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
