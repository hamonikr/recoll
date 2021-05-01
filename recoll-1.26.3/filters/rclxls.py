#!/usr/bin/env python3

# Extractor for Excel files.

import rclexecm
import rclexec1
import xlsxmltocsv
import re
import sys
import os
import xml.sax

class XLSProcessData:
    def __init__(self, em, ishtml = False):
        self.em = em
        self.out = []
        self.gotdata = 0
        self.xmldata = []
        self.ishtml = ishtml
        
    def takeLine(self, line):
        if not line:
            return
        if self.ishtml:
            self.out.append(line)
            return
        if not self.gotdata:
            self.out.append(b'''<html><head>''' + \
                        b'''<meta http-equiv="Content-Type" ''' + \
                        b'''content="text/html;charset=UTF-8">''' + \
                        b'''</head><body><pre>''')
            self.gotdata = True
        self.xmldata.append(line)

    def wrapData(self):
        if not self.gotdata:
            raise Exception("xls-dump returned no data")
            return b''
        if self.ishtml:
            return b'\n'.join(self.out)
        handler =  xlsxmltocsv.XlsXmlHandler()
        xml.sax.parseString(b'\n'.join(self.xmldata), handler)
        self.out.append(self.em.htmlescape(b'\n'.join(handler.output)))
        return b'\n'.join(self.out) + b'</pre></body></html>'

class XLSFilter:
    def __init__(self, em):
        self.em = em
        self.ntry = 0

    def reset(self):
        self.ntry = 0
        pass
            
    def getCmd(self, fn):
        if self.ntry:
            return ([], None)
        self.ntry = 1
        # Some HTML files masquerade as XLS
        try:
            data = open(fn, 'rb').read(512)
            if data.find(b'html') != -1 or data.find(b'HTML') != -1:
                return ("cat", XLSProcessData(self.em, True))
        except Exception as err:
            self.em.rclog("Error reading %s:%s" % (fn, str(err)))
            pass
        cmd = rclexecm.which("xls-dump.py")
        if cmd:
            # xls-dump.py often exits 1 with valid data. Ignore exit value
            # We later treat an empty output as an error
            return ([sys.executable, cmd, "--dump-mode=canonical-xml", \
                     "--utf-8", "--catch"],
                    XLSProcessData(self.em), rclexec1.Executor.opt_ignxval)
        else:
            return ([], None)

if __name__ == '__main__':
    if not rclexecm.which("xls-dump.py"):
        print("RECFILTERROR HELPERNOTFOUND ppt-dump.py")
        sys.exit(1)
    proto = rclexecm.RclExecM()
    filter = XLSFilter(proto)
    extract = rclexec1.Executor(proto, filter)
    rclexecm.main(proto, extract)
