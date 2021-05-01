#!/usr/bin/env python3
from __future__ import print_function

import locale
import re
import os
import sys
import base64
import platform

try:
    from . import conftree
except:
    import conftree

class RclDynConf:
    def __init__(self, fname):
        self.data = conftree.ConfSimple(fname)

    def getStringList(self, sk):
        nms = self.data.getNames(sk)
        out = []
        if nms is not None:
            for nm in nms:
                out.append(base64.b64decode(self.data.get(nm, sk)))
        return out
    
class RclConfig:
    def __init__(self, argcnf = None):
        self.config = None
        self.mimemap = None
        platsys = platform.system()
        # Find configuration directory
        if argcnf is not None:
            self.confdir = os.path.abspath(argcnf)
        elif "RECOLL_CONFDIR" in os.environ:
            self.confdir = os.environ["RECOLL_CONFDIR"]
        else:
            if platsys == "Windows":
                if "LOCALAPPDATA" in os.environ:
                    dir = os.environ["LOCALAPPDATA"]
                else:
                    dir = os.path.expanduser("~")
                self.confdir = os.path.join(dir, "Recoll")
            else:
                self.confdir = os.path.expanduser("~/.recoll")
        #print("Confdir: [%s]" % self.confdir, file=sys.stderr)
        
        # Also find datadir. This is trickier because this is set by
        # "configure" in the C code. We can only do our best. Have to
        # choose a preference order. Use RECOLL_DATADIR if the order is wrong
        self.datadir = None
        if "RECOLL_DATADIR" in os.environ:
            self.datadir = os.environ["RECOLL_DATADIR"]
        else:
            if platsys == "Windows":
                self.datadir = os.path.join(os.path.dirname(sys.argv[0]), "..")
            else:
                dirs = ("/opt/local", "/usr", "/usr/local")
                for dir in dirs:
                    dd = os.path.join(dir, "share/recoll")
                    if os.path.exists(dd):
                        self.datadir = dd
        if self.datadir is None:
            self.datadir = "/usr/share/recoll"
        #print("Datadir: [%s]" % self.datadir, file=sys.stderr)
        self.cdirs = []
        
        # Additional config directory, values override user ones
        if "RECOLL_CONFTOP" in os.environ:
            self.cdirs.append(os.environ["RECOLL_CONFTOP"])
        self.cdirs.append(self.confdir)
        # Additional config directory, overrides system's, overridden by user's
        if "RECOLL_CONFMID" in os.environ:
            self.cdirs.append(os.environ["RECOLL_CONFMID"])
        self.cdirs.append(os.path.join(self.datadir, "examples"))
        #print("Config dirs: %s" % self.cdirs, file=sys.stderr)
        self.keydir = ''

    def getConfDir(self):
        return self.confdir
    def getDataDir(self):
        return self.datadir
    
    def setKeyDir(self, dir):
        self.keydir = dir

    def getConfParam(self, nm):
        if not self.config:
            self.config = conftree.ConfStack("recoll.conf", self.cdirs, "tree")
        return self.config.get(nm, self.keydir)

    # This is a simplified version of the c++ code, intended mostly for the
    # test mode of rclexecm.py. We don't attempt to check the data, so this
    # will not work on extension-less paths (e.g. mbox/mail/etc.)
    def mimeType(self, path):
        if not self.mimemap:
            self.mimemap = conftree.ConfStack("mimemap", self.cdirs, "tree")
        if os.path.exists(path):
            if os.path.isdir(path):
                return "inode/directory"
            if os.path.islink(path):
                return "inode/symlink"
            if not os.path.isfile(path):
                return "inode/x-fsspecial"
            try:
                size = os.path.getsize(path)
                if size == 0:
                    return "inode/x-empty"
            except:
                pass
        ext = os.path.splitext(path)[1]
        return self.mimemap.get(ext, self.keydir)
        
class RclExtraDbs:
    def __init__(self, config):
        self.config = config

    def getActDbs(self):
        dyncfile = os.path.join(self.config.getConfDir(), "history")
        dync = RclDynConf(dyncfile)
        return dync.getStringList("actExtDbs")
    
if __name__ == '__main__':
    config = RclConfig()
    print("topdirs = %s" % config.getConfParam("topdirs"))
    extradbs = RclExtraDbs(config)
    print(extradbs.getActDbs())
