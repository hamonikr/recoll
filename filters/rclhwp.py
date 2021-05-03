#!/usr/bin/python3
# Copyright (C) 2020 J.F.Dockes
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
#########################################
# Recoll Hanword .hwp handler
#
# The real work is done by pyhwp:
#   https://github.com/mete0r/pyhwp
#   https://pypi.org/project/pyhwp/
# pip3 install pyhwp
#

import sys
import os
from io import BytesIO
import subprocess

import rclexecm
from rclbasehandler import RclBaseHandler

from hwp5.filestructure import Hwp5File as fs_Hwp5File
from hwp5.transforms import BaseTransform
from hwp5.xmlmodel import Hwp5File as xml_Hwp5File
from hwp5.utils import cached_property

# Associate HTML meta names and hwp summaryinfo values
def metafields(summaryinfo):
    yield(('Description', summaryinfo.subject + " " +
           summaryinfo.comments))
    yield(('Author', summaryinfo.author))
    yield(('Keywords', summaryinfo.keywords))
    yield(('Date', summaryinfo.lastSavedTime))


# Extractor class. We use hwp summaryinfo to extract metadata and code
# extracted from hwp.hwp5txt.py to extract the text.
class HWP5Dump(RclBaseHandler):
    def __init__(self, em):
        super(HWP5Dump, self).__init__(em)

    def html_text(self, fn):
        # hwp wants str filenames. This is unfortunate
        fn = fn.decode('utf-8')
        try:
            hwpfile = fs_Hwp5File(fn)
        except Exception as ex:
            self.em.rclog("hwpfile open failed: %s" % ex)
            raise ex
        try:
            tt = hwpfile.summaryinfo.title.strip()
            if tt:
                tt = rclexecm.htmlescape(tt.encode('utf-8'))
                self.em.setfield('caption', tt)

            for k,v in metafields(hwpfile.summaryinfo):
                v = "{0}".format(v)
                v = v.strip()
                if v:
                    v = rclexecm.htmlescape(v.encode('utf-8'))
                    k = k.encode('utf-8')
                    self.em.setfield(k, v)
        except Exception as e:
            self.em.rclog("Exception: %s" % e)
        finally:
            hwpfile.close()

        # The first version of this file used conversion to text using
        # the hwp5 module (no subproc). But this apparently mishandled
        # tables. Switched to executing hwp5html instead. See 1st git
        # version for the old approach.
        return rclexecm.execPythonScript(["hwp5html", "--html", fn])

if __name__ == '__main__':
    proto = rclexecm.RclExecM()
    extract = HWP5Dump(proto)
    rclexecm.main(proto, extract)
