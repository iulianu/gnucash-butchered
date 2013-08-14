## -*-m4-*-

dnl Process this file with autoconf to produce a configure script.

# FILE:
# configure.in
#
# FUNCTION:
# implements checks for a variety of system-specific functions
#
# Contents:
#   Headers   - Autoconf header stuff
#   Variables - Hardcoded variables
#   Programs  - Check for programs binaries
#   Functions - Check for functions
#   With      - Check for --with/without options
#   Enable    - Check for --enable/disable options
#   Libraries - Check for libraries
#   Footer    - Autoconf footer stuff

# Autoconf initialization
AC_PREREQ(2.60)
AC_INIT([GnuCash], [2.4.99], [gnucash-devel@gnucash.org])
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_SRCDIR(src/engine/Transaction.h)

#Change this in development versions when changing anything that
#affects stored data structures. Reset to zero when bumping version.
GNUCASH_NANO_VERSION=0
#This should be the earliest version in the form XXMMUUNN (XX=MAJOR,
#MM=MINOR, UU=MICRO, NN=NANO) or SVN revision if before 2.4.1 which is
#storage-compatible with the current version. See the comments in
#src/backend/dbi/gnc-backend-dbi.c:gnc_dbi_load.
GNUCASH_RESAVE_VERSION=19920
# Initialize automake -- make sure we have at least version 1.9
AM_INIT_AUTOMAKE([1.9 dist-bzip2])

# Parse out the version number
GNUCASH_VERSION_STRING=AC_PACKAGE_VERSION
GNUCASH_MAJOR_VERSION=`echo $GNUCASH_VERSION_STRING | [ sed -e 's/^\([^\.]*\)\.\([^.]*\)\.\(.*\)$/\1/' ]`
GNUCASH_MINOR_VERSION=`echo $GNUCASH_VERSION_STRING | [ sed -e 's/^\([^\.]*\)\.\([^.]*\)\.\(.*\)$/\2/' ]`
GNUCASH_MICRO_VERSION=`echo $GNUCASH_VERSION_STRING | [ sed -e 's/^\([^\.]*\)\.\([^.]*\)\.\(.*\)$/\3/' ]`
GNUCASH_LATEST_STABLE_SERIES=$GNUCASH_MAJOR_VERSION.$(($GNUCASH_MINOR_VERSION-($GNUCASH_MINOR_VERSION%2)))

# save any user/environment optimization flags before we muck with them in
# --enable-debug:
USER_OPTIMIZATION=""
for flag in $CFLAGS; do
  tmp_flag=`echo $flag | sed -e 's,-O.,,'`
  if test -z "$tmp_flag"; then
    USER_OPTIMIZATION="$USER_OPTIMIZATION ${flag}"
  fi
done

##  Do this first, because the other tests depend on it:

# Check for gcc and intltool (somehow breaks the suggested content
# order, doesn't it? Whatever.)
AC_PROG_CC
AM_PROG_CC_C_O
AC_GNU_SOURCE
AC_PROG_INTLTOOL

AM_GCONF_SOURCE_2

# Override gconf default install locations.  The default is to always
# put them into /etc.  This change puts them into $(prefix)/etc.
if test "x$GCONF_SCHEMA_INSTALL_SOURCE" = "x"; then
  GCONF_SCHEMA_CONFIG_SOURCE="xml::${sysconfdir}/gconf/gconf.xml.defaults"
fi
GCONF_SCHEMA_CONFIG_SOURCE_DIRONLY=`echo $GCONF_SCHEMA_CONFIG_SOURCE | cut -d ':' -f 3`
AC_SUBST(GCONF_SCHEMA_CONFIG_SOURCE_DIRONLY)


AC_SUBST(GNUCASH_MAJOR_VERSION)
AC_SUBST(GNUCASH_MINOR_VERSION)
AC_SUBST(GNUCASH_MICRO_VERSION)
AC_SUBST(GNUCASH_NANO_VERSION)
AC_SUBST(GNUCASH_LATEST_STABLE_SERIES)

AC_DEFINE_UNQUOTED(GNUCASH_MAJOR_VERSION, $GNUCASH_MAJOR_VERSION,
    [GnuCash Major version number])
AC_DEFINE_UNQUOTED(GNUCASH_MINOR_VERSION, $GNUCASH_MINOR_VERSION,
    [GnuCash Minor version number])
AC_DEFINE_UNQUOTED(GNUCASH_MICRO_VERSION, $GNUCASH_MICRO_VERSION,
    [GnuCash Micro version number])
AC_DEFINE_UNQUOTED(GNUCASH_NANO_VERSION, $GNUCASH_NANO_VERSION,
    [GnuCash Nano version number])
AC_DEFINE_UNQUOTED(GNUCASH_RESAVE_VERSION, $GNUCASH_RESAVE_VERSION,
    [GnuCash earliest compatible databaseversion number])
AC_DEFINE_UNQUOTED(GNUCASH_LATEST_STABLE_SERIES, "$GNUCASH_LATEST_STABLE_SERIES",
    [Most recent stable GnuCash series])

dnl Set of available languages.
ALL_LINGUAS="bg ca cs da de el en_GB es_NI es eu fa fi fr he hu it ja ko lt lv nb ne nl pl pt_BR pt ro ru rw sk sv ta tr uk vi zh_CN zh_TW"

GETTEXT_PACKAGE=gnucash
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
    [GetText version number])

AM_GLIB_GNU_GETTEXT


# Enable only when we have autoconf --version >= 2.59
# on all platforms. Note that AC_PREREQ(2.59) may be
# insufficient to actually use v2.59 on OSX
#AC_CONFIG_MACRO_DIR(macros)

AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG


AC_LIBTOOL_DLOPEN
AM_DISABLE_STATIC
AM_PROG_LIBTOOL

AC_SUBST(LIBTOOL_DEPS)

AC_ISC_POSIX
AC_C_BIGENDIAN
AC_PROG_MAKE_SET
AC_PROG_LN_S
AC_HEADER_STDC

AC_CHECK_HEADERS(limits.h sys/time.h sys/times.h sys/wait.h pow.h)
AC_CHECK_FUNCS(stpcpy memcpy timegm towupper)
AC_CHECK_FUNCS(setenv,,[
  AC_CHECK_FUNCS(putenv,,[
    AC_MSG_ERROR([Must have one of the setenv or putenv functions.])
  ])
])

STRUCT_TM_GMTOFF_CHECK
SCANF_LLD_CHECK
if test x$am_cv_scanf_lld = "xno"; then
  SCANF_QD_CHECK

  if test x$am_cv_scanf_qd = "xno"; then
    SCANF_I64D_CHECK
    if test x$am_cv_scanf_i64d = "xno"; then
      AC_MSG_ERROR([Cannot continue, no long long conversion support in scanf.])
    fi
  fi
fi

# Build dir adjustments
AM_CONDITIONAL(GNUCASH_SEPARATE_BUILDDIR, test "x${srcdir}" != "x.")

# Init binreloc
AM_BINRELOC

### --------------------------------------------------------------------------
### See if we have pkg-config

AC_PATH_PROG(PKG_CONFIG,pkg-config)
if test "x$PKG_CONFIG" = x; then
  # @*%&$ Ximian programmers renamed this application
  AC_PATH_PROG(PKG_CONFIG,pkgconfig)
  if test "x$PKG_CONFIG" = x; then
    AC_MSG_ERROR([Could not find the pkg-config (or pkgconfig) program.],)
  fi
fi

### --------------------------------------------------------------------------
### Glib checks.

# We require glib >= 2.28, released together with gtk-2.24; There are
# some thread deprecations that affect us in 2.32, so we check for
# that first.
PKG_CHECK_MODULES(GLIB,
                  glib-2.0 >= 2.32 gthread-2.0 gobject-2.0 gmodule-2.0,
		  [AC_DEFINE([HAVE_GLIB_2_32], [1],
		             [Configure gthread deprecations])],
		  [PKG_CHECK_MODULES(GLIB,
		   glib-2.0 >= 2.28 gthread-2.0 gobject-2.0 gmodule-2.0)])

AC_CHECK_HEADERS(dirent.h dlfcn.h dl.h utmp.h locale.h mcheck.h unistd.h wctype.h)

# I'm sure we (used to?) require this in various places, so don't remove
# this test unless you've done sufficient code review/testing.
AC_MSG_CHECKING(if unsigned long is at least as big as guint32)
GNC_OLDCFLAGS="$CFLAGS"
GNC_OLDLDFLAGS="$LDFLAGS"
CFLAGS="${CFLAGS} ${GLIB_CFLAGS}"
LDFLAGS="${LDFLAGS} ${GLIB_LIBS}"
AC_TRY_RUN([
  #include <glib.h>
  int main(int argc, char *argv[]) {
    return(!(sizeof(unsigned long) >= sizeof(guint32)));
  }
],[
  AC_MSG_RESULT(yes)
],[
  AC_MSG_RESULT(no)
  AC_MSG_ERROR(cannot continue, size of unsigned long too small.)
],[
  AC_MSG_RESULT(assuming yes)
])
CFLAGS="$GNC_OLDCFLAGS"
LDFLAGS="$GNC_OLDLDFLAGS"

# Gnucash replaced dlopen/dlsym by the g_module functions; dlsym
# is needed optionally in one place for BSD linkers, though.
DL_LIB=
AC_CHECK_FUNCS(dlsym,,[
  AC_CHECK_LIB(dl, dlsym, DL_LIB="-ldl",[
    AC_CHECK_LIB(dld, shl_load, DL_LIB="-ldld",[
      AC_CHECK_FUNCS(dlsym, DL_LIB="")
    ])
  ])
])

oLIBS="$LIBS"
LIBS="$LIBS $DL_LIB"
AC_CHECK_FUNCS(dlerror,,)
LIBS="$oLIBS"
AC_SUBST(DL_LIB)

##################################################
# OS dependent checks
##################################################
AC_SUBST(SED)

# Set some defaults, they may be overridden below
REGEX_LIBS=""

AC_MSG_CHECKING(operating system)
platform=linux/other
AC_MSG_RESULT($platform)
AC_SUBST(REGEX_LIBS)

AM_CONDITIONAL(PLATFORM_LINUX, test "x$platform" = "xlinux/other")

AC_CHECK_HEADERS(X11/Xlib.h glob.h)
AC_CHECK_FUNCS(chown gethostname getppid getuid gettimeofday gmtime_r)
AC_CHECK_FUNCS(gethostid link)
##################################################

### --------------------------------------------------------------------------
### look for gconf

PKG_CHECK_MODULES(GCONF, gconf-2.0 >= 2.22)
# Look up gconftool-2
AC_PATH_PROG(GCONFTOOL, gconftool-2)
AC_SUBST(GCONFTOOL)
AC_SUBST(GCONF_CFLAGS)
AC_SUBST(GCONF_LIBS)

### --------------------------------------------------------------------------
### LIBXML -- GNOME_XML_LIB is defined by GNOME_XML_CHECK

LIBXML2_REQUIRED=2.5.10
PKG_CHECK_MODULES(LIBXML2, libxml-2.0 >= $LIBXML2_REQUIRED)
AS_SCRUB_INCLUDE(LIBXML2_CFLAGS)
AC_SUBST(LIBXML2_CFLAGS)
AC_SUBST(LIBXML2_LIBS)

oLIBS="$LIBS"
LIBS="$LIBS $LIBXML2_LIBS"

# Compile a test program to verify that the xml2 library works.
AC_CHECK_LIB(xml2, xmlElemDump, [:], [
  AC_MSG_ERROR([libxml2 is required for GnuCash])
  ])
LIBS="$oLIBS"

### --------------------------------------------------------------------------
### Zlib

ZLIB_LIBS=-lz
AC_SUBST(ZLIB_LIBS)

oLIBS="$LIBS"
LIBS="$LIBS $ZLIB_LIBS"
AC_CHECK_LIB(z, gzopen, [:], [
  AC_MSG_ERROR([zlib is required for GnuCash])
])
LIBS="$oLIBS"

### --------------------------------------------------------------------------
### Internal code part which is called "qof"

LIBQOF_LIBRARY_VERSION=1:4:0
AC_SUBST(LIBQOF_LIBRARY_VERSION)

QOF_CFLAGS='-I${top_builddir}/src/libqof/qof -I${top_srcdir}/src/libqof/qof'
QOF_LIBS='${top_builddir}/src/libqof/qof/libgnc-qof.la'
AC_SUBST(QOF_CFLAGS)
AC_SUBST(QOF_LIBS)

### --------------------------------------------------------------------------
### Variables
### Set up all the initial variable values...
GNC_CONFIGDIR='${sysconfdir}/gnucash'
GNC_DOC_INSTALL_DIR='${docdir}'
GNC_INCLUDE_DIR='${includedir}/gnucash'
GNC_SCM_INSTALL_DIR='${pkgdatadir}/scm'
GNC_SHAREDIR='${pkgdatadir}'
GNC_LIBEXECDIR='${libexecdir}/gnucash'

GNC_ACCOUNTS_DIR='${GNC_SHAREDIR}/accounts'
GNC_CHECKS_DIR='${GNC_SHAREDIR}/checks'
GNC_GTKBUILDER_DIR='${GNC_SHAREDIR}/gtkbuilder'
GNC_UI_DIR='${GNC_SHAREDIR}/ui'
GNC_PIXMAP_DIR='${GNC_SHAREDIR}/pixmaps'

AC_SUBST(GNC_ACCOUNTS_DIR)
AC_SUBST(GNC_CHECKS_DIR)
AC_SUBST(GNC_CONFIGDIR)
AC_SUBST(GNC_DOC_INSTALL_DIR)
AC_SUBST(GNC_GTKBUILDER_DIR)
AC_SUBST(GNC_UI_DIR)
AC_SUBST(GNC_INCLUDE_DIR)
AC_SUBST(GNC_PIXMAP_DIR)
AC_SUBST(GNC_SCM_INSTALL_DIR)
AC_SUBST(GNC_SHAREDIR)
AC_SUBST(GNC_LIBEXECDIR)

AC_ARG_ENABLE( debug,
  [AS_HELP_STRING([--enable-debug],[compile with debugging flags set])],
  [
     # remove any optimization flags...
     CFLAGS=`echo ${CFLAGS} | sed -e 's,-O.,,g'`
     # ...except for those the user wants.
     CFLAGS="${CFLAGS} -g ${USER_OPTIMIZATION}"
     LDFLAGS="${LDFLAGS} -g"
     AC_DEFINE(DEBUG_MEMORY,1,[Enable debug memory])
  ],
  [     AC_DEFINE(DEBUG_MEMORY,0,[Enable debug memory])
  ])

AC_ARG_ENABLE( profile,
  [AS_HELP_STRING([--enable-profile],[compile with profiling set])],
  CFLAGS="${CFLAGS} -pg"
  LDFLAGS="${LDFLAGS} -pg")

### --------------------------------------------------------------------------
### i18n

# This variable is set by GLIB_WITH_NLS, called through
# AM_GLIB_GNU_GETTEXT above. Usually it is "share", but on some
# platforms it is "lib". Make sure it is set.
if test "x$DATADIRNAME" = "x"; then
  DATADIRNAME="share"
  AC_SUBST(DATADIRNAME)
fi

dnl check for nl_langinfo(D_FMT) which is missing on FreeBSD
LANGINFO_D_FMT_CHECK

dnl Make sure we have a proper gettext installed
AC_MSG_CHECKING(for a valid gettext/gmsgfmt installation)
if test "$gt_cv_have_gettext" != "yes" || test "x$GMSGFMT" = "x"; then
  AC_MSG_RESULT(no)
  AC_MSG_ERROR([Cannot find Glib Gettext.  Maybe you need to install the gettext package?])
else
  AC_MSG_RESULT(yes - $GMSGFMT)
fi

# -------------------------------------------------------------------------
# URLs for MySQL and PostgreSQL testing
AC_ARG_WITH(test-mysql-url,
            AS_HELP_STRING([--with-test-mysql-url=URL],
                           [MySQL database URL for testing [default=none]]),
            [],[with_test_mysql_url=""])

TEST_MYSQL_URL=$with_test_mysql_url
AC_SUBST(TEST_MYSQL_URL)

AC_ARG_WITH(test-pgsql-url,
            AS_HELP_STRING([--with-test-pgsql-url=URL],
                           [PgSQL database URL for testing [default=none]]),
            [],[with_test_pgsql_url=""])

TEST_PGSQL_URL=$with_test_pgsql_url
AC_SUBST(TEST_PGSQL_URL)

### --------------------------------------------------------------------------
### Check for etags

AC_ARG_ENABLE( etags,
  [AS_HELP_STRING([--enable-etags],[enable automatic create of TAGS file])],
  if test x$enableval = xyes; then
     USE_ETAGS=1
  fi,
  USE_ETAGS=0)

if test x${USE_ETAGS} = x1; then
  AC_CHECK_PROG(GNC_ETAGS_FILE, etags, TAGS)
fi

AM_CONDITIONAL(GNC_ETAGS_FILE, test x${GNC_ETAGS_FILE} = xTAGS)

### --------------------------------------------------------------------------
### Check for ctags

AC_ARG_ENABLE( ctags,
  [AS_HELP_STRING([--enable-ctags],[enable automatic create of tags file])],
  if test x$enableval = xyes; then
     USE_CTAGS=1
  fi,
  USE_CTAGS=0)

if test x${USE_CTAGS} = x1; then
  AC_CHECK_PROG(GNC_CTAGS_FILE, ctags, tags)
fi

AM_CONDITIONAL(GNC_CTAGS_FILE, test x${GNC_CTAGS_FILE} = xtags)

### --------------------------------------------------------------------------
### Check for perl

# Check for perl, force version 5
AC_ARG_WITH(perl,
  [AS_HELP_STRING([--with-perl=FILE],[which perl executable to use])],
  PERL="${with_perl}")

# If the user didn't specify a perl, then go fetch.
if test x"$PERL" = x;
then
  AC_PATH_PROG(PERL, perl)
fi

# Make sure Perl was found
if test x"$PERL" = x; then
    AC_MSG_ERROR([Cannot find Perl. Try using the --with-perl flag.])
fi

# Make sure it's version 5 or later
if "$PERL" -e 'exit 1 if $] < 5.0'; then
    :
else
    AC_MSG_ERROR([Found ${PERL} reports version ]
                 [`${PERL} -e 'print $]'`, need version 5.*])
fi
AC_SUBST(PERL)

### ----------------------------------------------------------------------------
### Check for doxygen, mostly stolen from http://log4cpp.sourceforge.net/

AC_DEFUN([BB_ENABLE_DOXYGEN],
  [
    AC_ARG_ENABLE(doxygen, [AS_HELP_STRING([--enable-doxygen],[enable documentation generation with doxygen (auto)])])
    AC_ARG_ENABLE(dot, [AS_HELP_STRING([--enable-dot],[use 'dot' to generate graphs in doxygen (auto)])])
    AC_ARG_ENABLE(html-docs, [AS_HELP_STRING([--enable-html-docs],[enable HTML generation with doxygen (yes)])], [], [ enable_html_docs=yes])
    AC_ARG_ENABLE(latex-docs, [AS_HELP_STRING([--enable-latex-docs],[enable LaTeX documentation generation with doxygen (no)])], [], [ enable_latex_docs=no])
    if test "x$enable_doxygen" = xno; then
      enable_doc=no
    else
      AC_PATH_PROG(DOXYGEN, doxygen, , $PATH)
      if test x$DOXYGEN = x; then
        if test "x$enable_doxygen" = xyes; then
          AC_MSG_ERROR([could not find doxygen])
        fi
        enable_doc=no
      else
        enable_doc=yes
        AC_PATH_PROG(DOT, dot, , $PATH)
      fi
    fi
    AM_CONDITIONAL(DOC, test x$enable_doc = xyes)

    if test x$DOT = x; then
      if test "x$enable_dot" = xyes; then
        AC_MSG_ERROR([could not find dot])
      fi
      enable_dot=no
    else
      enable_dot=yes
    fi
    AM_CONDITIONAL(ENABLE_DOXYGEN, test x$enable_doc = xtrue)
    AC_SUBST(enable_dot)
    AC_SUBST(enable_html_docs)
    AC_SUBST(enable_latex_docs)
  ])

# check for doxygen
BB_ENABLE_DOXYGEN


### --------------------------------------------------------------------------
### Libraries
LIBS="$LIBS -lm"


### --------------------------------------------------------------------------
### Whether or not to build the GTK GUI components.
### When disabled, we don't even need to check for them!
### --------------------------------------------------------------------------

AC_ARG_ENABLE(gui,
  [AS_HELP_STRING([--disable-gui],[build without the GNOME GUI components of Gnucash])],
  [case "${enableval}" in
     yes) gnc_build_gui=true ;;
     no)  gnc_build_gui=false ;;
     *) gnc_build_gui=true ;;
   esac],
   [gnc_build_gui=true])

AM_CONDITIONAL(GNUCASH_ENABLE_GUI, test x${gnc_build_gui} = xtrue)

### --------------------------------------------------------------------------
### GNOME gui components -- built by default, but not if --enable-gui=no
### or --disable-gui is passed to autogen
### --------------------------------------------------------------------------

if test x${gnc_build_gui} = xtrue ;
then
  if test x${enable_compile_warnings} = x ;
  then
    enable_compile_warnings="yes"
  fi
  GNOME_COMPILE_WARNINGS

  PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.24.0)
  AS_SCRUB_INCLUDE(GTK_CFLAGS)
  AC_SUBST(GTK_CFLAGS)
  AC_SUBST(GTK_LIBS)

  PKG_CHECK_MODULES(GNOMECANVAS, libgnomecanvas-2.0)
  AS_SCRUB_INCLUDE(GNOMECANVAS_CFLAGS)
  AC_SUBST(GNOMECANVAS_CFLAGS)
  AC_SUBST(GNOMECANVAS_LIBS)

  PKG_CHECK_MODULES(GDK_PIXBUF, gdk-pixbuf-2.0)
  AS_SCRUB_INCLUDE(GDK_PIXBUF_CFLAGS)
  AC_SUBST(GDK_PIXBUF_CFLAGS)
  AC_SUBST(GDK_PIXBUF_LIBS)

  ### --------------------------------------------------------------------------
  ### determine the HTML engine

  AC_MSG_CHECKING([the html engine])
  AC_ARG_WITH(html_engine,
              AS_HELP_STRING([--with-html-engine=@<:@webkit@:>@],
                             [Obsolete: Only webkit is used as HTML engine now]),
              [],[with_html_engine="webkit"])

  AC_MSG_RESULT([$with_html_engine])

  case "$with_html_engine" in

    gtkhtml)
      # No more gtkhtml in gnucash-2.5.0
      AC_MSG_ERROR([gtkhtml is no longer sufficient for gnucash, as we now require the CSS/javascript features using webkit. Please remove your --with-html-engine argument from configure.])
      ;;

    webkit)
      PKG_CHECK_MODULES(WEBKIT, webkit-1.0 >= "1.0")
      case "$platform" in
        *)
          # Unsure - check functions exist
          oLIBS="$LIBS"
          LIBS="$LIBS ${WEBKIT_LIBS}"
          AC_CHECK_FUNCS(webkit_web_view_load_uri)
          LIBS="$oLIBS"
          AC_MSG_CHECKING(for webkit_web_frame_print_full)
          saved_CFLAGS="${CFLAGS}"
          saved_LIBS="${LIBS}"
          CFLAGS="${CFLAGS} ${WEBKIT_CFLAGS}"
          LIBS="${LIBS} ${WEBKIT_LIBS}"
          AC_LINK_IFELSE(
              [AC_LANG_PROGRAM(
                  [[#include <webkit/webkit.h>]],
                  [[webkit_web_frame_print_full( 0, 0, 0, 0 );]])],
                  [AC_MSG_RESULT(yes)
                   AC_DEFINE(HAVE_WEBKIT_WEB_FRAME_PRINT_FULL,1,[webkit_web_frame_print_full exists])],
                  [AC_MSG_RESULT(no)])
          CFLAGS="${saved_CFLAGS}"
          LIBS="${saved_LIBS}"
          ;;
        esac
        ;;

    *) AC_MSG_ERROR([Invalid HTML engine: must be webkit]) ;;
  esac

  AC_SUBST(WEBKIT_CFLAGS)
  AC_SUBST(WEBKIT_LIBS)

  ###-----------------------------------------------------------------------
  ## Find a suitable password store

  case $host_os in
    *)
      PKG_CHECK_MODULES(GNOME_KEYRING, gnome-keyring-1 >= "0.6",
          [AC_DEFINE(HAVE_GNOME_KEYRING,1,[System has gnome-keyring 0.6 or better])],
          [AC_DEFINE(HAVE_NO_KEYRING,1,[System has no suitable keyring service])])
  esac

  ### ----------------------------------------------------------------------

  AC_ARG_ENABLE( efence,
    [AS_HELP_STRING([--enable-efence],[link using efence])],
    if test x$enableval = xyes; then
      EFENCE_LIBS="-lefence"
      USE_EFENCE=1
      AC_DEFINE(USE_EFENCE,,We are using EFence)
    fi,
    USE_EFENCE=0
    EFENCE_LIBS="")
  AC_SUBST(EFENCE_LIBS)

  ### ----------------------------------------------------------------------
  ## For now, we just presume you're using the GNOME version.  The other
  ## UI's haven't been carried over during the automake transition.  At
  ## some point, if it's deemed worthwhile, they can be resurrected...

  GNOME=1
  AC_DEFINE(GNOME,,using GNOME)

else
  # GNOME_COMPILE_WARNINGS will add -Wall; no need to set it again.
  # also, only add it for GCC.
  if test ${GCC}x = yesx
  then
    # We should always see these errors...
    CFLAGS="${CFLAGS} -Wall"
  fi

fi
### End of gui-only checks

###-------------------------------------------------------------------------
### Selectively disable deprecated bits of glib/gdk/gtk/gnome
###-------------------------------------------------------------------------

allow_deprecated=true
AC_ARG_ENABLE(deprecated-glib,
  [AS_HELP_STRING([--disable-deprecated-glib],[don't use deprecated glib functions])],
  [case "${enableval}" in
     no) allow_deprecated=false ;;
     *)  allow_deprecated=true ;;
   esac]
  )
if test x${allow_deprecated} != "xtrue"
then
  # FIXME: This define is unused; this block can be removed
  AC_DEFINE(G_DISABLE_DEPRECATED,1, [Don't use deprecated glib functions])
fi


# default: Allow deprecated gtk functions
allow_deprecated=true
AC_ARG_ENABLE(deprecated-gtk,
  [AS_HELP_STRING([--disable-deprecated-gtk],[don't use deprecated gtk, gdk or gdk-pixbuf functions])],
  [case "${enableval}" in
     no) allow_deprecated=false ;;
     *)  allow_deprecated=true ;;
   esac]
  )
if test x${allow_deprecated} != "xtrue"
then
  # FIXME: This define is unused; this block can be removed
  AC_DEFINE(GTK_DISABLE_DEPRECATED,1, [Don't use deprecated gtk functions])
  AC_DEFINE(GDK_DISABLE_DEPRECATED,1, [Don't use deprecated gdk functions])
  AC_DEFINE(GDK_PIXBUF_DISABLE_DEPRECATED,1, [Don't use deprecated gdk-pixbuf functions])
fi


allow_deprecated=true
AC_ARG_ENABLE(deprecated-gnome,
  [AS_HELP_STRING([--disable-deprecated-gnome],[don't use deprecated gnome functions])],
  [case "${enableval}" in
     no) allow_deprecated=false ;;
     *)  allow_deprecated=true ;;
   esac]
  )
if test x${allow_deprecated} != "xtrue"
then
  # FIXME: This define is unused; this block can be removed
  AC_DEFINE(GNOME_DISABLE_DEPRECATED,1, [Don't use deprecated gnome functions])
fi

if test x$am_cv_val_LC_MESSAGES = "xno"; then
  LC_MESSAGES_ENUM="LC_ALL"
else
  LC_MESSAGES_ENUM="LC_MESSAGES"
fi
AC_SUBST(LC_MESSAGES_ENUM)

###-------------------------------------------------------------------------
### Additional compiler warnings (or not) if we're running GCC
###-------------------------------------------------------------------------

# This has to come after AC_PROG_CC _AND_ GNOME_COMPILE_WARNINGS
AC_MSG_CHECKING(what extra warning flags to pass to the C compiler)
if test ${GCC}x = yesx
then
  warnFLAGS=
  CFLAGS="${CFLAGS} -Wno-unused"
  # other flags...
  # These next two are included in the GNOME_COMPILE_WARNINGS
  #warnFLAGS="${warnFLAGS} -Wmissing-prototypes"
  #warnFLAGS="${warnFLAGS} -Wmissing-declarations"
  #warnFLAGS="${warnFLAGS} -Werror-implicit-function-declaration" # In -Wall

  # This code is from svn/svk/git/bzr, so enable error-on-warning
  error_on_warning_as_default="yes"

  # Enable error-on-warning by default -- I'm tired of fixing other
  # people's missing #includes, etc.
  AC_ARG_ENABLE(error-on-warning,
    [AS_HELP_STRING([--disable-error-on-warning],[disable treating compile warnings as errors])],
    [case "${enableval}" in
       yes) warnFLAGS="${warnFLAGS} -Werror" ; gnc_error_on_warning=yes ;;
       no)  gnc_error_on_warning=no ;;
       *) AC_MSG_ERROR(bad value ${enableval} for --enable-error-on-warning) ;;
     esac],
    [ if test "${error_on_warning_as_default}" = "yes"; then
        warnFLAGS="${warnFLAGS} -Werror";
        gnc_error_on_warning=auto
      else
        gnc_error_on_warning=no
      fi
    ])

  # For gcc >= 3.4.x, specifically enable the new warning switch
  # -Wdeclaration-after-statement in order to preserve source code
  # compatibility to gcc 2.95 and other compilers.
  GCC_VERSION=`${CC} -dumpversion`
  if test "`echo ${GCC_VERSION} | cut -d. -f1`" -ge 3; then
    # This is gcc >= 3.x.x
    if test "`echo ${GCC_VERSION} | cut -d. -f2`" -ge 4; then
      # This is gcc >= 3.4.x
      warnFLAGS="${warnFLAGS} -Wdeclaration-after-statement"
    else if test "`echo ${GCC_VERSION} | cut -d. -f1`" -ge 4; then
           # This is gcc == 4.x.x
           warnFLAGS="${warnFLAGS} -Wdeclaration-after-statement -Wno-pointer-sign"
           # rpmbuild on FC4 forces this flag. Can't hurt to always compile with it.
           warnFLAGS="${warnFLAGS} -D_FORTIFY_SOURCE=2"
         fi
    fi
  fi
  CFLAGS="${warnFLAGS} ${CFLAGS}"
else
  warnFLAGS=none
fi
AC_MSG_RESULT($warnFLAGS)

### --------------------------------------------------------------------------
### Adjustments -- especially executables that aren't generated via
### makefiles, so that we don't have an opportunity to adjust them
### there.

chmod u+x ${srcdir}/src/gnc-test-env
chmod u+x ${srcdir}/src/bin/generate-gnc-script
chmod u+x ${srcdir}/src/bin/overrides/gnucash-make-guids

### --------------------------------------------------------------------------
### Makefile creation

# This is necessary so that .o files in LIBOBJS are also built via
# the ANSI2KNR-filtering rules.
LIBOBJS_SEDSCRIPT="s,\.[[^.]]* ,$U&,g;s,\.[[^.]]*\$\$,$U&,"
AC_SUBST(LIBOBJS_SEDSCRIPT)

# Don't add empty lines in the following macro, they
# will cause the Windows build to fail if using a git repo as source
AC_CONFIG_FILES(
  po/Makefile.in
  dnl # Makefiles
  Makefile
  accounts/Makefile
  accounts/C/Makefile
  accounts/cs/Makefile
  accounts/da/Makefile
  accounts/de_AT/Makefile
  accounts/de_CH/Makefile
  accounts/de_DE/Makefile
  accounts/el_GR/Makefile
  accounts/en_GB/Makefile
  accounts/es_ES/Makefile
  accounts/es_MX/Makefile
  accounts/fi_FI/Makefile
  accounts/fr_CA/Makefile
  accounts/fr_CH/Makefile
  accounts/fr_FR/Makefile
  accounts/hu_HU/Makefile
  accounts/it/Makefile
  accounts/ja/Makefile
  accounts/ko/Makefile
  accounts/lv/Makefile
  accounts/nl/Makefile
  accounts/nb/Makefile
  accounts/pl/Makefile
  accounts/pt_BR/Makefile
  accounts/pt_PT/Makefile
  accounts/ru/Makefile
  accounts/sk/Makefile
  accounts/sv_SE/Makefile
  accounts/tr_TR/Makefile
  accounts/zh_CN/Makefile
  checks/Makefile
  doc/Makefile
  doc/examples/Makefile
  intl-scm/Makefile
  packaging/Makefile
  src/Makefile
  src/app-utils/Makefile
  src/app-utils/test/Makefile
  src/backend/Makefile
  src/backend/xml/Makefile
  src/backend/xml/test/Makefile
  src/backend/xml/test/test-files/Makefile
  src/backend/xml/test/test-files/xml2/Makefile
  src/backend/sql/Makefile
  src/backend/sql/test/Makefile
  src/bin/Makefile
  src/bin/overrides/Makefile
  src/bin/test/Makefile
  src/core-utils/Makefile
  src/core-utils/test/Makefile
  src/debug/Makefile
  src/debug/valgrind/Makefile
  src/doc/Makefile
  src/doc/design/Makefile
  src/doc/xml/Makefile
  src/engine/Makefile
  src/engine/test/Makefile
  src/engine/test-core/Makefile
  src/gnc-module/Makefile
  src/gnc-module/test/Makefile
  src/gnc-module/test/misc-mods/Makefile
  src/gnome/Makefile
  src/gnome/gnucash.desktop.in
  src/gnome/gtkbuilder/Makefile
  src/gnome/schemas/Makefile
  src/gnome/ui/Makefile
  src/gnome-utils/Makefile
  src/gnome-utils/gtkbuilder/Makefile
  src/gnome-utils/schemas/Makefile
  src/gnome-utils/test/Makefile
  src/gnome-utils/ui/Makefile
  src/gnome-search/Makefile
  src/html/Makefile
  src/import-export/Makefile
  src/import-export/test/Makefile
  src/import-export/schemas/Makefile
  src/import-export/log-replay/Makefile
  src/libqof/Makefile
  src/libqof/qof/Makefile
  src/libqof/qof/test/Makefile
  src/pixmaps/Makefile
  src/quotes/Makefile
  src/register/Makefile
  src/register/ledger-core/Makefile
  src/register/ledger-core/test/Makefile
  src/register/register-core/Makefile
  src/register/register-core/test/Makefile
  src/register/register-gnome/Makefile
  src/register/register-gnome/test/Makefile
  src/scm/Makefile
  src/scm/gnumeric/Makefile
  src/test-core/Makefile
  src/business/Makefile
  src/business/business-gnome/Makefile
  src/business/business-gnome/gtkbuilder/Makefile
  src/business/business-gnome/schemas/Makefile
  src/business/business-gnome/ui/Makefile
  src/business/business-ledger/Makefile
  dnl # Stuff for bill/invoice import plugin
  src/plugins/Makefile
  src/plugins/bi_import/Makefile
  src/plugins/bi_import/gtkbuilder/Makefile
  src/plugins/bi_import/ui/Makefile
  dnl # Stuff for customer import.
  src/plugins/customer_import/Makefile
  src/plugins/customer_import/gtkbuilder/Makefile
  src/plugins/customer_import/ui/Makefile
  dnl # non-makefiles
  dnl # Please read doc/build-system before adding *anything* here
  ,
  dnl # commands go here, but we don't have any right now
)
AC_OUTPUT


AC_MSG_RESULT([
  Options detected/selected
  -------------------------
  gnucash version ...... : $VERSION
  Build for host ....... : $host
  Optional components... : $_COMPONENTS
  Extra Warnings ....... : $warnFLAGS
  CPPFLAGS ............. : $CPPFLAGS
  CFLAGS ............... : $CFLAGS
  LDFLAGS .............. : $LDFLAGS
  prefix................ : ${prefix}


])
