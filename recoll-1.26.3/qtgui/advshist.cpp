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

#include "advshist.h"
#include "guiutils.h"
#include "log.h"
#include "xmltosd.h"

using namespace std;
using namespace Rcl;

AdvSearchHist::AdvSearchHist()
{
    read();
}

AdvSearchHist::~AdvSearchHist()
{
    for (auto& entry : m_entries) {
	entry.reset();
    }
}

std::shared_ptr<Rcl::SearchData> AdvSearchHist::getnewest() 
{
    if (m_entries.empty())
        return std::shared_ptr<Rcl::SearchData>();

    return m_entries[0];
}

std::shared_ptr<Rcl::SearchData> AdvSearchHist::getolder()
{
    m_current++;
    if (m_current >= int(m_entries.size())) {
	m_current--;
	return std::shared_ptr<Rcl::SearchData>();
    }
    return m_entries[m_current];
}

std::shared_ptr<Rcl::SearchData> AdvSearchHist::getnewer()
{
    if (m_current == -1 || m_current == 0 || m_entries.empty())
	return std::shared_ptr<Rcl::SearchData>();
    return m_entries[--m_current];
}

bool AdvSearchHist::push(std::shared_ptr<SearchData> sd)
{
    m_entries.insert(m_entries.begin(), sd);
    if (m_current != -1)
	m_current++;

    string xml = sd->asXML();
    // dynconf interprets <= 0 as unlimited size, but we want 0 to
    // disable saving history
    if (prefs.historysize != 0) {
        g_dynconf->enterString(advSearchHistSk, xml, prefs.historysize);
    }
    return true;
}

bool AdvSearchHist::read()
{
    if (!g_dynconf)
	return false;

    // getStringEntries() return the entries in order (lower key
    // first), but we want most recent first, so revert
    vector<string> lxml =
        g_dynconf->getStringEntries<vector>(advSearchHistSk);
    for (auto it = lxml.rbegin(); it != lxml.rend(); it++) {
        std::shared_ptr<SearchData> sd = xmlToSearchData(*it);
        if (sd)
            m_entries.push_back(sd);
    }
    return true;
}

void AdvSearchHist::clear()
{
    g_dynconf->eraseAll(advSearchHistSk);
}

