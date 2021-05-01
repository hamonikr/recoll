#!/usr/bin/env python
from __future__ import print_function

import sys
import xapian

o_index_stripchars = True
md5wpref = "XM"

# Handle caps/diac-stripping option. If the db is raw the prefixes are
# wrapped with ":"
def wrap_prefix(prefix):
    if o_index_stripchars:
        return prefix
    else:
        return ":" + prefix + ":"

def init_stripchars(xdb):
    global o_index_stripchars
    global md5wpref
    t = xdb.allterms()
    t.skip_to(":")
    for term in t:
        if term.term.find(":") == 0:
            o_index_stripchars = False
        break
    md5wpref = wrap_prefix("XM")
    

# Retrieve named value from document data record.
# The record format is a sequence of nm=value lines
def get_attribute(xdb, docid, fld):
    doc = xdb.get_document(docid)
    data = doc.get_data()
    s = data.find(fld+"=")
    if s == -1:
        return ""
    e = data.find("\n", s)
    return data[s+len(fld)+1:e]

# Convenience: retrieve postings as Python list
def get_postlist(xdb, term):
    ret = list()
    for posting in xdb.postlist(term):
        ret.append(posting.docid)
    return ret
    
# Return list of docids having same md5 including self
def get_dups(xdb, docid):
    doc = xdb.get_document(int(docid))

    # It would be more efficient to retrieve the value, but it's
    # binary so we'd have to decode it
    md5term = doc.termlist().skip_to(md5wpref).term
    if not md5term.startswith(md5wpref):
        return

    posts = get_postlist(xdb, md5term)
    return posts

# Retrieve all sets of duplicates:
#   walk the list of all MD5 terms, look up their posting lists, and
#   store the docids where the list is longer than one.
def find_all_dups(xdb):
    alldups = list()

    # Walk the MD5 terms
    t = xdb.allterms()
    t.skip_to(md5wpref)
    for term in t:
        if not term.term.startswith(md5wpref):
            break
        # Check postlist for term, if it's not of length 1, we have a dup
        dups = get_postlist(xdb, term.term)
        if len(dups) != 1:
            alldups.append(dups)
    return alldups

# Print docid url ipath for list of docids
def print_urlipath(xdb, doclist):
    for docid in doclist:
        url = get_attribute(xdb, docid, "url")
        ipath = get_attribute(xdb, docid, "ipath")
        print("%s %s %s" % (docid, url, ipath))

def msg(s):
    print("%s" % s, file = sys.stderr)
    
########## Main program

if len(sys.argv) < 2:
    msg("Usage: %s /path/to/db [docid [docid ...]]" % \
          sys.argv[0])
    msg(" will print all sets of dups if no docid is given")
    msg(" else only the duplicates for the given docids")
    
    sys.exit(1)

xdbpath = sys.argv[1]
xdb = xapian.Database(xdbpath)

init_stripchars(xdb)

try:
    
    if len(sys.argv) == 2:
        # No docid args, 
        alldups = find_all_dups(xdb)
        for dups in alldups:
            print_urlipath(xdb, dups)
            print("")
    else:
        for docid in sys.argv[2:]:
            dups = get_dups(xdb, docid)
            if dups is not None and len(dups) > 1:
                print_urlipath(xdb, dups)
                
except Exception as e:
    msg("Xapian error: %s" % str(e))
    sys.exit(1)
