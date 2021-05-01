<?xml version="1.0"?>
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
