/* gnc-mod-baz.c : the Gnucash plugin that wraps the library
 * 'libbaz.so'. it does this by being linked against libbaz.so */

#include "config.h"
#include <stdio.h>
#include <libguile.h>
#include "guile-mappings.h"

#include "gnc-module.h"
#include "gnc-module-api.h"
#include "baz-gwrap.h"

int libgncmodbaz_LTX_gnc_module_system_interface = 0;

int libgncmodbaz_LTX_gnc_module_current = 0;
int libgncmodbaz_LTX_gnc_module_age = 0;
int libgncmodbaz_LTX_gnc_module_revision = 0;

/* forward references */
char *libgncmodbaz_LTX_gnc_module_path(void);
char *libgncmodbaz_LTX_gnc_module_description(void);
int libgncmodbaz_LTX_gnc_module_init(int refcount);

char * 
libgncmodbaz_LTX_gnc_module_path(void) {
  return g_strdup("gnucash/baz");
}

char * 
libgncmodbaz_LTX_gnc_module_description(void) {
  return g_strdup("this is the baz module");
}

int
libgncmodbaz_LTX_gnc_module_init(int refcount) {
  /* load libfoo */
  if(gnc_module_load("gnucash/foo", 0)) {
    /* publish the g-wrapped Scheme bindings for libbaz */
    gw_init_wrapset_baz_gwrap();
    
    /* use the scheme module */
    scm_c_eval_string("(use-modules (gnucash baz))");

    return TRUE;
  }
  else {
    return FALSE;
  }
}

