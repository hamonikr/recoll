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

#include "docseq.h"
#include "filtseq.h"
#include "sortseq.h"
#include "log.h"
#include "internfile.h"

std::mutex DocSequence::o_dblock;
string DocSequence::o_sort_trans;
string DocSequence::o_filt_trans;

int DocSequence::getSeqSlice(int offs, int cnt, vector<ResListEntry>& result)
{
    int ret = 0;
    for (int num = offs; num < offs + cnt; num++, ret++) {
	result.push_back(ResListEntry());
	if (!getDoc(num, result.back().doc, &result.back().subHeader)) {
	    result.pop_back();
	    return ret;
	}
    }
    return ret;
}

bool DocSequence::getEnclosing(Rcl::Doc& doc, Rcl::Doc& pdoc) 
{
    std::shared_ptr<Rcl::Db> db = getDb();
    if (!db) {
	LOGERR("DocSequence::getEnclosing: no db\n" );
	return false;
    }
    std::unique_lock<std::mutex> locker(o_dblock);
    string udi;
    if (!FileInterner::getEnclosingUDI(doc, udi))
        return false;

    bool dbret =  db->getDoc(udi, doc, pdoc);
    return dbret && pdoc.pc != -1;
}

// Remove stacked modifying sources (sort, filter) until we get to a real one
void DocSource::stripStack()
{
    if (!m_seq)
	return;
    while (m_seq->getSourceSeq()) {
	m_seq = m_seq->getSourceSeq();
    }
}

bool DocSource::buildStack()
{
    LOGDEB2("DocSource::buildStack()\n" );
    stripStack();

    if (!m_seq)
	return false;

    // Filtering must be done before sorting, (which may
    // truncates the original list)
    if (m_seq->canFilter()) {
	if (!m_seq->setFiltSpec(m_fspec)) {
	    LOGERR("DocSource::buildStack: setfiltspec failed\n" );
	}
    } else {
	if (m_fspec.isNotNull()) {
	    m_seq = 
		std::shared_ptr<DocSequence>(new DocSeqFiltered(m_config, m_seq, m_fspec));
	} 
    }
    
    if (m_seq->canSort()) {
	if (!m_seq->setSortSpec(m_sspec)) {
	    LOGERR("DocSource::buildStack: setsortspec failed\n" );
	}
    } else {
	if (m_sspec.isNotNull()) {
	    m_seq = std::shared_ptr<DocSequence>(new DocSeqSorted(m_seq, m_sspec));
	}
    }
    return true;
}

string DocSource::title()
{
    if (!m_seq)
	return string();
    string qual;
    if (m_fspec.isNotNull() && !m_sspec.isNotNull())
	qual = string(" (") + o_filt_trans + string(")");
    else if (!m_fspec.isNotNull() && m_sspec.isNotNull())
	qual = string(" (") + o_sort_trans + string(")");
    else if (m_fspec.isNotNull() && m_sspec.isNotNull())
	qual = string(" (") + o_sort_trans + string(",") + o_filt_trans + string(")");
    return m_seq->title() + qual;
}

bool DocSource::setFiltSpec(const DocSeqFiltSpec &f) 
{
    LOGDEB2("DocSource::setFiltSpec\n" );
    m_fspec = f;
    buildStack();
    return true;
}

bool DocSource::setSortSpec(const DocSeqSortSpec &s) 
{
    LOGDEB2("DocSource::setSortSpec\n" );
    m_sspec = s;
    buildStack();
    return true;
}


