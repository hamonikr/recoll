<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
                xmlns:x="http://schemas.openxmlformats.org/spreadsheetml/2006/main">

  <xsl:output omit-xml-declaration="yes"/>

  <xsl:template match="/">
    <div>
      <xsl:apply-templates/> 
    </div>
  </xsl:template>

  <xsl:template match="x:t">
    <p>
      <xsl:value-of select="."/>
    </p>
  </xsl:template>

</xsl:stylesheet>
