<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
		xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:output method="html" encoding="UTF-8"/>

  <xsl:template match="/">
    <html>
      <head>
	<xsl:if test="//*[local-name() = 'title']">
	  <title>
	    <xsl:value-of select="//*[local-name() = 'title'][1]"/>
	  </title>
	</xsl:if>
      </head>
      <body>
	<xsl:apply-templates/>
      </body>
    </html>
  </xsl:template>

  <xsl:template match="text()">
    <xsl:if test="string-length(normalize-space(.)) &gt; 0">
      <p><xsl:value-of select="."/></p>
      <xsl:text>
      </xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:template match="*">
    <xsl:apply-templates/>
  </xsl:template>

</xsl:stylesheet>
