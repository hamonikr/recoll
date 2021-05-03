# Copyright (C) 2014-2020 J.F.Dockes
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

# Common code for the remaining Python xslt-based filters (most xslt work is
# now done in the c++ mh_xslt module, the ones remaining don't fit with its
# model).

import sys

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

