SUBDIRS = gtkbuilder schemas . test ui

pkglib_LTLIBRARIES = libgncmod-gnome-utils.la

# Note that src/gnome-utils CANNOT depend on src/gnome!

AM_CPPFLAGS = \
  -I${top_srcdir}/src/core-utils \
  -I${top_srcdir}/src/gnc-module \
  -I${top_srcdir}/src/engine \
  -I${top_srcdir}/src/backend/xml \
  -I${top_srcdir}/src/app-utils \
  -I${top_srcdir}/src \
  -I${top_builddir}/src \
  -I${top_srcdir}/lib/libc \
  -I${top_srcdir}/src/libqof/qof \
  ${GLIB_CFLAGS} \
  ${GTK_CFLAGS} \
  ${GCONF_CFLAGS} \
  ${GNOME_KEYRING_CFLAGS} \
  ${QOF_CFLAGS} \
  ${LIBGDA_CFLAGS} \
  ${LIBXML2_CFLAGS} 

libgncmod_gnome_utils_la_SOURCES = \
  account-quickfill.cpp \
  assistant-gconf-setup.cpp \
  assistant-utils.cpp \
  assistant-xml-encoding.cpp \
  cursors.cpp \
  dialog-account.cpp \
  dialog-book-close.cpp \
  dialog-commodity.cpp \
  dialog-dup-trans.cpp \
  dialog-file-access.cpp \
  dialog-object-references.cpp \
  dialog-options.cpp \
  dialog-preferences.cpp \
  dialog-query-view.cpp \
  dialog-reset-warnings.cpp \
  dialog-tax-table.cpp \
  dialog-totd.cpp \
  dialog-transfer.cpp \
  dialog-userpass.cpp \
  dialog-utils.cpp \
  gnc-account-sel.cpp \
  gnc-amount-edit.cpp \
  gnc-autosave.cpp \
  gnc-cell-renderer-date.cpp \
  gnc-cell-renderer-popup.cpp \
  gnc-cell-renderer-popup-entry.cpp \
  gnc-combott.cpp \
  gnc-commodity-edit.cpp \
  gnc-currency-edit.cpp \
  gnc-date-delta.cpp \
  gnc-date-edit.cpp \
  gnc-date-format.cpp \
  gnc-dense-cal.cpp \
  gnc-dense-cal-model.cpp \
  gnc-dense-cal-store.cpp \
  gnc-embedded-window.cpp \
  gnc-file.cpp \
  gnc-frequency.cpp \
  gnc-recurrence.cpp \
  gnc-general-select.cpp \
  gnc-gnome-utils.cpp \
  gnc-gobject-utils.cpp \
  gnc-gtk-utils.cpp \
  gnc-gui-query.cpp \
  gnc-icons.cpp \
  gnc-keyring.cpp \
  gnc-main-window.cpp \
  gnc-menu-extensions.cpp \
  gnc-plugin-file-history.cpp \
  gnc-plugin-manager.cpp \
  gnc-plugin-menu-additions.cpp \
  gnc-plugin-page.cpp \
  gnc-plugin.cpp \
  gnc-period-select.cpp \
  gnc-query-view.cpp \
  gnc-splash.cpp \
  gnc-sx-instance-dense-cal-adapter.cpp \
  gnc-sx-list-tree-model-adapter.cpp \
  gnc-tree-control-split-reg.cpp \
  gnc-tree-model.cpp \
  gnc-tree-model-account-types.cpp \
  gnc-tree-model-account.cpp \
  gnc-tree-model-budget.cpp \
  gnc-tree-model-owner.cpp \
  gnc-tree-model-commodity.cpp \
  gnc-tree-model-price.cpp \
  gnc-tree-model-split-reg.cpp \
  gnc-tree-view-account.cpp \
  gnc-tree-view-commodity.cpp \
  gnc-tree-view-owner.cpp \
  gnc-tree-view-price.cpp \
  gnc-tree-view-split-reg.cpp \
  gnc-tree-view-sx-list.cpp \
  gnc-tree-view.cpp \
  gnc-window.cpp \
  gncmod-gnome-utils.cpp \
  misc-gnome-utils.cpp \
  search-param.cpp \
  window-main-summarybar.cpp

gncincludedir = ${GNC_INCLUDE_DIR}
gncinclude_HEADERS = \
  account-quickfill.h \
  assistant-gconf-setup.h \
  assistant-utils.h \
  assistant-xml-encoding.h \
  dialog-account.h \
  dialog-book-close.h \
  dialog-commodity.h \
  dialog-dup-trans.h \
  dialog-file-access.h \
  dialog-preferences.h \
  dialog-object-references.h \
  dialog-options.h \
  dialog-query-view.h \
  dialog-reset-warnings.h \
  dialog-totd.h \
  dialog-transfer.h \
  dialog-utils.h \
  gnc-account-sel.h \
  gnc-amount-edit.h \
  gnc-cell-renderer-date.h \
  gnc-cell-renderer-popup.h \
  gnc-cell-renderer-popup-entry.h \
  gnc-combott.h \
  gnc-commodity-edit.h \
  gnc-currency-edit.h \
  gnc-date-delta.h \
  gnc-date-edit.h \
  gnc-date-format.h \
  gnc-dense-cal.h \
  gnc-dense-cal-model.h \
  gnc-dense-cal-store.h \
  gnc-embedded-window.h \
  gnc-file.h \
  gnc-frequency.h \
  gnc-recurrence.h \
  gnc-general-select.h \
  gnc-gnome-utils.h \
  gnc-gui-query.h \
  gnc-icons.h \
  gnc-keyring.h \
  gnc-main-window.h \
  gnc-menu-extensions.h \
  gnc-plugin-file-history.h \
  gnc-plugin-manager.h \
  gnc-plugin-menu-additions.h \
  gnc-plugin-page.h \
  gnc-plugin.h \
  gnc-period-select.h \
  gnc-query-view.h \
  gnc-splash.h \
  gnc-sx-instance-dense-cal-adapter.h \
  gnc-sx-list-tree-model-adapter.h \
  gnc-tree-control-split-reg.h \
  gnc-tree-model.h \
  gnc-tree-model-account-types.h \
  gnc-tree-model-account.h \
  gnc-tree-model-budget.h \
  gnc-tree-model-owner.h \
  gnc-tree-model-commodity.h \
  gnc-tree-model-price.h \
  gnc-tree-model-split-reg.h \
  gnc-tree-view-account.h \
  gnc-tree-view-commodity.h \
  gnc-tree-view-owner.h \
  gnc-tree-view-price.h \
  gnc-tree-view-split-reg.h \
  gnc-tree-view-sx-list.h \
  gnc-tree-view.h \
  gnc-ui.h \
  gnc-window.h \
  misc-gnome-utils.h \
  window-main-summarybar.h

noinst_HEADERS = \
  dialog-tax-table.h \
  gnc-autosave.h \
  gnc-gobject-utils.h \
  gnc-gtk-utils.h \
  search-param.h

libgncmod_gnome_utils_la_LDFLAGS = -avoid-version

libgncmod_gnome_utils_la_LIBADD = \
  ${top_builddir}/src/core-utils/libgnc-core-utils.la \
  ${top_builddir}/src/gnc-module/libgnc-module.la \
  ${top_builddir}/src/engine/libgncmod-engine.la \
  ${top_builddir}/src/backend/xml/libgnc-backend-xml-utils.la \
  ${top_builddir}/src/app-utils/libgncmod-app-utils.la \
  ${top_builddir}/src/libqof/qof/libgnc-qof.la \
  ${GTK_LIBS} \
  ${GCONF_LIBS} \
  ${GNOME_KEYRING_LIBS} \
  ${GLIB_LIBS} \
  ${DB_LIBS} \
  ${REGEX_LIBS} \
  ${LIBXML2_LIBS} 

libgncmod_gnome_utils_la_LIBADD += -lX11

gncmoddir = 
gncmod_DATA =  

gncscmdir =  
gncscm_DATA =  

EXTRA_DIST = \
  gnome-utils.i \
  ${gncmod_DATA} \
  ${gncscm_DATA}


if GNUCASH_SEPARATE_BUILDDIR
#For executing test cases
SCM_FILE_LINKS = \
  ${gncmod_DATA} \
  ${gncscm_DATA}
endif

.scm-links: 
	$(RM) -rf gnucash
	mkdir -p  gnucash
if GNUCASH_SEPARATE_BUILDDIR
	for X in ${SCM_FILE_LINKS} ; do \
	  $(LN_S) -f ${srcdir}/$$X . ; \
	done
endif
	( cd gnucash; for A in $(gncmod_DATA) ; do $(LN_S) -f ../$$A . ; done )
	touch .scm-links

noinst_DATA = .scm-links
clean-local:
	rm -rf gnucash

CLEANFILES = $(BUILT_SOURCES) .scm-links ${SCM_FILE_LINKS}
MAINTAINERCLEANFILES = 

# I hate inconsistent standards. Autotools puts help files into
# ${datadir}/gnome/help/${program} while the gnome2 libraries expect
# them in ${pkgdatadir}/gnome/help/${program}.
#
install-data-hook:
	$(LN_S) -f ../gnome ${DESTDIR}${pkgdatadir}

uninstall-hook:
	rm -f ${DESTDIR}${pkgdatadir}/gnome

AM_CPPFLAGS += -DG_LOG_DOMAIN=\"gnc.gui\"
