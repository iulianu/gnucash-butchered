abs_top_srcdir=@abs_top_srcdir@

EXTRA_DIST = \
  xgettext.scm

# Cause automake to generate correct rules
# for maintainers
noinst_HEADERS = 

BUILT_SOURCES = 

SCMFILES = $(shell find ${abs_top_srcdir}/src -name test -prune -o -name '*.scm' -print | sort)
