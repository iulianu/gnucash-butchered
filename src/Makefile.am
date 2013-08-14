# These directories do not contain any gtk dependencies
NONGUI_SUBDIRS = \
  libqof \
  test-core \
  debug \
  doc \
  pixmaps \
  core-utils \
  gnc-module \
  engine \
  backend \
  quotes 

# These directories should also not contain any gtk dependencies, but
# currently there is only single very little dependency in
# app-utils. It should go away in the long run, though.
ALMOST_NONGUI_SUBDIRS = \
  app-utils \
  ${PYTHON_DIR}

# These directories contain the code with gtk dependency
GUI_SUBDIRS = \
  gnome-utils \
  html \
  gnome-search \
  register \
  gnome \
  import-export \
  business \
  plugins \
  bin

# Need to include '.' in order to build swig-runtime.h
if GNUCASH_ENABLE_GUI
SUBDIRS = . $(NONGUI_SUBDIRS) $(ALMOST_NONGUI_SUBDIRS) $(GUI_SUBDIRS)
else
SUBDIRS = . $(NONGUI_SUBDIRS) $(ALMOST_NONGUI_SUBDIRS) 
endif

noinst_HEADERS =

noinst_DATA = 
#gnc-test-env

MAINTAINERCLEANFILES = 

EXTRA_DIST = \
  README.modules \
  gnc-test-env \
  platform.h
