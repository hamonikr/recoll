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

#include <limits.h>
#include <string>
#include <utility>
#include <list>
#include <set>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <regex>

using std::vector;
using std::list;
using std::pair;
using std::set;
using std::unordered_map;

#include "rcldb.h"
#include "rclconfig.h"
#include "log.h"
#include "textsplit.h"
#include "utf8iter.h"
#include "smallut.h"
#include "chrono.h"
#include "plaintorich.h"
#include "cancelcheck.h"
#include "unacpp.h"

// Text splitter used to take note of the position of query terms
// inside the result text. This is then used to insert highlight tags.
class TextSplitPTR : public TextSplit {
public:

    // Out: begin and end byte positions of query terms/groups in text
    vector<GroupMatchEntry> m_tboffs;

    TextSplitPTR(const HighlightData& hdata)
        :  m_wcount(0), m_hdata(hdata) {
        // We separate single terms and groups and extract the group
        // terms for computing positions list before looking for group
        // matches. Single terms are stored with a reference to the
        // entry they come with.
        for (unsigned int i = 0; i < hdata.index_term_groups.size(); i++) {
            const HighlightData::TermGroup& tg(hdata.index_term_groups[i]);
            if (tg.kind == HighlightData::TermGroup::TGK_TERM) {
                m_terms[tg.term] = i;
            } else {
                for (const auto& group : tg.orgroups) {
                    for (const auto& term : group) {
                        m_gterms.insert(term);
                    }
                }
            }
        }
    }

    // Accept word and its position. If word is search term, add
    // highlight zone definition. If word is part of search group
    // (phrase or near), update positions list.
    virtual bool takeword(const std::string& term, int pos, int bts, int bte) {
        string dumb = term;
        if (o_index_stripchars) {
            if (!unacmaybefold(term, dumb, "UTF-8", UNACOP_UNACFOLD)) {
                LOGINFO("PlainToRich::takeword: unac failed for [" << term <<
                        "]\n");
                return true;
            }
        }

        LOGDEB2("Input dumbbed term: '" << dumb << "' " <<  pos << " " << bts
                << " " << bte << "\n");

        // If this word is a search term, remember its byte-offset span. 
        map<string, size_t>::const_iterator it = m_terms.find(dumb);
        if (it != m_terms.end()) {
            m_tboffs.push_back(GroupMatchEntry(bts, bte, it->second));
        }
        
        // If word is part of a search group, update its positions list
        if (m_gterms.find(dumb) != m_gterms.end()) {
            // Term group (phrase/near) handling
            m_plists[dumb].push_back(pos);
            m_gpostobytes[pos] = pair<int,int>(bts, bte);
            LOGDEB2("Recorded bpos for " << pos << ": " << bts << " " <<
                    bte << "\n");
        }

        // Check for cancellation request
        if ((m_wcount++ & 0xfff) == 0)
            CancelCheck::instance().checkCancel();

        return true;
    }

    // Must be called after the split to find the phrase/near match positions
    virtual bool matchGroups();

private:
    // Word count. Used to call checkCancel from time to time.
    int m_wcount;

    // In: user query terms
    map<string, size_t>    m_terms; 

    // m_gterms holds all the terms in m_groups, as a set for quick lookup
    set<string>    m_gterms;

    const HighlightData& m_hdata;

    // group/near terms word positions.
    unordered_map<string, vector<int> > m_plists;
    unordered_map<int, pair<int, int> > m_gpostobytes;
};


// Look for matches to PHRASE and NEAR term groups and finalize the
// matched regions list (sort it by increasing start then decreasing
// length)
bool TextSplitPTR::matchGroups()
{
    for (unsigned int i = 0; i < m_hdata.index_term_groups.size(); i++) {
        if (m_hdata.index_term_groups[i].kind !=
            HighlightData::TermGroup::TGK_TERM) {
            matchGroup(m_hdata, i, m_plists, m_gpostobytes, m_tboffs);
        }
    }

    // Sort regions by increasing start and decreasing width.  
    // The output process will skip overlapping entries.
    std::sort(m_tboffs.begin(), m_tboffs.end(),
              [](const GroupMatchEntry& a, const GroupMatchEntry& b) -> bool {
                  if (a.offs.first != b.offs.first)
                      return a.offs.first < b.offs.first;
                  return a.offs.second > b.offs.second;
              }
        );
    return true;
}

// Replace HTTP(s) urls in text/plain with proper HTML anchors so that
// they become clickable in the preview. We don't make a lot of effort
// for validating, or catching things which are probably urls but miss
// a scheme (e.g. www.xxx.com/index.html), because complicated.
static const string urlRE = "(https?://[[:alnum:]~_/.%?&=,#@]+)[[:space:]|]";
static const string urlRep{"<a href=\"$1\">$1</a>"};
static std::regex url_re(urlRE);
static string activate_urls(const string& in)
{
    return std::regex_replace(in, url_re, urlRep);
}

// Fix result text for display inside the gui text window.
//
// We call overridden functions to output header data, beginnings and ends of
// matches etc.
//
// If the input is text, we output the result in chunks, arranging not
// to cut in the middle of a tag, which would confuse qtextedit. If
// the input is html, the body is always a single output chunk.
bool PlainToRich::plaintorich(const string& in, 
                              list<string>& out, // Output chunk list
                              const HighlightData& hdata,
                              int chunksize)
{
    Chrono chron;
    bool ret = true;
    LOGDEB1("plaintorichich: in: [" << in << "]\n");

    m_hdata = &hdata;
    // Compute the positions for the query terms.  We use the text
    // splitter to break the text into words, and compare the words to
    // the search terms,
    TextSplitPTR splitter(hdata);
    // Note: the splitter returns the term locations in byte, not
    // character, offsets.
    splitter.text_to_words(in);
    LOGDEB2("plaintorich: split done " << chron.millis() << " mS\n");
    // Compute the positions for NEAR and PHRASE groups.
    splitter.matchGroups();
    LOGDEB2("plaintorich: group match done " << chron.millis() << " mS\n");

    out.clear();
    out.push_back("");
    list<string>::iterator olit = out.begin();

    // Rich text output
    *olit = header();
    
    // No term matches. Happens, for example on a snippet selected for
    // a term match when we are actually looking for a group match
    // (the snippet generator does this...).
    if (splitter.m_tboffs.empty()) {
        LOGDEB1("plaintorich: no term matches\n");
        ret = false;
    }

    // Iterator for the list of input term positions. We use it to
    // output highlight tags and to compute term positions in the
    // output text
    vector<GroupMatchEntry>::iterator tPosIt = splitter.m_tboffs.begin();
    vector<GroupMatchEntry>::iterator tPosEnd = splitter.m_tboffs.end();

#if 0
    for (vector<pair<int, int> >::const_iterator it = splitter.m_tboffs.begin();
         it != splitter.m_tboffs.end(); it++) {
        LOGDEB2("plaintorich: region: " << it->first << " "<<it->second<< "\n");
    }
#endif

    // Input character iterator
    Utf8Iter chariter(in);

    // State variables used to limit the number of consecutive empty lines,
    // convert all eol to '\n', and preserve some indentation
    int eol = 0;
    int hadcr = 0;
    int inindent = 1;

    // HTML state
    bool intag = false, inparamvalue = false;
    // My tag state
    int inrcltag = 0;

    string::size_type headend = 0;
    if (m_inputhtml) {
        headend = in.find("</head>");
        if (headend == string::npos)
            headend = in.find("</HEAD>");
        if (headend != string::npos)
            headend += 7;
    }

    for (string::size_type pos = 0; pos != string::npos; pos = chariter++) {
        // Check from time to time if we need to stop
        if ((pos & 0xfff) == 0) {
            CancelCheck::instance().checkCancel();
        }

        // If we still have terms positions, check (byte) position. If
        // we are at or after a term match, mark.
        if (tPosIt != tPosEnd) {
            int ibyteidx = int(chariter.getBpos());
            if (ibyteidx == tPosIt->offs.first) {
                if (!intag && ibyteidx >= (int)headend) {
                    *olit += startMatch((unsigned int)(tPosIt->grpidx));
                }
                inrcltag = 1;
            } else if (ibyteidx == tPosIt->offs.second) {
                // Output end of match region tags
                if (!intag && ibyteidx > (int)headend) {
                    *olit += endMatch();
                }
                // Skip all highlight areas that would overlap this one
                int crend = tPosIt->offs.second;
                while (tPosIt != splitter.m_tboffs.end() && 
                       tPosIt->offs.first < crend)
                    tPosIt++;
                inrcltag = 0;
            }
        }
        
        unsigned int car = *chariter;

        if (car == '\n') {
            if (!hadcr)
                eol++;
            hadcr = 0;
            continue;
        } else if (car == '\r') {
            hadcr++;
            eol++;
            continue;
        } else if (eol) {
            // Got non eol char in line break state. Do line break;
            inindent = 1;
            hadcr = 0;
            if (eol > 2)
                eol = 2;
            while (eol) {
                if (!m_inputhtml && m_eolbr)
                    *olit += "<br>";
                *olit += "\n";
                eol--;
            }
            // Maybe end this chunk, begin next. Don't do it on html
            // there is just no way to do it right (qtextedit cant grok
            // chunks cut in the middle of <a></a> for example).
            if (!m_inputhtml && !inrcltag && 
                olit->size() > (unsigned int)chunksize) {
                if (m_activatelinks) {
                    *olit = activate_urls(*olit);
                }
                out.push_back(string(startChunk()));
                olit++;
            }
        }

        switch (car) {
        case '<':
            inindent = 0;
            if (m_inputhtml) {
                if (!inparamvalue)
                    intag = true;
                chariter.appendchartostring(*olit);    
            } else {
                *olit += "&lt;";
            }
            break;
        case '>':
            inindent = 0;
            if (m_inputhtml) {
                if (!inparamvalue)
                    intag = false;
            }
            chariter.appendchartostring(*olit);    
            break;
        case '&':
            inindent = 0;
            if (m_inputhtml) {
                chariter.appendchartostring(*olit);
            } else {
                *olit += "&amp;";
            }
            break;
        case '"':
            inindent = 0;
            if (m_inputhtml && intag) {
                inparamvalue = !inparamvalue;
            }
            chariter.appendchartostring(*olit);
            break;

        case ' ': 
            if (m_eolbr && inindent) {
                *olit += "&nbsp;";
            } else {
                chariter.appendchartostring(*olit);
            }
            break;
        case '\t': 
            if (m_eolbr && inindent) {
                *olit += "&nbsp;&nbsp;&nbsp;&nbsp;";
            } else {
                chariter.appendchartostring(*olit);
            }
            break;

        default:
            inindent = 0;
            chariter.appendchartostring(*olit);
        }

    } // End chariter loop

#if 0
    {
        FILE *fp = fopen("/tmp/debugplaintorich", "a");
        fprintf(fp, "BEGINOFPLAINTORICHOUTPUT\n");
        for (list<string>::iterator it = out.begin();
             it != out.end(); it++) {
            fprintf(fp, "BEGINOFPLAINTORICHCHUNK\n");
            fprintf(fp, "%s", it->c_str());
            fprintf(fp, "ENDOFPLAINTORICHCHUNK\n");
        }
        fprintf(fp, "ENDOFPLAINTORICHOUTPUT\n");
        fclose(fp);
    }
#endif
    LOGDEB2("plaintorich: done " << chron.millis() << " mS\n");
    if (!m_inputhtml && m_activatelinks) {
        out.back() = activate_urls(out.back());
    }
    return ret;
}
