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

#include "safeunistd.h"

#include <list>

#include <QMessageBox>

#include "qxtconfirmationmessage.h"

#include "log.h"
#include "fileudi.h"
#include "execmd.h"
#include "transcode.h"
#include "docseqhist.h"
#include "docseqdb.h"
#include "internfile.h"
#include "rclmain_w.h"
#include "rclzg.h"
#include "pathut.h"

using namespace std;

// Start native viewer or preview for input Doc. This is used to allow
// using recoll from another app (e.g. Unity Scope) to view embedded
// result docs (docs with an ipath). . We act as a proxy to extract
// the data and start a viewer.  The Url are encoded as
// file://path#ipath
void RclMain::viewUrl()
{
    if (m_urltoview.isEmpty() || !rcldb)
	return;

    QUrl qurl(m_urltoview);
    LOGDEB("RclMain::viewUrl: Path [" <<
           ((const char *)qurl.path().toLocal8Bit()) << "] fragment ["
           << ((const char *)qurl.fragment().toLocal8Bit()) << "]\n");

    /* In theory, the url might not be for a file managed by the fs
       indexer so that the make_udi() call here would be
       wrong(). When/if this happens we'll have to hide this part
       inside internfile and have some url magic to indicate the
       appropriate indexer/identification scheme */
    string udi;
    make_udi((const char *)qurl.path().toLocal8Bit(),
	     (const char *)qurl.fragment().toLocal8Bit(), udi);
    
    Rcl::Doc doc;
    Rcl::Doc idxdoc; // idxdoc.idxi == 0 -> works with base index only
    if (!rcldb->getDoc(udi, idxdoc, doc) || doc.pc == -1)
	return;

    // StartNativeViewer needs a db source to call getEnclosing() on.
    Rcl::Query *query = new Rcl::Query(rcldb.get());
    DocSequenceDb *src = new DocSequenceDb(
        rcldb, std::shared_ptr<Rcl::Query>(query), "", 
        std::shared_ptr<Rcl::SearchData>(new Rcl::SearchData));
    m_source = std::shared_ptr<DocSequence>(src);


    // Start a native viewer if the mimetype has one defined, else a
    // preview.
    string apptag;
    doc.getmeta(Rcl::Doc::keyapptg, &apptag);
    string viewer = theconfig->getMimeViewerDef(doc.mimetype, apptag, 
						prefs.useDesktopOpen);
    if (viewer.empty()) {
	startPreview(doc);
    } else {
	hide();
	startNativeViewer(doc);
	// We have a problem here because xdg-open will exit
	// immediately after starting the command instead of waiting
	// for it, so we can't wait either and we don't know when we
	// can exit (deleting the temp file). As a bad workaround we
	// sleep some time then exit. The alternative would be to just
	// prevent the temp file deletion completely, leaving it
	// around forever. Better to let the user save a copy if he
	// wants I think.
	sleep(30);
	fileExit();
    }
}

/* Look for html browser. We make a special effort for html because it's
 * used for reading help. This is only used if the normal approach 
 * (xdg-open etc.) failed */
static bool lookForHtmlBrowser(string &exefile)
{
    vector<string> blist{"opera", "google-chrome", "chromium-browser",
            "palemoon", "iceweasel", "firefox", "konqueror", "epiphany"};

    const char *path = getenv("PATH");
    if (path == 0) {
	path = "/usr/local/bin:/usr/bin:/bin";
    }
    // Look for each browser 
    for (const auto& entry : blist) {
	if (ExecCmd::which(entry, exefile, path)) 
	    return true;
    }
    exefile.clear();
    return false;
}

void RclMain::openWith(Rcl::Doc doc, string cmdspec)
{
    LOGDEB("RclMain::openWith: " << cmdspec << "\n");

    // Split the command line
    vector<string> lcmd;
    if (!stringToStrings(cmdspec, lcmd)) {
	QMessageBox::warning(0, "Recoll", 
			     tr("Bad desktop app spec for %1: [%2]\n"
				"Please check the desktop file")
			     .arg(QString::fromUtf8(doc.mimetype.c_str()))
			     .arg(QString::fromLocal8Bit(cmdspec.c_str())));
	return;
    }

    // Look for the command to execute in the exec path and the filters 
    // directory
    string execname = lcmd.front();
    lcmd.erase(lcmd.begin());
    string url = doc.url;
    string fn = fileurltolocalpath(doc.url);

    // Try to keep the letters used more or less consistent with the reslist
    // paragraph format.
    map<string, string> subs;
#ifdef _WIN32
    path_backslashize(fn);
#endif
    subs["F"] = fn;
    subs["f"] = fn;
    subs["U"] = url_encode(url);
    subs["u"] = url;

    execViewer(subs, false, execname, lcmd, cmdspec, doc);
}

void RclMain::startNativeViewer(Rcl::Doc doc, int pagenum, QString term)
{
    string apptag;
    doc.getmeta(Rcl::Doc::keyapptg, &apptag);
    LOGDEB("RclMain::startNativeViewer: mtype [" << doc.mimetype <<
           "] apptag ["  << apptag << "] page "  << pagenum << " term ["  <<
           qs2utf8s(term) << "] url ["  << doc.url << "] ipath [" <<
           doc.ipath << "]\n");

    // Look for appropriate viewer
    string cmdplusattr = theconfig->getMimeViewerDef(doc.mimetype, apptag, 
						     prefs.useDesktopOpen);
    if (cmdplusattr.empty()) {
	QMessageBox::warning(0, "Recoll", 
			     tr("No external viewer configured for mime type [")
			     + doc.mimetype.c_str() + "]");
	return;
    }
    LOGDEB("StartNativeViewer: viewerdef from config: " << cmdplusattr << endl);

    // Separate command string and viewer attributes (if any)
    ConfSimple viewerattrs;
    string cmd;
    theconfig->valueSplitAttributes(cmdplusattr, cmd, viewerattrs);
    bool ignoreipath = false;
    int execwflags = 0;
    if (viewerattrs.get("ignoreipath", cmdplusattr))
        ignoreipath = stringToBool(cmdplusattr);
    if (viewerattrs.get("maximize", cmdplusattr)) {
        if (stringToBool(cmdplusattr)) {
            execwflags |= ExecCmd::EXF_MAXIMIZED;
        }
    }
    
    // Split the command line
    vector<string> lcmd;
    if (!stringToStrings(cmd, lcmd)) {
	QMessageBox::warning(0, "Recoll", 
			     tr("Bad viewer command line for %1: [%2]\n"
				"Please check the mimeview file")
			     .arg(QString::fromUtf8(doc.mimetype.c_str()))
			     .arg(QString::fromLocal8Bit(cmd.c_str())));
	return;
    }

    // Look for the command to execute in the exec path and the filters 
    // directory
    string execpath;
    if (!ExecCmd::which(lcmd.front(), execpath)) {
	execpath = theconfig->findFilter(lcmd.front());
	// findFilter returns its input param if the filter is not in
	// the normal places. As we already looked in the path, we
	// have no use for a simple command name here (as opposed to
	// mimehandler which will just let execvp do its thing). Erase
	// execpath so that the user dialog will be started further
	// down.
	if (!execpath.compare(lcmd.front())) 
	    execpath.erase();

	// Specialcase text/html because of the help browser need
	if (execpath.empty() && !doc.mimetype.compare("text/html") && 
	    apptag.empty()) {
	    if (lookForHtmlBrowser(execpath)) {
		lcmd.clear();
		lcmd.push_back(execpath);
		lcmd.push_back("%u");
	    }
	}
    }

    // Command not found: start the user dialog to help find another one:
    if (execpath.empty()) {
	QString mt = QString::fromUtf8(doc.mimetype.c_str());
	QString message = tr("The viewer specified in mimeview for %1: %2"
			     " is not found.\nDo you want to start the "
			     " preferences dialog ?")
	    .arg(mt).arg(QString::fromLocal8Bit(lcmd.front().c_str()));

	switch(QMessageBox::warning(0, "Recoll", message, 
				    "Yes", "No", 0, 0, 1)) {
	case 0: 
	    showUIPrefs();
	    if (uiprefs)
		uiprefs->showViewAction(mt);
	    break;
	case 1:
	    break;
	}
        // The user will have to click on the link again to try the
        // new command.
	return;
    }
    // Get rid of the command name. lcmd is now argv[1...n]
    lcmd.erase(lcmd.begin());

    // Process the command arguments to determine if we need to create
    // a temporary file.

    // If the command has a %i parameter it will manage the
    // un-embedding. Else if ipath is not empty, we need a temp file.
    // This can be overridden with the "ignoreipath" attribute
    bool groksipath = (cmd.find("%i") != string::npos) || ignoreipath;

    // We used to try being clever here, but actually, the only case
    // where we don't need a local file copy of the document (or
    // parent document) is the case of an HTML page with a non-file
    // URL (http or https). Trying to guess based on %u or %f is
    // doomed because we pass %u to xdg-open.
    bool wantsfile = false;
    bool wantsparentfile = cmd.find("%F") != string::npos;
    if (!wantsparentfile &&
        (cmd.find("%f") != string::npos || urlisfileurl(doc.url) ||
         doc.mimetype.compare("text/html"))) {
        wantsfile = true;
    } 

    if (wantsparentfile && !urlisfileurl(doc.url)) {
	QMessageBox::warning(0, "Recoll", 
			     tr("Viewer command line for %1 specifies "
				"parent file but URL is http[s]: unsupported")
			     .arg(QString::fromUtf8(doc.mimetype.c_str())));
	return;
    }
    if (wantsfile && wantsparentfile) {
	QMessageBox::warning(0, "Recoll", 
			     tr("Viewer command line for %1 specifies both "
				"file and parent file value: unsupported")
			     .arg(QString::fromUtf8(doc.mimetype.c_str())));
	return;
    }
	
    string url = doc.url;
    string fn = fileurltolocalpath(doc.url);
    Rcl::Doc pdoc;
    if (wantsparentfile) {
	// We want the path for the parent document. For example to
	// open the chm file, not the internal page. Note that we just
	// override the other file name in this case.
	if (!m_source || !m_source->getEnclosing(doc, pdoc)) {
	    QMessageBox::warning(0, "Recoll",
				 tr("Cannot find parent document"));
	    return;
	}
	// Override fn with the parent's : 
	fn = fileurltolocalpath(pdoc.url);

	// If the parent document has an ipath too, we need to create
	// a temp file even if the command takes an ipath
	// parameter. We have no viewer which could handle a double
	// embedding. Will have to change if such a one appears.
	if (!pdoc.ipath.empty()) {
	    groksipath = false;
	}
    }

    // Can't remember what enterHistory was actually for. Set it to
    // true always for now
    bool enterHistory = true;
    bool istempfile = false;
    
    LOGDEB("StartNativeViewer: groksipath " << groksipath << " wantsf " <<
           wantsfile << " wantsparentf " << wantsparentfile << "\n");

    // If the command wants a file but this is not a file url, or
    // there is an ipath that it won't understand, we need a temp file:
    theconfig->setKeyDir(fn.empty() ? "" : path_getfather(fn));
    if (((wantsfile || wantsparentfile) && fn.empty()) ||
	(!groksipath && !doc.ipath.empty()) ) {
	TempFile temp;
	Rcl::Doc& thedoc = wantsparentfile ? pdoc : doc;
	if (!FileInterner::idocToFile(temp, string(), theconfig, thedoc)) {
	    QMessageBox::warning(0, "Recoll",
				 tr("Cannot extract document or create "
				    "temporary file"));
	    return;
	}
	enterHistory = true;
        istempfile = true;
	rememberTempFile(temp);
	fn = temp.filename();
	url = path_pathtofileurl(fn);
    }

    // If using an actual file, check that it exists, and if it is
    // compressed, we may need an uncompressed version
    if (!fn.empty() && theconfig->mimeViewerNeedsUncomp(doc.mimetype)) {
        if (!path_readable(fn)) {
            QMessageBox::warning(0, "Recoll", 
                                 tr("Can't access file: ") + u8s2qs(fn));
            return;
        }
        TempFile temp;
        if (FileInterner::isCompressed(fn, theconfig)) {
            if (!FileInterner::maybeUncompressToTemp(temp, fn, theconfig,  
                                                     doc)) {
                QMessageBox::warning(0, "Recoll", 
                                     tr("Can't uncompress file: ") + 
                                     QString::fromLocal8Bit(fn.c_str()));
                return;
            }
        }
        if (temp.ok()) {
            istempfile = true;
	    rememberTempFile(temp);
            fn = temp.filename();
            url = path_pathtofileurl(fn);
        }
    }

    if (istempfile) {
        QxtConfirmationMessage confirm(
            QMessageBox::Warning,
            "Recoll",
            tr("Opening a temporary copy. Edits will be lost if you don't save"
               "<br/>them to a permanent location."),
            tr("Do not show this warning next time (use GUI preferences "
               "to restore)."));
        confirm.setSettingsPath("Recoll/prefs");
        confirm.setOverrideSettingsKey("showTempFileWarning");
        confirm.exec();
        // Pita: need to keep the prefs struct in sync, else the value
        // will be clobbered on program exit.
        QSettings settings("Recoll.org", "recoll");
        prefs.showTempFileWarning =
            settings.value("Recoll/prefs/showTempFileWarning").toInt();
    }

    // If we are not called with a page number (which would happen for a call
    // from the snippets window), see if we can compute a page number anyway.
    if (pagenum == -1) {
	pagenum = 1;
	string lterm;
	if (m_source)
	    pagenum = m_source->getFirstMatchPage(doc, lterm);
	if (pagenum == -1)
	    pagenum = 1;
	else // We get the match term used to compute the page
	    term = QString::fromUtf8(lterm.c_str());
    }
    char cpagenum[20];
    sprintf(cpagenum, "%d", pagenum);


    // Substitute %xx inside arguments
    string efftime;
    if (!doc.dmtime.empty() || !doc.fmtime.empty()) {
        efftime = doc.dmtime.empty() ? doc.fmtime : doc.dmtime;
    } else {
        efftime = "0";
    }
    // Try to keep the letters used more or less consistent with the reslist
    // paragraph format.
    map<string, string> subs;
    subs["D"] = efftime;
#ifdef _WIN32
    path_backslashize(fn);
#endif
    subs["f"] = fn;
    subs["F"] = fn;
    subs["i"] = FileInterner::getLastIpathElt(doc.ipath);
    subs["M"] = doc.mimetype;
    subs["p"] = cpagenum;
    subs["s"] = (const char*)term.toLocal8Bit();
    subs["U"] = url_encode(url);
    subs["u"] = url;
    // Let %(xx) access all metadata.
    for (const auto& ent :doc.meta) {
        subs[ent.first] = ent.second;
    }
    execViewer(subs, enterHistory, execpath, lcmd, cmd, doc, execwflags);
}

void RclMain::execViewer(const map<string, string>& subs, bool enterHistory,
                         const string& execpath,
                         const vector<string>& _lcmd, const string& cmd,
                         Rcl::Doc doc, int flags)
{
    string ncmd;
    vector<string> lcmd;
    for (vector<string>::const_iterator it = _lcmd.begin(); 
         it != _lcmd.end(); it++) {
        pcSubst(*it, ncmd, subs);
        LOGDEB(""  << *it << "->"  << (ncmd) << "\n" );
        lcmd.push_back(ncmd);
    }

    // Also substitute inside the unsplitted command line and display
    // in status bar
    pcSubst(cmd, ncmd, subs);
#ifndef _WIN32
    ncmd += " &";
#endif
    QStatusBar *stb = statusBar();
    if (stb) {
	string prcmd;
#ifdef _WIN32
        prcmd = ncmd;
#else
	string fcharset = theconfig->getDefCharset(true);
	transcode(ncmd, prcmd, fcharset, "UTF-8");
#endif
	QString msg = tr("Executing: [") + 
	    QString::fromUtf8(prcmd.c_str()) + "]";
	stb->showMessage(msg, 10000);
    }

    if (enterHistory)
	historyEnterDoc(rcldb.get(), g_dynconf, doc);
    
    // Do the zeitgeist thing
    zg_send_event(ZGSEND_OPEN, doc);

    // We keep pushing back and never deleting. This can't be good...
    ExecCmd *ecmd = new ExecCmd(ExecCmd::EXF_SHOWWINDOW | flags);
    m_viewers.push_back(ecmd);
    ecmd->startExec(execpath, lcmd, false, false);
}

void RclMain::startManual()
{
    startManual(string());
}

void RclMain::startManual(const string& index)
{
    string docdir = path_cat(theconfig->getDatadir(), "doc");

    // The single page user manual is nicer if we have an index. Else
    // the webhelp one is nicer if it is present
    string usermanual = path_cat(docdir, "usermanual.html");
    string webhelp = path_cat(docdir, "webhelp");
    webhelp = path_cat(webhelp, "index.html");
    bool has_wh = path_exists(webhelp);
    
    LOGDEB("RclMain::startManual: help index is " <<
           (index.empty() ? "(null)" : index) << "\n");
    bool indexempty = index.empty();

#ifdef _WIN32
    // On Windows I could not find any way to pass the fragment through
    // rclstartw (tried to set text/html as exception with rclstartw %u).
    // So always start the webhelp
    indexempty = true;
#endif
    
    if (!indexempty) {
	usermanual += "#";
	usermanual += index;
    }
    Rcl::Doc doc;
    if (has_wh && indexempty) {
        doc.url = path_pathtofileurl(webhelp);
    } else {
        doc.url = path_pathtofileurl(usermanual);
    }
    doc.mimetype = "text/html";
    doc.addmeta(Rcl::Doc::keyapptg, "rclman");
    startNativeViewer(doc);
}
