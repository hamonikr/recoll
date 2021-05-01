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

#include "log.h"
#include "filtseq.h"
#include "rclconfig.h"

using std::string;

static bool filter(const DocSeqFiltSpec& fs, const Rcl::Doc *x)
{
    LOGDEB2("  Filter: ncrits "  << (fs.crits.size()) << "\n" );
    // Compare using each criterion in term. We're doing an or:
    // 1st ok ends 
    for (unsigned int i = 0; i < fs.crits.size(); i++) {
	switch (fs.crits[i]) {
	case DocSeqFiltSpec::DSFS_MIMETYPE:
	    LOGDEB2(" filter: MIMETYPE: me ["  << (fs.values[i]) << "] doc ["  << (x->mimetype) << "]\n" );
	    if (x->mimetype == fs.values[i])
		return true;
	    break;
	case DocSeqFiltSpec::DSFS_QLANG:
	{
	    LOGDEB(" filter: QLANG ["  << (fs.values[i]) << "]!!\n" );
	}
	break;
	case DocSeqFiltSpec::DSFS_PASSALL:
	    return true;
	}
    }
    // Did all comparisons
    return false;
} 

DocSeqFiltered::DocSeqFiltered(RclConfig *conf, std::shared_ptr<DocSequence> iseq, 
			       DocSeqFiltSpec &filtspec)
    :  DocSeqModifier(iseq), m_config(conf)
{
    setFiltSpec(filtspec);
}

bool DocSeqFiltered::setFiltSpec(const DocSeqFiltSpec &filtspec)
{
    LOGDEB0("DocSeqFiltered::setFiltSpec\n" );
    for (unsigned int i = 0; i < filtspec.crits.size(); i++) {
	switch (filtspec.crits[i]) {
	case DocSeqFiltSpec::DSFS_MIMETYPE:
	    m_spec.orCrit(filtspec.crits[i], filtspec.values[i]);
	    break;
	case DocSeqFiltSpec::DSFS_QLANG:
	{
	    // There are very few lang constructs that we can
	    // interpret. The default config uses rclcat:value
	    // only. That will be all for now...
	    string val = filtspec.values[i];
	    if (val.find("rclcat:") == 0) {
		string catg = val.substr(7);
		vector<string> tps;
		m_config->getMimeCatTypes(catg, tps);
		for (vector<string>::const_iterator it = tps.begin();
		     it != tps.end(); it++) {
		    LOGDEB2("Adding mime: ["  << (it) << "]\n" );
		    m_spec.orCrit(DocSeqFiltSpec::DSFS_MIMETYPE, *it);
		}
	    }
	}
	break;
	default:
	    break;
	}
    }
    // If m_spec ends up empty, pass everything, better than filtering all.
    if (m_spec.crits.empty()) {
	m_spec.orCrit(DocSeqFiltSpec::DSFS_PASSALL, "");
    }
    m_dbindices.clear();
    return true;
}

bool DocSeqFiltered::getDoc(int idx, Rcl::Doc &doc, string *)
{
    LOGDEB2("DocSeqFiltered::getDoc() fetching "  << (idx) << "\n" );

    if (idx >= (int)m_dbindices.size()) {
	// Have to fetch docs and filter until we get enough or
	// fail
	m_dbindices.reserve(idx+1);

	// First backend seq doc we fetch is the one after last stored 
	int backend_idx = m_dbindices.size() > 0 ? m_dbindices.back() + 1 : 0;

	// Loop until we get enough docs
	Rcl::Doc tdoc;
	while (idx >= (int)m_dbindices.size()) {
	    if (!m_seq->getDoc(backend_idx, tdoc)) 
		return false;
	    if (filter(m_spec, &tdoc)) {
		m_dbindices.push_back(backend_idx);
	    }
	    backend_idx++;
	}
	doc = tdoc;
    } else {
	// The corresponding backend indice is already known
	if (!m_seq->getDoc(m_dbindices[idx], doc)) 
	    return false;
    }
    return true;
}

