/* Copyright (C) 2012-2019 J.F.Dockes
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

#include <iostream>
#include <algorithm>
#include <memory>

#include "log.h"
#include "cstr.h"
#include "xmacros.h"
#include "synfamily.h"
#include "smallut.h"

using namespace std;

namespace Rcl {

bool XapWritableSynFamily::createMember(const string& membername)
{
    string ermsg;
    try {
        m_wdb.add_synonym(memberskey(), membername);
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
        LOGERR("XapSynFamily::createMember: error: " << ermsg << "\n");
        return false;
    }
    return true;
}

bool XapWritableSynFamily::deleteMember(const string& membername)
{
    string key = entryprefix(membername);

    for (Xapian::TermIterator xit = m_wdb.synonym_keys_begin(key);
         xit != m_wdb.synonym_keys_end(key); xit++) {
        m_wdb.clear_synonyms(*xit);
    }
    m_wdb.remove_synonym(memberskey(), membername);
    return true;
}

bool XapSynFamily::getMembers(vector<string>& members)
{
    string key = memberskey();
    string ermsg;
    try {
        for (Xapian::TermIterator xit = m_rdb.synonyms_begin(key);
             xit != m_rdb.synonyms_end(key); xit++) {
            members.push_back(*xit);
        }
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
        LOGERR("XapSynFamily::getMembers: xapian error " << ermsg << "\n");
        return false;
    }
    return true;
}

bool XapSynFamily::listMap(const string& membername)
{
    string key = entryprefix(membername);
    string ermsg;
    try {
        for (Xapian::TermIterator kit = m_rdb.synonym_keys_begin(key);
             kit != m_rdb.synonym_keys_end(key); kit++) {
            cout << "[" << *kit << "] -> ";
            for (Xapian::TermIterator xit = m_rdb.synonyms_begin(*kit);
                 xit != m_rdb.synonyms_end(*kit); xit++) {
                cout << *xit << " ";
            }
            cout << endl;
        }
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
        LOGERR("XapSynFamily::listMap: xapian error " << ermsg << "\n");
        return false;
    }
    vector<string>members;
    getMembers(members);
    cout << "All family members: ";
    for (vector<string>::const_iterator it = members.begin();
         it != members.end(); it++) {
        cout << *it << " ";
    }
    cout << endl;
    return true;
}

bool XapSynFamily::synExpand(const string& member, const string& term,
                             vector<string>& result)
{
    LOGDEB("XapSynFamily::synExpand:(" << m_prefix1 << ") " << term <<
           " for " << member << "\n");

    string key = entryprefix(member) + term;
    string ermsg;
    try {
        for (Xapian::TermIterator xit = m_rdb.synonyms_begin(key);
             xit != m_rdb.synonyms_end(key); xit++) {
            LOGDEB2("  Pushing " << *xit << "\n");
            result.push_back(*xit);
        }
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
        LOGERR("synFamily::synExpand: error for member [" << member <<
               "] term [" << term << "]\n");
        result.push_back(term);
        return false;
    }
    // If the input term is not in the list, add it
    if (find(result.begin(), result.end(), term) == result.end()) {
        result.push_back(term);
    }

    return true;
}

bool XapComputableSynFamMember::synExpand(const string& term, 
                                          vector<string>& result,
                                          SynTermTrans *filtertrans)
{
    string root = (*m_trans)(term);
    string filter_root;
    if (filtertrans)
        filter_root = (*filtertrans)(term);

    string key = m_prefix + root;

    LOGDEB("XapCompSynFamMbr::synExpand([" << m_prefix << "]): term [" <<
           term << "] root [" << root << "] m_trans: " << m_trans->name() <<
           " filter: " << (filtertrans ? filtertrans->name() : "none") << "\n");

    string ermsg;
    try {
        for (Xapian::TermIterator xit = m_family.getdb().synonyms_begin(key);
             xit != m_family.getdb().synonyms_end(key); xit++) {
            LOGDEB("XapCompSynFamMbr::synExpand: testing " << *xit << endl);
            if (!filtertrans || (*filtertrans)(*xit) == filter_root) {
                LOGDEB2("  Pushing " << *xit << "\n");
                result.push_back(*xit);
            }
        }
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
        LOGERR("XapSynDb::synExpand: error for term [" << term <<
               "] (key " << key << ")\n");
        result.push_back(term);
        return false;
    }

    // If the input term and root are not in the list, add them
    if (find(result.begin(), result.end(), term) == result.end()) {
        LOGDEB2("  Pushing " << term << "\n");
        result.push_back(term);
    }
    if (root != term && 
        find(result.begin(), result.end(), root) == result.end()) {
        if (!filtertrans || (*filtertrans)(root) == filter_root) {
            LOGDEB2("  Pushing " << root << "\n");
            result.push_back(root);
        }
    }
    LOGDEB("XapCompSynFamMbr::synExpand([" << m_prefix << "]): term [" <<
           term << "] -> [" << stringsToString(result) << "]\n");
    return true;
}

bool XapComputableSynFamMember::synKeyExpand(StrMatcher* inexp,
                                             vector<string>& result,
                                             SynTermTrans *filtertrans)
{
    LOGDEB("XapCompSynFam::synKeyExpand: [" << inexp->exp() << "]\n");
    
    // If set, compute filtering term (e.g.: only case-folded)
    std::shared_ptr<StrMatcher> filter_exp;
    if (filtertrans) {
        filter_exp = std::shared_ptr<StrMatcher>(inexp->clone());
        filter_exp->setExp((*filtertrans)(inexp->exp()));
    }

    // Transform input into our key format (e.g.: case-folded + diac-stripped),
    // and prepend prefix
    inexp->setExp(m_prefix + (*m_trans)(inexp->exp()));
    // Find the initial section before any special chars for skipping the keys
    string::size_type es = inexp->baseprefixlen();
    string is = inexp->exp().substr(0, es);  
    string::size_type preflen = m_prefix.size();
    LOGDEB2("XapCompSynFam::synKeyExpand: init section: [" << is << "]\n");

    string ermsg;
    try {
        for (Xapian::TermIterator xit = m_family.getdb().synonym_keys_begin(is);
             xit != m_family.getdb().synonym_keys_end(is); xit++) {
            LOGDEB2("  Checking1 [" << *xit << "] against [" <<
                    inexp->exp() << "]\n");
            if (!inexp->match(*xit))
                continue;

            // Push all the synonyms if they match the secondary filter
            for (Xapian::TermIterator xit1 = 
                     m_family.getdb().synonyms_begin(*xit);
                 xit1 != m_family.getdb().synonyms_end(*xit); xit1++) {
                string term = *xit1;
                if (filter_exp) {
                    string term1 = (*filtertrans)(term);
                    LOGDEB2("  Testing [" << term1 << "] against [" <<
                            filter_exp->exp() << "]\n");
                    if (!filter_exp->match(term1)) {
                        continue;
                    }
                }
                LOGDEB2("XapCompSynFam::keyWildExpand: [" << *xit1 << "]\n");
                result.push_back(*xit1);
            }
            // Same with key itself
            string term = (*xit).substr(preflen);
            if (filter_exp) {
                string term1 = (*filtertrans)(term);
                LOGDEB2(" Testing [" << term1 << "] against [" <<
                        filter_exp->exp() << "]\n");
                if (!filter_exp->match(term1)) {
                    continue;
                }
            }
            LOGDEB2("XapCompSynFam::keyWildExpand: [" << term << "]\n");
            result.push_back(term);
        }
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
        LOGERR("XapCompSynFam::synKeyExpand: xapian: [" << ermsg << "]\n");
        return false;
    }
    LOGDEB1("XapCompSynFam::synKeyExpand: final: [" <<
            stringsToString(result) << "]\n");
    return true;
}


} // Namespace Rcl
