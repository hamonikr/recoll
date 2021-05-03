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

# Caching OCR'd data
#
# OCR is extremely slow, caching the results is necessary.
#
# The cache stores 2 kinds of objects:
# - Path files are named from the hash of the image file path and
#   contain the image data hash, the modification time and size of the
#   image file at the time the OCR'd data was stored in the cache, and
#   the image path itself (the last is for purging only).
# - Data files are named with the hash of the image data and contain
#   the zlib-compressed OCR'd data.
#
# When retrieving data from the cache:
#  - We first use the image file size and modification time: if an
#    entry exists for the imagepath/mtime/size triplet, and is up to
#    date, the corresponding data is obtained from the data file and
#    returned.
#  - Else we then use the image data: if an entry exists for the
#    computed hashed value of the data, it is returned. This allows
#    moving files around without needing to run OCR again, but of
#    course, it is more expensive than the first step
#
#  If we need to use the second step, as a side effect, a path file is
#  created or updated so that the data will be found with the first
#  step next time around.
#
# Purging the cache of obsolete data.
#
#  - The cache path and data files are stored under 2 different
#    directories (objects, paths) to make purging easier.
#  - Purging the paths tree just involves walking it, reading the
#    files, and checking the existence of the recorded paths.
#  - There is no easy way to purge the data tree. The only possibility
#    is to input a list of possible source files (e.g. result of a
#    find in the image files area), and compute all the hashes. Data
#    files which do not match one of the hashes are deleted.

import sys
import os
import hashlib
import urllib.parse
import zlib
import glob

import rclexecm

def _deb(s):
    rclexecm.logmsg(s)
    

class OCRCache(object):
    def __init__(self, conf):
        self.config = conf
        self.cachedir = conf.getConfParam("ocrcachedir")
        if not self.cachedir:
            self.cachedir = os.path.join(self.config.getConfDir(), "ocrcache")
        self.objdir = os.path.join(self.cachedir, "objects")
        self.pathdir = os.path.join(self.cachedir, "paths")
        for dir in (self.objdir, self.pathdir):
            if not os.path.exists(dir):
                os.makedirs(dir)

    # Compute sha1 of path, as two parts of 2 and 38 chars
    def _hashpath(self, data):
        if type(data) != type(b""):
            data = data.encode('utf-8')
            m = hashlib.sha1()
            m.update(data)
            h = m.hexdigest()
        return h[0:2], h[2:]

    # Compute sha1 of path data contents, as two parts of 2 and 38 chars
    def _hashdata(self, path):
        #_deb("Hashing DATA")
        m = hashlib.sha1()
        with open(path, "rb") as f:
            while True:
                d = f.read(8192)
                if not d:
                    break
                m.update(d)
                h = m.hexdigest()
        return h[0:2], h[2:]

    
    def _readpathfile(self, ppf):
        '''Read path file and return values. We do not decode the image path
        as this is only used for purging'''
        with open(ppf, 'r') as f:
            line = f.read()
        dd,df,tm,sz,pth = line.split()
        tm = int(tm)
        sz = int(sz)
        return dd,df,tm,sz,pth
        
    # Try to read the stored attributes for a given path: data hash,
    # modification time and size. If this fails, the path itself is
    # not cached (but the data still might be, maybe the file was moved)
    def _cachedpathattrs(self, path):
        pd,pf = self._hashpath(path)
        pathfilepath = os.path.join(self.pathdir, pd, pf)
        if not os.path.exists(pathfilepath):
            return False, None, None, None, None
        try:
            dd, df, tm, sz, pth = self._readpathfile(pathfilepath)
            return True, dd, df, tm, sz
        except:
            return False, None, None, None, None

    # Compute the path hash, and get the mtime and size for given
    # path, for updating the cache path file
    def _newpathattrs(self, path):
        pd,pf = self._hashpath(path)
        tm = int(os.path.getmtime(path))
        sz = int(os.path.getsize(path))
        return pd, pf, tm, sz
    
    # Check if the cache appears up to date for a given path, only
    # using the modification time and size. Return the data file path
    # elements if we get a hit.
    def _pathincache(self, path):
        ret, od, of, otm, osz = self._cachedpathattrs(path)
        if not ret:
            return False, None, None
        pd, pf, ntm, nsz = self._newpathattrs(path)
        #_deb(" tm %d  sz %d" % (ntm, nsz))
        #_deb("otm %d osz %d" % (otm, osz))
        if otm != ntm or osz != nsz:
            return False, None, None
        return True, od, of

    # Check if cache appears up to date for path (no data check),
    # return True/False
    def pathincache(self, path):
        ret, dd, df = self._pathincache(path)
        return ret
    
    # Compute the data file name for path. Expensive: we compute the data hash.
    # Return both the data file path and path elements (for storage in path file)
    def _datafilename(self, path):
        d, f = self._hashdata(path)
        return os.path.join(self.objdir, d, f), d, f

    # Check if the data for path is in cache: expensive, needs to
    # compute the hash for the path's data contents. Returns True/False
    def dataincache(self, path):
        return os.path.exists(self._datafilename(path)[0])

    # Create path file with given elements.
    def _updatepathfile(self, pd, pf, dd, df, tm, sz, path):
        dir = os.path.join(self.pathdir, pd)
        if not os.path.exists(dir):
            os.makedirs(dir)
        pfile = os.path.join(dir, pf)
        codedpath = urllib.parse.quote(path)
        with open(pfile, "w") as f:
            f.write("%s %s %d %d %s\n" % (dd, df, tm, sz, codedpath))

    # Store data for path. Only rewrite an existing data file if told
    # to do so: this is only useful if we are forcing an OCR re-run.
    def store(self, path, datatostore, force=False):
        dd,df = self._hashdata(path)
        pd, pf, tm, sz = self._newpathattrs(path)
        self._updatepathfile(pd, pf, dd, df, tm, sz, path)
        dir = os.path.join(self.objdir, dd)
        if not os.path.exists(dir):
            os.makedirs(dir)
        dfile = os.path.join(dir, df)
        if force or not os.path.exists(dfile):
            #_deb("Storing data")
            cpressed = zlib.compress(datatostore)
            with open(dfile, "wb") as f:
                f.write(cpressed)
        return True

    # Retrieve cached OCR'd data for image path. Possibly update the
    # path file as a side effect (case where the image has moved, but
    # the data has not changed).
    def get(self, path):
        pincache, dd, df = self._pathincache(path)
        if pincache:
            dfn = os.path.join(self.objdir, dd, df)
        else:
            dfn, dd, df = self._datafilename(path)

        if not os.path.exists(dfn):
            return False, b""

        if not pincache:
            # File has moved. create/Update path file for next time
            _deb("ocrcache::get file %s was moved, updating path data" % path)
            pd, pf, tm, sz = self._newpathattrs(path)
            self._updatepathfile(pd, pf, dd, df, tm, sz, path)

        with open(dfn, "rb") as f:
            cpressed = f.read()
            data = zlib.decompress(cpressed)
            return True, data

    def _pathstale(self, origpath, otm, osz):
        '''Return True if the input path has been removed or modified'''
        if not os.path.exists(origpath):
            return True
        ntm = int(os.path.getmtime(origpath))
        nsz = int(os.path.getsize(origpath))
        if ntm != otm or nsz != osz:
            #_deb("Purgepaths otm %d ntm %d osz %d nsz %d"%(otm, ntm, osz, nsz))
            return True
        return False
    
    def purgepaths(self):
        '''Remove all stale pathfiles: source image does not exist or has
        been changed. Mostly useful for removed files, modified ones would be
        processed by recollindex.'''
        allpathfiles = glob.glob(os.path.join(self.pathdir, "*", "*"))
        for pathfile in allpathfiles:
            dd, df, tm, sz, orgpath = self._readpathfile(pathfile)
            needpurge = self._pathstale(orgpath, tm, sz)
            if needpurge:
                _deb("purgepaths: removing %s (%s)" % (pathfile, orgpath))
                os.remove(pathfile)

    def _walk(self, topdir, cb):
        '''Specific fs walk: we know that our tree has 2 levels. Call cb with
        the file path as parameter for each file'''
        dlist = glob.glob(os.path.join(topdir, "*"))
        for dir in dlist:
            files = glob.glob(os.path.join(dir, "*"))
            for f in files:
                cb(f)

    def _pgdt_pathcb(self, f):
        '''Get a pathfile name, read it, and record datafile identifier
        (concatenate data file subdir and file name)'''
        #_deb("_pgdt_pathcb: %s" % f)
        dd, df, tm, sz, orgpath = self._readpathfile(f)
        self._pgdt_alldatafns.add(dd+df)

    def _pgdt_datacb(self, datafn):
        '''Get a datafile name and check that it is referenced by a previously
        seen pathfile'''
        p1,fn = os.path.split(datafn)
        p2,dn = os.path.split(p1)
        tst = dn+fn
        if tst in self._pgdt_alldatafns:
            _deb("purgedata: ok         : %s" % datafn)
            pass
        else:
            _deb("purgedata: removing   : %s" % datafn)
            os.remove(datafn)
            
    def purgedata(self):
        '''Remove all data files which do not match any from the input list,
        based on data contents hash. We make a list of all data files
        referenced by the path files, then walk the data tree,
        removing all unreferenced files. This should only be run after
        an indexing pass, so that the path files are up to date. It's
        a relatively onerous operation as we have to read all the path
        files, and walk both sets of files.'''

        self._pgdt_alldatafns = set()
        self._walk(self.pathdir, self._pgdt_pathcb)
        self._walk(self.objdir, self._pgdt_datacb)
        


if __name__ == '__main__':
    import rclconfig
    def _Usage():
        _deb("Usage: rclocrcache.py --purge")
        sys.exit(1)
    if len(sys.argv) != 2:
        _Usage()
    if sys.argv[1] != "--purge":
        _Usage()
    
    conf = rclconfig.RclConfig()
    cache = OCRCache(conf)
    cache.purgepaths()
    cache.purgedata()
    sys.exit(0)
    
#    def trycache(p):
#        _deb("== CACHE tests for %s"%p)
#        ret = cache.pathincache(p)
#        s = "" if ret else " not"
#        _deb("path for %s%s in cache" % (p, s))
#        if not ret:
#            return False
#        ret = cache.dataincache(p)
#        s = "" if ret else " not"
#        _deb("data for %s%s in cache" % (p, s))
#        return ret
#    def trystore(p):
#        _deb("== STORE test for %s" % p)
#        cache.store(p, b"my OCR'd text is one line\n", force=False)
#    def tryget(p):
#        _deb("== GET test for %s" % p)
#        incache, data = cache.get(p)
#        if incache:
#            _deb("Data from cache [%s]" % data)
#        else:
#            _deb("Data was not found in cache")
#        return incache, data
#    if False:
#        path = sys.argv[1]
#        incache, data = tryget(path)
#        if not incache:
#            trystore(path)
#
