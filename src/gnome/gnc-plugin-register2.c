/*
 * gnc-plugin-register2.c --
 *
 * Copyright (C) 2003 Jan Arne Petersen
 * Author: Jan Arne Petersen <jpetersen@uni-bonn.de>
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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include "gnc-component-manager.h"
#include "gnc-plugin-register2.h"
#include "gnc-plugin-page-register2.h"

/* This define will enable the New Register menu option, comment out to hide it */
#define REG2ENABLE "yes"


static void gnc_plugin_register2_class_init (GncPluginRegister2Class *klass);
static void gnc_plugin_register2_init (GncPluginRegister2 *plugin);
static void gnc_plugin_register2_finalize (GObject *object);

/* Command callbacks */
static void gnc_plugin_register2_cmd_general_ledger (GtkAction *action, GncMainWindowActionData *data);

#define PLUGIN_ACTIONS_NAME "gnc-plugin-register2-actions"
#define PLUGIN_UI_FILENAME  "gnc-plugin-register2-ui.xml"
#define GCONF_REGISTER2_SECTION "general/register"

static GtkActionEntry gnc_plugin_actions [] =
{
#ifdef REG2ENABLE
    {
        "ToolsGeneralLedger2Action", NULL, N_("_General Ledger2"), NULL,
        N_("Open a general ledger2 window"),
        G_CALLBACK (gnc_plugin_register2_cmd_general_ledger)
    },
#endif
};
static guint gnc_plugin_n_actions = G_N_ELEMENTS (gnc_plugin_actions);

typedef struct GncPluginRegister2Private
{
    gpointer dummy;
} GncPluginRegister2Private;

#define GNC_PLUGIN_REGISTER2_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNC_TYPE_PLUGIN_REGISTER2, GncPluginRegister2Private))

static GObjectClass *parent_class = NULL;
static QofLogModule log_module = GNC_MOD_GUI;

/************************************************************
 *                     Other Functions                      *
 ************************************************************/

/** This function is called whenever an entry in the general register
 *  section of gconf is changed.  It does nothing more than kick off a
 *  gui refresh which should be delivered to any open register page.
 *  The register pages will then reread their settings from gconf and
 *  update the screen.
 *
 *  @client Unused.
 *
 *  @cnxn_id Unused.
 *
 *  @entry Unused.
 *
 *  @user_data Unused.
 */
static void
gnc_plugin_register2_gconf_changed (GConfClient *client,
                                   guint cnxn_id,
                                   GConfEntry *entry,
                                   gpointer user_data)
{
    ENTER("");
    gnc_gui_refresh_all ();
    LEAVE("");
}

/************************************************************
 *                  Object Implementation                   *
 ************************************************************/

GType
gnc_plugin_register2_get_type (void)
{
    static GType gnc_plugin_register2_type = 0;

    if (gnc_plugin_register2_type == 0)
    {
        static const GTypeInfo our_info =
        {
            sizeof (GncPluginRegister2Class),
            NULL,		/* base_init */
            NULL,		/* base_finalize */
            (GClassInitFunc) gnc_plugin_register2_class_init,
            NULL,		/* class_finalize */
            NULL,		/* class_data */
            sizeof (GncPluginRegister2),
            0,		/* n_preallocs */
            (GInstanceInitFunc) gnc_plugin_register2_init
        };

        gnc_plugin_register2_type = g_type_register_static (GNC_TYPE_PLUGIN,
                                   "GncPluginRegister2",
                                   &our_info, 0);
    }

    return gnc_plugin_register2_type;
}

GncPlugin *
gnc_plugin_register2_new (void)
{
    GncPluginRegister2 *plugin;

    /* Reference the register page plugin to ensure it exists in
     * the gtk type system. */
    GNC_TYPE_PLUGIN_PAGE_REGISTER2;

    plugin = g_object_new (GNC_TYPE_PLUGIN_REGISTER2,
                           NULL);

    return GNC_PLUGIN (plugin);
}

static void
gnc_plugin_register2_class_init (GncPluginRegister2Class *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GncPluginClass *plugin_class = GNC_PLUGIN_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->finalize = gnc_plugin_register2_finalize;

    /* plugin info */
    plugin_class->plugin_name  = GNC_PLUGIN_REGISTER2_NAME;

#ifdef REG2ENABLE
    /* widget addition/removal */
    plugin_class->actions_name = PLUGIN_ACTIONS_NAME;
    plugin_class->actions      = gnc_plugin_actions;
    plugin_class->n_actions    = gnc_plugin_n_actions;
    plugin_class->ui_filename  = PLUGIN_UI_FILENAME;
#endif
    plugin_class->gconf_section = GCONF_REGISTER2_SECTION;
    plugin_class->gconf_notifications = gnc_plugin_register2_gconf_changed;

    g_type_class_add_private(klass, sizeof(GncPluginRegister2Private));
}

static void
gnc_plugin_register2_init (GncPluginRegister2 *plugin)
{
}

static void
gnc_plugin_register2_finalize (GObject *object)
{
    GncPluginRegister2 *plugin;
    GncPluginRegister2Private *priv;

    g_return_if_fail (GNC_IS_PLUGIN_REGISTER2 (object));

    plugin = GNC_PLUGIN_REGISTER2 (object);
    priv = GNC_PLUGIN_REGISTER2_GET_PRIVATE(plugin);

    G_OBJECT_CLASS (parent_class)->finalize (object);
}

/************************************************************
 *                    Command Callbacks                     *
 ************************************************************/

static void
gnc_plugin_register2_cmd_general_ledger (GtkAction *action,
                                        GncMainWindowActionData *data)
{
    GncPluginPage *page;

    g_return_if_fail (data != NULL);

    page = gnc_plugin_page_register2_new_gl ();
    gnc_main_window_open_page (data->window, page);
}
