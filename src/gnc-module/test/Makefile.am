SUBDIRS = misc-mods

LDADD = \
  ${top_builddir}/src/test-core/libtest-core.la \
  ${top_builddir}/src/libqof/qof/libgnc-qof.la \
  ../libgnc-module.la ${GLIB_LIBS} 

AM_CPPFLAGS = \
  -I${top_srcdir}/src \
  -I${top_srcdir}/src/gnc-module \
  -I${top_srcdir}/src/libqof/qof \
  -I${top_srcdir}/src/test-core \
  ${GLIB_CFLAGS}

TESTS = \
  test-modsysver \
  test-incompatdep \
  test-dynload 

test_modsysver_SOURCES = test-modsysver.cpp
test_incompatdep_SOURCES = test-incompatdep.cpp
test_dynload_SOURCES = test-dynload.cpp

GNC_TEST_DEPS = \
  --gnc-module-dir ${top_builddir}/src/gnc-module \
  --gnc-module-dir ${top_builddir}/src/gnc-module/test/misc-mods \
  --library-dir    ${top_builddir}/src/libqof/qof \
  --library-dir    ${top_builddir}/src/core-utils \
  --library-dir    ${top_builddir}/src/test-core \
  --library-dir    ${top_builddir}/src/gnc-module

TESTS_ENVIRONMENT = \
  $(shell ${top_srcdir}/src/gnc-test-env --no-exports ${GNC_TEST_DEPS})

check_PROGRAMS = \
  test-modsysver \
  test-incompatdep \
  test-dynload

test_dynload_LDFLAGS = 

EXTRA_DIST = 
