noinst_LTLIBRARIES = libtest-core.la

libtest_core_la_SOURCES = \
	test-stuff.cpp \
	unittest-support.cpp

libtest_core_la_LIBADD = \
  ${top_builddir}/src/libqof/qof/libgnc-qof.la \
  ${GLIB_LIBS}

noinst_HEADERS = \
	test-stuff.h \
	unittest-support.h

libtest_core_la_CPPFLAGS = \
  -I${top_srcdir}/src \
  -I${top_srcdir}/src/libqof/qof \
  ${GLIB_CFLAGS}

libtest_core_la_LDFLAGS = \
  -rpath ${exec-prefix}/lib

SCM_FILES = 

gncmoddir = 
gncmod_DATA = $(SCM_FILES)

.scm-links:
	$(RM) -rf gnucash
	mkdir -p  gnucash
if GNUCASH_SEPARATE_BUILDDIR
	for X in ${SCM_FILES} ; do \
	  $(LN_S) -f ${srcdir}/$$X . ; \
	done
endif
	( cd gnucash; for A in $(SCM_FILES) ; do $(LN_S) -f ../$$A . ; done )
	touch .scm-links

clean-local:
	$(RM) -rf gnucash

noinst_DATA = .scm-links
CLEANFILES = .scm-links

EXTRA_DIST = \
  $(SCM_FILES)

MAINTAINERCLEANFILES = 
