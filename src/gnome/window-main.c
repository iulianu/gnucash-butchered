/********************************************************************
 * window-main.c -- open/close/configure GNOME MDI main window      *
 * Copyright (C) 1998,1999 Jeremy Collins	                    *
 * Copyright (C) 1998,1999,2000 Linas Vepstas                       *
 * Copyright (C) 2001 Bill Gribble                                  *
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
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
 ********************************************************************/

#include "config.h"

#include <errno.h>
#include <gnome.h>
#include <guile/gh.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dialog-account.h"
#include "dialog-fincalc.h"
#include "dialog-find-transactions.h"
#include "dialog-options.h"
#include "dialog-scheduledxaction.h"
#include "dialog-sxsincelast.h"
#include "dialog-totd.h"
#include "dialog-transfer.h"
#include "dialog-utils.h"
#include "druid-loan.h"
#include "gfec.h"
#include "global-options.h"
#include "gnc-engine.h"
#include "gnc-engine-util.h"
#include "gnc-file-dialog.h"
#include "gnc-file-history.h"
#include "gnc-file-history-gnome.h"
#include "gnc-file.h"
#include "gnc-gui-query.h"
#include "gnc-menu-extensions.h"
#include "gnc-ui.h"
#include "guile-util.h"
#include "mainwindow-account-tree.h"
#include "option-util.h"
#include "top-level.h"
#include "window-acct-tree.h"
#include "window-help.h"
#include "window-main-summarybar.h"
#include "window-main.h"
#include "window-reconcile.h"
#include "window-register.h"
#include "window-report.h"

static gboolean gnc_show_status_bar = TRUE;
static gboolean gnc_show_summary_bar = TRUE;

static void gnc_main_window_create_menus(GNCMDIInfo * maininfo);
static GnomeUIInfo * gnc_main_window_toolbar_prefix (void);
static GnomeUIInfo * gnc_main_window_toolbar_suffix (void);


/**
 * gnc_main_window_get_mdi_child
 *
 * This routine grovels through the mdi data structures and finds the
 * GNCMDIChildInfo data structure for the view currently at the top of
 * the stack.
 *
 * returns: A pointer to the GNCMDIChildInfo data structure for the
 * current view, or NULL in cast of error.
 */
static GNCMDIChildInfo *
gnc_main_window_get_mdi_child (void)
{
  GNCMDIInfo *main_info;
  GnomeMDI *mdi;

  main_info = gnc_mdi_get_current ();
  if (!main_info)
    return(NULL);

  mdi = main_info->mdi;
  if (!mdi || !mdi->active_child)
    return(NULL);

  return(gtk_object_get_user_data(GTK_OBJECT(mdi->active_child)));
}

/********************************************************************
 * gnc_shutdown
 * close down gnucash from the C side...
 ********************************************************************/
void
gnc_shutdown (int exit_status)
{
  /*SCM scm_shutdown = gnc_scm_lookup("gnucash bootstrap", "gnc:shutdown");*/
  SCM scm_shutdown = gh_eval_str("gnc:shutdown");

  if(scm_procedure_p(scm_shutdown) != SCM_BOOL_F)
  {
    SCM scm_exit_code = gh_long2scm(exit_status);    
    gh_call1(scm_shutdown, scm_exit_code);
  }
  else
  {
    /* Either guile is not running, or for some reason we
       can't find gnc:shutdown. Either way, just exit. */
    g_warning("couldn't find gnc:shutdown -- exiting anyway.");
    exit(exit_status);
  }
}

/********************************************************************
 * gnc_main_window_app_created_cb()
 * called when a new top-level GnomeApp is created.  
 ********************************************************************/

static void
gnc_main_window_app_created_cb(GnomeMDI * mdi, GnomeApp * app, 
                               gpointer data) {
  GtkWidget * summarybar;
  GtkWidget * statusbar;

  /* add the summarybar */
  summarybar = gnc_main_window_summary_new();

  {
    GnomeDockItemBehavior behavior;
    GtkWidget *item;

    /* This is essentially gnome_app_add_docked, but without using
     * gnome_app_add_dock_item because it emits the layout_changed
     * signal which creates a new layout and writes it over the saved
     * layout config before we've had a chance to read it.
     */

    behavior = (GNOME_DOCK_ITEM_BEH_EXCLUSIVE |
                GNOME_DOCK_ITEM_BEH_NEVER_VERTICAL);
    if (!gnome_preferences_get_toolbar_detachable ())
      behavior |= GNOME_DOCK_ITEM_BEH_LOCKED;

    item = gnome_dock_item_new("Summary Bar", behavior);
    gtk_container_add( GTK_CONTAINER (item), summarybar );

    if (app->layout) {
      gnome_dock_layout_add_item( app->layout,
                                  GNOME_DOCK_ITEM(item),
                                  GNOME_DOCK_TOP,
                                  2, 0, 0 );
    }
    else {
      gnome_dock_add_item( GNOME_DOCK(app->dock),
                           GNOME_DOCK_ITEM(item),
                           GNOME_DOCK_TOP,
                           2, 0, 0, FALSE );
    }
  }

  /* add the statusbar */ 
  statusbar = gnome_appbar_new(TRUE, TRUE, GNOME_PREFERENCES_USER);
  gnome_app_set_statusbar(app, statusbar);

  /* set up extensions menu and hints */
  gnc_extensions_menu_setup (app, WINDOW_NAME_MAIN);

  /* make sure the file history is shown */ 
  gnc_history_update_menu (GTK_WIDGET (app));
}

static void
gnc_refresh_main_window_info (void)
{
  GList *containers = gtk_container_get_toplevels ();

  while (containers)
  {
    GtkWidget *w = containers->data;

    if (GNOME_IS_APP (w))
    {
      gnc_app_set_title (GNOME_APP (w));
      gnc_history_update_menu (w);
    }

    containers = containers->next;
  }
}


/********************************************************************
 * gnc_main_window_create_child()
 * open an MDI child given a config string (URL).  This is used at 
 * MDI session restore time 
 ********************************************************************/

static GnomeMDIChild * 
gnc_main_window_create_child(const gchar * configstring)
{
  GnomeMDIChild *child;
  URLType type;
  char * location;
  char * label;

  if (!configstring)
  {
    gnc_main_window_open_accounts (FALSE);
    return NULL;
  }

  type = gnc_html_parse_url(NULL, configstring, &location, &label);
  g_free(location);
  g_free(label);

  if (!safe_strcmp (type, URL_TYPE_REPORT)) {
    child = gnc_report_window_create_child(configstring);

  } else if (!safe_strcmp (type, URL_TYPE_ACCTTREE)) {
    child = gnc_acct_tree_window_create_child(configstring);

  } else {
    child = NULL;
  }

  return child;
}

/********************************************************************
 * gnc_main_window_can_*()
 ********************************************************************/

static gboolean
gnc_main_window_can_restore_cb (const char * filename)
{
  return !gnc_commodity_table_has_namespace (gnc_get_current_commodities (),
                                             GNC_COMMODITY_NS_LEGACY);
}

/**
 * gnc_main_window_tweak_menus
 *
 * @mc: A pointer to the GNC MDI child associated with the Main
 * window.
 *
 * This routine tweaks the View window in the main window menubar so
 * that the menu checkboxes correctly show the state of the Toolbar,
 * Summarybar and Statusbar.  There is no way to have the checkboxes
 * start checked.  This will trigger each of the callbacks once, but
 * they are designed to ignore the first 'sync' callback.  This is a
 * suboptimal solution, but I can't find a better one at the moment.
 */
static void
gnc_main_window_tweak_menus(GNCMDIChildInfo * mc)
{
  GtkWidget *widget;

  widget = gnc_mdi_child_find_menu_item(mc, "View/_Toolbar");
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), TRUE);

  widget = gnc_mdi_child_find_menu_item(mc, "View/_Status Bar");
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), TRUE);

  widget = gnc_mdi_child_find_menu_item(mc, "View/S_ummary Bar");
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), TRUE);

  mc->gnc_mdi->menu_tweaking = NULL;
}

/**
 * gnc_main_window_flip_toolbar_cb
 *
 * @widget: A pointer to the menu item selected. (ignored)
 * @data: The user data for this menu item. (ignored)
 *
 * This routine flips the state of the toolbar, hiding it if currently
 * visible, and showing it if not.  This routine has to grovel through
 * the mdi related data structures to find the current child data
 * structure (because there are potentially many windows).  The
 * callback data should point to the right mdi child data structure,
 * but doesn't appear to.
 */
static void
gnc_main_window_flip_toolbar_cb(GtkWidget * widget, gpointer data)
{
  GNCMDIChildInfo * mc;
  static gboolean in_sync = FALSE;
  gboolean toolbar_visibility = !gnc_mdi_get_toolbar_visibility();

  if (!in_sync) {
    /*
     * Sync the hard way. Syncing by calling the xxx_set_active
     * function causes an infinite recursion.
     */
    in_sync = TRUE;
    return;
  }

  mc = gnc_main_window_get_mdi_child();
  if (!mc)
    return;
  gnc_mdi_set_toolbar_visibility(toolbar_visibility);
  gnc_mdi_show_toolbar(mc);
}

/**
 * gnc_main_window_flip_status_bar_cb
 *
 * @widget: A pointer to the menu item selected. (ignored)
 * @data: The user data for this menu item. (ignored)
 *
 * This routine flips the state of the status bar, hiding it if
 * currently visible, and showing it if not.  This routine has to
 * grovel through the mdi related data structures to find the current
 * child data structure (because there are potentially many windows).
 * The callback data should point to the right mdi child data
 * structure, but doesn't appear to.
 */
static void
gnc_main_window_flip_status_bar_cb(GtkWidget * widget, gpointer data)
{
  GNCMDIChildInfo * mc;
  static gboolean in_sync = FALSE;

  if (!in_sync) {
    /*
     * Sync the hard way. Syncing by calling the xxx_set_active
     * function causes an infinite recursion.
     */
    in_sync = TRUE;
    return;
  }

  gnc_show_status_bar = !gnc_show_status_bar;

  mc = gnc_main_window_get_mdi_child();
  if (!mc || !mc->app)
    return;

  if (gnc_show_status_bar) {
    gtk_widget_show(mc->app->statusbar);
  } else {
    gtk_widget_hide(mc->app->statusbar);
    gtk_widget_queue_resize(mc->app->statusbar->parent);
  }
}

/**
 * gnc_main_window_flip_summary_bar_cb
 *
 * @widget: A pointer to the menu item selected. (ignored)
 * @data: The user data for this menu item. (ignored)
 *
 * This routine flips the state of the summary bar, hiding it if
 * currently visible, and showing it if not.  This routine has to
 * grovel through the mdi related data structures to find the current
 * child data structure (because there are potentially many windows).
 * The callback data should point to the right mdi child data
 * structure, but doesn't appear to.
 */
static void
gnc_main_window_flip_summary_bar_cb(GtkWidget * widget, gpointer data)
{
  GNCMDIChildInfo * mc;
  GnomeDockItem *summarybar;
  guint dc1, dc2, dc3, dc4;
  static gboolean in_sync = FALSE;

  if (!in_sync) {
    /*
     * Sync the hard way. Syncing by calling the xxx_set_active
     * function causes an infinite recursion.
     */
    in_sync = TRUE;
    return;
  }

  gnc_show_summary_bar = !gnc_show_summary_bar;

  mc = gnc_main_window_get_mdi_child();
  if (!mc || !mc->app)
    return;

  summarybar = gnome_dock_get_item_by_name(GNOME_DOCK(mc->app->dock),
					   "Summary Bar",
					   &dc1, &dc2, &dc3, &dc4);
  if (!summarybar) return;

  if (gnc_show_summary_bar) {
    gtk_widget_show(GTK_WIDGET(summarybar));
  } else {
    gtk_widget_hide(GTK_WIDGET(summarybar));
    gtk_widget_queue_resize(mc->app->dock);
  }
}

/********************************************************************
 * gnc_main_window_new()
 * initialize the Gnome MDI system
 ********************************************************************/

GNCMDIInfo * 
gnc_main_window_new (void)
{
  GNCMDIInfo * retval;

  retval = gnc_mdi_new ("GnuCash", "GnuCash",
                        gnc_main_window_toolbar_prefix (),
                        gnc_main_window_toolbar_suffix (),
                        gnc_shutdown,
                        gnc_main_window_can_restore_cb,
                        gnc_main_window_create_child);

  g_return_val_if_fail (retval != NULL, NULL);

  /* these menu and toolbar options are the ones that are always 
   * available */ 
  gnc_main_window_create_menus (retval);

  /* set up the position where the child menus/toolbars will be 
   * inserted  */
  gnome_mdi_set_child_menu_path(GNOME_MDI(retval->mdi),
                                "_Edit");
  gnome_mdi_set_child_list_path(GNOME_MDI(retval->mdi),
                                "_Windows/");

  /* handle top-level signals */
  gtk_signal_connect(GTK_OBJECT(retval->mdi), "app_created",
                     GTK_SIGNAL_FUNC(gnc_main_window_app_created_cb),
                     retval);

  /* handle show/hide items in view menu */
  retval->menu_tweaking = gnc_main_window_tweak_menus;

  return retval;
}

/********************************************************************
 * menu/toolbar data structures and callbacks 
 * these are the "templates" that are installed in every top level
 * MDI window
 ********************************************************************/

static void
gnc_main_window_options_cb(GtkWidget *widget, gpointer data)
{
  gnc_show_options_dialog();
}

static void
gnc_main_window_file_new_file_cb(GtkWidget * widget, gpointer data)
{
  gnc_file_new ();
  gnc_refresh_main_window_info ();
}

static void
gnc_main_window_file_new_window_cb(GtkWidget * widget, gpointer data)
{
  GNCMDIInfo *main_info;
  GnomeMDI *mdi;

  main_info = gnc_mdi_get_current ();
  if (!main_info) return;

  mdi = main_info->mdi;
  if (!mdi) return;

  if (mdi->active_child && mdi->active_view)
  {
    if (!strcmp(mdi->active_child->name, _("Accounts")))
    {
      gnc_main_window_open_accounts (TRUE);
    }
    else
    {
      GnomeMDIChild * child = mdi->active_child;
      gnome_mdi_remove_view(mdi, mdi->active_view, FALSE);
      gnome_mdi_add_toplevel_view(mdi, child);
    }
  }
}

static void
gnc_main_window_file_open_cb(GtkWidget * widget, gpointer data)
{
  gnc_file_open ();
  gnc_refresh_main_window_info ();
}

void
gnc_main_window_file_save_cb(GtkWidget * widget, gpointer data)
{
  gnc_file_save ();
  gnc_refresh_main_window_info ();
}

void
gnc_main_window_file_save_as_cb(GtkWidget * widget, gpointer data)
{
  gnc_file_save_as ();
  gnc_refresh_main_window_info ();
}

static void
gnc_main_window_file_export_cb(GtkWidget * widget, gpointer data)
{
  gnc_file_export_file(NULL);
  gnc_refresh_main_window_info ();
}

static void
gnc_main_window_file_shutdown_cb(GtkWidget * widget, gpointer data)
{
  gnc_shutdown (0);
}

/**
 * gnc_main_window_dispatch_cb
 *
 * @widget: A pointer to the menu item selected. (ignored)
 * @data: The user data for this menu item. (ignored)
 *
 * The main menubar has several items that must react differently
 * depending upon what window is in front when they are selected.
 * These menus all point to this dispatch routine, which uses the user
 * data associated with the menu item to determine which function was
 * requested. If then calls the appropriate dispatch function (and
 * data) registered for this item and saved in the GNCMDIChildInfo
 * data structure.
 *
 * Again, this routine has to grovel through the mdi related data
 * structures to find the current child data structure (because there
 * are potentially many windows).  The callback data should point to
 * the right mdi child data structure, but doesn't appear to.
 */
static void
gnc_main_window_dispatch_cb(GtkWidget * widget, gpointer data)
{
  GNCMDIChildInfo *mc;
  GNCMDIDispatchType type;
  gpointer *uidata;

  /* How annoying. MDI overrides the user data. Get it the hard way. */
  uidata = gtk_object_get_data(GTK_OBJECT(widget), GNOMEUIINFO_KEY_UIDATA);
  type = (GNCMDIDispatchType)GPOINTER_TO_UINT(uidata);
  g_return_if_fail(type < GNC_DISP_LAST);

  mc = gnc_main_window_get_mdi_child();
  if (mc && mc->dispatch_callback[type])
    mc->dispatch_callback[type](widget, mc->dispatch_data[type]);
}

/**
 * gnc_main_window_tax_info_cb
 *
 * @widget: A pointer to the menu item selected. (ignored)
 * @unused: The user data for this menu item. (ignored)
 *
 * Bring up the window for editing tax data.
 */
static void
gnc_main_window_tax_info_cb (GtkWidget * widget, gpointer unused) {
  GNCMDIChildInfo * mc;

  mc = gnc_main_window_get_mdi_child();
  if (!mc || !mc->app)
    return;
  gnc_tax_info_dialog(GTK_WIDGET(mc->app));
}

static void
gnc_main_window_file_close_cb(GtkWidget * widget, gpointer data)
{
  GNCMDIInfo *main_info;
  GnomeMDI *mdi;

  main_info = gnc_mdi_get_current ();
  if (!main_info) return;

  mdi = main_info->mdi;
  if (!mdi) return;

  if (mdi->active_child)
  {
    GNCMDIChildInfo * inf;

    inf = gtk_object_get_user_data(GTK_OBJECT(mdi->active_child));
    if (inf->toolbar)
    {
      gtk_widget_destroy (GTK_WIDGET(inf->toolbar)->parent);
      inf->toolbar = NULL;
    }

    gnome_mdi_remove_child (mdi, mdi->active_child, FALSE);
  }  
  else
  {
    gnc_warning_dialog (_("Select \"Exit\" to exit GnuCash."));
  }
}

void
gnc_main_window_fincalc_cb(GtkWidget *widget, gpointer data)
{
  gnc_ui_fincalc_dialog_create();
}

void
gnc_main_window_gl_cb(GtkWidget *widget, gpointer data)
{
  GNCLedgerDisplay *ld;
  RegWindow *regData;

  ld = gnc_ledger_display_gl ();

  regData = regWindowLedger (ld);

  gnc_register_raise (regData);
}

void
gnc_main_window_prices_cb(GtkWidget *widget, gpointer data)
{
  gnc_prices_dialog (NULL);
}


void
gnc_main_window_find_transactions_cb (GtkWidget *widget, gpointer data)
{
  gnc_ui_find_transactions_dialog_create(NULL);
}

void
gnc_main_window_sched_xaction_cb (GtkWidget *widget, gpointer data)
{
  gnc_ui_scheduled_xaction_dialog_create();
}

static
void
gnc_main_window_sched_xaction_slr_cb (GtkWidget *widget, gpointer data)
{
  gint ret;
  
  const char *nothing_to_do_msg =
    _( "There are no Scheduled Transactions to be entered at this time." );
  const char *no_dialog_but_created_msg =
    _( "There are no Scheduled Transactions to be entered at this time.\n"
       "(%d %s automatically created)" );
  
  ret = gnc_ui_sxsincelast_dialog_create();
  if ( ret == 0 ) {
    gnc_info_dialog( nothing_to_do_msg );
  } else if ( ret < 0 ) {
    gnc_info_dialog( no_dialog_but_created_msg,
                     -(ret), -(ret) == 1 ? _("transaction") : _("transactions") );
  } /* else { this else [>0 means dialog was created] intentionally left
     * blank. } */
}

static
void
gnc_main_window_sx_loan_druid_cb( GtkWidget *widget, gpointer data)
{
  gnc_ui_sx_loan_druid_create();
}

void
gnc_main_window_about_cb (GtkWidget *widget, gpointer data)
{
  GtkWidget *about;
  const gchar *message = _("The GnuCash personal finance manager.\n"
                           "The GNU way to manage your money!\n"
                           "http://www.gnucash.org/");
  const gchar *copyright = "(C) 1998-2002 Linas Vepstas";
  const gchar *authors[] = {
    "Rob Browning <rlb@cs.utexas.edu>",
    "Bill Gribble <grib@billgribble.com>",
    "James LewisMoss <dres@debian.org>",
    "Robert Graham Merkel <rgmerk@mira.net>",
    "Dave Peticolas <dave@krondo.com>",
    "Christian Stimming <stimming@tuhh.de>",
    "Linas Vepstas <linas@linas.org>",
    "Joshua Sled <jsled@asynchronous.org>",
    "Derek Atkins <derek@ihtfp.com>",
    "David Hampton <hampton@employees.org>",
    NULL
  };

  about = gnome_about_new ("GnuCash", VERSION, copyright,
                           authors, message, NULL);
  gnome_dialog_set_parent (GNOME_DIALOG(about),
                           GTK_WINDOW(gnc_ui_get_toplevel()));

  gnome_dialog_run_and_close (GNOME_DIALOG(about));
}

void
gnc_main_window_commodities_cb(GtkWidget *widget, gpointer data)
{
  gnc_commodities_dialog (NULL);
}


void
gnc_main_window_tutorial_cb (GtkWidget *widget, gpointer data)
{
  helpWindow(NULL, NULL, HH_MAIN);
}

void
gnc_main_window_totd_cb (GtkWidget *widget, gpointer data)

{
  gnc_ui_totd_dialog_create_and_run();
  return;
}

void
gnc_main_window_help_cb (GtkWidget *widget, gpointer data)
{
  helpWindow(NULL, NULL, HH_HELP);
}

void
gnc_main_window_exit_cb (GtkWidget *widget, gpointer data)
{
  gnc_shutdown(0);
}

static void
gnc_main_window_file_new_account_tree_cb(GtkWidget * w, gpointer data)
{
  gnc_main_window_open_accounts(FALSE);
}

static void
gnc_main_window_create_menus(GNCMDIInfo * maininfo)
{
  static GnomeUIInfo gnc_file_recent_files_submenu_template[] =
  {
    GNOMEUIINFO_END
  };

  static GnomeUIInfo gnc_file_import_submenu_template[] =
  {
    GNOMEUIINFO_END
  };

  static GnomeUIInfo gnc_file_export_submenu_template[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("Export _Accounts..."),
      N_("Export the account hierarchy to a new file"),
      gnc_main_window_file_export_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  static GnomeUIInfo gnc_file_menu_template[] = 
  {
    GNOMEUIINFO_MENU_NEW_ITEM(N_("_New File"),
                              N_("Create a new file"),
                              gnc_main_window_file_new_file_cb, NULL),
    {
      GNOME_APP_UI_ITEM,
      N_("New Account _Tree"),
      N_("Open a new account tree view"),
      gnc_main_window_file_new_account_tree_cb, NULL, NULL, 
      GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL
    },    
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_MENU_OPEN_ITEM(gnc_main_window_file_open_cb, NULL),
    {
      GNOME_APP_UI_ITEM,
      N_("Open in a New Window"),
      N_("Open a new top-level GnuCash window for the current view"),
      gnc_main_window_file_new_window_cb, NULL, NULL, 
      GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL
    },    
    GNOMEUIINFO_SUBTREE( N_("Open _Recent"),
                         gnc_file_recent_files_submenu_template ),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_MENU_SAVE_ITEM(gnc_main_window_file_save_cb, NULL),
    GNOMEUIINFO_MENU_SAVE_AS_ITEM(gnc_main_window_file_save_as_cb, NULL),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_SUBTREE( N_("_Import"),
                         gnc_file_import_submenu_template ),
    GNOMEUIINFO_SUBTREE( N_("_Export"),
                         gnc_file_export_submenu_template ),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_MENU_PRINT_ITEM(gnc_main_window_dispatch_cb,
				GUINT_TO_POINTER(GNC_DISP_PRINT)),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_MENU_CLOSE_ITEM(gnc_main_window_file_close_cb, NULL),
    GNOMEUIINFO_MENU_EXIT_ITEM(gnc_main_window_file_shutdown_cb, NULL),
    GNOMEUIINFO_END
  };
  
  static GnomeUIInfo gnc_edit_menu_template[] = 
  {
    GNOMEUIINFO_MENU_CUT_ITEM(gnc_main_window_dispatch_cb,
				GUINT_TO_POINTER(GNC_DISP_CUT)),
    GNOMEUIINFO_MENU_COPY_ITEM(gnc_main_window_dispatch_cb,
				GUINT_TO_POINTER(GNC_DISP_COPY)),
    GNOMEUIINFO_MENU_PASTE_ITEM(gnc_main_window_dispatch_cb,
				GUINT_TO_POINTER(GNC_DISP_PASTE)),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_MENU_PREFERENCES_ITEM(gnc_main_window_options_cb, NULL),
    { GNOME_APP_UI_ITEM,
      N_("Ta_x Options"),
      N_("Setup tax information for all income and expense accounts"),
      gnc_main_window_tax_info_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  static GnomeUIInfo gnc_view_menu_template[] = 
  {
    { GNOME_APP_UI_ITEM,
      N_("_Refresh"),
      N_("Refresh this window"),
      gnc_main_window_dispatch_cb, GUINT_TO_POINTER(GNC_DISP_REFRESH), NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    { GNOME_APP_UI_TOGGLEITEM,
      N_("_Toolbar"),
      N_("Show/hide the toolbar on this window"),
      gnc_main_window_flip_toolbar_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    { GNOME_APP_UI_TOGGLEITEM,
      N_("S_ummary Bar"),
      N_("Show/hide the summary bar on this window"),
      gnc_main_window_flip_summary_bar_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    { GNOME_APP_UI_TOGGLEITEM,
      N_("_Status Bar"),
      N_("Show/Hide the status bar on this window"),
      gnc_main_window_flip_status_bar_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  static GnomeUIInfo gnc_sched_xaction_tools_submenu_template[] = 
  {
    { GNOME_APP_UI_ITEM,
      N_("_Scheduled Transaction Editor"),
      N_("The list of Scheduled Transactions"),
      gnc_main_window_sched_xaction_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    { GNOME_APP_UI_ITEM,
      N_("_Since Last Run..."),
      N_("Create Scheduled Transactions since the last time run."),
      gnc_main_window_sched_xaction_slr_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    { GNOME_APP_UI_ITEM,
      N_( "_Mortgage & Loan Repayment..." ),
      N_( "Setup scheduled transactions for repayment of a loan" ),
      gnc_main_window_sx_loan_druid_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };
  
  static GnomeUIInfo gnc_actions_menu_template[] =
  {
    GNOMEUIINFO_SUBTREE( N_("_Scheduled Transactions"),
                         gnc_sched_xaction_tools_submenu_template ),
    GNOMEUIINFO_END
  };
  static GnomeUIInfo gnc_tools_menu_template[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("_General Ledger"),
      N_("Open a general ledger window"),
      gnc_main_window_gl_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Price Editor"),
      N_("View and edit the prices for stocks and mutual funds"),
      gnc_main_window_prices_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Commodity _Editor"),
      N_("View and edit the commodities for stocks and mutual funds"),
      gnc_main_window_commodities_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Financial _Calculator"),
      N_("Use the financial calculator"),
      gnc_main_window_fincalc_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    { GNOME_APP_UI_ITEM,
      N_("_Find Transactions"),
      N_("Find transactions with a search"),
      gnc_main_window_find_transactions_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      'f', GDK_CONTROL_MASK, NULL
    },
    GNOMEUIINFO_END
  };
  
  static GnomeUIInfo gnc_help_menu_template[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("Tutorial and Concepts _Guide"),
      N_("Open the GnuCash Tutorial"),
      gnc_main_window_tutorial_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Tips Of The Day"),
      N_("View the Tips of the Day"),
      gnc_main_window_totd_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Help"),
      N_("Open the GnuCash Help"),
      gnc_main_window_help_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    
    GNOMEUIINFO_MENU_ABOUT_ITEM(gnc_main_window_about_cb, NULL),

    GNOMEUIINFO_END
  };

  static GnomeUIInfo gnc_windows_menu_template[] =
  {
    GNOMEUIINFO_END
  };

  static GnomeUIInfo gnc_main_menu_template[] =
  {
    GNOMEUIINFO_MENU_FILE_TREE(gnc_file_menu_template),
    GNOMEUIINFO_MENU_EDIT_TREE(gnc_edit_menu_template),
    GNOMEUIINFO_MENU_VIEW_TREE(gnc_view_menu_template),
    GNOMEUIINFO_SUBTREE(N_("_Actions"), gnc_actions_menu_template),
    GNOMEUIINFO_SUBTREE(N_("_Tools"), gnc_tools_menu_template),
    GNOMEUIINFO_SUBTREE(N_("_Windows"), gnc_windows_menu_template),    
    GNOMEUIINFO_MENU_HELP_TREE(gnc_help_menu_template),
    GNOMEUIINFO_END
  };

  gnome_mdi_set_menubar_template(GNOME_MDI(maininfo->mdi),
                                 gnc_main_menu_template);

  gnc_file_history_add_after ("Open _Recent/");
}

static GnomeUIInfo *
gnc_main_window_toolbar_prefix (void)
{
  static GnomeUIInfo prefix[] = 
  {
    { GNOME_APP_UI_ITEM,
      N_("Save"),
      N_("Save the file to disk"),
      gnc_main_window_file_save_cb,
      NULL,
      NULL,
      GNOME_APP_PIXMAP_STOCK, 
      GNOME_STOCK_PIXMAP_SAVE,
      0, 0, NULL
    },
    { GNOME_APP_UI_ITEM,
      N_("Close"),
      N_("Close the current notebook page"),
      gnc_main_window_file_close_cb,
      NULL,
      NULL,
      GNOME_APP_PIXMAP_STOCK, 
      GNOME_STOCK_PIXMAP_CLOSE,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_END
  };

  return prefix;
}

static GnomeUIInfo *
gnc_main_window_toolbar_suffix (void)
{
  static GnomeUIInfo suffix[] = 
  {
    GNOMEUIINFO_SEPARATOR,
    { GNOME_APP_UI_ITEM,
      N_("Exit"),
      N_("Exit GnuCash"),
      gnc_main_window_exit_cb, 
      NULL,
      NULL,
      GNOME_APP_PIXMAP_STOCK,
      GNOME_STOCK_PIXMAP_QUIT,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  return suffix;
}
