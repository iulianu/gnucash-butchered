/*********************************************************************
 * gncmod-report-gnome.c
 * module definition/initialization for the gnome report infrastructure 
 * 
 * Copyright (c) 2001 Linux Developers Group, Inc. 
 *********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <stdio.h>
#include <libguile.h>

#include "gnc-module.h"
#include "gnc-module-api.h"

#include "window-report.h"

/* version of the gnc module system interface we require */
int libgncmod_report_gnome_LTX_gnc_module_system_interface = 0;

/* module versioning uses libtool semantics. */
int libgncmod_report_gnome_LTX_gnc_module_current  = 0;
int libgncmod_report_gnome_LTX_gnc_module_revision = 0;
int libgncmod_report_gnome_LTX_gnc_module_age      = 0;

/* forward references */
char *libgncmod_report_gnome_LTX_gnc_module_path(void);
char *libgncmod_report_gnome_LTX_gnc_module_description(void);
int libgncmod_report_gnome_LTX_gnc_module_init(int refcount);
int libgncmod_report_gnome_LTX_gnc_module_end(int refcount);


char *
libgncmod_report_gnome_LTX_gnc_module_path(void) {
  return g_strdup("gnucash/report/report-gnome");
}

char * 
libgncmod_report_gnome_LTX_gnc_module_description(void) {
  return g_strdup("Gnome component of GnuCash report generation system");
}

static void
lmod(char * mn)
{
  char * form = g_strdup_printf("(use-modules %s)\n", mn);
  scm_c_eval_string(form);
  g_free(form);
}

int
libgncmod_report_gnome_LTX_gnc_module_init(int refcount) {
  if(!gnc_module_load("gnucash/app-utils", 0)) {
    return FALSE;
  }

  if(!gnc_module_load("gnucash/gnome-utils", 0)) {
    return FALSE;
  }

  if(!gnc_module_load("gnucash/report/report-system", 0)) {
    return FALSE;
  }

  lmod ("(g-wrapped gw-report-gnome)");
  lmod ("(gnucash report report-gnome)");

  if (refcount == 0)
    gnc_report_init ();

  return TRUE;
}

int
libgncmod_report_gnome_LTX_gnc_module_end(int refcount) {
  return TRUE;
}
