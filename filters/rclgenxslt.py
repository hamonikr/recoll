#!/usr/bin/env python3
# Copyright (C) 2018 J.F.Dockes
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the
#   Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
######################################

# Base class for simple (one stylesheet) xslt-based handlers

from __future__ import print_function

import sys
import rclxslt
import gzip
from rclbasehandler import RclBaseHandler

class XSLTExtractor(RclBaseHandler):
    def __init__(self, em, stylesheet, gzip=False):
        super(XSLTExtractor, self).__init__(em)
        self.stylesheet = stylesheet
        self.dogz = gzip

    def html_text(self, fn):
        if self.dogz:
            data = gzip.open(fn, 'rb').read()
        else:
            data = open(fn, 'rb').read()
        return rclxslt.apply_sheet_data(self.stylesheet, data)
