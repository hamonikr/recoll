#!/usr/bin/env python3
# Copyright (C) 2014-2018 J.F.Dockes
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

import sys
import rclexecm
import rclgenxslt

stylesheet = '''<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" 
  xmlns:xlink="http://www.w3.org/1999/xlink" 
  xmlns:dc="http://purl.org/dc/elements/1.1/" 
  xmlns:meta="urn:oasis:names:tc:opendocument:xmlns:meta:1.0" 
  xmlns:ooo="http://openoffice.org/2004/office"
  xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
  exclude-result-prefixes="office xlink meta ooo dc text"
  >

  <xsl:output method="html" encoding="UTF-8"/>

  <xsl:template match="/">
    <html>
      <head>
        <xsl:apply-templates select="/office:document/office:meta" />
      </head>
      <body>
        <xsl:apply-templates select="/office:document/office:body" />
      </body></html>
  </xsl:template>


  <xsl:template match="/office:document/office:meta">
    <xsl:apply-templates select="dc:title"/>
    <xsl:apply-templates select="dc:description"/>
    <xsl:apply-templates select="dc:subject"/>
    <xsl:apply-templates select="meta:keyword"/>
    <xsl:apply-templates select="dc:creator"/>
  </xsl:template>

  <xsl:template match="/office:document/office:body">
    <xsl:apply-templates select=".//text:p" />
    <xsl:apply-templates select=".//text:h" />
    <xsl:apply-templates select=".//text:s" />
    <xsl:apply-templates select=".//text:line-break" />
    <xsl:apply-templates select=".//text:tab" />
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

  <xsl:template match="office:body//text:p">
    <p><xsl:apply-templates/></p><xsl:text>
  </xsl:text>
  </xsl:template>

  <xsl:template match="office:body//text:h">
    <p><xsl:apply-templates/></p><xsl:text>
  </xsl:text>
  </xsl:template>

  <xsl:template match="office:body//text:s">
    <xsl:text> </xsl:text>
  </xsl:template>

  <xsl:template match="office:body//text:line-break">
    <br />
  </xsl:template>

  <xsl:template match="office:body//text:tab">
    <xsl:text>    </xsl:text>
  </xsl:template>

</xsl:stylesheet>
'''

if __name__ == '__main__':
    proto = rclexecm.RclExecM()
    extract = rclgenxslt.XSLTExtractor(proto, stylesheet)
    rclexecm.main(proto, extract)
