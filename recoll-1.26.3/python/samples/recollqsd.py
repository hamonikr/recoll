#!/usr/bin/env python
"""Example for using the ''searchdata''' structured query interface.
Not good for anything except showing/trying the code."""

import sys
from recoll import recoll

def dotest(db, q):
    query = db.query()
    query.sortby("title", 1)

    nres = query.executesd(q)
    print "Result count: ", nres
    print "Query: ", query.getxquery().encode('utf-8')
    if nres > 10:
        nres = 10
    for i in range(nres):
        doc = query.fetchone()
        print query.next if type(query.next) == int else query.rownumber
        for k in ("url", "mtime", "title", "author", "abstract"):
            if getattr(doc, k):
                print k, ":", getattr(doc, k).encode('utf-8')
            else:
                print k, ": None"
        print
# End dotest

# addclause(type='and'|'or'|'filename'|'phrase'|'near'|'path'|'sub'
#          qstring=string, slack=int, field=string, stemming=1|0,
#          subSearch=SearchData, exclude=0|1, anchorstart=0|1, anchorend=0|1
#          casesens=0|1, diacsens=0|1)

#sd.addclause("and", "dockes", field="author")
#sd.addclause("phrase", "jean francois", 1)

db = recoll.connect(confdir="/home/dockes/.recoll-prod")

sd = recoll.SearchData(stemlang="english")
sd.addclause('filename', "recollqsd*")

dotest(db, sd)

sys.exit(0)
