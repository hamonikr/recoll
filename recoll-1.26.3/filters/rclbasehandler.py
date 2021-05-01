#!/usr/bin/env python3
# Copyright (C) 2016 J.F.Dockes
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# Base for extractor classes. With some common generic implementations
# for the boilerplate functions.

from __future__ import print_function

import os
import sys
import rclexecm

class RclBaseHandler(object):
    '''Base Object for simple extractors.

    This implements the boilerplate code for simple extractors for
    file types with a single document. The derived class would
    typically need only to implement the html_text method to return
    the document text in HTML format'''
    
    def __init__(self, em):
        self.em = em


    def extractone(self, params):
        #self.em.rclog("extractone fn %s mt %s" % (params["filename:"], \
        #                                          params["mimetype:"]))
        if not "filename:" in params:
            self.em.rclog("extractone: no file name")
            return (False, "", "", rclexecm.RclExecM.eofnow)
        fn = params["filename:"]

        if "mimetype:" in params:
            self.inputmimetype = params["mimetype:"]
        else:
            self.inputmimetype = None

        self.outputmimetype = 'text/html'
        try:
            # Note: "html_text" can change self.outputmimetype and
            # output text/plain
            html = self.html_text(fn)
        except Exception as err:
            #import traceback
            #traceback.print_exc()
            self.em.rclog("%s : %s" % (fn, err))
            return (False, "", "", rclexecm.RclExecM.eofnow)

        self.em.setmimetype(self.outputmimetype)
        return (True, html, "", rclexecm.RclExecM.eofnext)
        

    ###### File type handler api, used by rclexecm ---------->
    def openfile(self, params):
        self.currentindex = 0
        return True


    def getipath(self, params):
        return self.extractone(params)


    def getnext(self, params):
        if self.currentindex >= 1:
            return (False, "", "", rclexecm.RclExecM.eofnow)
        else:
            ret= self.extractone(params)
            self.currentindex += 1
            return ret
