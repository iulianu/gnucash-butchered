#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unittest-support.h>

#include "gnc-module.h"

int
main(int argc, char ** argv)
{
    GNCModule foo;
    gchar *msg1 = "Module '../../../src/gnc-module/test/misc-mods/.libs/libgncmod_futuremodsys.so' requires newer module system\n";
    gchar *msg2 = "Could not locate module gnucash/incompatdep interface v.0";
    gchar *logdomain = "gnc.module";
    guint loglevel = G_LOG_LEVEL_WARNING;
    TestErrorStruct check1 = { loglevel, logdomain, msg1 };
    TestErrorStruct check2 = { loglevel, logdomain, msg2 };
    test_add_error (&check1);
    test_add_error (&check2);
    g_log_set_handler (logdomain, loglevel,
                       (GLogFunc)test_list_handler, NULL);

    g_test_message("  test-incompatdep.cpp:  loading a module with bad deps ...\n");

    gnc_module_system_init();

    foo = gnc_module_load("gnucash/incompatdep", 0);

    if (!foo)
    {
        printf("  ok\n");
        exit(0);
    }
    else
    {
        printf("  oops! loaded incompatible module\n");
        exit(-1);
    }
    return 0;
}
