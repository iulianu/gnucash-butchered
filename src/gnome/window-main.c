/********************************************************************\
 * window-main.c -- the main window, and associated helper functions* 
 *                  and callback functions for GnuCash              *
 * Copyright (C) 1998,1999 Jeremy Collins	                    *
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
#include <guile/gh.h>
#include <string.h>

#include "AccWindow.h"
#include "AdjBWindow.h"
#include "global-options.h"
#include "FileDialog.h"
#include "g-wrap.h"
#include "gnucash.h"
#include "MainWindow.h"
#include "Destroy.h"
#include "enriched-messages.h"
#include "RegWindow.h"
#include "Refresh.h"
#include "window-main.h"
#include "window-mainP.h"
#include "window-reconcile.h"
#include "window-register.h"
#include "window-help.h"
#include "account-tree.h"
#include "dialog-transfer.h"
#include "dialog-edit.h"
#include "Scrub.h"
#include "util.h"
#include "gnc.h"


/* This static indicates the debugging module that this .o belongs to.  */
static short module = MOD_GUI;

/* Codes for the file menu */
enum {
  FMB_NEW,
  FMB_OPEN,
  FMB_IMPORT,
  FMB_SAVE,
  FMB_SAVEAS,
  FMB_QUIT,
};


static void
gnc_ui_refresh_statusbar()
{
  GtkWidget *label;
  double assets  = 0.0;
  double profits = 0.0;
  AccountGroup *group;
  AccountGroup *children;
  Account *account;
  char *asset_string;
  char *profit_string;
  char *label_string;
  int num_accounts;
  int account_type;
  int i;

  group = gncGetCurrentGroup ();
  num_accounts = xaccGroupGetNumAccounts(group);
  for (i = 0; i < num_accounts; i++)
  {
    account = xaccGroupGetAccount(group, i);
 
    account_type = xaccAccountGetType(account);
    children = xaccAccountGetChildren(account);

    switch (account_type)
    {
      case BANK:
      case CASH:
      case ASSET:
      case STOCK:
      case MUTUAL:
      case CREDIT:
      case LIABILITY:
	assets += xaccAccountGetBalance(account);
	if (children != NULL)
	  assets += xaccGroupGetBalance(children);
	break;
      case INCOME:
      case EXPENSE:
	profits -= xaccAccountGetBalance(account); /* flip the sign !! */
	if (children != NULL)
	  profits -= xaccGroupGetBalance(children); /* flip the sign !! */
	break;
      case EQUITY:
      default:
	break;
    }
  }
  
  asset_string = g_strdup(xaccPrintAmount(assets, PRTSYM | PRTSEP));
  profit_string = g_strdup(xaccPrintAmount(profits, PRTSYM | PRTSEP));

  label_string = g_strconcat(ASSETS_STR,  ": ", asset_string, "   ",
                             PROFITS_STR, ": ", profit_string, "  ",
                             NULL);
   
  label = gtk_object_get_data(GTK_OBJECT(gnc_get_ui_data()),
			      "balance_label");

  gtk_label_set_text(GTK_LABEL(label), label_string);

  g_free(asset_string);
  g_free(profit_string);
  g_free(label_string);
}

/* Required for compatibility with Motif code. */
void
gnc_refresh_main_window()
{
  xaccRecomputeGroupBalance(gncGetCurrentGroup());
  gnc_ui_refresh_statusbar();
  gnc_account_tree_refresh(gnc_get_current_account_tree());
}

static void
gnc_ui_exit_cb ( GtkWidget *widget, gpointer data )
{
  gnc_shutdown(0);
}

static void
gnc_ui_about_cb (GtkWidget *widget, gpointer data)
{
  GtkWidget *about;
  const gchar *copyright = "(C) 1998-1999 Linas Vepstas";
  const gchar *authors[] = {
    "Linas Vepstas <linas@linas.org>",
    NULL
  };

  about = gnome_about_new("GnuCash", VERSION, copyright,
                          authors, ABOUT_MSG, NULL);

  gnome_dialog_run_and_close(GNOME_DIALOG(about));
}

static void
gnc_ui_help_cb ( GtkWidget *widget, gpointer data )
{
  helpWindow(NULL, HELP_STR, HH_MAIN);
}

static void
gnc_ui_add_account ( GtkWidget *widget, gpointer data )
{
  accWindow(NULL);
}

static void
gnc_ui_delete_account ( Account *account )
{
  /* Step 1: Remove the account from the tree */
  gnc_account_tree_remove_account(gnc_get_current_account_tree(), account);

  /* Step 2: Delete associated windows */
  xaccAccountWindowDestroy(account);

  /* Step 3: Delete the actual account */  
  xaccRemoveAccount(account);
  xaccFreeAccount(account);

  /* Step 4: Refresh things */
  gnc_ui_refresh_statusbar();
  gnc_group_ui_refresh(gncGetCurrentGroup());
}

static void
gnc_ui_delete_account_cb ( GtkWidget *widget, gpointer data )
{
  Account *account = gnc_get_current_account();

  if (account)
  {
    gchar *name;
    gchar *message;

    name = xaccAccountGetName(account);
    message = g_strdup_printf(ACC_DEL_SURE_MSG, name);

    if (gnc_verify_dialog(message, GNC_F))
      gnc_ui_delete_account(account);

    g_free(message);
  }
  else
    gnc_error_dialog(ACC_DEL_MSG);
}

static void
gnc_ui_mainWindow_toolbar_open ( GtkWidget *widget, gpointer data )
{
  RegWindow *regData;
  Account *account = gnc_get_current_account();
  
  if (account == NULL)
  {
    gnc_error_dialog(ACC_OPEN_MSG);
    return;
  }

  PINFO ("calling regWindowSimple(%p)\n", account);

  regData = regWindowSimple(account);
  gnc_register_raise(regData);
}

static void
gnc_ui_mainWindow_toolbar_open_subs(GtkWidget *widget, gpointer data)
{
  RegWindow *regData;
  Account *account = gnc_get_current_account();
  
  if (account == NULL)
  {
    gnc_error_dialog(ACC_OPEN_MSG);
    return;
  }

  PINFO ("calling regWindowAccGroup(%p)\n", account);

  regData = regWindowAccGroup(account);
  gnc_register_raise(regData);
}

static void
gnc_ui_mainWindow_toolbar_edit ( GtkWidget *widget, gpointer data )
{
  Account *account = gnc_get_current_account();
  EditAccWindow *edit_window_data;
  
  if (account != NULL)
  {
    edit_window_data = editAccWindow(account);
    gnc_ui_edit_account_window_raise(edit_window_data);
  }
  else
    gnc_error_dialog(ACC_EDIT_MSG);
}

static void
gnc_ui_mainWindow_reconcile(GtkWidget *widget, gpointer data)
{
  Account *account = gnc_get_current_account();
  RecnWindow *recnData;

  if (account != NULL)
  {
    recnData = recnWindow(gnc_get_ui_data(), account);
    gnc_ui_reconcile_window_raise(recnData);
  }
  else
    gnc_error_dialog(ACC_RECONCILE_MSG);
}

static void
gnc_ui_mainWindow_transfer(GtkWidget *widget, gpointer data)
{
  gnc_xfer_dialog(gnc_get_ui_data(), gnc_get_current_account());
}

static void
gnc_ui_mainWindow_adjust_balance(GtkWidget *widget, gpointer data)
{
  Account *account = gnc_get_current_account();

  if (account != NULL)
    adjBWindow(account);
  else
    gnc_error_dialog(ACC_ADJUST_MSG);
}

static void
gnc_ui_mainWindow_scrub(GtkWidget *widget, gpointer data)
{
  Account *account = gnc_get_current_account();

  if (account == NULL)
  {
    gnc_error_dialog(ACC_SCRUB_MSG);
    return;
  }

  xaccAccountScrubOrphans(account);
  xaccAccountScrubImbalance(account);

  gnc_account_ui_refresh(account);
  gnc_refresh_main_window();
}

static void
gnc_ui_mainWindow_scrub_sub(GtkWidget *widget, gpointer data)
{
  Account *account = gnc_get_current_account();

  if (account == NULL)
  {
    gnc_error_dialog(ACC_SCRUB_MSG);
    return;
  }

  xaccAccountTreeScrubOrphans(account);
  xaccAccountTreeScrubImbalance(account);

  gnc_account_ui_refresh(account);
  gnc_refresh_main_window();
}

static void
gnc_ui_mainWindow_scrub_all(GtkWidget *widget, gpointer data)
{
  AccountGroup *group = gncGetCurrentGroup();

  xaccGroupScrubOrphans(group);
  xaccGroupScrubImbalance(group);

  gnc_group_ui_refresh(group);
  gnc_refresh_main_window();
}

static void
gnc_ui_options_cb(GtkWidget *widget, gpointer data)
{
  gnc_show_options_dialog();
}

static void
gnc_ui_filemenu_cb(GtkWidget *widget, gpointer menuItem)
{
  switch(GPOINTER_TO_INT(menuItem))
  {
    case FMB_NEW:
      gncFileNew();
      gnc_refresh_main_window();
      break;
    case FMB_OPEN:
      gncFileOpen();
      gnc_refresh_main_window();
      break;
    case FMB_SAVE:
      gncFileSave();
      break;
    case FMB_SAVEAS:
      gncFileSaveAs();
      break;
    case FMB_IMPORT:
      gncFileQIFImport();
      gnc_refresh_main_window();
      break;
    case FMB_QUIT:
      gnc_shutdown(0);
      break;
    default:
      break;  
  }  
}

static gboolean
gnc_ui_mainWindow_delete_cb(GtkWidget       *widget,
			    GdkEvent        *event,
			    gpointer         user_data)
{
  /* Don't allow deletes if we're in a modal dialog */
  if (gtk_main_level() == 1)
    gnc_shutdown(0);

  /* Don't delete the window, we'll handle things ourselves. */
  return TRUE;
}


static gboolean
gnc_ui_mainWindow_destroy_cb(GtkWidget       *widget,
			     GdkEvent        *event,
			     gpointer         user_data)
{
  return FALSE;
}

GNCAccountTree *
gnc_get_current_account_tree()
{
  GtkWidget *widget;

  widget = gnc_get_ui_data();
  if (widget == NULL)
    return NULL;

  return gtk_object_get_data(GTK_OBJECT(widget), "account_tree");
}

Account *
gnc_get_current_account()
{
  GNCAccountTree * tree = gnc_get_current_account_tree();
  return gnc_account_tree_get_current_account(tree);
}

GList *
gnc_get_current_accounts()
{
  GNCAccountTree * tree = gnc_get_current_account_tree();
  return gnc_account_tree_get_current_accounts(tree);
}

static void
gnc_account_tree_activate_cb(GNCAccountTree *tree,
                             Account *account,
                             gpointer user_data)
{
  RegWindow *regData;

  regData = regWindowSimple(account);
  gnc_register_raise(regData);
}

static void
gnc_configure_account_tree(gpointer data)
{
  GtkObject *app;
  GNCAccountTree *tree;
  AccountViewInfo new_avi;
  AccountViewInfo old_avi;

  app = GTK_OBJECT(gnc_get_ui_data());
  tree = GNC_ACCOUNT_TREE(gtk_object_get_data(app, "account_tree"));

  if (tree == NULL)
    return;

  gnc_account_tree_get_view_info(tree, &old_avi);

  new_avi.include_type[BANK] =
    gnc_lookup_boolean_option("Account Types",
			      "Show bank accounts",
			      old_avi.include_type[BANK]);

  new_avi.include_type[CASH] =
    gnc_lookup_boolean_option("Account Types",
			      "Show cash accounts",
			      old_avi.include_type[CASH]);

  new_avi.include_type[CREDIT] =
    gnc_lookup_boolean_option("Account Types",
			      "Show credit accounts",
			      old_avi.include_type[CREDIT]);

  new_avi.include_type[ASSET] =
    gnc_lookup_boolean_option("Account Types",
			      "Show asset accounts",
			      old_avi.include_type[ASSET]);

  new_avi.include_type[LIABILITY] =
    gnc_lookup_boolean_option("Account Types",
			      "Show liability accounts",
			      old_avi.include_type[LIABILITY]);

  new_avi.include_type[STOCK] =
    gnc_lookup_boolean_option("Account Types",
			      "Show stock accounts",
			      old_avi.include_type[STOCK]);

  new_avi.include_type[MUTUAL] =
    gnc_lookup_boolean_option("Account Types",
			      "Show mutual fund accounts",
			      old_avi.include_type[MUTUAL]);

  new_avi.include_type[CURRENCY] =
    gnc_lookup_boolean_option("Account Types",
			      "Show currency accounts",
			      old_avi.include_type[CURRENCY]);

  new_avi.include_type[INCOME] =
    gnc_lookup_boolean_option("Account Types",
			      "Show income accounts",
			      old_avi.include_type[INCOME]);

  new_avi.include_type[EXPENSE] =
    gnc_lookup_boolean_option("Account Types",
			      "Show expense accounts",
			      old_avi.include_type[EXPENSE]);

  new_avi.include_type[EQUITY] =
    gnc_lookup_boolean_option("Account Types",
			      "Show equity accounts",
			      old_avi.include_type[EQUITY]);


  new_avi.show_field[ACCOUNT_TYPE] =
    gnc_lookup_boolean_option("Account Fields",
			      "Show account type",
			      old_avi.show_field[ACCOUNT_TYPE]);

  new_avi.show_field[ACCOUNT_NAME] =
    gnc_lookup_boolean_option("Account Fields",
			      "Show account name",
			      old_avi.show_field[ACCOUNT_NAME]);

  new_avi.show_field[ACCOUNT_CODE] =
    gnc_lookup_boolean_option("Account Fields",
			      "Show account code",
			      old_avi.show_field[ACCOUNT_CODE]);

  new_avi.show_field[ACCOUNT_DESCRIPTION] =
    gnc_lookup_boolean_option("Account Fields",
			      "Show account description",
			      old_avi.show_field[ACCOUNT_DESCRIPTION]);

  new_avi.show_field[ACCOUNT_NOTES] =
    gnc_lookup_boolean_option("Account Fields",
			      "Show account notes",
			      old_avi.show_field[ACCOUNT_NOTES]);

  new_avi.show_field[ACCOUNT_CURRENCY] =
    gnc_lookup_boolean_option("Account Fields",
			      "Show account currency",
			      old_avi.show_field[ACCOUNT_CURRENCY]);

  new_avi.show_field[ACCOUNT_SECURITY] =
    gnc_lookup_boolean_option("Account Fields",
			      "Show account security",
			      old_avi.show_field[ACCOUNT_SECURITY]);

  new_avi.show_field[ACCOUNT_BALANCE] =
    gnc_lookup_boolean_option("Account Fields",
			      "Show account balance",
			      old_avi.show_field[ACCOUNT_BALANCE]);

  if (memcmp(&old_avi, &new_avi, sizeof(AccountViewInfo)) != 0)
    gnc_account_tree_set_view_info(tree, &new_avi);
}

static void
gnc_main_create_toolbar(GnomeApp *app)
{
  GnomeUIInfo toolbar[] = 
  {
    { GNOME_APP_UI_ITEM,
      OPEN_STR,
      TOOLTIP_OPEN_FILE,
      gnc_ui_filemenu_cb, 
      GINT_TO_POINTER(FMB_OPEN),
      NULL,
      GNOME_APP_PIXMAP_STOCK, 
      GNOME_STOCK_PIXMAP_OPEN,
      0, 0, NULL
    },
    { GNOME_APP_UI_ITEM,
      SAVE_STR,
      TOOLTIP_SAVE_FILE,
      gnc_ui_filemenu_cb, 
      GINT_TO_POINTER(FMB_SAVE),
      NULL,
      GNOME_APP_PIXMAP_STOCK, 
      GNOME_STOCK_PIXMAP_SAVE,
      0, 0, NULL
    },
    { GNOME_APP_UI_ITEM,
      IMPORT_STR,
      TOOLTIP_IMPORT_QIF,
      gnc_ui_filemenu_cb, 
      GINT_TO_POINTER(FMB_IMPORT),
      NULL,
      GNOME_APP_PIXMAP_STOCK, 
      GNOME_STOCK_PIXMAP_CONVERT,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    { GNOME_APP_UI_ITEM, 
      OPEN_STR,
      TOOLTIP_OPEN,
      gnc_ui_mainWindow_toolbar_open, 
      NULL,
      NULL,
      GNOME_APP_PIXMAP_STOCK,
      GNOME_STOCK_PIXMAP_JUMP_TO,
      0, 0, NULL 
    },
    { GNOME_APP_UI_ITEM,
      EDIT_STR,
      TOOLTIP_EDIT,
      gnc_ui_mainWindow_toolbar_edit, 
      NULL,
      NULL,
      GNOME_APP_PIXMAP_STOCK,
      GNOME_STOCK_PIXMAP_PROPERTIES,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    { GNOME_APP_UI_ITEM,
      NEW_STR,
      TOOLTIP_NEW,
      gnc_ui_add_account, 
      NULL,
      NULL,
      GNOME_APP_PIXMAP_STOCK,
      GNOME_STOCK_PIXMAP_ADD,
      0, 0, NULL
    },
    { GNOME_APP_UI_ITEM,
      DELETE_STR,
      TOOLTIP_DELETE,
      gnc_ui_delete_account_cb, 
      NULL,
      NULL,
      GNOME_APP_PIXMAP_STOCK,
      GNOME_STOCK_PIXMAP_REMOVE,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    { GNOME_APP_UI_ITEM,
      EXIT_STR,
      TOOLTIP_EXIT,
      gnc_ui_exit_cb, 
      NULL,
      NULL,
      GNOME_APP_PIXMAP_STOCK,
      GNOME_STOCK_PIXMAP_QUIT,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  gnome_app_create_toolbar(app, toolbar);
}

static void
gnc_main_create_menus(GnomeApp *app, GtkWidget *account_tree)
{
  GtkWidget *popup;

  GnomeUIInfo filemenu[] = {
    GNOMEUIINFO_MENU_NEW_ITEM(NEW_FILE_STR,
                              TOOLTIP_NEW_FILE,
                              gnc_ui_filemenu_cb,
                              GINT_TO_POINTER(FMB_NEW)),
    GNOMEUIINFO_MENU_OPEN_ITEM(gnc_ui_filemenu_cb,
                               GINT_TO_POINTER(FMB_OPEN)),
    GNOMEUIINFO_MENU_SAVE_ITEM(gnc_ui_filemenu_cb,
                               GINT_TO_POINTER(FMB_SAVE)),
    GNOMEUIINFO_MENU_SAVE_AS_ITEM(gnc_ui_filemenu_cb,
                                  GINT_TO_POINTER(FMB_SAVEAS)),
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      IMPORT_QIF_E_STR, TOOLTIP_IMPORT_QIF,
      gnc_ui_filemenu_cb, GINT_TO_POINTER(FMB_IMPORT), NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CONVERT,
      'i', GDK_CONTROL_MASK, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_MENU_EXIT_ITEM(gnc_ui_filemenu_cb,
                               GINT_TO_POINTER(FMB_QUIT)),
    GNOMEUIINFO_END
  };

  GnomeUIInfo optionsmenu[] = {
    {
      GNOME_APP_UI_ITEM,
      PREFERENCES_STR, TOOLTIP_PREFERENCES,
      gnc_ui_options_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PREF,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  GnomeUIInfo scrubmenu[] = {
    {
      GNOME_APP_UI_ITEM,
      SCRUB_ACCT_STR, TOOLTIP_SCRUB_ACCT,
      gnc_ui_mainWindow_scrub, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      SCRUB_SUBACCTS_STR, TOOLTIP_SCRUB_SUB,
      gnc_ui_mainWindow_scrub_sub, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      SCRUB_ALL_STR, TOOLTIP_SCRUB_ALL,
      gnc_ui_mainWindow_scrub_all, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  GnomeUIInfo accountsmenu[] = {
    {
      GNOME_APP_UI_ITEM,
      OPEN_ACC_E_STR, TOOLTIP_OPEN,
      gnc_ui_mainWindow_toolbar_open, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN,
      'v', GDK_CONTROL_MASK, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      OPEN_SUB_E_STR, TOOLTIP_OPEN_SUB,
      gnc_ui_mainWindow_toolbar_open_subs, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      EDIT_ACCT_E_STR, TOOLTIP_EDIT,
      gnc_ui_mainWindow_toolbar_edit, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PROP,
      'e', GDK_CONTROL_MASK, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      RECONCILE_E_STR, TOOLTIP_RECONCILE,
      gnc_ui_mainWindow_reconcile, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      TRANSFER_E_STR, TOOLTIP_TRANSFER,
      gnc_ui_mainWindow_transfer, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      't', GDK_CONTROL_MASK, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      ADJ_BALN_E_STR, TOOLTIP_ADJUST,
      gnc_ui_mainWindow_adjust_balance, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      'b', GDK_CONTROL_MASK, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      NEW_ACC_E_STR, TOOLTIP_NEW,
      gnc_ui_add_account, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_ADD,
      'a', GDK_CONTROL_MASK, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      DEL_ACC_E_STR, TOOLTIP_DELETE,
      gnc_ui_delete_account_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_REMOVE,
      'r', GDK_CONTROL_MASK, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_SUBTREE(SCRUB_STR, scrubmenu),
    GNOMEUIINFO_END
  };  

  GnomeUIInfo helpmenu[] = {
    GNOMEUIINFO_MENU_ABOUT_ITEM(gnc_ui_about_cb, NULL),
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      HELP_E_STR, TOOLTIP_HELP,
      gnc_ui_help_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  GnomeUIInfo scriptsmenu[] = {
    GNOMEUIINFO_END
  };

  GnomeUIInfo mainmenu[] = {
    GNOMEUIINFO_MENU_FILE_TREE(filemenu),
    GNOMEUIINFO_SUBTREE(ACCOUNTS_STR, accountsmenu),
    GNOMEUIINFO_MENU_SETTINGS_TREE(optionsmenu),
    GNOMEUIINFO_SUBTREE(EXTENSIONS_STR, scriptsmenu),    
    GNOMEUIINFO_MENU_HELP_TREE(helpmenu),
    GNOMEUIINFO_END
  };

  gnome_app_create_menus(app, mainmenu);
  gnome_app_install_menu_hints(app, mainmenu);

  popup = gnome_popup_menu_new(accountsmenu);
  gnome_popup_menu_attach(popup, account_tree, NULL);
}

void
mainWindow()
{
  GtkWidget *app = gnc_get_ui_data();
  GtkWidget *scrolled_win;
  GtkWidget *statusbar;
  GtkWidget *account_tree;
  GtkWidget *label;

  account_tree = gnc_account_tree_new();
  gtk_object_set_data(GTK_OBJECT (app), "account_tree", account_tree);
  gnc_configure_account_tree(NULL);
  gnc_register_option_change_callback(gnc_configure_account_tree, NULL);

  gtk_signal_connect(GTK_OBJECT (account_tree), "activate_account",
		     GTK_SIGNAL_FUNC (gnc_account_tree_activate_cb), NULL);

  /* create statusbar and add it to the application. */
  statusbar = gnome_appbar_new(GNC_F, /* no progress bar, maybe later? */
			       GNC_T, /* has status area */
			       GNOME_PREFERENCES_USER /* recommended */);

  gnome_app_set_statusbar(GNOME_APP(app), GTK_WIDGET(statusbar));

  gnc_main_create_menus(GNOME_APP(app), account_tree);
  gnc_main_create_toolbar(GNOME_APP(app));

  /* create the label containing the account balances */
  label = gtk_label_new("");
  gtk_object_set_data (GTK_OBJECT (app), "balance_label", label);
  gtk_box_pack_end(GTK_BOX(statusbar), label, GNC_F, GNC_F, 0);

  /* create scrolled window */
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gnome_app_set_contents(GNOME_APP(app), scrolled_win);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
                                  GTK_POLICY_AUTOMATIC, 
                                  GTK_POLICY_AUTOMATIC);

  gtk_container_add(GTK_CONTAINER(scrolled_win), GTK_WIDGET(account_tree));

  gtk_window_set_default_size(GTK_WINDOW(app), 0, 400);

  {
    SCM run_danglers = gh_eval_str("gnc:hook-run-danglers");
    SCM hook = gh_eval_str("gnc:*main-window-opened-hook*");
    SCM window = POINTER_TOKEN_to_SCM(make_POINTER_TOKEN("gncUIWidget", app));
    gh_call2(run_danglers, hook, window); 
  }

  /* Attach delete and destroy signals to the main window */  
  gtk_signal_connect (GTK_OBJECT (app), "delete_event",
                      GTK_SIGNAL_FUNC (gnc_ui_mainWindow_delete_cb),
                      NULL);

  gtk_signal_connect (GTK_OBJECT (app), "destroy_event",
                      GTK_SIGNAL_FUNC (gnc_ui_mainWindow_destroy_cb),
                      NULL);

  /* Show everything now that it is created */
  gtk_widget_show(label);
  gtk_widget_show(statusbar);
  gtk_widget_show(account_tree);
  gtk_widget_show(scrolled_win);

  gnc_refresh_main_window();

  gtk_widget_grab_focus(account_tree);
} 

/********************* END OF FILE **********************************/
