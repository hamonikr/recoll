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
#ifndef _DOCSEQHIST_H_INCLUDED_
#define _DOCSEQHIST_H_INCLUDED_
#include <time.h>

#include <vector>
#include <memory>

#include "docseq.h"
#include "dynconf.h"

namespace Rcl {
    class Db;
}

/** DynConf Document history entry */
class RclDHistoryEntry : public DynConfEntry {
 public:
    RclDHistoryEntry() : unixtime(0) {}
    RclDHistoryEntry(time_t t, const std::string& u, const std::string& d) 
	: unixtime(t), udi(u), dbdir(d) {}
    virtual ~RclDHistoryEntry() {}
    virtual bool decode(const std::string &value);
    virtual bool encode(std::string& value);
    virtual bool equal(const DynConfEntry& other);
    time_t unixtime;
    std::string udi;
    std::string dbdir;
};

/** A DocSequence coming from the history file. 
 *  History is kept as a list of urls. This queries the db to fetch
 *  metadata for an url key */
class DocSequenceHistory : public DocSequence {
 public:
    DocSequenceHistory(std::shared_ptr<Rcl::Db> db, RclDynConf *h,
                       const std::string &t) 
	: DocSequence(t), m_db(db), m_hist(h) {}
    virtual ~DocSequenceHistory() {}

    virtual bool getDoc(int num, Rcl::Doc &doc, std::string *sh = 0);
    virtual int getResCnt();
    virtual std::string getDescription() {return m_description;}
    void setDescription(const std::string& desc) {m_description = desc;}
protected:
    virtual std::shared_ptr<Rcl::Db> getDb() {
        return m_db;
    }
private:
    std::shared_ptr<Rcl::Db> m_db;
    RclDynConf *m_hist;
    time_t      m_prevtime{-1};
    std::string m_description; // This is just an nls translated 'doc history'
    std::vector<RclDHistoryEntry> m_history;
};

extern bool historyEnterDoc(Rcl::Db *db, RclDynConf *dncf, const Rcl::Doc& doc);

#endif /* _DOCSEQ_H_INCLUDED_ */
