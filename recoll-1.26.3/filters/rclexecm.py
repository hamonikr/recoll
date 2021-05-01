#################################
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
########################################################
## Recoll multifilter communication module and utilities
#
# All data is binary. This is important for Python3
# All parameter names are converted to and processed as str/unicode

from __future__ import print_function

import sys
import os
import tempfile
import shutil
import getopt
import rclconfig

PY3 = (sys.version > '3')
_mswindows = (sys.platform == "win32")

def makebytes(data):
    if type(data) == type(u''):
        return data.encode("UTF-8")
    return data

def subprocfile(fn):
    # On Windows PY3 the list2cmdline() method in subprocess assumes that
    # all args are str, and we receive file names as UTF-8. So we need
    # to convert.
    # On Unix all list elements get converted to bytes in the C
    # _posixsubprocess module, nothing to do
    if PY3 and _mswindows:
        return fn.decode('UTF-8')
    else:
        return fn

def configparamtrue(value):
    if not value:
        return False
    try:
        ivalue = int(value)
        if ivalue:
            return True
        else:
            return False
    except:
        pass
    if value[0] in 'tT':
        return True
    return False

my_config = rclconfig.RclConfig()

############################################
# RclExecM implements the
# communication protocol with the recollindex process. It calls the
# object specific of the document type to actually get the data.
class RclExecM:
    noteof = 0
    eofnext = 1
    eofnow = 2

    noerror = 0
    subdocerror = 1
    fileerror = 2
    
    def __init__(self):
        try:
            self.myname = os.path.basename(sys.argv[0])
        except:
            self.myname = "???"
        self.mimetype = b""

        self.fields = {}
        
        if os.environ.get("RECOLL_FILTER_MAXMEMBERKB"):
            self.maxmembersize = \
            int(os.environ.get("RECOLL_FILTER_MAXMEMBERKB"))
        else:
            self.maxmembersize = 50 * 1024
        self.maxmembersize = self.maxmembersize * 1024
        if sys.platform == "win32":
            import msvcrt
            msvcrt.setmode(sys.stdout.fileno(), os.O_BINARY)
            msvcrt.setmode(sys.stdin.fileno(), os.O_BINARY)
        self.debugfile = my_config.getConfParam("filterdebuglog")
        if self.debugfile:
            self.errfout = open(self.debugfile, "a")
        else:
            self.errfout = sys.stderr
    
    def rclog(self, s, doexit = 0, exitvalue = 1):
        # On windows, and I think that it changed quite recently (Qt change?)
        # we get stdout as stderr. So don't write at all
        if self.debugfile or sys.platform != "win32":
            print("RCLMFILT: %s: %s" % (self.myname, s), file=self.errfout)
            self.errfout.flush()
        if doexit:
            sys.exit(exitvalue)

    def breakwrite(self, outfile, data):
        if sys.platform != "win32":
            outfile.write(data)
        else:
            # On windows, writing big chunks can fail with a "not enough space"
            # error. Seems a combined windows/python bug, depending on versions.
            # See https://bugs.python.org/issue11395
            # In any case, just break it up
            total = len(data)
            bs = 4*1024
            offset = 0
            while total > 0:
                if total < bs:
                    tow = total
                else:
                    tow = bs
                #self.rclog("Total %d Writing %d to stdout: %s" % (total,tow,data[offset:offset+tow]))
                outfile.write(data[offset:offset+tow])
                offset += tow
                total -= tow
                
    # Note: tried replacing this with a multiple replacer according to
    #  http://stackoverflow.com/a/15221068, which was **10 times** slower
    def htmlescape(self, txt):
        # &amp must stay first (it somehow had managed to skip
        # after the next replace, with rather interesting results)
        try:
            txt = txt.replace(b'&', b'&amp;').replace(b'<', b'&lt;').\
                  replace(b'>', b'&gt;').replace(b'"', b'&quot;')
        except:
            txt = txt.replace("&", "&amp;").replace("<", "&lt;").\
                  replace(">", "&gt;").replace("\"", "&quot;")
        return txt

    # Our worker sometimes knows the mime types of the data it sends
    def setmimetype(self, mt):
        self.mimetype = makebytes(mt)

    def setfield(self, nm, value):
        self.fields[nm] = value

    # Read single parameter from process input: line with param name and size
    # followed by data. The param name is returned as str/unicode, the data
    # as bytes
    def readparam(self):
        if PY3:
            inf = sys.stdin.buffer
        else:
            inf = sys.stdin
        s = inf.readline()
        if s == b'':
            sys.exit(0)

        s = s.rstrip(b'\n')

        if s == b'':
            return ('', b'')
        l = s.split()
        if len(l) != 2:
            self.rclog(b'bad line: [' + s + b']', 1, 1)

        paramname = l[0].decode('ASCII').lower()
        paramsize = int(l[1])
        if paramsize > 0:
            paramdata = inf.read(paramsize)
            if len(paramdata) != paramsize:
                self.rclog("Bad read: wanted %d, got %d" %
                      (paramsize, len(paramdata)), 1, 1)
        else:
            paramdata = b''
    
        #self.rclog("paramname [%s] paramsize %d value [%s]" %
        #          (paramname, paramsize, paramdata))
        return (paramname, paramdata)

    if PY3:
        def senditem(self, nm, data):
            data = makebytes(data)
            l = len(data)
            sys.stdout.buffer.write(makebytes("%s: %d\n" % (nm, l)))
            self.breakwrite(sys.stdout.buffer, data)
    else:
        def senditem(self, nm, data):
            data = makebytes(data)
            l = len(data)
            sys.stdout.write(makebytes("%s: %d\n" % (nm, l)))
            self.breakwrite(sys.stdout, data)
        
    # Send answer: document, ipath, possible eof.
    def answer(self, docdata, ipath, iseof = noteof, iserror = noerror):

        if iserror != RclExecM.fileerror and iseof != RclExecM.eofnow:
            self.senditem("Document", docdata)

            if len(ipath):
                self.senditem("Ipath", ipath)

            if len(self.mimetype):
                self.senditem("Mimetype", self.mimetype)

            for nm,value in self.fields.items():
                #self.rclog("Senditem: [%s] -> [%s]" % (nm, value))
                self.senditem("%s:"%nm, value)
            self.fields = {}
            
        # If we're at the end of the contents, say so
        if iseof == RclExecM.eofnow:
            self.senditem("Eofnow", b'')
        elif iseof == RclExecM.eofnext:
            self.senditem("Eofnext", b'')
        if iserror == RclExecM.subdocerror:
            self.senditem("Subdocerror", b'')
        elif iserror == RclExecM.fileerror:
            self.senditem("Fileerror", b'')
  
        # End of message
        print()
        sys.stdout.flush()
        #self.rclog("done writing data")

    def processmessage(self, processor, params):

        # We must have a filename entry (even empty). Else exit
        if "filename:" not in params:
            print("%s" % params, file=sys.stderr)
            self.rclog("no filename ??", 1, 1)

        # If we're given a file name, open it. 
        if len(params["filename:"]) != 0:
            try:
                if not processor.openfile(params):
                    self.answer("", "", iserror = RclExecM.fileerror)
                    return
            except Exception as err:
                self.rclog("processmessage: openfile raised: [%s]" % err)
                self.answer("", "", iserror = RclExecM.fileerror)
                return

        # If we have an ipath, that's what we look for, else ask for next entry
        ipath = ""
        eof = True
        self.mimetype = ""
        try:
            if "ipath:" in params and len(params["ipath:"]):
                ok, data, ipath, eof = processor.getipath(params)
            else:
                ok, data, ipath, eof = processor.getnext(params)
        except Exception as err:
            self.rclog("getipath/next: exception: %s" %err)
            self.answer("", "", eof, RclExecM.fileerror)
            return

        #self.rclog("processmessage: ok %s eof %s ipath %s"%(ok, eof, ipath))
        if ok:
            self.answer(data, ipath, eof)
        else:
            self.answer("", "", eof, RclExecM.subdocerror)

    # Loop on messages from our master
    def mainloop(self, processor):
        while 1:
            #self.rclog("waiting for command")

            params = dict()

            # Read at most 10 parameters (normally 1 or 2), stop at empty line
            # End of message is signalled by empty paramname
            for i in range(10):
                paramname, paramdata = self.readparam()
                if paramname == "":
                    break
                params[paramname] = paramdata

            # Got message, act on it
            self.processmessage(processor, params)


# Helper routine to test for program accessibility
# Note that this works a bit differently from Linux 'which', which
# won't search the PATH if there is a path part in the program name,
# even if not absolute (e.g. will just try subdir/cmd in current
# dir). We will find such a command if it exists in a matching subpath
# of any PATH element.
# This is very useful esp. on Windows so that we can have several bin
# filter directories under filters (to avoid dll clashes). The
# corresponding c++ routine in recoll execcmd works the same.
def which(program):
    def is_exe(fpath):
        return os.path.exists(fpath) and os.access(fpath, os.X_OK)
    def ext_candidates(fpath):
        yield fpath
        for ext in os.environ.get("PATHEXT", "").split(os.pathsep):
            yield fpath + ext

    def path_candidates():
        yield os.path.dirname(sys.argv[0])
        rclpath = my_config.getConfParam("recollhelperpath")
        if rclpath:
            for path in rclpath.split(os.pathsep):
                yield path
        for path in os.environ["PATH"].split(os.pathsep):
            yield path

    if os.path.isabs(program):
        if is_exe(program):
            return program
    else:
        for path in path_candidates():
            exe_file = os.path.join(path, program)
            for candidate in ext_candidates(exe_file):
                if is_exe(candidate):
                    return candidate
    return None

# Temp dir helper
class SafeTmpDir:
    def __init__(self, em):
        self.em = em
        self.toptmp = ""
        self.tmpdir = ""

    def __del__(self):
        try:
            if self.toptmp:
                shutil.rmtree(self.tmpdir, True)
                os.rmdir(self.toptmp)
        except Exception as err:
            self.em.rclog("delete dir failed for " + self.toptmp)

    def getpath(self):
        if not self.tmpdir:
            envrcltmp = os.getenv('RECOLL_TMPDIR')
            if envrcltmp:
                self.toptmp = tempfile.mkdtemp(prefix='rcltmp', dir=envrcltmp)
            else:
                self.toptmp = tempfile.mkdtemp(prefix='rcltmp')

            self.tmpdir = os.path.join(self.toptmp, 'rclsofftmp')
            os.makedirs(self.tmpdir)

        return self.tmpdir
   

# Common main routine for all python execm filters: either run the
# normal protocol engine or a local loop to test without recollindex
def main(proto, extract):
    if len(sys.argv) == 1:
        proto.mainloop(extract)
        # mainloop does not return. Just in case
        sys.exit(1)


    # Not running the main loop: either acting as single filter (when called
    # from other filter for example), or debugging
    def usage():
        print("Usage: rclexecm.py [-d] [-s] [-i ipath] <filename>",
              file=sys.stderr)
        print("       rclexecm.py -w <prog>",
              file=sys.stderr)
        sys.exit(1)
        
    actAsSingle = False
    debugDumpData = False
    ipath = b""

    args = sys.argv[1:]
    opts, args = getopt.getopt(args, "hdsi:w:")
    for opt, arg in opts:
        if opt in ['-h']:
            usage()
        elif opt in ['-s']:
            actAsSingle = True
        elif opt in ['-i']:
            ipath = makebytes(arg)
        elif opt in ['-w']:
            ret = which(arg)
            if ret:
                print("%s" % ret)
                sys.exit(0)
            else:
                sys.exit(1)
        elif opt in ['-d']:
            debugDumpData = True
        else:
            print("unknown option %s\n"%opt, file=sys.stderr)
            usage()

    if len(args) != 1:
        usage()
    path = args[0]
        
    def mimetype_with_file(f):
        cmd = 'file -i "' + f + '"'
        fileout = os.popen(cmd).read()
        lst = fileout.split(':')
        mimetype = lst[len(lst)-1].strip()
        lst = mimetype.split(';')
        return makebytes(lst[0].strip())

    def mimetype_with_xdg(f):
        cmd = 'xdg-mime query filetype "' + f + '"'
        return makebytes(os.popen(cmd).read().strip())

    def debprint(out, s):
        if not actAsSingle:
            proto.breakwrite(out, makebytes(s+'\n'))

    params = {'filename:': makebytes(path)}

    # Some filters (e.g. rclaudio) need/get a MIME type from the indexer.
    # We make a half-assed attempt to emulate:
    mimetype = my_config.mimeType(path)
    if not mimetype and not _mswindows:
        mimetype = mimetype_with_file(path)
    if mimetype:
        params['mimetype:'] = mimetype

    if not extract.openfile(params):
        print("Open error", file=sys.stderr)
        sys.exit(1)

    if PY3:
        ioout = sys.stdout.buffer
    else:
        ioout = sys.stdout
    if ipath != b"" or actAsSingle:
        params['ipath:'] = ipath
        ok, data, ipath, eof = extract.getipath(params)
        if ok:
            debprint(ioout, "== Found entry for ipath %s (mimetype [%s]):" % \
                  (ipath, proto.mimetype.decode('cp1252')))
            bdata = makebytes(data)
            if debugDumpData or actAsSingle:
                proto.breakwrite(ioout, bdata)
                ioout.write(b'\n')
            sys.exit(0)
        else:
            print("Got error, eof %d"%eof, file=sys.stderr)
            sys.exit(1)

    ecnt = 0   
    while 1:
        ok, data, ipath, eof = extract.getnext(params)
        if ok:
            ecnt = ecnt + 1
            bdata = makebytes(data)
            debprint(ioout, "== Entry %d dlen %d ipath %s (mimetype [%s]):" % \
                  (ecnt, len(data), ipath, proto.mimetype.decode('cp1252')))
            if debugDumpData:
                proto.breakwrite(ioout, bdata)
                ioout.write(b'\n')
            if eof != RclExecM.noteof:
                sys.exit(0)
        else:
            print("Not ok, eof %d" % eof, file=sys.stderr)
            sys.exit(1)
        # Not sure this makes sense, but going on looping certainly does not
        if actAsSingle:
            sys.exit(0)
