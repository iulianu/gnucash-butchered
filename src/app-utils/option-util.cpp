/********************************************************************\
 * option-util.cpp -- GNOME<->guile option interface                *
 * Copyright (C) 2000 Dave Peticolas                                *
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
\********************************************************************/

#include "config.h"

#ifdef GNOME
# include <gtk/gtk.h>
#endif
#include <glib/gi18n.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "option-util.h"
#include "engine-helpers.h"
#include "qof.h"

/* TODO:

  - for make-date-option, there seems to be only support for getting,
    not for setting.
*/


/****** Structures *************************************************/

struct gnc_option
{
    /* Flag to indicate change by the UI */
    gboolean changed;

    /* The widget which is holding this option */
    gpointer widget;

    /* The option db which holds this option */
    GNCOptionDB *odb;
};

struct gnc_option_section
{
    char * section_name;

    GSList * options;
};

struct gnc_option_db
{
    GSList *option_sections;

    gboolean options_dirty;

    GNCOptionDBHandle handle;
};


/****** Globals ****************************************************/

/* This static indicates the debugging module this .o belongs to.  */
static QofLogModule log_module = GNC_MOD_GUI;

static GHashTable *option_dbs = NULL;
static int last_db_handle = 0;


/*******************************************************************/
void
gnc_option_set_changed (GNCOption *option, gboolean changed)
{
    g_return_if_fail (option != NULL);
    option->changed = changed;
}

gpointer
gnc_option_get_widget (GNCOption *option)
{
    if (!option) return NULL;
    return option->widget;
}

void
gnc_option_set_widget (GNCOption *option, gpointer widget)
{
    g_return_if_fail (option != NULL);
    option->widget = widget;
}


/********************************************************************\
 * gnc_option_db_init                                               *
 *   initialize the options structures from the guile side          *
 *                                                                  *
 * Args: odb     - the option database to initialize                *
 * Returns: nothing                                                 *
\********************************************************************/
static void
gnc_option_db_init(GNCOptionDB *odb)
{
}


typedef struct
{
    GNCOptionDB *odb;
} ODBFindInfo;

static void
option_db_finder (gpointer key, gpointer value, gpointer data)
{
    ODBFindInfo *find_info = data;
    GNCOptionDB *odb = value;

    if (odb)
        find_info->odb = odb;
}

/********************************************************************\
 * gnc_option_db_destroy                                            *
 *   unregister the scheme options and free all the memory          *
 *   associated with an option database, including the database     *
 *   itself                                                         *
 *                                                                  *
 * Args: options database to destroy                                *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_option_db_destroy(GNCOptionDB *odb)
{
    GSList *snode;

    if (odb == NULL)
        return;

    for (snode = odb->option_sections; snode; snode = snode->next)
    {
        GNCOptionSection *section = snode->data;
        GSList *onode;

        for (onode = section->options; onode; onode = onode->next)
        {
            GNCOption *option = onode->data;

            g_free (option);
        }

        /* Free the option list */
        g_slist_free(section->options);
        section->options = NULL;

        if (section->section_name != NULL)
            free(section->section_name);
        section->section_name = NULL;

        g_free (section);
    }

    g_slist_free(odb->option_sections);

    odb->option_sections = NULL;
    odb->options_dirty = FALSE;

    g_hash_table_remove(option_dbs, &odb->handle);

    if (g_hash_table_size(option_dbs) == 0)
    {
        g_hash_table_destroy(option_dbs);
        option_dbs = NULL;
    }

    g_free(odb);
}

void
gncp_option_invoke_callback (GNCOptionChangeCallback callback, void *data)
{
    callback (data);
}



/********************************************************************\
 * gnc_option_db_get_changed                                        *
 *   returns a boolean value, TRUE if any option has changed,       *
 *   FALSE is none of the options have changed                      *
 *                                                                  *
 * Args: odb - option database to check                             *
 * Return: boolean                                                  *
\********************************************************************/
gboolean
gnc_option_db_get_changed(GNCOptionDB *odb)
{
    GSList *section_node;
    GSList *option_node;
    GNCOptionSection *section;
    GNCOption *option;

    g_return_val_if_fail (odb, FALSE);

    for (section_node = odb->option_sections; section_node;
            section_node = section_node->next)
    {

        section = section_node->data;

        for (option_node = section->options; option_node;
                option_node = option_node->next)
        {

            option = option_node->data;

            if (option->changed)
                return TRUE;
        }
    }
    return FALSE;
}



/********************************************************************\
 * gnc_option_db_section_reset_widgets                              *
 *   reset all option widgets in one section to their default.      *
 *   values                                                         *
 *                                                                  *
 * Args: odb - option database to reset                             *
 * Return: nothing                                                  *
\********************************************************************/
void
gnc_option_db_section_reset_widgets (GNCOptionSection *section)
{
    GSList *option_node;
    GNCOption *option;

    g_return_if_fail (section);

    /* Don't reset "invisible" options.
     * If the section name begins "__" we should not reset
     */
    if (section->section_name == NULL ||
            strncmp (section->section_name, "__", 2) == 0)
        return;

    for (option_node = section->options;
            option_node != NULL;
            option_node = option_node->next)
    {
        option = option_node->data;

//        gnc_option_set_ui_value (option, TRUE);
        gnc_option_set_changed (option, TRUE);
    }
}


/********************************************************************\
 * gnc_option_db_reset_widgets                                      *
 *   reset all option widgets to their default values.              *
 *                                                                  *
 * Args: odb - option database to reset                             *
 * Return: nothing                                                  *
\********************************************************************/
void
gnc_option_db_reset_widgets (GNCOptionDB *odb)
{
    GSList *section_node;
    GNCOptionSection *section;

    g_return_if_fail (odb);

    for (section_node = odb->option_sections;
            section_node != NULL;
            section_node = section_node->next)
    {
        section = section_node->data;
        gnc_option_db_section_reset_widgets (section);
    }
}


static void
free_helper(gpointer string, gpointer not_used)
{
    if (string) free(string);
}

void
gnc_free_list_option_value(GSList *list)
{
    g_slist_foreach(list, free_helper, NULL);
    g_slist_free(list);
}


/********************************************************************\
 * gnc_option_db_set_option_default                                 *
 *   set the option to its default value                            *
 *                                                                  *
 * Args: odb     - option database to search in                     *
 *       section - section name of option                           *
 *       name    - name of option                                   *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_option_db_set_option_default(GNCOptionDB *odb,
                                 const char *section,
                                 const char *name)
{
    GNCOption *option;

//    option = gnc_option_db_get_option_by_name(odb, section, name);

//    gnc_option_set_default(option);
}


/* For now, this is global, just like when it was in guile.
   But, it should be make per-book. */
static GHashTable *kvp_registry = NULL;

static void
init_table(void)
{
    if (!kvp_registry)
        kvp_registry = g_hash_table_new(g_str_hash, g_str_equal);
}
