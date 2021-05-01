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
import rclgenxslt

stylesheet_all = '''<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:fb="http://www.gribuser.ru/xml/fictionbook/2.0"
  exclude-result-prefixes="fb"
  >

<xsl:output method="html" encoding="UTF-8"/>

<xsl:template match="/fb:FictionBook">
 <html>
  <xsl:apply-templates select="fb:description"/>
  <xsl:apply-templates select="fb:body"/>
 </html>
</xsl:template>

<xsl:template match="fb:description">
  <head>
    <xsl:apply-templates select="fb:title-info"/>
  </head><xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="fb:description/fb:title-info">
    <xsl:apply-templates select="fb:book-title"/>
    <xsl:apply-templates select="fb:author"/>
</xsl:template>

<xsl:template match="fb:description/fb:title-info/fb:book-title">
<title> <xsl:value-of select="."/> </title>
</xsl:template>

<xsl:template match="fb:description/fb:title-info/fb:author">
  <meta>
  <xsl:attribute name="name">author</xsl:attribute>
  <xsl:attribute name="content">
     <xsl:value-of select="fb:first-name"/><xsl:text> </xsl:text>
     <xsl:value-of select="fb:middle-name"/><xsl:text> </xsl:text>
     <xsl:value-of select="fb:last-name"/>
  </xsl:attribute>
  </meta>
</xsl:template>

<xsl:template match="fb:body">
 <body>
 <xsl:apply-templates select="fb:section"/>
 </body>
</xsl:template>

<xsl:template match="fb:body/fb:section">
  <xsl:for-each select="fb:p">
  <p><xsl:value-of select="."/></p>
  </xsl:for-each>
</xsl:template>

</xsl:stylesheet>
'''

if __name__ == '__main__':
    proto = rclexecm.RclExecM()
    extract = rclgenxslt.XSLTExtractor(proto, stylesheet_all)
    rclexecm.main(proto, extract)
