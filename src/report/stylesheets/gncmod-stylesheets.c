/*********************************************************************
 * gncmod-stylesheets.c
 * module definition/initialization for the standard reports 
 * 
 * Copyright (c) 2001 Linux Developers Group, Inc. 
 *********************************************************************/

#include "config.h"
#include <stdio.h>
#include <glib.h>
#include <libguile.h>

#include "gnc-module.h"
#include "gnc-module-api.h"
#include "gnc-plugin-stylesheets.h"

/* version of the gnc module system interface we require */
int libgncmod_stylesheets_LTX_gnc_module_system_interface = 0;

/* module versioning uses libtool semantics. */
int libgncmod_stylesheets_LTX_gnc_module_current  = 0;
int libgncmod_stylesheets_LTX_gnc_module_revision = 0;
int libgncmod_stylesheets_LTX_gnc_module_age      = 0;

/* forward references */
char *libgncmod_stylesheets_LTX_gnc_module_path(void);
char *libgncmod_stylesheets_LTX_gnc_module_description(void);
int libgncmod_stylesheets_LTX_gnc_module_init(int refcount);
int libgncmod_stylesheets_LTX_gnc_module_end(int refcount);


char *
libgncmod_stylesheets_LTX_gnc_module_path(void) {
  return g_strdup("gnucash/report/stylesheets");
}

char * 
libgncmod_stylesheets_LTX_gnc_module_description(void) {
  return g_strdup("Standard report stylesheet definitions");
}

int
libgncmod_stylesheets_LTX_gnc_module_init(int refcount) {
  /* load the report system */
  if(!gnc_module_load("gnucash/report/report-system", 0)) {
    return FALSE;
  }

  /* load the report generation scheme code */
  if(scm_c_eval_string("(use-modules (gnucash report stylesheets))") ==
     SCM_BOOL_F) {
    return FALSE;
  }
  
  /* Add menu items with C callbacks */
  gnc_plugin_stylesheets_create_plugin();
  
  return TRUE;
}

int
libgncmod_stylesheets_LTX_gnc_module_end(int refcount) {
  return TRUE;
}
