#!/bin/sh

[ ! "$BASH" -a -x /bin/bash ] && exec /bin/bash "$0" "$@"

set -e

echo -n "Build Starting at "
date

function qpushd() { pushd "$@" >/dev/null; }
function qpopd() { popd >/dev/null; }
function unix_path() { echo "$*" | sed 's,^\([A-Za-z]\):,/\1,;s,\\,/,g'; }

qpushd "$(dirname $(unix_path "$0"))"
. functions.sh
. defaults.sh

register_env_var ACLOCAL_FLAGS " "
register_env_var AUTOTOOLS_CPPFLAGS " "
register_env_var AUTOTOOLS_LDFLAGS " "
register_env_var GMP_CPPFLAGS " "
register_env_var GMP_LDFLAGS " "
register_env_var GNOME_CPPFLAGS " "
register_env_var GNOME_LDFLAGS " "
register_env_var GNUTLS_CPPFLAGS " "
register_env_var GNUTLS_LDFLAGS " "
register_env_var GUILE_LOAD_PATH ";"
register_env_var GUILE_CPPFLAGS " "
register_env_var GUILE_LDFLAGS " "
register_env_var HH_CPPFLAGS " "
register_env_var HH_LDFLAGS " "
register_env_var INTLTOOL_PERL " "
register_env_var LIBDBI_CPPFLAGS " "
register_env_var LIBDBI_LDFLAGS " "
register_env_var KTOBLZCHECK_CPPFLAGS " "
register_env_var KTOBLZCHECK_LDFLAGS " "
register_env_var PATH ":"
register_env_var PCRE_CPPFLAGS " "
register_env_var PCRE_LDFLAGS " "
register_env_var PKG_CONFIG ":" ""
register_env_var PKG_CONFIG_PATH ":"
register_env_var READLINE_CPPFLAGS " "
register_env_var READLINE_LDFLAGS " "
register_env_var REGEX_CPPFLAGS " "
register_env_var REGEX_LDFLAGS " "
register_env_var WEBKIT_CFLAGS " "
register_env_var WEBKIT_LIBS " "

function prepare() {
    # Necessary so that intltoolize doesn't come up with some
    # foolish AC_CONFIG_AUX_DIR; bug#362006
    # We cannot simply create install-sh in the repository, because
    # this will confuse other parts of the tools
    _REPOS_UDIR=`unix_path $REPOS_DIR`
    level0=.
    level1=$(basename ${_REPOS_UDIR})
    level2=$(basename $(dirname ${_REPOS_UDIR}))"/"$level1
    for mydir in $level0 $level1 $level2; do
        if [ -f $mydir/make-gnucash-potfiles.in ]; then
            die "Do not save install.sh in the repository or one its parent directories"
        fi
    done
#     # Remove old empty install-sh files
#     if [ -f ${_REPOS_UDIR}/install-sh -a "$(cat ${_REPOS_UDIR}/install-sh &>/dev/null | wc -l)" -eq 0 ]; then
#         rm -f ${_REPOS_UDIR}/install-sh
#     fi
    # Partially remove RegEx-GNU if installed
    _REGEX_UDIR=`unix_path $REGEX_DIR`
    if [ -f ${_REGEX_UDIR}/contrib/regex-0.12-GnuWin32.README ]; then
        qpushd ${_REGEX_UDIR}
            rm -f bin/*regex*.dll
            rm -f contrib/regex*
            rm -f lib/*regex*
        qpopd
    fi

    DOWNLOAD_UDIR=`unix_path $DOWNLOAD_DIR`
    TMP_UDIR=`unix_path $TMP_DIR`
    mkdir -p $TMP_UDIR
    mkdir -p $DOWNLOAD_UDIR

    if [ "$DISABLE_OPTIMIZATIONS" = "yes" ]; then
        export CFLAGS="$CFLAGS -g -O0"
    fi

    if [ "$CROSS_COMPILE" = "yes" ]; then
        # to avoid using the build machine's installed packages
        set_env "" PKG_CONFIG_PATH    # registered
        export PKG_CONFIG_LIBDIR=""   # not registered
    fi
}

function check_m4_version_ok() {
    v=`m4 --version | grep -e '[0-9]*\.[0-9]*\.[0-9]*' | sed -e 's/[mM]4//g' -e 's/[^\.0-9]//g'`
    if [ "$v" = "1.4.7" -o "$v" = "1.4.11" -o "$v" = "1.4.13" ];
	then
	    return 1
    else
	    return 0
    fi
}

function inst_wget() {
    setup Wget
    _WGET_UDIR=`unix_path $WGET_DIR`
    add_to_env $_WGET_UDIR/bin PATH
    if quiet $_WGET_UDIR/wget --version || quiet wget --version
    then
        echo "already installed.  skipping."
    else
        mkdir -p $_WGET_UDIR/bin
        tar -xjpf $DOWNLOAD_UDIR/wget*.tar.bz2 -C $_WGET_UDIR
        cp $_WGET_UDIR/*/*/wget.exe $_WGET_UDIR/bin
        quiet wget --version || die "wget unavailable"
    fi
}

function inst_dtk() {
    setup MSYS DTK
    _MSYS_UDIR=`unix_path $MSYS_DIR`
    if quiet ${_MSYS_UDIR}/bin/perl --help && [ check_m4_version_ok ]
    then
    echo "msys dtk already installed.  skipping."
    else
        smart_wget $DTK_URL $DOWNLOAD_DIR
        $LAST_FILE //SP- //SILENT //DIR="$MSYS_DIR"
        for file in \
            /bin/{aclocal*,auto*,ifnames,libtool*,guile*} \
            /share/{aclocal,aclocal-1.7,autoconf,autogen,automake-1.7,guile,libtool}
        do
			[ -f $file ] || continue
            [ "${file##*.bak}" ] || continue
            _dst_file=$file.bak
            while [ -e $_dst_file ]; do _dst_file=$_dst_file.bak; done
            mv $file $_dst_file
        done
        wget_unpacked $M4_URL $DOWNLOAD_DIR $TMP_DIR
        mv $TMP_UDIR/usr/bin/m4.exe /bin
        quiet ${_MSYS_UDIR}/bin/perl --help &&
        [ check_m4_version_ok ] || die "msys dtk not installed correctly"
    fi
}

function test_for_mingw() {
    ${CC} --version
    ${LD} --help
    if [ "$CROSS_COMPILE" != "yes" ]; then
        g++ --version
        mingw32-make --help
    fi
}

function inst_mingw() {
    setup MinGW
    _MINGW_UDIR=`unix_path $MINGW_DIR`
    _MINGW_WFSDIR=`win_fs_path $MINGW_DIR`
    [ "$CROSS_COMPILE" = "yes" ] && add_to_env $_MINGW_UDIR/mingw32/bin PATH
    [ "$CROSS_COMPILE" = "yes" ] && add_to_env $_MINGW_UDIR/bin PATH

    if quiet test_for_mingw
    then
        echo "mingw already installed.  skipping."
    else
        mkdir -p $_MINGW_UDIR
        if [ "$CROSS_COMPILE" != "yes" ]; then
            wget_unpacked $BINUTILS_URL $DOWNLOAD_DIR $MINGW_DIR
            wget_unpacked $GCC_CORE_URL $DOWNLOAD_DIR $MINGW_DIR
            wget_unpacked $GCC_GPP_URL $DOWNLOAD_DIR $MINGW_DIR
            wget_unpacked $MINGW_RT_URL $DOWNLOAD_DIR $MINGW_DIR
            wget_unpacked $W32API_URL $DOWNLOAD_DIR $MINGW_DIR
            wget_unpacked $MINGW_MAKE_URL $DOWNLOAD_DIR $MINGW_DIR
            (echo "y"; echo "y"; echo "$_MINGW_WFSDIR"; echo "y") | sh pi.sh
        else
            ./create_cross_mingw.sh
        fi
        quiet test_for_mingw || die "mingw not installed correctly"
    fi
}

function inst_unzip() {
    setup Unzip
    _UNZIP_UDIR=`unix_path $UNZIP_DIR`
    add_to_env $_UNZIP_UDIR/bin PATH
    if quiet $_UNZIP_UDIR/bin/unzip --help || quiet unzip --help
    then
        echo "unzip already installed.  skipping."
    else
        smart_wget $UNZIP_URL $DOWNLOAD_DIR
        $LAST_FILE //SP- //SILENT //DIR="$UNZIP_DIR"
        quiet unzip --help || die "unzip unavailable"
    fi
}

function inst_regex() {
    setup RegEx
    _REGEX_UDIR=`unix_path $REGEX_DIR`
    add_to_env -lregex REGEX_LDFLAGS
    add_to_env -I$_REGEX_UDIR/include REGEX_CPPFLAGS
    add_to_env -L$_REGEX_UDIR/lib REGEX_LDFLAGS
    add_to_env $_REGEX_UDIR/bin PATH
    if quiet ${LD} $REGEX_LDFLAGS -o $TMP_UDIR/ofile
    then
        echo "regex already installed.  skipping."
    else
        mkdir -p $_REGEX_UDIR
        wget_unpacked $REGEX_URL $DOWNLOAD_DIR $REGEX_DIR
        wget_unpacked $REGEX_DEV_URL $DOWNLOAD_DIR $REGEX_DIR
        quiet ${LD} $REGEX_LDFLAGS -o $TMP_UDIR/ofile || die "regex not installed correctly"
    fi
}

function inst_readline() {
    setup Readline
    _READLINE_UDIR=`unix_path $READLINE_DIR`
    add_to_env -I$_READLINE_UDIR/include READLINE_CPPFLAGS
    add_to_env -L$_READLINE_UDIR/lib READLINE_LDFLAGS
    add_to_env $_READLINE_UDIR/bin PATH
    if quiet ${LD} $READLINE_LDFLAGS -lreadline -o $TMP_UDIR/ofile
    then
        echo "readline already installed.  skipping."
    else
        mkdir -p $_READLINE_UDIR
        wget_unpacked $READLINE_BIN_URL $DOWNLOAD_DIR $READLINE_DIR
        wget_unpacked $READLINE_LIB_URL $DOWNLOAD_DIR $READLINE_DIR
        quiet ${LD} $READLINE_LDFLAGS -lreadline -o $TMP_UDIR/ofile || die "readline not installed correctly"
    fi
}

function inst_active_perl() {
    setup ActivePerl \(intltool\)
    _ACTIVE_PERL_UDIR=`unix_path $ACTIVE_PERL_DIR`
    _ACTIVE_PERL_WFSDIR=`win_fs_path $ACTIVE_PERL_DIR`
    set_env_or_die $_ACTIVE_PERL_WFSDIR/ActivePerl/Perl/bin/perl INTLTOOL_PERL
    if quiet $INTLTOOL_PERL --help
    then
        echo "ActivePerl already installed.  skipping."
    else
        wget_unpacked $ACTIVE_PERL_URL $DOWNLOAD_DIR $ACTIVE_PERL_DIR
        qpushd $_ACTIVE_PERL_UDIR
            assert_one_dir ActivePerl-*
            mv ActivePerl-* ActivePerl
        qpopd
        quiet $INTLTOOL_PERL --help || die "ActivePerl not installed correctly"
    fi
}

function inst_autotools() {
    setup Autotools
    _AUTOTOOLS_UDIR=`unix_path $AUTOTOOLS_DIR`
    add_to_env $_AUTOTOOLS_UDIR/bin PATH
    add_to_env -I$_AUTOTOOLS_UDIR/include AUTOTOOLS_CPPFLAGS
    add_to_env -L$_AUTOTOOLS_UDIR/lib AUTOTOOLS_LDFLAGS
    if quiet autoconf --help && quiet automake --help
    then
        echo "autoconf/automake already installed.  skipping."
    else
        wget_unpacked $AUTOCONF_URL $DOWNLOAD_DIR $TMP_DIR
        wget_unpacked $AUTOMAKE_URL $DOWNLOAD_DIR $TMP_DIR
        assert_one_dir $TMP_UDIR/autoconf-*
        qpushd $TMP_UDIR/autoconf-*
            echo "building autoconf..."
            ./configure --prefix=$_AUTOTOOLS_UDIR
            make
            make install
        qpopd
        assert_one_dir $TMP_UDIR/automake-*
        qpushd $TMP_UDIR/automake-*
            echo "building automake..."
            ./configure --prefix=$_AUTOTOOLS_UDIR
            make
            make install
        qpopd
        quiet autoconf --help && quiet automake --help || die "autoconf/automake not installed correctly"
        rm -rf ${TMP_UDIR}/autoconf-* ${TMP_UDIR}/automake-*
    fi
    if quiet libtoolize --help
    then
        echo "libtool/libtoolize already installed.  skipping."
    else
        wget_unpacked $LIBTOOL_URL $DOWNLOAD_DIR $TMP_DIR
        assert_one_dir $TMP_UDIR/libtool-*
        qpushd $TMP_UDIR/libtool-*
            echo "building libtool..."
            ./configure ${HOST_XCOMPILE} --prefix=$_AUTOTOOLS_UDIR --disable-static
            make
            make install
        qpopd
        quiet libtoolize --help || die "libtool/libtoolize not installed correctly"
        rm -rf ${TMP_UDIR}/libtool-*
    fi
    [ ! -d $_AUTOTOOLS_UDIR/share/aclocal ] || add_to_env "-I $_AUTOTOOLS_UDIR/share/aclocal" ACLOCAL_FLAGS
}

function inst_gmp() {
    setup Gmp
    _GMP_UDIR=`unix_path ${GMP_DIR}`
    add_to_env -I$_GMP_UDIR/include GMP_CPPFLAGS
    add_to_env -L$_GMP_UDIR/lib GMP_LDFLAGS
    add_to_env ${_GMP_UDIR}/bin PATH
    if quiet ${LD} $GMP_LDFLAGS -lgmp -o $TMP_UDIR/ofile
    then
        echo "Gmp already installed. skipping."
    else
        wget_unpacked $GMP_URL $DOWNLOAD_DIR $TMP_DIR
        assert_one_dir $TMP_UDIR/gmp-*
        qpushd $TMP_UDIR/gmp-*
            ./configure ${HOST_XCOMPILE} \
                ABI=$GMP_ABI \
                --prefix=${_GMP_UDIR} \
                --disable-static --enable-shared 
            make
#            [ "$CROSS_COMPILE" != "yes" ] && make check
            make install
        qpopd
        quiet ${LD} $GMP_LDFLAGS -lgmp -o $TMP_UDIR/ofile || die "Gmp not installed correctly"
        rm -rf ${TMP_UDIR}/gmp-*
    fi
}

function inst_guile() {
    setup Guile
    _GUILE_WFSDIR=`win_fs_path $GUILE_DIR`
    _GUILE_UDIR=`unix_path $GUILE_DIR`
    add_to_env -I$_GUILE_UDIR/include GUILE_CPPFLAGS
    add_to_env -L$_GUILE_UDIR/lib GUILE_LDFLAGS
    add_to_env $_GUILE_UDIR/bin PATH
    if quiet guile -c '(use-modules (srfi srfi-39))' &&
        quiet guile -c "(use-modules (ice-9 slib)) (require 'printf)"
    then
        echo "guile and slib already installed.  skipping."
    else
        smart_wget $GUILE_URL $DOWNLOAD_DIR
        _GUILE_BALL=$LAST_FILE
        smart_wget $SLIB_URL $DOWNLOAD_DIR
        _SLIB_BALL=$LAST_FILE
        tar -xzpf $_GUILE_BALL -C $TMP_UDIR
        assert_one_dir $TMP_UDIR/guile-*
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
            ./configure ${HOST_XCOMPILE} \
                --disable-static \
                --disable-elisp \
                --disable-networking \
                --disable-dependency-tracking \
                --disable-libtool-lock \
                --disable-linuxthreads \
                -C --prefix=$_GUILE_WFSDIR \
                ac_cv_func_regcomp_rx=yes \
                CPPFLAGS="${READLINE_CPPFLAGS} ${REGEX_CPPFLAGS}" \
                LDFLAGS="-lwsock32 ${READLINE_LDFLAGS} ${REGEX_LDFLAGS}"
            cp config.status config.status.bak
            cat config.status.bak | sed 's# fileblocks[$.A-Za-z]*,#,#' > config.status
            ./config.status
            qpushd guile-config
              cp Makefile Makefile.bak
              cat Makefile.bak | sed '/-bindir-/s,:,^,g' > Makefile
            qpopd
            make LDFLAGS="-lwsock32 ${READLINE_LDFLAGS} ${REGEX_LDFLAGS} -no-undefined -avoid-version"
            make install
        qpopd
        _SLIB_DIR=$_GUILE_UDIR/share/guile/1.*
        unzip $_SLIB_BALL -d $_SLIB_DIR
        qpushd $_SLIB_DIR/slib
            cp guile.init guile.init.bak
            sed '/lambda.*'"'"'unix/a\
(define software-type (lambda () '"'"'ms-dos))' guile.init.bak > guile.init
        qpopd
        guile -c '(use-modules (srfi srfi-39))' &&
        guile -c "(use-modules (ice-9 slib)) (require 'printf)" || die "guile not installed correctly"

        # If this libguile is used from MSVC compiler, we must
        # deactivate some macros of scmconfig.h again.
        SCMCONFIG_H=$_GUILE_UDIR/include/libguile/scmconfig.h
        cat >> ${SCMCONFIG_H} <<EOF

#ifdef _MSC_VER
# undef HAVE_STDINT_H
# undef HAVE_INTTYPES_H
# undef HAVE_UNISTD_H
#endif
EOF
        # Also, for MSVC compiler we need to create an import library
        if [ x"$(which pexports.exe > /dev/null 2>&1)" != x ]
        then
            pexports $_GUILE_UDIR/bin/libguile.dll > $_GUILE_UDIR/lib/libguile.def
            ${DLLTOOL} -d $_GUILE_UDIR/lib/libguile.def -D $_GUILE_UDIR/bin/libguile.dll -l $_GUILE_UDIR/lib/libguile.lib
        fi
        # Also, for MSVC compiler we need to slightly modify the gc.h header
        GC_H=$_GUILE_UDIR/include/libguile/gc.h
        grep -v 'extern .*_freelist2;' ${GC_H} > ${GC_H}.tmp
        grep -v 'extern int scm_block_gc;' ${GC_H}.tmp > ${GC_H}
        cat >> ${GC_H} <<EOF
#ifdef _MSC_VER
# define LIBGUILEDECL __declspec (dllimport)
#else
# define LIBGUILEDECL /* */
#endif
extern LIBGUILEDECL SCM scm_freelist2;
extern LIBGUILEDECL struct scm_t_freelist scm_master_freelist2;
extern LIBGUILEDECL int scm_block_gc;
EOF
        rm -rf ${TMP_UDIR}/guile-*
    fi
    if [ "$CROSS_COMPILE" = "yes" ]; then
        mkdir -p $_GUILE_UDIR/bin
        qpushd $_GUILE_UDIR/bin
        # The cross-compiling guile expects these program names
        # for the build-time guile
        ln -sf /usr/bin/guile-config mingw32-guile-config
        ln -sf /usr/bin/guile mingw32-build-guile
        qpopd
    fi
    [ ! -d $_GUILE_UDIR/share/aclocal ] || add_to_env "-I $_GUILE_UDIR/share/aclocal" ACLOCAL_FLAGS
}

function inst_svn() {
    setup Subversion
    _SVN_UDIR=`unix_path $SVN_DIR`
    add_to_env $_SVN_UDIR/bin PATH
    if quiet svn --version
    then
        echo "subversion already installed.  skipping."
    else
		smart_wget $SVN_URL $DOWNLOAD_DIR
		$LAST_FILE //SP- //SILENT //DIR="$SVN_DIR"
        quiet svn --version || die "svn not installed correctly"
    fi
}

function inst_openssl() {
    setup OpenSSL
    _OPENSSL_UDIR=`unix_path $OPENSSL_DIR`
    add_to_env $_OPENSSL_UDIR/bin PATH
    # Make sure the files of Win32OpenSSL-0_9_8d are really gone!
    if test -f $_OPENSSL_UDIR/unins000.exe ; then
        die "Wrong version of OpenSSL installed! Run $_OPENSSL_UDIR/unins000.exe and start install.sh again."
    fi
    # Make sure the files of openssl-0.9.7c-{bin,lib}.zip are really gone!
    if [ -f $_OPENSSL_UDIR/lib/libcrypto.dll.a ] ; then
        die "Found old OpenSSL installation in $_OPENSSL_UDIR.  Please remove that first."
    fi

    if quiet ${LD} -L$_OPENSSL_UDIR/lib -leay32 -lssl32 -o $TMP_UDIR/ofile ; then
        echo "openssl already installed.  skipping."
    else
        smart_wget $OPENSSL_URL $DOWNLOAD_DIR
        echo -n "Extracting ${LAST_FILE##*/} ... "
        tar -xzpf $LAST_FILE -C $TMP_UDIR &>/dev/null | true
        echo "done"
        assert_one_dir $TMP_UDIR/openssl-*
        qpushd $TMP_UDIR/openssl-*
            for _dir in crypto ssl ; do
                qpushd $_dir
                    find . -name "*.h" -exec cp {} ../include/openssl/ \;
                qpopd
            done
            cp *.h include/openssl
            _COMSPEC_U=`unix_path $COMSPEC`
            PATH=$_ACTIVE_PERL_UDIR/ActivePerl/Perl/bin:$_MINGW_UDIR/bin $_COMSPEC_U //c ms\\mingw32
            mkdir -p $_OPENSSL_UDIR/bin
            mkdir -p $_OPENSSL_UDIR/lib
            mkdir -p $_OPENSSL_UDIR/include
            cp -a libeay32.dll libssl32.dll $_OPENSSL_UDIR/bin
            cp -a libssl32.dll $_OPENSSL_UDIR/bin/ssleay32.dll
            for _implib in libeay32 libssl32 ; do
                cp -a out/$_implib.a $_OPENSSL_UDIR/lib/$_implib.dll.a
            done
            cp -a include/openssl $_OPENSSL_UDIR/include
        qpopd
        quiet ${LD} -L$_OPENSSL_UDIR/lib -leay32 -lssl32 -o $TMP_UDIR/ofile || die "openssl not installed correctly"
        rm -rf ${TMP_UDIR}/openssl-*
    fi
    _eay32dll=$(echo $(which libeay32.dll))  # which sucks
    if [ -z "$_eay32dll" ] ; then
        die "Did not find libeay32.dll in your PATH, why that?"
    fi
    if [ "$_eay32dll" != "$_OPENSSL_UDIR/bin/libeay32.dll" ] ; then
        die "Found $_eay32dll in PATH.  If you have added $_OPENSSL_UDIR/bin to your PATH before, make sure it is listed before paths from other packages shipping SSL libraries, like SVN.  In particular, check $_MINGW_UDIR/etc/profile.d/installer.sh."
    fi
}

function inst_mingwutils() {
    setup MinGW-Utils
    _MINGW_UTILS_UDIR=`unix_path $MINGW_UTILS_DIR`
    add_to_env $_MINGW_UTILS_UDIR/bin PATH
    if quiet which pexports && quiet which reimp
    then
        echo "mingw-utils already installed.  skipping."
    else
        wget_unpacked $MINGW_UTILS_URL $DOWNLOAD_DIR $MINGW_UTILS_DIR
        (quiet which pexports && quiet which reimp) || die "mingw-utils not installed correctly"
    fi
}

function inst_exetype() {
    setup exetype
    _EXETYPE_UDIR=`unix_path $EXETYPE_DIR`
    add_to_env $_EXETYPE_UDIR/bin PATH
    if quiet which exetype
    then
        echo "exetype already installed.  skipping."
    else
        mkdir -p $_EXETYPE_UDIR/bin
        cp $EXETYPE_SCRIPT $_EXETYPE_UDIR/bin/exetype
        chmod +x $_EXETYPE_UDIR/bin/exetype
        quiet which exetype || die "exetype unavailable"
    fi
}

function inst_libxslt() {
    setup LibXSLT
    _LIBXSLT_UDIR=`unix_path $LIBXSLT_DIR`
    add_to_env $_LIBXSLT_UDIR/bin PATH
    if quiet which xsltproc
    then
        echo "libxslt already installed.  skipping."
    else
        [ "$CROSS_COMPILE" = "yes" ] && die "xsltproc unavailable"
        wget_unpacked $LIBXSLT_URL $DOWNLOAD_DIR $LIBXSLT_DIR
        wget_unpacked $LIBXSLT_LIBXML2_URL $DOWNLOAD_DIR $LIBXSLT_DIR
        qpushd $_LIBXSLT_UDIR
            mv libxslt-* mydir
            cp -r mydir/* .
            rm -rf mydir
            mv libxml2-* mydir
            cp -r mydir/* .
            rm -rf mydir
        qpopd
        quiet which xsltproc || die "libxslt not installed correctly"
    fi
}

function inst_gnome() {
    setup Gnome platform
    _GNOME_UDIR=`unix_path $GNOME_DIR`
    add_to_env -I$_GNOME_UDIR/include GNOME_CPPFLAGS
    add_to_env -L$_GNOME_UDIR/lib GNOME_LDFLAGS
    add_to_env $_GNOME_UDIR/bin PATH
    add_to_env $_GNOME_UDIR/lib/pkgconfig PKG_CONFIG_PATH
    if [ "$CROSS_COMPILE" != "yes" ]; then
        add_to_env $_GNOME_UDIR/bin/pkg-config-msys.sh PKG_CONFIG
    else
        add_to_env pkg-config PKG_CONFIG
    fi
    if quiet gconftool-2 --version &&
        quiet ${PKG_CONFIG} --exists gconf-2.0 libgnome-2.0 libgnomeui-2.0 libgtkhtml-3.14 &&
        quiet intltoolize --version
    then
        echo "gnome packages installed.  skipping."
    else
        mkdir -p $_GNOME_UDIR
        wget_unpacked $LIBXML2_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $LIBXML2_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $GETTEXT_RUNTIME_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $GETTEXT_RUNTIME_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $GETTEXT_TOOLS_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $LIBICONV_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $GLIB_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $GLIB_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $LIBJPEG_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $LIBJPEG_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $LIBPNG_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $LIBPNG_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $LIBTIFF_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $LIBTIFF_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $ZLIB_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $ZLIB_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $PKG_CONFIG_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $PKG_CONFIG_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $CAIRO_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $CAIRO_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $EXPAT_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $EXPAT_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
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
        wget_unpacked $GTKHTML_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $GTKHTML_DEV_URL $DOWNLOAD_DIR $GNOME_DIR
        wget_unpacked $GTK_DOC_URL $DOWNLOAD_DIR $TMP_DIR
        qpushd $_GNOME_UDIR
            assert_one_dir $TMP_UDIR/gtk-doc-*
            mv $TMP_UDIR/gtk-doc-*/gtk-doc.m4 $_GNOME_UDIR/share/aclocal
            if [ ! -f libexec/gconfd-2.console.exe ]; then
                cp libexec/gconfd-2.exe libexec/gconfd-2.console.exe
            fi
            exetype libexec/gconfd-2.exe windows
            for file in bin/intltool-*; do
                sed '1s,!.*perl,!'"$INTLTOOL_PERL"',;s,/opt/gnu/bin/iconv,iconv,' $file > tmp
                mv tmp $file
            done
            # work around a bug in msys bash, adding 0x01 smilies
            cat > bin/pkg-config-msys.sh <<EOF
#!/bin/sh
PKG_CONFIG="\$(dirname \$0)/pkg-config"
if \${PKG_CONFIG} "\$@" > /dev/null 2>&1 ; then
    res=true
else
    res=false
fi
\${PKG_CONFIG} "\$@" | tr -d \\\\r && \$res
EOF
            chmod +x bin/pkg-config{.exe,-msys.sh}
            sed '/Requires/s,\(.*\) enchant\(.*\) iso-codes\(.*\),\1\2\3,' lib/pkgconfig/libgtkhtml-3.14.pc > tmp
            mv tmp lib/pkgconfig/libgtkhtml-3.14.pc
            rm -rf $TMP_UDIR/gtk-doc-*
        qpopd
        wget_unpacked $PIXMAN_URL $DOWNLOAD_DIR $TMP_DIR
        assert_one_dir $TMP_UDIR/pixman-*
        qpushd $TMP_UDIR/pixman-*
            ./configure ${HOST_XCOMPILE} \
                --prefix=$_GNOME_UDIR \
                --disable-static
            make
            make install
        qpopd
        rm -rf $TMP_UDIR/pixman-*
        ${PKG_CONFIG} --exists pixman-1 || die "pixman not installed correctly"
        quiet gconftool-2 --version &&
        quiet ${PKG_CONFIG} --exists gconf-2.0 libgnome-2.0 libgnomeui-2.0 libgtkhtml-3.14 &&
        quiet intltoolize --version || die "gnome not installed correctly"
    fi
    if [ "$CROSS_COMPILE" = "yes" ]; then
        qpushd $_GNOME_UDIR/lib/pkgconfig
            perl -pi.bak -e"s!^prefix=.*\$!prefix=$_GNOME_UDIR!" *.pc
            #perl -pi.bak -e's!^Libs: !Libs: -L\${prefix}/bin !' *.pc
        qpopd
    fi
    [ ! -d $_GNOME_UDIR/share/aclocal ] || add_to_env "-I $_GNOME_UDIR/share/aclocal" ACLOCAL_FLAGS
}

function inst_swig() {
    setup Swig
    _SWIG_UDIR=`unix_path $SWIG_DIR`
    add_to_env $_SWIG_UDIR PATH
    if quiet swig -version
    then
        echo "swig already installed.  skipping."
    else
        wget_unpacked $SWIG_URL $DOWNLOAD_DIR $SWIG_DIR
        qpushd $SWIG_DIR
            mv swigwin-* mydir
            mv mydir/* .
            mv mydir/.[A-Za-z]* . # hidden files
            rmdir mydir
            rm INSTALL # bites with /bin/install
        qpopd
        quiet swig -version || die "swig unavailable"
    fi
}

function inst_pcre() {
    setup pcre
    _PCRE_UDIR=`unix_path $PCRE_DIR`
    add_to_env -I$_PCRE_UDIR/include PCRE_CPPFLAGS
    add_to_env -L$_PCRE_UDIR/lib PCRE_LDFLAGS
    add_to_env $_PCRE_UDIR/bin PATH
    if quiet ${LD} $PCRE_LDFLAGS -lpcre -o $TMP_UDIR/ofile
    then
        echo "pcre already installed.  skipping."
    else
        mkdir -p $_PCRE_UDIR
        wget_unpacked $PCRE_BIN_URL $DOWNLOAD_DIR $PCRE_DIR
        wget_unpacked $PCRE_LIB_URL $DOWNLOAD_DIR $PCRE_DIR
    fi
    quiet ${LD} $PCRE_LDFLAGS -lpcre -o $TMP_UDIR/ofile || die "pcre not installed correctly"
}

function inst_libbonoboui() {
    setup libbonoboui
    _LIBBONOBOUI_UDIR=`unix_path $LIBBONOBOUI_DIR`
    add_to_env $_LIBBONOBOUI_UDIR/bin PATH
    add_to_env $_LIBBONOBOUI_UDIR/lib/pkgconfig PKG_CONFIG_PATH
    if quiet ${PKG_CONFIG} --exists --atleast-version=2.24.2 libbonoboui-2.0 && [ -f $_LIBBONOBOUI_UDIR/bin/libbonoboui*.dll ]
    then
        echo "libbonoboui already installed.  skipping."
    else
        wget_unpacked $LIBBONOBOUI_SRC_URL $DOWNLOAD_DIR $TMP_DIR
        mydir=`pwd`
        assert_one_dir $TMP_UDIR/libbonoboui-*
        qpushd $TMP_UDIR/libbonoboui-*
            [ -n "$LIBBONOBOUI_PATCH" -a -f "$LIBBONOBOUI_PATCH" ] && \
                patch -p1 < $LIBBONOBOUI_PATCH
            #libtoolize --force
            #aclocal ${ACLOCAL_FLAGS} -I .
            #automake
            #autoconf
            ./configure ${HOST_XCOMPILE} --prefix=$_LIBBONOBOUI_UDIR \
                POPT_LIBS="-lpopt" \
                CPPFLAGS="${GNOME_CPPFLAGS}" \
                LDFLAGS="${GNOME_LDFLAGS}" \
                --enable-static=no
            make
            make install

            # We override the $GNOME_DIR libbonoboui files because
            # those erroneously depend on the obsolete libxml2.dll
            cp -a $_LIBBONOBOUI_UDIR/bin/libbonoboui*.dll $_GNOME_UDIR/bin
            cp -a $_LIBBONOBOUI_UDIR/lib/libbonoboui* $_GNOME_UDIR/lib
        qpopd
        ${PKG_CONFIG} --exists --atleast-version=2.24.2 libbonoboui-2.0 && [ -f $_LIBBONOBOUI_UDIR/bin/libbonoboui*.dll ] || die "libbonoboui not installed correctly"
        rm -rf ${TMP_UDIR}/libbonoboui-*
    fi
}

function inst_libgsf() {
    setup libGSF
    _LIBGSF_UDIR=`unix_path $LIBGSF_DIR`
    add_to_env $_LIBGSF_UDIR/bin PATH
    add_to_env $_LIBGSF_UDIR/lib/pkgconfig PKG_CONFIG_PATH
    if quiet ${PKG_CONFIG} --exists libgsf-1 libgsf-gnome-1
    then
        echo "libgsf already installed.  skipping."
    else
        wget_unpacked $LIBGSF_URL $DOWNLOAD_DIR $TMP_DIR
        assert_one_dir $TMP_UDIR/libgsf-*
        qpushd $TMP_UDIR/libgsf-*
            ./configure ${HOST_XCOMPILE} \
                --prefix=$_LIBGSF_UDIR \
                --disable-static \
                --without-python \
                CPPFLAGS="${GNOME_CPPFLAGS}" \
                LDFLAGS="${GNOME_LDFLAGS}"
            make
            make install
        qpopd
        ${PKG_CONFIG} --exists libgsf-1 libgsf-gnome-1 || die "libgsf not installed correctly"
    fi
}

function inst_goffice() {
    setup GOffice
    _GOFFICE_UDIR=`unix_path $GOFFICE_DIR`
    add_to_env $_GOFFICE_UDIR/bin PATH
    add_to_env $_GOFFICE_UDIR/lib/pkgconfig PKG_CONFIG_PATH
    if quiet ${PKG_CONFIG} --exists libgoffice-0.8
    then
        echo "goffice already installed.  skipping."
    else
        wget_unpacked $GOFFICE_URL $DOWNLOAD_DIR $TMP_DIR
        mydir=`pwd`
        assert_one_dir $TMP_UDIR/goffice-*
        qpushd $TMP_UDIR/goffice-*
            [ -n "$GOFFICE_PATCH" -a -f "$GOFFICE_PATCH" ] && \
                patch -p1 < $GOFFICE_PATCH
            libtoolize --force
            aclocal ${ACLOCAL_FLAGS} -I .
            automake
            autoconf
            ./configure ${HOST_XCOMPILE} --prefix=$_GOFFICE_UDIR \
                CPPFLAGS="${GNOME_CPPFLAGS} ${PCRE_CPPFLAGS} ${HH_CPPFLAGS}" \
                LDFLAGS="${GNOME_LDFLAGS} ${PCRE_LDFLAGS} ${HH_LDFLAGS}"
            [ -d ../libgsf-* ] || die "We need the unpacked package $TMP_UDIR/libgsf-*; please unpack it in $TMP_UDIR"
            [ -f dumpdef.pl ] || cp -p ../libgsf-*/dumpdef.pl .
            make
            make install
        qpopd
        ${PKG_CONFIG} --exists libgoffice-0.8 && [ -f $_GOFFICE_UDIR/bin/libgoffice*.dll ] || die "goffice not installed correctly"
        rm -rf ${TMP_UDIR}/goffice-*
        rm -rf ${TMP_UDIR}/libgsf-*
    fi
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
        assert_one_dir $TMP_UDIR/glade3-*
        qpushd $TMP_UDIR/glade3-*
            ./configure ${HOST_XCOMPILE} --prefix=$_GLADE_WFSDIR
            make
            make install
        qpopd
        quiet glade-3 --version || die "glade not installed correctly"
        rm -rf ${TMP_UDIR}/glade3-*
    fi
}

function inst_inno() {
    setup Inno Setup Compiler
    _INNO_UDIR=`unix_path $INNO_DIR`
    add_to_env $_INNO_UDIR PATH
    if quiet which iscc
    then
        echo "Inno Setup Compiler already installed.  skipping."
    else
        smart_wget $INNO_URL $DOWNLOAD_DIR
        $LAST_FILE //SP- //SILENT //DIR="$INNO_DIR"
        quiet which iscc || die "iscc (Inno Setup Compiler) not installed correctly"
    fi
}

function test_for_hh() {
    qpushd $TMP_UDIR
        cat > ofile.c <<EOF
#include <windows.h>
#include <htmlhelp.h>
int main(int argc, char **argv) {
  HtmlHelpW(0, (wchar_t*)"", HH_HELP_CONTEXT, 0);
  return 0;
}
EOF
        gcc -shared -o ofile.dll ofile.c $HH_CPPFLAGS $HH_LDFLAGS -lhtmlhelp || return 1
    qpopd
}

function inst_hh() {
    setup HTML Help Workshop
    _HH_UDIR=`unix_path $HH_DIR`
    add_to_env -I$_HH_UDIR/include HH_CPPFLAGS
    add_to_env -L$_HH_UDIR/lib HH_LDFLAGS
    add_to_env $_HH_UDIR PATH
    if quiet test_for_hh
    then
        echo "html help workshop already installed.  skipping."
    else
        smart_wget $HH_URL $DOWNLOAD_DIR
        echo "!!! When asked for an installation path, specify $HH_DIR !!!"
        $LAST_FILE
        qpushd $HH_DIR
           _HHCTRL_OCX=$(which hhctrl.ocx || true)
           [ "$_HHCTRL_OCX" ] || die "Did not find hhctrl.ocx"
           pexports -h include/htmlhelp.h $_HHCTRL_OCX > lib/htmlhelp.def
           qpushd lib
               ${DLLTOOL} -k -d htmlhelp.def -l libhtmlhelp.a
               mv htmlhelp.lib htmlhelp.lib.bak
           qpopd
        qpopd
        quiet test_for_hh || die "html help workshop not installed correctly"
    fi
}

function inst_opensp() {
    setup OpenSP
    _OPENSP_UDIR=`unix_path ${OPENSP_DIR}`
    add_to_env ${_OPENSP_UDIR}/bin PATH
    if test -f ${_OPENSP_UDIR}/bin/libosp-5.dll
    then
        echo "OpenSP already installed. skipping."
    else
        wget_unpacked $OPENSP_URL $DOWNLOAD_DIR $TMP_DIR
        assert_one_dir $TMP_UDIR/OpenSP-*
        qpushd $TMP_UDIR/OpenSP-*
            [ -n "$OPENSP_PATCH" -a -f "$OPENSP_PATCH" ] && \
                patch -p0 < $OPENSP_PATCH
            libtoolize --force
            aclocal ${ACLOCAL_FLAGS} -I m4
            automake
            autoconf
            ./configure ${HOST_XCOMPILE} \
                --prefix=${_OPENSP_UDIR} \
                --disable-doc-build --disable-static
            # On many windows machines, none of the programs will
            # build, but we only need the library, so ignore the rest.
            make all-am
            make -C lib
            make -i
            make -i install
        qpopd
        test -f ${_OPENSP_UDIR}/bin/libosp-5.dll || die "OpenSP not installed correctly"
    fi
}

function inst_libofx() {
    setup Libofx
    _LIBOFX_UDIR=`unix_path ${LIBOFX_DIR}`
    add_to_env ${_LIBOFX_UDIR}/bin PATH
    add_to_env ${_LIBOFX_UDIR}/lib/pkgconfig PKG_CONFIG_PATH
    if quiet ${PKG_CONFIG} --exists libofx
    then
        echo "Libofx already installed. skipping."
    else
        wget_unpacked $LIBOFX_URL $DOWNLOAD_DIR $TMP_DIR
        assert_one_dir $TMP_UDIR/libofx-*
        qpushd $TMP_UDIR/libofx-*
            [ -n "$LIBOFX_PATCH" -a -f "$LIBOFX_PATCH" ] && \
                patch -p1 < $LIBOFX_PATCH
            ./configure ${HOST_XCOMPILE} \
                --prefix=${_LIBOFX_UDIR} \
                --with-opensp-includes=${_OPENSP_UDIR}/include/OpenSP \
                --with-opensp-libs=${_OPENSP_UDIR}/lib \
                CPPFLAGS="-DOS_WIN32" \
                --disable-static
            make LDFLAGS="${LDFLAGS} -no-undefined"
            make install
        qpopd
        quiet ${PKG_CONFIG} --exists libofx || die "Libofx not installed correctly"
        rm -rf ${TMP_UDIR}/libofx-*
    fi
}

function inst_gnutls() {
    setup GNUTLS
    _GNUTLS_UDIR=`unix_path ${GNUTLS_DIR}`
    add_to_env ${_GNUTLS_UDIR}/bin PATH
    add_to_env ${_GNUTLS_UDIR}/lib/pkgconfig PKG_CONFIG_PATH
    add_to_env "-I${_GNUTLS_UDIR}/include" GNUTLS_CPPFLAGS
    add_to_env "-L${_GNUTLS_UDIR}/lib" GNUTLS_LDFLAGS
    if quiet ${PKG_CONFIG} --exists gnutls
    then
        echo "GNUTLS already installed. skipping."
    else
        wget_unpacked $GNUTLS_URL $DOWNLOAD_DIR $GNUTLS_DIR
        rm -f $_GNUTLS_UDIR/lib/*.la
        quiet ${PKG_CONFIG} --exists gnutls || die "GNUTLS not installed correctly"
    fi
    [ ! -d $_GNUTLS_UDIR/share/aclocal ] || add_to_env "-I $_GNUTLS_UDIR/share/aclocal" ACLOCAL_FLAGS
}

function inst_gwenhywfar() {
    setup Gwenhywfar
    _GWENHYWFAR_UDIR=`unix_path ${GWENHYWFAR_DIR}`
    add_to_env ${_GWENHYWFAR_UDIR}/bin PATH
    add_to_env ${_GWENHYWFAR_UDIR}/lib/pkgconfig PKG_CONFIG_PATH
    if quiet ${PKG_CONFIG} --exists gwenhywfar
    then
        echo "Gwenhywfar already installed. skipping."
    else
        wget_unpacked $GWENHYWFAR_URL $DOWNLOAD_DIR $TMP_DIR
        assert_one_dir $TMP_UDIR/gwenhywfar-*
        qpushd $TMP_UDIR/gwenhywfar-*
            # circumvent binreloc bug, http://trac.autopackage.org/ticket/28
            if [ "$AQBANKING3" != "yes" ]; then
                ./configure ${HOST_XCOMPILE} \
                    --with-openssl-includes=$_OPENSSL_UDIR/include \
                    --disable-binreloc \
                    ssl_libraries="-L${_OPENSSL_UDIR}/lib" \
                    ssl_lib="-leay32 -lssl32" \
                    --prefix=$_GWENHYWFAR_UDIR \
                    CPPFLAGS="${REGEX_CPPFLAGS} ${GNOME_CPPFLAGS}" \
                    LDFLAGS="${REGEX_LDFLAGS} ${GNOME_LDFLAGS} -lintl"
            else
                if [ -n "$GWENHYWFAR_PATCH" -a -f "$GWENHYWFAR_PATCH" ] ; then
                    patch -p1 < $GWENHYWFAR_PATCH
                    #aclocal -I m4 ${ACLOCAL_FLAGS}
                    #automake
                    #autoconf
                fi
                # Note: gwenhywfar-3.x and higher don't use openssl anymore.
                ./configure ${HOST_XCOMPILE} \
                    --with-libgcrypt-prefix=$_GNUTLS_UDIR \
                    --disable-binreloc \
                    --prefix=$_GWENHYWFAR_UDIR \
                    CPPFLAGS="${REGEX_CPPFLAGS} ${GNOME_CPPFLAGS} ${GNUTLS_CPPFLAGS}" \
                    LDFLAGS="${REGEX_LDFLAGS} ${GNOME_LDFLAGS} ${GNUTLS_LDFLAGS} -lintl"
            fi
            make
#            [ "$CROSS_COMPILE" != "yes" ] && make check
            make install
        qpopd
        ${PKG_CONFIG} --exists gwenhywfar || die "Gwenhywfar not installed correctly"
        rm -rf ${TMP_UDIR}/gwenhywfar-*
    fi
    [ ! -d $_GWENHYWFAR_UDIR/share/aclocal ] || add_to_env "-I $_GWENHYWFAR_UDIR/share/aclocal" ACLOCAL_FLAGS
}

function inst_ktoblzcheck() {
    setup Ktoblzcheck
    # Out of convenience ktoblzcheck is being installed into
    # GWENHYWFAR_DIR
    add_to_env "-I${_GWENHYWFAR_UDIR}/include" KTOBLZCHECK_CPPFLAGS
    add_to_env "-L${_GWENHYWFAR_UDIR}/lib" KTOBLZCHECK_LDFLAGS
    if quiet ${PKG_CONFIG} --exists ktoblzcheck
    then
        echo "Ktoblzcheck already installed. skipping."
    else
        wget_unpacked $KTOBLZCHECK_URL $DOWNLOAD_DIR $TMP_DIR
        assert_one_dir $TMP_UDIR/ktoblzcheck-*
        qpushd $TMP_UDIR/ktoblzcheck-*
            # circumvent binreloc bug, http://trac.autopackage.org/ticket/28
            ./configure ${HOST_XCOMPILE} \
                --prefix=${_GWENHYWFAR_UDIR} \
                --disable-binreloc \
                --disable-python
            make
#            [ "$CROSS_COMPILE" != "yes" ] && make check
            make install
        qpopd
        ${PKG_CONFIG} --exists ktoblzcheck || die "Ktoblzcheck not installed correctly"
        rm -rf ${TMP_UDIR}/ktoblzcheck-*
    fi
}

function inst_qt4() {
    # This section is not a full install, but the .la creation is
    # already useful in itself and that's why it has already been
    # added.

    [ "$QTDIR" ] || die "QTDIR is not set.  Please install Qt and set that variable in custom.sh, or deactivate AQBANKING_WITH_QT"
    export QTDIR=`unix_path ${QTDIR}`  # help configure of aqbanking
    _QTDIR=$QTDIR
    # This section creates .la files for the Qt-4 DLLs so that
    # libtool correctly links to the DLLs.
    if test ! -f ${_QTDIR}/lib/libQtCore4.la ; then
        qpushd ${_QTDIR}/lib
            for A in lib*.a; do
                LIBBASENAME=`basename ${A} .a`
                OUTFILE="${LIBBASENAME}.la"
                BASENAME=`echo ${LIBBASENAME} | sed -e"s/lib//" `
                DLLNAME="${BASENAME}.dll"

                # Create la file
                echo "# Generated by foo bar libtool" > $OUTFILE
                echo "dlname='../bin/${DLLNAME}'" >> $OUTFILE
                echo "library_names='${DLLNAME}'" >> $OUTFILE
                echo "libdir='${_QTDIR}/bin'" >> $OUTFILE
            done
        qpopd
    fi
}

function inst_aqbanking() {
    setup AqBanking
    _AQBANKING_UDIR=`unix_path ${AQBANKING_DIR}`
    add_to_env ${_AQBANKING_UDIR}/bin PATH
    add_to_env ${_AQBANKING_UDIR}/lib/pkgconfig PKG_CONFIG_PATH
    if quiet ${PKG_CONFIG} --exists aqbanking
    then
        echo "AqBanking already installed. skipping."
    else
        wget_unpacked $AQBANKING_URL $DOWNLOAD_DIR $TMP_DIR
        assert_one_dir $TMP_UDIR/aqbanking-*
        qpushd $TMP_UDIR/aqbanking-*
            _AQ_CPPFLAGS="-I${_LIBOFX_UDIR}/include ${KTOBLZCHECK_CPPFLAGS} ${GNOME_CPPFLAGS} ${GNUTLS_CPPFLAGS}"
            _AQ_LDFLAGS="-L${_LIBOFX_UDIR}/lib ${KTOBLZCHECK_LDFLAGS} ${GNOME_LDFLAGS} ${GNUTLS_LDFLAGS}"
            if test x$CROSS_COMPILE = xyes; then
                XMLMERGE="xmlmerge"
            else
                XMLMERGE="${_GWENHYWFAR_UDIR}/bin/xmlmerge"
            fi
            if test x$AQBANKING3 = xyes; then
                _AQ_BACKENDS="aqhbci aqofxconnect"
            else
                _AQ_BACKENDS="aqdtaus aqhbci aqofxconnect"
            fi
            if test x$AQBANKING_WITH_QT = xyes; then
                inst_qt4
                if [ -n "$AQBANKING_PATCH" -a -f "$AQBANKING_PATCH" ] ; then
                    patch -p1 < $AQBANKING_PATCH
                    automake
                    aclocal -I m4 ${ACLOCAL_FLAGS}
                    autoconf
                fi
                ./configure ${HOST_XCOMPILE} \
                    --with-gwen-dir=${_GWENHYWFAR_UDIR} \
                    --with-xmlmerge=${XMLMERGE} \
                    --with-frontends="cbanking qbanking" \
                    --with-backends="${_AQ_BACKENDS}" \
                    CPPFLAGS="${_AQ_CPPFLAGS} ${GMP_CPPFLAGS}" \
                    LDFLAGS="${_AQ_LDFLAGS} ${GMP_LDFLAGS}" \
                    qt3_libs="-L${_QTDIR}/lib -L${_QTDIR}/bin -lQtCore4 -lQtGui4 -lQt3Support4" \
                    qt3_includes="-I${_QTDIR}/include -I${_QTDIR}/include/Qt -I${_QTDIR}/include/QtCore -I${_QTDIR}/include/QtGui -I${_QTDIR}/include/Qt3Support" \
                    --prefix=${_AQBANKING_UDIR}
                make qt4-port
                make clean
            else
                if [ -n "$AQBANKING_PATCH" -a -f "$AQBANKING_PATCH" ] ; then
                    patch -p1 < $AQBANKING_PATCH
                    automake
                    aclocal -I m4 ${ACLOCAL_FLAGS}
                    autoconf
                fi
                ./configure ${HOST_XCOMPILE} \
                    --with-gwen-dir=${_GWENHYWFAR_UDIR} \
                    --with-xmlmerge=${XMLMERGE} \
                    --with-frontends="cbanking" \
                    --with-backends="${_AQ_BACKENDS}" \
                    CPPFLAGS="${_AQ_CPPFLAGS} ${GMP_CPPFLAGS}" \
                    LDFLAGS="${_AQ_LDFLAGS} ${GMP_LDFLAGS}" \
                    --prefix=${_AQBANKING_UDIR}
            fi
            make
            make install
        qpopd
        qpushd ${_AQBANKING_UDIR}/bin
            if [ "$AQBANKING3" != "yes" ]; then
                exetype aqbanking-tool.exe console
                exetype aqhbci-tool.exe console
            else
                exetype aqbanking-cli.exe console
                exetype aqhbci-tool4.exe console
            fi
        qpopd
        ${PKG_CONFIG} --exists aqbanking || die "AqBanking not installed correctly"
        rm -rf ${TMP_UDIR}/aqbanking-*
    fi
    [ ! -d $_AQBANKING_UDIR/share/aclocal ] || add_to_env "-I $_AQBANKING_UDIR/share/aclocal" ACLOCAL_FLAGS
}

function inst_libdbi() {
    setup LibDBI
    _SQLITE3_UDIR=`unix_path ${SQLITE3_DIR}`
    _MYSQL_LIB_UDIR=`unix_path ${MYSQL_LIB_DIR}`
    _PGSQL_UDIR=`unix_path ${PGSQL_DIR}`
    _LIBDBI_UDIR=`unix_path ${LIBDBI_DIR}`
    _LIBDBI_DRIVERS_UDIR=`unix_path ${LIBDBI_DRIVERS_DIR}`
    add_to_env -I$_LIBDBI_UDIR/include LIBDBI_CPPFLAGS
    add_to_env -L$_LIBDBI_UDIR/lib LIBDBI_LDFLAGS
    if test -f ${_SQLITE3_UDIR}/bin/libsqlite3-0.dll
    then
        echo "SQLite3 already installed.  skipping."
    else
        wget_unpacked $SQLITE3_URL $DOWNLOAD_DIR $TMP_DIR
        assert_one_dir $TMP_UDIR/sqlite-*
        qpushd $TMP_UDIR/sqlite-*
            ./configure ${HOST_XCOMPILE} \
                --prefix=${_SQLITE3_UDIR}
            make
            make install
        qpopd
        test -f ${_SQLITE3_UDIR}/bin/libsqlite3-0.dll || die "SQLite3 not installed correctly"
        rm -rf ${TMP_UDIR}/sqlite-*
    fi
    if test -f ${_MYSQL_LIB_UDIR}/lib/libmysql.dll -a \
	        -f ${_MYSQL_LIB_UDIR}/lib/libmysqlclient.a
    then
        echo "MySQL library already installed.  skipping."
    else
        wget_unpacked $MYSQL_LIB_URL $DOWNLOAD_DIR $TMP_DIR
        mkdir -p $_MYSQL_LIB_UDIR
        assert_one_dir $TMP_UDIR/mysql*
        cp -r $TMP_UDIR/mysql*/* $_MYSQL_LIB_UDIR
        mv $TMP_UDIR/mysql*/include $_MYSQL_LIB_UDIR/include/mysql
		cd $_MYSQL_LIB_UDIR/lib
		${DLLTOOL} --input-def $LIBMYSQL_DEF --dllname libmysql.dll --output-lib libmysqlclient.a -k
        test -f ${_MYSQL_LIB_UDIR}/lib/libmysql.dll || die "mysql not installed correctly - libmysql.dll"
        test -f ${_MYSQL_LIB_UDIR}/lib/libmysqlclient.a || die "mysql not installed correctly - libmysqlclient.a"
        rm -rf ${TMP_UDIR}/mysql*
    fi
    if test -f ${_PGSQL_UDIR}/lib/libpq.dll
    then
        echo "PGSQL library already installed.  skipping."
    else
        wget_unpacked $PGSQL_LIB_URL $DOWNLOAD_DIR $TMP_DIR
        mv $TMP_UDIR/pgsql* $PGSQL_DIR
        test -f ${_PGSQL_UDIR}/lib/libpq.dll || die "libpq not installed correctly"
    fi
    if test -f ${_LIBDBI_UDIR}/bin/libdbi-0.dll
    then
        echo "libdbi already installed.  skipping."
    else
        wget_unpacked $LIBDBI_URL $DOWNLOAD_DIR $TMP_DIR
        assert_one_dir $TMP_UDIR/libdbi-0*
        qpushd $TMP_UDIR/libdbi-0*
            if [ -n "$LIBDBI_PATCH" -a -f "$LIBDBI_PATCH" ]; then
                patch -p1 < $LIBDBI_PATCH
                ./autogen.sh
            fi
            if [ -n "$LIBDBI_PATCH2" -a -f "$LIBDBI_PATCH2" ]; then
                patch -p1 < $LIBDBI_PATCH2
	    fi
            ./configure ${HOST_XCOMPILE} \
                --disable-docs \
                --prefix=${_LIBDBI_UDIR}
            make
            make install
        qpopd
        qpushd ${_LIBDBI_UDIR}
            pexports bin/libdbi-0.dll > lib/libdbi.def
            ${DLLTOOL} -d lib/libdbi.def -D bin/libdbi-0.dll -l lib/libdbi.lib
        qpopd
        test -f ${_LIBDBI_UDIR}/bin/libdbi-0.dll || die "libdbi not installed correctly"
        rm -rf ${TMP_UDIR}/libdbi-0*
    fi
    if test -f ${_LIBDBI_DRIVERS_UDIR}/lib/dbd/libdbdsqlite3.dll -a \
            -f ${_LIBDBI_DRIVERS_UDIR}/lib/dbd/libdbdmysql.dll -a \
            -f ${_LIBDBI_DRIVERS_UDIR}/lib/dbd/libdbdpgsql.dll
    then
        echo "libdbi drivers already installed.  skipping."
    else
        wget_unpacked $LIBDBI_DRIVERS_URL $DOWNLOAD_DIR $TMP_DIR
        assert_one_dir $TMP_UDIR/libdbi-drivers-*
        qpushd $TMP_UDIR/libdbi-drivers*
            [ -n "$LIBDBI_DRIVERS_PATCH" -a -f "$LIBDBI_DRIVERS_PATCH" ] && \
                patch -p0 < $LIBDBI_DRIVERS_PATCH
            [ -n "$LIBDBI_DRIVERS_PATCH2" -a -f "$LIBDBI_DRIVERS_PATCH2" ] && \
                patch -p0 < $LIBDBI_DRIVERS_PATCH2
            [ -n "$LIBDBI_DRIVERS_PATCH3" -a -f "$LIBDBI_DRIVERS_PATCH3" ] && \
                patch -p0 < $LIBDBI_DRIVERS_PATCH3
            [ -n "$LIBDBI_DRIVERS_PATCH4" -a -f "$LIBDBI_DRIVERS_PATCH4" ] && \
                patch -p0 < $LIBDBI_DRIVERS_PATCH4
            LDFLAGS=-no-undefined ./configure ${HOST_XCOMPILE} \
                --disable-docs \
                --with-dbi-incdir=${_LIBDBI_UDIR}/include \
                --with-dbi-libdir=${_LIBDBI_UDIR}/lib \
                --with-sqlite3 \
                --with-sqlite3-dir=${_SQLITE3_UDIR} \
                --with-mysql \
                --with-mysql-dir=${_MYSQL_LIB_UDIR} \
                --with-pgsql \
                --with-pgsql-dir=${_PGSQL_UDIR} \
                --prefix=${_LIBDBI_DRIVERS_UDIR}
            make
            make install
        qpopd
        test -f ${_LIBDBI_DRIVERS_UDIR}/lib/dbd/libdbdsqlite3.dll || die "libdbi sqlite3 driver not installed correctly"
        test -f ${_LIBDBI_DRIVERS_UDIR}/lib/dbd/libdbdmysql.dll || die "libdbi mysql driver not installed correctly"
        test -f ${_LIBDBI_DRIVERS_UDIR}/lib/dbd/libdbdpgsql.dll || die "libdbi pgsql driver not installed correctly"
        rm -rf ${TMP_UDIR}/libdbi-drivers-*
    fi
}

function inst_cmake() {
    setup CMake
    _CMAKE_UDIR=`unix_path ${CMAKE_DIR}`
    add_to_env ${_CMAKE_UDIR}/bin PATH
    if [ -f ${_CMAKE_UDIR}/bin/cmake.exe ]
    then
        echo "cmake already installed.  skipping."
    else
        wget_unpacked $CMAKE_URL $DOWNLOAD_DIR $CMAKE_DIR

        assert_one_dir ${_CMAKE_UDIR}/cmake-2*
        mv ${_CMAKE_UDIR}/cmake-2*/* ${_CMAKE_UDIR}
        rm -rf ${_CMAKE_UDIR}/cmake-2*

        [ -f ${_CMAKE_UDIR}/bin/cmake.exe ] || die "cmake not installed correctly"
    fi
}

function inst_cutecash() {
    setup Cutecash
    _BUILD_UDIR=`unix_path $CUTECASH_BUILD_DIR`
    _REPOS_UDIR=`unix_path $REPOS_DIR`
    mkdir -p $_BUILD_UDIR

    qpushd $_BUILD_UDIR
        cmake ${_REPOS_UDIR} \
            -G"MSYS Makefiles" \
            -DREGEX_INCLUDE_PATH=${_REGEX_UDIR}/include \
            -DREGEX_LIBRARY=${_REGEX_UDIR}/lib/libregex.a \
            -DGUILE_INCLUDE_DIR=${_GUILE_UDIR}/include \
            -DGUILE_LIBRARY=${_GUILE_UDIR}/bin/libguile.dll \
            -DLIBINTL_INCLUDE_PATH=${_GNOME_UDIR}/include \
            -DLIBINTL_LIBRARY=${_GNOME_UDIR}/bin/intl.dll \
            -DLIBXML2_INCLUDE_DIR=${_GNOME_UDIR}/include/libxml2 \
            -DLIBXML2_LIBRARIES=${_GNOME_UDIR}/bin/libxml2-2.dll \
            -DPKG_CONFIG_EXECUTABLE=${_GNOME_UDIR}/bin/pkg-config \
            -DZLIB_INCLUDE_DIR=${_GNOME_UDIR}/include \
            -DZLIB_LIBRARY=${_GNOME_UDIR}/bin/zlib1.dll \
            -DSWIG_EXECUTABLE=${_SWIG_UDIR}/swig.exe \
            -DHTMLHELP_INCLUDE_PATH=${_HH_UDIR}/include \
            -DWITH_SQL=ON \
            -DLIBDBI_INCLUDE_PATH=${_LIBDBI_UDIR}/include \
            -DLIBDBI_LIBRARY=${_LIBDBI_UDIR}/lib/libdbi.dll.a \
            -DCMAKE_BUILD_TYPE=Debug
        make
    qpopd
}

function inst_webkit() {
    setup WebKit
    _WEBKIT_UDIR=`unix_path ${WEBKIT_DIR}`
    add_to_env ${_WEBKIT_UDIR}/bin PATH
    add_to_env -lwebkit-1.0-2 WEBKIT_LIBS
    add_to_env -L${_WEBKIT_UDIR}/bin WEBKIT_LIBS
    add_to_env -I${_WEBKIT_UDIR}/include/webkit-1.0 WEBKIT_CFLAGS
    if quiet ${LD} ${WEBKIT_LIBS} -o $TMP_UDIR/ofile
    then
        echo "webkit already installed.  skipping."
    else
        wget_unpacked $WEBKIT_URL $DOWNLOAD_DIR $WEBKIT_DIR
ls $WEBKIT_DIR
        quiet ${LD} ${WEBKIT_LIBS} -o $TMP_UDIR/ofile || die "webkit not installed correctly"
    fi
}

function svn_up() {
    mkdir -p $_REPOS_UDIR
    qpushd $REPOS_DIR
    if [ -x .svn ]; then
        setup "svn update in ${REPOS_DIR}"
        svn up -r ${SVN_REV}
    else
        setup svn co
        svn co -r ${SVN_REV} $REPOS_URL .
    fi
    qpopd
}

function inst_gnucash() {
    setup GnuCash
    _INSTALL_WFSDIR=`win_fs_path $INSTALL_DIR`
    _INSTALL_UDIR=`unix_path $INSTALL_DIR`
    _BUILD_UDIR=`unix_path $BUILD_DIR`
    _REL_REPOS_UDIR=`unix_path $REL_REPOS_DIR`
    mkdir -p $_BUILD_UDIR
    add_to_env $_INSTALL_UDIR/bin PATH

    AQBANKING_OPTIONS="--enable-hbci --with-aqbanking-dir=${_AQBANKING_UDIR}"
    AQBANKING_UPATH="${_OPENSSL_UDIR}/bin:${_GWENHYWFAR_UDIR}/bin:${_AQBANKING_UDIR}/bin"
    LIBOFX_OPTIONS="--enable-ofx --with-ofx-prefix=${_LIBOFX_UDIR}"

    qpushd $REPOS_DIR
        if [ "$CROSS_COMPILE" = "yes" ]; then
            # Set these variables manually because of cross-compiling
            export GUILE_LIBS="${GUILE_LDFLAGS} -lguile -lguile-ltdl"
            export GUILE_INCS="${GUILE_CPPFLAGS}"
            export BUILD_GUILE=yes
            export name_build_guile=/usr/bin/guile-config
        fi
        if [ "$BUILD_FROM_TARBALL" != "yes" ]; then
            ./autogen.sh
        fi
    qpopd

    qpushd $BUILD_DIR
        $_REL_REPOS_UDIR/configure ${HOST_XCOMPILE} \
            --prefix=$_INSTALL_WFSDIR \
            --enable-debug \
            --enable-schemas-install=no \
            --enable-dbi \
            --with-dbi-dbd-dir=${_LIBDBI_DRIVERS_UDIR}/lib/dbd \
            ${LIBOFX_OPTIONS} \
            ${AQBANKING_OPTIONS} \
            --enable-binreloc \
            --enable-locale-specific-tax \
            --with-html-engine=webkit \
            CPPFLAGS="${AUTOTOOLS_CPPFLAGS} ${REGEX_CPPFLAGS} ${GNOME_CPPFLAGS} ${GUILE_CPPFLAGS} ${LIBDBI_CPPFLAGS} ${KTOBLZCHECK_CPPFLAGS} ${HH_CPPFLAGS} -D_WIN32" \
            LDFLAGS="${AUTOTOOLS_LDFLAGS} ${REGEX_LDFLAGS} ${GNOME_LDFLAGS} ${GUILE_LDFLAGS} ${LIBDBI_LDFLAGS} ${KTOBLZCHECK_LDFLAGS} ${HH_LDFLAGS}" \
            PKG_CONFIG_PATH="${PKG_CONFIG_PATH}"

        make

        make_install
    qpopd
}

# This function will be called by make_install.sh as well,
# so do not regard variables from inst_* functions as set
# Parameters allowed: skip_scripts, skip_schemas
function make_install() {
    _BUILD_UDIR=`unix_path $BUILD_DIR`
    _INSTALL_UDIR=`unix_path $INSTALL_DIR`
    _GOFFICE_UDIR=`unix_path $GOFFICE_DIR`
    _LIBGSF_UDIR=`unix_path $LIBGSF_DIR`
    _PCRE_UDIR=`unix_path $PCRE_DIR`
    _GNOME_UDIR=`unix_path $GNOME_DIR`
    _GUILE_UDIR=`unix_path $GUILE_DIR`
    _REGEX_UDIR=`unix_path $REGEX_DIR`
    _AUTOTOOLS_UDIR=`unix_path $AUTOTOOLS_DIR`
    _OPENSSL_UDIR=`unix_path $OPENSSL_DIR`
    _GWENHYWFAR_UDIR=`unix_path ${GWENHYWFAR_DIR}`
    _AQBANKING_UDIR=`unix_path ${AQBANKING_DIR}`
    _LIBOFX_UDIR=`unix_path ${LIBOFX_DIR}`
    _OPENSP_UDIR=`unix_path ${OPENSP_DIR}`
    _LIBDBI_UDIR=`unix_path ${LIBDBI_DIR}`
    _SQLITE3_UDIR=`unix_path ${SQLITE3_DIR}`
    _WEBKIT_UDIR=`unix_path ${WEBKIT_DIR}`
    _GNUTLS_UDIR=`unix_path ${GNUTLS_DIR}`
    AQBANKING_UPATH="${_OPENSSL_UDIR}/bin:${_GWENHYWFAR_UDIR}/bin:${_AQBANKING_UDIR}/bin"
    AQBANKING_PATH="${OPENSSL_DIR}\\bin;${GWENHYWFAR_DIR}\\bin;${AQBANKING_DIR}\\bin"

    for param in "$@"; do
        [ "$param" = "skip_scripts" ] && _skip_scripts=1
        [ "$param" = "skip_schemas" ] && _skip_schemas=1
    done

    if [ -z $_skip_scripts ]; then
        # Try to fix the paths in the environment config file
        qpushd $_BUILD_UDIR/src/bin
            rm environment
            make \
                bindir="${_INSTALL_UDIR}/bin;${_INSTALL_UDIR}/lib;${_INSTALL_UDIR}/lib/gnucash;${_GNUTLS_UDIR}/bin;${_GMP_UDIR}/bin;${_GOFFICE_UDIR}/bin;${_LIBGSF_UDIR}/bin;${_PCRE_UDIR}/bin;${_GNOME_UDIR}/bin;${_GUILE_UDIR}/bin;${_WEBKIT_UDIR}/bin;${_REGEX_UDIR}/bin;${_AUTOTOOLS_UDIR}/bin;${AQBANKING_UPATH};${_LIBOFX_UDIR}/bin;${_OPENSP_UDIR}/bin;${_LIBDBI_UDIR}/bin;${_SQLITE3_UDIR}/bin;${MYSQL_LIB_DIR}/lib;${PGSQL_DIR}/lib;${PGSQL_DIR}/bin" \
                environment
        qpopd
    fi
    
    make install

    qpushd $_INSTALL_UDIR/lib
        # Move modules that are compiled without -module to lib/gnucash and
        # correct the 'dlname' in the libtool archives. We do not use these
        # files to dlopen the modules, so actually this is unneeded.
        # Also, in all installed .la files, remove the dependency_libs line
        mv bin/*.dll gnucash 2>/dev/null || true
        for A in gnucash/*.la; do
            sed '/dependency_libs/d;s#../bin/##' $A > tmp ; mv tmp $A
        done
        for A in *.la; do
            sed '/dependency_libs/d' $A > tmp ; mv tmp $A
        done
    qpopd

    if [ -z $_skip_schemas ]; then
        qpushd $_INSTALL_UDIR/etc/gconf/schemas
            for file in *.schemas; do
                gconftool-2 \
                    --config-source=xml:merged:${_INSTALL_WFSDIR}/etc/gconf/gconf.xml.defaults \
                    --install-schema-file $file >/dev/null
            done
            gconftool-2 --shutdown
        qpopd
    fi

    if [ -z $_skip_scripts ]; then
        # Create a startup script that works without the msys shell
        # If you make any changes here, you should probably also change
		# the equivalent sections in packaging/win32/gnucash.iss.in, and
		# src/bin/environment*.in
        qpushd $_INSTALL_UDIR/bin
            echo "setlocal" > gnucash.cmd
            echo "set PATH=${INSTALL_DIR}\\bin;${INSTALL_DIR}\\lib;${INSTALL_DIR}\\lib\\gnucash;${GNUTLS_DIR}\\bin;${GMP_DIR}\\bin;${GOFFICE_DIR}\\bin;${LIBGSF_DIR}\\bin;${PCRE_DIR}\\bin;${WEBKIT_DIR}\\bin;${GNOME_DIR}\\bin;${GUILE_DIR}\\bin;${REGEX_DIR}\\bin;${AUTOTOOLS_DIR}\\bin;${AQBANKING_PATH};${LIBOFX_DIR}\\bin;${OPENSP_DIR}\\bin;${LIBDBI_DIR}\\bin;${SQLITE3_DIR}\\bin;${MYSQL_LIB_DIR}\\lib;${PGSQL_DIR}\\lib;${PGSQL_DIR}\\bin;%PATH%" >> gnucash.cmd
            echo "set GUILE_WARN_DEPRECATED=no" >> gnucash.cmd
            echo "set GNC_MODULE_PATH=${INSTALL_DIR}\\lib\\gnucash" >> gnucash.cmd
            echo "set GUILE_LOAD_PATH=${INSTALL_DIR}\\share\\gnucash\\guile-modules;${INSTALL_DIR}\\share\\gnucash\\scm;%GUILE_LOAD_PATH%" >> gnucash.cmd
            echo "set LTDL_LIBRARY_PATH=${INSTALL_DIR}\\lib" >> gnucash.cmd
            echo "set GNC_DBD_DIR=${LIBDBI_DRIVERS_DIR}\\lib\\dbd" >> gnucash.cmd
			echo "set GNC_STANDARD_REPORTS_DIR=${INSTALL_DIR}\\share\\gnucash\\guile-modules\\gnucash\\report\\standard-reports" >> gnucash.cmd
            echo "set SCHEME_LIBRARY_PATH=" >> gnucash.cmd
            echo "start gnucash-bin %*" >> gnucash.cmd
        qpopd
    fi
}

function make_chm() {
    _CHM_TYPE=$1
    _CHM_LANG=$2
    _XSLTPROC_OPTS=$3
    echo "Processing $_CHM_TYPE ($_CHM_LANG) ..."
    qpushd $_CHM_TYPE/$_CHM_LANG
        xsltproc $XSLTPROCFLAGS $_XSLTPROC_OPTS ../../../docbook-xsl/htmlhelp/htmlhelp.xsl gnucash-$_CHM_TYPE.xml
        count=0
        echo >> htmlhelp.hhp
        echo "[ALIAS]" >> htmlhelp.hhp
        echo "IDH_0=index.html" >> htmlhelp.hhp
        echo "#define IDH_0 0" > mymaps
        echo "[Map]" > htmlhelp.hhmap
        echo "Searching for anchors ..."
        for id in `cat *.xml | sed '/sect.*id=/!d;s,.*id=["'\'']\([^"'\'']*\)["'\''].*,\1,'` ; do
            files=`grep -l "[\"']${id}[\"']" *.html` || continue
            echo "IDH_$((++count))=${files}#${id}" >> htmlhelp.hhp
            echo "#define IDH_${count} ${count}" >> mymaps
            echo "${id}=${count}" >> htmlhelp.hhmap
        done
        echo >> htmlhelp.hhp
        echo "[MAP]" >> htmlhelp.hhp
        cat mymaps >> htmlhelp.hhp
        rm mymaps
        hhc htmlhelp.hhp  >/dev/null  || true
        cp -fv htmlhelp.chm $_DOCS_INST_UDIR/$_CHM_LANG/gnucash-$_CHM_TYPE.chm
        cp -fv htmlhelp.hhmap $_DOCS_INST_UDIR/$_CHM_LANG/gnucash-$_CHM_TYPE.hhmap
    qpopd
}

function inst_docs() {
    _DOCS_UDIR=`unix_path $DOCS_DIR`
    if [ ! -d $_DOCS_UDIR/docbook-xsl ] ; then
        wget_unpacked $DOCBOOK_XSL_URL $DOWNLOAD_DIR $DOCS_DIR
        # add a pause to allow windows to realize that the files now exist
        sleep 1
        mv $_DOCS_UDIR/docbook-xsl-* $_DOCS_UDIR/docbook-xsl
    fi
    mkdir -p $_DOCS_UDIR/repos
    qpushd $DOCS_DIR/repos
        if [ "$UPDATE_DOCS" = "yes" ]; then
            if [ -x .svn ]; then
                setup "SVN update of docs"
                svn up -r ${DOCS_REV}
            else
                setup "SVN checkout of docs"
                svn co -r ${DOCS_REV} $DOCS_URL .
            fi
        fi
        setup docs
        _DOCS_INST_UDIR=`unix_path $INSTALL_DIR`/share/gnucash/help
        mkdir -p $_DOCS_INST_UDIR/{C,de_DE,it_IT,ja_JP}
        make_chm guide C
        make_chm guide de_DE
        make_chm guide it_IT
        make_chm guide ja_JP "--stringparam chunker.output.encoding Shift_JIS --stringparam htmlhelp.encoding Shift_JIS"
        make_chm help C
        make_chm help de_DE
#        make_chm help it_IT
    qpopd
}

function finish() {
    setup Finish...
    if [ "$NO_SAVE_PROFILE" != "yes" ]; then
        _NEW=x
        for _ENV in $ENV_VARS; do
            _ADDS=`eval echo '"\$'"${_ENV}"'_ADDS"'`
            if [ "$_ADDS" ]; then
                if [ "$_NEW" ]; then
                    echo
                    echo "Environment variables changed, please do the following"
                    echo
                    [ -d /etc/profile.d ] || echo "mkdir -p /etc/profile.d"
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
    fi
    if [ "$CROSS_COMPILE" = "yes" ]; then
        echo "You might want to create a binary tarball now as follows:"
        qpushd $GLOBAL_DIR
        echo tar -czf $HOME/gnucash-fullbin.tar.gz --anchored \
            --exclude='*.a' --exclude='*.o' --exclude='*.h' \
            --exclude='*.info' --exclude='*.html' \
            --exclude='*include/*' --exclude='*gtk-doc*' \
            --exclude='bin*' \
            --exclude='mingw32/*' --exclude='*bin/mingw32-*' \
            --exclude='gnucash-trunk*' \
            *
        qpopd
    fi
}

for step in "${steps[@]}" ; do
    eval $step
done
qpopd

echo -n "Build Finished at "
date

### Local Variables: ***
### sh-basic-offset: 4 ***
### indent-tabs-mode: nil ***
### End: ***
