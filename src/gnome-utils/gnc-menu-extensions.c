/********************************************************************\
 * gnc-menu-extensions.c -- functions to build dynamic menus        *
 * Copyright (C) 1999 Rob Browning         	                    *
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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <ctype.h>

#include "gnc-engine.h"
#include "gnc-menu-extensions.h"
#include "gnc-ui.h"


/* This static indicates the debugging module that this .o belongs to.  */
static QofLogModule log_module = GNC_MOD_GUI;

static GSList *extension_list = NULL;

GSList *
gnc_extensions_get_menu_list (void)
{
    return g_slist_copy(extension_list);
}


/******************** New Menu Item ********************/

static gchar*
gnc_ext_gen_action_name (const gchar *name)
{

    const gchar *extChar;
    GString *actionName;

    actionName = g_string_sized_new( strlen( name ) + 7 );

    // 'Mum & ble12' => 'Mumble___ble12'
    for ( extChar = name; *extChar != '\0'; extChar++ )
    {
        if ( ! isalnum( *extChar ) )
            g_string_append_c( actionName, '_' );
        g_string_append_c( actionName, *extChar );
    }

    // 'Mumble + 'Action' => 'MumbleAction'
    g_string_append_printf( actionName, "Action" );

    return g_string_free(actionName, FALSE);
}

/******************** Callback ********************/

static void
cleanup_extension_info(gpointer extension_info, gpointer not_used)
{
    ExtensionInfo *ext_info = extension_info;

//    if (ext_info->extension)
//        scm_gc_unprotect_object(ext_info->extension);

    g_free(ext_info->sort_key);
    g_free((gchar *)ext_info->ae.name);
    g_free((gchar *)ext_info->ae.label);
    g_free((gchar *)ext_info->ae.tooltip);
    g_free(ext_info->path);
    g_free(ext_info);
}

/******************** Shutdown ********************/

void
gnc_extensions_shutdown (void)
{
    g_slist_foreach(extension_list, cleanup_extension_info, NULL);

    g_slist_free(extension_list);

    extension_list = NULL;
}
