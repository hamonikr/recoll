#!/usr/bin/env python3
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
######################################

from __future__ import print_function

import sys
import rclexecm
import rclxslt
from rclbasehandler import RclBaseHandler
from zipfile import ZipFile

stylesheet_meta = '''<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" 
  xmlns:xlink="http://www.w3.org/1999/xlink" 
  xmlns:dc="http://purl.org/dc/elements/1.1/" 
  xmlns:meta="urn:oasis:names:tc:opendocument:xmlns:meta:1.0" 
  xmlns:ooo="http://openoffice.org/2004/office"
  exclude-result-prefixes="office xlink meta ooo dc"
  >

<xsl:output method="html" encoding="UTF-8"/>

<xsl:template match="/office:document-meta">
  <xsl:apply-templates select="office:meta/dc:description"/>
  <xsl:apply-templates select="office:meta/dc:subject"/>
  <xsl:apply-templates select="office:meta/dc:title"/>
  <xsl:apply-templates select="office:meta/meta:keyword"/>
  <xsl:apply-templates select="office:meta/dc:creator"/>
</xsl:template>

<xsl:template match="dc:title">
<title> <xsl:value-of select="."/> </title><xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="dc:description">
  <meta>
  <xsl:attribute name="name">abstract</xsl:attribute>
  <xsl:attribute name="content">
     <xsl:value-of select="."/>
  </xsl:attribute>
  </meta><xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="dc:subject">
  <meta>
  <xsl:attribute name="name">keywords</xsl:attribute>
  <xsl:attribute name="content">
     <xsl:value-of select="."/>
  </xsl:attribute>
  </meta><xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="dc:creator">
  <meta>
  <xsl:attribute name="name">author</xsl:attribute>
  <xsl:attribute name="content">
     <xsl:value-of select="."/>
  </xsl:attribute>
  </meta><xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="meta:keyword">
  <meta>
  <xsl:attribute name="name">keywords</xsl:attribute>
  <xsl:attribute name="content">
     <xsl:value-of select="."/>
  </xsl:attribute>
  </meta><xsl:text>
</xsl:text>
</xsl:template>

</xsl:stylesheet>
'''

stylesheet_content  = '''<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
  exclude-result-prefixes="text"
>

<xsl:output method="html" encoding="UTF-8"/>

<xsl:template match="text:p">
  <p><xsl:apply-templates/></p><xsl:text>
  </xsl:text>
</xsl:template>

<xsl:template match="text:h">
<p><xsl:apply-templates/></p><xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="text:s">
<xsl:text> </xsl:text>
</xsl:template>

<xsl:template match="text:line-break">
<br />
</xsl:template>

<xsl:template match="text:tab">
<xsl:text>    </xsl:text>
</xsl:template>

</xsl:stylesheet>
'''

class OOExtractor(RclBaseHandler):
    def __init__(self, em):
        super(OOExtractor, self).__init__(em)


    def html_text(self, fn):

        f = open(fn, 'rb')
        zip = ZipFile(f)

        docdata = b'<html>\n<head>\n<meta http-equiv="Content-Type"' \
                  b'content="text/html; charset=UTF-8">'

        # Wrap metadata extraction because it can sometimes throw
        # while the main text will be valid
        try:
            metadata = zip.read("meta.xml")
            if metadata:
                res = rclxslt.apply_sheet_data(stylesheet_meta, metadata)
                docdata += res
        except:
            # To be checked. I'm under the impression that I get this when
            # nothing matches?
            #self.em.rclog("No/bad metadata in %s" % fn)
            pass

        docdata += b'</head>\n<body>\n'

        content = zip.read("content.xml")
        if content:
            res = rclxslt.apply_sheet_data(stylesheet_content, content)
            docdata += res
        docdata += b'</body></html>'

        return docdata
    

if __name__ == '__main__':
    proto = rclexecm.RclExecM()
    extract = OOExtractor(proto)
    rclexecm.main(proto, extract)
