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
#ifndef _DOCSEQDB_H_INCLUDED_
#define _DOCSEQDB_H_INCLUDED_

#include <memory>

#include "docseq.h"
#include "searchdata.h"
#include "rclquery.h"

/** A DocSequence from a Db query */
class DocSequenceDb : public DocSequence {
 public:
    DocSequenceDb(std::shared_ptr<Rcl::Db> db,
                  std::shared_ptr<Rcl::Query> q, const std::string &t, 
		  std::shared_ptr<Rcl::SearchData> sdata);
    virtual ~DocSequenceDb() {}
    virtual bool getDoc(int num, Rcl::Doc &doc, std::string * = 0) override;
    virtual int getResCnt() override;
    virtual void getTerms(HighlightData& hld) override;

    // Called to fill-up the snippets window. Ignoers
    // buildabstract/replaceabstract and syntabslen
    virtual bool getAbstract(Rcl::Doc &doc, std::vector<Rcl::Snippet>&,
                             int maxlen, bool sortbypage) override;

    virtual bool getAbstract(Rcl::Doc &doc, std::vector<std::string>&) override;
    virtual int getFirstMatchPage(Rcl::Doc&, std::string& term) override;
    virtual bool docDups(const Rcl::Doc& doc, std::vector<Rcl::Doc>& dups)
        override;
    virtual std::string getDescription() override;
    virtual std::list<std::string> expand(Rcl::Doc &doc) override;
    virtual bool canFilter()  override {return true;}
    virtual bool setFiltSpec(const DocSeqFiltSpec &filtspec) override;
    virtual bool canSort()  override {return true;} 
    virtual bool setSortSpec(const DocSeqSortSpec &sortspec) override;
    virtual void setAbstractParams(bool qba, bool qra) {
        m_queryBuildAbstract = qba;
        m_queryReplaceAbstract = qra;
    }

    virtual bool snippetsCapable() override {
	return true;
    }
    virtual std::string title() override;

protected:
    virtual std::shared_ptr<Rcl::Db> getDb() override {
        return m_db;
    }
private:
    std::shared_ptr<Rcl::Db>         m_db;
    std::shared_ptr<Rcl::Query>      m_q;
    std::shared_ptr<Rcl::SearchData> m_sdata;
    std::shared_ptr<Rcl::SearchData> m_fsdata; // Filtered 
    int                      m_rescnt;
    bool                     m_queryBuildAbstract;
    bool                     m_queryReplaceAbstract;
    bool                     m_isFiltered;
    bool                     m_isSorted;
    bool   m_needSetQuery; // search data changed, need to reapply before fetch
    bool   m_lastSQStatus;
    bool setQuery();
};

#endif /* _DOCSEQDB_H_INCLUDED_ */
