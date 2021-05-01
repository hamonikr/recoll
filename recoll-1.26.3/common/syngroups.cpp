/* Copyright (C) 2014-2019 J.F.Dockes
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

#include "syngroups.h"

#include "log.h"
#include "smallut.h"
#include "pathut.h"

#include <errno.h>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <cstring>
#include "safesysstat.h"

using namespace std;

// Note that we are storing each term twice. I don't think that the
// size could possibly be a serious issue, but if it was, we could
// reduce the storage a bit by storing (short hash)-> vector<int>
// correspondances in the direct map, and then checking all the
// resulting groups for the input word.
//
// As it is, a word can only index one group (the last it is found
// in). It can be part of several groups though (appear in
// expansions). I really don't know what we should do with multiple
// groups anyway
class SynGroups::Internal {
public:
    Internal() : ok(false) {
    }
    void setpath(const string& fn) {
        path = path_canon(fn);
        stat(path.c_str(), &st);
    }
    bool samefile(const string& fn) {
        string p1 = path_canon(fn);
        if (path != p1) {
            return false;
        }
        struct stat st1;
        if (stat(p1.c_str(), &st1) != 0) {
            return false;
        }
        return st.st_mtime == st1.st_mtime && st.st_size == st1.st_size;
    }
    bool ok;
    // Term to group num 
    std::unordered_map<string, unsigned int> terms;
    // Group num to group
    vector<vector<string> > groups;
    std::string path;
    struct stat st;
};

bool SynGroups::ok() 
{
    return m && m->ok;
}

SynGroups::~SynGroups()
{
    delete m;
}

SynGroups::SynGroups()
    : m(new Internal)
{
}

bool SynGroups::setfile(const string& fn)
{
    LOGDEB("SynGroups::setfile(" << fn << ")\n");
    if (!m) {
        m = new Internal;
        if (!m) {
            LOGERR("SynGroups:setfile:: new Internal failed: no mem ?\n");
            return false;
        }
    }

    if (fn.empty()) {
        delete m;
        m = 0;
	return true;
    }

    if (m->samefile(fn)) {
        LOGDEB("SynGroups::setfile: unchanged: " << fn << endl);
        return true;
    }
    LOGDEB("SynGroups::setfile: parsing file " << fn << endl);
    
    ifstream input;
    input.open(fn.c_str(), ios::in);
    if (!input.is_open()) {
	LOGSYSERR("SynGroups:setfile", "open", fn);
	return false;
    }	    

    string cline;
    bool appending = false;
    string line;
    bool eof = false;
    int lnum = 0;

    for (;;) {
        cline.clear();
	getline(input, cline);
	if (!input.good()) {
	    if (input.bad()) {
                LOGERR("Syngroup::setfile(" << fn << "):Parse: input.bad()\n");
		return false;
	    }
	    // Must be eof ? But maybe we have a partial line which
	    // must be processed. This happens if the last line before
	    // eof ends with a backslash, or there is no final \n
            eof = true;
	}
	lnum++;

        {
            string::size_type pos = cline.find_last_not_of("\n\r");
            if (pos == string::npos) {
                cline.clear();
            } else if (pos != cline.length()-1) {
                cline.erase(pos+1);
            }
        }

	if (appending)
	    line += cline;
	else
	    line = cline;

	// Note that we trim whitespace before checking for backslash-eol
	// This avoids invisible whitespace problems.
	trimstring(line);
	if (line.empty() || line.at(0) == '#') {
            if (eof)
                break;
	    continue;
	}
	if (line[line.length() - 1] == '\\') {
	    line.erase(line.length() - 1);
	    appending = true;
	    continue;
	}
	appending = false;

	vector<string> words;
	if (!stringToStrings(line, words)) {
	    LOGERR("SynGroups:setfile: " << fn << ": bad line " << lnum <<
                   ": " << line << "\n");
	    continue;
	}

	if (words.empty())
	    continue;
	if (words.size() == 1) {
	    LOGERR("Syngroup::setfile(" << fn << "):single term group at line "
                   << lnum << " ??\n");
	    continue;
	}

	m->groups.push_back(words);
	for (const auto& word : words) {
	    m->terms[word] = m->groups.size()-1;
	}
	LOGDEB1("SynGroups::setfile: group: [" <<
                stringsToString(m->groups.back()) << "]\n");
    }
    LOGDEB("SynGroups::setfile: got " << m->groups.size() <<
           " distinct terms." << endl);
    m->ok = true;
    m->setpath(fn);
    return true;
}

vector<string> SynGroups::getgroup(const string& term)
{
    vector<string> ret;
    if (!ok())
	return ret;

    const auto it1 = m->terms.find(term);
    if (it1 == m->terms.end()) {
	LOGDEB0("SynGroups::getgroup: [" << term << "] not found in map\n");
	return ret;
    }

    unsigned int idx = it1->second;
    if (idx >= m->groups.size()) {
        LOGERR("SynGroups::getgroup: line index higher than line count !\n");
        return ret;
    }
    LOGDEB0("SynGroups::getgroup: result: " << stringsToString(m->groups[idx])
            << endl);
    return m->groups[idx];
}
