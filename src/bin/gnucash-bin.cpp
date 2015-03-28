/*
 * gnucash-bin.cpp -- The program entry point for GnuCash
 *
 * Copyright (C) 2006 Chris Shoemaker <c.shoemaker@cox.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact:
 *
 * Free Software Foundation           Voice:  +1-617-542-5942
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652
 * Boston, MA  02110-1301,  USA       gnu@gnu.org
 */
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "glib.h"
#include "gnc-module.h"
#include "gnc-path.h"
#include "binreloc.h"
#include "gnc-locale-utils.h"
#include "core-utils/gnc-version.h"
#include "gnc-engine.h"
#include "gnc-filepath-utils.h"
#include "gnc-ui-util.h"
#include "gnc-file.h"
#include "gnc-hooks.h"
#include "top-level.h"
#include "gfec.h"
#include "gnc-commodity.h"
#include "gnc-main.h"
#include "gnc-main-window.h"
#include "gnc-splash.h"
#include "gnc-gnome-utils.h"
#include "gnc-plugin-file-history.h"
#include "gnc-gconf-utils.h"
#include "dialog-new-user.h"
#include "gnc-session.h"
#include "engine-helpers.h"

/* This static indicates the debugging module that this .o belongs to.  */
static QofLogModule log_module = GNC_MOD_GUI;

#include <libintl.h>
#include <locale.h>

/* GNUCASH_SCM is defined whenever we're building from an svn/svk/git/bzr tree */
#ifdef GNUCASH_SCM
static int is_development_version = TRUE;
#else
static int is_development_version = FALSE;
#endif

/* Command-line option variables */
static int          gnucash_show_version = 0;
static int          debugging        = 0;
static int          extra            = 0;
static gchar      **log_flags        = NULL;
static gchar       *log_to_filename  = NULL;
static int          nofile           = 0;
static const gchar *gconf_path       = NULL;
static const char  *add_quotes_file  = NULL;
static char        *namespace_regexp = NULL;
static const char  *file_to_load     = NULL;
static gchar      **args_remaining   = NULL;

static GOptionEntry options[] =
{
    {
        "version", 'v', 0, G_OPTION_ARG_NONE, &gnucash_show_version,
        N_("Show GnuCash version"), NULL
    },

    {
        "debug", '\0', 0, G_OPTION_ARG_NONE, &debugging,
        N_("Enable debugging mode: increasing logging to provide deep detail."), NULL
    },

    {
        "extra", '\0', 0, G_OPTION_ARG_NONE, &extra,
        N_("Enable extra/development/debugging features."), NULL
    },

    {
        "log", '\0', 0, G_OPTION_ARG_STRING_ARRAY, &log_flags,
        N_("Log level overrides, of the form \"log.ger.path={debug,info,warn,crit,error}\""),
        NULL
    },

    {
        "logto", '\0', 0, G_OPTION_ARG_STRING, &log_to_filename,
        N_("File to log into; defaults to \"/tmp/gnucash.trace\"; can be \"stderr\" or \"stdout\"."),
        NULL
    },

    {
        "nofile", '\0', 0, G_OPTION_ARG_NONE, &nofile,
        N_("Do not load the last file opened"), NULL
    },
    {
        "gconf-path", '\0', 0, G_OPTION_ARG_STRING, &gconf_path,
        N_("Set the prefix path for gconf queries"),
        /* Translators: Argument description for autohelp; see
           http://developer.gnome.org/doc/API/2.0/glib/glib-Commandline-option-parser.html */
        N_("GCONFPATH")
    },
    {
        "add-price-quotes", '\0', 0, G_OPTION_ARG_STRING, &add_quotes_file,
        N_("Add price quotes to given GnuCash datafile"),
        /* Translators: Argument description for autohelp; see
           http://developer.gnome.org/doc/API/2.0/glib/glib-Commandline-option-parser.html */
        N_("FILE")
    },
    {
        "namespace", '\0', 0, G_OPTION_ARG_STRING, &namespace_regexp,
        N_("Regular expression determining which namespace commodities will be retrieved"),
        /* Translators: Argument description for autohelp; see
           http://developer.gnome.org/doc/API/2.0/glib/glib-Commandline-option-parser.html */
        N_("REGEXP")
    },
    {
        G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &args_remaining, NULL, N_("[datafile]") },
    { NULL }
};

static void
gnc_print_unstable_message(void)
{
    if (!is_development_version) return;

    g_print("\n\n%s\n%s\n%s\n%s\n",
            _("This is a development version. It may or may not work."),
            _("Report bugs and other problems to gnucash-devel@gnucash.org"),
            _("You can also lookup and file bug reports at http://bugzilla.gnome.org"),
            _("To find the last stable version, please refer to http://www.gnucash.org"));
}

static gchar  *environment_expand(gchar *param)
{
    gchar *search_start;
    gchar *opening_brace;
    gchar *closing_brace;
    gchar *result;
    gchar *tmp;
    gchar *expanded = NULL;

    if (!param)
        return NULL;

    /* Set an initial return value, so we can always use g_strconcat below) */
    result = g_strdup ("x");

    /* Look for matching pairs of { and }. Anything in between should be expanded */
    search_start = param;
    opening_brace = g_strstr_len (search_start, -1, "{");
    closing_brace = g_strstr_len (search_start, -1, "}");

    /* Note: the test on valid braces is fairly simple:
     *       * if no pair of opening/closing braces is found, no expansion occurs
     *       * braces can't be nested, this will give unexpected results
     *       * the string should contain no other braces than those used to mark
     *         expandable variables, or unexpected results will be returned.
     */
    while ( opening_brace && closing_brace && (closing_brace > opening_brace) )
    {
        /* Found a first matching pair */
        gchar *to_expand;
        const gchar *env_val;

        /* If the string had characters before the opening {, copy them first */
        if (opening_brace > search_start)
        {
            gchar *prefix = g_strndup (search_start, opening_brace - search_start);

            tmp = g_strconcat (result, prefix, NULL);
            g_free (result);
            result = tmp;
            g_free (prefix);
        }

        /* Expand the variable  we found and append it to the result */
        to_expand = g_strndup (opening_brace + 1, closing_brace - opening_brace - 1);
        env_val = g_getenv (to_expand);
        tmp = g_strconcat (result, env_val, NULL);
        g_free (result);
        result = tmp;
        g_free (to_expand);

        /* Look for matching pairs of { and }. Anything in between should be expanded */
        search_start = closing_brace + 1;
        opening_brace = g_strstr_len (search_start, -1, "{");
        closing_brace = g_strstr_len (search_start, -1, "}");
    }

    /* No more braces found, append the remaining characters */
    tmp = g_strconcat (result, search_start, NULL);
    g_free (result);
    result = tmp;

    /* Remove the "x" from our result */
    if (g_strcmp0 (result, "x"))
        expanded = g_strdup (result + 1);
    g_free (result);

    return expanded;
}

static void
environment_override()
{
    gchar *config_path;
    gchar *env_file;
    GKeyFile    *keyfile = g_key_file_new();
    GError      *error;
    gchar **env_vars;
    gsize param_count;
    gint i;
    gboolean got_keyfile;
    gchar *env_parm, *bin_parm;

    /* Export default parameters to the environment */
    env_parm = gnc_path_get_prefix();
    if (!g_setenv("GNC_HOME", env_parm, FALSE))
        g_warning ("Couldn't set/override environment variable GNC_HOME.");
    bin_parm = g_build_filename(env_parm, "bin", NULL);
    if (!g_setenv("GNC_BIN", bin_parm, FALSE))
        g_warning ("Couldn't set/override environment variable GNC_BIN.");
    g_free (env_parm);
    g_free (bin_parm);
    env_parm = gnc_path_get_pkglibdir();
    if (!g_setenv("GNC_LIB", env_parm, FALSE))
        g_warning ("Couldn't set/override environment variable GNC_LIB.");
    g_free (env_parm);
    env_parm = gnc_path_get_pkgdatadir();
    if (!g_setenv("GNC_DATA", env_parm, FALSE))
        g_warning ("Couldn't set/override environment variable GNC_DATA.");
    g_free (env_parm);
    env_parm = gnc_path_get_pkgsysconfdir();
    if (!g_setenv("GNC_CONF", env_parm, FALSE))
        g_warning ("Couldn't set/override environment variable GNC_CONF.");
    g_free (env_parm);
    env_parm = gnc_path_get_libdir();
    if (!g_setenv("SYS_LIB", env_parm, FALSE))
        g_warning ("Couldn't set/override environment variable SYS_LIB.");
    g_free (env_parm);

    config_path = gnc_path_get_pkgsysconfdir();

    env_file = g_build_filename (config_path, "environment", NULL);
    got_keyfile = g_key_file_load_from_file (keyfile, env_file, G_KEY_FILE_NONE, &error);
    g_free (config_path);
    g_free (env_file);
    if ( !got_keyfile )
    {
        g_key_file_free(keyfile);
        return;
    }

    /* Read the environment overrides and apply them */
    env_vars = g_key_file_get_keys(keyfile, "Variables", &param_count, &error);
    for ( i = 0; i < param_count; i++ )
    {
        gchar **val_list;
        gsize val_count;
        gint j;
        gchar *new_val = NULL, *tmp_val;

        /* For each variable, read its new value, optionally expand it and set/unset it */
        val_list = g_key_file_get_string_list (keyfile, "Variables",
                                               env_vars[i], &val_count,
                                               &error );
        if ( val_count == 0 )
            g_unsetenv (env_vars[i]);
        else
        {
            /* Set an initial return value, so we can always use g_build_path below) */
            tmp_val = g_strdup ("x");
            for ( j = 0; j < val_count; j++ )
            {
                gchar *expanded = environment_expand (val_list[j]);
                new_val = g_build_path (G_SEARCHPATH_SEPARATOR_S, tmp_val, expanded, NULL);
                g_free (tmp_val);
                g_free(expanded);
                tmp_val = new_val;
            }
            g_strfreev (val_list);

            /* Remove the "x" from our result */
            if (g_strcmp0 (tmp_val, "x"))
                new_val = g_strdup (tmp_val + sizeof (G_SEARCHPATH_SEPARATOR_S));
            g_free (tmp_val);

            if (!g_setenv (env_vars[i], new_val, TRUE))
                g_warning ("Couldn't properly override environment variable \"%s\". "
                           "This may lead to unexpected results", env_vars[i]);
            g_free(new_val);
        }
    }

    g_strfreev(env_vars);
    g_key_file_free(keyfile);
}

static gboolean
try_load_config_array(const gchar *fns[])
{
    gchar *filename;
    int i;

    for (i = 0; fns[i]; i++)
    {
        filename = gnc_build_dotgnucash_path(fns[i]);
        if (gfec_try_load(filename))
        {
            g_free(filename);
            return TRUE;
        }
        g_free(filename);
    }
    return FALSE;
}

static void
update_message(const gchar *msg)
{
    gnc_update_splash_screen(msg, GNC_SPLASH_PERCENTAGE_UNKNOWN);
    g_message("%s", msg);
}

static void
load_system_config(void)
{
    static int is_system_config_loaded = FALSE;
    gchar *system_config_dir;
    gchar *system_config;

    if (is_system_config_loaded) return;

    update_message("loading system configuration");
    system_config_dir = gnc_path_get_pkgsysconfdir();
    system_config = g_build_filename(system_config_dir, "config", NULL);
    is_system_config_loaded = gfec_try_load(system_config);
    g_free(system_config_dir);
    g_free(system_config);
}

static void
load_user_config(void)
{
    /* Don't continue adding to this list. When 2.0 rolls around bump
       the 1.4 (unnumbered) files off the list. */
    static const gchar *user_config_files[] =
    {
        "config-2.0.user", "config-1.8.user", "config-1.6.user",
        "config.user", NULL
    };
    static const gchar *auto_config_files[] =
    {
        "config-2.0.auto", "config-1.8.auto", "config-1.6.auto",
        "config.auto", NULL
    };
    static const gchar *saved_report_files[] =
    {
        "saved-reports-2.4", "saved-reports-2.0", NULL
    };
    static const gchar *stylesheet_files[] = { "stylesheets-2.0", NULL};
    static int is_user_config_loaded = FALSE;

    if (is_user_config_loaded)
        return;
    else is_user_config_loaded = TRUE;

    update_message("loading user configuration");
    try_load_config_array(user_config_files);
    update_message("loading auto configuration");
    try_load_config_array(auto_config_files);
    update_message("loading saved reports");
    try_load_config_array(saved_report_files);
    update_message("loading stylesheets");
    try_load_config_array(stylesheet_files);
}

/* Parse command line options, using GOption interface.
 * We can't let gtk_init_with_args do it because it fails
 * before parsing any arguments if the GUI can't be initialized.
 */
static void
gnc_parse_command_line(int *argc, char ***argv)
{

    GError *error = NULL;
    GOptionContext *context = g_option_context_new (_("- GnuCash personal and small business finance management"));

    g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
    g_option_context_add_group (context, gtk_get_option_group(FALSE));
    if (!g_option_context_parse (context, argc, argv, &error))
    {
        g_printerr (_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
                    error->message, *argv[0]);
        g_error_free (error);
        exit (1);
    }
    g_option_context_free (context);

#ifndef GNUCASH_SCM
#define GNUCASH_SCM "unknown-scm"
#endif
#ifndef GNUCASH_SCM_REV
#define GNUCASH_SCM_REV "unknown-rev"
#endif

    if (gnucash_show_version)
    {
        gchar *fixed_message;

        if (is_development_version)
        {
            fixed_message = g_strdup_printf(_("GnuCash %s development version"), VERSION);

            /* Translators: 1st %s is a fixed message, which is translated independently;
                            2nd %s is the scm type (svn/svk/git/bzr);
                            3rd %s is the scm revision number;
                            4th %s is the build date */
            g_print ( _("%s\nThis copy was built from %s rev %s on %s."),
                      fixed_message, GNUCASH_SCM, GNUCASH_SCM_REV,
                      GNUCASH_BUILD_DATE );
        }
        else
        {
            fixed_message = g_strdup_printf(_("GnuCash %s"), VERSION);

            /* Translators: 1st %s is a fixed message, which is translated independently;
                            2nd %s is the scm (svn/svk/git/bzr) revision number;
                            3rd %s is the build date */
            g_print ( _("%s\nThis copy was built from rev %s on %s."),
                      fixed_message, GNUCASH_SCM_REV, GNUCASH_BUILD_DATE );
        }
        g_print("\n");
        g_free (fixed_message);
        exit(0);
    }

    gnc_set_debugging(debugging);
    gnc_set_extra(extra);

    if (!gconf_path)
    {
        const char *path = g_getenv("GNC_GCONF_PATH");
        if (path)
            gconf_path = path;
        else
            gconf_path = GCONF_PATH;
    }
    gnc_set_gconf_path(g_strdup(gconf_path));

    if (namespace_regexp)
        gnc_main_set_namespace_regexp(namespace_regexp);

    if (args_remaining)
        file_to_load = args_remaining[0];
}

static void
load_gnucash_modules()
{
    int i, len;
    struct
    {
        gchar * name;
        int version;
        gboolean optional;
    } modules[] =
    {
        { "gnucash/app-utils", 0, FALSE },
        { "gnucash/engine", 0, FALSE },
        { "gnucash/register/ledger-core", 0, FALSE },
        { "gnucash/register/register-core", 0, FALSE },
        { "gnucash/register/register-gnome", 0, FALSE },
        { "gnucash/import-export/qif-import", 0, FALSE },
        { "gnucash/import-export/ofx", 0, TRUE },
        { "gnucash/import-export/csv-import", 0, TRUE },
        { "gnucash/import-export/csv-export", 0, TRUE },
        { "gnucash/import-export/log-replay", 0, TRUE },
        { "gnucash/import-export/aqbanking", 0, TRUE },
        { "gnucash/report/report-system", 0, FALSE },
        { "gnucash/report/stylesheets", 0, FALSE },
        { "gnucash/report/standard-reports", 0, FALSE },
        { "gnucash/report/utility-reports", 0, FALSE },
        { "gnucash/report/locale-specific/us", 0, FALSE },
        { "gnucash/report/report-gnome", 0, FALSE },
        { "gnucash/business-gnome", 0, TRUE },
        { "gnucash/gtkmm", 0, TRUE },
        { "gnucash/python", 0, TRUE },
    };

    /* module initializations go here */
    len = sizeof(modules) / sizeof(*modules);
    for (i = 0; i < len; i++)
    {
        DEBUG("Loading module %s started", modules[i].name);
        gnc_update_splash_screen(modules[i].name, GNC_SPLASH_PERCENTAGE_UNKNOWN);
        if (modules[i].optional)
            gnc_module_load_optional(modules[i].name, modules[i].version);
        else
            gnc_module_load(modules[i].name, modules[i].version);
        DEBUG("Loading module %s finished", modules[i].name);
    }
    if (!gnc_engine_is_initialized())
    {
        /* On Windows this check used to fail anyway, see
         * https://lists.gnucash.org/pipermail/gnucash-devel/2006-September/018529.html
         * but more recently it seems to work as expected
         * again. 2006-12-20, cstim. */
        g_warning("GnuCash engine failed to initialize.  Exiting.\n");
        exit(1);
    }
}

static char *
get_file_to_load()
{
    if (file_to_load)
        return g_strdup(file_to_load);
    else
        return gnc_history_get_last();
}

static void
inner_main (void *closure, int argc, char **argv)
{
    char* fn;
    GError *error = NULL;

    load_gnucash_modules();

    /* Load the config before starting up the gui. This insures that
     * custom reports have been read into memory before the Reports
     * menu is created. */
    load_system_config();
    load_user_config();

    /* TODO: After some more guile-extraction, this should happen even
       before booting guile.  */
    gnc_main_gui_init();

    gnc_hook_add_dangler(HOOK_UI_SHUTDOWN, (GFunc)gnc_file_quit, NULL);

    /* Install Price Quote Sources */
//    gnc_update_splash_screen(_("Checking Finance::Quote..."), GNC_SPLASH_PERCENTAGE_UNKNOWN);

    gnc_hook_run(HOOK_STARTUP, NULL);

    if (!nofile && (fn = get_file_to_load()))
    {
        gnc_update_splash_screen(_("Loading data..."), GNC_SPLASH_PERCENTAGE_UNKNOWN);
        gnc_file_open_file(fn, /*open_readonly*/ FALSE);
        g_free(fn);
    }
    else if (gnc_gconf_get_bool("dialogs/new_user", "first_startup", &error)
             && !error)
    {
        gnc_destroy_splash_screen();
        gnc_ui_new_user_dialog();
    }

    gnc_destroy_splash_screen();
    gnc_main_window_show_all_windows();

    gnc_hook_run(HOOK_UI_POST_STARTUP, NULL);
    gnc_ui_start_event_loop();
    gnc_hook_remove_dangler(HOOK_UI_SHUTDOWN, (GFunc)gnc_file_quit);

    gnc_shutdown(0);
    return;
}

static void
gnc_log_init()
{
    if (log_to_filename != NULL)
    {
        qof_log_init_filename_special(log_to_filename);
    }
    else
    {
        /* initialize logging to our file. */
        gchar *tracefilename;
        tracefilename = g_build_filename(g_get_tmp_dir(), "gnucash.trace",
                                         (gchar *)NULL);
        qof_log_init_filename(tracefilename);
        g_free(tracefilename);
    }

    // set a reasonable default.
    qof_log_set_default(QOF_LOG_WARNING);

    gnc_log_default();

    if (gnc_is_debugging())
    {
        qof_log_set_level("", QOF_LOG_INFO);
        qof_log_set_level("qof", QOF_LOG_INFO);
        qof_log_set_level("gnc", QOF_LOG_INFO);
    }

    {
        gchar *log_config_filename;
        log_config_filename = gnc_build_dotgnucash_path("log.conf");
        if (g_file_test(log_config_filename, G_FILE_TEST_EXISTS))
            qof_log_parse_log_config(log_config_filename);
        g_free(log_config_filename);
    }

    if (log_flags != NULL)
    {
        int i = 0;
        for (; log_flags[i] != NULL; i++)
        {
            QofLogLevel level;
            gchar **parts = NULL;

            gchar *log_opt = log_flags[i];
            parts = g_strsplit(log_opt, "=", 2);
            if (parts == NULL || parts[0] == NULL || parts[1] == NULL)
            {
                g_warning("string [%s] not parseable", log_opt);
                continue;
            }

            level = qof_log_level_from_string(parts[1]);
            qof_log_set_level(parts[0], level);
            g_strfreev(parts);
        }
    }
}

int
main(int argc, char ** argv)
{
#if !defined(G_THREADS_ENABLED) || defined(G_THREADS_IMPL_NONE)
#    error "No GLib thread implementation available!"
#endif
#ifdef ENABLE_BINRELOC
    {
        GError *binreloc_error = NULL;
        if (!gnc_gbr_init(&binreloc_error))
        {
            g_print("main: Error on gnc_gbr_init: %s\n", binreloc_error->message);
            g_error_free(binreloc_error);
        }
    }
#endif

    /* This should be called before gettext is initialized
     * The user may have configured a different language via
     * the environment file.
     */
    environment_override();

    {
        gchar *localedir = gnc_path_get_localedir();
        bindtextdomain(GETTEXT_PACKAGE, localedir);
        textdomain(GETTEXT_PACKAGE);
        bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
        g_free(localedir);
    }
    
    gnc_parse_command_line(&argc, &argv);
    gnc_print_unstable_message();

    gnc_log_init();
    gnc_module_system_init();

    /* No quotes fetching was asked - attempt to initialize the gui */
    gnc_gtk_add_rc_file ();
    if(!gtk_init_check (&argc, &argv))
    {
        g_printerr(_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
                   _("Error: could not initialize graphical user interface and option add-price-quotes was not set."),
                   argv[0]);
        return 1;
    }
    gnc_gui_init();
//    scm_boot_guile(argc, argv, inner_main, 0);
    inner_main(NULL, argc, argv);
    exit(0); /* never reached */
}