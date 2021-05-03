#!/usr/bin/env python3
# Copyright (C) 2016 J.F.Dockes
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

# Code to extract XMP tags using libexempi and python-xmp
from __future__ import print_function

can_xmp = True
try:
    import libxmp.utils
except:
    can_xmp = False

import re
import sys

xmp_index_re = re.compile('''\[[0-9+]\]$''')
#xmp_index_re = re.compile('''3''')

def rclxmp_enabled():
    return can_xmp

def rclxmp(filename):
    if not can_xmp:
        return None, "python-xmp not accessible"
    errstr = ""
    try:
        xmp = libxmp.utils.file_to_dict(filename)
    except Exception as e:
        errstr = str(e)

    if errstr:
        return None, errstr

    out = {}
    for ns in xmp.keys():
        for entry in xmp[ns]:
            if entry[1]:
                k = xmp_index_re.sub('', entry[0])
                if k.find("/") != -1:
                    continue
                if k in out:
                    out[k] += " " + entry[1]
                else:
                    out[k] = entry[1]
    return out, ""

if __name__ == "__main__":
    d, err = rclxmp(sys.argv[1])
    if d:
        print("Data: %s" % d)
    else:
        print("Error: %s" % err)
        

