/********************************************************************
 * window-acct-tree.c -- the main window account tree               * 
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

#include <gnome.h>
#include <guile/gh.h>
#include <string.h>

#include "AccWindow.h"
#include "Scrub.h"
#include "dialog-account.h"
#include "dialog-options.h"
#include "dialog-transfer.h"
#include "dialog-utils.h"
#include "global-options.h"
#include "gnc-account-tree.h"
#include "gnc-book.h"
#include "gnc-component-manager.h"
#include "gnc-engine-util.h"
#include "gnc-engine.h"
#include "gnc-gui-query.h"
#include "gnc-html.h"
#include "gnc-ui.h"
#include "gtkselect.h"
#include "io-gncxml-v2.h"
#include "mainwindow-account-tree.h"
#include "messages.h"
#include "option-util.h"
#include "window-acct-tree.h"
#include "window-main.h"
#include "window-reconcile.h"
#include "window-register.h"

static short module = MOD_GUI;
#define WINDOW_ACCT_TREE_CM_CLASS "window-acct-tree"


/* acct tree window information structure */
struct GNCAcctTreeWin_p 
{
  GtkWidget   * account_tree;

  SCM         euro_change_callback_id;
  SCM         name_change_callback_id;

  GNCOptionDB * odb;
  SCM         options; 
  int         options_id;
  GNCOptionWin * editor_dialog;

  GList       * account_sensitives;
};

static void gnc_acct_tree_tweak_menu (GNCMDIChildInfo * mc);


/**
 * gnc_acct_tree_window_set_sensitives
 *
 * @win: A pointer to the data structure holding all the data
 * associated with the Account Tree window.
 *
 * @sensitive: TRUE to enable the list of widgets, FALSE to disable
 * them.
 *
 * Run the list of account sensitive widgets and enable/disable all
 * the items in the list.
 */
static void
gnc_acct_tree_window_set_sensitives(GNCAcctTreeWin * win,
                                    gboolean sensitive)
{
  g_list_foreach(win->account_sensitives, (GFunc)gtk_widget_set_sensitive,
		 (gpointer)sensitive);
}


/**
 * gnc_acct_tree_window_add_sensitive
 *
 * @win: A pointer to the data structure holding all the data
 * associated with the Account Tree window.
 *
 * @widget: A pointer to a menu or toolbar item.
 *
 * Add this widget to the list of items to be enabled/disabled when an
 * account is selected in this window.
 */
static void
gnc_acct_tree_window_add_sensitive(GNCAcctTreeWin * win, GtkWidget *widget)
{
  if (widget == NULL)
    return;
  win->account_sensitives = g_list_append(win->account_sensitives, widget);
}

/**
 * gnc_acct_tree_window_find_popup_item
 *
 * @win: A pointer to the data structure holding all the data
 * associated with the Account Tree window.
 *
 * @popup: A pointer to the popup menu for this window.
 *
 * @name: The name of the menu item to find.
 *
 * This routine looks for a particular menu item in a popup menu.  If
 * found, it adds the menu to the list of items to be enabled/disabled
 * when an account is selected in this window.
 */
static void
gnc_acct_tree_window_find_popup_item(GNCAcctTreeWin * win, GtkWidget *popup,
				     gchar *name)
{
    GtkWidget *menuitem;
    gint pos;

    if (gnome_app_find_menu_pos(popup, name, &pos)) {
      menuitem = (GtkWidget*)g_list_nth_data(GTK_MENU_SHELL(popup)->children,
					     pos-1);
      gnc_acct_tree_window_add_sensitive(win, menuitem);
    }
}

/********************************************************************
 * ACCOUNT WINDOW FUNCTIONS 
 * creating/managing account-window mdi children
 ********************************************************************/

/********************************************************************
 * acct_labeler
 ********************************************************************/

static GtkWidget * 
gnc_acct_tree_view_labeler(GnomeMDIChild * child, GtkWidget * current,
                           gpointer user_data) {
  GNCMDIChildInfo * mc = gtk_object_get_user_data(GTK_OBJECT(child));
  GNCAcctTreeWin   * win = NULL;
  char             * name = NULL;

  if(mc) {
    win = mc->user_data;
    if(win) {
      name = gnc_option_db_lookup_string_option(win->odb, 
                                                "Account Tree", 
                                                "Name of account view",
                                                NULL);
    }
    else {
      name = strdup(_("Accounts"));
    }
    g_free(mc->title);
    mc->title = g_strdup(name);
  }
  else {
    name = strdup(_("Accounts"));
  }
  
  if(current == NULL) {    
    current = gtk_label_new(name); 
  }
  else {
    gtk_label_set_text(GTK_LABEL(current), name);
  }

  gtk_misc_set_alignment (GTK_MISC(current), 0.0, 0.5);

  if (name) free (name);

  return current;
}


static void
gnc_acct_tree_view_destroy(GtkObject * obj, gpointer user_data) {
  GNCMDIChildInfo * mc = user_data;
  GNCAcctTreeWin * w = mc->user_data;

  gnc_mdi_remove_child(gnc_mdi_get_current (), mc);
  gnc_unregister_gui_component (mc->component_id);
  gnc_acct_tree_window_destroy(w);
  g_free(mc->toolbar_info);
  g_free(mc->menu_info);
  g_free(mc->title);
  g_free(mc);
}


/********************************************************************
 * acct_tree_view_new
 * create a new account view.  
 ********************************************************************/

static void
gnc_acct_tree_view_refresh (gpointer data)
{
  gnc_mdi_child_refresh (data);
}

static GtkWidget *
gnc_acct_tree_view_new(GnomeMDIChild * child, gpointer user_data)
{
  GNCMDIInfo        * maininfo = user_data;
  GNCMDIChildInfo   * mc = g_new0(GNCMDIChildInfo, 1);
  GNCAcctTreeWin     * win = gnc_acct_tree_window_new(child->name);
  GtkWidget          * popup;
  char               * name;

  mc->contents     = gnc_acct_tree_window_get_widget(win);
  mc->child        = child;
  mc->app          = NULL;
  mc->toolbar      = NULL;
  mc->component_id = gnc_register_gui_component(WINDOW_ACCT_TREE_CM_CLASS,
                                                NULL, NULL, mc);
  mc->user_data    = win;
  mc->title        = g_strdup(_("Accounts"));

  mc->menu_tweaking = gnc_acct_tree_tweak_menu;

  gtk_object_set_user_data(GTK_OBJECT(child), mc);

  /* set the child name that will get used to save app state */
  name = g_strdup_printf("gnc-acct-tree:id=%d", win->options_id);
  gnome_mdi_child_set_name(mc->child, name);
  g_free (name);

  gtk_signal_connect(GTK_OBJECT(child), "destroy", 
                     gnc_acct_tree_view_destroy, mc);

  gnc_mdi_add_child (maininfo, mc);

  win->name_change_callback_id = 
    gnc_option_db_register_change_callback(win->odb, 
                                           gnc_acct_tree_view_refresh,
                                           mc, 
                                           N_("Account Tree"),
                                           N_("Name of account view"));
  scm_protect_object(win->name_change_callback_id);

  gnc_acct_tree_window_create_menu(win, mc);
  gnc_acct_tree_window_create_toolbar(win, mc);
  gnc_mdi_create_child_toolbar(maininfo, mc);

  if (mc->menu_info) {
    popup = gnc_mainwin_account_tree_attach_popup
      (GNC_MAINWIN_ACCOUNT_TREE (win->account_tree),
       mc->menu_info->moreinfo, child);
    gnc_acct_tree_window_find_popup_item(win, popup, "Open Account");
    gnc_acct_tree_window_find_popup_item(win, popup, "Open Subaccounts");
    gnc_acct_tree_window_find_popup_item(win, popup, "Edit Account");
    gnc_acct_tree_window_find_popup_item(win, popup, "Delete Account");
    gnc_acct_tree_window_find_popup_item(win, popup, "Reconcile...");
  }

  /*
   * The 'Account' menu used to be created at this point. Its
   * functionality has been integrated into the other menus.  The
   * GnomeUIInfo data structures are still used to create the popup
   * menu.
   */

  return mc->contents;
}


/********************************************************************
 * gnc_acct_tree_window_create_child()
 * return an MDI child that will create views of the specified tree 
 * (configstring is the acct tree URL)
 ********************************************************************/

GnomeMDIChild * 
gnc_acct_tree_window_create_child(const gchar * url) {
  GNCMDIInfo          * maininfo = gnc_mdi_get_current ();
  GnomeMDIGenericChild * accountchild = 
    gnome_mdi_generic_child_new(url);

  gnome_mdi_generic_child_set_label_func(accountchild, 
                                         gnc_acct_tree_view_labeler,
                                         maininfo);
  gnome_mdi_generic_child_set_view_creator(accountchild, 
                                           gnc_acct_tree_view_new,
                                           maininfo);
  return GNOME_MDI_CHILD(accountchild);
}


/********************************************************************
 * gnc_main_window_open_accounts()
 * open a top-level window with the account browser in it
 ********************************************************************/

void
gnc_main_window_open_accounts(gboolean toplevel) {
  GNCMDIInfo * maininfo = gnc_mdi_get_current ();
  GnomeMDIChild * accountchild = gnc_acct_tree_window_create_child(NULL);
  gnome_mdi_add_child(GNOME_MDI(maininfo->mdi), 
                      GNOME_MDI_CHILD(accountchild));  
  
  if(toplevel) {
    gnome_mdi_add_toplevel_view(GNOME_MDI(maininfo->mdi), 
                                GNOME_MDI_CHILD(accountchild));
  }
  else {
    gnome_mdi_add_view(GNOME_MDI(maininfo->mdi), 
                       GNOME_MDI_CHILD(accountchild));
  }
}


static void
gnc_acct_tree_window_toolbar_open_cb (GtkWidget *widget, gpointer data)
{
  RegWindow *regData;
  GNCAcctTreeWin * win = data;
  Account * account = gnc_acct_tree_window_get_current_account(win);

  if (account == NULL)
  {
    const char *message = _("To open an account, you must first\n"
                            "choose an account to open.");
    gnc_error_dialog(message);
    return;
  }

  PINFO ("calling regWindowSimple(%p)\n", account);

  regData = regWindowSimple(account);
  gnc_register_raise(regData);
}

static void
gnc_acct_tree_window_toolbar_edit_cb (GtkWidget *widget, gpointer data)
{
  GNCAcctTreeWin * win = data;
  Account        * account = gnc_acct_tree_window_get_current_account(win);

  AccountWindow *edit_window_data;
  
  if (account != NULL)
  {
    edit_window_data = gnc_ui_edit_account_window(account);
    gnc_ui_edit_account_window_raise(edit_window_data);
  }
  else
  {
    const char *message = _("To edit an account, you must first\n"
                            "choose an account to edit.\n");
    gnc_error_dialog(message);
  }
}

static void
gnc_acct_tree_window_toolbar_add_account_cb (GtkWidget *widget, gpointer data)
{
  GNCAcctTreeWin * win = data;
  Account        * account = gnc_acct_tree_window_get_current_account(win);
  gnc_ui_new_account_window_with_default (NULL, account);
}


/********************************************************************
 * delete_account_helper
 * See if this account has any splits present.  Set the user data
 * and return the same value to stop walking the account tree if
 * appropriate.
 ********************************************************************/
static gpointer
delete_account_helper (Account *account, gpointer data)
{
  gboolean *has_splits = data;

  *has_splits = (xaccAccountGetSplitList(account) != NULL);
  return (gpointer)*has_splits;
}

static void
gnc_acct_tree_window_delete_common (Account *account)
{
  if (account)
  {
    const char *no_splits_no_children =
			    _("Are you sure you want to delete the %s account?");
    const char *no_splits = _("Are you sure you want to delete the %s\n"
			      "account and all its children?");
    const char *acct_has_splits = 
			 _("This account contains transactions.  Are you sure you\n"
			   "want to delete the %s account?");
    const char *child_has_splits =
			 _("One (or more) children of this account contain\n"
			   "transactions.  Are you sure you want to delete the\n"
			   "%s account and all its children?");
    const char *format;
    char *name;

    name = xaccAccountGetFullName(account, gnc_get_account_separator ());
    if (!name)
      name = g_strdup ("");

    if (xaccAccountGetSplitList(account) != NULL) {
      format = acct_has_splits;
    } else {
      AccountGroup *children;
      gboolean has_splits = FALSE;

      children = xaccAccountGetChildren(account);
      xaccGroupForEachAccount(children, delete_account_helper, &has_splits, TRUE);
      if (has_splits) 
	format= child_has_splits;
      else
	format = children ? no_splits : no_splits_no_children;
    }

    if (gnc_verify_dialog(FALSE, format, name)) {
      gnc_suspend_gui_refresh ();
      
      xaccAccountBeginEdit (account);
      xaccAccountDestroy (account);
      
      gnc_resume_gui_refresh ();
    }
    g_free(name);
  }
  else
  {
    const char *message = _("To delete an account, you must first\n"
                            "choose an account to delete.\n");
    gnc_error_dialog(message);
  }
}

static void
gnc_acct_tree_window_toolbar_delete_account_cb (GtkWidget *widget, 
                                                gpointer data)
{
  GNCAcctTreeWin * win = data;
  Account        * account = gnc_acct_tree_window_get_current_account(win);

  gnc_acct_tree_window_delete_common (account);
}

static void
gnc_acct_tree_window_menu_open_subs_cb(GtkWidget * widget, 
                                       GnomeMDIChild * child) {
  GNCMDIChildInfo * mc = gtk_object_get_user_data(GTK_OBJECT(child));
  GNCAcctTreeWin   * win = mc->user_data;
  Account          * account = gnc_acct_tree_window_get_current_account(win);
  RegWindow        * regData;
  
  if (account == NULL) {
    const char *message = _("To open an account, you must first\n"
                            "choose an account to open.");
    gnc_error_dialog(message);
    return;
  }
  else {
    PINFO ("calling regWindowAccGroup(%p)\n", account);
    
    regData = regWindowAccGroup(account);
    gnc_register_raise(regData);
  }
}


static void
gnc_acct_tree_window_menu_edit_cb (GtkWidget * widget, 
                                   GnomeMDIChild * child) {
  GNCMDIChildInfo * mc = gtk_object_get_user_data(GTK_OBJECT(child));
  GNCAcctTreeWin   * win = mc->user_data;
  Account        * account = gnc_acct_tree_window_get_current_account(win);
  
  AccountWindow *edit_window_data;
  
  if (account != NULL)
  {
    edit_window_data = gnc_ui_edit_account_window(account);
    gnc_ui_edit_account_window_raise(edit_window_data);
  }
  else
  {
    const char *message = _("To edit an account, you must first\n"
                            "choose an account to edit.\n");
    gnc_error_dialog(message);
  }
}


static void
gnc_acct_tree_window_menu_reconcile_cb(GtkWidget * widget, 
                                       GnomeMDIChild * child) {
  GNCMDIChildInfo * mc = gtk_object_get_user_data(GTK_OBJECT(child));
  GNCAcctTreeWin   * win = mc->user_data;
  Account        * account = gnc_acct_tree_window_get_current_account(win);
  RecnWindow     * recnData;
  
  if (account != NULL)
  {
    recnData = recnWindow(gnc_ui_get_toplevel(), account);
    gnc_ui_reconcile_window_raise(recnData);
  }
  else
  {
    const char *message = _("To reconcile an account, you must first\n"
                            "choose an account to reconcile.");
    gnc_error_dialog(message);
  }
}

static void
gnc_acct_tree_window_menu_transfer_cb (GtkWidget * widget, 
                                       GnomeMDIChild * child) {
  GNCMDIChildInfo * mc = gtk_object_get_user_data(GTK_OBJECT(child));
  GNCAcctTreeWin   * win = mc->user_data;
  Account        * account = gnc_acct_tree_window_get_current_account(win);

  gnc_xfer_dialog (gnc_ui_get_toplevel (), account);
}

static void
gnc_acct_tree_window_menu_stock_split_cb (GtkWidget * widget, 
                                          GnomeMDIChild * child) {
  GNCMDIChildInfo * mc = gtk_object_get_user_data(GTK_OBJECT(child));
  GNCAcctTreeWin   * win = mc->user_data;
  Account        * account = gnc_acct_tree_window_get_current_account(win);

  gnc_stock_split_dialog (account);
}

static void
gnc_acct_tree_window_menu_add_account_cb (GtkWidget * widget, 
                                          GnomeMDIChild * child) {
  GNCMDIChildInfo * mc = gtk_object_get_user_data(GTK_OBJECT(child));
  GNCAcctTreeWin   * win = mc->user_data;
  Account        * account = gnc_acct_tree_window_get_current_account(win);
  gnc_ui_new_account_window_with_default (NULL, account);
}

static void
gnc_acct_tree_window_menu_delete_account_cb (GtkWidget *widget, 
                                             GnomeMDIChild * child) {
  GNCMDIChildInfo * mc = gtk_object_get_user_data(GTK_OBJECT(child));
  GNCAcctTreeWin   * win = mc->user_data;
  Account        * account = gnc_acct_tree_window_get_current_account(win);

  gnc_acct_tree_window_delete_common (account);
}

static void
gnc_acct_tree_window_menu_scrub_cb(GtkWidget * widget, 
                                   GnomeMDIChild * child) {
  GNCMDIChildInfo * mc = gtk_object_get_user_data(GTK_OBJECT(child));
  GNCAcctTreeWin   * win = mc->user_data;
  Account        * account = gnc_acct_tree_window_get_current_account(win);

  if (account == NULL)
  {
    const char *message = _("You must select an account to check and repair.");
    gnc_error_dialog (message);
    return;
  }

  gnc_suspend_gui_refresh ();

  xaccAccountScrubOrphans (account, gnc_get_current_book ());
  xaccAccountScrubImbalance (account, gnc_get_current_book ());

  gnc_resume_gui_refresh ();
}

static void
gnc_acct_tree_window_menu_scrub_sub_cb(GtkWidget * widget, 
                                       GnomeMDIChild * child) {
  GNCMDIChildInfo * mc = gtk_object_get_user_data(GTK_OBJECT(child));
  GNCAcctTreeWin   * win = mc->user_data;
  Account        * account = gnc_acct_tree_window_get_current_account(win);
  
  if (account == NULL)
  {
    const char *message = _("You must select an account to check and repair.");
    gnc_error_dialog(message);
    return;
  }

  gnc_suspend_gui_refresh ();

  xaccAccountTreeScrubOrphans (account, gnc_get_current_book ());
  xaccAccountTreeScrubImbalance (account, gnc_get_current_book ());

  gnc_resume_gui_refresh ();
}

static void
gnc_acct_tree_window_menu_scrub_all_cb(GtkWidget * widget, 
                                       GnomeMDIChild * child) {
  AccountGroup *group = gnc_get_current_group ();

  gnc_suspend_gui_refresh ();

  xaccGroupScrubOrphans (group, gnc_get_current_book ());
  xaccGroupScrubImbalance (group, gnc_get_current_book ());

  gnc_resume_gui_refresh ();
}

static void
gnc_acct_tree_window_menu_open_cb (GtkWidget *widget, GnomeMDIChild * child)
{
  GNCMDIChildInfo * mc = gtk_object_get_user_data(GTK_OBJECT(child));
  GNCAcctTreeWin   * win = mc->user_data;
  Account        * account = gnc_acct_tree_window_get_current_account(win);
  RegWindow      * regData;
  
  if (account == NULL) {
    const char *message = _("To open an account, you must first\n"
                            "choose an account to open.");
    gnc_error_dialog(message);
    return;
  }
  else {
    PINFO ("calling regWindowSimple(%p)\n", account);
    
    regData = regWindowSimple(account);
    gnc_register_raise(regData);
  }
}


static void
gnc_acct_tree_window_activate_cb(GNCMainWinAccountTree *tree,
                                 Account *account,
                                 gpointer user_data)
{
  GNCAcctTreeWin * win = user_data;
  RegWindow *regData;
  gboolean expand;

  expand =
    gnc_option_db_lookup_boolean_option(win->odb, 
                                        "Account Tree",
                                        "Double click expands parent accounts",
                                        FALSE);

  if (expand)
  {
    AccountGroup *group;

    group = xaccAccountGetChildren(account);
    if (xaccGroupGetNumAccounts(group) > 0)
    {
      gnc_mainwin_account_tree_toggle_account_expansion(tree, account);
      return;
    }
  }

  regData = regWindowSimple(account);
  gnc_register_raise(regData);
}

static void
gnc_acct_tree_window_configure (GNCAcctTreeWin * info) {

  GNCMainWinAccountTree *tree;
  AccountViewInfo new_avi;
  GSList *list, *node;

  memset (&new_avi, 0, sizeof(new_avi));

  tree = GNC_MAINWIN_ACCOUNT_TREE(info->account_tree);

  list = gnc_option_db_lookup_list_option(info->odb,
                                          "Account Tree",
                                          "Account types to display",
                                          NULL);

  for (node = list; node != NULL; node = node->next)
  {
    if (safe_strcmp(node->data, "bank") == 0)
      new_avi.include_type[BANK] = TRUE;

    else if (safe_strcmp(node->data, "cash") == 0)
      new_avi.include_type[CASH] = TRUE;

    else if (safe_strcmp(node->data, "credit") == 0)
      new_avi.include_type[CREDIT] = TRUE;

    else if (safe_strcmp(node->data, "asset") == 0)
      new_avi.include_type[ASSET] = TRUE;

    else if (safe_strcmp(node->data, "liability") == 0)
      new_avi.include_type[LIABILITY] = TRUE;

    else if (safe_strcmp(node->data, "stock") == 0)
      new_avi.include_type[STOCK] = TRUE;

    else if (safe_strcmp(node->data, "mutual") == 0)
      new_avi.include_type[MUTUAL] = TRUE;

    else if (safe_strcmp(node->data, "currency") == 0)
      new_avi.include_type[CURRENCY] = TRUE;

    else if (safe_strcmp(node->data, "income") == 0)
      new_avi.include_type[INCOME] = TRUE;

    else if (safe_strcmp(node->data, "expense") == 0)
      new_avi.include_type[EXPENSE] = TRUE;

    else if (safe_strcmp(node->data, "equity") == 0)
      new_avi.include_type[EQUITY] = TRUE;

    else if (safe_strcmp(node->data, "payable") == 0)
      new_avi.include_type[PAYABLE] = TRUE;

    else if (safe_strcmp(node->data, "receivable") == 0)
      new_avi.include_type[RECEIVABLE] = TRUE;
  }

  gnc_free_list_option_value (list);

  list = gnc_option_db_lookup_list_option(info->odb, 
                                          "Account Tree",
                                          "Account fields to display",
                                          NULL);

  for (node = list; node != NULL; node = node->next)
  {
    if (safe_strcmp(node->data, "type") == 0)
      new_avi.show_field[ACCOUNT_TYPE] = TRUE;

    else if (safe_strcmp(node->data, "code") == 0)
      new_avi.show_field[ACCOUNT_CODE] = TRUE;

    else if (safe_strcmp(node->data, "description") == 0)
      new_avi.show_field[ACCOUNT_DESCRIPTION] = TRUE;

    else if (safe_strcmp(node->data, "notes") == 0)
      new_avi.show_field[ACCOUNT_NOTES] = TRUE;

    else if (safe_strcmp(node->data, "commodity") == 0)
      new_avi.show_field[ACCOUNT_COMMODITY] = TRUE;

    else if (safe_strcmp(node->data, "tax-info") == 0)
      new_avi.show_field[ACCOUNT_TAX_INFO] = TRUE;

    else if (safe_strcmp(node->data, "balance") == 0)
    {
      new_avi.show_field[ACCOUNT_BALANCE] = TRUE;
      if(gnc_lookup_boolean_option("International",
                                   "Enable EURO support", FALSE))
	new_avi.show_field[ACCOUNT_BALANCE_EURO] = TRUE;
    }

    else if (safe_strcmp(node->data, "total") == 0)
    {
      new_avi.show_field[ACCOUNT_TOTAL] = TRUE;
      if(gnc_lookup_boolean_option("International",
                                   "Enable EURO support", FALSE))
	new_avi.show_field[ACCOUNT_TOTAL_EURO] = TRUE;
    }
  }

  gnc_free_list_option_value (list);

  new_avi.show_field[ACCOUNT_NAME] = TRUE;

  gnc_mainwin_account_tree_set_view_info (tree, new_avi);
}

static void
gnc_euro_change (gpointer data)
{
  gnc_acct_tree_window_configure (data);
  gnc_gui_refresh_all ();
}

static void gnc_acct_tree_window_toolbar_options_cb(GtkWidget * w, gpointer d);

void
gnc_acct_tree_window_create_toolbar(GNCAcctTreeWin * win, 
                                    GNCMDIChildInfo * child)
{
  GnomeUIInfo toolbar_template[] = 
  {
    { GNOME_APP_UI_ITEM, 
      N_("Open"),
      N_("Open the selected account"),
      gnc_acct_tree_window_toolbar_open_cb, 
      win,
      NULL,
      GNOME_APP_PIXMAP_STOCK,
      GNOME_STOCK_PIXMAP_JUMP_TO,
      0, 0, NULL 
    },
    { GNOME_APP_UI_ITEM,
      N_("Edit"),
      N_("Edit the selected account"),
      gnc_acct_tree_window_toolbar_edit_cb, 
      win,
      NULL,
      GNOME_APP_PIXMAP_STOCK,
      GNOME_STOCK_PIXMAP_PROPERTIES,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    { GNOME_APP_UI_ITEM,
      N_("Options"),
      N_("Edit the account view options"),
      gnc_acct_tree_window_toolbar_options_cb, 
      win,
      NULL,
      GNOME_APP_PIXMAP_STOCK,
      GNOME_STOCK_PIXMAP_PROPERTIES,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    { GNOME_APP_UI_ITEM,
      N_("New"),
      N_("Create a new account"),
      gnc_acct_tree_window_toolbar_add_account_cb, 
      win,
      NULL,
      GNOME_APP_PIXMAP_STOCK,
      GNOME_STOCK_PIXMAP_ADD,
      0, 0, NULL
    },
    { GNOME_APP_UI_ITEM,
      N_("Delete"),
      N_("Delete selected account"),
      gnc_acct_tree_window_toolbar_delete_account_cb, 
      win,
      NULL,
      GNOME_APP_PIXMAP_STOCK,
      GNOME_STOCK_PIXMAP_REMOVE,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  child->toolbar_info = g_memdup (toolbar_template, sizeof(toolbar_template));
}

/*
 * The scrub menu is shared by both the code to insert items into the
 * main menus, and the code to create the right click popup menu.
 */
static GnomeUIInfo scrubmenu[] =
{
  {
    GNOME_APP_UI_ITEM,
    N_("Check & Repair A_ccount"),
    N_("Check for and repair unbalanced transactions and orphan splits "
       "in this account"),
    gnc_acct_tree_window_menu_scrub_cb, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_ITEM,
    N_("Check & Repair Su_baccounts"),
    N_("Check for and repair unbalanced transactions and orphan splits "
       "in this account and its subaccounts"),
    gnc_acct_tree_window_menu_scrub_sub_cb, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_ITEM,
    N_("Check & Repair A_ll"),
    N_("Check for and repair unbalanced transactions and orphan splits "
       "in all accounts"),
    gnc_acct_tree_window_menu_scrub_all_cb, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  GNOMEUIINFO_END
};

/**
 * gnc_acct_tree_window_create_menu
 *
 * @main_info: A pointer to the data structure holding all the data
 * associated with the Account Tree window.
 *
 * @child: A pointer to the GNC MDI child associated with the Account
 * Tree window.
 *
 * This routine creates the menu for the right-click popup menu in the
 * account tree window.  This same menu was also previously inserted
 * into the menu bar and available there.  These menu items are now
 * separately incorporated into the menus.  See
 * gnc_acct_tree_tweak_menu().
 */
void
gnc_acct_tree_window_create_menu(GNCAcctTreeWin * main_info,
				 GNCMDIChildInfo * child)
{
  GnomeUIInfo * dup_scrub = g_memdup(scrubmenu, sizeof(scrubmenu));
  
  GnomeUIInfo accountsmenu[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("Open Account"),
      N_("Open the selected account"),
      gnc_acct_tree_window_menu_open_cb, child->child, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN,
      'o', GDK_CONTROL_MASK, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Open _Subaccounts"),
      N_("Open the selected account and all its subaccounts"),
      gnc_acct_tree_window_menu_open_subs_cb, child->child, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Edit Account"),
      N_("Edit the selected account"),
      gnc_acct_tree_window_menu_edit_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PROP,
      'e', GDK_CONTROL_MASK, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("_Reconcile..."),
      N_("Reconcile the selected account"),
      gnc_acct_tree_window_menu_reconcile_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      'r', GDK_CONTROL_MASK, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Transfer..."),
      N_("Transfer funds from one account to another"),
      gnc_acct_tree_window_menu_transfer_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      't', GDK_CONTROL_MASK, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Stock S_plit..."),
      N_("Record a stock split or a stock merger"),
      gnc_acct_tree_window_menu_stock_split_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("_New Account..."),
      N_("Create a new account"),
      gnc_acct_tree_window_menu_add_account_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_ADD,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Delete Account"),
      N_("Delete selected account"),
      gnc_acct_tree_window_menu_delete_account_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_REMOVE,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_SUBTREE(N_("_Check & Repair"), dup_scrub),
    GNOMEUIINFO_END
  };
  GnomeUIInfo * dup_accts = g_memdup(accountsmenu, sizeof(accountsmenu));

  GnomeUIInfo menucontainer[] = 
  {
    GNOMEUIINFO_SUBTREE(N_("_Account"), dup_accts),
    GNOMEUIINFO_END
  };
  
  child->menu_info = g_memdup(menucontainer, sizeof(menucontainer));
}


static void
gnc_acct_tree_window_select_cb(GNCMainWinAccountTree *tree, 
                               Account *account, 
                               GNCAcctTreeWin * win) {
  gboolean sensitive;
  
  account = gnc_mainwin_account_tree_get_current_account(tree);
  sensitive = (account != NULL);
  
  gnc_acct_tree_window_set_sensitives(win, sensitive);
}


Account * 
gnc_acct_tree_window_get_current_account(GNCAcctTreeWin * win) {
  return gnc_mainwin_account_tree_get_current_account
    (GNC_MAINWIN_ACCOUNT_TREE(win->account_tree));
}


static void
gnc_acct_tree_window_options_new(GNCAcctTreeWin * win) {
  SCM func = gh_eval_str("gnc:make-new-acct-tree-window");
  SCM opts_and_id = gh_call0(func);
  
  scm_unprotect_object(win->options);
  win->options = gh_car(opts_and_id);
  scm_protect_object(win->options);
  win->options_id = gh_scm2int(gh_cdr(opts_and_id));
}

void
gnc_acct_tree_window_destroy(GNCAcctTreeWin * win) {
  SCM  free_tree = gh_eval_str("gnc:free-acct-tree-window");
  gnc_unregister_option_change_callback_id
    (win->euro_change_callback_id);
  
  if(win->editor_dialog) {
    gnc_options_dialog_destroy(win->editor_dialog);
    win->editor_dialog = NULL;
  }
  
  g_list_free(win->account_sensitives);
  win->account_sensitives = NULL;
  
  gnc_option_db_destroy(win->odb);

  gh_call1(free_tree, gh_int2scm(win->options_id));

  scm_unprotect_object(win->options);
  g_free (win);
}


GNCAcctTreeWin *
gnc_acct_tree_window_new(const gchar * url)  {
  GNCAcctTreeWin * treewin = g_new0(GNCAcctTreeWin, 1);
  SCM find_options = gh_eval_str("gnc:find-acct-tree-window-options");
  SCM temp;
  int options_id;
  URLType type;

  treewin->euro_change_callback_id =
    gnc_register_option_change_callback(gnc_euro_change, treewin,
                                        "International",
                                        "Enable EURO support");
  treewin->account_tree = gnc_mainwin_account_tree_new();
  treewin->options = SCM_BOOL_F;
  scm_protect_object(treewin->options);
  treewin->editor_dialog = NULL;

  /* get the options and the window ID */ 
  if(!url) {
    gnc_acct_tree_window_options_new(treewin);
  }
  else {
    char * location = NULL;
    char * label = NULL;

    /* if an URL is specified, it should look like 
     * gnc-acct-tree:id=17 .  We want to get the number out,
     * then look up the options in the global DB. */
    type = gnc_html_parse_url(NULL, url, &location, &label);
    if((type == URL_TYPE_ACCTTREE) &&
       location && (strlen(location) > 3) && 
       !strncmp("id=", location, 3)) {
      sscanf(location+3, "%d", &options_id);
      temp = gh_call1(find_options, gh_int2scm(options_id));

      if(temp != SCM_BOOL_F) {
        scm_unprotect_object(treewin->options);
        treewin->options = temp;
        scm_protect_object(treewin->options);
        treewin->options_id = options_id;
      }
      else {
        gnc_acct_tree_window_options_new(treewin);
      }
    }
    else {
      gnc_acct_tree_window_options_new(treewin);
    }

    g_free (location);
    g_free (label);
  }

  treewin->odb     = gnc_option_db_new(treewin->options);
  
  gtk_signal_connect(GTK_OBJECT(treewin->account_tree), "activate_account",
		     GTK_SIGNAL_FUNC (gnc_acct_tree_window_activate_cb), 
                     treewin);

  gtk_signal_connect(GTK_OBJECT(treewin->account_tree), "select_account",
                     GTK_SIGNAL_FUNC(gnc_acct_tree_window_select_cb), 
                     treewin);

  gtk_signal_connect(GTK_OBJECT(treewin->account_tree), "unselect_account",
                     GTK_SIGNAL_FUNC(gnc_acct_tree_window_select_cb), 
                     treewin);
  
  /* Show everything now that it is created */
  gtk_widget_show (treewin->account_tree);

  gnc_acct_tree_window_configure (treewin);

  /* gnc_refresh_main_window (); */
  gnc_account_tree_refresh 
    (GNC_MAINWIN_ACCOUNT_TREE (treewin->account_tree)->acc_tree);

  gnc_acct_tree_window_set_sensitives(treewin, FALSE); 

  gtk_widget_grab_focus(treewin->account_tree);
  return treewin;
} 

GtkWidget * 
gnc_acct_tree_window_get_widget(GNCAcctTreeWin * win) {
  return win->account_tree;
}

SCM 
gnc_acct_tree_window_get_options(GNCAcctTreeWin * w) {
  return w->options;
}

int
gnc_acct_tree_window_get_id(GNCAcctTreeWin * w) {
  return w->options_id;
}

/********************************************************************
 * parameter editor handling 
 ********************************************************************/

static void
gnc_options_dialog_apply_cb(GNCOptionWin * propertybox,
                            gpointer user_data) {
  GNCAcctTreeWin * win = user_data;
  if(!win) return;

  gnc_option_db_commit(win->odb);
  gnc_acct_tree_window_configure(win);
}

static void
gnc_options_dialog_help_cb(GNCOptionWin * propertybox,
                           gpointer user_data) {
  gnome_ok_dialog("Set the account tree options you want using this dialog.");
}

static void
gnc_options_dialog_close_cb(GNCOptionWin * propertybox,
                            gpointer user_data) {
  GNCAcctTreeWin * win = user_data; 
  gnc_options_dialog_destroy(win->editor_dialog);
  win->editor_dialog = NULL;
}


void
gnc_acct_tree_window_toolbar_options_cb(GtkWidget * widget, gpointer data) {
  GNCAcctTreeWin * win = data;

  if(win->editor_dialog) {
    gtk_window_present(GTK_WINDOW
                     (gnc_options_dialog_widget(win->editor_dialog)));
  }
  else {
    win->editor_dialog = gnc_options_dialog_new(TRUE, NULL);
    gnc_build_options_dialog_contents(win->editor_dialog, 
                                      win->odb);
    
    gnc_options_dialog_set_apply_cb(win->editor_dialog, 
                                    gnc_options_dialog_apply_cb,
                                    (gpointer)win);
    gnc_options_dialog_set_help_cb(win->editor_dialog, 
                                   gnc_options_dialog_help_cb,
                                   (gpointer)win);
    gnc_options_dialog_set_close_cb(win->editor_dialog, 
                                    gnc_options_dialog_close_cb,
                                    (gpointer)win);    
  }
}

/**
 * gnc_acct_tree_tweak_menu
 *
 * @mc: A pointer to the GNC MDI child associated with the Account
 * Tree window.
 *
 * This routine is called when the account tree view is created and
 * shown for the first time.  It performs a variety of setup functions.
 * First, it creates menu items needed for the Account Tree window,
 * and inserts them into the main MDI application menus at the correct
 * positions.  Second, it also sets up certain main menu items to be
 * enabled/disabled whenever the account tree view is brought to the
 * top.  Third, it builds a list of widgets (menu and toolbar items)
 * to be enabled/disabled whenever an account is selected in the
 * view.
 */
static void
gnc_acct_tree_tweak_menu (GNCMDIChildInfo * mc)
{
  GNCAcctTreeWin * win;
  GtkWidget      * widget;
  GnomeUIInfo fileitems1[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("_New Account..."),
      N_("Create a new account"),
      gnc_acct_tree_window_menu_add_account_cb, mc->child, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_ADD,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };
  GnomeUIInfo fileitems2[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("_Open Account"),
      N_("Open the selected account"),
      gnc_acct_tree_window_menu_open_cb, mc->child, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN,
      'o', GDK_CONTROL_MASK, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Open S_ubaccounts"),
      N_("Open the selected account and all its subaccounts"),
      gnc_acct_tree_window_menu_open_subs_cb, mc->child, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  GnomeUIInfo edititems[] =
  {
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("_Edit Account"),
      N_("Edit the selected account"),
      gnc_acct_tree_window_menu_edit_cb, mc->child, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PROP,
      'e', GDK_CONTROL_MASK, NULL
    },
    { GNOME_APP_UI_ITEM,
      N_("_Delete Account"),
      N_("Delete selected account"),
      gnc_acct_tree_window_menu_delete_account_cb, 
      mc->child,
      NULL,
      GNOME_APP_PIXMAP_STOCK,
      GNOME_STOCK_PIXMAP_REMOVE,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  GnomeUIInfo * dup_scrub = g_memdup(scrubmenu, sizeof(scrubmenu));
  GnomeUIInfo actionitems[] =
  {
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("_Transfer..."),
      N_("Transfer funds from one account to another"),
      gnc_acct_tree_window_menu_transfer_cb, mc->child, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      't', GDK_CONTROL_MASK, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Reconcile..."),
      N_("Reconcile the selected account"),
      gnc_acct_tree_window_menu_reconcile_cb, mc->child, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      'r', GDK_CONTROL_MASK, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Stock S_plit..."),
      N_("Record a stock split or a stock merger"),
      gnc_acct_tree_window_menu_stock_split_cb, mc->child, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_SUBTREE(N_("_Check & Repair"), dup_scrub),
    GNOMEUIINFO_END
  };

  if (mc->app == NULL)
    return;

  /*
   * This window can be created multiple times, so the code needs to
   * check and see if the menus its adding are already present.  This
   * can't just be code to insure this routine is only called
   * once,because the Gnome MDI code may destroy and recreate the
   * menus and toolbars.  What a pain.
   */
  /* Do not i18n these strings!!! */
  if (gnc_mdi_child_find_menu_item(mc, "File/New Account..."))
    return;
    
  /* Do not i18n these strings!!! */
  dup_scrub[0].user_data = mc->child;
  dup_scrub[1].user_data = mc->child;
  dup_scrub[2].user_data = mc->child;
  gnome_app_insert_menus (mc->app, "File/New File", fileitems1);
  gnome_app_insert_menus (mc->app, "File/Open...", fileitems2);
  gnome_app_insert_menus (mc->app, "Edit/Paste", edititems);
  gnome_app_insert_menus (mc->app, "Actions/Scheduled Transactions",
			  actionitems);

  win = (GNCAcctTreeWin *)mc->user_data;
  gnc_acct_tree_window_add_sensitive(win, fileitems2[0].widget);
  gnc_acct_tree_window_add_sensitive(win, fileitems2[1].widget);
  gnc_acct_tree_window_add_sensitive(win, edititems[1].widget);
  gnc_acct_tree_window_add_sensitive(win, edititems[2].widget);
  gnc_acct_tree_window_add_sensitive(win, actionitems[2].widget);

  /* Do not i18n these strings!!! */
  widget = gnc_mdi_child_find_toolbar_item(mc, "Open");
  gnc_acct_tree_window_add_sensitive(win, widget);
  widget = gnc_mdi_child_find_toolbar_item(mc, "Edit");
  gnc_acct_tree_window_add_sensitive(win, widget);
  widget = gnc_mdi_child_find_toolbar_item(mc, "Delete");
  gnc_acct_tree_window_add_sensitive(win, widget);

  /* Do not i18n these strings!!! */
  gnc_mdi_child_auto_menu(mc, GNC_AUTO_DISABLE, "File/Close", NULL);
  gnc_mdi_child_auto_toolbar(mc, GNC_AUTO_DISABLE, "Close", NULL);

  /* Start with all the 'sensitives' disabled. */
  g_list_foreach(win->account_sensitives, (GFunc)gtk_widget_set_sensitive,
		 (gpointer)FALSE);
}

