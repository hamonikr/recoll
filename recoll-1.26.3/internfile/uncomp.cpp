/* Copyright (C) 2013 J.F.Dockes
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

#include <errno.h>

#include <string>
#include <vector>
#include <map>

#include "uncomp.h"
#include "log.h"
#include "smallut.h"
#include "execmd.h"
#include "pathut.h"

using std::map;
using std::string;
using std::vector;

Uncomp::UncompCache Uncomp::o_cache;

Uncomp::Uncomp(bool docache)
	: m_docache(docache)
{
    LOGDEB0("Uncomp::Uncomp: m_docache: " << m_docache << "\n");
}

bool Uncomp::uncompressfile(const string& ifn, 
			    const vector<string>& cmdv, string& tfile)
{
    if (m_docache) {
        std::unique_lock<std::mutex> lock(o_cache.m_lock);
	if (!o_cache.m_srcpath.compare(ifn)) {
	    m_dir = o_cache.m_dir;
	    m_tfile = tfile = o_cache.m_tfile;
	    m_srcpath = ifn;
	    o_cache.m_dir = 0;
	    o_cache.m_srcpath.clear();
	    return true;
	}
    }

    m_srcpath.clear();
    m_tfile.clear();
    if (m_dir == 0) {
	m_dir = new TempDir;
    }
    // Make sure tmp dir is empty. we guarantee this to filters
    if (!m_dir || !m_dir->ok() || !m_dir->wipe()) {
	LOGERR("uncompressfile: can't clear temp dir " << m_dir->dirname() <<
               "\n");
	return false;
    }

    // Check that we have enough available space to have some hope of
    // decompressing the file.
    int pc;
    long long availmbs;
    if (!fsocc(m_dir->dirname(), &pc, &availmbs)) {
        LOGERR("uncompressfile: can't retrieve avail space for " <<
               m_dir->dirname() << "\n");
        // Hope for the best
    } else {
	long long fsize = path_filesize(ifn);
        if (fsize < 0) {
            LOGERR("uncompressfile: stat input file " << ifn << " errno " <<
                   errno << "\n");
            return false;
        }
        // We need at least twice the file size for the uncompressed
        // and compressed versions. Most compressors don't store the
        // uncompressed size, so we have no way to be sure that we
        // have enough space before trying. We take a little margin

        // use same Mb def as fsocc()
        long long filembs = fsize / (1024 * 1024); 
        
        if (availmbs < 2 * filembs + 1) {
            LOGERR("uncompressfile. " << availmbs << " MBs available in " <<
                   m_dir->dirname() << " not enough to uncompress " <<
                   ifn << " of size "  << filembs << " MBs\n");
            return false;
        }
    }

    string cmd = cmdv.front();

    // Substitute file name and temp dir in command elements
    vector<string>::const_iterator it = cmdv.begin();
    ++it;
    vector<string> args;
    map<char, string> subs;
    subs['f'] = ifn;
    subs['t'] = m_dir->dirname();
    for (; it != cmdv.end(); it++) {
	string ns;
	pcSubst(*it, ns, subs);
	args.push_back(ns);
    }

    // Execute command and retrieve output file name, check that it exists
    ExecCmd ex;
    int status = ex.doexec(cmd, args, 0, &tfile);
    if (status || tfile.empty()) {
	LOGERR("uncompressfile: doexec: " << cmd << " " <<
               stringsToString(args) << " failed for [" <<
               ifn << "] status 0x" << status << "\n");
	if (!m_dir->wipe()) {
	    LOGERR("uncompressfile: wipedir failed\n");
	}
	return false;
    }
    rtrimstring(tfile, "\n\r");
    m_tfile = tfile;
    m_srcpath = ifn;
    return true;
}

Uncomp::~Uncomp()
{
    LOGDEB0("Uncomp::~Uncomp: m_docache: " << m_docache << " m_dir " <<
            (m_dir?m_dir->dirname():"(null)") << "\n");
    if (m_docache) {
        std::unique_lock<std::mutex> lock(o_cache.m_lock);
	delete o_cache.m_dir;
	o_cache.m_dir = m_dir;
	o_cache.m_tfile = m_tfile;
	o_cache.m_srcpath = m_srcpath;
    } else {
	delete m_dir;
    }
}

void Uncomp::clearcache()
{
    LOGDEB0("Uncomp::clearcache\n");
    std::unique_lock<std::mutex> lock(o_cache.m_lock);
    delete o_cache.m_dir;
    o_cache.m_dir = 0;
    o_cache.m_tfile.clear();
    o_cache.m_srcpath.clear();
}
