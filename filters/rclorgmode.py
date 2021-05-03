#!/usr/bin/env python3
from __future__ import print_function

# Read an org-mode file, break it into "documents" along the separator lines
# and interface with recoll execm

import rclexecm
import sys
import re

class OrgModeExtractor:
    def __init__(self, em):
        self.file = ""
        self.contents = []
        self.em = em

    def extractone(self, index):
        if index >= len(self.docs):
            return(False, "", "", True)
        docdata = self.docs[index]
        #self.em.rclog(docdata)

        iseof = rclexecm.RclExecM.noteof
        if self.currentindex >= len(self.docs) -1:
            iseof = rclexecm.RclExecM.eofnext
        self.em.setmimetype("text/plain")
        try:
            self.em.setfield("title", docdata.splitlines()[0])
        except:
            pass
        return (True, docdata, str(index), iseof)

    ###### File type handler api, used by rclexecm ---------->
    def openfile(self, params):
        self.file = params["filename"]

        try:
            data = open(self.file, "rb").read()
        except Exception as e:
            self.em.rclog("Openfile: open: %s" % str(e))
            return False

        self.currentindex = -1

        res = rb'''^\* '''
        self.docs = re.compile(res, flags=re.MULTILINE).split(data)
        self.docs = self.docs[1:]
        #self.em.rclog("openfile: Entry count: %d" % len(self.docs))
        return True

    def getipath(self, params):
        try:
            if params["ipath"] == b'':
                index = 0
            else:
                index = int(params["ipath"])
        except:
            return (False, "", "", True)
        return self.extractone(index)
        
    def getnext(self, params):

        if self.currentindex == -1:
            # Return "self" doc
            self.currentindex = 0
            self.em.setmimetype(b'text/plain')
            if len(self.docs) == 0:
                eof = rclexecm.RclExecM.eofnext
            else:
                eof = rclexecm.RclExecM.noteof
            return (True, "", "", eof)

        if self.currentindex >= len(self.docs):
            self.em.rclog("getnext: EOF hit")
            return (False, "", "", rclexecm.RclExecM.eofnow)
        else:
            ret= self.extractone(self.currentindex)
            self.currentindex += 1
            return ret


proto = rclexecm.RclExecM()
extract = OrgModeExtractor(proto)
rclexecm.main(proto, extract)
