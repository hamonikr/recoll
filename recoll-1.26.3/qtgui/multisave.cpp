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

#include <string>
#include <set>
#include <sstream>
using namespace std;

#include <QWidget>
#include <QFileDialog>
#include <QMessageBox>

#include "recoll.h"
#include "multisave.h"
#include "smallut.h"
#include "log.h"
#include "pathut.h"
#include "internfile.h"

const unsigned int maxlen = 200;

void multiSave(QWidget *p, vector<Rcl::Doc>& docs)
{
    QFileDialog fdialog(p, QWidget::tr("Create or choose save directory"));
    fdialog.setAcceptMode(QFileDialog::AcceptSave);
    fdialog.setFileMode(QFileDialog::Directory);
    fdialog.setOption(QFileDialog::ShowDirsOnly);
    if (fdialog.exec() == 0) 
	return;
    QStringList dirl = fdialog.selectedFiles();
    if (dirl.size() != 1) {
	// Can't happen ?
	QMessageBox::warning(0, "Recoll",
			     QWidget::tr("Choose exactly one directory"));
	return;
    }
    string dir((const char *)dirl[0].toLocal8Bit());
    LOGDEB2("multiSave: got dir "  << (dir) << "\n" );

    /* Save doc to files in target directory. Issues:
       - It is quite common to have docs in the array with the same
         file names, e.g. all messages in a folder have the same file
         name (the folder's).
       - There is no warranty that the ipath is going to be acceptable
         as a file name or interesting at all. We don't use it. 
       - We have to make sure the names don't end up too long.

       If collisions occur, we add a numeric infix (e.g. somefile.23.pdf).

       We never overwrite existing files and don't give the user an
       option to do it (they can just as well save to an empty
       directory and use the file manager to accomplish whatever they
       want).

       We don't try hard to protect against race-conditions
       though. The existing file names are read before beginning the
       save sequence, and collisions appearing after this are handled
       by aborting. There is a window between existence check and creation
       because idoctofile does not use O_EXCL
    */
    set<string> existingNames;
    string reason;
    if (!readdir(dir, reason, existingNames)) {
	QMessageBox::warning(0, "Recoll",
			     QWidget::tr("Could not read directory: ") +
			     QString::fromLocal8Bit(reason.c_str()));
	return;
    }

    set<string> toBeCreated;
    vector<string> filenames;
    for (vector<Rcl::Doc>::iterator it = docs.begin(); it != docs.end(); it++) {
	string utf8fn;
	it->getmeta(Rcl::Doc::keyfn, &utf8fn);
	string suffix = path_suffix(utf8fn);
	LOGDEB("Multisave: ["  << (utf8fn) << "] suff ["  << (suffix) << "]\n" );
	if (suffix.empty() || suffix.size() > 10) {
	    suffix = theconfig->getSuffixFromMimeType(it->mimetype);
	    LOGDEB("Multisave: suff from config ["  << (suffix) << "]\n" );
	}
	string simple = path_basename(utf8fn, string(".") + suffix);
	LOGDEB("Multisave: simple ["  << (simple) << "]\n" );
	if (simple.empty())
	    simple = "rclsave";
	if (simple.size() > maxlen) {
	    simple = simple.substr(0, maxlen);
	}
	for  (int vers = 0; ; vers++) {
	    ostringstream ss;
	    ss << simple;
	    if (vers)
		ss << "." << vers;
	    if (!suffix.empty()) 
		ss << "." << suffix;

	    string fn = 
		(const char *)QString::fromUtf8(ss.str().c_str()).toLocal8Bit();
	    if (existingNames.find(fn) == existingNames.end() &&
		toBeCreated.find(fn) == toBeCreated.end()) {
		toBeCreated.insert(fn);
		filenames.push_back(fn);
		break;
	    }
	}
    }
    
    for (unsigned int i = 0; i != docs.size(); i++) {
	string fn = path_cat(dir, filenames[i]);
	if (path_exists(fn)) {
	    QMessageBox::warning(0, "Recoll",
				 QWidget::tr("Unexpected file name collision, "
				       "cancelling."));
	    return;
	}
	// There is still a race condition here, should we care ?
	TempFile temp;// not used
	if (!FileInterner::idocToFile(temp, fn, theconfig, docs[i], false)) {
	    QMessageBox::warning(0, "Recoll",
				 QWidget::tr("Cannot extract document: ") +
				 QString::fromLocal8Bit(docs[i].url.c_str()) +
				 " | " +
				 QString::fromLocal8Bit(docs[i].ipath.c_str())
		);
	}
    }
}

