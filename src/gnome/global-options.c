/********************************************************************\
 * global-options.c -- GNOME global option handling                 *
 * Copyright (C) 1998,1999 Linas Vepstas                            *
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
 * along with this program; if not, write to the Free Software      *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.        *
\********************************************************************/

#include "top-level.h"

#include <gnome.h>

#include "global-options.h"
#include "dialog-options.h"
#include "dialog-utils.h"
#include "option-util.h"
#include "query-user.h"
#include "guile-util.h"
#include "messages.h"
#include "util.h"

/* This static indicates the debugging module that this .o belongs to.  */
static short module = MOD_GUI;

static GNCOptionDB *global_options = NULL;
static SCM guile_global_options    = SCM_UNDEFINED;
static SCM guile_global_options_id = SCM_UNDEFINED;


/********************************************************************\
 * gnc_options_init                                                 *
 *   initialize the options structures from the guile side          *
 *                                                                  *
 * Args: none                                                       *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_options_init()
{
  SCM func = gh_eval_str("gnc:send-global-options");

  if (gh_procedure_p(func))
    gh_call0(func);
  else
  {
    PERR("gnc_options_init: no guile options!");
  }

  global_options = gnc_option_db_new();
  gnc_option_db_init(global_options, guile_global_options);
}


/********************************************************************\
 * gnc_options_shutdown                                             *
 *   unregister the scheme options and free the structure memory    *
 *                                                                  *
 * Args: none                                                       *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_options_shutdown()
{
  gnc_option_db_destroy(global_options);
  global_options = NULL;

  if (guile_global_options_id != SCM_UNDEFINED)
    gnc_unregister_c_side_scheme_ptr_id(guile_global_options_id);
  guile_global_options = SCM_UNDEFINED;
  guile_global_options_id = SCM_UNDEFINED;
}


/********************************************************************\
 * gnc_register_option_change_callback                              *
 *   register a callback to be called whenever an option changes    *
 *   this is rather heavy-weight, since all handlers will be called *
 *   whenever any option changes. We may need to refine it later.   *
 *                                                                  *
 * Args: callback - the callback function                           *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_register_option_change_callback(OptionChangeCallback callback,
                                    gpointer user_data)
{
  gnc_option_db_register_change_callback(global_options, callback, user_data);
}


/********************************************************************\
 * gnc_get_option_by_name                                           *
 *   returns an option given section name and name                  *
 *                                                                  *
 * Args: section_name - name of section to search for               *
 *       name         - name to search for                          *
 * Returns: given option, or NULL if none                           *
\********************************************************************/
GNCOption *
gnc_get_option_by_name(char *section_name, char *name)
{
  return gnc_option_db_get_option_by_name(global_options,
                                          section_name, name);
}


/********************************************************************\
 * gnc_get_option_by_SCM                                            *
 *   returns an option given SCM handle. Uses section and name.     *
 *                                                                  *
 * Args: guile_option - SCM handle of option                        *
 * Returns: given option, or NULL if none                           *
\********************************************************************/
GNCOption *
gnc_get_option_by_SCM(SCM guile_option)
{
  return gnc_option_db_get_option_by_SCM(global_options, guile_option);
}


/********************************************************************\
 * gnc_lookup_boolean_option                                        *
 *   looks up a boolean option. If present, returns its value,      *
 *   otherwise returns the default.                                 *
 *                                                                  *
 * Args: section - section name of option                           *
 *       name    - name of option                                   *
 *       default - default value if not found                       *
 * Return: gboolean option value                                    *
\********************************************************************/
gboolean
gnc_lookup_boolean_option(char *section, char *name, gboolean default_value)
{
  return gnc_option_db_lookup_boolean_option(global_options, section,
                                             name, default_value);
}


/********************************************************************\
 * gnc_lookup_string_option                                         *
 *   looks up a string option. If present, returns its malloc'ed    *
 *   value, otherwise returns the strdup'ed default, or NULL if     *
 *   default was NULL.                                              *
 *                                                                  *
 * Args: section - section name of option                           *
 *       name    - name of option                                   *
 *       default - default value if not found                       *
 * Return: char * option value                                      *
\********************************************************************/
char *
gnc_lookup_string_option(char *section, char *name, char *default_value)
{
  return gnc_option_db_lookup_string_option(global_options, section,
                                            name, default_value);
}


/********************************************************************\
 * gnc_lookup_multichoice_option                                    *
 *   looks up a multichoice option. If present, returns its         *
 *   name as a malloc'ed string                                     *
 *   value, otherwise returns the strdup'ed default, or NULL if     *
 *   default was NULL.                                              *
 *                                                                  *
 * Args: section - section name of option                           *
 *       name    - name of option                                   *
 *       default - default value if not found                       *
 * Return: char * option value                                      *
\********************************************************************/
char *
gnc_lookup_multichoice_option(char *section, char *name, char *default_value)
{
  return gnc_option_db_lookup_multichoice_option(global_options, section,
                                                 name, default_value);
}


/********************************************************************\
 * _gnc_option_refresh_ui                                           *
 *   sets the GUI representation of an option with its current      *
 *   current guile value. this is intended for use by guile only    *
 *                                                                  *
 * Args: option - SCM handle to option                              *
 * Return: nothing                                                  *
\********************************************************************/
void
_gnc_option_refresh_ui(SCM guile_option)
{
  GNCOption *option;

  option = gnc_option_db_get_option_by_SCM(global_options, guile_option);
  gnc_option_set_ui_value(option, FALSE);
}


/********************************************************************\
 * _gnc_register_global_options                                     *
 *   registers the global options. Intended to be called from guile.*
 *                                                                  *
 * Args: options - the guile global options                         *
 * Returns: nothing                                                 *
\********************************************************************/
void
_gnc_register_global_options(SCM options)
{
  guile_global_options = options;
  guile_global_options_id = gnc_register_c_side_scheme_ptr(options);
}


static void
gnc_options_dialog_apply_cb(GnomePropertyBox *propertybox,
			    gint arg1, gpointer user_data)
{
  if (arg1 == -1)
    gnc_option_db_commit(global_options);
}

static void
gnc_options_dialog_help_cb(GnomePropertyBox *propertybox,
			   gint arg1, gpointer user_data)
{
  gnome_ok_dialog("Help on properties");
}

/* Options dialog... this should house all of the config options     */
/* like where the docs reside, and whatever else is deemed necessary */
void
gnc_show_options_dialog()
{
  static GnomePropertyBox *options_dialog = NULL;

  if (gnc_option_db_num_sections(global_options) == 0)
  {
    gnc_warning_dialog("No options!");
    return;
  }

  if (gnc_option_db_dirty(global_options))
  {
    if (options_dialog != NULL)
      gtk_widget_destroy(GTK_WIDGET(options_dialog));

    options_dialog = NULL;
  }

  if (options_dialog == NULL)
  {
    options_dialog = GNOME_PROPERTY_BOX(gnome_property_box_new());
    gnome_dialog_close_hides(GNOME_DIALOG(options_dialog), TRUE);

    gnc_build_options_dialog_contents(options_dialog, global_options);
    gnc_option_db_clean(global_options);

    gtk_window_set_title(GTK_WINDOW(options_dialog), GNC_PREFS);

    gtk_signal_connect(GTK_OBJECT(options_dialog), "apply",
		       GTK_SIGNAL_FUNC(gnc_options_dialog_apply_cb),
		       NULL);

    gtk_signal_connect(GTK_OBJECT(options_dialog), "help",
		       GTK_SIGNAL_FUNC(gnc_options_dialog_help_cb),
		       NULL);
  }

  gtk_widget_show(GTK_WIDGET(options_dialog));  
  gdk_window_raise(GTK_WIDGET(options_dialog)->window);
}
