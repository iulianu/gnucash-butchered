#!/bin/sh

set -e

function add_step() { steps=("${steps[@]}" "$@"); }
function qpushd() { pushd "$@" >/dev/null; }
function qpopd() { popd >/dev/null; }

# c:/dir/sub
function win_fs_path() {
    echo "$*" | sed 's,\\,/,g'
}

# /c/dir/sub
function unix_path() {
    echo "$*" | sed 's,^\([A-Za-z]\):,/\1,;s,\\,/,g'
}

qpushd "$(dirname $(unix_path "$0"))"
. custom.sh

SEPS_ACLOCAL_FLAGS=" "
SEPS_AUTOTOOLS_CPPFLAGS=" "
SEPS_AUTOTOOLS_LDFLAGS=" "
SEPS_GUILE_LOAD_PATH=";"
SEPS_INTLTOOL_PERL=" "
SEPS_PATH=":"
SEPS_PKG_CONFIG_PATH=":"
SEPS_READLINE_CPPFLAGS=" "
SEPS_READLINE_LDFLAGS=" "
SEPS_REGEX_CPPFLAGS=" "
SEPS_REGEX_LDFLAGS=" "
SEPS_SCHEME_LIBRARY_PATH=";"
ENV_VARS="\
ACLOCAL_FLAGS \
AUTOTOOLS_CPPFLAGS \
AUTOTOOLS_LDFLAGS \
GUILE_LOAD_PATH \
INTLTOOL_PERL \
PATH \
PKG_CONFIG_PATH \
READLINE_CPPFLAGS \
READLINE_LDFLAGS \
REGEX_CPPFLAGS \
REGEX_LDFLAGS \
SCHEME_LIBRARY_PATH \
"

function setup() {
    echo
    echo "############################################################"
    echo "###  $*"
    echo "############################################################"
}

function die() {
    echo
    echo "!!! $* !!!"
    echo "!!! ABORTING !!!"
    exit -1
}

function quiet() { "$@" &>/dev/null; }
function add_to_env() {
    _SEP=`eval echo '"$'"SEPS_$2"'"'`
    _ENV=`eval echo '"$'"$2"'"'`
    _SED=`eval echo '"s#.*'"${_SEP}$1${_SEP}"'.*##"'`
    _TEST=`echo "${_SEP}${_ENV}${_SEP}" | sed "${_SED}"`
    if [ "$_TEST" ]; then
	if [ "$_ENV" ]; then
	    eval "$2_ADDS"'="'"$1${_SEP}"'$'"$2_ADDS"'"'
	else
	    eval "$2_ADDS"'="'"$1"'"'
	fi
	eval "$2"'="$'"$2_ADDS"'$'"$2_BASE"'"'
    fi
}

function prepare() {
    mkdir -p $TMP_DIR
    for _ENV in $ENV_VARS; do
	eval "${_ENV}_BASE"'=$'"${_ENV}"
	eval "${_ENV}_ADDS="
	eval export "${_ENV}"
    done
    DOWNLOAD_UDIR=`unix_path $DOWNLOAD_DIR`
    TMP_UDIR=`unix_path $TMP_DIR`
}

# usage:  smart_wget URL DESTDIR
function smart_wget() {
    _FILE=`basename $1`
    _DLD=`unix_path $2`

    # If the file already exists in the download directory ($2)
    # then don't do anything.  But if it does NOT exist then
    # download the file to the tmpdir and then when that completes
    # move it to the dest dir.
    if [ ! -f $_DLD/$_FILE ] ; then
	wget -c $1 -P $TMP_DIR
	mv $TMP_UDIR/$_FILE $_DLD
    fi
    LAST_FILE=$_DLD/$_FILE
}

# usage:  wget_unpacked URL DOWNLOAD_DIR UNPACK_DIR
function wget_unpacked() {
    smart_wget $1 $2
    _UPD=`unix_path $3`
    echo -n "Extracting ${LAST_FILE##*/} ... "
    case $LAST_FILE in
        *.zip)     unzip -q -o $LAST_FILE -d $_UPD;;
        *.tar.gz)  tar -xzpf $LAST_FILE -C $_UPD;;
        *.tar.bz2) tar -xjpf $LAST_FILE -C $_UPD;;
        *)         die "Cannot unpack file $LAST_FILE!";;
    esac
    echo "done"
}

function inst_wget() {
    setup Wget
    _WGET_UDIR=`unix_path $WGET_DIR`
    if quiet $_WGET_UDIR/wget --version
    then
        echo "already installed.  skipping."
    else
        mkdir -p $WGET_DIR
        tar -xjpf $DOWNLOAD_UDIR/wget*.tar.bz2 -C $WGET_DIR
        cp $_WGET_UDIR/*/*/wget.exe $WGET_DIR
    fi
    add_to_env $_WGET_UDIR PATH
    quiet wget --version || die "wget unavailable"
}

function inst_dtk() {
    setup MSYS DTK
    if quiet perl --help
    then
        echo "msys dtk already installed.  skipping."
    else
        smart_wget $DTK_URL $DOWNLOAD_DIR
        echo "!!! When asked for an installation path, specify $MSYS_DIR !!!"
        $LAST_FILE
        for file in \
	    /bin/{aclocal*,auto*,ifnames,libtool*,guile*} \
	    /share/{aclocal,aclocal-1.7,autoconf,autogen,automake-1.7,guile,libtool}
        do
            [ "${file##*.bak}" ] || continue
            _dst_file=$file.bak
            while [ -e $_dst_file ]; do _dst_file=$_dst_file.bak; done
            mv $file $_dst_file
        done
    fi
    quiet perl --help || die "msys dtk not installed correctly"
}

function inst_mingw() {
    setup MinGW
    if quiet gcc --version
    then
        echo "mingw already installed.  skipping."
    else
        _MINGW_WFSDIR=`win_fs_path $MINGW_DIR`
        smart_wget $MINGW_URL $DOWNLOAD_DIR
        echo "!!! Install g++ !!!"
        echo "!!! When asked for an installation path, specify $MINGW_DIR !!!"
        $LAST_FILE
        (echo "y"; echo "y"; echo "$_MINGW_WFSDIR") | sh pi.sh
    fi
    quiet gcc --version && quiet ld --help || die "mingw not installed correctly"
}

function inst_unzip() {
    setup Unzip
    _UNZIP_UDIR=`unix_path $UNZIP_DIR`
    if quiet $_UNZIP_UDIR/bin/unzip --help
    then
        echo "unzip already installed.  skipping."
    else
        smart_wget $UNZIP_URL $DOWNLOAD_DIR
        echo "!!! When asked for an installation path, specify $UNZIP_DIR !!!"
        $LAST_FILE
    fi
    add_to_env $_UNZIP_UDIR/bin PATH
    quiet unzip --help || die "unzip unavailable"
}

function inst_regex() {
    setup RegEx
    _REGEX_UDIR=`unix_path $REGEX_DIR`
    add_to_env -I$_REGEX_UDIR/include REGEX_CPPFLAGS
    add_to_env -L$_REGEX_UDIR/lib REGEX_LDFLAGS
    add_to_env $_REGEX_UDIR/bin PATH
    if quiet ld $REGEX_LDFLAGS -lregex -o $TMP_UDIR/ofile
    then
        echo "regex already installed.  skipping."
    else
        mkdir -p $REGEX_DIR
        wget_unpacked $REGEX_BIN_URL $DOWNLOAD_DIR $REGEX_DIR
        wget_unpacked $REGEX_LIB_URL $DOWNLOAD_DIR $REGEX_DIR
    fi
    quiet ld $REGEX_LDFLAGS -lregex -o $TMP_UDIR/ofile || die "regex not installed correctly"
}

function inst_readline() {
    setup Readline
    _READLINE_UDIR=`unix_path $READLINE_DIR`
    add_to_env -I$_READLINE_UDIR/include READLINE_CPPFLAGS
    add_to_env -L$_READLINE_UDIR/lib READLINE_LDFLAGS
    add_to_env $_READLINE_UDIR/bin PATH
    if quiet ld $READLINE_LDFLAGS -lreadline -o $TMP_UDIR/ofile
    then
        echo "readline already installed.  skipping."
    else
        mkdir -p $READLINE_DIR
        wget_unpacked $READLINE_BIN_URL $DOWNLOAD_DIR $READLINE_DIR
        wget_unpacked $READLINE_LIB_URL $DOWNLOAD_DIR $READLINE_DIR
    fi
    quiet ld $READLINE_LDFLAGS -lreadline -o $TMP_UDIR/ofile || die "readline not installed correctly"
}

function inst_indent() {
    setup Indent
    _INDENT_UDIR=`unix_path $INDENT_DIR`
    add_to_env $_INDENT_UDIR/bin PATH
    if quiet which indent
    then
        echo "indent already installed.  skipping."
    else
        mkdir -p $INDENT_DIR
        wget_unpacked $INDENT_BIN_URL $DOWNLOAD_DIR $INDENT_DIR
    fi
    quiet which indent || die "indent unavailable"
}

function inst_active_perl() {
    setup ActivePerl \(intltool\)
    _ACTIVE_PERL_WFSDIR=`win_fs_path $ACTIVE_PERL_DIR`
    add_to_env $_ACTIVE_PERL_WFSDIR/bin/perl INTLTOOL_PERL
    if quiet $INTLTOOL_PERL --help
    then
        echo "ActivePerl already installed.  skipping."
    else
        wget_unpacked $ACTIVE_PERL_URL $DOWNLOAD_DIR $ACTIVE_PERL_DIR
        # this is the first of several bad hacks
        # it would be much more natural to have a sort of -p flag like for `patch'
        # please deuglify me
        qpushd $ACTIVE_PERL_DIR
            cp -r ActivePerl/Perl/* .
            rm -rf ActivePerl
        qpopd
    fi
    quiet $INTLTOOL_PERL --help || die "ActivePerl not installed correctly"
}

function inst_guile() {
    setup Guile
    _GUILE_WFSDIR=`win_fs_path $GUILE_DIR`
    _GUILE_UDIR=`unix_path $GUILE_DIR`
    add_to_env $_GUILE_UDIR/bin PATH
    add_to_env $_GUILE_WFSDIR/share/guile/site/slib/ SCHEME_LIBRARY_PATH
    if quiet guile -c '(use-modules (srfi srfi-39))' &&
        quiet guile -c "(use-modules (ice-9 slib)) (require 'printf)"
    then
        echo "guile and slib already installed.  skipping."
    else
        smart_wget $GUILE_URL $DOWNLOAD_DIR
        smart_wget $SLIB_URL $DOWNLOAD_DIR
        tar -xzpf $DOWNLOAD_UDIR/guile-*.tar.gz -C $TMP_UDIR
        qpushd $TMP_UDIR/guile-*
            qpushd ice-9
                cp boot-9.scm boot-9.scm.bak
                cat boot-9.scm.bak | sed '/SIGBUS/d' > boot-9.scm
            qpopd
            qpushd libguile
                cp fports.c fports.c.bak
                cat fports.c.bak | sed 's,#elif defined (FIONREAD),#elif 0,' > fports.c
                cp load.c load.c.bak
                cat load.c.bak | sed '/scan !=/s,:,;,' > load.c
            qpopd
            qpushd libguile-ltdl
                cp raw-ltdl.c raw-ltdl.c.bak
                cat raw-ltdl.c.bak | sed 's,\(SCMLTSTATIC\) LT_GLOBAL_DATA,\1,' > raw-ltdl.c
                touch upstream/ltdl.c.diff
            qpopd
            ./configure \
	        --disable-elisp \
	        --disable-networking \
	        --disable-dependency-tracking \
	        --disable-libtool-lock \
	        --disable-linuxthreads \
	        -C --prefix=$_GUILE_WFSDIR \
	        ac_cv_func_regcomp_rx=yes \
	        CPPFLAGS="${READLINE_CPPFLAGS} ${REGEX_CPPFLAGS}" \
	        LDFLAGS="-lwsock32 ${READLINE_LDFLAGS} ${REGEX_LDFLAGS} -lregex"
	    cp config.status config.status.bak
	    cat config.status.bak | sed 's# fileblocks[$.A-Za-z]*,#,#' > config.status
	    ./config.status
	    for file in {srfi,libguile-ltdl,libguile}/Makefile; do
	        cp $file $file.bak
	        cat $file.bak | sed '/^lib.*LDFLAGS =/s,\(\\*\)$, -no-undefined \1,' > $file
	    done
	    qpushd guile-config
	      cp Makefile Makefile.bak
	      cat Makefile.bak | sed '/-bindir-/s,:,^,g' > Makefile
	    qpopd
	    make
	    make install
	qpopd
	qpushd $GUILE_DIR/bin
	    mv libguilereadline-v-12-12.dll libguilereadline-v-12.dll
	    mv libguile-srfi-srfi-4-v-1-1.dll libguile-srfi-srfi-4-v-1.dll
	    mv libguile-srfi-srfi-13-14-v-1-1.dll libguile-srfi-srfi-13-14-v-1.dll
	qpopd
	_SLIB_DIR=$GUILE_DIR\\share\\guile\\site
	mkdir -p $_SLIB_DIR
	unzip $DOWNLOAD_DIR/slib*.zip -d $_SLIB_DIR
	qpushd $_SLIB_DIR/slib
	    cp guile.init guile.init.bak
	    echo "(define software-type (lambda () 'MS-DOS))" >> guile.init
	qpopd
    fi
    add_to_env "-I $_GUILE_UDIR/share/aclocal" ACLOCAL_FLAGS
    guile -c '(use-modules (srfi srfi-39))' &&
    guile -c "(use-modules (ice-9 slib)) (require 'printf)" || die "guile not installed correctly"
}

function inst_openssl() {
    setup OpenSSL
    if [ -f $WINDIR\\system32\\libssl32.dll ]
    then
        echo "openssl already installed.  skipping."
    else
        smart_wget $OPENSSL_URL $DOWNLOAD_DIR
	echo "!!! When asked for an installation path, specify $OPENSSL_DIR !!!"
        $LAST_FILE
    fi
    [ -f $WINDIR\\system32\\libssl32.dll ] || die "openssl not installed correctly"
}

function inst_pexports() {
    setup pexports
    _PEXPORTS_UDIR=`unix_path $PEXPORTS_DIR`
    add_to_env $_PEXPORTS_UDIR/bin PATH
    if quiet which pexports
    then
        echo "pexports already installed.  skipping."
    else
        wget_unpacked $PEXPORTS_URL $DOWNLOAD_DIR $PEXPORTS_DIR
        qpushd $PEXPORTS_DIR
            mv pexports-* mydir
            mv mydir/* .
            rmdir mydir
        qpopd
    fi
    quiet which pexports || die "pexports unavailable"
}

function inst_libxml2() {
    setup LibXML2
    _LIBXML2_UDIR=`unix_path $LIBXML2_DIR`
    if quiet ld -L$_LIBXML2_UDIR/lib -lxml2 -o $TMP_UDIR/ofile
    then
        echo "libxml2 already installed.  skipping."
    else
        wget_unpacked $LIBXML2_URL $DOWNLOAD_DIR $LIBXML2_DIR
        qpushd $LIBXML2_DIR
            mv libxml2-* mydir
            cp -r mydir/* .
            rm -rf mydir
            pexports bin/libxml2.dll > libxml2.def
            dlltool --input-def libxml2.def --output-lib lib/libxml2.a
            rm libxml2.def
            _LIBXML2_VERSION=`echo $LAST_FILE | sed 's#.*libxml2-\(.*\).win32.zip#\1#'`
            mkdir -p lib/pkgconfig
            cat > lib/pkgconfig/libxml-2.0.pc <<EOF
prefix=/ignore
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: libXML
Version: $_LIBXML2_VERSION
Description: libXML library version2.
Requires:
Libs: -L\${libdir} -lxml2 -lz
Cflags: -I\${includedir}
EOF
        qpopd
    fi
    quiet ld -L$_LIBXML2_UDIR/lib -lxml2 -o $TMP_UDIR/ofile || die "libxml2 not installed correctly"
}

function inst_expat() {
    setup Expat
    _EXPAT_UDIR=`unix_path $EXPAT_DIR`
    add_to_env $_EXPAT_UDIR/bin PATH
    if quiet which xmlwf
    then
        echo "expat already installed.  skipping."
    else
        wget_unpacked $EXPAT_URL $DOWNLOAD_DIR $EXPAT_DIR
        qpushd $EXPAT_DIR
            cp -r expat/* .
            rm -rf expat
        qpopd
    fi
    quiet which xmlwf || die "expat not installed correctly"
}

function inst_gnome() {
    setup Gnome platform
    _GNOME_UDIR=`unix_path $GNOME_DIR`
    add_to_env $_GNOME_UDIR/bin PATH
    add_to_env $_GNOME_UDIR/lib/pkgconfig PKG_CONFIG_PATH
    add_to_env "-I $_GNOME_UDIR/share/aclocal" ACLOCAL_FLAGS
    if quiet gconftool-2 --version &&
        pkg-config --exists gconf-2.0 libgnome-2.0 libgnomeui-2.0 libgnomeprint-2.2 libgnomeprintui-2.2 libgtkhtml-3.8 &&
        quiet intltoolize --version
    then
        echo "gnome packages installed.  skipping."
    else
        mkdir -p $GNOME_DIR
	wget_unpacked $GETTEXT_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $GETTEXT_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBICONV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $GLIB_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $GLIB_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBJPEG_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBPNG_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $ZLIB_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $PKG_CONFIG_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $CAIRO_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $CAIRO_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $FONTCONFIG_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $FONTCONFIG_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $FREETYPE_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $FREETYPE_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $ATK_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $ATK_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $PANGO_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $PANGO_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBART_LGPL_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBART_LGPL_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $GTK_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $GTK_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $INTLTOOL_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $ORBIT2_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $ORBIT2_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $GAIL_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $GAIL_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $POPT_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $POPT_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $GCONF_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $GCONF_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBBONOBO_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBBONOBO_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $GNOME_VFS_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $GNOME_VFS_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBGNOME_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBGNOME_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBGNOMECANVAS_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBGNOMECANVAS_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBBONOBOUI_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBBONOBOUI_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBGNOMEUI_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBGNOMEUI_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBGLADE_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBGLADE_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBGNOMEPRINT_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBGNOMEPRINT_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBGNOMEPRINTUI_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $LIBGNOMEPRINTUI_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $GTKHTML_URL $DOWNLOAD_DIR $GNOME_DIR
	wget_unpacked $GTKHTML_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
        qpushd $GNOME_DIR
            [ -f bin/zlib1.dll ] || mv zlib1.dll bin
            [ -f lib/libz.dll.a ] || dlltool -D bin/zlib1.dll -d lib/zlib.def -l lib/libz.dll.a
        qpopd
    fi
    quiet gconftool-2 --version &&
    pkg-config --exists gconf-2.0 libgnome-2.0 libgnomeui-2.0 libgnomeprint-2.2 libgnomeprintui-2.2 libgtkhtml-3.8 &&
    quiet intltoolize --version || die "gnome not installed correctly"
}

function inst_gwrap() {
    setup G-Wrap
    _GWRAP_WFSDIR=`win_fs_path $GWRAP_DIR`
    _GWRAP_UDIR=`unix_path $GWRAP_DIR`
    add_to_env $_GWRAP_UDIR/bin PATH
    add_to_env $_GWRAP_WFSDIR/share/guile/site GUILE_LOAD_PATH
    if quiet g-wrap-config --version
    then
        echo "g-wrap already installed.  skipping."
    else
        wget_unpacked $GWRAP_URL $DOWNLOAD_DIR $TMP_DIR
        qpushd $TMP_UDIR/g-wrap-*
            qpushd g-wrap
                cp core-runtime.c core-runtime.c.bak
                cat core-runtime.c.bak | sed '/vasprintf/d' > core-runtime.c
            qpopd
            cp configure configure.bak
            cat configure.bak | sed 's,"glib","glib-2.0",g' > configure
            ./configure \
                --prefix=$_GWRAP_WFSDIR \
                --with-modules-dir=`echo $GWRAP_DIR | sed 's#\\\\#\\\\\\\\#g'`
            LDFLAGS="-no-undefined"
            qpushd guile/g-wrap/gw
                cp Makefile Makefile.bak
                cat Makefile.bak | sed '/^libgw_guile_standard_la_LIBADD/s,$, ../../../libffi/libffi.la ../../../g-wrap/libgwrap-core-runtime.la,;/libgw_guile_gw_glib_la_LIBADD/s,$, ../../../g-wrap/libgwrap-core-runtime.la,' > Makefile
                make standard.c gw-glib.c
                cp standard.scm standard.scm.bak
                cat standard.scm.bak | sed -c 's,\(libgw-guile-standard\),../lib/\1-0,' > standard.scm
                cp gw-glib.scm gw-glib.scm.bak
                cat gw-glib.scm.bak | sed -c 's,\(libgw-guile-gw-glib\),../lib/\1-0,' > gw-glib.scm
            qpopd
            qpushd guile/test
                cp Makefile Makefile.bak
                cat Makefile.bak | sed '/^std_libs/s,\\$, ../../libffi/libffi.la \\,' > Makefile
            qpopd
            make
	    qpushd guile/g-wrap
		# Fix the filenames of the to be loaded DLLs
		cp guile.scm guile.scm.bak
		sed "s/slot-ref wrapset 'shlib-path) \"\\\\\"))/slot-ref wrapset 'shlib-path) \"-0\\\\\"))/" guile.scm.bak > guile.scm
	    qpopd
            make install
        qpopd
    fi    
    add_to_env $_GWRAP_UDIR/lib/pkgconfig PKG_CONFIG_PATH
    guile -c '(use-modules (srfi srfi-39))' &&
    quiet g-wrap-config --version || die "g-wrap not installed correctly"
}

function inst_autotools() {
    setup Autotools
    _AUTOTOOLS_UDIR=`unix_path $AUTOTOOLS_DIR`
    add_to_env $_AUTOTOOLS_UDIR/bin PATH
    if quiet autoconf --help && quiet automake --help && quiet libtool --help 
    then
        echo "auto tools already installed.  skipping."
    else
        wget_unpacked $AUTOCONF_URL $DOWNLOAD_DIR $TMP_DIR
        wget_unpacked $AUTOMAKE_URL $DOWNLOAD_DIR $TMP_DIR
        wget_unpacked $LIBTOOL_URL $DOWNLOAD_DIR $TMP_DIR
        qpushd $TMP_UDIR/autoconf-*
            echo "building autoconf..."
           ./configure --prefix=$_AUTOTOOLS_UDIR
            make
            make install
        qpopd
        qpushd $TMP_UDIR/automake-*
            echo "building automake..."
            ./configure --prefix=$_AUTOTOOLS_UDIR
            make
            make install
        qpopd
        qpushd $TMP_UDIR/libtool-*
            echo "building libtool..."
            ./configure --prefix=$_AUTOTOOLS_UDIR
            make
            make install
        qpopd
    fi
    add_to_env -I$_AUTOTOOLS_UDIR/include AUTOTOOLS_CPPFLAGS
    add_to_env -L$_AUTOTOOLS_UDIR/lib AUTOTOOLS_LDFLAGS
    add_to_env "-I $_AUTOTOOLS_UDIR/share/aclocal" ACLOCAL_FLAGS
    quiet autoconf --help &&
    quiet automake --help &&
    quiet libtool --help || die "autotools not installed correctly"
}

function inst_libgsf() {
    setup libGSF
    _LIBGSF_UDIR=`unix_path $LIBGSF_DIR`
    add_to_env $_LIBGSF_UDIR/bin PATH
    add_to_env $_LIBGSF_UDIR/lib/pkgconfig PKG_CONFIG_PATH
    if quiet pkg-config --exists libgsf-1 libgsf-gnome-1
    then
	echo "libgsf already installed.  skipping."
    else
	wget_unpacked $LIBGSF_URL $DOWNLOAD_DIR $TMP_DIR
	qpushd $TMP_UDIR/libgsf-*
	    cp configure.in configure.in.bak
	    cat configure.in.bak | sed '/AC_PROG_INTLTOOL/s#$#([],[no-xml])#' > configure.in
	    autoconf
	    ./configure --prefix=$_LIBGSF_UDIR
	    make
	    make install
	qpopd
    fi
    pkg-config --exists libgsf-1 libgsf-gnome-1 || die "libgsf not installed correctly"
}

function inst_goffice() {
    setup GOffice
    _GOFFICE_UDIR=`unix_path $GOFFICE_DIR`
    add_to_env $_GOFFICE_UDIR/lib/pkgconfig PKG_CONFIG_PATH
    if quiet pkg-config --exists libgoffice-0.3
    then
	echo "goffice already installed.  skipping."
    else
	wget_unpacked $GOFFICE_URL $DOWNLOAD_DIR $TMP_DIR
	mydir=`pwd`
	qpushd $TMP_UDIR/goffice-*
	    cp configure.in configure.in.bak
	    cat configure.in.bak | sed '/AC_PROG_INTLTOOL/s#)$#,[no-xml])#' > configure.in
	    [ -n "$GOFFICE_PATCH" -a -f "$GOFFICE_PATCH" ] && \
		patch -p1 < $GOFFICE_PATCH
	    cp goffice/Makefile.am goffice/Makefile.am.bak
	    cat goffice/Makefile.am.bak \
		| sed '/LIBADD/s#-lurlmon##;s#-lhtmlhelp##' \
		> goffice/Makefile.am
	    automake
	    autoconf
	    ./configure --prefix=$_GOFFICE_UDIR
	    [ -f dumpdef.pl ] || cp -p ../libgsf-*/dumpdef.pl .
	    [ -f $mydir/intltool-merge ] && \
		( mv intltool-merge intltool-merge.bak ; \
		  cp -p $mydir/intltool-merge . )
	    echo libgoffice_init >> goffice/local.def
	    echo libgoffice_shutdown >> goffice/local.def
	    make
	    make install
	qpopd
    fi
    pkg-config --exists libgoffice-0.3 || die "goffice not installed correctly"
}

function inst_glade() {
    setup Glade
    _GLADE_UDIR=`unix_path $GLADE_DIR`
    _GLADE_WFSDIR=`win_fs_path $GLADE_DIR`
    add_to_env $_GLADE_UDIR/bin PATH
    if quiet glade-3 --version
    then
        echo "glade already installed.  skipping."
    else
        wget_unpacked $GLADE_URL $DOWNLOAD_DIR $TMP_DIR
        qpushd $TMP_UDIR/glade3-*
            ./configure --prefix=$_GLADE_WFSDIR
            make
            make install
        qpopd
    fi
    quiet glade-3 --version || die "glade not installed correctly"
}

function inst_svn() {
    setup Subversion
    _SVN_UDIR=`unix_path $SVN_DIR`
    export PATH="$_SVN_UDIR/bin:$PATH"
    if quiet svn --version
    then
        echo "subversion already installed.  skipping."
    else
        smart_wget $SVN_URL $DOWNLOAD_DIR
        echo "!!! When asked for an installation path, specify $SVN_DIR !!!"
        $LAST_FILE
    fi
}

function svn_up() {
    mkdir -p $REPOS_DIR
    qpushd $REPOS_DIR
    if [ -x .svn ]; then
	setup svn up
	svn up
    else
	setup svn co
	svn co $REPOS_URL .
    fi
    qpopd
}

function inst_gnucash() {
    setup GnuCash
    _GNUCASH_WFSDIR=`win_fs_path $GNUCASH_DIR`
    _GNUCASH_UDIR=`unix_path $GNUCASH_DIR`
    POPT_CPPFLAGS="-I${_GNOME_UDIR}/include"
    POPT_LDFLAGS="-L${_GNOME_UDIR}/lib"
    qpushd $REPOS_DIR
    cp configure.in configure.in.bak
    cat configure.in.bak | sed '/AC_PROG_INTLTOOL/s#TOOL$#TOOL([],[no-xml])#;/GUILE_LOAD_PATH/s,:,;,g' > configure.in
    ./autogen.sh
    ./configure \
	--prefix=$_GNUCASH_WFSDIR \
	--enable-debug \
	--enable-schemas-install=no \
	CPPFLAGS="${AUTOTOOLS_CPPFLAGS} ${REGEX_CPPFLAGS} ${POPT_CPPFLAGS} -D_WIN32" \
	LDFLAGS="${AUTOTOOLS_LDFLAGS} ${REGEX_LDFLAGS} ${POPT_LDFLAGS}" \
	PKG_CONFIG_PATH="${PKG_CONFIG_PATH}"

    # Add -no-undefined to LDFLAGS here manually because this must
    # not be given during the ./configure tests
    cp config.status config.status.bak ; sed 's/\(s,@LDFLAGS@,.*\),/\1 -no-undefined,/' config.status.bak > config.status
    ./config.status
    # Windows DLLs don't need relinking
    cp ltmain.sh ltmain.sh.bak ; grep -v "need_relink=yes" ltmain.sh.bak > ltmain.sh
    cp libtool libtool.bak ; grep -v "need_relink=yes" libtool.bak > libtool
    # Exclude (for now) the test subdirectories from the build
    # because executable linking is so painfully slow on mingw
    perl -pi.bak -e's#^(SUBDIRS.* )test( .*)?$#\1\2#' `find src -name Makefile`

    make

    # Try to fix the paths in the "gnucash" script
    qpushd src/bin
    rm gnucash
    make PATH_SEPARATOR=";" \
	bindir="${_GNUCASH_UDIR}/bin:${_GNUCASH_UDIR}/lib/bin:${_GOFFICE_UDIR}/bin:${_LIBGSF_UDIR}/bin:${_GWRAP_UDIR}/bin:${_GNOME_UDIR}/bin:${_LIBXML2_UDIR}/bin:${_GUILE_UDIR}/bin:${_REGEX_UDIR}/bin" \
	gnucash
    qpopd

    make install
    qpopd

    qpushd ${_GNUCASH_WFSDIR}/lib/gnucash
    # Remove the dependency_libs line from the installed .la files
    # because otherwise loading the modules literally takes hours.
    for A in *.la; do grep -v dependency_libs $A > tmp ; mv  tmp $A; done
    find . -name '*.a' -exec rm '{}' ';'
    qpopd

    # Create a startup script that works without the msys shell
    qpushd ${_GNUCASH_WFSDIR}/bin
    echo "set PATH=${GNUCASH_DIR}\\bin;${GNUCASH_DIR}\\lib\\bin;${GOFFICE_DIR}\\bin;${LIBGSF_DIR}\\bin;${GWRAP_DIR}\\bin;${GNOME_DIR}\\bin;${LIBXML2_DIR}\\bin;${GUILE_DIR}\\bin;${REGEX_DIR}\\bin" > gnucash.bat
    echo "set GUILE_WARN_DEPRECATED=no" >> gnucash.bat
    echo "set GNC_MODULE_PATH=${GNUCASH_DIR}\\lib\\gnucash" >> gnucash.bat
    echo "set GUILE_LOAD_PATH=${GUILE_DIR}\\share\\guile\\site;${GNUCASH_DIR}\\share\\gnucash\\guile-modules;${GNUCASH_DIR}\\share\\gnucash\\scm;%GUILE_LOAD_PATH%" >> gnucash.bat
    echo "start gnucash-bin" >> gnucash.bat
    qpopd
}

function finish() {
    _NEW=x
    for _ENV in $ENV_VARS; do
	_ADDS=`eval echo '"\$'"${_ENV}"'_ADDS"'`
	if [ "$_ADDS" ]; then
	    if [ "$_NEW" ]; then
		echo
		echo "Environment variables changed, please do the following"
		echo
		_NEW=
	    fi
	    _VAL=`eval echo '"$'"${_ENV}_BASE"'"'`
	    if [ "$_VAL" ]; then
		_CHANGE="export ${_ENV}=\"${_ADDS}"'$'"${_ENV}\""
	    else
		_CHANGE="export ${_ENV}=\"${_ADDS}\""
	    fi
	    echo $_CHANGE
	    echo echo "'${_CHANGE}' >> /etc/profile.d/installer.sh"
	fi
    done
}

prepare
for step in "${steps[@]}" ; do
    eval $step
done
finish
qpopd


### Local Variables: ***
### sh-basic-offset: 4 ***
### tab-width: 8 ***
### End: ***
