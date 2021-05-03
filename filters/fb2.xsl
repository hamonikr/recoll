<?xml version="1.0"?>
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
