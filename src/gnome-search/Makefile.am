pkglib_LTLIBRARIES = libgncmod-gnome-search.la

AM_CPPFLAGS = \
  -I${top_srcdir}/src \
  -I${top_srcdir}/src/core-utils \
  -I${top_srcdir}/src/engine \
  -I${top_srcdir}/src/app-utils \
  -I${top_srcdir}/src/gnome-utils \
  -I${top_srcdir}/src/gnc-module \
  -I${top_srcdir}/src/libqof/qof \
  ${GCONF_CFLAGS} \
  ${GTK_CFLAGS} \
  ${GLIB_CFLAGS}

libgncmod_gnome_search_la_SOURCES = \
  gncmod-gnome-search.cpp \
  gnc-general-search.cpp \
  dialog-search.cpp \
  search-account.cpp \
  search-boolean.cpp \
  search-core-type.cpp \
  search-core-utils.cpp \
  search-date.cpp \
  search-double.cpp \
  search-int64.cpp \
  search-numeric.cpp \
  search-reconciled.cpp \
  search-string.cpp

gncincludedir = ${GNC_INCLUDE_DIR}
gncinclude_HEADERS = \
  dialog-search.h \
  gnc-general-search.h

noinst_HEADERS = \
  search-account.h \
  search-boolean.h \
  search-core-type.h \
  search-core-utils.h \
  search-date.h \
  search-double.h \
  search-int64.h \
  search-numeric.h \
  search-reconciled.h \
  search-string.h

libgncmod_gnome_search_la_LDFLAGS = -avoid-version

libgncmod_gnome_search_la_LIBADD = \
  ${top_builddir}/src/gnome-utils/libgncmod-gnome-utils.la \
  ${top_builddir}/src/app-utils/libgncmod-app-utils.la \
  ${top_builddir}/src/gnc-module/libgnc-module.la \
  ${top_builddir}/src/core-utils/libgnc-core-utils.la \
  ${top_builddir}/src/libqof/qof/libgnc-qof.la \
  ${GCONF_LIBS} \
  ${GLIB_LIBS} \
  ${REGEX_LIBS}

gtkbuilderdir = $(GNC_GTKBUILDER_DIR)
gtkbuilder_DATA = \
  dialog-search.glade

EXTRA_DIST = \
  ${gtkbuilder_DATA}

AM_CPPFLAGS += -DG_LOG_DOMAIN=\"gnc.gui.search\"
