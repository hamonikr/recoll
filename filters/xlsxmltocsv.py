#!/usr/bin/env python3
# Copyright (C) 2015 J.F.Dockes
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

# Transform XML output from xls-dump.py into csv format.
#
# Note: this would be difficult to make compatible with python 3 <=
# 3.4 because of the use of % interpolation on what should be bytes.
# # % terpolation for bytes is available as of python 3.5, which is
# the minimum version supported.


from __future__ import print_function

import sys
import xml.sax

dtt = True

if dtt:
    sepstring = b"\t"
    dquote = b""
else:
    sepstring = b","
    dquote = b'"'

class XlsXmlHandler(xml.sax.handler.ContentHandler):
    def __init__(self):
        self.output = []
        
    def startElement(self, name, attrs):
        if name == "worksheet":
            if "name" in attrs:
                self.output.append(b"%s\n" % attrs["name"].encode("UTF-8"))
        elif name == "row":
            self.cells = dict()
        elif name == "label-cell" or name == "number-cell":
            if "value" in attrs:
                value = attrs["value"].encode("UTF-8")
            else:
                value = b''
            if "col" in attrs:
                self.cells[int(attrs["col"])] = value
            else:
                #??
                self.output.append(b"%s%s" % (value.encode("UTF-8"), sepstring))
        elif name == "formula-cell":
            if "formula-result" in attrs and "col" in attrs:
                self.cells[int(attrs["col"])] = \
                             attrs["formula-result"].encode("UTF-8")
            
    def endElement(self, name, ):
        if name == "row":
            curidx = 0
            line = []
            for idx, value in self.cells.items():
                line.append(sepstring * (idx - curidx))
                line.append(b"%s%s%s" % (dquote, value, dquote))
                curidx = idx
            self.output.append(b''.join(line))
        elif name == "worksheet":
            self.output.append(b'')


if __name__ == '__main__':
    try:
        handler = XlsXmlHandler()
        xml.sax.parse(sys.stdin, handler)
        print(b'\n'.join(handler.output))
    except BaseException as err:
        print("xml-parse: %s\n" % (str(sys.exc_info()[:2]),), file=sys.stderr)
        sys.exit(1)

    sys.exit(0)
