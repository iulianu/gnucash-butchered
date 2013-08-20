#include <stdlib.h>
#include <gnc-module.h>

int
main(int argc, char ** argv)
{
    GNCModule mod;
    g_setenv ("GNC_UNINSTALLED", "1", TRUE);
    gnc_module_system_init();
    mod = gnc_module_load("gnucash/gnome-utils", 0);

    exit(mod == NULL);
}
