#!/usr/bin/env python3
# Copyright (C) 2015 J.F.Dockes
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
######################################
from __future__ import print_function

import sys
from zipfile import ZipFile
import fnmatch
import rclexecm
from rclbasehandler import RclBaseHandler
import rclxslt

meta_stylesheet = '''<?xml version="1.0"?>
<xsl:stylesheet 
 xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
 xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties"
 xmlns:dc="http://purl.org/dc/elements/1.1/"
 xmlns:dcterms="http://purl.org/dc/terms/"
 xmlns:dcmitype="http://purl.org/dc/dcmitype/"
 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">

<!--  <xsl:output method="text"/> -->
  <xsl:output omit-xml-declaration="yes"/>

  <xsl:template match="cp:coreProperties">
    <xsl:text>&#10;</xsl:text>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>
    <xsl:text>&#10;</xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="dc:creator">
    <meta>
    <xsl:attribute name="name">
      <!-- <xsl:value-of select="name()"/> pour sortir tous les meta avec 
       le meme nom que dans le xml (si on devenait dc-natif) -->
      <xsl:text>author</xsl:text> 
    </xsl:attribute>
    <xsl:attribute name="content">
       <xsl:value-of select="."/>
    </xsl:attribute>
    </meta>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="dcterms:modified">
    <meta>
    <xsl:attribute name="name">
      <xsl:text>date</xsl:text> 
    </xsl:attribute>
    <xsl:attribute name="content">
       <xsl:value-of select="."/>
    </xsl:attribute>
    </meta>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="*">
  </xsl:template>

</xsl:stylesheet>
'''

word_tagmatch = 'w:p'
word_xmlns_decls = '''xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
xmlns:ve="http://schemas.openxmlformats.org/markup-compatibility/2006"
xmlns:o="urn:schemas-microsoft-com:office:office"
xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"
xmlns:m="http://schemas.openxmlformats.org/officeDocument/2006/math"
xmlns:v="urn:schemas-microsoft-com:vml"
xmlns:wp="http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing"
xmlns:w10="urn:schemas-microsoft-com:office:word"
xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"
xmlns:wne="http://schemas.microsoft.com/office/word/2006/wordml"
'''
word_moretemplates = ''


xl_tagmatch = 'x:t'
xl_xmlns_decls='''xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
xmlns:x="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
 '''
xl_moretemplates = ''

pp_tagmatch = 'a:t'
pp_xmlns_decls = '''xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" 
xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" 
xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"
'''
# I want to suppress text output for all except a:t, don't know how to do it
# help ! At least get rid of these:
pp_moretemplates = '''<xsl:template match="p:attrName">
</xsl:template>
'''

content_stylesheet = '''<?xml version="1.0"?>
<xsl:stylesheet @XMLNS_DECLS@ >

 <xsl:output omit-xml-declaration="yes"/>

 <xsl:template match="/">
  <div>
  <xsl:apply-templates/> 
  </div>
</xsl:template>

 <xsl:template match="@TAGMATCH@">
  <p>
  <xsl:value-of select="."/>
  </p>
 </xsl:template>

@MORETEMPLATES@

</xsl:stylesheet>
'''

class OXExtractor(RclBaseHandler):
    def __init__(self, em):
        super(OXExtractor, self).__init__(em)


    # Replace values inside data style sheet, depending on type of doc
    def computestylesheet(self, nm):
        decls = globals()[nm + '_xmlns_decls']
        stylesheet = content_stylesheet.replace('@XMLNS_DECLS@', decls)
        tagmatch = globals()[nm + '_tagmatch']
        stylesheet = stylesheet.replace('@TAGMATCH@', tagmatch)
        moretmpl = globals()[nm + '_moretemplates']
        stylesheet = stylesheet.replace('@MORETEMPLATES@', moretmpl)

        return stylesheet
    

    def html_text(self, fn):

        f = open(fn, 'rb')
        zip = ZipFile(f)

        docdata = b'<html><head>'

        try:
            metadata = zip.read("docProps/core.xml")
            if metadata:
                res = rclxslt.apply_sheet_data(meta_stylesheet, metadata)
                docdata += res
        except Exception as err:
            pass

        docdata += b'</head><body>'

        try:
            content= zip.read('word/document.xml')
            stl = self.computestylesheet('word')
            docdata += rclxslt.apply_sheet_data(stl, content)
        except:
            pass

        try:
            content = zip.read('xl/sharedStrings.xml')
            stl = self.computestylesheet('xl')
            docdata += rclxslt.apply_sheet_data(stl, content)
        except:
            pass

        try:
            stl = self.computestylesheet('pp')
            # Note that we'd need a numeric sort really (else we get slide1
            # slide11 slide2)
            for fn in sorted(zip.namelist()):
                if fnmatch.fnmatch(fn, 'ppt/slides/slide*.xml'):
                    content = zip.read(fn)
                    docdata += rclxslt.apply_sheet_data(stl, content)
        except:
            pass

        docdata += b'</body></html>'

        return docdata
    

if __name__ == '__main__':
    proto = rclexecm.RclExecM()
    extract = OXExtractor(proto)
    rclexecm.main(proto, extract)
