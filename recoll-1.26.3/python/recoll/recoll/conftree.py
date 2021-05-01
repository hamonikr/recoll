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
#   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

from __future__ import print_function

import locale
import re
import os
import sys
import base64
import platform
import shlex

def _debug(s):
    print("%s"%s, file=sys.stderr)
    
class ConfSimple(object):
    """A ConfSimple class reads a recoll configuration file, which is
    a typical ini file (see the Recoll manual). It's a dictionary of
    dictionaries which lets you retrieve named values from the top
    level or a subsection"""

    def __init__(self, confname, tildexp = False, readonly = True):
        self.submaps = {}
        self.dotildexpand = tildexp
        self.readonly = readonly
        self.confname = confname
        
        try:
            f = open(confname, 'rb')
        except Exception as exc:
            #_debug("Open Exception: %s" % exc)
            # File does not exist -> empty config, not an error.
            self.submaps = {}
            self.submaps[b''] = {}
            return

        self._parseinput(f)
        
    def _parseinput(self, f):
        appending = False
        line = b''
        submapkey = b''
        for cline in f:
            cline = cline.rstrip(b'\r\n')
            if appending:
                line = line + cline
            else:
                line = cline
            line = line.strip()
            if line == b'' or line[0] == b'#'[0]:
                continue

            if line[len(line)-1] == b'\\'[0]:
                line = line[0:len(line)-1]
                appending = True
                continue

            appending = False
            #_debug(line)
            if line[0] == b'['[0]:
                line = line.strip(b'[]')
                if self.dotildexpand:
                    submapkey = os.path.expanduser(line)
                    if type(submapkey) == type(u''):
                        submapkey = submapkey.encode('utf-8')
                else:
                    submapkey = line
                #_debug("Submapkey: [%s]" % submapkey)
                continue

            nm, sep, value = line.partition(b'=')
            if sep == b'':
                # No equal sign in line -> considered comment
                continue

            nm = nm.strip()
            value = value.strip()
            #_debug("sk [%s] nm: [%s] value: [%s]" % (submapkey, nm, value))
            if not submapkey in self.submaps:
                self.submaps[submapkey] = {}
            self.submaps[submapkey][nm] = value

    def getbin(self, nm, sk = b''):
        '''Returns None if not found, empty string if found empty'''
        if type(nm) != type(b'') or type(sk) != type(b''):
            raise TypeError("getbin: parameters must be binary not unicode")
        #_debug("ConfSimple::getbin nm [%s] sk [%s]" % (nm, sk))
        if not sk in self.submaps:
            return None
        if not nm in self.submaps[sk]:
            return None
        return self.submaps[sk][nm]

    def get(self, nm, sk = b''):
        dodecode = False
        if type(nm) == type(u''):
            dodecode = True
            nm = nm.encode('utf-8')
        if type(sk) == type(u''):
            sk = sk.encode('utf-8')
        #v = ConfSimple.getbin(self, nm, sk)
        v = self.getbin(nm, sk)
        if v and dodecode:
            v = v.decode('utf-8')
        return v

    def getNamesbin(self, sk = b''):
        if not sk in self.submaps:
            return None
        return list(self.submaps[sk].keys())

    def getNames(self, sk = ''):
        if not sk in self.submaps:
            return None
        dodecode = False
        if type(sk) == type(u''):
            dodecode = True
            sk = sk.encode('utf-8')
        names = self.getNamesbin(sk)
        if names and dodecode:
            names = [nm.decode('utf-8') for nm in names]
        return names

    def _rewrite(self):
        if self.readonly:
            raise Exception("ConfSimple is readonly")

        tname = self.confname + "-"
        f = open(tname, 'wb')
        # First output null subkey submap
        if b'' in self.submaps:
            for nm,value in self.submaps[b''].items():
                f.write(nm + b'=' + value + b'\n')
        for sk,mp in self.submaps.items():
            if sk == b'':
                continue
            f.write(b'[' + sk + b']\n')
            for nm,value in mp.items():
                f.write(nm + b'=' + value + b'\n')
        f.close()
        try:
            # os.replace works on Windows even if dst exists, but py3 only
            os.replace(tname, self.confname)
        except:
            try:
                os.rename(tname, self.confname)
            except:
                import shutil
                shutil.move(tname, self.confname)

    def setbin(self, nm, value, sk = b''):
        if self.readonly:
            raise Exception("ConfSimple is readonly")
        if sk not in self.submaps:
            self.submaps[sk] = {}
        self.submaps[sk][nm] = value
        self._rewrite()
        return True

    def set(self, nm, value, sk = b''):
        if self.readonly:
            raise Exception("ConfSimple is readonly")
        if type(nm) == type(u''):
            nm = nm.encode('utf-8')
        if type(value) == type(u''):
            value = value.encode('utf-8')
        if type(sk) == type(u''):
            sk = sk.encode('utf-8')
        return self.setbin(nm, value, sk)
    
    
class ConfTree(ConfSimple):
    """A ConfTree adds path-hierarchical interpretation of the section keys,
    which should be '/'-separated values. When a value is requested for a
    given path, it will also be searched in the sections corresponding to
    the ancestors. E.g. get(name, '/a/b') will also look in sections '/a' and
    '/' or '' (the last 2 are equivalent)"""

    def getbin(self, nm, sk = b''):
        if type(nm) != type(b'') or type(sk) != type(b''):
            raise TypeError("getbin: parameters must be binary not unicode")
        #_debug("ConfTree::getbin: nm [%s] sk [%s]" % (nm, sk))
        
        if sk == b'' or sk[0] != b'/'[0]:
            return ConfSimple.getbin(self, nm, sk)

        if sk[len(sk)-1] == b'/'[0]:
             sk = sk[:len(sk)-1]

        # Try all sk ancestors as submaps (/a/b/c-> /a/b/c, /a/b, /a, b'')
        while sk:
            if sk in self.submaps:
                return ConfSimple.getbin(self, nm, sk)
            if sk + b'/' in self.submaps:
                return ConfSimple.getbin(self, nm, sk+b'/')
            i = sk.rfind(b'/')
            if i == -1:
                break
            sk = sk[:i]

        return ConfSimple.getbin(self, nm)


class ConfStack(object):
    """ A ConfStack manages the superposition of a list of Configuration
    objects. Values are looked for in each object from the list until found.
    This typically provides for defaults overriden by sparse values in the
    topmost file."""

    def __init__(self, nm, dirs, tp = 'simple'):
        fnames = []
        for dir in dirs:
            fnm = os.path.join(dir, nm)
            fnames.append(fnm)
            self._construct(tp, fnames)

    def _construct(self, tp, fnames):
        self.confs = []
        for fname in fnames:
            if tp.lower() == 'simple':
                conf = ConfSimple(fname)
            else:
                conf = ConfTree(fname)
            self.confs.append(conf)

    # Accepts / returns binary strings (non-unicode)
    def getbin(self, nm, sk = b''):
        if type(nm) != type(b'') or type(sk) != type(b''):
           raise TypeError("getbin: parameters must be binary not unicode")
        for conf in self.confs:
            value = conf.getbin(nm, sk)
            if value is not None:
                return value
        return None

    def get(self, nm, sk = b''):
        dodecode = False
        if type(nm) == type(u''):
            dodecode = True
            nm = nm.encode('utf-8')
        if type(sk) == type(u''):
            sk = sk.encode('utf-8')
        #v = ConfSimple.getbin(self, nm, sk)
        v = self.getbin(nm, sk)
        if v and dodecode:
            v = v.decode('utf-8')
        return v

# Split string of strings, with possible quoting and escaping.
# The default is do do Recoll stringToStrings emulation: whitespace
# separated, and doublequotes only (C-style). E.G.:
#   word1 word2 "compound \\"quoted\\" string" ->
#       ['word1', 'word2', 'compound "quoted string']
#
# This is not the shlex default and can be changed by setting the
# parameters
def stringToStrings(s, quotes = '"', escape = '\\', escapedquotes = '"',
                    whitespace = None):
    lex = shlex.shlex(s, posix=True)
    lex.whitespace_split = True
    if quotes is not None:
        lex.quotes = quotes
    if escape is not None:
        lex.escape = escape
    if escapedquotes is not None:
        lex.escapedquotes = escapedquotes
    if whitespace is not None:
        lex.whitespace = whitespace
    l = []
    while True:
        tok = lex.get_token()
        if not tok:
            break
        l.append(tok)
    return l

def stringsToString(vs):
    out = []
    for s in vs:
        if s.find(" ") != -1 or s.find("\t") != -1 or s.find("\\") != -1 or \
               s.find('"') != -1:
            out.append('"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"')
        else:
            out.append(s)
    return " ".join(out)

