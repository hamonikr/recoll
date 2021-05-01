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
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:output method="html" encoding="UTF-8"/>
<xsl:strip-space elements="*" />

<xsl:template match="/">
<html>
  <head>
   <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
   <title>
     Okular notes about: <xsl:value-of select="/documentInfo/@url" />
   </title>
  </head>
  <body>
    <xsl:apply-templates />
  </body>
</html>
</xsl:template>

<xsl:template match="node()">
  <xsl:apply-templates select="@* | node() "/>
</xsl:template>

<xsl:template match="text()">
  <p><xsl:value-of select="."/></p>
<xsl:text >
</xsl:text>
</xsl:template>

<xsl:template match="@contents|@author">
  <p><xsl:value-of select="." /></p>
<xsl:text >
</xsl:text>
</xsl:template>

<xsl:template match="@*"/>

</xsl:stylesheet>
'''

if __name__ == '__main__':
   proto = rclexecm.RclExecM()
   extract = rclgenxslt.XSLTExtractor(proto, stylesheet_all)
   rclexecm.main(proto, extract)

