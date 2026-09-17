<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="html" encoding="utf-8"/>

  <xsl:template match="/toc">
    <html>
      <head>
        <link rel="stylesheet" href="layout.css"/>
        <meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
        <title>Altirra Help: Contents</title>
        <style>
          ul.web-toc { line-height: 1.6; }
          ul.web-toc ul { margin-bottom: 0.35em; }
        </style>
      </head>
      <body>
        <div class="header">
          <div class="header-banner">Altirra Help</div>
          <div class="header-topic">Contents</div>
        </div>
        <div class="main">
          <ul class="web-toc">
            <xsl:apply-templates select="t"/>
          </ul>
        </div>
      </body>
    </html>
  </xsl:template>

  <xsl:template match="t">
    <li>
      <xsl:choose>
        <xsl:when test="@href">
          <a href="{@href}"><xsl:value-of select="@name"/></a>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="@name"/>
        </xsl:otherwise>
      </xsl:choose>
      <xsl:if test="t">
        <ul><xsl:apply-templates select="t"/></ul>
      </xsl:if>
    </li>
  </xsl:template>
</xsl:stylesheet>
