/* Copyright (C) 2007-2019 J.F.Dockes
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
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

#include <sstream>
#include <iostream>
#include <list>
using std::ostringstream;
using std::endl;
using std::list;

#include "cstr.h"
#include "reslistpager.h"
#include "log.h"
#include "rclconfig.h"
#include "smallut.h"
#include "rclutil.h"
#include "plaintorich.h"
#include "mimehandler.h"
#include "transcode.h"

// Default highlighter. No need for locking, this is query-only.
static const string cstr_hlfontcolor("<span style='color: blue;'>");
static const string cstr_hlendfont("</span>");
class PlainToRichHtReslist : public PlainToRich {
public:
    virtual string startMatch(unsigned int) {
        return cstr_hlfontcolor;
    }
    virtual string endMatch() {
        return cstr_hlendfont;
    }
};
static PlainToRichHtReslist g_hiliter;

ResListPager::ResListPager(int pagesize) 
    : m_pagesize(pagesize),
      m_newpagesize(pagesize),
      m_resultsInCurrentPage(0),
      m_winfirst(-1),
      m_hasNext(true),
      m_hiliter(&g_hiliter)
{
}

void ResListPager::resultPageNext()
{
    if (!m_docSource) {
        LOGDEB("ResListPager::resultPageNext: null source\n");
        return;
    }

    int resCnt = m_docSource->getResCnt();
    LOGDEB("ResListPager::resultPageNext: rescnt " << resCnt <<
           ", winfirst " << m_winfirst << "\n");

    if (m_winfirst < 0) {
        m_winfirst = 0;
    } else {
        m_winfirst += int(m_respage.size());
    }
    // Get the next page of results. Note that we look ahead by one to
    // determine if there is actually a next page
    vector<ResListEntry> npage;
    int pagelen = m_docSource->getSeqSlice(m_winfirst, m_pagesize + 1, npage);

    // If page was truncated, there is no next
    m_hasNext = (pagelen == m_pagesize + 1);

    // Get rid of the possible excess result
    if (pagelen == m_pagesize + 1) {
        npage.resize(m_pagesize);
        pagelen--;
    }

    if (pagelen <= 0) {
        // No results ? This can only happen on the first page or if the
        // actual result list size is a multiple of the page pref (else
        // there would have been no Next on the last page)
        if (m_winfirst > 0) {
            // Have already results. Let them show, just disable the
            // Next button. We'd need to remove the Next link from the page
            // too.
            // Restore the m_winfirst value, let the current result vector alone
            m_winfirst -= int(m_respage.size());
        } else {
            // No results at all (on first page)
            m_winfirst = -1;
        }
        return;
    }
    m_resultsInCurrentPage = pagelen;
    m_respage = npage;
}
static string maybeEscapeHtml(const string& fld)
{
    if (fld.compare(0, cstr_fldhtm.size(), cstr_fldhtm))
        return escapeHtml(fld);
    else
        return fld.substr(cstr_fldhtm.size());
}


void ResListPager::resultPageFor(int docnum)
{
    if (!m_docSource) {
        LOGDEB("ResListPager::resultPageFor: null source\n");
        return;
    }

    int resCnt = m_docSource->getResCnt();
    LOGDEB("ResListPager::resultPageFor(" << docnum << "): rescnt " <<
           resCnt << ", winfirst " << m_winfirst << "\n");
    m_winfirst = (docnum / m_pagesize) * m_pagesize;

    // Get the next page of results.
    vector<ResListEntry> npage;
    int pagelen = m_docSource->getSeqSlice(m_winfirst, m_pagesize, npage);

    // If page was truncated, there is no next
    m_hasNext = (pagelen == m_pagesize);

    if (pagelen <= 0) {
        m_winfirst = -1;
        return;
    }
    m_respage = npage;
}

void ResListPager::displayDoc(RclConfig *config, int i, Rcl::Doc& doc, 
                              const HighlightData& hdata, const string& sh)
{
    ostringstream chunk;

    // Determine icon to display if any
    string iconurl = iconUrl(config, doc);
    
    // Printable url: either utf-8 if transcoding succeeds, or url-encoded
    string url;
    printableUrl(config->getDefCharset(), doc.url, url);

    // Same as url, but with file:// possibly stripped. output by %u instead
    // of %U. 
    string urlOrLocal;
    urlOrLocal = fileurltolocalpath(url);
    if (urlOrLocal.empty())
        urlOrLocal = url;

    // Make title out of file name if none yet
    string titleOrFilename;
    string utf8fn;
    doc.getmeta(Rcl::Doc::keytt, &titleOrFilename);
    doc.getmeta(Rcl::Doc::keyfn, &utf8fn);
    if (utf8fn.empty()) {
        utf8fn = path_getsimple(url);   
    }
    if (titleOrFilename.empty()) {
        titleOrFilename = utf8fn;
    }

    // Url for the parent directory. We strip the file:// part for local
    // paths
    string parenturl = url_parentfolder(url);
    {
        string localpath = fileurltolocalpath(parenturl);
        if (!localpath.empty())
            parenturl = localpath;
    }

    // Result number
    char numbuf[20];
    int docnumforlinks = m_winfirst + 1 + i;
    sprintf(numbuf, "%d", docnumforlinks);

    // Document date: either doc or file modification times
    string datebuf;
    if (!doc.dmtime.empty() || !doc.fmtime.empty()) {
        char cdate[100];
        cdate[0] = 0;
        time_t mtime = doc.dmtime.empty() ?
            atoll(doc.fmtime.c_str()) : atoll(doc.dmtime.c_str());
        struct tm *tm = localtime(&mtime);
        strftime(cdate, 99, dateFormat().c_str(), tm);
        transcode(cdate, datebuf, RclConfig::getLocaleCharset(), "UTF-8");
    }

    // Size information. We print both doc and file if they differ a lot
    int64_t fsize = -1, dsize = -1;
    if (!doc.dbytes.empty())
        dsize = static_cast<int64_t>(atoll(doc.dbytes.c_str()));
    if (!doc.fbytes.empty())
        fsize =  static_cast<int64_t>(atoll(doc.fbytes.c_str()));
    string sizebuf;
    if (dsize > 0) {
        sizebuf = displayableBytes(dsize);
        if (fsize > 10 * dsize && fsize - dsize > 1000)
            sizebuf += string(" / ") + displayableBytes(fsize);
    } else if (fsize >= 0) {
        sizebuf = displayableBytes(fsize);
    }

    string richabst;
    bool needabstract = parFormat().find("%A") != string::npos;
    if (needabstract && m_docSource) {
        vector<string> vabs;
        m_docSource->getAbstract(doc, vabs);
        m_hiliter->set_inputhtml(false);

        for (vector<string>::const_iterator it = vabs.begin();
             it != vabs.end(); it++) {
            if (!it->empty()) {
                // No need to call escapeHtml(), plaintorich handles it
                list<string> lr;
                // There may be data like page numbers before the snippet text.
                // will be in brackets.
                string::size_type bckt = it->find("]");
                if (bckt == string::npos) {
                    m_hiliter->plaintorich(*it, lr, hdata);
                } else {
                    m_hiliter->plaintorich(it->substr(bckt), lr, hdata);
                    lr.front() = it->substr(0, bckt) + lr.front();
                }
                richabst += lr.front();
                richabst += absSep();
            }
        }
    }

    // Links; Uses utilities from mimehandler.h
    ostringstream linksbuf;
    if (canIntern(&doc, config)) { 
        linksbuf << "<a href=\""<< linkPrefix()<< "P" << docnumforlinks << "\">" 
                 << trans("Preview") << "</a>&nbsp;&nbsp;";
    }
    if (canOpen(&doc, config)) {
        linksbuf << "<a href=\"" <<linkPrefix() + "E" <<docnumforlinks << "\">"  
                 << trans("Open") << "</a>";
    }
    ostringstream snipsbuf;
    if (doc.haspages) {
        snipsbuf << "<a href=\"" <<linkPrefix()<<"A" << docnumforlinks << "\">" 
                 << trans("Snippets") << "</a>&nbsp;&nbsp;";
        linksbuf << "&nbsp;&nbsp;" << snipsbuf.str();
    }

    string collapscnt;
    if (doc.getmeta(Rcl::Doc::keycc, &collapscnt) && !collapscnt.empty()) {
        ostringstream collpsbuf;
        int clc = atoi(collapscnt.c_str()) + 1;
        collpsbuf << "<a href=\""<<linkPrefix()<<"D" << docnumforlinks << "\">" 
                  << trans("Dups") << "(" << clc << ")" << "</a>&nbsp;&nbsp;";
        linksbuf << "&nbsp;&nbsp;" << collpsbuf.str();
    }

    // Build the result list paragraph:

    // Subheader: this is used by history
    if (!sh.empty())
        chunk << "<p style='clear: both;'><b>" << sh << "</p>\n<p>";
    else
        chunk << "<p style='margin: 0px;padding: 0px;clear: both;'>";

    char xdocidbuf[100];
    sprintf(xdocidbuf, "%lu", doc.xdocid);
    
    // Configurable stuff
    map<string, string> subs;
    subs["A"] = !richabst.empty() ? richabst : "";
    subs["D"] = datebuf;
    subs["E"] = snipsbuf.str();
    subs["I"] = iconurl;
    subs["i"] = doc.ipath;
    subs["K"] = !doc.meta[Rcl::Doc::keykw].empty() ? 
        string("[") + maybeEscapeHtml(doc.meta[Rcl::Doc::keykw]) + "]" : "";
    subs["L"] = linksbuf.str();
    subs["N"] = numbuf;
    subs["M"] = doc.mimetype;
    subs["P"] = parenturl;
    subs["R"] = doc.meta[Rcl::Doc::keyrr];
    subs["S"] = sizebuf;
    subs["T"] = maybeEscapeHtml(titleOrFilename);
    subs["t"] = maybeEscapeHtml(doc.meta[Rcl::Doc::keytt]);
    subs["U"] = url;
    subs["u"] = urlOrLocal;
    subs["x"] = xdocidbuf;
    
    // Let %(xx) access all metadata. HTML-neuter everything:
    for (const auto& entry : doc.meta) {
        if (!entry.first.empty()) 
            subs[entry.first] = maybeEscapeHtml(entry.second);
    }

    string formatted;
    pcSubst(parFormat(), formatted, subs);
    chunk << formatted;

    chunk << "</p>" << endl;
    // This was to force qt 4.x to clear the margins (which it should do
    // anyway because of the paragraph's style), but we finally took
    // the table approach for 1.15 for now (in guiutils.cpp)
//      chunk << "<br style='clear:both;height:0;line-height:0;'>" << endl;

    LOGDEB2("Chunk: [" << chunk.rdbuf()->str() << "]\n");
    append(chunk.rdbuf()->str(), i, doc);
}

bool ResListPager::getDoc(int num, Rcl::Doc& doc)
{
    if (m_winfirst < 0 || m_respage.size() == 0)
        return false;
    if (num < m_winfirst || num >= m_winfirst + int(m_respage.size()))
        return false;
    doc = m_respage[num-m_winfirst].doc;
    return true;
}

void ResListPager::displayPage(RclConfig *config)
{
    LOGDEB("ResListPager::displayPage. linkPrefix: " << linkPrefix() << "\n");
    if (!m_docSource) {
        LOGDEB("ResListPager::displayPage: null source\n");
        return;
    }
    if (m_winfirst < 0 && !pageEmpty()) {
        LOGDEB("ResListPager::displayPage: sequence error: winfirst < 0\n");
        return;
    }

    ostringstream chunk;

    // Display list header
    // We could use a <title> but the textedit doesnt display
    // it prominently
    // Note: have to append text in chunks that make sense
    // html-wise. If we break things up too much, the editor
    // gets confused. Hence the use of the 'chunk' text
    // accumulator
    // Also note that there can be results beyond the estimated resCnt.
    chunk << "<html><head>" << endl
          << "<meta http-equiv=\"content-type\""
          << " content=\"text/html; charset=utf-8\">" << endl
          << headerContent()
          << "</head><body>" << endl
          << pageTop()
          << "<p><span style=\"font-size:110%;\"><b>"
          << m_docSource->title()
          << "</b></span>&nbsp;&nbsp;&nbsp;";

    if (pageEmpty()) {
        chunk << trans("<p><b>No results found</b><br>");
        string reason = m_docSource->getReason();
        if (!reason.empty()) {
            chunk << "<blockquote>" << escapeHtml(reason) << 
                "</blockquote></p>";
        } else {
            HighlightData hldata;
            m_docSource->getTerms(hldata);
            vector<string> uterms(hldata.uterms.begin(), hldata.uterms.end());
            if (!uterms.empty()) {
                map<string, vector<string> > spellings;
                suggest(uterms, spellings);
                if (!spellings.empty()) {
                    if (o_index_stripchars) {
                        chunk << 
                            trans("<p><i>Alternate spellings (accents suppressed): </i>")
                              << "<br /><blockquote>";
                    } else {
                        chunk << 
                            trans("<p><i>Alternate spellings: </i>")
                              << "<br /><blockquote>";
                    
                    }

                    for (const auto& entry: spellings) {
                        chunk << "<b>" << entry.first << "</b> : ";
                        for (const auto& spelling : entry.second) {
                            chunk << spelling << " ";
                        }
                        chunk << "<br />";
                    }
                    chunk << "</blockquote></p>";
                }
            }
        }
    } else {
        unsigned int resCnt = m_docSource->getResCnt();
        if (m_winfirst + m_respage.size() < resCnt) {
            chunk << trans("Documents") << " <b>" << m_winfirst + 1
                  << "-" << m_winfirst + m_respage.size() << "</b> " 
                  << trans("out of at least") << " " 
                  << resCnt << " " << trans("for") << " " ;
        } else {
            chunk << trans("Documents") << " <b>" 
                  << m_winfirst + 1 << "-" << m_winfirst + m_respage.size()
                  << "</b> " << trans("for") << " ";
        }
    }
    chunk << detailsLink();
    if (hasPrev() || hasNext()) {
        chunk << "&nbsp;&nbsp;";
        if (hasPrev()) {
            chunk << "<a href=\"" << linkPrefix() + prevUrl() + "\"><b>"
                  << trans("Previous")
                  << "</b></a>&nbsp;&nbsp;&nbsp;";
        }
        if (hasNext()) {
            chunk << "<a href=\"" << linkPrefix() + nextUrl() + "\"><b>"
                  << trans("Next")
                  << "</b></a>";
        }
    }
    chunk << "</p>" << endl;

    append(chunk.rdbuf()->str());
    chunk.rdbuf()->str("");
    if (pageEmpty())
        return;

    HighlightData hdata;
    m_docSource->getTerms(hdata);

    // Emit data for result entry paragraph. Do it in chunks that make sense
    // html-wise, else our client may get confused
    for (int i = 0; i < (int)m_respage.size(); i++) {
        Rcl::Doc& doc(m_respage[i].doc);
        string& sh(m_respage[i].subHeader);
        displayDoc(config, i, doc, hdata, sh);
    }

    // Footer
    chunk << "<p align=\"center\">";
    if (hasPrev() || hasNext()) {
        if (hasPrev()) {
            chunk << "<a href=\"" + linkPrefix() + prevUrl() + "\"><b>" 
                  << trans("Previous")
                  << "</b></a>&nbsp;&nbsp;&nbsp;";
        }
        if (hasNext()) {
            chunk << "<a href=\"" << linkPrefix() + nextUrl() + "\"><b>"
                  << trans("Next")
                  << "</b></a>";
        }
    }
    chunk << "</p>" << endl;
    chunk << "</body></html>" << endl;
    append(chunk.rdbuf()->str());
}

// Default implementations for things that should be implemented by 
// specializations
string ResListPager::nextUrl()
{
    return "n-1";
}

string ResListPager::prevUrl()
{
    return "p-1";
}

string ResListPager::iconUrl(RclConfig *config, Rcl::Doc& doc)
{
    string apptag;
    doc.getmeta(Rcl::Doc::keyapptg, &apptag);

    return path_pathtofileurl(config->getMimeIconPath(doc.mimetype, apptag));
}

bool ResListPager::append(const string& data)
{
    fprintf(stderr, "%s", data.c_str());
    return true;
}

string ResListPager::trans(const string& in) 
{
    return in;
}

string ResListPager::detailsLink()
{
    string chunk = string("<a href=\"") + linkPrefix() + "H-1\">";
    chunk += trans("(show query)") + "</a>";
    return chunk;
}

const string &ResListPager::parFormat()
{
    static const string cstr_format("<img src=\"%I\" align=\"left\">"
                                    "%R %S %L &nbsp;&nbsp;<b>%T</b><br>"
                                    "%M&nbsp;%D&nbsp;&nbsp;&nbsp;<i>%U</i><br>"
                                    "%A %K");
    return cstr_format;
}

const string &ResListPager::dateFormat()
{
    static const string cstr_format("&nbsp;%Y-%m-%d&nbsp;%H:%M:%S&nbsp;%z");
    return cstr_format;
}
