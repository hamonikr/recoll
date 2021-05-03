<?xml version="1.0"?>
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
