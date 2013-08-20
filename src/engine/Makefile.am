SUBDIRS = . test-core test 

pkglib_LTLIBRARIES = libgncmod-engine.la

AM_CPPFLAGS = \
	-I${top_srcdir}/lib/libc \
	-I${top_srcdir}/src/core-utils \
	-I${top_srcdir}/src \
	-I${top_builddir}/src \
	-I${top_srcdir}/src/gnc-module \
	-I${top_srcdir}/src/libqof/qof \
	${GLIB_CFLAGS}  

libgncmod_engine_la_SOURCES = \
  Account.cpp \
  Recurrence.cpp \
  Query.cpp \
  SchedXaction.cpp \
  SX-book.cpp \
  SX-ttinfo.cpp \
  Scrub.cpp \
  Scrub2.cpp \
  Scrub3.cpp \
  Split.cpp \
  TransLog.cpp \
  Transaction.cpp \
  cap-gains.cpp \
  cashobjects.cpp \
  gnc-budget.cpp \
  gnc-commodity.cpp \
  gnc-engine.cpp \
  gnc-event.cpp \
  gnc-hooks.cpp \
  gnc-lot.cpp \
  gnc-pricedb.cpp \
  gnc-session.cpp \
  gncmod-engine.cpp \
  engine-helpers.cpp \
  policy.cpp \
  gncBusiness.cpp \
  gncAddress.cpp \
  gncBillTerm.cpp \
  gncCustomer.cpp \
  gncEmployee.cpp \
  gncEntry.cpp \
  gncInvoice.cpp \
  gncJob.cpp \
  gncOrder.cpp \
  gncOwner.cpp \
  gncTaxTable.cpp \
  gncIDSearch.cpp \
  gncVendor.cpp

EXTRA_libgncmod_engine_la_SOURCES = iso-4217-currencies.cpp

gncincludedir = ${GNC_INCLUDE_DIR}
gncinclude_HEADERS = \
  Account.h \
  FreqSpec.h \
  Recurrence.h \
  GNCId.h \
  SchedXaction.h \
  SX-book.h \
  SX-ttinfo.h \
  Query.h \
  Scrub.h \
  Scrub2.h \
  Scrub3.h \
  Split.h \
  TransLog.h \
  Transaction.h \
  cap-gains.h \
  cashobjects.h \
  engine-helpers.h \
  gnc-budget.h \
  gnc-commodity.h \
  gnc-engine.h \
  gnc-event.h \
  gnc-hooks.h \
  gnc-pricedb.h \
  gnc-session.h \
  policy.h \
  gncAddress.h \
  gncAddressP.h \
  gncBillTerm.h \
  gncBillTermP.h \
  gncBusiness.h \
  gncCustomer.h \
  gncCustomerP.h \
  gncEmployee.h \
  gncEmployeeP.h \
  gncEntry.h \
  gncEntryP.h \
  gncInvoice.h \
  gncInvoiceP.h \
  gncJob.h \
  gncJobP.h \
  gncOrder.h \
  gncOrderP.h \
  gncOwner.h \
  gncOwnerP.h \
  gncTaxTable.h \
  gncTaxTableP.h \
  gncIDSearch.h \
  gncVendor.h \
  gncVendorP.h

noinst_HEADERS = \
  AccountP.h \
  ScrubP.h \
  SplitP.h \
  SX-book.h \
  SX-ttinfo.h \
  TransactionP.h \
  gnc-lot.h \
  gnc-lot-p.h \
  gnc-pricedb-p.h \
  policy-p.h

noinst_SCRIPTS = iso-currencies-to-c

libgncmod_engine_la_LDFLAGS = -avoid-version

libgncmod_engine_la_LIBADD = \
  ../gnc-module/libgnc-module.la \
  ../core-utils/libgnc-core-utils.la \
  ${top_builddir}/src/libqof/qof/libgnc-qof.la \
  ${REGEX_LIBS} \
  ${GLIB_LIBS}

gncmoddir =
gncmod_DATA =

gncscmdir =  
gncscm_DATA =  

EXTRA_DIST = \
  README.query-api \
  design.txt \
  extensions.txt \
  iso-4217-currencies.scm \
  iso-currencies-to-c \
  kvp_doc.txt \
  SX-book-p.h \
  ${gncmod_DATA} \
  ${gncscm_DATA}

if GNUCASH_SEPARATE_BUILDDIR
#For compiling
SCM_FILE_LINKS = iso-4217-currencies.scm
#For executing test cases
SCM_FILE_LINKS += \
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


# The generated file depends on various libqof headers. Out of
# laziness I only include a few here - more might be needed
# subsequently.
QOFHEADERS = \
  $(top_srcdir)/src/libqof/qof/qofbook.h \
  $(top_srcdir)/src/libqof/qof/qofbookslots.h

iso-4217-currencies.cpp: iso-4217-currencies.scm iso-currencies-to-c
	-chmod u+x ${srcdir}/iso-currencies-to-c
	${srcdir}/iso-currencies-to-c "${srcdir}"

BUILT_SOURCES = iso-4217-currencies.cpp

CLEANFILES = $(BUILT_SOURCES) .scm-links \
	${SCM_FILE_LINKS}
MAINTAINERCLEANFILES = 

clean-local:
	rm -rf gnucash

AM_CPPFLAGS += -DG_LOG_DOMAIN=\"gnc.engine\"
