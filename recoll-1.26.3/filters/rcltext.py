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

# Wrapping a text file. Recoll does it internally in most cases, but
# this is for use by another filter.

from __future__ import print_function

import rclexecm
import sys
from rclbasehandler import RclBaseHandler

class TxtDump(RclBaseHandler):
    def __init__(self, em):
        super(TxtDump, self).__init__(em)

    def html_text(self, fn):
        # No charset, so recoll will have to use its config to guess it
        html = b'<html><head><title></title></head><body><pre>'
        with open(fn, "rb") as f:
            html += self.em.htmlescape(f.read())
        html += b'</pre></body></html>'
        return html

if __name__ == '__main__':
    proto = rclexecm.RclExecM()
    extract = TxtDump(proto)
    rclexecm.main(proto, extract)
