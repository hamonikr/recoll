/* Copyright (C) 2011 J.F.Dockes
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
#ifndef _TERMPROC_H_INCLUDED_
#define _TERMPROC_H_INCLUDED_

#include <vector>
#include <string>

#include "textsplit.h"
#include "stoplist.h"
#include "smallut.h"
#include "utf8iter.h"

namespace Rcl {

/**
 * Termproc objects take term tokens as input and do something
 * with them: transform to lowercase, filter out stop words, generate n-grams,
 * finally index or generate search clauses, etc. They are chained and can
 * be arranged to form different pipelines depending on the desired processing
 * steps: for example, optional stoplist or commongram processing.
 *
 * Shared processing steps are defined in this file. The first and last steps
 * are usually defined in the specific module.
 * - The front TermProc is typically chained from a TextSplit object
 *   which generates the original terms, and calls takeword() from its
 *   own takeword() method.
 * - The last TermProc does something with the finalized terms, e.g. adds
 *   them to the index.
 */

/**
 * The base class takes care of chaining: all derived classes call its
 * takeword() and flush() methods to ensure that terms go through the pipe.
 */
class TermProc {
public:
    TermProc(TermProc* next) : m_next(next) {}
    virtual ~TermProc() {}
    virtual bool takeword(const string &term, int pos, int bs, int be)
    {
        if (m_next)
            return m_next->takeword(term, pos, bs, be);
        else
            return true;
    }
    // newpage() is like takeword(), but for page breaks.
    virtual void newpage(int pos)
    {
        if (m_next)
            m_next->newpage(pos);
    }
    virtual bool flush()
    {
        if (m_next)
            return m_next->flush();
        else
            return true;
    }
private:
    TermProc *m_next;
    /* Copyconst and assignment private and forbidden */
    TermProc(const TermProc &) {}
    TermProc& operator=(const TermProc &) {
        return *this;
    };
};

/**
 * Helper specialized TextSplit class, feeds the pipeline:
 * - The takeword() method calls a TermProc->takeword().
 * - The text_to_words() method also takes care of flushing.
 * Both methods can be further specialized by the user (they should then call
 * the base methods when they've done the local processing).
 */
class TextSplitP : public TextSplit {
public:
    TextSplitP(TermProc *prc, Flags flags = Flags(TXTS_NONE))
        : TextSplit(flags), m_prc(prc)  {}

    virtual bool text_to_words(const string &in) {
        bool ret = TextSplit::text_to_words(in);
        if (m_prc && !m_prc->flush())
            return false;
        return ret;
    }

    virtual bool takeword(const string& term, int pos, int bs, int be) {
        if (m_prc)
            return m_prc->takeword(term, pos, bs, be);
        else
            return true;
    }

    virtual void newpage(int pos) {
        if (m_prc)
            return m_prc->newpage(pos);
    }

private:
    TermProc *m_prc;
};

/** Unaccent and lowercase term. If the index is
 *  not case/diac-sensitive, this is usually the first step in the pipeline
 */
class TermProcPrep : public TermProc {
public:
    TermProcPrep(TermProc *nxt)
        : TermProc(nxt), m_totalterms(0), m_unacerrors(0)
    {
    }

    virtual bool takeword(const string& itrm, int pos, int bs, int be)
    {
        m_totalterms++;
        string otrm;

        if (!unacmaybefold(itrm, otrm, "UTF-8", UNACOP_UNACFOLD)) {
            LOGDEB("splitter::takeword: unac [" << itrm << "] failed\n");
            m_unacerrors++;
            // We don't generate a fatal error because of a bad term,
            // but one has to put the limit somewhere
            if (m_unacerrors > 500 &&
                    (double(m_totalterms) / double(m_unacerrors)) < 2.0) {
                // More than 1 error for every other term
                LOGERR("splitter::takeword: too many unac errors " <<
                       m_unacerrors << "/"  << m_totalterms << "\n");
                return false;
            }
            return true;
        }

        if (otrm.empty()) {
	    // It may happen in some weird cases that the output from
	    // unac is empty (if the word actually consisted entirely
	    // of diacritics ...)  The consequence is that a phrase
	    // search won't work without addional slack.
            return true;
	}

        // We should have a Japanese stemmer to handle this, but for
        // experimenting, let's do it here: remove 'prolounged sound
        // mark' and its halfwidth variant from the end of terms.
        if ((unsigned int)otrm[0] > 127) {
            Utf8Iter it(otrm);
            if (TextSplit::isKATAKANA(*it)) {
                Utf8Iter itprev = it;
                while (*it != (unsigned int)-1) {
                    itprev = it;
                    it++;
                }
                if (*itprev == 0x30fc || *itprev == 0xff70) {
                    otrm = otrm.substr(0, itprev.getBpos());
                }
            }
        }
        if (otrm.empty()) {
            return true;
        }
        
	// It may also occur that unac introduces spaces in the string
	// (when removing isolated accents, may happen for Greek
	// for example). This is a pathological situation. We
	// index all the resulting terms at the same pos because
	// the surrounding code is not designed to handle a pos
	// change in here. This means that phrase searches and
	// snippets will be wrong, but at least searching for the
	// terms will work.
	bool hasspace = false;
	for (string::const_iterator it = otrm.begin();it < otrm.end();it++) {
	    if (*it == ' ') {
		hasspace=true;
		break;
	    }
	}
	if (hasspace) {
            std::vector<std::string> terms;
	    stringToTokens(otrm, terms, " ", true);
	    for (std::vector<std::string>::const_iterator it = terms.begin(); 
		 it < terms.end(); it++) {
		if (!TermProc::takeword(*it, pos, bs, be)) {
		    return false;
		}
	    }
	    return true;
	} else {
	    return TermProc::takeword(otrm, pos, bs, be);
	}
    }

    virtual bool flush()
    {
        m_totalterms = m_unacerrors = 0;
        return TermProc::flush();
    }

private:
    int m_totalterms;
    int m_unacerrors;
};

/** Compare to stop words list and discard if match found */
class TermProcStop : public TermProc {
public:
    TermProcStop(TermProc *nxt, const Rcl::StopList& stops)
        : TermProc(nxt), m_stops(stops)
    {
    }

    virtual bool takeword(const string& term, int pos, int bs, int be)
    {
        if (m_stops.isStop(term)) {
            return true;
        }
        return TermProc::takeword(term, pos, bs, be);
    }

private:
    const Rcl::StopList& m_stops;
};

/** Handle common-gram generation: combine frequent terms with neighbours to
 *  shorten the positions lists for phrase searches.
 *  NOTE: This does not currently work because of bad interaction with the
 *  spans (ie john@domain.com) generation in textsplit. Not used, kept for
 *  testing only
 */
class TermProcCommongrams : public TermProc {
public:
    TermProcCommongrams(TermProc *nxt, const Rcl::StopList& stops)
        : TermProc(nxt), m_stops(stops), m_onlygrams(false)
    {
    }

    virtual bool takeword(const string& term, int pos, int bs, int be)
    {
        LOGDEB1("TermProcCom::takeword: pos "  << (pos) << " "  << (bs) << " "  << (be) << " ["  << (term) << "]\n" );
        bool isstop = m_stops.isStop(term);
        bool twogramemit = false;

        if (!m_prevterm.empty() && (m_prevstop || isstop)) {
            // create 2-gram. space unnecessary but improves
            // the readability of queries
            string twogram;
            twogram.swap(m_prevterm);
            twogram.append(1, ' ');
            twogram += term;
            // When emitting a complex term we set the bps to 0. This may
            // be used by our clients
            if (!TermProc::takeword(twogram, m_prevpos, 0, 0))
                return false;
            twogramemit = true;
#if 0
            if (m_stops.isStop(twogram)) {
                firstword = twogram;
                isstop = false;
            }
#endif
        }

        m_prevterm = term;
        m_prevstop = isstop;
        m_prevpos = pos;
        m_prevsent = false;
        m_prevbs = bs;
        m_prevbe = be;
        // If flags allow, emit the bare term at the current pos.
        if (!m_onlygrams || (!isstop && !twogramemit)) {
            if (!TermProc::takeword(term, pos, bs, be))
                return false;
            m_prevsent = true;
        }

        return true;
    }

    virtual bool flush()
    {
        if (!m_prevsent && !m_prevterm.empty())
            if (!TermProc::takeword(m_prevterm, m_prevpos, m_prevbs, m_prevbe))
                return false;

        m_prevterm.clear();
        m_prevsent = true;
        return TermProc::flush();
    }
    void onlygrams(bool on)
    {
        m_onlygrams = on;
    }
private:
    // The stoplist we're using
    const Rcl::StopList& m_stops;
    // Remembered data for the last processed term
    string m_prevterm;
    bool   m_prevstop;
    int    m_prevpos;
    int    m_prevbs;
    int    m_prevbe;
    bool   m_prevsent;
    // If this is set, we only emit longest grams
    bool   m_onlygrams;
};


} // End namespace Rcl

#endif /* _TERMPROC_H_INCLUDED_ */

