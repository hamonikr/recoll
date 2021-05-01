<?xml version="1.0"?>
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
