<?xml version="1.0"?>
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
