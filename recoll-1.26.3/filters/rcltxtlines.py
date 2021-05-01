#!/usr/bin/env python3
"""Index text lines as document (execm handler sample). This exists
to demonstrate the execm interface and is not meant to be useful or
efficient"""
from __future__ import print_function

import sys
import os

import rclexecm

# Here try to import your document module if you need one. There is
# not much risk of 'sys' missing, but this shows what you should do if
# something is not there: the data will go to the 'missing' file, which
# can be displayed by the GUI as a list of MIME type and missing
# helpers.
try:
    import sys
except:
    print("RECFILTERROR HELPERNOTFOUND python3:sys")
    sys.exit(1);

# Our class.
class rclTXTLINES:
    def __init__(self, em):
        # Store a ref to our execm object so that we can use its services.
        self.em = em

    # This is called once for every processed file during indexing, or
    # query preview. For multi-document files, it usually creates some
    # kind of table of contents, and resets the current index in it,
    # because we don't know at this point if this is for indexing
    # (will walk all entries) or previewing (will request
    # one). Actually we could know from the environment but it's just
    # simpler this way in general. Note that there is no close call,
    # openfile() will just be called repeatedly during indexing, and
    # should clear any existing state
    def openfile(self, params):
        """Open the text file, create a contents array"""
        self.currentindex = -1
        try:
            f = open(params["filename:"].decode('UTF-8'), "r")
        except Exception as err:
            self.em.rclog("openfile: open failed: [%s]" % err)
            return False
        self.lines = f.readlines()
        return True

    # This is called during indexing to walk the contents. The first
    # time, we return a 'self' document, which may be empty (e.g. for
    # a tar file), or might contain data (e.g. for an email body,
    # further docs being the attachments), and may also be the only
    # document returned (for single document files).
    def getnext(self, params):

        # Self doc. Here empty.
        #
        # This could also be the only entry if this file type holds a
        # single document. We return eofnext in this case
        #
        # !Note that the self doc has an *empty* ipath
        if self.currentindex == -1:
            self.currentindex = 0
            if len(self.lines) == 0:
                eof = rclexecm.RclExecM.eofnext
            else:
                eof = rclexecm.RclExecM.noteof
            return (True, "", "", eof)


        if self.currentindex >= len(self.lines):
            return (False, "", "", rclexecm.RclExecM.eofnow)
        else:
            ret= self.extractone(self.currentindex)
            self.currentindex += 1
            return ret

    # This is called for query preview to request one specific (or the
    # only) entry. Here our internal paths are stringified line
    # numbers, but they could be tar archive paths or whatever we
    # returned during indexing.
    def getipath(self, params):
        return self.extractone(int(params["ipath:"]))

    # Most handlers factorize common code from getipath() and
    # getnext() in an extractone() method, but this is not part of the
    # interface.
    def extractone(self, lno):
        """Extract one line from the text file"""

        # Need to specify the MIME type here. This would not be
        # necessary if the ipath was a file name with a usable
        # extension.
        self.em.setmimetype("text/plain")

        # Warning of upcoming eof saves one roundtrip
        iseof = rclexecm.RclExecM.noteof
        if lno == len(self.lines) - 1:
            iseof = rclexecm.RclExecM.eofnext

        try:
            # Return the doc data and internal path (here stringified
            # line number). If we're called from getipath(), the
            # returned ipath is not that useful of course.
            return (True, self.lines[lno], str(lno), iseof)
        except Exception as err:
            self.em.rclog("extractone: failed: [%s]" % err)
            return (False, "", lno, iseof)


# Initialize: create our protocol handler, the filetype-specific
# object, link them and run.
proto = rclexecm.RclExecM()
extract = rclTXTLINES(proto)
rclexecm.main(proto, extract)
