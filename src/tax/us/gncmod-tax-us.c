/*********************************************************************
 * gncmod-tax-us.c
 * module definition/initialization for us tax info 
 * 
 * Copyright (c) 2001 Linux Developers Group, Inc. 
 *********************************************************************/

#include "config.h"
#include <stdio.h>
#include <glib.h>
#include <libguile.h>
#include <locale.h>
#include <string.h>

#include "gnc-module.h"
#include "gnc-module-api.h"
#include "guile-mappings.h"

/* version of the gnc module system interface we require */
int libgncmod_tax_us_LTX_gnc_module_system_interface = 0;

/* module versioning uses libtool semantics. */
int libgncmod_tax_us_LTX_gnc_module_current  = 0;
int libgncmod_tax_us_LTX_gnc_module_revision = 0;
int libgncmod_tax_us_LTX_gnc_module_age      = 0;

/* forward references */
char *libgncmod_tax_us_LTX_gnc_module_path(void);
char *libgncmod_tax_us_LTX_gnc_module_description(void);
int libgncmod_tax_us_LTX_gnc_module_init(int refcount);
int libgncmod_tax_us_LTX_gnc_module_end(int refcount);


char *
libgncmod_tax_us_LTX_gnc_module_path(void) {
  const char *thislocale = setlocale(LC_ALL, NULL);
  if (strncmp(thislocale, "de_DE", 5) == 0)
    return g_strdup("gnucash/tax/de_DE");
  else
    return g_strdup("gnucash/tax/us");
}

char * 
libgncmod_tax_us_LTX_gnc_module_description(void) {
  return g_strdup("US income tax information");
}

static void
lmod(char * mn) 
{
  char * form = g_strdup_printf("(use-modules %s)\n", mn);
  scm_c_eval_string(form);
  g_free(form);
}

int
libgncmod_tax_us_LTX_gnc_module_init(int refcount) {
  /* This is a very simple hack that loads the (new, special) German
     tax definition file in a German locale, or (default) loads the
     previous US tax file. */
  const char *thislocale = setlocale(LC_ALL, NULL);
  if (strncmp(thislocale, "de_DE", 5) == 0)
    lmod("(gnucash tax de_DE)");
  else
    lmod("(gnucash tax us)");
  return TRUE;
}

int
libgncmod_tax_us_LTX_gnc_module_end(int refcount) {
  return TRUE;
}
