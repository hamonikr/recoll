# Copyright (C) 2014 J.F.Dockes
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

# Helper module for xslt-based filters

from __future__ import print_function

import sys

PY2 = sys.version < '3'

if PY2:
    try:
        import libxml2
        import libxslt
        libxml2.substituteEntitiesDefault(1)
    except:
        print("RECFILTERROR HELPERNOTFOUND python:libxml2/python:libxslt1")
        sys.exit(1);
    def _apply_sheet_doc(sheet, doc):
        styledoc = libxml2.readMemory(sheet, len(sheet), '', '',
                                      options=libxml2.XML_PARSE_NONET)
        style = libxslt.parseStylesheetDoc(styledoc)
        result = style.applyStylesheet(doc, None)
        res = ""
        try:
            res = style.saveResultToString(result)
        except Exception as err:
            # print("saveResultToString got exception: %s"%err)
            pass
        style.freeStylesheet()
        doc.freeDoc()
        result.freeDoc()
        return res
    def apply_sheet_data(sheet, data):
        doc = libxml2.readMemory(data, len(data), '', '',
                                 options=libxml2.XML_PARSE_NONET)
        return _apply_sheet_doc(sheet, doc)
    def apply_sheet_file(sheet, fn):
        doc = libxml2.readFile(fn, '', options=libxml2.XML_PARSE_NONET)
        return _apply_sheet_doc(sheet, doc)
else:
    try:
        from lxml import etree
    except:
        print("RECFILTERROR HELPERNOTFOUND python3:lxml")
        sys.exit(1);
    def _apply_sheet_doc(sheet, doc):
        styledoc = etree.fromstring(sheet)
        transform = etree.XSLT(styledoc)
        return bytes(transform(doc))
    def apply_sheet_data(sheet, data):
        doc = etree.fromstring(data)
        return _apply_sheet_doc(sheet, doc)
    def apply_sheet_file(sheet, fn):
        doc = etree.parse(fn)
        return _apply_sheet_doc(sheet, doc)

