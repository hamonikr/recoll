CXXFLAGS ?= @CXXFLAGS@
LIBXAPIAN=@LIBXAPIAN@
XAPIANCXXFLAGS=@XAPIANCXXFLAGS@
XSLT_CFLAGS=@XSLT_CFLAGS@
XSLT_LINKADD=@XSLT_LINKADD@
LIBICONV=@LIBICONV@
INCICONV=@INCICONV@
LIBFAM = @LIBFAM@
RCLLIBVERSION=@RCLLIBVERSION@
X_CFLAGS=@X_CFLAGS@
X_PRE_LIBS=@X_PRE_LIBS@
X_LIBS=@X_LIBS@
X_EXTRA_LIBS=@X_EXTRA_LIBS@
X_LIBX11=@X_LIBX11@
DEFS=@DEFS@

COMMONCPPFLAGS = -I. \
    -I$(top_srcdir)/aspell \
    -I$(top_srcdir)/bincimapmime \
    -I$(top_srcdir)/common \
    -I$(top_srcdir)/index \
    -I$(top_srcdir)/internfile \
    -I$(top_srcdir)/rcldb \
    -I$(top_srcdir)/unac \
    -I$(top_srcdir)/utils \
    -I$(top_srcdir)/xaposix \
    -DBUILDING_RECOLL

AM_CPPFLAGS = -Wall -Wno-unused -std=c++11 \
    $(COMMONCPPFLAGS) \
    $(INCICONV) \
    $(XAPIANCXXFLAGS) \
    $(XSLT_CFLAGS) \
    $(X_CFLAGS) \
    -DRECOLL_DATADIR=\"${pkgdatadir}\" \
    -DREADFILE_ENABLE_ZLIB -DREADFILE_ENABLE_MINIZ -DREADFILE_ENABLE_MD5 \
    -D_GNU_SOURCE \
    $(DEFS)

noinst_PROGRAMS = textsplit utf8iter fstreewalk rclconfig hldata unac mbox

textsplit_SOURCES = trtextsplit.cpp
textsplit_LDADD = ../librecoll.la

utf8iter_SOURCES = trutf8iter.cpp
utf8iter_LDADD = ../librecoll.la

fstreewalk_SOURCES = trfstreewalk.cpp
fstreewalk_LDADD = ../librecoll.la

rclconfig_SOURCES = trrclconfig.cpp
rclconfig_LDADD = ../librecoll.la

hldata_SOURCES = trhldata.cpp
hldata_LDADD = ../librecoll.la

unac_SOURCES = trunac.cpp
unac_LDADD = ../librecoll.la

mbox_SOURCES = trmbox.cpp
mbox_LDADD = ../librecoll.la
