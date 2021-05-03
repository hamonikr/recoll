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
#ifndef _RESTABLE_H_INCLUDED_
#define _RESTABLE_H_INCLUDED_
#include "autoconfig.h"

#include <Qt>

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <fstream>
#include <functional>

#include "ui_restable.h"
#include "docseq.h"
#include "plaintorich.h"

class ResTable;

typedef std::string (FieldGetter)(
    const std::string& fldname, const Rcl::Doc& doc);

class RecollModel : public QAbstractTableModel {

    Q_OBJECT;

public:
    RecollModel(const QStringList fields, ResTable *tb, QObject *parent = 0);

    // Reimplemented methods
    virtual int rowCount (const QModelIndex& = QModelIndex()) const;
    virtual int columnCount(const QModelIndex& = QModelIndex()) const;
    virtual QVariant headerData (int col, Qt::Orientation orientation, 
                                 int role = Qt::DisplayRole ) const;
    virtual QVariant data(const QModelIndex& index, 
                          int role = Qt::DisplayRole ) const;
    virtual void saveAsCSV(std::fstream& fp);
    virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);
    // Specific methods
    virtual void readDocSource();
    virtual void setDocSource(std::shared_ptr<DocSequence> nsource);
    virtual std::shared_ptr<DocSequence> getDocSource() {return m_source;}
    virtual void deleteColumn(int);
    virtual const std::vector<std::string>& getFields() {return m_fields;}
    static const std::map<std::string, QString>& getAllFields() { 
        return o_displayableFields;
    }
    static QString displayableField(const std::string& in);
    virtual void addColumn(int, const std::string&);
    // Some column name are aliases/translator for base document field 
    // (ie: date, datetime->mtime). Help deal with this:
    virtual std::string baseField(const std::string&);

    // Ignore sort() call because 
    virtual void setIgnoreSort(bool onoff) {m_ignoreSort = onoff;}

    friend class ResTable;

signals:
    void sortDataChanged(DocSeqSortSpec);

private:
    ResTable *m_table{0};
    mutable std::shared_ptr<DocSequence> m_source;
    std::vector<std::string> m_fields;
    std::vector<FieldGetter*> m_getters;
    static std::map<std::string, QString> o_displayableFields;
    bool m_ignoreSort;
    FieldGetter* chooseGetter(const std::string&);
    HighlightData m_hdata;
    // Things we cache because we are repeatedly asked for the same.
    mutable QFont m_cachedfont;
    mutable int   m_reslfntszforcached{-1};
    mutable Rcl::Doc m_cachedoc;
    mutable int m_rowforcachedoc{-1};
};

class ResTable;

// Modified textBrowser for the detail area
class ResTableDetailArea : public QTextBrowser {
    Q_OBJECT;

public:
    ResTableDetailArea(ResTable* parent = 0);
    
public slots:
    virtual void createPopupMenu(const QPoint& pos);
    virtual void setFont();
    virtual void init();

private:
    ResTable *m_table;
};


class ResTablePager;
class QUrl;
class RclMain;
class QShortcut;

// This is an intermediary object to help setting up multiple similar
// shortcuts with different data (e.g. Ctrl+1, Ctrl+2 etc.). Maybe
// there is another way, but this one works.
class SCData : public QObject {
    Q_OBJECT;
public:
    SCData(QObject* parent, std::function<void (int)> cb, int row)
        : QObject(parent), m_cb(cb), m_row(row) {}
public slots:
    virtual void activate() {
        m_cb(m_row);
    }
private:
    std::function<void (int)>  m_cb;
    int m_row;
};

class ResTable : public QWidget, public Ui::ResTable 
{
    Q_OBJECT;

public:
    ResTable(QWidget* parent = 0) 
        : QWidget(parent) {
            setupUi(this);
            init();
        }
    
    virtual ~ResTable() {}
    virtual RecollModel *getModel() {return m_model;}
    virtual ResTableDetailArea* getDetailArea() {return m_detail;}
    virtual int getDetailDocNumOrTopRow();

    void setRclMain(RclMain *m, bool ismain);
    void setDefRowHeight();

public slots:
    virtual void onTableView_currentChanged(const QModelIndex&);
    virtual void on_tableView_entered(const QModelIndex& index);
    virtual void setDocSource(std::shared_ptr<DocSequence> nsource);
    virtual void saveColState();
    virtual void resetSource();
    virtual void readDocSource(bool resetPos = true);
    virtual void onSortDataChanged(DocSeqSortSpec);
    virtual void createPopupMenu(const QPoint& pos);
    virtual void onClicked(const QModelIndex&);
    virtual void onDoubleClick(const QModelIndex&);
    virtual void menuPreview();
    virtual void menuSaveToFile();
    virtual void menuSaveSelection();
    virtual void menuEdit();
    virtual void menuEditAndQuit();
    virtual void menuOpenWith(QAction *);
    virtual void menuCopyFN();
    virtual void menuCopyPath();
    virtual void menuCopyURL();
    virtual void menuCopyText();
    virtual void menuExpand();
    virtual void menuPreviewParent();
    virtual void menuOpenParent();
    virtual void menuOpenFolder();
    virtual void menuShowSnippets();
    virtual void menuShowSubDocs();
    virtual void createHeaderPopupMenu(const QPoint&);
    virtual void deleteColumn();
    virtual void addColumn();
    virtual void resetSort(); // Revert to natural (relevance) order
    virtual void saveAsCSV(); 
    virtual void linkWasClicked(const QUrl&);
    virtual void makeRowVisible(int row);
    virtual void takeFocus();
    virtual void onUiPrefsChanged();
    virtual void onNewShortcuts();
    virtual void setCurrentRowFromKbd(int row);
    virtual void toggleHeader();
    virtual void toggleVHeader();
    
signals:
    void docPreviewClicked(int, Rcl::Doc, int);
    void docSaveToFileClicked(Rcl::Doc);
    void previewRequested(Rcl::Doc);
    void editRequested(Rcl::Doc);
    void openWithRequested(Rcl::Doc, string cmd);
    void headerClicked();
    void docExpand(Rcl::Doc);
    void showSubDocs(Rcl::Doc);
    void showSnippets(Rcl::Doc);
    void detailDocChanged(Rcl::Doc, std::shared_ptr<DocSequence>);
    
    friend class ResTablePager;
    friend class ResTableDetailArea;

protected:
    bool eventFilter(QObject* obj, QEvent* event);

private:
    void init();

    RecollModel   *m_model{nullptr};
    ResTablePager *m_pager{nullptr};
    ResTableDetailArea *m_detail{nullptr};
    int            m_detaildocnum{-1};
    Rcl::Doc       m_detaildoc;
    int            m_popcolumn{0};
    RclMain *m_rclmain{nullptr};
    bool     m_ismainres{true};
    bool m_rowchangefromkbd{false};
    QShortcut *m_opensc{nullptr};
    QShortcut *m_openquitsc{nullptr};
    QShortcut *m_previewsc{nullptr};
    QShortcut *m_showsnipssc{nullptr};
    QShortcut *m_showheadersc{nullptr};
    QShortcut *m_showvheadersc{nullptr};
    QShortcut *m_copycurtextsc{nullptr};
    std::vector<SCData*> m_rowlinks;
    std::vector<QShortcut *> m_rowsc;
};

       
#endif /* _RESTABLE_H_INCLUDED_ */
