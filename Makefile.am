
# Conditionally enable building the small test drivers, but don't
# distribute them, they are not generally useful
if COND_TESTMAINS
  MAYBE_TESTMAINS = testmains
endif
SUBDIRS = . $(MAYBE_TESTMAINS)
DIST_SUBDIRS = .

CXXFLAGS ?= @CXXFLAGS@
LIBXAPIAN=@LIBXAPIAN@
XAPIANCXXFLAGS=@XAPIANCXXFLAGS@
XSLT_CFLAGS=@XSLT_CFLAGS@
XSLT_LIBS=@XSLT_LIBS@
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

ACLOCAL_AMFLAGS = -I m4

if NOTHREADS
    LIBTHREADS=
else
    LIBTHREADS= $(LIBSYSTHREADS)
endif

librcldir = $(libdir)/recoll
librcl_LTLIBRARIES = librecoll.la

librecoll_la_SOURCES = \
aspell/rclaspell.cpp \
aspell/rclaspell.h \
bincimapmime/convert.cc \
bincimapmime/convert.h \
bincimapmime/mime-inputsource.h \
bincimapmime/mime-parsefull.cc \
bincimapmime/mime-parseonlyheader.cc \
bincimapmime/mime-printbody.cc \
bincimapmime/mime-utils.h \
bincimapmime/mime.cc \
bincimapmime/mime.h \
common/webstore.cpp \
common/webstore.h \
common/conf_post.h \
common/cstr.cpp \
common/cstr.h \
common/rclconfig.cpp \
common/rclconfig.h \
common/rclinit.cpp \
common/rclinit.h \
common/syngroups.cpp \
common/syngroups.h \
common/textsplit.cpp \
common/textsplitko.cpp \
common/textsplit.h \
common/unacpp.cpp \
common/unacpp.h \
common/uproplist.h \
common/utf8fn.cpp \
common/utf8fn.h \
index/webqueuefetcher.cpp \
index/webqueuefetcher.h \
index/checkretryfailed.cpp \
index/checkretryfailed.h \
index/exefetcher.cpp \
index/exefetcher.h \
index/fetcher.cpp \
index/fetcher.h \
index/fsfetcher.cpp \
index/fsfetcher.h \
index/idxdiags.h \
index/idxdiags.cpp \
index/idxstatus.h \
index/idxstatus.cpp \
index/mimetype.cpp \
index/mimetype.h \
index/rclmon.h \
index/recollindex.h \
index/subtreelist.cpp \
index/subtreelist.h \
internfile/Filter.h \
internfile/extrameta.cpp \
internfile/extrameta.h \
internfile/htmlparse.cpp \
internfile/htmlparse.h \
internfile/indextext.h \
internfile/internfile.cpp \
internfile/internfile.h \
internfile/mh_exec.cpp \
internfile/mh_exec.h \
internfile/mh_execm.cpp \
internfile/mh_execm.h \
internfile/mh_html.cpp \
internfile/mh_html.h \
internfile/mh_mail.cpp \
internfile/mh_mail.h \
internfile/mh_mbox.cpp \
internfile/mh_mbox.h \
internfile/mh_null.h \
internfile/mh_symlink.h \
internfile/mh_text.cpp \
internfile/mh_text.h \
internfile/mh_unknown.h \
internfile/mh_xslt.cpp \
internfile/mh_xslt.h \
internfile/mimehandler.cpp \
internfile/mimehandler.h \
internfile/myhtmlparse.cpp \
internfile/myhtmlparse.h \
internfile/txtdcode.cpp \
internfile/uncomp.cpp \
internfile/uncomp.h \
query/docseq.cpp \
query/docseq.h \
query/docseqdb.cpp \
query/docseqdb.h \
query/docseqdocs.h \
query/docseqhist.cpp \
query/docseqhist.h \
query/dynconf.cpp \
query/dynconf.h \
query/filtseq.cpp \
query/filtseq.h \
query/plaintorich.cpp \
query/plaintorich.h \
query/qresultstore.cpp \
query/qresultstore.h \
query/recollq.cpp \
query/recollq.h \
query/reslistpager.cpp \
query/reslistpager.h \
query/sortseq.cpp \
query/sortseq.h \
query/wasaparse.ypp \
query/wasaparseaux.cpp \
query/wasaparserdriver.h \
query/wasatorcl.h \
rcldb/daterange.cpp \
rcldb/daterange.h \
rcldb/expansiondbs.cpp \
rcldb/expansiondbs.h \
rcldb/rclabstract.cpp \
rcldb/rclabsfromtext.cpp \
rcldb/rcldb.cpp \
rcldb/rcldb.h \
rcldb/rcldb_p.h \
rcldb/rcldoc.cpp \
rcldb/rcldoc.h \
rcldb/rcldups.cpp \
rcldb/rclquery.cpp \
rcldb/rclquery.h \
rcldb/rclquery_p.h \
rcldb/rclterms.cpp \
rcldb/rclvalues.cpp \
rcldb/rclvalues.h \
rcldb/searchdata.cpp \
rcldb/searchdata.h \
rcldb/searchdatatox.cpp \
rcldb/searchdataxml.cpp \
rcldb/stemdb.cpp \
rcldb/stemdb.h \
rcldb/stoplist.cpp \
rcldb/stoplist.h \
rcldb/synfamily.cpp \
rcldb/synfamily.h \
rcldb/termproc.h \
rcldb/xmacros.h \
unac/unac.cpp \
unac/unac.h \
unac/unac_version.h \
utils/appformime.cpp \
utils/appformime.h \
utils/base64.cpp \
utils/base64.h \
utils/cancelcheck.cpp \
utils/cancelcheck.h \
utils/chrono.h \
utils/chrono.cpp \
utils/circache.cpp \
utils/circache.h \
utils/closefrom.cpp \
utils/closefrom.h \
utils/cmdtalk.cpp \
utils/cmdtalk.h \
utils/conftree.cpp \
utils/conftree.h \
utils/copyfile.cpp \
utils/copyfile.h \
utils/cpuconf.cpp \
utils/cpuconf.h \
utils/dlib.cpp \
utils/dlib.h \
utils/ecrontab.cpp \
utils/ecrontab.h \
utils/execmd.cpp \
utils/execmd.h \
utils/fileudi.cpp \
utils/fileudi.h \
utils/fstreewalk.cpp \
utils/fstreewalk.h \
utils/hldata.h \
utils/hldata.cpp \
utils/idfile.cpp \
utils/idfile.h \
utils/listmem.cpp \
utils/listmem.h \
utils/log.cpp \
utils/log.h \
utils/md5.cpp \
utils/md5.h \
utils/md5ut.cpp \
utils/md5ut.h \
utils/mimeparse.cpp \
utils/mimeparse.h \
utils/miniz.cpp \
utils/miniz.h \
utils/netcon.cpp \
utils/netcon.h \
utils/pathut.cpp \
utils/pathut.h \
utils/picoxml.h \
utils/pxattr.cpp \
utils/pxattr.h \
utils/rclionice.cpp \
utils/rclionice.h \
utils/rclutil.h \
utils/rclutil.cpp \
utils/readfile.cpp \
utils/readfile.h \
utils/smallut.cpp \
utils/smallut.h \
utils/strmatcher.cpp \
utils/strmatcher.h \
utils/transcode.cpp \
utils/transcode.h \
utils/utf8iter.cpp \
utils/utf8iter.h \
utils/wipedir.cpp \
utils/wipedir.h \
utils/workqueue.h \
utils/x11mon.cpp \
utils/x11mon.h \
utils/zlibut.cpp \
utils/zlibut.h \
xaposix/safefcntl.h \
xaposix/safesysstat.h \
xaposix/safesyswait.h \
xaposix/safeunistd.h

BUILT_SOURCES = query/wasaparse.cpp
AM_YFLAGS = -d

# We use -release: the lib is only shared
# between recoll programs from the same release. So the lib is just named
# after the recoll version (e.g librecoll-1.27.1.so)
# -version-info $(VERSION_INFO) would handle ABI compat issues, we don't
# need it
librecoll_la_LDFLAGS = -release $(VERSION) -no-undefined @NO_UNDEF_LINK_FLAG@

librecoll_la_LIBADD = $(XSLT_LIBS) $(LIBXAPIAN) $(LIBICONV) $(X_LIBX11) $(LIBTHREADS)

# There is probably a better way to do this. The KIO needs to be linked
# with librecoll, but librecoll is installed into a non-standard place
# (/usr/lib/recoll). Debian packaging has something against setting an
# rpath on the kio (cause it's not the same package as the lib), so I don't
# know how to link it dynamically. The other thing I don't know is how to
# force automake to build a static lib with the PIC objects. So the
# following target, which is only used from the KIO build, deletes any .a
# and .so and rebuilds the .a with the pic objs (the kio build calls
# configured --disable-static).
# Of course this is very uncomfortably close to automake/libtool internals
# and may not work on all systems.
PicStatic: $(librecoll_la_OBJECTS)
	rm -f .libs/librecoll.a
	rm -f .libs/librecoll.so
	$(LIBTOOL) --tag=LD --mode=link gcc -g -O -o librecoll.la \
		$(librecoll_la_OBJECTS)

bin_PROGRAMS = recollindex
if MAKECMDLINE
    bin_PROGRAMS += recollq
endif

if MAKEXADUMP
   bin_PROGRAMS += xadump
endif

recollindex_SOURCES = \
    index/checkindexed.cpp \
    index/checkindexed.h \
    index/fsindexer.cpp \
    index/fsindexer.h \
    index/indexer.cpp \
    index/indexer.h \
    index/rclmonprc.cpp \
    index/rclmonrcv.cpp \
    index/recollindex.cpp \
    index/webqueue.cpp \
    index/webqueue.h

recollindex_LDADD = librecoll.la 

recollq_SOURCES = query/recollqmain.cpp
recollq_LDADD = librecoll.la

xadump_SOURCES = query/xadump.cpp
xadump_LDADD = librecoll.la $(LIBXAPIAN) $(LIBICONV)

# Note: I'd prefer the generated query parser files not to be distributed
# at all, but failed to achieve this
EXTRA_DIST = \
bincimapmime/00README.recoll bincimapmime/AUTHORS bincimapmime/COPYING \
\
desktop/hotrecoll.py \
desktop/recoll.appdata.xml \
desktop/recollindex.desktop \
desktop/recoll_index_on_ac.sh \
desktop/recoll-searchgui.desktop \
desktop/recoll.png desktop/recoll.svg desktop/recoll.xcf \
\
doc/prog/Makefile doc/prog/Doxyfile doc/prog/filters.txt doc/prog/top.txt \
\
doc/user/usermanual.html doc/user/docbook-xsl.css doc/user/docbook.css \
doc/user/Makefile doc/user/recoll.conf.xml \
doc/user/custom.xsl doc/user/usermanual.xml \
\
filters/injectcommon.sh filters/recfiltcommon filters/rcltxtlines.py \
\
index/rclmon.sh \
\
kde/kioslave/kio_recoll/00README.txt \
kde/kioslave/kio_recoll/CMakeLists.txt \
kde/kioslave/kio_recoll/data/help.html \
kde/kioslave/kio_recoll/data/searchable.html \
kde/kioslave/kio_recoll/data/welcome.html \
kde/kioslave/kio_recoll/dirif.cpp \
kde/kioslave/kio_recoll/htmlif.cpp \
kde/kioslave/kio_recoll/kio_recoll.cpp \
kde/kioslave/kio_recoll/kio_recoll.h \
kde/kioslave/kio_recoll/recollf.protocol \
kde/kioslave/kio_recoll/recollnolist.protocol \
kde/kioslave/kio_recoll/recoll.protocol \
\
kde/kioslave/kio_recoll-kde4/00README.txt \
kde/kioslave/kio_recoll-kde4/CMakeLists.txt \
kde/kioslave/kio_recoll-kde4/data/help.html \
kde/kioslave/kio_recoll-kde4/data/searchable.html \
kde/kioslave/kio_recoll-kde4/data/welcome.html \
kde/kioslave/kio_recoll-kde4/dirif.cpp \
kde/kioslave/kio_recoll-kde4/htmlif.cpp \
kde/kioslave/kio_recoll-kde4/kio_recoll.cpp \
kde/kioslave/kio_recoll-kde4/kio_recoll.h \
kde/kioslave/kio_recoll-kde4/recollf.protocol \
kde/kioslave/kio_recoll-kde4/recollnolist.protocol \
kde/kioslave/kio_recoll-kde4/recoll.protocol \
\
query/location.hh  query/position.hh  query/stack.hh \
\
qtgui/advsearch.ui \
qtgui/advsearch_w.cpp \
qtgui/advsearch_w.h \
qtgui/advshist.cpp \
qtgui/advshist.h \
qtgui/confgui/confgui.cpp \
qtgui/confgui/confgui.h \
qtgui/confgui/confguiindex.cpp \
qtgui/confgui/confguiindex.h \
qtgui/crontool.cpp \
qtgui/crontool.h \
qtgui/crontool.ui \
qtgui/firstidx.h \
qtgui/firstidx.ui \
qtgui/fragbuts.cpp \
qtgui/fragbuts.h \
qtgui/guiutils.cpp \
qtgui/guiutils.h \
qtgui/i18n/*.qm qtgui/i18n/*.ts \
qtgui/idxsched.h \
qtgui/idxsched.ui \
qtgui/images/asearch.png \
qtgui/images/cancel.png \
qtgui/images/close.png \
qtgui/images/clock.png \
qtgui/images/menu.png \
qtgui/images/code-block.png \
qtgui/images/down.png \
qtgui/images/firstpage.png \
qtgui/images/history.png \
qtgui/images/interro.png \
qtgui/images/nextpage.png \
qtgui/images/prevpage.png \
qtgui/images/recoll.icns \
qtgui/images/recoll.png \
qtgui/images/sortparms.png \
qtgui/images/spell.png \
qtgui/images/table.png \
qtgui/images/up.png \
qtgui/main.cpp \
qtgui/mtpics/License_sidux.txt \
qtgui/mtpics/README \
qtgui/mtpics/aptosid-book.png \
qtgui/mtpics/aptosid-manual-copyright.txt \
qtgui/mtpics/aptosid-manual.png \
qtgui/mtpics/archive.png \
qtgui/mtpics/book.png \
qtgui/mtpics/bookchap.png \
qtgui/mtpics/document.png \
qtgui/mtpics/drawing.png \
qtgui/mtpics/emblem-symbolic-link.png \
qtgui/mtpics/folder.png \
qtgui/mtpics/html.png \
qtgui/mtpics/image.png \
qtgui/mtpics/message.png \
qtgui/mtpics/mozilla_doc.png \
qtgui/mtpics/pdf.png \
qtgui/mtpics/pidgin.png \
qtgui/mtpics/postscript.png \
qtgui/mtpics/presentation.png \
qtgui/mtpics/sidux-book.png \
qtgui/mtpics/soffice.png \
qtgui/mtpics/source.png \
qtgui/mtpics/sownd.png \
qtgui/mtpics/spreadsheet.png \
qtgui/mtpics/text-x-python.png \
qtgui/mtpics/txt.png \
qtgui/mtpics/video.png \
qtgui/mtpics/wordprocessing.png \
qtgui/multisave.cpp \
qtgui/multisave.h \
qtgui/preview_load.cpp \
qtgui/preview_load.h \
qtgui/preview_plaintorich.cpp \
qtgui/preview_plaintorich.h \
qtgui/preview_w.cpp \
qtgui/preview_w.h \
qtgui/preview.ui \
qtgui/ptrans.ui \
qtgui/ptrans_w.cpp \
qtgui/ptrans_w.h \
qtgui/rclhelp.cpp \
qtgui/rclhelp.h \
qtgui/rclm_idx.cpp \
qtgui/rclm_menus.cpp \
qtgui/rclm_preview.cpp \
qtgui/rclm_saveload.cpp \
qtgui/rclm_view.cpp \
qtgui/rclm_wins.cpp \
qtgui/rclmain.ui \
qtgui/rclmain_w.cpp \
qtgui/rclmain_w.h \
qtgui/rclzg.cpp \
qtgui/rclzg.h \
qtgui/recoll.h \
qtgui/recoll.pro.in \
qtgui/recoll.qrc \
qtgui/reslist.cpp \
qtgui/reslist.h \
qtgui/respopup.cpp \
qtgui/respopup.h \
qtgui/restable.cpp \
qtgui/restable.h \
qtgui/restable.ui \
qtgui/rtitool.cpp \
qtgui/rtitool.h \
qtgui/rtitool.ui \
qtgui/scbase.cpp \
qtgui/scbase.h \
qtgui/searchclause_w.cpp \
qtgui/searchclause_w.h \
qtgui/snippets.ui \
qtgui/snippets_w.cpp \
qtgui/snippets_w.h \
qtgui/specialindex.h \
qtgui/specialindex.ui \
qtgui/spell.ui \
qtgui/spell_w.cpp \
qtgui/spell_w.h \
qtgui/ssearch_w.cpp \
qtgui/ssearch_w.h \
qtgui/ssearchb.ui \
qtgui/systray.cpp \
qtgui/systray.h \
qtgui/uiprefs.ui \
qtgui/uiprefs_w.cpp \
qtgui/uiprefs_w.h \
qtgui/viewaction.ui \
qtgui/viewaction_w.cpp \
qtgui/viewaction_w.h \
qtgui/webcache.ui \
qtgui/webcache.cpp \
qtgui/webcache.h \
qtgui/widgets/editdialog.h \
qtgui/widgets/editdialog.ui \
qtgui/widgets/listdialog.h \
qtgui/widgets/listdialog.ui \
qtgui/widgets/qxtconfirmationmessage.cpp \
qtgui/widgets/qxtconfirmationmessage.h \
qtgui/widgets/qxtglobal.h \
qtgui/winschedtool.cpp \
qtgui/winschedtool.h \
qtgui/winschedtool.ui \
qtgui/xmltosd.cpp \
qtgui/xmltosd.h \
\
python/README.txt \
python/pychm/AUTHORS \
python/pychm/COPYING \
python/pychm/MANIFEST.in \
python/pychm/README-RECOLL.txt \
python/pychm/recollchm \
python/pychm/recollchm/__init__.py \
python/pychm/recollchm/chm.py \
python/pychm/recollchm/chmlib.py \
python/pychm/recollchm/extra.c \
python/pychm/recollchm/swig_chm.c \
python/pychm/recollchm/swig_chm.i \
python/pychm/setup.py.in \
python/recoll/pyrclextract.cpp \
python/recoll/pyrecoll.cpp \
python/recoll/pyrecoll.h \
python/recoll/pyresultstore.cpp \
python/recoll/recoll/__init__.py \
python/recoll/recoll/conftree.py \
python/recoll/recoll/rclconfig.py \
python/recoll/recoll/rclextract.py \
python/recoll/recoll/recoll.py \
python/recoll/setup.py.in \
python/samples/docdups.py \
python/samples/mutt-recoll.py \
python/samples/rcldlkp.py \
python/samples/rclmbox.py \
python/samples/recollgui/Makefile \
python/samples/recollgui/qrecoll.py \
python/samples/recollgui/rclmain.ui \
python/samples/recollq.py \
python/samples/recollqsd.py \
\
sampleconf/fields sampleconf/fragbuts.xml sampleconf/mimeconf \
sampleconf/mimemap sampleconf/mimeview sampleconf/mimeview.mac \
sampleconf/recoll.conf sampleconf/recoll.qss \
sampleconf/recoll-dark.qss sampleconf/recoll-dark.css \
\
testmains/Makefile.am \
\
unac/AUTHORS unac/COPYING unac/README unac/README.recoll unac/unac.c \
\
VERSION

# EXTRA_DIST: The Php Code does not build anymore. No need to ship it until
# someone fixes it:
# php/00README.txt php/recoll/config.m4 php/recoll/make.sh 
# php/recoll/php_recoll.h php/recoll/recoll.cpp php/sample/shell.php 

OPTSFORPYTHON = $(shell test -f /etc/debian_version && echo --install-layout=deb)

if MAKEPYTHON
all-local:: recollpython
install-exec-local:: recollpython-install
clean-local:: recollpython-clean
recollpython: librecoll.la
	(cd python/recoll; set -x; \
        for v in 2 3;do test -n "`which python$${v}`" && \
        libdir=$(libdir) python$${v} setup.py build; \
        done \
        )
recollpython-install:
	(cd python/recoll; set -x; \
        for v in 2 3;do test -n "`which python$${v}`" && \
        python$${v} setup.py install \
         --prefix=${prefix} --root=$${DESTDIR:-/} $(OPTSFORPYTHON); \
        done; \
         )
recollpython-clean:
	rm -f python/recoll/*.pyc
	rm -rf python/pychm/build
	rm -rf python/pychm/recollchm.egg-info
	rm -rf python/pychm/setup.py
	rm -rf python/recoll/Recoll.egg-info
	rm -rf python/recoll/__pycache__
	rm -rf python/recoll/build
endif

if MAKEPYTHONCHM
all-local:: rclpychm
install-exec-local:: rclpychm-install
clean-local:: rclpychm-clean
rclpychm:
	(cd python/pychm; set -x; \
        for v in 2 3;do \
        test -n "`which python$${v}`" && python$${v} setup.py build;\
        done \
        )
rclpychm-install:
	(cd python/pychm; set -x; \
        for v in 2 3;do test -n "`which python$${v}`" && \
        python$${v} setup.py install \
         --prefix=${prefix} --root=$${DESTDIR:-/} $(OPTSFORPYTHON); \
        done \
        )
rclpychm-clean:
	rm -rf python/pychm/build
	rm -rf python/pychm/dist/*
endif

if MAKEQT
all-local:: recollqt
recollqt: librecoll.la
	(cd $(QTGUI); ${QMAKE} PREFIX=${prefix} recoll.pro)
	$(MAKE) -C $(QTGUI) LFLAGS="$(LDFLAGS)" prefix=$(prefix) \
                exec_prefix=$(exec_prefix) libdir=$(libdir)
clean-local:: recollqt-clean
recollqt-clean:
	-$(MAKE) -C $(QTGUI) clean
install-exec-local:: recollqt-install
recollqt-install:
	$(MAKE) -C $(QTGUI) LFLAGS="$(LDFLAGS)" INSTALL_ROOT=$(DESTDIR) \
                prefix=$(prefix) exec_prefix=$(exec_prefix) libdir=$(libdir) \
                install
endif

defconfdir = $(pkgdatadir)/examples
defconf_DATA = \
desktop/recollindex.desktop \
index/rclmon.sh \
sampleconf/fragbuts.xml \
sampleconf/fields \
sampleconf/recoll.conf \
sampleconf/mimeconf \
sampleconf/recoll.qss \
sampleconf/recoll-dark.qss \
sampleconf/recoll-dark.css \
sampleconf/mimemap \
sampleconf/mimeview 

filterdir = $(pkgdatadir)/filters
dist_filter_DATA = \
desktop/hotrecoll.py \
filters/abiword.xsl \
filters/cmdtalk.py \
filters/fb2.xsl \
filters/gnumeric.xsl \
filters/kosplitter.py \
filters/msodump.zip \
filters/okular-note.xsl \
filters/opendoc-body.xsl \
filters/opendoc-flat.xsl \
filters/opendoc-meta.xsl \
filters/openxml-xls-body.xsl \
filters/openxml-word-body.xsl \
filters/openxml-meta.xsl \
filters/ppt-dump.py \
filters/rcl7z \
filters/rclaptosidman \
filters/rclaudio \
filters/rclbasehandler.py \
filters/rclbibtex.sh \
filters/rclcheckneedretry.sh \
filters/rclchm \
filters/rcldia \
filters/rcldjvu.py \
filters/rcldoc.py \
filters/rcldvi \
filters/rclepub \
filters/rclepub1 \
filters/rclexec1.py \
filters/rclexecm.py \
filters/rclfb2.py \
filters/rclgaim \
filters/rclgenxslt.py \
filters/rclhwp.py \
filters/rclics \
filters/rclimg \
filters/rclimg.py \
filters/rclinfo \
filters/rclkar \
filters/rclkwd \
filters/rcllatinclass.py \
filters/rcllatinstops.zip \
filters/rcllyx \
filters/rclman \
filters/rclmidi.py \
filters/rclocrcache.py \
filters/rclocr.py \
filters/rclocrabbyy.py \
filters/rclocrtesseract.py \
filters/rclopxml.py \
filters/rclorgmode.py \
filters/rclpdf.py \
filters/rclppt.py \
filters/rclps \
filters/rclpst.py \
filters/rclpurple \
filters/rclpython.py \
filters/rclrar \
filters/rclrtf.py \
filters/rclscribus \
filters/rclshowinfo \
filters/rcltar \
filters/rcltex \
filters/rcltext.py \
filters/rcluncomp \
filters/rcluncomp.py \
filters/rclwar \
filters/rclxls.py \
filters/rclxml.py \
filters/rclxmp.py \
filters/rclxslt.py \
filters/rclzip \
filters/recoll-we-move-files.py \
filters/recollepub.zip \
filters/svg.xsl \
filters/xls-dump.py \
filters/xlsxmltocsv.py \
filters/xml.xsl \
python/recoll/recoll/conftree.py \
python/recoll/recoll/rclconfig.py 

install-data-hook: 
	(cd $(DESTDIR)/$(filterdir); \
	chmod a+x rcl* ppt-dump.py xls-dump.py xlsxmltocsv.py hotrecoll.py; \
	chmod a+x recoll-we-move-files.py ../examples/rclmon.sh kosplitter.py; \
	chmod 0644 msodump.zip recollepub.zip rclexecm.py rcllatinstops.zip \
        rclconfig.py conftree.py rclmidi.py rclexec1.py rcluncomp.py rclxslt.py)

if MAKEUSERDOC
rdocdir = $(pkgdatadir)/doc
rdoc_DATA = doc/user/usermanual.html doc/user/docbook-xsl.css 
doc/user/usermanual.html: doc/user/usermanual.xml
	mkdir -p doc/user
	test -f doc/user/Makefile  || \
		cp -p $(top_srcdir)/doc/user/Makefile doc/user
	$(MAKE) -C doc/user VPATH=$(VPATH):$(VPATH)/doc/user usermanual.html
endif

dist_man1_MANS = doc/man/recoll.1 doc/man/recollq.1 \
               doc/man/recollindex.1 doc/man/xadump.1
dist_man5_MANS = doc/man/recoll.conf.5

dist-hook:
	(cd $(top_srcdir); find . \
            \( -name '*.pyc' -o -name '#*' -o -name '*~' \) -delete)
	if test -z "$(NOTAG)";then \
	  test -z "`git status -s|grep -v recoll-$(VERSION)`"||exit 1; \
	  vers=`echo $(VERSION) | sed -e 's/~/_/g'`;\
	  git tag -a RECOLL-$$vers -m "Release $$vers tagged"; \
	fi
