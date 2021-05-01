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

#include <cstdlib>
#include <list>

#include <qapplication.h>
#include <qtranslator.h>
#include <qtextcodec.h> 
#include <qtimer.h>
#include <qthread.h>
#include <qmessagebox.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <QLocale>
#include <QLibraryInfo>
#include <QFileDialog>
#include <QUrl>

#include "rcldb.h"
#include "rclconfig.h"
#include "pathut.h"
#include "recoll.h"
#include "smallut.h"
#include "rclinit.h"
#include "log.h"
#include "rclmain_w.h"
#include "ssearch_w.h"
#include "guiutils.h"
#include "smallut.h"
#include "readfile.h"
#include "uncomp.h"

#include "recollq.h"

extern RclConfig *theconfig;

std::mutex thetempfileslock;
// Use a list not a vector so that contained objects have stable
// addresses when extending.
static list<TempFile>  o_tempfiles;
/* Keep an array of temporary files for deletion at exit. It happens that we
   erase some of them before exiting (ie: when closing a preview tab), we don't 
   reuse the array holes for now */
TempFile *rememberTempFile(TempFile temp)
{
    std::unique_lock<std::mutex> locker(thetempfileslock);
    o_tempfiles.push_back(temp);
    return &o_tempfiles.back();
}    

void forgetTempFile(string &fn)
{
    if (fn.empty())
        return;
    std::unique_lock<std::mutex> locker(thetempfileslock);
    for (auto& entry : o_tempfiles) {
        if (entry.ok() && !fn.compare(entry.filename())) {
            entry = TempFile();
        }
    }
    fn.erase();
}    

void deleteAllTempFiles()
{
    std::unique_lock<std::mutex> locker(thetempfileslock);
    o_tempfiles.clear();
    Uncomp::clearcache();
}

std::shared_ptr<Rcl::Db> rcldb;
int recollNeedsExit;
RclMain *mainWindow;

void startManual(const string& helpindex)
{
    if (mainWindow)
        mainWindow->startManual(helpindex);
}

bool maybeOpenDb(string &reason, bool force, bool *maindberror)
{
    LOGDEB2("maybeOpenDb: force " << force << "\n");

    if (force) {
        rcldb = std::shared_ptr<Rcl::Db>(new Rcl::Db(theconfig));
    }
    rcldb->rmQueryDb("");
    for (const auto& dbdir : prefs.activeExtraDbs) {
        LOGDEB("main: adding [" << dbdir << "]\n");
        rcldb->addQueryDb(dbdir);
    }
    Rcl::Db::OpenError error;
    if (!rcldb->isopen() && !rcldb->open(Rcl::Db::DbRO, &error)) {
        reason = "Could not open database";
        if (maindberror) {
            reason +=  " in " +  theconfig->getDbDir() + 
                " wait for indexing to complete?";          
            *maindberror = (error == Rcl::Db::DbOpenMainDb) ? true : false;
        }
        return false;
    }
    rcldb->setAbstractParams(-1, prefs.syntAbsLen, prefs.syntAbsCtx);
    return true;
}

// Retrieve the list currently active stemming languages. We try to
// get this from the db, as some may have been added from recollindex
// without changing the config. If this fails, use the config. This is
// used for setting up choice menus, not updating the configuration.
bool getStemLangs(vector<string>& vlangs)
{
    // Try from db
    string reason;
    if (maybeOpenDb(reason)) {
        vlangs = rcldb->getStemLangs();
        LOGDEB0("getStemLangs: from index: " << stringsToString(vlangs) <<"\n");
        return true;
    } else {
        // Cant get the langs from the index. Maybe it just does not
        // exist yet. So get them from the config
        string slangs;
        if (theconfig->getConfParam("indexstemminglanguages", slangs)) {
            stringToStrings(slangs, vlangs);
            return true;
        }
        return false;
    }
}

// This is never called because we _Exit() in rclmain_w.cpp
static void recollCleanup()
{
    LOGDEB2("recollCleanup: closing database\n" );
    rcldb.reset();
    deleteZ(theconfig);

    deleteAllTempFiles();
    LOGDEB2("recollCleanup: done\n" );
}

void applyStyleSheet(const QString& ssfname)
{
    const char *cfname = (const char *)ssfname.toLocal8Bit();
    LOGDEB0("Applying style sheet: ["  << (cfname) << "]\n" );
    if (cfname && *cfname) {
        string stylesheet;
        file_to_string(cfname, stylesheet);
        qApp->setStyleSheet(QString::fromUtf8(stylesheet.c_str()));
    } else {
        qApp->setStyleSheet(QString());
    }
}

extern void qInitImages_recoll();

static const char *thisprog;

// BEWARE COMPATIBILITY WITH recollq OPTIONS letters
static int    op_flags;
#define OPT_a 0x1   
#define OPT_c 0x2       
#define OPT_f 0x4       
#define OPT_h 0x8       
#define OPT_L 0x10      
#define OPT_l 0x20      
#define OPT_o 0x40      
#define OPT_q 0x80      
#define OPT_t 0x100      
#define OPT_v 0x200     
#define OPT_w 0x400

static const char usage [] =
    "\n"
    "recoll [-h] [-c <configdir>] [-q query]\n"
    "  -h : Print help and exit\n"
    "  -c <configdir> : specify config directory, overriding $RECOLL_CONFDIR\n"
    "  -L <lang> : force language for GUI messages (e.g. -L fr)\n"
    "  [-o|l|f|a] [-t] -q 'query' : search query to be executed as if entered\n"
    "      into simple search. The default is to interpret the argument as a \n"
    "      query language string (but see modifier options)\n"
    "      In most cases, the query string should be quoted with single-quotes to\n"
    "      avoid shell interpretation\n"
    "     -a : the query will be interpreted as an AND query.\n"
    "     -o : the query will be interpreted as an OR query.\n"
    "     -f : the query will be interpreted as a filename search\n"
    "     -l : the query will be interpreted as a query language string (default)\n"
    "  -t : terminal display: no gui. Results go to stdout. MUST be given\n"
    "       explicitly as -t (not ie, -at), and -q <query> MUST\n"
    "       be last on the command line if this is used.\n"
    "       Use -t -h to see the additional non-gui options\n"
    "  -w : open minimized\n"
    "recoll -v : print version\n"
    "recoll <url>\n"
    "   This is used to open a recoll url (including an ipath), and called\n"
    "   typically from another search interface like the Unity Dash\n"
    ;
static void
Usage(void)
{
    FILE *fp = (op_flags & OPT_h) ? stdout : stderr;
    fprintf(fp, "%s\n", Rcl::version_string().c_str());
    fprintf(fp, "%s: Usage: %s", thisprog, usage);
    exit((op_flags & OPT_h)==0);
}

int main(int argc, char **argv)
{
    // if we are named recollq or option "-t" is present at all, we
    // don't do the GUI thing and pass the whole to recollq for
    // command line / pipe usage.
    if (!strcmp(argv[0], "recollq"))
        exit(recollq(&theconfig, argc, argv));
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-t")) {
            exit(recollq(&theconfig, argc, argv));
        }
    }

#ifdef USING_WEBENGINE
    // This is necessary for allowing webengine to load local resources (icons)
    // It is not an issue because we never access remote sites.
    char arg_disable_web_security[] = "--disable-web-security";
    int appargc = argc + 1;
    char** appargv = new char*[appargc+1];
    for(int i = 0; i < argc; i++) {
        appargv[i] = argv[i];
    }
    appargv[argc] = arg_disable_web_security;
    appargv[argc+1] = nullptr;
    QApplication app(appargc, appargv);
#else
    QApplication app(argc, argv);
#endif

    QCoreApplication::setOrganizationName("Recoll.org");
    QCoreApplication::setApplicationName("recoll");

    string a_config;
    string a_lang;
    string question;
    string urltoview;

    // Avoid disturbing argc and argv. Especially, setting argc to 0
    // prevents WM_CLASS to be set from argv[0] (it appears that qt
    // keeps a ref to argc, and that it is used at exec() time to set
    // WM_CLASS from argv[0]). Curiously, it seems that the argv
    // pointer can be modified without consequences, but we use a copy
    // to play it safe
    int myargc = argc;
    char **myargv = argv;
    thisprog = myargv[0];
    myargc--; myargv++;
    
    while (myargc > 0 && **myargv == '-') {
        (*myargv)++;
        if (!(**myargv))
            Usage();
        while (**myargv)
            switch (*(*myargv)++) {
            case 'a': op_flags |= OPT_a; break;
            case 'c':   op_flags |= OPT_c; if (myargc < 2)  Usage();
                a_config = *(++myargv);
                myargc--; goto b1;
            case 'f': op_flags |= OPT_f; break;
            case 'h': op_flags |= OPT_h; Usage();break;
            case 'L':   op_flags |= OPT_L; if (myargc < 2)  Usage();
                a_lang = *(++myargv);
                myargc--; goto b1;
            case 'l': op_flags |= OPT_l; break;
            case 'o': op_flags |= OPT_o; break;
            case 'q':   op_flags |= OPT_q; if (myargc < 2)  Usage();
                question = *(++myargv);
                myargc--; goto b1;
            case 't': op_flags |= OPT_t; break;
            case 'v': op_flags |= OPT_v;
                fprintf(stdout, "%s\n", Rcl::version_string().c_str());
                return 0;
            case 'w': op_flags |= OPT_w; break;
            default: Usage();
            }
    b1: myargc--; myargv++;
    }

    // If -q was given, all remaining non-option args are concatenated
    // to the query. This is for the common case recoll -q x y z to
    // avoid needing quoting "x y z"
    if (op_flags & OPT_q)
        while (myargc > 0) {
            question += " ";
            question += *myargv++;
            myargc--;
        }

    // Else the remaining argument should be an URL to be opened
    if (myargc == 1) {
        urltoview = *myargv++;myargc--;
        if (urltoview.compare(0, 7, cstr_fileu)) {
            Usage();
        }
    } else if (myargc > 0)
        Usage();


    string reason;
    theconfig = recollinit(0, recollCleanup, 0, reason, &a_config);
    if (!theconfig || !theconfig->ok()) {
        QString msg = "Configuration problem: ";
        msg += QString::fromUtf8(reason.c_str());
        QMessageBox::critical(0, "Recoll",  msg);
        exit(1);
    }
    //    fprintf(stderr, "recollinit done\n");

    // Translations for Qt standard widgets
    QString slang;
    if (op_flags & OPT_L) {
        slang = u8s2qs(a_lang);
    } else {
        slang = QLocale::system().name().left(2);
    }
    QTranslator qt_trans(0);
    qt_trans.load(QString("qt_%1").arg(slang), 
                  QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    app.installTranslator(&qt_trans);

    // Translations for Recoll
    string translatdir = path_cat(theconfig->getDatadir(), "translations");
    QTranslator translator(0);
    translator.load( QString("recoll_") + slang, translatdir.c_str() );
    app.installTranslator( &translator );

    //    fprintf(stderr, "Translations installed\n");

    string historyfile = path_cat(theconfig->getConfDir(), "history");
    g_dynconf = new RclDynConf(historyfile);
    if (!g_dynconf || !g_dynconf->ok()) {
        QString msg = app.translate
            ("Main",
             "\"history\" file is damaged, please check "
             "or remove it: ") + QString::fromLocal8Bit(historyfile.c_str());
        QMessageBox::critical(0, "Recoll",  msg);
        exit(1);
    }
    g_advshistory = new AdvSearchHist;

    //    fprintf(stderr, "History done\n");
    rwSettings(false);
    //    fprintf(stderr, "Settings done\n");

    if (!prefs.qssFile.isEmpty()) {
        applyStyleSheet(prefs.qssFile);
    }
    QIcon icon;
    icon.addFile(QString::fromUtf8(":/images/recoll.png"));
    app.setWindowIcon(icon);

    // Create main window and set its size to previous session's
    RclMain w;
    mainWindow = &w;

    if (prefs.mainwidth > 100) {
        QSize s(prefs.mainwidth, prefs.mainheight);
        mainWindow->resize(s);
    }

    string dbdir = theconfig->getDbDir();
    if (dbdir.empty()) {
        QMessageBox::critical(
            0, "Recoll",
            app.translate("Main", "No db directory in configuration"));
        exit(1);
    }

    maybeOpenDb(reason);

    if (op_flags & OPT_w) {
        mainWindow->showMinimized();
    } else {
        switch (prefs.showmode) {
        case PrefsPack::SHOW_NORMAL: mainWindow->show(); break;
        case PrefsPack::SHOW_MAX: mainWindow->showMaximized(); break;
        case PrefsPack::SHOW_FULL: mainWindow->showFullScreen(); break;
        }
    }
    QTimer::singleShot(0, mainWindow, SLOT(initDbOpen()));

    // Connect exit handlers etc.. Beware, apparently this must come
    // after mainWindow->show()?
    app.connect(&app, SIGNAL(lastWindowClosed()), &app, SLOT(quit()));
    app.connect(&app, SIGNAL(aboutToQuit()), mainWindow, SLOT(close()));

    mainWindow->sSearch->searchTypCMB->setCurrentIndex(prefs.ssearchTyp);
    mainWindow->sSearch->searchTypeChanged(prefs.ssearchTyp);
    if (op_flags & OPT_q) {
        SSearch::SSearchType stype;
        if (op_flags & OPT_o) {
            stype = SSearch::SST_ANY;
        } else if (op_flags & OPT_f) {
            stype = SSearch::SST_FNM;
        } else if (op_flags & OPT_a) {
            stype = SSearch::SST_ALL;
        } else {
            stype = SSearch::SST_LANG;
        }
        mainWindow->sSearch->searchTypCMB->setCurrentIndex(int(stype));
        mainWindow->
            sSearch->setSearchString(QString::fromLocal8Bit(question.c_str()));
    } else if (!urltoview.empty()) {
        LOGDEB("MAIN: got urltoview ["  << (urltoview) << "]\n" );
        mainWindow->setUrlToView(QString::fromLocal8Bit(urltoview.c_str()));
    }
    return app.exec();
}

QString myGetFileName(bool isdir, QString caption, bool filenosave,
                      QString dirloc, QString dfltnm)
{
    LOGDEB1("myFileDialog: isdir " << isdir << "\n");
    QFileDialog dialog(0, caption);

#ifdef _WIN32
    // The default initial directory on WIndows is the Recoll install,
    // which is not appropriate. Change it, only for the first call
    // (next will start with the previous selection).
    static bool first{true};
    if (first) {
        first = false;
        // See https://doc.qt.io/qt-5/qfiledialog.html#setDirectoryUrl
        // about the clsid magic (this one points to the desktop).
        dialog.setDirectoryUrl(
            QUrl("clsid:B4BFCC3A-DB2C-424C-B029-7FE99A87C641"));
    }
#endif
    if (!dirloc.isEmpty()) {
        dialog.setDirectory(dirloc);
    }
    if (!dfltnm.isEmpty()) {
        dialog.selectFile(dfltnm);
    }
    if (isdir) {
        dialog.setFileMode(QFileDialog::Directory);
        dialog.setOptions(QFileDialog::ShowDirsOnly);
    } else {
        dialog.setFileMode(QFileDialog::AnyFile);
        if (filenosave)
            dialog.setAcceptMode(QFileDialog::AcceptOpen);
        else
            dialog.setAcceptMode(QFileDialog::AcceptSave);
    }
    dialog.setViewMode(QFileDialog::List);
    QFlags<QDir::Filter> flags = QDir::NoDotAndDotDot | QDir::Hidden; 
    if (isdir)
        flags |= QDir::Dirs;
    else 
        flags |= QDir::Dirs | QDir::Files;
    dialog.setFilter(flags);

    if (dialog.exec() == QDialog::Accepted) {
        return dialog.selectedFiles().value(0);
    }
    return QString();
}
