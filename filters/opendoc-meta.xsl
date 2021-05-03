<?xml version="1.0"?>
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
