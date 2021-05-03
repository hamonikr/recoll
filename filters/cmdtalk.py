#################################
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
########################################################
# Command communication module and utilities. See commands in cmdtalk.h
#
# All data is binary. This is important for Python3
# All parameter names are converted to and processed as str/unicode

from __future__ import print_function

import sys
import os
import tempfile
import shutil
import getopt
import traceback

PY3 = sys.version > '3'

if PY3:
    def makebytes(data):
        if data is None:
            return b""
        if isinstance(data, bytes):
            return data
        else:
            return data.encode("UTF-8")
else:
    def makebytes(data):
        if data is None:
            return ""
        if isinstance(data, unicode):
            return data.encode("UTF-8")
        else:
            return data


############################################
# CmdTalk implements the communication protocol with the master
# process. It calls an external method to use the args and produce
# return data.
class CmdTalk(object):

    def __init__(self, outfile=sys.stdout, infile=sys.stdin, exitfunc=None):
        try:
            self.myname = os.path.basename(sys.argv[0])
        except:
            self.myname = "???"

        self.outfile = outfile
        self.infile = infile
        self.exitfunc = exitfunc
        self.fields = {}
        
        if sys.platform == "win32":
            import msvcrt
            msvcrt.setmode(self.outfile.fileno(), os.O_BINARY)
            msvcrt.setmode(self.infile.fileno(), os.O_BINARY)
        
        try:
            self.debugfile
        except:
            self.debugfile = None
        try:
            self.nodecodeinput
        except:
            self.nodecodeinput = False
        if self.debugfile:
            self.errfout = open(self.debugfile, "a")
        else:
            self.errfout = sys.stderr
        
    def log(self, s, doexit = 0, exitvalue = 1):
        print("CMDTALK: %s: %s" % (self.myname, s), file=self.errfout)
        if doexit:
            if self.exitfunc:
                self.exitfunc(exitvalue)
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
                #self.log("Total %d Writing %d to stdout: %s" % (total,tow,data[offset:offset+tow]))
                outfile.write(data[offset:offset+tow])
                offset += tow
                total -= tow
                
    # Read single parameter from process input: line with param name and size
    # followed by data. The param name is returned as str/unicode, the data
    # as bytes
    def readparam(self):
        if PY3:
            inf = self.infile.buffer
        else:
            inf = self.infile
        s = inf.readline()
        if s == b'':
            if self.exitfunc:
                self.exitfunc(0)
            sys.exit(0)

        s = s.rstrip(b'\n')

        if s == b'':
            return ('', b'')
        l = s.split()
        if len(l) != 2:
            self.log(b'bad line: [' + s + b']', 1, 1)

        paramname = l[0].decode('ASCII').rstrip(':')
        paramsize = int(l[1])
        if paramsize > 0:
            paramdata = inf.read(paramsize)
            if len(paramdata) != paramsize:
                self.log("Bad read: wanted %d, got %d" %
                      (paramsize, len(paramdata)), 1, 1)
        else:
            paramdata = b''
        if PY3 and not self.nodecodeinput:
            try:
                paramdata = paramdata.decode('utf-8')
            except Exception as ex:
                self.log("Exception decoding param: %s" % ex)
                paramdata = b''
                
        #self.log("paramname [%s] paramsize %d value [%s]" %
        #          (paramname, paramsize, paramdata))
        return (paramname, paramdata)

    if PY3:
        def senditem(self, nm, data):
            data = makebytes(data)
            l = len(data)
            self.outfile.buffer.write(makebytes("%s: %d\n" % (nm, l)))
            self.breakwrite(self.outfile.buffer, data)
    else:
        def senditem(self, nm, data):
            data = makebytes(data)
            l = len(data)
            self.outfile.write(makebytes("%s: %d\n" % (nm, l)))
            self.breakwrite(self.outfile, data)
        
    # Send answer: document, ipath, possible eof.
    def answer(self, outfields):
        for nm,value in outfields.items():
            #self.log("Senditem: [%s] -> [%s]" % (nm, value))
            self.senditem(nm, value)
            
        # End of message
        print(file=self.outfile)
        self.outfile.flush()
        #self.log("done writing data")

    # Call processor with input params, send result
    def processmessage(self, processor, params):
        # In normal usage we try to recover from processor errors, but
        # we sometimes want to see the real stack trace when testing
        safeexec = True
        if safeexec:
            try:
                outfields = processor.process(params)
            except Exception as err:
                self.log("processmessage: processor raised: [%s]" % err)
                traceback.print_exc()
                outfields = {}
                outfields["cmdtalkstatus"] = "1"
                outfields["cmdtalkerrstr"] = str(err)
        else:
            outfields = processor.process(params)

        self.answer(outfields)

    # Loop on messages from our master
    def mainloop(self, processor):
        while 1:
            #self.log("waiting for command")

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


# Common main routine for testing: either run the normal protocol
# engine or a local loop. This means that you can call
# cmdtalk.main(proto,processor) instead of proto.mainloop(processor)
# from your module, and get the benefits of command line testing
def main(proto, processor):
    if len(sys.argv) == 1:
        proto.mainloop(processor)
        # mainloop does not return. Just in case
        sys.exit(1)

    # Not running the main loop: run one processor call for debugging
    def usage():
        print("Usage: cmdtalk.py pname pvalue [pname pvalue...]",
              file=sys.stderr)
        sys.exit(1)
    def debprint(out, s):
        proto.breakwrite(out, makebytes(s+'\n'))
        
    args = sys.argv[1:]
    if len(args) == 0 or len(args) % 2 != 0:
        usage()
    params = dict()
    for i in range(int(len(args)/2)):
        params[args[2*i]] = args[2*i+1]
    res = processor.process(params)

    ioout = sys.stdout.buffer if PY3 else sys.stdout

    for nm,value in res.items():
        #self.log("Senditem: [%s] -> [%s]" % (nm, value))
        bdata = makebytes(value)
        debprint(ioout, "%s->" % nm)
        proto.breakwrite(ioout, bdata)
        ioout.write(b'\n')
