
#include <stdio.h>
#include <stdlib.h>
#include <libguile.h>

#include "gnc-module.h"

static void
guile_main(void *closure, int argc, char ** argv)
{
  int       (*foo_hello)(void);
  int helloval; 
  GNCModule foo;

  printf("  test-load-c.c: testing module load/unload from C ... ");

  gnc_module_system_init();

  foo = gnc_module_load("gnucash/foo", 0);
  
  if(!foo) {
    printf("  Failed to load foo\n");
    exit(-1);
  }
  
  foo_hello = gnc_module_lookup(foo, "foo_hello");
  helloval = foo_hello();
  if(helloval != 10) {
    printf("  Call of module function failed.\n");
    exit(-1);
  }

  if(!gnc_module_unload(foo)) {
    printf("  Failed to unload foo\n");
    exit(-1);
  }
  printf(" successful.\n");

  exit(0);
}

int
main(int argc, char ** argv)
{
  scm_boot_guile(argc, argv, guile_main, NULL);
  return 0;
}
