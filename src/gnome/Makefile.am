SUBDIRS = gtkbuilder schemas ui

lib_LTLIBRARIES = libgnc-gnome.la

libgnc_gnome_la_LIBADD = \
  ${top_builddir}/src/register/ledger-core/libgncmod-ledger-core.la \
  ${top_builddir}/src/register/register-gnome/libgncmod-register-gnome.la \
  ${top_builddir}/src/register/register-core/libgncmod-register-core.la \
  ${top_builddir}/src/gnome-search/libgncmod-gnome-search.la \
  ${top_builddir}/src/gnome-utils/libgncmod-gnome-utils.la \
  ${top_builddir}/src/backend/xml/libgnc-backend-xml-utils.la \
  ${top_builddir}/src/app-utils/libgncmod-app-utils.la \
  ${top_builddir}/src/html/libgncmod-html.la \
  ${top_builddir}/src/engine/libgncmod-engine.la \
  ${top_builddir}/src/core-utils/libgnc-core-utils.la \
  ${top_builddir}/src/libqof/qof/libgnc-qof.la \
  ${LIBXML2_LIBS} \
  ${GTK_LIBS} \
  ${GCONF_LIBS} \
  ${GLIB_LIBS} \
  ${QOF_LIBS}

libgnc_gnome_la_SOURCES = \
  assistant-acct-period.cpp \
  assistant-hierarchy.cpp \
  assistant-loan.cpp \
  assistant-stock-split.cpp \
  dialog-commodities.cpp \
  dialog-fincalc.cpp \
  dialog-find-transactions.cpp \
  dialog-lot-viewer.cpp \
  dialog-new-user.cpp \
  dialog-price-editor.cpp \
  dialog-price-edit-db.cpp \
  dialog-progress.cpp \
  dialog-sx-editor.cpp \
  dialog-sx-from-trans.cpp \
  dialog-sx-since-last-run.cpp \
  dialog-tax-info.cpp \
  gnc-budget-view.cpp \
  gnc-plugin-account-tree.cpp \
  gnc-plugin-basic-commands.cpp \
  gnc-plugin-budget.cpp \
  gnc-plugin-register.cpp \
  gnc-plugin-register2.cpp \
  gnc-plugin-page-account-tree.cpp \
  gnc-plugin-page-budget.cpp \
  gnc-plugin-page-sx-list.cpp \
  gnc-plugin-page-register.cpp \
  gnc-plugin-page-register2.cpp \
  gnc-split-reg.cpp \
  gnc-split-reg2.cpp \
  reconcile-view.cpp \
  top-level.cpp \
  window-reconcile.cpp \
  window-autoclear.cpp

gnomeappdir = ${datadir}/applications

gnomeapp_in_files = gnucash.desktop.in
gnomeapp_DATA = $(gnomeapp_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

appicondir = $(datadir)/pixmaps

noinst_HEADERS = \
  assistant-acct-period.h \
  assistant-hierarchy.h \
  assistant-loan.h \
  assistant-stock-split.h \
  dialog-fincalc.h \
  dialog-find-transactions.h \
  dialog-lot-viewer.h \
  dialog-new-user.h \
  dialog-progress.h \
  dialog-sx-editor.h \
  dialog-sx-from-trans.h \
  dialog-sx-since-last-run.h \
  gnc-plugin-account-tree.h \
  gnc-plugin-basic-commands.h \
  gnc-plugin-budget.h \
  gnc-plugin-register.h \
  gnc-plugin-register2.h \
  gnc-plugin-page-account-tree.h \
  gnc-plugin-page-budget.h \
  gnc-plugin-page-sx-list.h \
  gnc-plugin-page-register.h \
  gnc-plugin-page-register2.h \
  gnc-split-reg.h \
  gnc-split-reg2.h \
  reconcile-view.h \
  top-level.h \
  window-reconcile.h \
  window-autoclear.h

EXTRA_DIST = \
  gnucash.desktop.in

AM_CPPFLAGS = \
  -I${top_srcdir}/src \
  -I${top_builddir}/src \
  -I${top_srcdir}/src/core-utils \
  -I${top_srcdir}/src/engine \
  -I${top_srcdir}/src/app-utils \
  -I${top_srcdir}/src/app-utils/calculation \
  -I${top_srcdir}/src/backend/xml \
  -I${top_srcdir}/src/gnome-utils \
  -I${top_srcdir}/src/gnome-search \
  -I${top_srcdir}/src/html \
  -I${top_srcdir}/src/register/ledger-core \
  -I${top_srcdir}/src/register/register-core \
  -I${top_srcdir}/src/register/register-gnome \
  -I${top_srcdir}/lib/libc \
  -I${top_srcdir}/src/libqof/qof \
  ${LIBXML2_CFLAGS} \
  ${GTK_CFLAGS} \
  ${GCONF_CFLAGS} \
  $(GLIB_CFLAGS) 

# We build this in an earlier directory.

# The line below breaks automake 1.5.  
##OMIT_DEPENDENCIES = gnc-dir.h

CLEANFILES = gnucash.desktop
MAINTAINERCLEANFILES = 

AM_CPPFLAGS += -DG_LOG_DOMAIN=\"gnc.gui\"
