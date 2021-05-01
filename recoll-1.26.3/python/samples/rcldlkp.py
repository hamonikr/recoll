#!/usr/bin/env python
__doc__ = """
An exemple indexer for an arbitrary multi-document file format.
Not supposed to run ''as-is'' or be really useful.

''Lookup'' notes file indexing

The file format has text notes separated by lines with a single '%' character

If the script is called with just the file name as an argument, it will 
(re)index the contents.

If the script is called with second numeric argument, it will retrieve the
specified record and output it in html
"""

import os
import stat
import sys
import re

rclconf = "/Users/dockes/.recoll-dlkp"

def udi(docfile, numrec):
    return docfile + "#" + str(numrec)

###############################################################
def index_rec(db, numrec, rec):
    doc = recoll.Doc()
    # url
    doc.url = "file://" + docfile
    # utf8fn
    # ipath
    doc.ipath = str(numrec)
    # mimetype
    doc.mimetype = "text/plain"
    # mtime
    # origcharset
    # title
    lines = rec.split("\n")
    if len(lines) >= 2:
        doc.title = unicode(lines[1], "iso-8859-1")
    if len(doc.title.strip()) == 0 and len(lines) >= 3:
        doc.title = unicode(lines[2], "iso-8859-1")
    # keywords
    # abstract
    # author
    # fbytes
    doc.fbytes = str(fbytes)
    # text
    doc.text = unicode(rec, "iso-8859-1")
    # dbytes
    doc.dbytes = str(len(rec))
    # sig
    if numrec == 0:
        doc.sig = str(fmtime)
    db.addOrUpdate(udi(docfile, numrec), doc)

def output_rec(rec):
    # Escape html
    rec = unicode(rec, "iso-8859-1").encode("utf-8")
    rec = rec.replace("<", "&lt;");
    rec = rec.replace("&", "&amp;");
    rec = rec.replace('"', "&dquot;");
    print '<html><head>'
    print '<meta http-equiv="Content-Type" content="text/html;charset=UTF-8">'
    print '</head><body><pre>'
    print rec
    print '</pre></body></html>'


################################################################

def usage():
    sys.stderr.write("Usage: rcldlkp.py <filename> [<recnum>]\n")
    exit(1)

if len(sys.argv) < 2:
    usage()

docfile = sys.argv[1]

if len(sys.argv) > 2:
    targetnum = int(sys.argv[2])
else:
    targetnum = None

#print docfile, targetnum

stdata = os.stat(docfile)
fmtime = stdata[stat.ST_MTIME]
fbytes = stdata[stat.ST_SIZE]
f = open(docfile, 'r')

if targetnum == None:
    import recoll
    db = recoll.connect(confdir=rclconf, writable=1)
    if not db.needUpdate(udi(docfile, 0), str(fmtime)):
        exit(0)

rec = ""
numrec = 1
for line in f:
    if re.compile("^%[ \t]*").match(line):
        if targetnum == None:
            index_rec(db, numrec, rec)
        elif targetnum == numrec:
            output_rec(rec)
            exit(0)
        numrec += 1
        rec = ""
    else:
        rec += line

if targetnum == None:
    index_rec(db, 0, "")

