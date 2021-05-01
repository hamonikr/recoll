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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <string>

#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QUrlQuery>
#include <QStandardPaths>

#include "rclconfig.h"
#include "rcldb.h"
#include "rclinit.h"
#include "pathut.h"
#include "searchdata.h"
#include "rclquery.h"
#include "wasatorcl.h"
#include "kio_recoll.h"
#include "docseqdb.h"
#include "readfile.h"
#include "smallut.h"
#include "textsplit.h"
#include "guiutils.h"

using namespace KIO;
using namespace std;

RclConfig *RecollProtocol::o_rclconfig;

RecollProtocol::RecollProtocol(const QByteArray& pool, const QByteArray& app)
    : SlaveBase("recoll", pool, app), m_initok(false), m_alwaysdir(false)
{
    qDebug() << "RecollProtocol::RecollProtocol()";
    if (o_rclconfig == 0) {
        o_rclconfig = recollinit(0, 0, 0, m_reason);
        if (!o_rclconfig || !o_rclconfig->ok()) {
            m_reason = string("Configuration problem: ") + m_reason;
            return;
        }
    }
    if (o_rclconfig->getDbDir().empty()) {
        // Note: this will have to be replaced by a call to a
        // configuration building dialog for initial configuration? Or
        // do we assume that the QT GUO is always used for this ?
        m_reason = "No db directory in configuration ??";
        return;
    }
    rwSettings(false);

    m_rcldb = std::shared_ptr<Rcl::Db>(new Rcl::Db(o_rclconfig));
    if (!m_rcldb) {
        m_reason = "Could not build database object. (out of memory ?)";
        return;
    }

    // Decide if we allow switching between html and file manager
    // presentation by using an end slash or not. Can also be done dynamically
    // by switching proto names.
    const char *cp = getenv("RECOLL_KIO_ALWAYS_DIR");
    if (cp) {
        m_alwaysdir = stringToBool(cp);
    } else {
        o_rclconfig->getConfParam("kio_always_dir", &m_alwaysdir);
    }

    cp = getenv("RECOLL_KIO_STEMLANG");
    if (cp) {
        m_stemlang = cp;
    } else {
        m_stemlang = "english";
    }
    m_pager.setParent(this);
    m_initok = true;
    return;
}

// There should be an object counter somewhere to delete the config when done.
// Doesn't seem needed in the kio context.
RecollProtocol::~RecollProtocol()
{
    qDebug() << "RecollProtocol::~RecollProtocol()";
}

bool RecollProtocol::maybeOpenDb(string& reason)
{
    if (!m_rcldb) {
        reason = "Internal error: initialization error";
        return false;
    }
    if (!m_rcldb->isopen() && !m_rcldb->open(Rcl::Db::DbRO)) {
        reason = "Could not open database in " + o_rclconfig->getDbDir();
        return false;
    }
    return true;
}

// This is never called afaik
void RecollProtocol::mimetype(const QUrl& url)
{
    qDebug() << "RecollProtocol::mimetype: url: " << url;
    mimeType("text/html");
    finished();
}

UrlIngester::UrlIngester(RecollProtocol *p, const QUrl& url)
    : m_parent(p), m_slashend(false), m_alwaysdir(false),
      m_retType(UIRET_NONE), m_resnum(0), m_type(UIMT_NONE)
{
    qDebug() << "UrlIngester::UrlIngester: Url: " << url;
    m_alwaysdir = !url.scheme().compare("recollf");
    QString path = url.path();
    if (url.host().isEmpty()) {
        if (path.isEmpty() || !path.compare("/")) {
            m_type = UIMT_ROOTENTRY;
            m_retType = UIRET_ROOT;
            return;
        } else if (!path.compare("/help.html")) {
            m_type = UIMT_ROOTENTRY;
            m_retType = UIRET_HELP;
            return;
        } else if (!path.compare("/search.html")) {
            m_type = UIMT_ROOTENTRY;
            m_retType = UIRET_SEARCH;
            QUrlQuery q(url);
            // Retrieve the query value for preloading the form
            m_query.query = q.queryItemValue("q");
            return;
        } else if (m_parent->isRecollResult(url, &m_resnum, &m_query.query)) {
            m_type = UIMT_QUERYRESULT;
            m_query.opt = "l";
            m_query.page = 0;
        } else {
            // Have to think this is some search string
            m_type = UIMT_QUERY;
            m_query.query = url.path();
            m_query.opt = "l";
            m_query.page = 0;
        }
    } else {
        // Non empty host, url must be something like :
        //      //search/query?q=query&param=value...
        qDebug() << "UrlIngester::UrlIngester: host " <<
                 url.host() << " path " << url.path();
        if (url.host().compare("search") || url.path().compare("/query")) {
            return;
        }
        m_type = UIMT_QUERY;
        // Decode the forms' arguments
        // Retrieve the query value for preloading the form
        QUrlQuery q(url);
        m_query.query = q.queryItemValue("q");

        m_query.opt = q.queryItemValue("qtp");
        if (m_query.opt.isEmpty()) {
            m_query.opt = "l";
        }
        QString p = q.queryItemValue("p");
        if (p.isEmpty()) {
            m_query.page = 0;
        } else {
            sscanf(p.toUtf8(), "%d", &m_query.page);
        }
        p = q.queryItemValue("det");
        m_query.isDetReq = !p.isEmpty();

        p = q.queryItemValue("cmd");
        if (!p.isEmpty() && !p.compare("pv")) {
            p = q.queryItemValue("dn");
            if (!p.isEmpty()) {
                // Preview and no docnum ??
                m_resnum = atoi((const char *)p.toUtf8());
                // Result in page is 1+
                m_resnum--;
                m_type = UIMT_PREVIEW;
            }
        }
    }
    if (m_query.query.startsWith("/")) {
        m_query.query.remove(0, 1);
    }
    if (m_query.query.endsWith("/")) {
        qDebug() << "UrlIngester::UrlIngester: query Ends with /";
        m_slashend = true;
        m_query.query.chop(1);
    } else {
        m_slashend = false;
    }
    return;
}

bool RecollProtocol::syncSearch(const QueryDesc& qd)
{
    qDebug() << "RecollProtocol::syncSearch";
    if (!m_initok || !maybeOpenDb(m_reason)) {
        string reason = "RecollProtocol::listDir: Init error:" + m_reason;
        error(KIO::ERR_SLAVE_DEFINED, u8s2qs(reason));
        return false;
    }
    if (qd.sameQuery(m_query)) {
        return true;
    }
    // doSearch() calls error() if appropriate.
    return doSearch(qd);
}

// This is used by the html interface, but also by the directory one
// when doing file copies for exemple. This is the central dispatcher
// for requests, it has to know a little about both models.
void RecollProtocol::get(const QUrl& url)
{
    qDebug() << "RecollProtocol::get: " << url;

    if (!m_initok || !maybeOpenDb(m_reason)) {
        string reason = "Recoll: init error: " + m_reason;
        error(KIO::ERR_SLAVE_DEFINED, u8s2qs(reason));
        return;
    }

    UrlIngester ingest(this, url);
    UrlIngester::RootEntryType rettp;
    QueryDesc qd;
    int resnum;
    if (ingest.isRootEntry(&rettp)) {
        switch (rettp) {
        case UrlIngester::UIRET_HELP: {
            QString location =
                QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                       "kio_recoll/help.html");
            redirection(QUrl::fromLocalFile(location));
        }
        goto out;
        default:
            searchPage();
            goto out;
        }
    } else if (ingest.isResult(&qd, &resnum)) {
        // Url matched one generated by konqueror/Dolphin out of a
        // search directory listing: ie:
        // recoll:/some search string/recollResultxx
        //
        // This happens when the user drags/drop the result to another
        // app, or with the "open-with" right-click. Does not happen
        // if the entry itself is clicked (the UDS_URL is apparently
        // used in this case
        //
        // Redirect to the result document URL
        if (!syncSearch(qd)) {
            return;
        }
        Rcl::Doc doc;
        if (resnum >= 0 && m_source && m_source->getDoc(resnum, doc)) {
            mimeType(doc.mimetype.c_str());
            redirection(QUrl::fromLocalFile((const char *)(doc.url.c_str() + 7)));
            goto out;
        }
    } else if (ingest.isPreview(&qd, &resnum)) {
        if (!syncSearch(qd)) {
            return;
        }
        Rcl::Doc doc;
        if (resnum >= 0 && m_source && m_source->getDoc(resnum, doc)) {
            showPreview(doc);
            goto out;
        }
    } else if (ingest.isQuery(&qd)) {
#if 0
// Do we need this ?
        if (host.isEmpty()) {
            char cpage[20];
            sprintf(cpage, "%d", page);
            QString nurl = QString::fromAscii("recoll://search/query?q=") +
                           query + "&qtp=" + opt + "&p=" + cpage;
            redirection(QUrl(nurl));
            goto out;
        }
#endif
        // htmlDoSearch does the search syncing (needs to know about changes).
        htmlDoSearch(qd);
        goto out;
    }

    error(KIO::ERR_SLAVE_DEFINED, u8s2qs("Unrecognized URL or internal error"));
out:
    finished();
}

// Execute Recoll search, and set the docsource
bool RecollProtocol::doSearch(const QueryDesc& qd)
{
    qDebug() << "RecollProtocol::doSearch:query" << qd.query << "opt" << qd.opt;
    m_query = qd;

    char opt = qd.opt.isEmpty() ? 'l' : qd.opt.toUtf8().at(0);
    string qs = (const char *)qd.query.toUtf8();
    Rcl::SearchData *sd = 0;
    if (opt != 'l') {
        Rcl::SearchDataClause *clp = 0;
        if (opt == 'f') {
            clp = new Rcl::SearchDataClauseFilename(qs);
        } else {
            clp = new Rcl::SearchDataClauseSimple(opt == 'o' ? Rcl::SCLT_OR :
                                                  Rcl::SCLT_AND, qs);
        }
        sd = new Rcl::SearchData(Rcl::SCLT_OR, m_stemlang);
        if (sd && clp) {
            sd->addClause(clp);
        }
    } else {
        sd = wasaStringToRcl(o_rclconfig, m_stemlang, qs, m_reason);
    }
    if (!sd) {
        m_reason = "Internal Error: cant build search";
        error(KIO::ERR_SLAVE_DEFINED, u8s2qs(m_reason));
        return false;
    }

    std::shared_ptr<Rcl::SearchData> sdata(sd);
    std::shared_ptr<Rcl::Query>query(new Rcl::Query(m_rcldb.get()));
    query->setCollapseDuplicates(prefs.collapseDuplicates);
    if (!query->setQuery(sdata)) {
        m_reason = "Query execute failed. Invalid query or syntax error?";
        error(KIO::ERR_SLAVE_DEFINED, u8s2qs(m_reason));
        return false;
    }

    DocSequenceDb *src =
        new DocSequenceDb(m_rcldb, std::shared_ptr<Rcl::Query>(query),
                          "Query results", sdata);
    if (src == 0) {
        error(KIO::ERR_SLAVE_DEFINED, u8s2qs("Can't build result sequence"));
        return false;
    }
    m_source = std::shared_ptr<DocSequence>(src);
    // Reset pager in all cases. Costs nothing, stays at page -1 initially
    // htmldosearch will fetch the first page if needed.
    m_pager.setDocSource(m_source);
    return true;
}

int kdemain(int argc, char **argv)
{
    QCoreApplication::setApplicationName("kio_recoll");
    qDebug() << "*** starting kio_recoll ";

    if (argc != 4)  {
        qDebug() << "Usage: kio_recoll proto dom-socket1 dom-socket2\n";
        exit(-1);
    }

    RecollProtocol slave(argv[2], argv[3]);
    slave.dispatchLoop();

    qDebug() << "kio_recoll Done";
    return 0;
}
