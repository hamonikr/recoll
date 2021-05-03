#!/usr/bin/env python3
#################################
# Copyright (C) 2020 J.F.Dockes
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
########################################################

# Running abbyyocr for Recoll OCR (see rclocr.py)

import os
import sys
import atexit
import tempfile
import subprocess
import glob

import rclexecm

_mswindows = (sys.platform == "win32")
if _mswindows:
    ocrlangfile = "rclocrlang.txt"
else:
    ocrlangfile = ".rclocrlang"

_okexts = ('.pdf', '.tif', '.tiff', '.jpg', '.png', '.jpeg')

abbyyocrcmd = ""
abbyocrdir = ""

def _deb(s):
    rclexecm.logmsg(s)

# Return true if abbyy appears to be available
def ocrpossible(config, path):
    global abbyyocrcmd
    if not abbyyocrcmd:
        config.setKeyDir(os.path.dirname(path))
        abbyyocrcmd = config.getConfParam("abbyyocrcmd")
        if not abbyyocrcmd:
            abbyyocrcmd = rclexecm.which("abbyyocr11")
        if not abbyyocrcmd:
            return False
        global abbyyocrdir
        abbyyocrdir = os.path.dirname(abbyyocrcmd)
    
    # Check input format
    base,ext = os.path.splitext(path)
    ext = ext.lower()
    if ext in _okexts:
        return True
    return False


# Try to guess tesseract language. This should depend on the input
# file, but we have no general way to determine it. So use the
# environment and hope for the best.
def _guessocrlang(config, path):
    ocrlang = ""

    dirname = os.path.dirname(path)

    # First look for a language def file in the file's directory
    langfile = os.path.join(dirname, ocrlangfile)
    if os.path.isfile(langfile):
        ocrlang = open(langfile, "r").read().strip()
    if ocrlang:
        _deb("OCR lang from file: %s" % ocrlang)
        return ocrlang

    # Then look for a config file  option.
    config.setKeyDir(dirname)
    ocrlang = config.getConfParam("abbyylang")
    if ocrlang:
        _deb("OCR lang from config: %s" % ocrlang)
        return ocrlang

    # Half-assed trial to guess from LANG then default to english
    try:
        localelang = os.environ.get("LANG", "").split("_")[0]
        if localelang == "en":
            ocrlang = "English"
        elif localelang == "de":
            ocrlang = "German"
        elif localelang == "fr":
            ocrlang = "French"
    except:
        pass

    if not ocrlang:
        ocrlang = "English"
    _deb("OCR lang (guessed): %s" % ocrlang)
    return ocrlang


# run ocr on the input path and output the result data.
def runocr(config, path):
    ocrlang = _guessocrlang(config, path)
    my_env = os.environ.copy()
    eldpn = "LD_LIBRARY_PATH"
    if eldpn in my_env:
        oldpath = ":" + my_env[eldpn]
    else:
        oldpath = ""
    my_env[eldpn] = abbyyocrdir + oldpath

    try:
        out = subprocess.check_output(
            [abbyyocrcmd, "-lpp", "BookArchiving_Accuracy",
             "-rl", ocrlang,
             "-tet", "UTF8",
             "-f", "TextUnicodeDefaults",
             "-if", path,
             "-c"],
            env = my_env,
            stderr=subprocess.DEVNULL)
    except Exception as e:
        _deb("%s failed: %s" % (abbyyocrcmd,e))
        return False, ""
    return True, out
            

if __name__ == '__main__':
    import rclconfig
    config = rclconfig.RclConfig()
    path =  sys.argv[1]
    if ocrpossible(config, path):
        ok, data = runocr(config, sys.argv[1])
    else:
        _deb("ocrpossible returned false")
        sys.exit(1)
    if ok:
        sys.stdout.buffer.write(data)
    else:
        _deb("OCR program failed")
