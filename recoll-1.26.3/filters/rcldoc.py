#!/usr/bin/env python3
from __future__ import print_function

import rclexecm
import rclexec1
import re
import sys
import os

# Processing the output from antiword: create html header and tail, process
# continuation lines escape, HTML special characters, accumulate the data.
class WordProcessData:
    def __init__(self, em):
        self.em = em
        self.out = []
        self.cont = b''
        self.gotdata = False
        # Line with continued word (ending in -)
        # we strip the - which is not nice for actually hyphenated word.
        # What to do ?
        self.patcont = re.compile(b'''[\w][-]$''')
        # Pattern for breaking continuation at last word start
        self.patws = re.compile(b'''([\s])([\w]+)(-)$''')

    def takeLine(self, line):
        if not self.gotdata:
            if line == b'':
                return
            self.out.append(b'<html><head><title></title>' + \
                       b'<meta http-equiv="Content-Type"' + \
                       b'content="text/html;charset=UTF-8">' + \
                       b'</head><body><p>')
            self.gotdata = True

        if self.cont:
            line = self.cont + line
            self.cont = ""

        if line == b'\f':
            self.out.append('</p><hr><p>')
            return

        if self.patcont.search(line):
            # Break at last whitespace
            match = self.patws.search(line)
            if match:
                self.cont = line[match.start(2):match.end(2)]
                line = line[0:match.start(1)]
            else:
                self.cont = line
                line = b''

        if line:
            self.out.append(self.em.htmlescape(line) + b'<br>')
        else:
            self.out.append(b'<br>')

    def wrapData(self):
        if self.gotdata:
            self.out.append(b'</p></body></html>')
        self.em.setmimetype("text/html")
        return b'\n'.join(self.out)

# Null data accumulator. We use this when antiword has failed, and the
# data actually comes from rclrtf, rcltext or vwWare, which all
# output HTML
class WordPassData:
    def __init__(self, em):
        self.out = []
        self.em = em

    def takeLine(self, line):
        self.out.append(line)

    def wrapData(self):
        self.em.setmimetype("text/html")
        return b'\n'.join(self.out)
        

# Filter for msword docs. Try antiword, and if this fails, check for
# an rtf or text document (.doc are sometimes like this...). Also try
# vwWare if the doc is actually a word doc
class WordFilter:
    def __init__(self, em, td):
        self.em = em
        self.ntry = 0
        self.execdir = td
        self.rtfprolog = b'{\\rtf1'
        self.docprolog = b'\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1'
        
    def reset(self):
        self.ntry = 0
            
    def hasControlChars(self, data):
        for c in data:
            if c < b' '[0] and c != b'\n'[0] and c != b'\t'[0] and \
                   c !=  b'\f'[0] and c != b'\r'[0]:
                return True
        return False

    def mimetype(self, fn):
        try:
            f = open(fn, "rb")
        except:
            return ""
        data = f.read(100)
        if data[0:6] == self.rtfprolog:
            return "text/rtf"
        elif data[0:8] == self.docprolog:
            return "application/msword"
        elif self.hasControlChars(data):
            return "application/octet-stream"
        else:
            return "text/plain"

    def getCmd(self, fn):
        '''Return command to execute, and postprocessor, according to
        our state: first try antiword, then others depending on mime
        identification. Do 2 tries at most'''
        if self.ntry == 0:
            self.ntry = 1
            cmd = rclexecm.which("antiword")
            if cmd:
                return ([cmd, "-t", "-i", "1", "-m", "UTF-8"],
                        WordProcessData(self.em))
            else:
                return ([],None)
        elif self.ntry == 1:
            self.ntry = 2
            # antiword failed. Check for an rtf file, or text and
            # process accordingly. It the doc is actually msword, try
            # wvWare.
            mt = self.mimetype(fn)
            self.em.rclog("rcldoc.py: actual MIME type %s" % mt)
            if mt == "text/plain":
                return ([sys.executable, os.path.join(self.execdir, "rcltext.py")],
                       WordPassData(self.em))
            elif mt == "text/rtf":
                cmd = [sys.executable, os.path.join(self.execdir, "rclrtf.py"),
                       "-s"]
                self.em.rclog("rcldoc.py: returning cmd %s" % cmd)
                return (cmd, WordPassData(self.em))
            elif mt == "application/msword":
                cmd = rclexecm.which("wvWare")
                if cmd:
                    return ([cmd, "--nographics", "--charset=utf-8"],
                            WordPassData(self.em))
                else:
                    return ([],None)    
            else:
                return ([],None)
        else:
            return ([],None)

if __name__ == '__main__':
    # Remember where we execute filters from, in case we need to exec another
    execdir = os.path.dirname(sys.argv[0])
    # Check that we have antiword. We could fallback to wvWare, but
    # this is not what the old filter did.
    if not rclexecm.which("antiword"):
        print("RECFILTERROR HELPERNOTFOUND antiword")
        sys.exit(1)
    proto = rclexecm.RclExecM()
    filter = WordFilter(proto, execdir)
    extract = rclexec1.Executor(proto, filter)
    rclexecm.main(proto, extract)
