#include <stdio.h>
#include <stdlib.h>
#include <libguile.h>

#include "gnc-module.h"

static void
guile_main (void *closure, int argc, char ** argv)
{
  GNCModule engine;

  printf("  test-load-engine.c: loading/unloading engine module ... ");

  gnc_module_system_init();
  engine = gnc_module_load("gnucash/engine", 0);
  
  if(!engine) {
    printf("  Failed to load engine\n");
    exit(-1);
  }
  
  if(!gnc_module_unload(engine)) {
    printf("  Failed to unload engine\n");
    exit(-1);
  }
  printf(" successful.\n");

  exit(0);
}

int
main (int argc, char ** argv)
{
  scm_boot_guile(argc, argv, guile_main, NULL);
  return 0;
}
