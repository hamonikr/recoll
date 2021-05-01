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
import rclgenxslt

stylesheet_all = '''<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:svg="http://www.w3.org/2000/svg"
  xmlns:dc="http://purl.org/dc/elements/1.1/"
  exclude-result-prefixes="svg"
  >

<xsl:output method="html" encoding="UTF-8"/>

<xsl:template match="/">
  <html>
  <head>
  <xsl:apply-templates select="svg:svg/svg:title"/>
  <xsl:apply-templates select="svg:svg/svg:desc"/>
  <xsl:apply-templates select="svg:svg/svg:metadata/descendant::dc:creator"/>
  <xsl:apply-templates select="svg:svg/svg:metadata/descendant::dc:subject"/>
  <xsl:apply-templates select="svg:svg/svg:metadata/descendant::dc:description"/>
  </head>
  <body>
  <xsl:apply-templates select="//svg:text"/>
  </body>
  </html>
</xsl:template>

<xsl:template match="svg:desc"> 
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

<xsl:template match="dc:subject"> 
  <meta>
  <xsl:attribute name="name">keywords</xsl:attribute>
  <xsl:attribute name="content">
     <xsl:value-of select="."/>
  </xsl:attribute>
  </meta><xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="dc:description"> 
  <meta>
  <xsl:attribute name="name">description</xsl:attribute>
  <xsl:attribute name="content">
     <xsl:value-of select="."/>
  </xsl:attribute>
  </meta><xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="svg:title"> 
  <title><xsl:value-of select="."/></title><xsl:text>
  </xsl:text>
</xsl:template>
	    
<xsl:template match="svg:text"> 
  <p><xsl:value-of select="."/></p><xsl:text>
  </xsl:text>
</xsl:template>

</xsl:stylesheet>
'''

if __name__ == '__main__':
    proto = rclexecm.RclExecM()
    extract = rclgenxslt.XSLTExtractor(proto, stylesheet_all)
    rclexecm.main(proto, extract)
