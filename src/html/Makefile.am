#SUBDIRS = . test
SUBDIRS = .

pkglib_LTLIBRARIES = libgncmod-html.la

AM_CPPFLAGS = \
  -I${top_srcdir}/src/core-utils \
  -I${top_srcdir}/src/gnc-module \
  -I${top_srcdir}/src/engine \
  -I${top_srcdir}/src/gnome-utils \
  -I${top_srcdir}/src/app-utils \
  -I${top_srcdir}/src \
  -I${top_builddir}/src \
  -I${top_srcdir}/lib/libc \
  ${QOF_CFLAGS} \
  ${GLIB_CFLAGS} \
  ${GTK_CFLAGS} \
  ${GNOME_CFLAGS} \
  ${WEBKIT_CFLAGS}

libgncmod_html_la_SOURCES = \
  gncmod-html.cpp \
  gnc-html.cpp \
  gnc-html-history.cpp \
  gnc-html-factory.cpp \
  gnc-html-webkit.cpp 

gncincludedir = ${GNC_INCLUDE_DIR}
gncinclude_HEADERS = \
  gnc-html-history.h \
  gnc-html.h \
  gnc-html-p.h \
  gnc-html-factory.h \
  gnc-html-webkit.h \
  gnc-html-extras.h \
  gnc-html-webkit-p.h

libgncmod_html_la_LDFLAGS = -avoid-version

libgncmod_html_la_LIBADD = \
  ${top_builddir}/src/core-utils/libgnc-core-utils.la \
  ${top_builddir}/src/gnc-module/libgnc-module.la \
  ${top_builddir}/src/engine/libgncmod-engine.la \
  ${top_builddir}/src/gnome-utils/libgncmod-gnome-utils.la \
  ${top_builddir}/src/app-utils/libgncmod-app-utils.la \
  ${WEBKIT_LIBS} \
  ${GTK_LIBS} \
  ${GLIB_LIBS} \
  ${DB_LIBS} \
  ${QOF_LIBS} \
  ${REGEX_LIBS}


EXTRA_DIST =


CLEANFILES = $(BUILT_SOURCES) gnucash
MAINTAINERCLEANFILES = 

# I hate inconsistent standards. Autotools puts help files into
# ${datadir}/gnome/help/${program} while the gnome2 libraries expect
# them in ${pkgdatadir}/gnome/help/${program}.
#
install-data-hook:
	$(LN_S) -f ../gnome ${DESTDIR}${pkgdatadir}

uninstall-hook:
	rm -f ${DESTDIR}${pkgdatadir}/gnome

AM_CPPFLAGS += -DG_LOG_DOMAIN=\"gnc.html\"
