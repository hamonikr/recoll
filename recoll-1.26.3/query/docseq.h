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
#ifndef _DOCSEQ_H_INCLUDED_
#define _DOCSEQ_H_INCLUDED_

#include "autoconfig.h"

#include <string>
#include <list>
#include <vector>
#include <mutex>
#include <memory>

#include "rcldoc.h"
#include "hldata.h"

// Need this for the "Snippet" class def.
#include "rclquery.h"

// A result list entry. 
struct ResListEntry {
    Rcl::Doc doc;
    std::string subHeader;
};

/** Sort specification. */
class DocSeqSortSpec {
public:
    DocSeqSortSpec() : desc(false) {}
    bool isNotNull() const {return !field.empty();}
    void reset() {field.erase();}
    std::string field;
    bool   desc;
};

/** Filtering spec. This is only used to filter by doc category for now, hence
    the rather specialized interface */
class DocSeqFiltSpec {
public:
    DocSeqFiltSpec() {}
    enum Crit {DSFS_MIMETYPE, DSFS_QLANG, DSFS_PASSALL};
    void orCrit(Crit crit, const std::string& value) {
        crits.push_back(crit);
        values.push_back(value);
    }
    std::vector<Crit> crits;
    std::vector<std::string> values;
    void reset() {crits.clear(); values.clear();}
    bool isNotNull() const {return crits.size() != 0;}
};

/** Interface for a list of documents coming from some source.

    The result list display data may come from different sources (ie:
    history or Db query), and be post-processed (DocSeqSorted).
    Additional functionality like filtering/sorting can either be
    obtained by stacking DocSequence objects (ie: sorting history), or
    by native capability (ex: docseqdb can sort and filter). The
    implementation might be nicer by using more sophisticated c++ with
    multiple inheritance of sort and filter virtual interfaces, but
    the current one will have to do for now.
*/
class DocSequence {
public:
    DocSequence(const std::string &t) : m_title(t) {}
    virtual ~DocSequence() {}

    /** Get document at given rank. 
     *
     * @param num document rank in sequence
     * @param doc return data
     * @param sh subheader to display before this result (ie: date change 
     *           inside history)
     * @return true if ok, false for error or end of data
     */
    virtual bool getDoc(int num, Rcl::Doc &doc, std::string *sh = 0) = 0;

    /** Get next page of documents. This accumulates entries into the result
     *  list parameter (doesn't reset it). */
    virtual int getSeqSlice(int offs, int cnt, 
                            std::vector<ResListEntry>& result);

    /** Get abstract for document. This is special because it may take time.
     *  The default is to return the input doc's abstract fields, but some 
     *  sequences can compute a better value (ie: docseqdb) */
    virtual bool getAbstract(Rcl::Doc& doc, std::vector<std::string>& abs) {
        abs.push_back(doc.meta[Rcl::Doc::keyabs]);
        return true;
    }
    virtual bool getAbstract(Rcl::Doc& doc, std::vector<Rcl::Snippet>& abs, 
                             int, bool) {
        abs.push_back(Rcl::Snippet(0, doc.meta[Rcl::Doc::keyabs]));
        return true;
    }
    virtual int getFirstMatchPage(Rcl::Doc&, std::string&) {
        return -1;
    }
    /** Get duplicates. */
    virtual bool docDups(const Rcl::Doc&, std::vector<Rcl::Doc>&) {
        return false;
    }

    virtual bool getEnclosing(Rcl::Doc&, Rcl::Doc&);

    /** Get estimated total count in results */
    virtual int getResCnt() = 0;

    /** Get title for result list */
    virtual std::string title() 
        {
            return m_title;
        }

    /** Can do snippets ? */
    virtual bool snippetsCapable()
        {
            return false;
        }
    /** Get description for underlying query */
    virtual std::string getDescription() = 0;

    /** Get search terms (for highlighting abstracts). Some sequences
     * may have no associated search terms. Implement this for them. */
    virtual void getTerms(HighlightData& hld)                     
        {
            hld.clear();
        }
    virtual std::list<std::string> expand(Rcl::Doc &) 
        {
            return std::list<std::string>();
        }
    virtual std::string getReason() 
        {
            return m_reason;
        }
    /** Optional functionality. */
    virtual bool canFilter() {return false;}
    virtual bool canSort() {return false;}
    virtual bool setFiltSpec(const DocSeqFiltSpec &) {return false;}
    virtual bool setSortSpec(const DocSeqSortSpec &) {return false;}
    virtual std::shared_ptr<DocSequence> getSourceSeq() {
        return std::shared_ptr<DocSequence>();}

    static void set_translations(const std::string& sort,
                                 const std::string& filt) {
        o_sort_trans = sort;
        o_filt_trans = filt;
    }


protected:
    friend class DocSeqModifier;
    virtual std::shared_ptr<Rcl::Db> getDb() = 0;
    static std::mutex o_dblock;
    static std::string o_sort_trans;
    static std::string o_filt_trans;
    std::string          m_reason;

private:
    std::string          m_title;
};

/** A modifier has a child sequence which does the real work and does
 * something with the results. Some operations are just delegated
 */
class DocSeqModifier : public DocSequence {
public:
    DocSeqModifier(std::shared_ptr<DocSequence> iseq) 
        : DocSequence(""), m_seq(iseq) 
        {}
    virtual ~DocSeqModifier() {}

    virtual bool getAbstract(Rcl::Doc& doc, std::vector<std::string>& abs)
        override{
        if (!m_seq)
            return false;
        return m_seq->getAbstract(doc, abs);
    }
    virtual bool getAbstract(Rcl::Doc& doc, std::vector<Rcl::Snippet>& abs,
                             int maxlen, bool bypage) override {
        if (!m_seq)
            return false;
        return m_seq->getAbstract(doc, abs, maxlen, bypage);
    }
    /** Get duplicates. */
    virtual bool docDups(const Rcl::Doc& doc, std::vector<Rcl::Doc>& dups)
        override {
        if (!m_seq)
            return false;
        return m_seq->docDups(doc, dups);
    }

    virtual bool snippetsCapable() override {
        if (!m_seq)
            return false;
        return m_seq->snippetsCapable();
    }
    virtual std::string getDescription() override {
        if (!m_seq)
            return "";
        return m_seq->getDescription();
    }
    virtual void getTerms(HighlightData& hld) override {
        if (!m_seq)
            return;
        m_seq->getTerms(hld);
    }
    virtual bool getEnclosing(Rcl::Doc& doc, Rcl::Doc& pdoc) override {
        if (!m_seq)
            return false;
        return m_seq->getEnclosing(doc, pdoc);
    }
    virtual std::string getReason() override {
        if (!m_seq)
            return string();
        return m_seq->getReason();
    }
    virtual std::string title() override {
        return m_seq->title();
    }
    virtual std::shared_ptr<DocSequence> getSourceSeq() override {
        return m_seq;
    }

protected:
    virtual std::shared_ptr<Rcl::Db> getDb() override {
        if (!m_seq)
            return 0;
        return m_seq->getDb();
    }

    std::shared_ptr<DocSequence>    m_seq;
};

class RclConfig;

// A DocSource can juggle docseqs of different kinds to implement
// sorting and filtering in ways depending on the base seqs capabilities
class DocSource : public DocSeqModifier {
public:
    DocSource(RclConfig *config, std::shared_ptr<DocSequence> iseq) 
        : DocSeqModifier(iseq), m_config(config)
        {}
    virtual bool canFilter() {return true;}
    virtual bool canSort() {return true;}
    virtual bool setFiltSpec(const DocSeqFiltSpec &);
    virtual bool setSortSpec(const DocSeqSortSpec &);
    virtual bool getDoc(int num, Rcl::Doc &doc, std::string *sh = 0) {
        if (!m_seq)
            return false;
        return m_seq->getDoc(num, doc, sh);
    }
    virtual int getResCnt() {
        if (!m_seq)
            return 0;
        return m_seq->getResCnt();
    }
    virtual std::string title();
private:
    bool buildStack();
    void stripStack();
    RclConfig *m_config;
    DocSeqFiltSpec  m_fspec;
    DocSeqSortSpec  m_sspec;
};

#endif /* _DOCSEQ_H_ */
