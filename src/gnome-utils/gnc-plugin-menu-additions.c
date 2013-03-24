/*
 * gnc-plugin-menu-additions.c --
 * Copyright (C) 2005 David Hampton hampton@employees.org>
 *
 * From:
 * gnc-menu-extensions.c -- functions to build dynamic menus
 * Copyright (C) 1999 Rob Browning
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

/** @addtogroup MenuPlugins
    @{ */
/** @addtogroup PluginMenuAdditions Non-GtkAction Menu Support
    @{ */
/** @file gnc-plugin-menu-additions.c
    @brief Functions providing menu items from scheme code.
    @author Copyright (C) 2005 David Hampton <hampton@employees.org>
*/

#include "config.h"

#include <gtk/gtk.h>
#include <string.h>

#include "gnc-engine.h"
#include "gnc-main-window.h"
#include "gnc-plugin-menu-additions.h"
#include "gnc-window.h"
#include "gnc-gconf-utils.h"
#include "gnc-ui.h"
#include "gnc-menu-extensions.h"

static GObjectClass *parent_class = NULL;

static void gnc_plugin_menu_additions_init (GncPluginMenuAdditions *plugin);
static void gnc_plugin_menu_additions_finalize (GObject *object);

static void gnc_plugin_menu_additions_remove_from_window (GncPlugin *plugin, GncMainWindow *window, GQuark type);

/* Command callbacks */

/* This static indicates the debugging module that this .o belongs to.  */
static QofLogModule log_module = GNC_MOD_GUI;

#define PLUGIN_ACTIONS_NAME "gnc-plugin-menu-additions-actions"

/** Private data for this plugin.  This data structure is unused. */
typedef struct GncPluginMenuAdditionsPrivate
{
    gpointer dummy;
} GncPluginMenuAdditionsPrivate;

#define GNC_PLUGIN_MENU_ADDITIONS_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNC_TYPE_PLUGIN_MENU_ADDITIONS, GncPluginMenuAdditionsPrivate))


/** Per-window private data for this plugin.  This plugin is unique in
 *  that it manages its own menu items. */
typedef struct _GncPluginMenuAdditionsPerWindow
{
    /** The menu/toolbar action information associated with a specific
        window.  This plugin must maintain its own data because of the
        way the menus are currently built. */
    GncMainWindow  *window;
    GtkUIManager   *ui_manager;
    GtkActionGroup *group;
    gint merge_id;
} GncPluginMenuAdditionsPerWindow;

/************************************************************
 *                  Object Implementation                   *
 ************************************************************/

static void
gnc_plugin_menu_additions_init (GncPluginMenuAdditions *plugin)
{
    ENTER("plugin %p", plugin);
    LEAVE("");
}

static void
gnc_plugin_menu_additions_finalize (GObject *object)
{
//    g_return_if_fail (GNC_IS_PLUGIN_MENU_ADDITIONS (object));

    ENTER("plugin %p", object);
    G_OBJECT_CLASS (parent_class)->finalize (object);
    LEAVE("");
}


/*  Create a new menu_additions plugin.  This plugin attaches the menu
 *  items from Scheme code to any window that is opened.
 *
 *  @return A pointer to the new object.
 */
GncPlugin *
gnc_plugin_menu_additions_new (void)
{
    GncPlugin *plugin_page = NULL;

    ENTER("");
//    plugin_page = GNC_PLUGIN (g_object_new (GNC_TYPE_PLUGIN_MENU_ADDITIONS, NULL));
    LEAVE("plugin %p", plugin_page);
    return plugin_page;
}


/** Compare two extension menu items and indicate which should appear
 *  first in the menu listings.  If only one of them is a submenu,
 *  choose that one.  Otherwise, the sort_keys are collation keys
 *  produced by g_utf8_collate_key(), so compare them with strcmp.
 *
 *  @param a A menu extension.
 *
 *  @param b A second menu extension.
 *
 *  @return -1 if extension 'a' should appear first. 1 if extension
 *  'b' should appear first. */
static gint
gnc_menu_additions_sort (ExtensionInfo *a, ExtensionInfo *b)
{
    if (a->type == b->type)
        return strcmp(a->sort_key, b->sort_key);
    else if (a->type == GTK_UI_MANAGER_MENU)
        return -1;
    else if (b->type == GTK_UI_MANAGER_MENU)
        return 1;
    else
        return 0;
}


/** Initialize the hash table of accelerator key maps.
 *
 *  @param unused Unused.
 *
 *  @return an empty hash table. */
static gpointer
gnc_menu_additions_init_accel_table (gpointer unused)
{
    return g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
}


/** Examine an extension menu item and see if it already has an
 *  accelerator key defined (in the source).  If so, add this key to
 *  the map of already used accelerator keys.  These maps are
 *  maintained per path, so accelerator keys may be duplicated across
 *  different menus but are guaranteed to be unique within any given
 *  menu.
 *
 *  @param info A menu extension.
 *
 *  @param table A hash table of accelerator maps. */
static void
gnc_menu_additions_do_preassigned_accel (ExtensionInfo *info, GHashTable *table)
{
    gchar *map, *new_map, *accel_key;
    const gchar *ptr;

    ENTER("Checking %s/%s [%s]", info->path, info->ae.label, info->ae.name);
    if (info->accel_assigned)
    {
        LEAVE("Already processed");
        return;
    }

    if (!g_utf8_validate(info->ae.label, -1, NULL))
    {
        g_warning("Extension menu label '%s' is not valid utf8.", info->ae.label);
        info->accel_assigned = TRUE;
        LEAVE("Label is invalid utf8");
        return;
    }

    /* Was an accelerator pre-assigned in the source? */
    ptr = g_utf8_strchr(info->ae.label, -1, '_');
    if (ptr == NULL)
    {
        LEAVE("not preassigned");
        return;
    }

    accel_key = g_utf8_strdown(g_utf8_next_char(ptr), 1);
    DEBUG("Accelerator preassigned: '%s'", accel_key);

    /* Now build a new map. Old one freed automatically. */
    map = g_hash_table_lookup(table, info->path);
    if (map == NULL)
        map = "";
    new_map = g_strconcat(map, accel_key, (gchar *)NULL);
    DEBUG("path '%s', map '%s' -> '%s'", info->path, map, new_map);
    g_hash_table_replace(table, info->path, new_map);

    info->accel_assigned = TRUE;
    g_free(accel_key);
    LEAVE("preassigned");
}


/** Examine an extension menu item and see if it needs to have an
 *  accelerator key assigned to it.  If so, find the first character
 *  in the menu name that isn't already assigned as an accelerator in
 *  the same menu, assign it to this item, and add it to the map of
 *  already used accelerator keys.  These maps are maintained per
 *  path, so accelerator keys may be duplicated across different menus
 *  but are guaranteed to be unique within any given menu.
 *
 *  @param info A menu extension.
 *
 *  @param table A hash table of accelerator maps. */
static void
gnc_menu_additions_assign_accel (ExtensionInfo *info, GHashTable *table)
{
    gchar *map, *new_map, *new_label, *start, buf[16];
    const gchar *ptr;
    gunichar uni;
    gint len;

    ENTER("Checking %s/%s [%s]", info->path, info->ae.label, info->ae.name);
    if (info->accel_assigned)
    {
        LEAVE("Already processed");
        return;
    }

    /* Get map of used keys */
    map = g_hash_table_lookup(table, info->path);
    if (map == NULL)
        map = g_strdup("");
    DEBUG("map '%s', path %s", map, info->path);

    for (ptr = info->ae.label; *ptr; ptr = g_utf8_next_char(ptr))
    {
        uni = g_utf8_get_char(ptr);
        if (!g_unichar_isalpha(uni))
            continue;
        uni = g_unichar_tolower(uni);
        len = g_unichar_to_utf8(uni, buf);
        buf[len] = '\0';
        DEBUG("Testing character '%s'", buf);
        if (!g_utf8_strchr(map, -1, uni))
            break;
    }

    if (ptr == NULL)
    {
        /* Ran out of characters. Nothing to do. */
        info->accel_assigned = TRUE;
        LEAVE("All characters already assigned");
        return;
    }

    /* Now build a new string in the form "<start>_<end>". */
    start = g_strndup(info->ae.label, ptr - info->ae.label);
    DEBUG("start %p, len %ld, text '%s'", start, g_utf8_strlen(start, -1), start);
    new_label = g_strconcat(start, "_", ptr, (gchar *)NULL);
    g_free(start);
    DEBUG("label '%s' -> '%s'", info->ae.label, new_label);
    g_free((gchar *)info->ae.label);
    info->ae.label = new_label;

    /* Now build a new map. Old one freed automatically. */
    new_map = g_strconcat(map, buf, (gchar *)NULL);
    DEBUG("map '%s' -> '%s'", map, new_map);
    g_hash_table_replace(table, info->path, new_map);

    info->accel_assigned = TRUE;
    LEAVE("assigned");
}



/** Tear down the report menu and other additional menus.  This
 *  function is called as part of the cleanup of a window, while the
 *  plugin menu items are being removed from the menu structure.
 *
 *  @param plugin A pointer to the gnc-plugin object responsible for
 *  adding/removing the additional menu items.
 *
 *  @param window A pointer the gnc-main-window that is being destroyed.
 *
 *  @param type Unused
 */
static void
gnc_plugin_menu_additions_remove_from_window (GncPlugin *plugin,
        GncMainWindow *window,
        GQuark type)
{
    GtkActionGroup *group;

    ENTER(" ");

    /* Have to remove our actions manually. Its only automatic if the
     * actions name is installed into the plugin class. */
    group = gnc_main_window_get_action_group(window, PLUGIN_ACTIONS_NAME);
    if (group)
        gtk_ui_manager_remove_action_group(window->ui_merge, group);

    /* Note: This code does not clean up the per-callback data structures
     * that are created by the gnc_menu_additions_menu_setup_one()
     * function. Its not much memory and shouldn't be a problem. */

    LEAVE(" ");
}

/** @} */
/** @} */
