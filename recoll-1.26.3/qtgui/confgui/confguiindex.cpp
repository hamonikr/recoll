/* Copyright (C) 2007 J.F.Dockes
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

#include <qglobal.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFrame>
#include <qlayout.h>
#include <qwidget.h>
#include <qlabel.h>
#include <qtimer.h>
#include <qmessagebox.h>
#include <qcheckbox.h>
#include <QListWidget>

#include <vector>
#include <set>
#include <string>
#include <functional>
using std::vector;
using std::set;
using std::string;

#include "recoll.h"
#include "confguiindex.h"
#include "smallut.h"
#include "log.h"
#include "rcldb.h"
#include "execmd.h"
#include "rclconfig.h"

static const int spacing = 3;
static const int margin = 3;

using namespace confgui;

/* Link class for ConfTree. Has a subkey pointer member which makes it easy 
 * to change the current subkey for multiple instances. */
class ConfLinkRclRep : public ConfLinkRep {
public:
    ConfLinkRclRep(ConfNull **conf, const string& nm, string *sk = 0)
        : m_conf(conf), m_nm(nm), m_sk(sk) /* KEEP THE POINTER, shared data */
        {}
    virtual ~ConfLinkRclRep() {}

    virtual bool set(const string& val) {
        if (!m_conf || !*m_conf)
            return false;
        LOGDEB("ConfLinkRclRep: set " << m_nm << " -> " << val << " sk " <<
               getSk() << std::endl);
        bool ret = (*m_conf)->set(m_nm, val, getSk());
        if (!ret)
            LOGERR("Value set failed\n" );
        return ret;
    }
    virtual bool get(string& val) {
        if (!m_conf || !*m_conf)
            return false;
        bool ret = (*m_conf)->get(m_nm, val, getSk());
        LOGDEB("ConfLinkRcl::get: [" << m_nm << "] sk [" <<
                getSk() << "] -> ["  << (ret ? val : "no value") << "]\n");
        return ret;
    }
private:
    string getSk() {
        return m_sk ? *m_sk : string();
    }
    ConfNull     **m_conf;
    const string  m_nm;
    const string *m_sk;
};


/* Special link for skippedNames and noContentSuffixes which are
   computed as set differences */
typedef std::function<vector<string>()> RclConfVecValueGetter;
class ConfLinkPlusMinus : public ConfLinkRep {
public:
    ConfLinkPlusMinus(RclConfig *rclconf, ConfNull **conf,
                      const string& basename, RclConfVecValueGetter getter,
                      string *sk = 0)
        : m_rclconf(rclconf), m_conf(conf),
          m_basename(basename), m_getter(getter),
          m_sk(sk) /* KEEP THE POINTER, shared data */ {
    }
    virtual ~ConfLinkPlusMinus() {}

    virtual bool set(const string& snval) {
        if (!m_conf || !*m_conf || !m_rclconf)
            return false;

        string sbase;
        (*m_conf)->get(m_basename, sbase, getSk());
        std::set<string> nval;
        stringToStrings(snval, nval);
        string splus, sminus;
        RclConfig::setPlusMinus(sbase, nval, splus, sminus);
        LOGDEB1("ConfLinkPlusMinus: base [" << sbase << "] nvalue [" << snval <<
                "] splus [" << splus << "] sminus [" << sminus << "]\n");
        if (!(*m_conf)->set(m_basename + "-", sminus, getSk())) {
            return false;
        }
        if (!(*m_conf)->set(m_basename + "+", splus, getSk())) {
            return false;
        }
        return true;
    }

    virtual bool get(string& val) {
        LOGDEB("ConfLinPlusMinus::get [" << m_basename << "]\n");
        if (!m_conf || !*m_conf || !m_rclconf)
            return false;

        m_rclconf->setKeyDir(getSk());
        vector<string> vval = m_getter();
        val = stringsToString(vval);
        LOGDEB1("ConfLinkPlusMinus: " << m_basename << " -> " << val << "\n");
        return true;
    }

private:
    string getSk() {
        return m_sk ? *m_sk : string();
    }
      
    RclConfig    *m_rclconf;
    ConfNull     **m_conf;
    string        m_basename;
    RclConfVecValueGetter m_getter;
    const string *m_sk;
};


class MyConfLinkFactRCL : public ConfLinkFact {
public:
    MyConfLinkFactRCL() {}
    MyConfLinkFactRCL(ConfNull **conf, string *sk = 0)
        : m_conf(conf), m_sk(sk) /* KEEP THE POINTER, shared data */
        {}
    virtual ConfLink operator()(const QString& nm) {
        ConfLinkRep *lnk = new ConfLinkRclRep(m_conf, qs2utf8s(nm), m_sk);
        return ConfLink(lnk);
    }
    ConfNull     **m_conf{nullptr};
    string *m_sk{nullptr};
};

string sknull;
static MyConfLinkFactRCL conflinkfactory;

void ConfIndexW::showPrefs(bool modal)
{
    delete m_conf;
    if ((m_conf = m_rclconf->cloneMainConfig()) == 0) {
        return;
    }
    m_conf->holdWrites(true);

    if (nullptr == m_w) {
        QString title = u8s2qs("Recoll - Index Settings: ");
        title += QString::fromLocal8Bit(m_rclconf->getConfDir().c_str());
        conflinkfactory = MyConfLinkFactRCL(&m_conf, &sknull);
        if (nullptr == (m_w = new ConfTabsW(this, title, &conflinkfactory))) {
            return;
        }
        connect(m_w, SIGNAL(sig_prefsChanged()), this, SLOT(acceptChanges()));
        initPanels();
    } else {
        m_w->hide();
    }

    m_w->reloadPanels();
    if (modal) {
        m_w->exec();
        m_w->setModal(false);
    } else {
        m_w->show();
    }
}

void ConfIndexW::acceptChanges()
{
    LOGDEB("ConfIndexW::acceptChanges()\n" );
    if (!m_conf) {
        LOGERR("ConfIndexW::acceptChanges: no config\n" );
        return;
    }
    if (!m_conf->holdWrites(false)) {
        QMessageBox::critical(0, "Recoll",  
                              tr("Can't write configuration file"));
    }
    // Delete local copy and update the main one from the file
    delete m_conf;
    m_conf = 0;
    m_rclconf->updateMainConfig();
}

void ConfIndexW::initPanels()
{
    int idx = m_w->addPanel(tr("Global parameters"));
    setupTopPanel(idx);

    idx = m_w->addForeignPanel(
        new ConfSubPanelW(m_w, &m_conf, m_rclconf),
        tr("Local parameters"));

    idx = m_w->addPanel("Web history");
    setupWebHistoryPanel(idx);

    idx = m_w->addPanel(tr("Search parameters"));
    setupSearchPanel(idx);
}

bool ConfIndexW::setupTopPanel(int idx)
{
    m_w->addParam(idx, ConfTabsW::CFPT_DNL, "topdirs", 
                  tr("Top directories"),
                  tr("The list of directories where recursive "
                     "indexing starts. Default: your home."));

    ConfParamW *cparam = m_w->addParam(
        idx, ConfTabsW::CFPT_DNL, "skippedPaths", tr("Skipped paths"),
        tr("These are pathnames of directories which indexing "
           "will not enter.<br>Path elements may contain wildcards. "
           "The entries must match the paths seen by the indexer "
           "(e.g.: if topdirs includes '/home/me' and '/home' is "
           "actually a link to '/usr/home', a correct skippedPath entry "
           "would be '/home/me/tmp*', not '/usr/home/me/tmp*')"));
    cparam->setFsEncoding(true);
    ((confgui::ConfParamSLW*)cparam)->setEditable(true);
    
    if (m_stemlangs.empty()) {
        vector<string> cstemlangs = Rcl::Db::getStemmerNames();
        for (const auto &clang : cstemlangs) {
            m_stemlangs.push_back(u8s2qs(clang));
        }
    }
    m_w->addParam(idx, ConfTabsW::CFPT_CSTRL, "indexstemminglanguages",
                  tr("Stemming languages"),
                  tr("The languages for which stemming expansion<br>"
                     "dictionaries will be built."), 0, 0, &m_stemlangs);

    m_w->addParam(idx, ConfTabsW::CFPT_FN, "logfilename",
                  tr("Log file name"),
                  tr("The file where the messages will be written.<br>"
                     "Use 'stderr' for terminal output"), 0);
    
    m_w->addParam(
        idx, ConfTabsW::CFPT_INT, "loglevel", tr("Log verbosity level"),
        tr("This value adjusts the amount of messages,<br>from only "
           "errors to a lot of debugging data."), 0, 6);

    m_w->addParam(idx, ConfTabsW::CFPT_INT, "idxflushmb",
                  tr("Index flush megabytes interval"),
                  tr("This value adjust the amount of "
                     "data which is indexed between flushes to disk.<br>"
                     "This helps control the indexer memory usage. "
                     "Default 10MB "), 0, 1000);

    m_w->addParam(idx, ConfTabsW::CFPT_INT, "maxfsoccuppc",
                  tr("Disk full threshold to stop indexing<br>"
                     "(e.g. 90%, 0 means no limit)"),
                  tr("This is the percentage of disk usage "
                     "- total disk usage, not index size - at which "
                     "indexing will fail and stop.<br>"
                     "The default value of 0 removes any limit."), 0, 100);

    ConfParamW *bparam = m_w->addParam(
        idx, ConfTabsW::CFPT_BOOL, "noaspell", tr("No aspell usage"),
        tr("Disables use of aspell to generate spelling "
           "approximation in the term explorer tool.<br> "
           "Useful if aspell is absent or does not work. "));
    cparam = m_w->addParam(
        idx, ConfTabsW::CFPT_STR, "aspellLanguage",
        tr("Aspell language"),
        tr("The language for the aspell dictionary. "
           "This should look like 'en' or 'fr' ...<br>"
           "If this value is not set, the NLS environment "
           "will be used to compute it, which usually works. "
           "To get an idea of what is installed on your system, "
           "type 'aspell config' and look for .dat files inside "
           "the 'data-dir' directory. "));
    m_w->enableLink(bparam, cparam, true);

    m_w->addParam(
        idx, ConfTabsW::CFPT_FN, "dbdir", tr("Database directory name"),
        tr("The name for a directory where to store the index<br>"
           "A non-absolute path is taken relative to the "
           "configuration directory. The default is 'xapiandb'."), true);
    m_w->addParam(idx, ConfTabsW::CFPT_STR, "unac_except_trans",
                  tr("Unac exceptions"),
                  tr("<p>These are exceptions to the unac mechanism "
                     "which, by default, removes all diacritics, "
                     "and performs canonic decomposition. You can override "
                     "unaccenting for some characters, depending on your "
                     "language, and specify additional decompositions, "
                     "e.g. for ligatures. In each space-separated entry, "
                     "the first character is the source one, and the rest "
                     "is the translation."
                      ));
    m_w->endOfList(idx);
    return  true;
}

bool ConfIndexW::setupWebHistoryPanel(int idx)
{
    ConfParamW *bparam = m_w->addParam(
        idx, ConfTabsW::CFPT_BOOL, "processwebqueue",
        tr("Process the WEB history queue"),
        tr("Enables indexing Firefox visited pages.<br>"
           "(you need also install the Firefox Recoll plugin)"));
    ConfParamW *cparam = m_w->addParam(
        idx, ConfTabsW::CFPT_FN, "webcachedir",
        tr("Web page store directory name"),
        tr("The name for a directory where to store the copies "
           "of visited web pages.<br>"
           "A non-absolute path is taken relative to the "
           "configuration directory."), 1);
    m_w->enableLink(bparam, cparam);

    cparam = m_w->addParam(
        idx, ConfTabsW::CFPT_INT, "webcachemaxmbs",
        tr("Max. size for the web store (MB)"),
        tr("Entries will be recycled once the size is reached."
           "<br>"
           "Only increasing the size really makes sense because "
           "reducing the value will not truncate an existing "
           "file (only waste space at the end)."
            ), -1, 1000*1000); // Max 1TB...
    m_w->enableLink(bparam, cparam);
    m_w->endOfList(idx);
    return true;
}

bool ConfIndexW::setupSearchPanel(int idx)
{
    if (!o_index_stripchars) {
        m_w->addParam(idx, ConfTabsW::CFPT_BOOL, "autodiacsens",
                      tr("Automatic diacritics sensitivity"),
                      tr("<p>Automatically trigger diacritics sensitivity "
                         "if the search term has accented characters "
                         "(not in unac_except_trans). Else you need to "
                         "use the query language and the <i>D</i> "
                         "modifier to specify diacritics sensitivity."));

        m_w->addParam(idx, ConfTabsW::CFPT_BOOL, "autocasesens",
                      tr("Automatic character case sensitivity"),
                      tr("<p>Automatically trigger character case "
                         "sensitivity if the entry has upper-case "
                         "characters in any but the first position. "
                         "Else you need to use the query language and "
                         "the <i>C</i> modifier to specify character-case "
                         "sensitivity."));
    }

    m_w->addParam(idx, ConfTabsW::CFPT_INT, "maxTermExpand",
                  tr("Maximum term expansion count"),
                  tr("<p>Maximum expansion count for a single term "
                     "(e.g.: when using wildcards). The default "
                     "of 10 000 is reasonable and will avoid "
                     "queries that appear frozen while the engine is "
                     "walking the term list."), 0, 100000);

    m_w->addParam(idx, ConfTabsW::CFPT_INT, "maxXapianClauses",
                  tr("Maximum Xapian clauses count"),
                  tr("<p>Maximum number of elementary clauses we "
                     "add to a single Xapian query. In some cases, "
                     "the result of term expansion can be "
                     "multiplicative, and we want to avoid using "
                     "excessive memory. The default of 100 000 "
                     "should be both high enough in most cases "
                     "and compatible with current typical hardware "
                     "configurations."), 0, 1000000);

    m_w->endOfList(idx);
    return true;
}

ConfSubPanelW::ConfSubPanelW(QWidget *parent, ConfNull **config,
                             RclConfig *rclconf)
    : QWidget(parent), m_config(config)
{
    QVBoxLayout *vboxLayout = new QVBoxLayout(this);
    vboxLayout->setSpacing(spacing);
    vboxLayout->setMargin(margin);

    m_subdirs = new ConfParamDNLW(
        "bogus00", this, ConfLink(new confgui::ConfLinkNullRep()), 
        QObject::tr("<b>Customised subtrees"),
        QObject::tr("The list of subdirectories in the indexed "
                    "hierarchy <br>where some parameters need "
                    "to be redefined. Default: empty."));
    m_subdirs->getListBox()->setSelectionMode(
        QAbstractItemView::SingleSelection);
    connect(m_subdirs->getListBox(), 
            SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
            this, 
            SLOT(subDirChanged(QListWidgetItem *, QListWidgetItem *)));
    connect(m_subdirs, SIGNAL(entryDeleted(QString)),
            this, SLOT(subDirDeleted(QString)));

    // We only retrieve the subkeys from the user's config (shallow),
    // no use to confuse the user by showing the subtrees which are
    // customized in the system config like .thunderbird or
    // .purple. This doesn't prevent them to add and customize them
    // further.
    vector<string> allkeydirs = (*config)->getSubKeys(true); 
    QStringList qls;
    for (const auto& dir: allkeydirs) {
        qls.push_back(u8s2qs(dir));
    }
    m_subdirs->getListBox()->insertItems(0, qls);

    vboxLayout->addWidget(m_subdirs);

    QFrame *line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    vboxLayout->addWidget(line2);

    QLabel *explain = new QLabel(this);
    explain->setWordWrap(true);
    explain->setText(
        QObject::tr(
            "<i>The parameters that follow are set either at the "
            "top level, if nothing "
            "or an empty line is selected in the listbox above, "
            "or for the selected subdirectory. "
            "You can add or remove directories by clicking "
            "the +/- buttons."));
    vboxLayout->addWidget(explain);


    m_groupbox = new QGroupBox(this);
    setSzPol(m_groupbox, QSizePolicy::Preferred, QSizePolicy::Preferred, 1, 3);

    QGridLayout *gl1 = new QGridLayout(m_groupbox);
    gl1->setSpacing(spacing);
    gl1->setMargin(margin);
    int gridy = 0;

    ConfParamSLW *eskn = new ConfParamSLW(
        "skippedNames", m_groupbox, 
        ConfLink(new ConfLinkPlusMinus(
                     rclconf, config, "skippedNames",
                     std::bind(&RclConfig::getSkippedNames, rclconf), &m_sk)),
        QObject::tr("Skipped names"),
        QObject::tr("These are patterns for file or directory "
                    " names which should not be indexed."));
    eskn->setFsEncoding(true);
    eskn->setImmediate();
    m_widgets.push_back(eskn);
    gl1->addWidget(eskn, gridy, 0);

    vector<string> amimes = rclconf->getAllMimeTypes();
    QStringList amimesq;
    for (const auto& mime: amimes) {
        amimesq.push_back(u8s2qs(mime));
    }

    ConfParamCSLW *eincm = new ConfParamCSLW(
        "indexedmimetypes", m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "indexedmimetypes", &m_sk)),
        tr("Only mime types"),
        tr("An exclusive list of indexed mime types.<br>Nothing "
           "else will be indexed. Normally empty and inactive"), amimesq);
    eincm->setImmediate();
    m_widgets.push_back(eincm);
    gl1->addWidget(eincm, gridy++, 1);

    ConfParamCSLW *eexcm = new ConfParamCSLW(
        "excludedmimetypes", m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "excludedmimetypes", &m_sk)),
        tr("Exclude mime types"),
        tr("Mime types not to be indexed"), amimesq);
    eexcm->setImmediate();
    m_widgets.push_back(eexcm);
    gl1->addWidget(eexcm, gridy, 0);

    ConfParamSLW *encs = new ConfParamSLW(
        "noContentSuffixes", m_groupbox, 
        ConfLink(new ConfLinkPlusMinus(
                     rclconf, config, "noContentSuffixes",
                     std::bind(&RclConfig::getStopSuffixes, rclconf), &m_sk)),
        QObject::tr("Ignored endings"),
        QObject::tr("These are file name endings for files which will be "
                    "indexed by name only \n(no MIME type identification "
                    "attempt, no decompression, no content indexing)."));
    encs->setImmediate();
    encs->setFsEncoding(true);
    m_widgets.push_back(encs);
    gl1->addWidget(encs, gridy++, 1);

    vector<string> args;
    args.push_back("-l");
    ExecCmd ex;
    string icout;
    string cmd = "iconv";
    int status = ex.doexec(cmd, args, 0, &icout);
    if (status) {
        LOGERR("Can't get list of charsets from 'iconv -l'");
    }
    icout = neutchars(icout, ",");
    vector<string> ccsets;
    stringToStrings(icout, ccsets);
    QStringList charsets;
    charsets.push_back("");
    for (const auto& charset : ccsets) {
        charsets.push_back(u8s2qs(charset));
    }

    ConfParamCStrW *e21 = new ConfParamCStrW(
        "defaultcharset", m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "defaultcharset", &m_sk)), 
        QObject::tr("Default<br>character set"),
        QObject::tr("Character set used for reading files "
                    "which do not identify the character set "
                    "internally, for example pure text files.<br>"
                    "The default value is empty, "
                    "and the value from the NLS environnement is used."
            ), charsets);
    e21->setImmediate();
    m_widgets.push_back(e21);
    gl1->addWidget(e21, gridy++, 0);

    ConfParamBoolW *e3 = new ConfParamBoolW(
        "followLinks", m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "followLinks", &m_sk)), 
        QObject::tr("Follow symbolic links"),
        QObject::tr("Follow symbolic links while "
                    "indexing. The default is no, "
                    "to avoid duplicate indexing"));
    e3->setImmediate();
    m_widgets.push_back(e3);
    gl1->addWidget(e3, gridy, 0);

    ConfParamBoolW *eafln = new ConfParamBoolW(
        "indexallfilenames", m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "indexallfilenames", &m_sk)), 
        QObject::tr("Index all file names"),
        QObject::tr("Index the names of files for which the contents "
                    "cannot be identified or processed (no or "
                    "unsupported mime type). Default true"));
    eafln->setImmediate();
    m_widgets.push_back(eafln);
    gl1->addWidget(eafln, gridy++, 1);

    ConfParamIntW *ezfmaxkbs = new ConfParamIntW(
        "compressedfilemaxkbs", m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "compressedfilemaxkbs", &m_sk)), 
        tr("Max. compressed file size (KB)"),
        tr("This value sets a threshold beyond which compressed"
           "files will not be processed. Set to -1 for no "
           "limit, to 0 for no decompression ever."), -1, 1000000, -1);
    ezfmaxkbs->setImmediate();
    m_widgets.push_back(ezfmaxkbs);
    gl1->addWidget(ezfmaxkbs, gridy, 0);

    ConfParamIntW *etxtmaxmbs = new ConfParamIntW(
        "textfilemaxmbs", m_groupbox,
        ConfLink(new ConfLinkRclRep(config, "textfilemaxmbs", &m_sk)), 
        tr("Max. text file size (MB)"),
        tr("This value sets a threshold beyond which text "
           "files will not be processed. Set to -1 for no "
           "limit. \nThis is for excluding monster "
           "log files from the index."), -1, 1000000);
    etxtmaxmbs->setImmediate();
    m_widgets.push_back(etxtmaxmbs);
    gl1->addWidget(etxtmaxmbs, gridy++, 1);

    ConfParamIntW *etxtpagekbs = new ConfParamIntW(
        "textfilepagekbs", m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "textfilepagekbs", &m_sk)),
        tr("Text file page size (KB)"),
        tr("If this value is set (not equal to -1), text "
           "files will be split in chunks of this size for "
           "indexing.\nThis will help searching very big text "
           " files (ie: log files)."), -1, 1000000);
    etxtpagekbs->setImmediate();
    m_widgets.push_back(etxtpagekbs);
    gl1->addWidget(etxtpagekbs, gridy, 0);

    ConfParamIntW *efiltmaxsecs = new ConfParamIntW(
        "filtermaxseconds", m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "filtermaxseconds", &m_sk)), 
        tr("Max. filter exec. time (s)"),
        tr("External filters working longer than this will be "
           "aborted. This is for the rare case (ie: postscript) "
           "where a document could cause a filter to loop. "
           "Set to -1 for no limit.\n"), -1, 10000);
    efiltmaxsecs->setImmediate();
    m_widgets.push_back(efiltmaxsecs);
    gl1->addWidget(efiltmaxsecs, gridy++, 1);

    vboxLayout->addWidget(m_groupbox);
    subDirChanged(0, 0);
    LOGDEB("ConfSubPanelW::ConfSubPanelW: done\n");
}

void ConfSubPanelW::loadValues()
{
    LOGDEB("ConfSubPanelW::loadValues\n");
    for (auto widget : m_widgets) {
        widget->loadValue();
    }
    LOGDEB("ConfSubPanelW::loadValues done\n");
}

void ConfSubPanelW::storeValues()
{
    for (auto widget : m_widgets) {
        widget->storeValue();
    }
}

void ConfSubPanelW::subDirChanged(QListWidgetItem *current, QListWidgetItem *)
{
    LOGDEB("ConfSubPanelW::subDirChanged\n");
    
    if (current == 0 || current->text() == "") {
        m_sk = "";
        m_groupbox->setTitle(tr("Global"));
    } else {
        m_sk = qs2utf8s(current->text());
        m_groupbox->setTitle(current->text());
    }
    LOGDEB("ConfSubPanelW::subDirChanged: now [" << m_sk << "]\n");
    loadValues();
    LOGDEB("ConfSubPanelW::subDirChanged: done\n");
}

void ConfSubPanelW::subDirDeleted(QString sbd)
{
    LOGDEB("ConfSubPanelW::subDirDeleted(" << qs2utf8s(sbd) << ")\n");
    if (sbd == "") {
        // Can't do this, have to reinsert it
        QTimer::singleShot(0, this, SLOT(restoreEmpty()));
        return;
    }
    // Have to delete all entries for submap
    (*m_config)->eraseKey(qs2utf8s(sbd));
}

void ConfSubPanelW::restoreEmpty()
{
    LOGDEB("ConfSubPanelW::restoreEmpty()\n");
    m_subdirs->getListBox()->insertItem(0, "");
}
