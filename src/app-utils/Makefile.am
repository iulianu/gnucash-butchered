# This program is free software; you can redistribute it and/or    
# modify it under the terms of the GNU General Public License as   
# published by the Free Software Foundation; either version 2 of   
# the License, or (at your option) any later version.              
#                                                                  
# This program is distributed in the hope that it will be useful,  
# but WITHOUT ANY WARRANTY; without even the implied warranty of   
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    
# GNU General Public License for more details.                     
#                                                                  
# You should have received a copy of the GNU General Public License
# along with this program; if not, contact:
#
# Free Software Foundation           Voice:  +1-617-542-5942
# 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652
# Boston, MA  02110-1301,  USA       gnu@gnu.org

SUBDIRS = . test

pkglib_LTLIBRARIES = libgncmod-app-utils.la

AM_CPPFLAGS = \
  -Wno-error=deprecated-declarations \
  -I$(top_srcdir)/lib/libc \
  -I${top_srcdir}/src \
  -I${top_builddir}/src \
  -I${top_srcdir}/src/gnc-module \
  -I${top_srcdir}/src/app-utils/calculation \
  -I${top_srcdir}/src/core-utils \
  -I${top_srcdir}/src/engine \
  -I${top_srcdir}/src/libqof/qof \
  ${GLIB_CFLAGS} \
  ${GCONF_CFLAGS} \
  ${GTK_CFLAGS}

libgncmod_app_utils_la_SOURCES = \
  calculation/expression_parser.cpp \
  calculation/fin.cpp \
  business-options.cpp \
  QuickFill.cpp \
  file-utils.cpp \
  gfec.cpp \
  gnc-account-merge.cpp \
  gnc-accounting-period.cpp \
  gnc-addr-quickfill.cpp \
  gnc-component-manager.cpp \
  gnc-entry-quickfill.cpp \
  gnc-euro.cpp \
  gnc-exp-parser.cpp \
  gnc-gettext-util.cpp \
  gnc-helpers.cpp \
  gnc-sx-instance-model.cpp \
  gncmod-app-utils.cpp \
  gnc-ui-balances.cpp \
  gnc-ui-util.cpp \
  option-util.cpp

gncincludedir = ${GNC_INCLUDE_DIR}
gncinclude_HEADERS = \
  QuickFill.h \
  business-options.h \
  file-utils.h \
  gfec.h \
  gnc-basic-gobject.h \
  gnc-account-merge.h \
  gnc-accounting-period.h \
  gnc-addr-quickfill.h \
  gnc-component-manager.h \
  gnc-entry-quickfill.h \
  gnc-euro.h \
  gnc-exp-parser.h \
  gnc-gettext-util.h \
  gnc-help-utils.h \
  gnc-helpers.h \
  gnc-sx-instance-model.h \
  gnc-ui-balances.h \
  gnc-ui-util.h \
  option-util.h

libgncmod_app_utils_la_LDFLAGS = -avoid-version

libgncmod_app_utils_la_LIBADD = \
  ${top_builddir}/src/gnc-module/libgnc-module.la \
  ${top_builddir}/src/engine/libgncmod-engine.la \
  ${top_builddir}/src/core-utils/libgnc-core-utils.la \
  ${top_builddir}/src/libqof/qof/libgnc-qof.la \
  ${GCONF_LIBS} \
  ${GTK_LIBS} \
  ${GLIB_LIBS}

gncmoddir = 
gncmod_DATA =  

noinst_DATA = .scm-links

noinst_HEADERS = \
  calculation/finvar.h \
  calculation/finproto.h \
  calculation/fin_spl_protos.h \
  calculation/fin_static_proto.h

EXTRA_DIST = \
  ${gncmod_DATA}

EXTRA_DIST += gnc-help-utils.c

if GNUCASH_SEPARATE_BUILDDIR
#For executing test cases
SCM_FILE_LINKS = \
  ${gncmod_DATA} 
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

clean-local:
	rm -rf gnucash

CLEANFILES = ${SCM_FILE_LINKS}  .scm-links
MAINTAINERCLEANFILES = 

AM_CPPFLAGS += -DG_LOG_DOMAIN=\"gnc.app-utils\"
