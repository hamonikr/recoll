/* Copyright (C) 2008 J.F.Dockes
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

/*
 * A lot of code in this file was copied from kio_beagle 0.4.0,
 * which is a GPL program. The authors listed are:
 * Debajyoti Bera <dbera.web@gmail.com>
 *
 * KDE4 port:
 * Stephan Binner <binner@kde.org>
 */

#include "autoconfig.h"

// Couldn't get listDir() to work with kde 4.0, konqueror keeps
// crashing because of kdirmodel, couldn't find a workaround (not
// saying it's impossible)...

#include <sys/stat.h>

#include <QDebug>
#include <QUrl>
#include <QStandardPaths>

#include "kio_recoll.h"
#include "pathut.h"

using namespace KIO;

static const QString resultBaseName("recollResult");

// Check if the input URL is of the form that konqueror builds by
// appending one of our result file names to the directory name (which
// is the search string). If it is, extract and return the result
// document number. Possibly restart the search if the search string
// does not match the current one
bool RecollProtocol::isRecollResult(const QUrl& url, int *num, QString *q)
{
    *num = -1;
    qDebug() << "RecollProtocol::isRecollResult: url: " << url;

    // Basic checks
    if (!url.host().isEmpty() || url.path().isEmpty() ||
            (url.scheme().compare("recoll") && url.scheme().compare("recollf"))) {
        qDebug() << "RecollProtocol::isRecollResult: no: url.host " <<
                 url.host() << " path " << url.path() << " scheme " << url.scheme();
        return false;
    }

    QString path = url.path();
    qDebug() << "RecollProtocol::isRecollResult: path: " << path;
    if (!path.startsWith("/")) {
        return false;
    }

    // Look for the last '/' and check if it is followed by
    // resultBaseName (riiiight...)
    int slashpos = path.lastIndexOf("/");
    if (slashpos == -1 || slashpos == 0 || slashpos == path.length() - 1) {
        return false;
    }
    slashpos++;
    //qDebug() << "Comparing " << path.mid(slashpos, resultBaseName.length()) <<
    //  "and " << resultBaseName;
    if (path.mid(slashpos, resultBaseName.length()).compare(resultBaseName)) {
        return false;
    }

    // Extract the result number
    QString snum = path.mid(slashpos + resultBaseName.length());
    sscanf(snum.toUtf8(), "%d", num);
    if (*num == -1) {
        return false;
    }

    //qDebug() << "URL analysis ok, num:" << *num;

    // We do have something that ressembles a recoll result locator. Check if
    // this matches the current search, else have to run the requested one
    *q = path.mid(1, slashpos - 2);
    return true;
}

// Translate rcldoc result into directory entry
static const UDSEntry resultToUDSEntry(const Rcl::Doc& doc, int num)
{
    UDSEntry entry;

    QUrl url(doc.url.c_str());
    //qDebug() << doc.url.c_str();

    /// Filename - as displayed in directory listings etc.
    /// "." has the usual special meaning of "current directory"
    /// UDS_NAME must always be set and never be empty, neither contain '/'.
    ///
    /// Note that KIO will append the UDS_NAME to the url of their
    /// parent directory, so all kioslaves must use that naming scheme
    /// ("url_of_parent/filename" will be the full url of that file).
    /// To customize the appearance of files without changing the url
    /// of the items, use UDS_DISPLAY_NAME.  
    //
    // Use the result number to designate the file in case we are
    // asked to access it
    char cnum[30];
    sprintf(cnum, "%04d", num);
    entry.insert(KIO::UDSEntry::UDS_NAME, resultBaseName + cnum);

    // Display the real file name
    entry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, url.fileName());

    /// A local file path if the ioslave display files sitting on the
    /// local filesystem (but in another hierarchy, e.g. settings:/ or
    /// remote:/)
    entry.insert(KIO::UDSEntry::UDS_LOCAL_PATH, url.path());

    /// This file is a shortcut or mount, pointing to an
    /// URL in a different hierarchy
    /// @since 4.1
    // We should probably set this only if the scheme is not 'file' (e.g.
    // from the web cache).
    entry.insert(KIO::UDSEntry::UDS_TARGET_URL, doc.url.c_str());

    if (!doc.mimetype.compare("application/x-fsdirectory") ||
            !doc.mimetype.compare("inode/directory")) {
        entry.insert(KIO::UDSEntry::UDS_MIME_TYPE, "inode/directory");
        entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    } else {
        entry.insert(KIO::UDSEntry::UDS_MIME_TYPE, doc.mimetype.c_str());
        entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
    }

    // For local files, supply the usual file stat information
    struct stat info;
    if (lstat(url.path().toUtf8(), &info) >= 0) {
        entry.insert(KIO::UDSEntry::UDS_SIZE, info.st_size);
        entry.insert(KIO::UDSEntry::UDS_ACCESS, info.st_mode);
        entry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, info.st_mtime);
        entry.insert(KIO::UDSEntry::UDS_ACCESS_TIME, info.st_atime);
        entry.insert(KIO::UDSEntry::UDS_CREATION_TIME, info.st_ctime);
    }

    return entry;
}


// From kio_beagle
static void createRootEntry(KIO::UDSEntry& entry)
{
    entry.clear();
    entry.insert(KIO::UDSEntry::UDS_NAME, ".");
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.insert(KIO::UDSEntry::UDS_ACCESS, 0700);
    entry.insert(KIO::UDSEntry::UDS_MIME_TYPE, "inode/directory");
}

// Points to html query screen
static void createGoHomeEntry(KIO::UDSEntry& entry)
{
    entry.clear();
    entry.insert(KIO::UDSEntry::UDS_NAME, "search.html");
    entry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, "Recoll search (click me)");
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
    entry.insert(KIO::UDSEntry::UDS_TARGET_URL, "recoll:///search.html");
    entry.insert(KIO::UDSEntry::UDS_ACCESS, 0500);
    entry.insert(KIO::UDSEntry::UDS_MIME_TYPE, "text/html");
    entry.insert(KIO::UDSEntry::UDS_ICON_NAME, "recoll");
}

// Points to help file
static void createGoHelpEntry(KIO::UDSEntry& entry)
{
    QString location =
        QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                               "kio_recoll/help.html");
    entry.clear();
    entry.insert(KIO::UDSEntry::UDS_NAME, "help");
    entry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, "Recoll help (click me first)");
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
    entry.insert(KIO::UDSEntry::UDS_TARGET_URL, QString("file://") +
                 location);
    entry.insert(KIO::UDSEntry::UDS_ACCESS, 0500);
    entry.insert(KIO::UDSEntry::UDS_MIME_TYPE, "text/html");
    entry.insert(KIO::UDSEntry::UDS_ICON_NAME, "help");
}

// As far as I can see we only ever get this on '/' so why all the code?
void RecollProtocol::stat(const QUrl& url)
{
    qDebug() << "RecollProtocol::stat:" << url;

    UrlIngester ingest(this, url);

    KIO::UDSEntry entry;
//    entry.insert(KIO::UDSEntry::UDS_TARGET_URL, url.url());
//    entry.insert(KIO::UDSEntry::UDS_URL, url.url());
    UrlIngester::RootEntryType rettp;
    QueryDesc qd;
    int num;
    if (ingest.isRootEntry(&rettp)) {
        qDebug() << "RecollProtocol::stat: root entry";
        switch (rettp) {
        case UrlIngester::UIRET_ROOT:
            qDebug() << "RecollProtocol::stat: root";
            createRootEntry(entry);
            break;
        case UrlIngester::UIRET_HELP:
            qDebug() << "RecollProtocol::stat: root help";
            createGoHelpEntry(entry);
            break;
        case UrlIngester::UIRET_SEARCH:
            qDebug() << "RecollProtocol::stat: root search";
            createGoHomeEntry(entry);
            break;
        default:
            qDebug() << "RecollProtocol::stat: ??";
            error(ERR_DOES_NOT_EXIST, QString());
            break;
        }
    } else if (ingest.isResult(&qd, &num)) {
        qDebug() << "RecollProtocol::stat: isresult";
        if (syncSearch(qd)) {
            Rcl::Doc doc;
            if (num >= 0 && m_source && m_source->getDoc(num, doc)) {
                entry = resultToUDSEntry(doc, num);
            } else {
                error(ERR_DOES_NOT_EXIST, QString());
            }
        } else {
            // hopefully syncSearch() set the error?
        }
    } else if (ingest.isQuery(&qd)) {
        qDebug() << "RecollProtocol::stat: isquery";
        // ie "recoll:/some string" or "recoll:/some string/"
        //
        // We have a problem here. We'd like to let the user enter
        // either form and get an html or a dir contents result,
        // depending on the ending /. Otoh this makes the name space
        // inconsistent, because /toto can't be a file (the html
        // result page) while /toto/ would be a directory ? or can it
        //
        // Another approach would be to use different protocol names
        // to avoid any possibility of mixups
        if (m_alwaysdir || ingest.alwaysDir() || ingest.endSlashQuery()) {
            qDebug() << "RecollProtocol::stat: Directory type:";
            // Need to check no / in there
            entry.insert(KIO::UDSEntry::UDS_NAME, qd.query);
            entry.insert(KIO::UDSEntry::UDS_ACCESS, 0700);
            entry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, time(0));
            entry.insert(KIO::UDSEntry::UDS_CREATION_TIME, time(0));
            entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
            entry.insert(KIO::UDSEntry::UDS_MIME_TYPE, "inode/directory");
        }
    } else {
        qDebug() << "RecollProtocol::stat: none of the above ??";
    }
    statEntry(entry);
    finished();
}

void RecollProtocol::listDir(const QUrl& url)
{
    qDebug() << "RecollProtocol::listDir: url: " << url;

    UrlIngester ingest(this, url);
    UrlIngester::RootEntryType rettp;
    QueryDesc qd;

    if (ingest.isRootEntry(&rettp)) {
        switch (rettp) {
        case UrlIngester::UIRET_ROOT: {
            qDebug() << "RecollProtocol::listDir:list /";
            UDSEntryList entries;
            KIO::UDSEntry entry;
            createRootEntry(entry);
            entries.append(entry);
            createGoHomeEntry(entry);
            entries.append(entry);
            createGoHelpEntry(entry);
            entries.append(entry);
            listEntries(entries);
            finished();
        }
        return;
        default:
            error(ERR_CANNOT_ENTER_DIRECTORY, QString());
            return;
        }
    } else if (ingest.isQuery(&qd)) {
        // At this point, it seems that when the request is from
        // konqueror autocompletion it comes with a / at the end,
        // which offers an opportunity to not perform it.
        if (ingest.endSlashQuery()) {
            qDebug() << "RecollProtocol::listDir: Ends With /";
            error(ERR_SLAVE_DEFINED,
                  QString::fromUtf8("Autocompletion search aborted"));
            return;
        }
        if (!syncSearch(qd)) {
            // syncSearch did the error thing
            return;
        }
        // Fallthrough to actually listing the directory
    } else {
        qDebug() << "RecollProtocol::listDir: Cant grok input url";
        error(ERR_CANNOT_ENTER_DIRECTORY, QString());
        return;
    }

    static int maxentries = -1;
    if (maxentries == -1) {
        if (o_rclconfig) {
            o_rclconfig->getConfParam("kio_max_direntries", &maxentries);
        }
        if (maxentries == -1) {
            maxentries = 10000;
        }
    }
    static const int pagesize = 200;
    int pagebase = 0;
    while (pagebase < maxentries) {
        vector<ResListEntry> page;
        int pagelen = m_source->getSeqSlice(pagebase, pagesize, page);
        UDSEntry entry;
        if (pagelen < 0) {
            error(ERR_SLAVE_DEFINED, QString::fromUtf8("Internal error"));
            break;
        }
        UDSEntryList entries;
        for (int i = 0; i < pagelen; i++) {
            entries.push_back(resultToUDSEntry(page[i].doc, i));
        }
        listEntries(entries);
        if (pagelen != pagesize) {
            break;
        }
        pagebase += pagelen;
    }
    finished();
}
