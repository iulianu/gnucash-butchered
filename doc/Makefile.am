SUBDIRS = examples

doc_DATA = \
  README.francais \
  README.german \
  guile-hackers.txt \
  projects.html 

man_MANS = \
  gnucash.1 \
  gnc-fq-dump.1 \
  gnc-fq-helper.1 

tipsdir = $(GNC_SHAREDIR)
tips_DATA = tip_of_the_day.list

EXTRA_DIST = \
  ${doc_DATA} \
  README.build-system \
  README.HBCI \
  README.OFX \
  README.translator.txt \
  TRANSLATION_HOWTO \
  build-aix.txt \
  build-solaris.txt \
  gnucash.1.in \
  gtkrc-2.0.gnucash \
  misc-notes.txt \
  tip_of_the_day.list.in

## We borrow guile's convention and use @-...-@ as the substitution
## brackets here, instead of the usual @...@.  This prevents autoconf
## from substituting the values directly into the left-hand sides of
## the sed substitutions.
gnucash.1: gnucash.1.in Makefile
	rm -f $@.tmp
	${SED} < $< > $@.tmp \
            -e 's:@-VERSION-@:${VERSION}:g' \
            -e 's:@-DATE-@:$(shell date +'%B %Y'):g'
	chmod +x $@.tmp
	mv $@.tmp $@

tip_of_the_day.list: tip_of_the_day.list.in Makefile
	${CC} -E -P -x c -D'N_(x)=x' -o $@.tmp $<
	cat -s $@.tmp | ${SED} -e 's/^ *"//' \
	                       -e 's/"* *[|] */|/' \
                           -e 's:@-GNUCASH_LATEST_STABLE_SERIES-@:${GNUCASH_LATEST_STABLE_SERIES}:g' > $@
	rm -f $@.tmp
	perl -pi.bak -e 's/" *$$/\n/' $@

DISTCLEANFILES = gnucash.1 tip_of_the_day.list
