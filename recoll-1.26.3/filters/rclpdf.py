#!/usr/bin/env python3
# Copyright (C) 2014 J.F.Dockes
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# Recoll PDF extractor, with support for attachments
#
# pdftotext sometimes outputs unescaped text inside HTML text sections.
# We try to correct.
#
# If pdftotext produces no text and tesseract is available, we try to
# perform OCR. As this can be very slow and the result not always
# good, we only do this if this is required by the configuration
#
# We guess the OCR language in order of preference:
#  - From the content of a ".ocrpdflang" file if it exists in the same
#    directory as the PDF
#  - Else from the pdfocrlang in recoll.conf
#  - Else from an RECOLL_TESSERACT_LANG environment variable
#  - From the content of $RECOLL_CONFDIR/ocrpdf
#  - Default to "eng"

from __future__ import print_function

import os
import sys
import re
import rclexecm
import subprocess
import tempfile
import atexit
import signal
import rclconfig
import glob
import traceback

_mswindows = (sys.platform == "win32")
tmpdir = None

def finalcleanup():
    if tmpdir:
        vacuumdir(tmpdir)
        os.rmdir(tmpdir)

def signal_handler(signal, frame):
    sys.exit(1)

atexit.register(finalcleanup)

# Not all signals necessary exist on all systems, use catch
try: signal.signal(signal.SIGHUP, signal_handler)
except: pass
try: signal.signal(signal.SIGINT, signal_handler)
except: pass
try: signal.signal(signal.SIGQUIT, signal_handler)
except: pass
try: signal.signal(signal.SIGTERM, signal_handler)
except: pass

def vacuumdir(dir):
    if dir:
        for fn in os.listdir(dir):
            path = os.path.join(dir, fn)
            if os.path.isfile(path):
                os.unlink(path)
    return True

class PDFExtractor:
    def __init__(self, em):
        self.currentindex = 0
        self.pdftotext = None
        self.pdfinfo = None
        self.pdftk = None
        self.em = em
        self.tesseract = None

        # Avoid picking up a default version on Windows, we want ours
        if not _mswindows:
            self.pdftotext = rclexecm.which("pdftotext")
        if not self.pdftotext:
            self.pdftotext = rclexecm.which("poppler/pdftotext")
            if not self.pdftotext:
                # No need for anything else. openfile() will return an
                # error at once
                return

        self.config = rclconfig.RclConfig()
        self.confdir = self.config.getConfDir()
        # The user can set a list of meta tags to be extracted from
        # the XMP metadata packet. These are specified as
        # (xmltag,rcltag) pairs
        self.extrameta = self.config.getConfParam("pdfextrameta")
        if self.extrameta:
            self.extrametafix = self.config.getConfParam("pdfextrametafix")
            self._initextrameta()

        # Check if we need to escape portions of text where old
        # versions of pdftotext output raw HTML special characters.
        self.needescape = True
        try:
            version = subprocess.check_output([self.pdftotext, "-v"],
                                              stderr=subprocess.STDOUT)
            major,minor,rev = version.split()[2].split('.')
            # Don't know exactly when this changed but it's fixed in
            # jessie 0.26.5
            if int(major) > 0 or int(minor) >= 26:
                self.needescape = False
        except:
            pass
        
        # See if we'll try to perform OCR. Need the commands and the
        # either the presence of a file in the config dir (historical)
        # or a set config variable.
        self.ocrpossible = False
        self.tesseract = rclexecm.which("tesseract")
        if self.tesseract:
            self.pdftoppm = rclexecm.which("pdftoppm")
            if self.pdftoppm:
                self.ocrpossible = True
                self.maybemaketmpdir()
        # self.em.rclog("OCRPOSSIBLE: %d" % self.ocrpossible)

        # Pdftk is optionally used to extract attachments. This takes
        # a hit on performance even in the absence of any attachments,
        # so it can be disabled in the configuration.
        self.attextractdone = False
        self.attachlist = []
        cf_attach = self.config.getConfParam("pdfattach")
        cf_attach = rclexecm.configparamtrue(cf_attach)
        if cf_attach:
            self.pdftk = rclexecm.which("pdftk")
        if self.pdftk:
            self.maybemaketmpdir()

    def _initextrameta(self):
        if not _mswindows:
            self.pdfinfo = rclexecm.which("pdfinfo")
        if not self.pdfinfo:
            self.pdfinfo = rclexecm.which("poppler/pdfinfo")
        if not self.pdfinfo:
            self.extrameta = None
            return

        # extrameta is like "metanm|rclnm ...", where |rclnm maybe absent (keep
        # original name). Parse into a list of pairs.
        l = self.extrameta.split()
        self.extrameta = []
        for e in l:
            l1 = e.split('|')
            if len(l1) == 1:
                l1.append(l1[0])
            self.extrameta.append(l1)

        # Using lxml because it is better with
        # namespaces. With xml, we'd have to walk the XML tree
        # first, extracting all xmlns attributes and
        # constructing a tree (I tried and did not succeed in
        # doing this actually). lxml does it partially for
        # us. See http://stackoverflow.com/questions/14853243/
        #    parsing-xml-with-namespace-in-python-via-elementtree
        global ET
        #import xml.etree.ElementTree as ET
        try:
            import lxml.etree as ET
        except Exception as err:
            self.em.rclog("Can't import lxml etree: %s" % err)
            self.extrameta = None
            self.pdfinfo = None
            return

        self.re_head = re.compile(br'<head>', re.IGNORECASE)
        self.re_xmlpacket = re.compile(br'<\?xpacket[ 	]+begin.*\?>' +
                                       br'(.*)' + br'<\?xpacket[ 	]+end',
                                       flags = re.DOTALL)
        global EMF
        EMF = None
        if self.extrametafix:
            try:
                import importlib.util
                spec = importlib.util.spec_from_file_location(
                    'pdfextrametafix', self.extrametafix)
                EMF = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(EMF)
            except Exception as err:
                self.em.rclog("Import extrametafix failed: %s" % err)
                pass

    # Extract all attachments if any into temporary directory
    def extractAttach(self):
        if self.attextractdone:
            return True
        self.attextractdone = True

        global tmpdir
        if not tmpdir or not self.pdftk:
            # no big deal
            return True

        try:
            vacuumdir(tmpdir)
            subprocess.check_call([self.pdftk, self.filename, "unpack_files",
                                   "output", tmpdir])
            self.attachlist = sorted(os.listdir(tmpdir))
            return True
        except Exception as e:
            self.em.rclog("extractAttach: failed: %s" % e)
            # Return true anyway, pdf attachments are no big deal
            return True

    def extractone(self, ipath):
        #self.em.rclog("extractone: [%s]" % ipath)
        if not self.attextractdone:
            if not self.extractAttach():
                return (False, "", "", rclexecm.RclExecM.eofnow)
        path = os.path.join(tmpdir, ipath)
        if os.path.isfile(path):
            f = open(path)
            docdata = f.read();
            f.close()
        if self.currentindex == len(self.attachlist) - 1:
            eof = rclexecm.RclExecM.eofnext
        else:
            eof = rclexecm.RclExecM.noteof
        return (True, docdata, ipath, eof)


    # Try to guess tesseract language. This should depend on the input
    # file, but we have no general way to determine it. So use the
    # environment and hope for the best.
    def guesstesseractlang(self):
        tesseractlang = ""

        # First look for a language def file in the file's directory 
        pdflangfile = os.path.join(os.path.dirname(self.filename),
                                   b".ocrpdflang")
        if os.path.isfile(pdflangfile):
            tesseractlang = open(pdflangfile, "r").read().strip()
        if tesseractlang:
            return tesseractlang

        # Then look for a global option. The normal way now that we
        # have config reading capability in the handlers is to use the
        # config. Then, for backwards compat, environment variable and
        # file inside the configuration directory
        tesseractlang = self.config.getConfParam("pdfocrlang")
        if tesseractlang:
            return tesseractlang
        tesseractlang = os.environ.get("RECOLL_TESSERACT_LANG", "");
        if tesseractlang:
            return tesseractlang
        pdflangfile = os.path.join(self.confdir, "ocrpdf")
        if os.path.isfile(pdflangfile):
            tesseractlang = open(pdflangfile, "r").read().strip()
        if tesseractlang:
            return tesseractlang

        # Half-assed trial to guess from LANG then default to english
        localelang = os.environ.get("LANG", "").split("_")[0]
        if localelang == "en":
            tesseractlang = "eng"
        elif localelang == "de":
            tesseractlang = "deu"
        elif localelang == "fr":
            tesseractlang = "fra"
        if tesseractlang:
            return tesseractlang

        if not tesseractlang:
            tesseractlang = "eng"
        return tesseractlang

    # PDF has no text content and tesseract is available. Give OCR a try
    def ocrpdf(self):

        global tmpdir
        if not tmpdir:
            return b""

        tesseractlang = self.guesstesseractlang()
        # self.em.rclog("tesseractlang %s" % tesseractlang)

        tesserrorfile = os.path.join(tmpdir, "tesserrorfile")
        tmpfile = os.path.join(tmpdir, "ocrXXXXXX")

        # Split pdf pages
        try:
            vacuumdir(tmpdir)
            subprocess.check_call([self.pdftoppm, "-r", "300", self.filename,
                                   tmpfile])
        except Exception as e:
            self.em.rclog("pdftoppm failed: %s" % e)
            return b""

        files = glob.glob(tmpfile + "*")
        for f in files:
            out = b''
            try:
                out = subprocess.check_output([self.tesseract, f, f, "-l",
                                               tesseractlang],
                                              stderr = subprocess.STDOUT)
            except Exception as e:
                self.em.rclog("tesseract failed: %s" % e)

            errlines = out.split(b'\n')
            if len(errlines) > 2:
                self.em.rclog("Tesseract error: %s" % out)

        # Concatenate the result files
        files = glob.glob(tmpfile + "*" + ".txt")
        data = b""
        for f in files:
            data += open(f, "rb").read()

        return b'''<html><head>
        <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">
        </head><body><pre>''' + \
        self.em.htmlescape(data) + \
        b'''</pre></body></html>'''


    # pdftotext (used to?) badly escape text inside the header
    # fields. We do it here. This is not an html parser, and depends a
    # lot on the actual format output by pdftotext.
    # We also determine if the doc has actual content, for triggering OCR
    def _fixhtml(self, input):
        #print input
        inheader = False
        inbody = False
        didcs = False
        output = []
        isempty = True
        for line in input.split(b'\n'):
            if re.search(b'</head>', line):
                inheader = False
            if re.search(b'</pre>', line):
                inbody = False
            if inheader:
                if not didcs:
                    output.append(b'<meta http-equiv="Content-Type"' + \
                              b'content="text/html; charset=UTF-8">\n')
                    didcs = True
                if self.needescape:
                    m = re.search(b'''(.*<title>)(.*)(<\/title>.*)''', line)
                    if not m:
                        m = re.search(b'''(.*content=")(.*)(".*/>.*)''', line)
                    if m:
                        line = m.group(1) + self.em.htmlescape(m.group(2)) + \
                               m.group(3)

                # Recoll treats "Subject" as a "title" element
                # (based on emails). The PDF "Subject" metadata
                # field is more like an HTML "description"
                line = re.sub(b'name="Subject"', b'name="Description"', line, 1)

            elif inbody:
                s = line[0:1]
                if s != b"\x0c" and s != b"<":
                    isempty = False
                # We used to remove end-of-line hyphenation (and join
                # lines), but but it's not clear that we should do
                # this as pdftotext without the -layout option does it ?
                line = self.em.htmlescape(line)

            if re.search(b'<head>', line):
                inheader = True
            if re.search(b'<pre>', line):
                inbody = True

            output.append(line)

        return b'\n'.join(output), isempty

    def _metatag(self, nm, val):
        return b"<meta name=\"" + rclexecm.makebytes(nm) + b"\" content=\"" + \
               self.em.htmlescape(rclexecm.makebytes(val)) + b"\">"

    # metaheaders is a list of (nm, value) pairs
    def _injectmeta(self, html, metaheaders):
        metatxt = b''
        for nm, val in metaheaders:
            metatxt += self._metatag(nm, val) + b'\n'
        if not metatxt:
            return html
        res = self.re_head.sub(b'<head>\n' + metatxt, html)
        #self.em.rclog("Substituted html: [%s]"%res)
        if res:
            return res
        else:
            return html
    
    def _xmltreetext(self, elt):
        '''Extract all text content from subtree'''
        text = ''
        for e in elt.iter():
            if e.text:
                text += e.text + " "
        return text.strip()
        # or: return reduce((lambda t,p : t+p+' '),
        #       [e.text for e in elt.iter() if e.text]).strip()
        
    def _setextrameta(self, html):
        if not self.pdfinfo:
            return html

        emf = EMF.MetaFixer() if EMF else None

        # Execute pdfinfo and extract the XML packet
        all = subprocess.check_output([self.pdfinfo, "-meta", self.filename])
        res = self.re_xmlpacket.search(all)
        xml = res.group(1) if res else ''
        # self.em.rclog("extrameta: XML: [%s]" % xml)
        if not xml:
            return html

        metaheaders = []
        # The namespace thing is a drag. Can't do it from the top. See
        # the stackoverflow ref above. Maybe we'd be better off just
        # walking the full tree and building the namespaces dict.
        root = ET.fromstring(xml)
        #self.em.rclog("NSMAP: %s"% root.nsmap)
        namespaces = {'rdf' : "http://www.w3.org/1999/02/22-rdf-syntax-ns#"}
        rdf = root.find("rdf:RDF", namespaces)
        #self.em.rclog("RDF NSMAP: %s"% rdf.nsmap)
        if rdf is None:
            return html
        rdfdesclist = rdf.findall("rdf:Description", rdf.nsmap)
        for metanm,rclnm in self.extrameta:
            for rdfdesc in rdfdesclist:
                try:
                    elts = rdfdesc.findall(metanm, rdfdesc.nsmap)
                except:
                    # We get an exception when this rdf:Description does not
                    # define the required namespace.
                    continue
                
                if elts:
                    for elt in elts:
                        text = None
                        try:
                            # First try to get text from a custom element handler
                            text = emf.metafixelt(metanm, elt)
                        except:
                            pass
                        
                        if text is None:
                            # still nothing here, read the element text
                            text = self._xmltreetext(elt)
                            try:
                                # try to run metafix
                                text = emf.metafix(metanm, text)
                            except:
                                pass

                        if text:
                            # Can't use setfield as it only works for
                            # text/plain output at the moment.
                            #self.em.rclog("Appending: (%s,%s)"%(rclnm,text))
                            metaheaders.append((rclnm, text))
                else:
                    # Some docs define the values as attributes. don't
                    # know if this is valid but anyway...
                    try:
                        prefix,nm = metanm.split(":")
                        fullnm = "{%s}%s" % (rdfdesc.nsmap[prefix], nm)
                    except:
                        fullnm = metanm
                    text = rdfdesc.get(fullnm)
                    if text:
                        try:
                            # try to run metafix
                            text = emf.metafix(metanm, text)
                        except:
                            pass
                        metaheaders.append((rclnm, text))
        if metaheaders:
            if emf:
                try:
                    emf.wrapup(metaheaders)
                except:
                    pass
            return self._injectmeta(html, metaheaders)
        else:
            return html

    def _selfdoc(self):
        '''Extract the text from the pdf doc (as opposed to attachment)'''
        self.em.setmimetype('text/html')

        if self.attextractdone and len(self.attachlist) == 0:
            eof = rclexecm.RclExecM.eofnext
        else:
            eof = rclexecm.RclExecM.noteof
            
        html = subprocess.check_output([self.pdftotext, "-htmlmeta", "-enc",
                                        "UTF-8", "-eol", "unix", "-q",
                                        self.filename, "-"])

        html, isempty = self._fixhtml(html)
        #self.em.rclog("ISEMPTY: %d : data: \n%s" % (isempty, html))

        if isempty and self.ocrpossible:
            self.config.setKeyDir(os.path.dirname(self.filename))
            s = self.config.getConfParam("pdfocr")
            cf_doocr = rclexecm.configparamtrue(s)
            file_doocr = os.path.isfile(os.path.join(self.confdir, "ocrpdf"))
            if cf_doocr or file_doocr:
                html = self.ocrpdf()

        if self.extrameta:
            try:
                html = self._setextrameta(html)
            except Exception as err:
                self.em.rclog("Metadata extraction failed: %s %s" %
                              (err, traceback.format_exc()))

        return (True, html, "", eof)


    def maybemaketmpdir(self):
        global tmpdir
        if tmpdir:
            if not vacuumdir(tmpdir):
                self.em.rclog("openfile: vacuumdir %s failed" % tmpdir)
                return False
        else:
            tmpdir = tempfile.mkdtemp(prefix='rclmpdf')
        
    ###### File type handler api, used by rclexecm ---------->
    def openfile(self, params):
        if not self.pdftotext:
            print("RECFILTERROR HELPERNOTFOUND pdftotext")
            sys.exit(1);

        self.filename = rclexecm.subprocfile(params["filename:"])

        #self.em.rclog("openfile: [%s]" % self.filename)
        self.currentindex = -1
        self.attextractdone = False

        if self.pdftk:
            preview = os.environ.get("RECOLL_FILTER_FORPREVIEW", "no")
            if preview != "yes":
                # When indexing, extract attachments at once. This
                # will be needed anyway and it allows generating an
                # eofnext error instead of waiting for actual eof,
                # which avoids a bug in recollindex up to 1.20
                self.extractAttach()
        else:
            self.attextractdone = True
        return True

    def getipath(self, params):
        ipath = params["ipath:"]
        ok, data, ipath, eof = self.extractone(ipath)
        return (ok, data, ipath, eof)
        
    def getnext(self, params):
        # self.em.rclog("getnext: current %d" % self.currentindex)
        if self.currentindex == -1:
            self.currentindex = 0
            return self._selfdoc()
        else:
            self.em.setmimetype('')

            if not self.attextractdone:
                if not self.extractAttach():
                    return (False, "", "", rclexecm.RclExecM.eofnow)

            if self.currentindex >= len(self.attachlist):
                return (False, "", "", rclexecm.RclExecM.eofnow)
            try:
                ok, data, ipath, eof = \
                    self.extractone(self.attachlist[self.currentindex])
                self.currentindex += 1

                #self.em.rclog("getnext: returning ok for [%s]" % ipath)
                return (ok, data, ipath, eof)
            except:
                return (False, "", "", rclexecm.RclExecM.eofnow)


# Main program: create protocol handler and extractor and run them
proto = rclexecm.RclExecM()
extract = PDFExtractor(proto)
rclexecm.main(proto, extract)
