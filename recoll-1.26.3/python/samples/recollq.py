#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""A python version of the command line query tool recollq (a bit simplified)
The input string is always interpreted as a query language string.
This could actually be useful for something after some customization
"""

import sys
import locale
from getopt import getopt

if sys.version_info[0] >= 3:
    ISP3 = True
else:
    ISP3 = False

try:
    from recoll import recoll
    from recoll import rclextract
    hasextract = True
except:
    import recoll
    hasextract = False
    
allmeta = ("title", "keywords", "abstract", "url", "mimetype", "mtime",
           "ipath", "fbytes", "dbytes", "relevancyrating")

def Usage():
    print("Usage: recollq.py [-c conf] [-i extra_index] <recoll query>")
    sys.exit(1);

class ptrmeths:
    def __init__(self, groups):
        self.groups = groups
    def startMatch(self, idx):
        ugroup = " ".join(self.groups[idx][1])
        return '<span class="pyrclstart" idx="%d" ugroup="%s">' % (idx, ugroup)
    def endMatch(self):
        return '</span>'
    
def extract(doc):
    extractor = rclextract.Extractor(doc)
    newdoc = extractor.textextract(doc.ipath)
    return newdoc

def extractofile(doc, outfilename=""):
    extractor = rclextract.Extractor(doc)
    outfilename = extractor.idoctofile(doc.ipath, doc.mimetype, \
                                       ofilename=outfilename)
    return outfilename

def utf8string(s):
    if ISP3:
        return s
    else:
        return s.encode('utf8')
    
def doquery(db, q):
    # Get query object
    query = db.query()
    #query.sortby("dmtime", ascending=True)

    # Parse/run input query string
    nres = query.execute(q, stemming = 0, stemlang="english")
    qs = "Xapian query: [%s]" % query.getxquery()
    print(utf8string(qs))
    groups = query.getgroups()
    m = ptrmeths(groups)

    # Print results:
    print("Result count: %d %d" % (nres, query.rowcount))
    if nres > 20:
        nres = 20
    #results = query.fetchmany(nres)
    #for doc in results:

    for i in range(nres):
        doc = query.fetchone()
        rownum = query.next if type(query.next) == int else \
                 query.rownumber
        print("%d:"%(rownum,))
        #for k,v in doc.items().items():
        #print "KEY:", utf8string(k), "VALUE", utf8string(v)
        #continue
        #outfile = extractofile(doc)
        #print "outfile:", outfile, "url", utf8string(doc.url)
        for k in ("title", "mtime", "author"):
            value = getattr(doc, k)
#            value = doc.get(k)
            if value is None:
                print("%s: (None)"%(k,))
            else:
                print("%s : %s"%(k, utf8string(value)))
        #doc.setbinurl(bytearray("toto"))
        #burl = doc.getbinurl(); print("Bin URL : [%s]"%(doc.getbinurl(),))
        abs = query.makedocabstract(doc, methods=m)
        print(utf8string(abs))
        print('')
#        fulldoc = extract(doc)
#        print "FULLDOC MIMETYPE", fulldoc.mimetype, "TEXT:", fulldoc.text.encode("utf-8")


########################################### MAIN

if len(sys.argv) < 2:
    Usage()

language, localecharset = locale.getdefaultlocale()
confdir=""
extra_dbs = []
# Snippet params
maxchars = 120
contextwords = 4
# Process options: [-c confdir] [-i extra_db [-i extra_db] ...]
options, args = getopt(sys.argv[1:], "c:i:")
for opt,val in options:
    if opt == "-c":
        confdir = val
    elif opt == "-i":
        extra_dbs.append(val)
    else:
        print("Bad opt: %s"%(opt,))
        Usage()

# The query should be in the remaining arg(s)
if len(args) == 0:
    print("No query found in command line")
    Usage()
q = ''
for word in args:
    q += word + ' '

print("QUERY: [%s]"%(q,))
db = recoll.connect(confdir=confdir, extra_dbs=extra_dbs)
db.setAbstractParams(maxchars=maxchars, contextwords=contextwords)

doquery(db, q)
