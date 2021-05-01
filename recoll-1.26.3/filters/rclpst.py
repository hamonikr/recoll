#!/usr/bin/python3
#################################
# Copyright (C) 2019 J.F.Dockes
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

#
# Process the stream produced by a modified pffexport:
# https://github.com/libyal/libpff
# The modification allows producing a data stream instead of a file tree
#

import sys
import os
import pathlib
import email.parser
import email.policy
import mailbox
import subprocess
import rclexecm
import rclconfig
import conftree
import base64

_mswindows = (sys.platform == "win32" or sys.platform == "msys")
if _mswindows:
    import ntpath
    met_basename = ntpath.basename
    met_dirname = ntpath.dirname
    met_splitext = ntpath.splitext
    met_join = ntpath.join
    def _backslashize(s):
        if type(s) == type(""):
            return s.replace("/", "\\")
        else:
            return s.replace(b"/", b"\\")
else:
    met_basename = os.path.basename
    met_dirname = os.path.dirname
    met_splitext = os.path.splitext
    met_join = os.path.join
    def _backslashize(s):
        return s

# The pffexport stream yields the email in several pieces, with some
# data missing (e.g. attachment MIME types). We rebuild a complete
# message for parsing by the Recoll email handler
class EmailBuilder(object):
    def __init__(self, logger, mimemap):
        self.log = logger
        self.reset()
        self.mimemap = mimemap
        self.parser = email.parser.Parser(policy = email.policy.default)

    def reset(self):
        self.headers = ''
        self.body = ''
        self.bodymimemain = ''
        self.bodymimesub = ''
        self.attachments = []

    def setheaders(self, h):
        #self.log("EmailBuilder: headers")
        self.headers = h

    def setbody(self, body, main, sub):
        #self.log("EmailBuilder: body")
        self.body = body
        self.bodymimemain = main
        self.bodymimesub = sub

    def addattachment(self, att, filename):
        #self.log("EmailBuilder: attachment")
        self.attachments.append((att, filename))

    def flush(self):
        if not (self.headers and (self.body or self.attachments)):
            #self.log("Not flushing because no headers or no body/attach")
            self.reset()
            return None

        newmsg = email.message.EmailMessage(policy=email.policy.default)
        headerstr = self.headers.decode("UTF-8", errors='replace')
        # print("%s" % headerstr)
        headers = self.parser.parsestr(headerstr, headersonly=True)
        #self.log("EmailBuilder: content-type %s" % headers['content-type'])
        for nm in ('from', 'subject'):
            if nm in headers:
                newmsg.add_header(nm, headers[nm])

        for h in ('to', 'cc'):
            tolist = headers.get_all(h)
            if not tolist:
                continue
            alldests = ""
            for toheader in tolist:
                for dest in toheader.addresses:
                    sd = str(dest).replace('\n', '').replace('\r','')
                    #self.log("EmailBuilder: dest %s" % sd)
                    alldests += sd + ", "
            if alldests:
                alldests = alldests.rstrip(", ")
                newmsg.add_header(h, alldests)

# Decoding the body: the .pst contains the text value decoded from qp
# or base64 (at least that's what libpff sends). Unfortunately, it
# appears that the charset value for subparts (e.g. the html part of a
# multipart/related) is not saved (or not transmitted).
#
# This information is both necessary and unavailable, so we apply an heuristic
# which works in 'most' cases: if we have a charset in the message
# header, hopefully, this is a simple body and the charset
# applies. Else try to decode from utf-8, and use charset=utf-8 if it
# succeeds. Else, send binary and hope for the best (the HTML handler
# still has a chance to get the charset from the HTML header).
#
# There are cases of an HTML UTF-8 text having charset=iso in the
# head. Don't know if the original HTML was borked or if outlook or
# libpff decoded to utf-8 without changing the head charset.
        if self.body:
            if self.bodymimemain == 'text':
                charset = headers.get_content_charset()
                body = ''
                if charset:
                    body = self.body.decode(charset, errors='replace')
                    #self.log("DECODE FROM HEADER CHARSET %s SUCCEEDED"% charset)
                else:
                    try:
                        body = self.body.decode('utf-8')
                        #self.log("DECODE FROM GUESSED UTF-8 SUCCEEDED")
                    except:
                        pass
                if body:
                    #self.log("Unicode body: %s" % body)
                    newmsg.set_content(body, subtype = self.bodymimesub)
                else:
                    newmsg.set_content(self.body, maintype = self.bodymimemain,
                                       subtype = self.bodymimesub)
            else:
                newmsg.set_content(self.body, maintype = self.bodymimemain,
                                   subtype = self.bodymimesub)


        for att in self.attachments:
            fn = att[1]
            ext = met_splitext(fn)[1]
            mime = self.mimemap.get(ext)
            if not mime:
                mime = 'application/octet-stream'
            #self.log("Attachment: filename %s MIME %s" % (fn, mime))
            mt,st = mime.split('/')
            newmsg.add_attachment(att[0], maintype=mt, subtype=st,
                                  filename=fn)

        ret = newmsg.as_string(maxheaderlen=100)
        #newmsg.set_unixfrom("From some@place.org Sun Jan 01 00:00:00 2000")
        #print("%s\n" % newmsg.as_string(unixfrom=True, maxheaderlen=80))
        #self.log("MESSAGE: %s" % ret)
        self.reset()
        return ret
    

class PFFReader(object):
    def __init__(self, logger, infile=sys.stdin):
        self.log = logger
        config = rclconfig.RclConfig()
        dir1 = os.path.join(config.getConfDir(), "examples")
        dir2 = os.path.join(config.datadir, "examples")
        self.mimemap = conftree.ConfStack('mimemap', [dir1, dir2])
        self.infile = infile
        self.fields = {}
        self.msg = EmailBuilder(self.log, self.mimemap)
        
    # Read single parameter from process input: line with param name and size
    # followed by data. The param name is returned as str/unicode, the data
    # as bytes
    def readparam(self):
        inf = self.infile
        s = inf.readline()
        if s == b'':
            return ('', b'')
        s = s.rstrip(b'\n')
        if s == b'':
            return ('', b'')
        l = s.split()
        if len(l) != 2:
            self.log(b'bad line: [' + s + b']', 1, 1)
            return ('', b'')
        paramname = l[0].decode('ASCII').rstrip(':')
        paramsize = int(l[1])
        if paramsize > 0:
            paramdata = inf.read(paramsize)
            if len(paramdata) != paramsize:
                self.log("Bad read: wanted %d, got %d" %
                      (paramsize, len(paramdata)), 1, 1)
                return('', b'')
        else:
            paramdata = b''
        return (paramname, paramdata)

    def mainloop(self):
        basename = ''
        fullpath = ''
        ipath = ''
        while 1:
            name, data = self.readparam()
            if name == "":
                break
            try:
                paramstr = data.decode("UTF-8")
            except:
                paramstr = ''

            if name == 'filename':
                #self.log("filename: %s" %  paramstr)
                fullpath = paramstr
                basename = met_basename(fullpath)
                parentdir = met_basename(met_dirname(fullpath))
                #self.log("basename [%s] parentdir [%s]" % (basename, parentdir))
            elif name == 'data':
                if parentdir == 'Attachments':
                    #self.log("Attachment: %s" % basename)
                    self.msg.addattachment(data, basename)
                else:
                    if basename == 'OutlookHeaders.txt':
                        doc = self.msg.flush()
                        if doc:
                            yield((doc, ipath))
                    elif basename == 'InternetHeaders.txt':
                        #self.log("name: [%s] data: %s" % (name, paramstr[:20]))
                        # This part is the indispensable one. Record
                        # the ipath at this point:
                        if _mswindows:
                            p = pathlib.PureWindowsPath(fullpath)
                        else:
                            p = pathlib.Path(fullpath)
                        # Strip the top dir (/nonexistent.export/)
                        p = p.relative_to(*p.parts[:2])
                        # We use the parent directory as ipath: all
                        # the message parts are in there
                        ipath = str(p.parents[0])
                        self.msg.setheaders(data)
                    elif met_splitext(basename)[0] == 'Message':
                        ext = met_splitext(basename)[1]
                        if ext == '.txt':
                            self.msg.setbody(data, 'text', 'plain')
                        elif ext == '.html':
                            self.msg.setbody(data, 'text', 'html')
                        elif ext == '.rtf':
                            self.msg.setbody(data, 'text', 'rtf')
                        else:
                            # Note: I don't know what happens with a
                            # message body of type, e.g. image/jpg.
                            # This is probably not a big issue,
                            # because there is nothing to index
                            # We raised during dev to see if we would find one,
                            # now just pass 
                            # raise Exception("PST: Unknown body type %s"%ext)
                            pass
                    elif basename == 'ConversationIndex.txt':
                        pass
                    elif basename == 'Recipients.txt':
                        pass
            else:
                raise Exception("Unknown param name: %s" % name)

        #self.log("Out of loop")
        doc = self.msg.flush()
        if doc:
            yield((doc, ipath))
        return


class PstExtractor(object):
    def __init__(self, em):
        self.generator = None
        self.em = em
        if _mswindows:
            self.target = "\\\\?\\c:\\nonexistent"
        else:
            self.target = "/nonexistent"
        self.pffexport = rclexecm.which("pffinstall/mingw32/bin/pffexport")
        if not self.pffexport:
            self.pffexport = rclexecm.which("pffexport")
            if not self.pffexport:
                # No need for anything else. openfile() will return an
                # error at once
                return
        self.cmd = [self.pffexport, "-q", "-t", self.target, "-s"]

    def startCmd(self, filename, ipath=None):
        fullcmd = list(self.cmd)
        if ipath:
            # There is no way to pass an utf-8 string on the command
            # line on Windows. Use base64 encoding
            bip = base64.b64encode(ipath.encode("UTF-8"))
            fullcmd += ["-p", bip.decode("UTF-8")]
        fn = _backslashize(rclexecm.subprocfile(filename))
        fullcmd += [fn,]
        #self.em.rclog("PstExtractor: command: [%s]" % fullcmd)
        try:
            self.proc = subprocess.Popen(fullcmd, stdout=subprocess.PIPE)
        except subprocess.CalledProcessError as err:
            self.em.rclog("Pst: Popen(%s) error: %s" % (fullcmd, err))
            return False
        except OSError as err:
            self.em.rclog("Pst: Popen(%s) OS error: %s" % (fullcmd, err))
            return (False, "")
        except Exception as err:
            self.em.rclog("Pst: Popen(%s) Exception: %s" % (fullcmd, err))
            return (False, "")
        self.filein = self.proc.stdout
        return True

    ###### File type handler api, used by rclexecm ---------->
    def openfile(self, params):
        if not self.pffexport:
            print("RECFILTERROR HELPERNOTFOUND pffexport")
            sys.exit(1);
        self.filename = params["filename:"]
        self.generator = None
        return True

    def getipath(self, params):
        ipath = met_join(self.target + ".export",
                         params["ipath:"].decode("UTF-8"))
        self.em.rclog("getipath: [%s]" % ipath)
        if not self.startCmd(self.filename, ipath=ipath):
            return (False, "", "", rclexecm.RclExecM.eofnow) 
        reader = PFFReader(self.em.rclog, infile=self.filein)
        self.generator = reader.mainloop()
        try:
            doc, ipath = next(self.generator)
            self.em.setmimetype("message/rfc822")
            self.em.rclog("getipath doc len %d [%s] ipath %s" %
                          (len(doc), doc[:20], ipath))
        except StopIteration:
            self.em.rclog("getipath: StopIteration")
            return(False, "", "", rclexecm.RclExecM.eofnow)
        return (True, doc, ipath, False)
        

    def getnext(self, params):
        #self.em.rclog("getnext:")
        if not self.generator:
            #self.em.rclog("starting generator")
            if not self.startCmd(self.filename):
                return False
            reader = PFFReader(self.em.rclog, infile=self.filein)
            self.generator = reader.mainloop()

        ipath = ""
        try:
            doc, ipath = next(self.generator)
            self.em.setmimetype("message/rfc822")
            #self.em.rclog("getnext: ipath %s\ndoc\n%s" % (ipath, doc))
        except StopIteration:
            #self.em.rclog("getnext: end of iteration")
            self.proc.wait(3)
            if self.proc.returncode == 0:
                return(True, "", "", rclexecm.RclExecM.eofnext)
            else:
                self.em.rclog("getnext: subprocess returned code %d" % self.proc.returncode)
                return(False, "", "", rclexecm.RclExecM.eofnow)
        except Exception as ex:
            self.em.rclog("getnext: exception: %s" % ex)
            return(False, "", "", rclexecm.RclExecM.eofnow)
            
        return (True, doc, ipath, rclexecm.RclExecM.noteof)
    

if True:
    # Main program: create protocol handler and extractor and run them
    proto = rclexecm.RclExecM()
    extract = PstExtractor(proto)
    rclexecm.main(proto, extract)
else:
    reader = PFFReader(_deb, infile=sys.stdin.buffer)
    generator = reader.mainloop()
    for doc, ipath in generator:
        _deb("Got %s data len %d" % (ipath, len(doc)))
