#!/usr/bin/env python3
# Copyright (C) 2014-2020 J.F.Dockes
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
# If pdftotext produces no text and the configuration allows it, we may try to
# perform OCR.

import os
import sys
import re
import urllib.request
import subprocess
import tempfile
import glob
import traceback
import atexit
import signal

import rclexecm
import rclconfig

_mswindows = (sys.platform == "win32")

# Test access to the poppler-glib python3 bindings ? This allows
# extracting text from annotations.
# - On Ubuntu, this comes with package gir1.2-poppler-0.18
# - On opensuse the package is named typelib-1_0-Poppler-0_18
# (actual versions may differ of course).
# 
# NOTE: we are using the glib introspection bindings for the Poppler C
# API, which is not the same as the Python bindings of the Poppler C++
# API (poppler-cpp). The interface is quite different
# (e.g. attachments are named "embeddedfiles" in the C++ interface.
havepopplerglib = False
try:
    import gi
    gi.require_version('Poppler', '0.18')
    from gi.repository import Poppler
    havepopplerglib = True
except:
    pass

tmpdir = None

_htmlprefix =b'''<html><head>
<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">
</head><body><pre>'''
_htmlsuffix = b'''</pre></body></html>'''

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
                EMF = None
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
            # Note: the java version of pdftk sometimes/often fails
            # here with writing to stdout:
            #    Error occurred during initialization of VM
            #    Could not allocate metaspace: 1073741824 bytes
            # Maybe unsufficient resources when started from Python ?
            # In any case, the important thing is to discard the
            # output, until we fix the error or preferably find a way
            # to do it with poppler...
            subprocess.check_call(
                [self.pdftk, self.filename, "unpack_files", "output",
                 tmpdir], stdout=sys.stderr)
            self.attachlist = sorted(os.listdir(tmpdir))
            return True
        except Exception as e:
            self.em.rclog("extractAttach: failed: %s" % e)
            # Return true anyway, pdf attachments are no big deal
            return True

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
                        line = m.group(1) + rclexecm.htmlescape(m.group(2)) + \
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
                line = rclexecm.htmlescape(line)

            if re.search(b'<head>', line):
                inheader = True
            if re.search(b'<pre>', line):
                inbody = True

            output.append(line)

        return b'\n'.join(output), isempty

    def _metatag(self, nm, val):
        return b"<meta name=\"" + rclexecm.makebytes(nm) + b"\" content=\"" + \
               rclexecm.htmlescape(rclexecm.makebytes(val)) + b"\">"

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

    def maybemaketmpdir(self):
        global tmpdir
        if tmpdir:
            if not vacuumdir(tmpdir):
                self.em.rclog("openfile: vacuumdir %s failed" % tmpdir)
                return False
        else:
            tmpdir = tempfile.mkdtemp(prefix='rclmpdf')
            if self.pdftk and re.match("/snap/", self.pdftk):
                # We know this is Unix (Ubuntu actually). Check that tmpdir
                # belongs to the user as snap commands can't use /tmp to share
                # files. Don't generate an error as this only affects
                # attachment extraction
                ok = False
                if "TMPDIR" in os.environ:
                    st = os.stat(os.environ["TMPDIR"])
                    if st.st_uid == os.getuid():
                        ok = True
                if not ok:
                    self.em.rclog(
                        "pdftk is a snap command and needs TMPDIR to be "
                        "a directory you own")

    def _process_annotations(self, html):
        doc = Poppler.Document.new_from_file(
            'file://%s' %
            urllib.request.pathname2url(os.path.abspath(self.filename)), None)
        n_pages = doc.get_n_pages()
        all_annots = 0

        # output format
        f = 'P.: {0}, {1:10}, {2:10}: {3}'

        # Array of annotations indexed by page number. The page number
        # here is the physical one (evince -i), not a page label (evince
        # -p). This may be different for some documents.
        abypage = {}
        for i in range(n_pages):
            page = doc.get_page(i)
            pnum = i+1
            annot_mappings = page.get_annot_mapping ()
            num_annots = len(annot_mappings)
            for annot_mapping in annot_mappings:
                atype = annot_mapping.annot.get_annot_type().value_name
                if atype  != 'POPPLER_ANNOT_LINK':
                    # Catch because we sometimes get None values
                    try:
                        atext = f.format(
                            pnum,
                            annot_mapping.annot.get_modified(),
                            annot_mapping.annot.get_annot_type().value_nick,
                            annot_mapping.annot.get_contents()) + "\n"
                        if pnum in abypage:
                            abypage[pnum] += atext
                        else:
                            abypage[pnum] = atext
                    except:
                        pass
        #self.em.rclog("Annotations: %s" % abypage)
        pagevec = html.split(b"\f")
        html = b""
        annotsfield = b""
        pagenum = 1
        for page in pagevec:
            html += page
            if pagenum in abypage:
                html += abypage[pagenum].encode('utf-8')
                annotsfield += abypage[pagenum].encode('utf-8') + b" - " 
            html += b"\f"
            pagenum += 1
        if annotsfield:
            self.em.setfield("pdfannot", annotsfield)
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

        if isempty:
            self.config.setKeyDir(os.path.dirname(self.filename))
            s = self.config.getConfParam("pdfocr")
            if rclexecm.configparamtrue(s):
                try:
                    cmd = [sys.executable, os.path.join(_execdir, "rclocr.py"),
                           self.filename]
                    data = subprocess.check_output(cmd)
                    html = _htmlprefix + rclexecm.htmlescape(data) + _htmlsuffix
                except Exception as e:
                    self.em.rclog("%s failed: %s" % (cmd, e))
                    pass

        if self.extrameta:
            try:
                html = self._setextrameta(html)
            except Exception as err:
                self.em.rclog("Metadata extraction failed: %s %s" %
                              (err, traceback.format_exc()))

        if havepopplerglib:
            try:
                html = self._process_annotations(html)
            except Exception as err:
                self.em.rclog("Annotation extraction failed: %s %s" %
                              (err, traceback.format_exc()))
        return (True, html, "", eof)


    def extractone(self, ipath):
        #self.em.rclog("extractone: [%s]" % ipath)
        if not self.attextractdone:
            if not self.extractAttach():
                return (False, "", "", rclexecm.RclExecM.eofnow)
        path = os.path.join(tmpdir, ipath)
        if os.path.isfile(path):
            f = open(path, "rb")
            docdata = f.read();
            f.close()
        if self.currentindex == len(self.attachlist) - 1:
            eof = rclexecm.RclExecM.eofnext
        else:
            eof = rclexecm.RclExecM.noteof
        return (True, docdata, ipath, eof)

        
    ###### File type handler api, used by rclexecm ---------->
    def openfile(self, params):
        if not self.pdftotext:
            print("RECFILTERROR HELPERNOTFOUND pdftotext")
            sys.exit(1);

        self.filename = rclexecm.subprocfile(params["filename"])

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
        ipath = params["ipath"]
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
            except Exception as ex:
                self.em.rclog("getnext: extractone failed for index %d: %s" %
                                  (self.currentindex, ex))
                return (False, "", "", rclexecm.RclExecM.eofnow)


# Main program: create protocol handler and extractor and run them
_execdir = os.path.dirname(sys.argv[0])
proto = rclexecm.RclExecM()
extract = PDFExtractor(proto)
rclexecm.main(proto, extract)
