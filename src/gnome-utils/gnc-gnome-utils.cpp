/********************************************************************\
 * gnc-gnome-utils.cpp -- utility functions for gnome for GnuCash   *
 * Copyright (C) 2001 Linux Developers Group                        *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/

#include "config.h"

#include <glib/gi18n.h>
#include <gconf/gconf.h>
#include <X11/Xlib.h>
#include <libxml/xmlIO.h>

#include "assistant-gconf-setup.h"
#include "gnc-gconf-utils.h"
#include "gnc-gnome-utils.h"
#include "gnc-engine.h"
#include "gnc-path.h"
#include "gnc-ui.h"
#include "gnc-file.h"
#include "gnc-hooks.h"
#include "gnc-filepath-utils.h"
#include "gnc-menu-extensions.h"
#include "gnc-component-manager.h"
#include "gnc-splash.h"
#include "gnc-window.h"
#include "gnc-icons.h"
#include "dialog-options.h"
#include "dialog-commodity.h"
#include "dialog-totd.h"
#include "gnc-ui-util.h"
#include "gnc-session.h"
#include "qofbookslots.h"

static QofLogModule log_module = GNC_MOD_GUI;
static int gnome_is_running = FALSE;
static int gnome_is_terminating = FALSE;
static int gnome_is_initialized = FALSE;


#define ACCEL_MAP_NAME "accelerator-map"


void
gnc_options_dialog_set_new_book_option_values (GNCOptionDB *odb)
{
    GNCOption *num_source_option;
    GtkWidget *num_source_is_split_action_button;
    gboolean num_source_is_split_action;

    if (!odb) return;
    num_source_is_split_action = gnc_gconf_get_bool(GCONF_GENERAL,
                                                    KEY_NUM_SOURCE,
                                                    NULL);
    if (num_source_is_split_action)
    {
//        num_source_option = gnc_option_db_get_option_by_name(odb,
//                                                 OPTION_SECTION_ACCOUNTS,
//                                                 OPTION_NAME_NUM_FIELD_SOURCE);
//        num_source_is_split_action_button =
//                                gnc_option_get_gtk_widget (num_source_option);
//        gtk_toggle_button_set_active
//                    (GTK_TOGGLE_BUTTON (num_source_is_split_action_button),
//                        num_source_is_split_action);
    }
}

/* gnc_configure_date_format
 *    sets dateFormat to the current value on the scheme side
 *
 * Args: Nothing
 * Returns: Nothing
 */
static void
gnc_configure_date_format (void)
{
    char *format_code = gnc_gconf_get_string(GCONF_GENERAL,
                        KEY_DATE_FORMAT, NULL);

    QofDateFormat df;

    if (format_code == NULL)
        format_code = g_strdup("locale");
    if (*format_code == '\0')
    {
        g_free(format_code);
        format_code = g_strdup("locale");
    }

    if (gnc_date_string_to_dateformat(format_code, &df))
    {
        PERR("Incorrect date format code");
        if (format_code != NULL)
            free(format_code);
        return;
    }

    qof_date_format_set(df);

    if (format_code != NULL)
        free(format_code);
}

/* gnc_configure_date_completion
 *    sets dateCompletion to the current value on the scheme side.
 *    QOF_DATE_COMPLETION_THISYEAR: use current year
 *    QOF_DATE_COMPLETION_SLIDING: use a sliding 12-month window
 *    backmonths 0-11: windows starts this many months before current month
 *
 * Args: Nothing
 * Returns: Nothing
 */
static void
gnc_configure_date_completion (void)
{
    char *date_completion = gnc_gconf_get_string(GCONF_GENERAL,
                            KEY_DATE_COMPLETION, NULL);
    int backmonths = gnc_gconf_get_float(GCONF_GENERAL,
                                         KEY_DATE_BACKMONTHS, NULL);
    QofDateCompletion dc;

    if (backmonths < 0)
    {
        backmonths = 0;
    }
    else if (backmonths > 11)
    {
        backmonths = 11;
    }

    if (date_completion && strcmp(date_completion, "sliding") == 0)
    {
        dc = QOF_DATE_COMPLETION_SLIDING;
    }
    else if (date_completion && strcmp(date_completion, "thisyear") == 0)
    {
        dc = QOF_DATE_COMPLETION_THISYEAR;
    }
    else
    {
        /* No preference has been set yet */
        PINFO("Incorrect date completion code, using defaults");
        dc = QOF_DATE_COMPLETION_THISYEAR;
        backmonths = 6;
        gnc_gconf_set_string (GCONF_GENERAL, KEY_DATE_COMPLETION, "thisyear", NULL);
        gnc_gconf_set_float (GCONF_GENERAL, KEY_DATE_BACKMONTHS, 6.0, NULL);
    }
    qof_date_completion_set(dc, backmonths);

    if (date_completion != NULL)
    {
        free(date_completion);
    }
}

void
gnc_gtk_add_rc_file (void)
{
    const gchar *var;
    gchar *str;

    var = g_get_home_dir ();
    if (var)
    {
        str = g_build_filename (var, ".gtkrc-2.0.gnucash", (char *)NULL);
        gtk_rc_add_default_file (str);
        g_free (str);
    }
}



/********************************************************************\
 * gnc_gnome_get_pixmap                                             *
 *   returns a GtkWidget given a pixmap filename                    *
 *                                                                  *
 * Args: none                                                       *
 * Returns: GtkWidget or NULL if there was a problem                *
 \*******************************************************************/
GtkWidget *
gnc_gnome_get_pixmap (const char *name)
{
    GtkWidget *pixmap;
    char *fullname;

    g_return_val_if_fail (name != NULL, NULL);

    fullname = gnc_filepath_locate_pixmap (name);
    if (fullname == NULL)
        return NULL;

    DEBUG ("Loading pixmap file %s", fullname);

    pixmap = gtk_image_new_from_file (fullname);
    if (pixmap == NULL)
    {
        PERR ("Could not load pixmap");
    }
    g_free (fullname);

    return pixmap;
}

/********************************************************************\
 * gnc_gnome_get_gdkpixbuf                                          *
 *   returns a GdkImlibImage object given a pixmap filename         *
 *                                                                  *
 * Args: none                                                       *
 * Returns: GdkPixbuf or NULL if there was a problem                *
 \*******************************************************************/
GdkPixbuf *
gnc_gnome_get_gdkpixbuf (const char *name)
{
    GdkPixbuf *pixbuf;
    GError *error = NULL;
    char *fullname;

    g_return_val_if_fail (name != NULL, NULL);

    fullname = gnc_filepath_locate_pixmap (name);
    if (fullname == NULL)
        return NULL;

    DEBUG ("Loading pixbuf file %s", fullname);
    pixbuf = gdk_pixbuf_new_from_file (fullname, &error);
    if (error != NULL)
    {
        g_assert (pixbuf == NULL);
        PERR ("Could not load pixbuf: %s", error->message);
        g_error_free (error);
    }
    g_free (fullname);

    return pixbuf;
}

static gboolean
gnc_ui_check_events (gpointer not_used)
{
    QofSession *session;
    gboolean force;

    if (gtk_main_level() != 1)
        return TRUE;

    if (!gnc_current_session_exist())
        return TRUE;
    session = gnc_get_current_session ();

    if (gnc_gui_refresh_suspended ())
        return TRUE;

    if (!qof_session_events_pending (session))
        return TRUE;

    gnc_suspend_gui_refresh ();

    force = qof_session_process_events (session);

    gnc_resume_gui_refresh ();

    if (force)
        gnc_gui_refresh_all ();

    return TRUE;
}

static int
gnc_x_error (Display *display, XErrorEvent *error)
{
    if (error->error_code)
    {
        char buf[64];

        XGetErrorText (display, error->error_code, buf, 63);

        g_warning ("X-ERROR **: %s\n  serial %ld error_code %d "
                   "request_code %d minor_code %d\n",
                   buf,
                   error->serial,
                   error->error_code,
                   error->request_code,
                   error->minor_code);
    }

    return 0;
}

int
gnc_ui_start_event_loop (void)
{
    guint id;

    gnome_is_running = TRUE;

    id = g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 10000, /* 10 secs */
                             gnc_ui_check_events, NULL, NULL);

    XSetErrorHandler (gnc_x_error);

    /* Enter gnome event loop */
    gtk_main ();

    g_source_remove (id);

    gnome_is_running = FALSE;
    gnome_is_terminating = FALSE;

    return 0;
}

GncMainWindow *
gnc_gui_init(void)
{
    static GncMainWindow *main_window;
    gchar *map;
    int idx;
    char *icon_filenames[] = {"gnucash-icon-16x16.png",
                              "gnucash-icon-32x32.png",
                              "gnucash-icon-48x48.png",
                              NULL
                             };
    GList *icons = NULL;
    char *fullname;

    ENTER ("");

    if (gnome_is_initialized)
        return main_window;

    /* use custom icon */
    for (idx = 0; icon_filenames[idx] != NULL; idx++)
    {
        GdkPixbuf *buf = NULL;

        fullname = gnc_filepath_locate_pixmap(icon_filenames[idx]);
        if (fullname == NULL)
        {
            g_warning("couldn't find icon file [%s]", icon_filenames[idx]);
            continue;
        }

        buf = gnc_gnome_get_gdkpixbuf(fullname);
        if (buf == NULL)
        {
            g_warning("error loading image from [%s]", fullname);
            g_free(fullname);
            continue;
        }
        g_free(fullname);
        icons = g_list_append(icons, buf);
    }

    gtk_window_set_default_icon_list(icons);
    g_list_foreach(icons, (GFunc)g_object_unref, NULL);
    g_list_free(icons);

    /* initialization required for gtkhtml (is it also needed for webkit?) */
    gtk_widget_set_default_colormap (gdk_rgb_get_colormap ());

    g_set_application_name(PACKAGE_NAME);

    gnc_show_splash_screen();
    assistant_gconf_install_check_schemas();

    gnome_is_initialized = TRUE;

    gnc_ui_util_init();
    gnc_configure_date_format();
    gnc_configure_date_completion();

    gnc_gconf_general_register_cb(
        KEY_DATE_FORMAT, (GncGconfGeneralCb)gnc_configure_date_format, NULL);
    gnc_gconf_general_register_cb(
        KEY_DATE_COMPLETION, (GncGconfGeneralCb)gnc_configure_date_completion, NULL);
    gnc_gconf_general_register_cb(
        KEY_DATE_BACKMONTHS, (GncGconfGeneralCb)gnc_configure_date_completion, NULL);
    gnc_gconf_general_register_any_cb(
        (GncGconfGeneralAnyCb)gnc_gui_refresh_all, NULL);

    gnc_file_set_shutdown_callback (gnc_shutdown);

    main_window = gnc_main_window_new ();
    // Bug#350993:
    // gtk_widget_show (GTK_WIDGET (main_window));
    gnc_window_set_progressbar_window (GNC_WINDOW(main_window));

    map = gnc_build_dotgnucash_path(ACCEL_MAP_NAME);
    gtk_accel_map_load(map);
    g_free(map);

    gnc_load_stock_icons();
    gnc_totd_dialog(GTK_WINDOW(main_window), TRUE);

    LEAVE ("");
    return main_window;
}

gboolean
gnucash_ui_is_running(void)
{
    return gnome_is_running;
}

static void
gnc_gui_destroy (void)
{
    if (!gnome_is_initialized)
        return;

    gnc_extensions_shutdown ();
}

static void
gnc_gui_shutdown (void)
{
    gchar *map;

    if (gnome_is_running && !gnome_is_terminating)
    {
        gnome_is_terminating = TRUE;

        map = gnc_build_dotgnucash_path(ACCEL_MAP_NAME);
        gtk_accel_map_save(map);
        g_free(map);

        gtk_main_quit();
    }
}

/*  shutdown gnucash.  This function will initiate an orderly
 *  shutdown, and when that has finished it will exit the program.
 */
void
gnc_shutdown (int exit_status)
{
    if (gnucash_ui_is_running())
    {
        if (!gnome_is_terminating)
        {
            if (gnc_file_query_save(FALSE))
            {
                gnc_hook_run(HOOK_UI_SHUTDOWN, NULL);
                gnc_gui_shutdown();
            }
        }
    }
    else
    {
        gnc_gui_destroy();
        gnc_hook_run(HOOK_SHUTDOWN, NULL);
        gnc_engine_shutdown();
        exit(exit_status);
    }
}

