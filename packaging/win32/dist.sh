#!/bin/sh

set -e

function qpushd() { pushd "$@" >/dev/null; }
function qpopd() { popd >/dev/null; }
function unix_path() { echo "$*" | sed 's,^\([A-Za-z]\):,/\1,;s,\\,/,g'; }

qpushd "$(dirname $(unix_path "$0"))"
. functions.sh
. defaults.sh

register_env_var PATH ":"

function prepare() {
    # this directory is hardcoded in gnucash.iss.in
    DIST_DIR=${INSTALL_DIR}\\..\\dist
    DIST_UDIR=`unix_path $DIST_DIR`
    DIST_WFSDIR=`win_fs_path $DIST_DIR`
    TMP_UDIR=`unix_path $TMP_DIR`
    if [ -x $DIST_DIR ]; then
        die "Please remove ${DIST_DIR} first"
    fi
    _UNZIP_UDIR=`unix_path $UNZIP_DIR`
    _AUTOTOOLS_UDIR=`unix_path $AUTOTOOLS_DIR`
    _GUILE_UDIR=`unix_path $GUILE_DIR`
    _WIN_UDIR=`unix_path $WINDIR`
    _EXETYPE_UDIR=`unix_path $EXETYPE_DIR`
    _GNOME_UDIR=`unix_path $GNOME_DIR`
    _PCRE_UDIR=`unix_path $PCRE_DIR`
    _LIBGSF_UDIR=`unix_path $LIBGSF_DIR`
    _GOFFICE_UDIR=`unix_path $GOFFICE_DIR`
    _OPENSP_UDIR=`unix_path $OPENSP_DIR`
    _LIBOFX_UDIR=`unix_path $LIBOFX_DIR`
    _GWENHYWFAR_UDIR=`unix_path $GWENHYWFAR_DIR`
    _AQBANKING_UDIR=`unix_path $AQBANKING_DIR`
    _GNUCASH_UDIR=`unix_path $GNUCASH_DIR`
    _REPOS_UDIR=`unix_path $REPOS_DIR`
    _BUILD_UDIR=`unix_path $BUILD_DIR`
    _INSTALL_UDIR=`unix_path $INSTALL_DIR`
    _INNO_UDIR=`unix_path $INNO_DIR`
    add_to_env $_UNZIP_UDIR/bin PATH # unzip
    add_to_env $_GNOME_UDIR/bin PATH # gconftool-2
    add_to_env $_EXETYPE_UDIR/bin PATH # exetype
}

function dist_regex() {
    setup RegEx
    smart_wget $REGEX_URL $DOWNLOAD_DIR
    unzip -q $LAST_FILE bin/libgnurx-0.dll -d $DIST_DIR
}

function dist_autotools() {
    setup Autotools
    mkdir -p $DIST_UDIR/bin
    cp -a $_AUTOTOOLS_UDIR/bin/*.dll $DIST_UDIR/bin
}

function dist_guile() {
    setup Guile
    mkdir -p $DIST_UDIR/bin
    cp -a $_GUILE_UDIR/bin/libguile{.,-ltdl.,-srfi}*dll $DIST_UDIR/bin
    cp -a $_GUILE_UDIR/bin/guile.exe $DIST_UDIR/bin
    mkdir -p $DIST_UDIR/share
    cp -a $_GUILE_UDIR/share/guile $DIST_UDIR/share
    [ -f $DIST_UDIR/share/guile/1.6/slibcat ] && rm $DIST_UDIR/share/guile/1.6/slibcat
}

function dist_openssl() {
    setup OpenSSL
    _OPENSSL_UDIR=`unix_path $OPENSSL_DIR`
    mkdir -p $DIST_UDIR/bin
    cp -a $_OPENSSL_UDIR/bin/*.dll $DIST_UDIR/bin
}

function dist_gnome() {
    setup Gnome platform
    wget_unpacked $LIBXML2_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $GETTEXT_URL $DOWNLOAD_DIR $DIST_DIR
    smart_wget $LIBICONV_URL $DOWNLOAD_DIR
    unzip -q $LAST_FILE bin/iconv.dll -d $DIST_DIR
    wget_unpacked $GLIB_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $LIBJPEG_URL $DOWNLOAD_DIR $DIST_DIR
    smart_wget $LIBPNG_URL $DOWNLOAD_DIR
    unzip -q $LAST_FILE bin/libpng13.dll -d $DIST_DIR
    smart_wget $ZLIB_URL $DOWNLOAD_DIR
    unzip -q $LAST_FILE zlib1.dll -d $DIST_DIR\\bin
    wget_unpacked $CAIRO_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $EXPAT_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $FONTCONFIG_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $FREETYPE_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $ATK_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $PANGO_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $LIBART_LGPL_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $GTK_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $ORBIT2_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $GAIL_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $POPT_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $GCONF_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $LIBBONOBO_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $GNOME_VFS_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $LIBGNOME_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $LIBGNOMECANVAS_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $LIBBONOBOUI_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $LIBGNOMEUI_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $LIBGLADE_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $LIBGNOMEPRINT_URL $DOWNLOAD_DIR $DIST_DIR
    wget_unpacked $LIBGNOMEPRINTUI_URL $DOWNLOAD_DIR $DIST_DIR  # gnomeprint
    wget_unpacked $GTKHTML_URL $DOWNLOAD_DIR $DIST_DIR
    rm -rf $DIST_UDIR/etc/gconf/gconf.xml.defaults/{desktop,schemas}
}

function dist_pcre() {
    setup pcre
    mkdir -p $DIST_UDIR/bin
    cp -a $_PCRE_UDIR/bin/pcre3.dll $DIST_UDIR/bin
}

function dist_libgsf() {
    setup libGSF
    mkdir -p $DIST_UDIR/bin
    cp -a $_LIBGSF_UDIR/bin/libgsf*.dll $DIST_UDIR/bin
    mkdir -p $DIST_UDIR/etc/gconf/schemas
    cp -a $_LIBGSF_UDIR/etc/gconf/schemas/* $DIST_UDIR/etc/gconf/schemas
    mkdir -p $DIST_UDIR/lib
    cp -a $_LIBGSF_UDIR/lib/locale $DIST_UDIR/lib
}

function dist_goffice() {
    setup GOffice
    mkdir -p $DIST_UDIR/bin
    cp -a $_GOFFICE_UDIR/bin/libgoffice*.dll $DIST_UDIR/bin
    mkdir -p $DIST_UDIR/lib
    cp -a $_GOFFICE_UDIR/lib/{goffice,locale} $DIST_UDIR/lib
    mkdir -p $DIST_UDIR/share
    cp -a $_GOFFICE_UDIR/share/{goffice,pixmaps} $DIST_UDIR/share
}

function dist_libofx() {
    setup OpenSP and LibOFX
    cp -a ${_OPENSP_UDIR}/bin/*.dll ${DIST_UDIR}/bin
    cp -a ${_OPENSP_UDIR}/share/OpenSP ${DIST_UDIR}/share
    cp -a ${_LIBOFX_UDIR}/bin/*.dll ${DIST_UDIR}/bin
    cp -a ${_LIBOFX_UDIR}/bin/*.exe ${DIST_UDIR}/bin
    cp -a ${_LIBOFX_UDIR}/share/libofx ${DIST_UDIR}/share
}

function dist_gwenhywfar() {
    setup gwenhywfar
    cp -a ${_GWENHYWFAR_UDIR}/bin/*.dll ${DIST_UDIR}/bin
    mkdir -p ${DIST_UDIR}/etc
    cp -a ${_GWENHYWFAR_UDIR}/etc/* ${DIST_UDIR}/etc
    cp -a ${_GWENHYWFAR_UDIR}/lib/gwenhywfar ${DIST_UDIR}/lib
}

function dist_ktoblzcheck() {
    setup ktoblzcheck
    # dll is already copied in dist_gwenhywfar
    cp -a ${_GWENHYWFAR_UDIR}/share/ktoblzcheck ${DIST_UDIR}/share
}

function dist_aqbanking() {
    setup aqbanking
    cp -a ${_AQBANKING_UDIR}/bin/*.exe ${DIST_UDIR}/bin
    cp -a ${_AQBANKING_UDIR}/bin/*.dll ${DIST_UDIR}/bin
    cp -a ${_AQBANKING_UDIR}/lib/aqbanking ${DIST_UDIR}/lib
    cp -a ${_AQBANKING_UDIR}/share/aqbanking ${DIST_UDIR}/share
    cp -a ${_AQBANKING_UDIR}/share/aqhbci ${DIST_UDIR}/share
    cp -a ${_AQBANKING_UDIR}/share/locale ${DIST_UDIR}/lib
}

function dist_gnucash() {
    setup GnuCash
    mkdir -p $DIST_UDIR/bin
    cp -a $_INSTALL_UDIR/bin/* $DIST_UDIR/bin
    mkdir -p $DIST_UDIR/etc/gconf/schemas
    cp -a $_INSTALL_UDIR/etc/gconf/schemas/* $DIST_UDIR/etc/gconf/schemas
    mkdir -p $DIST_UDIR/lib
    cp -a $_INSTALL_UDIR/lib/locale $DIST_UDIR/lib
    cp -a $_INSTALL_UDIR/lib/lib*.la $DIST_UDIR/lib
    mkdir -p $DIST_UDIR/lib/gnucash
    cp -a $_INSTALL_UDIR/lib/gnucash/lib*.dll $DIST_UDIR/lib/gnucash
    cp -a $_INSTALL_UDIR/libexec $DIST_UDIR
    mkdir -p $DIST_UDIR/share
    cp -a $_INSTALL_UDIR/share/{gnucash,pixmaps,xml} $DIST_UDIR/share
    cp -a $_REPOS_UDIR/packaging/win32/install-fq-mods.bat $DIST_UDIR/bin
    cp -a $_BUILD_UDIR/packaging/win32/gnucash.iss $_GNUCASH_UDIR
}

function finish() {
    for file in $DIST_UDIR/etc/gconf/schemas/*.schemas; do
        echo -n "Installing $file ... "
        gconftool-2 \
            --config-source=xml:merged:${DIST_WFSDIR}/etc/gconf/gconf.xml.defaults \
            --install-schema-file $file >/dev/null
        echo "done"
    done
    gconftool-2 --shutdown

    mv $DIST_UDIR/libexec/gconfd-2.exe $DIST_UDIR/bin
    exetype $DIST_UDIR/bin/gconfd-2.exe windows
    cp $_BUILD_UDIR/packaging/win32/redirect.exe $DIST_UDIR/libexec/gconfd-2.exe

    if [ "$AQBANKING_WITH_QT" = "yes" ]; then
        mv ${DIST_UDIR}/lib/aqbanking/plugins/16/wizards/qt3-wizard.exe $DIST_UDIR/bin
        cp $_BUILD_UDIR/packaging/win32/redirect.exe $DIST_UDIR/lib/aqbanking/plugins/16/wizards/qt3-wizard.exe
    fi

    # Strip redirections in distributed libtool .la files
    for file in $DIST_UDIR/lib/*.la; do
        cat $file | sed 's,^libdir=,#libdir=,' > $file.new
        mv $file.new $file
    done

    echo "Now running the Inno Setup Compiler for creating the setup.exe"
    ${_INNO_UDIR}/iscc ${_GNUCASH_UDIR}/gnucash.iss

    if [ "$BUILD_FROM_TARBALL" = "no" ]; then
        # And changing output filename
        PKG_VERSION=`grep PACKAGE_VERSION ${_BUILD_UDIR}/config.h | cut -d" " -f3 | cut -d\" -f2 `
        SVN_REV=`grep GNUCASH_SVN_REV ${_BUILD_UDIR}/src/gnome-utils/gnc-svninfo.h | cut -d" " -f3 | cut -d\" -f2 `
        SETUP_FILENAME="gnucash-${PKG_VERSION}-svn-r${SVN_REV}-setup.exe"
        qpushd ${_GNUCASH_UDIR}
            mv gnucash-${PKG_VERSION}-setup.exe ${SETUP_FILENAME}
        qpopd
        echo "Final resulting Setup program is:"
        echo ${_GNUCASH_UDIR}/${SETUP_FILENAME}
    fi
}

prepare
dist_regex
dist_autotools
dist_guile
dist_openssl
dist_gnome
dist_pcre
dist_libgsf
dist_goffice
dist_libofx
dist_gwenhywfar
dist_aqbanking
dist_gnucash
finish
qpopd


### Local Variables: ***
### sh-basic-offset: 4 ***
### indent-tabs-mode: nil ***
### End: ***
