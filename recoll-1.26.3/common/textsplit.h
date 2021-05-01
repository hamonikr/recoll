/* Copyright (C) 2004 J.F.Dockes
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
#ifndef _TEXTSPLIT_H_INCLUDED_
#define _TEXTSPLIT_H_INCLUDED_

#include <math.h>

#include <string>
#include <vector>

class Utf8Iter;
class RclConfig;

/** 
 * Split text into words. 
 * See comments at top of .cpp for more explanations.
 * This uses a callback function. It could be done with an iterator instead,
 * but 'ts much simpler this way...
 */
class TextSplit {
public:
    enum Flags {
        // Default: will return spans and words (a_b, a, b)
        TXTS_NONE = 0, 
        // Only return maximum spans (a@b.com, not a, b, or com) 
        TXTS_ONLYSPANS = 1,  
        // Special: Only return atomic words (a, b, com).  This is not
        // used for indexing, but for position computation during
        // abstract generation,
        TXTS_NOSPANS = 2,  
        // Handle wildcards as letters. This is used with ONLYSPANS
        // for parsing a user query (never alone).
        TXTS_KEEPWILD = 4 
    };
    
    TextSplit(Flags flags = Flags(TXTS_NONE))
	: m_flags(flags) {}
    virtual ~TextSplit() {}

    /** Call at program initialization to read non default values from the 
        configuration */
    static void staticConfInit(RclConfig *config);
    
    /** Split text, emit words and positions. */
    virtual bool text_to_words(const std::string &in);

    /** Process one output word: to be implemented by the actual user class */
    virtual bool takeword(const std::string& term, 
			  int pos,  // term pos
			  int bts,  // byte offset of first char in term
			  int bte   // byte offset of first char after term
			  ) = 0; 

    /** Called when we encounter formfeed \f 0x0c. Override to use the event.
     * Mostly or exclusively used with pdftoxx output. Other filters mostly 
     * just don't know about pages. */
    virtual void newpage(int /*pos*/) {}

    // Static utility functions:

    /** Count words in string, as the splitter would generate them */
    static int countWords(const std::string &in, Flags flgs = TXTS_ONLYSPANS);

    /** Check if this is visibly not a single block of text */
    static bool hasVisibleWhite(const std::string &in);

    /** Split text span into strings, at white space, allowing for substrings
     * quoted with " . Escaping with \ works as usual inside the quoted areas.
     * This has to be kept separate from smallut.cpp's stringsToStrings, which
     * basically works only if whitespace is ascii, and which processes 
     * non-utf-8 input (iso-8859 config files work ok). This hopefully
     * handles all Unicode whitespace, but needs correct utf-8 input
     */
    static bool stringToStrings(const std::string &s,
                                std::vector<std::string> &tokens);

    /** Is char CJK ? (excluding Katakana) */
    static bool isCJK(int c);
    static bool isKATAKANA(int c);
    static bool isHANGUL(int c);

    /** Statistics about word length (average and dispersion) can
     * detect bad data like undecoded base64 or other mis-identified
     * pieces of data taken as text. In practise, this keeps some junk out 
     * of the index, but does not decrease the index size much, and is
     * probably not worth the trouble in general. Code kept because it
     * probably can be useful in special cases. Base64 data does has
     * word separators in it (+/) and is characterised by high average
     * word length (>10, often close to 20) and high word length
     * dispersion (avg/sigma > 0.8). In my tests, most natural
     * language text has average word lengths around 5-8 and avg/sigma
     * < 0.7
     */
#ifdef TEXTSPLIT_STATS
    class Stats {
    public:
	Stats()
	{
	    reset();
	}
	void reset()
	{
	    count = 0;
	    totlen = 0;
	    sigma_acc = 0;
	}
	void newsamp(unsigned int len)
	{
	    ++count;
	    totlen += len;
	    double avglen = double(totlen) / double(count);
	    sigma_acc += (avglen - len) * (avglen - len);
	}
	struct Values {
	    int count;
	    double avglen;
	    double sigma;
	};
	Values get()
	{
	    Values v;
	    v.count = count;
	    v.avglen = double(totlen) / double(count);
	    v.sigma = sqrt(sigma_acc / count);
	    return v;
	}
    private:
	int count;
	int totlen;
	double sigma_acc;
    };

    Stats::Values getStats()
    {
	return m_stats.get();
    }
    void resetStats()
    {
	m_stats.reset();
    }
#endif // TEXTSPLIT_STATS

private:
    static bool o_processCJK; // true
    static bool o_noNumbers;  // false
    static bool o_deHyphenate; // false
    static unsigned int o_CJKNgramLen; // 2
    static int o_maxWordLength; // 40

    Flags         m_flags;

    // Current span. Might be jf.dockes@wanadoo.f
    std::string        m_span; 

    std::vector <std::pair<int, int> > m_words_in_span;

    // Current word: no punctuation at all in there. Byte offset
    // relative to the current span and byte length
    int           m_wordStart;
    unsigned int  m_wordLen;

    // Currently inside number
    bool          m_inNumber;

    // Term position of current word and span
    int           m_wordpos; 
    int           m_spanpos;

    // It may happen that our cleanup would result in emitting the
    // same term twice. We try to avoid this
    int           m_prevpos{-1};
    int           m_prevlen;

#ifdef TEXTSPLIT_STATS
    // Stats counters. These are processed in TextSplit rather than by a 
    // TermProc so that we can take very long words (not emitted) into
    // account.
    Stats         m_stats;
#endif
    // Word length in characters. Declared but not updated if !TEXTSPLIT_STATS
    unsigned int  m_wordChars;

    void clearsplitstate() {
        m_span.clear();
        m_words_in_span.clear();
        m_inNumber = false;
        m_wordStart = m_wordLen = m_wordpos = m_spanpos = m_prevpos =
            m_prevlen = m_wordChars = 0;
    }

    // This processes cjk text:
    bool cjk_to_words(Utf8Iter *it, unsigned int *cp);

    bool emitterm(bool isspan, std::string &term, int pos, size_t bs,size_t be);
    bool doemit(bool spanerase, size_t bp);
    void discardspan();
    bool span_is_acronym(std::string *acronym);
    bool words_from_span(size_t bp);
};

#endif /* _TEXTSPLIT_H_INCLUDED_ */
