/********************************************************************\
 * dialog-account.c -- window for creating and editing accounts for *
 *                     GnuCash                                      *
 * Copyright (C) 2000 Dave Peticolas <petcola@cs.ucdavis.edu>       *
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
\********************************************************************/

#include "top-level.h"

#include <string.h>
#include <gnome.h>

#include "dialog-account.h"
#include "AccWindow.h"
#include "MainWindow.h"
#include "FileDialog.h"
#include "MultiLedger.h"
#include "Refresh.h"
#include "window-main.h"
#include "dialog-utils.h"
#include "account-tree.h"
#include "global-options.h"
#include "gnc-currency-edit.h"
#include "glade-gnc-dialogs.h"
#include "ui-callbacks.h"
#include "window-help.h"
#include "query-user.h"
#include "messages.h"
#include "util.h"


typedef enum
{
  NEW_ACCOUNT,
  EDIT_ACCOUNT
} AccountDialogType;


struct _AccountWindow
{
  GtkWidget *dialog;

  AccountDialogType dialog_type;

  Account *account;
  Account *top_level_account;

  GNCAccountType type;

  GtkWidget * name_entry;
  GtkWidget * description_entry;
  GtkWidget * code_entry;
  GtkWidget * notes_text;

  GtkWidget * currency_picker;
  GtkWidget * security_picker;

  GtkWidget * type_list;

  GtkWidget * parent_tree;

  GtkWidget * source_menu;

  gint source;
};


/** Static Globals *******************************************************/
static short module = MOD_GUI;

static gint last_width = 0;
static gint last_height = 0;

static int last_used_account_type = BANK;

static gchar * default_currency = "USD";
static gboolean default_currency_dynamically_allocated = FALSE;

static GList *new_account_windows = NULL;
static AccountWindow ** editAccountList = NULL;


/** Implementation *******************************************************/

/* Copy the account values to the GUI widgets */
static void
gnc_account_to_ui(AccountWindow *aw)
{
  AccInfo *accinfo;
  InvAcct *invacct;
  const char *string;
  gint pos = 0;

  string = xaccAccountGetName (aw->account);
  gtk_entry_set_text(GTK_ENTRY(aw->name_entry), string);

  string = xaccAccountGetDescription (aw->account);
  gtk_entry_set_text(GTK_ENTRY(aw->description_entry), string);

  string = xaccAccountGetCurrency (aw->account);
  gnc_currency_edit_set_currency (GNC_CURRENCY_EDIT (aw->currency_picker),
                                  string);

  string = xaccAccountGetSecurity (aw->account);
  gtk_entry_set_text(GTK_ENTRY(aw->security_picker), string);

  string = xaccAccountGetCode (aw->account);
  gtk_entry_set_text(GTK_ENTRY(aw->code_entry), string);

  string = xaccAccountGetNotes (aw->account);
  if (string == NULL)
    string = "";
  gtk_editable_delete_text (GTK_EDITABLE (aw->notes_text), 0, -1);
  gtk_editable_insert_text (GTK_EDITABLE (aw->notes_text), string,
                            strlen(string), &pos);

  if ((STOCK != aw->type) && (MUTUAL != aw->type) && (CURRENCY != aw->type))
    return;

  accinfo = xaccAccountGetAccInfo(aw->account);
  invacct = xaccCastToInvAcct(accinfo);
  if (invacct == NULL)
    return;

  string = xaccInvAcctGetPriceSrc(invacct);

  gtk_option_menu_set_history(GTK_OPTION_MENU(aw->source_menu),
                              gnc_get_source_code(string));
}


/* Record the GUI values into the Account structure */
static void
gnc_ui_to_account(AccountWindow *aw)
{
  Account *parent_account;
  GtkEntry *entry;
  const char *old_string;
  const char *string;

  xaccAccountBeginEdit (aw->account, TRUE);

  if (aw->type != xaccAccountGetType (aw->account))
    xaccAccountSetType (aw->account, aw->type);

  string = gtk_entry_get_text (GTK_ENTRY(aw->name_entry));
  old_string = xaccAccountGetName (aw->account);
  if (safe_strcmp (string, old_string) != 0)
    xaccAccountSetName (aw->account, string);

  string = gtk_entry_get_text (GTK_ENTRY(aw->description_entry));
  old_string = xaccAccountGetDescription (aw->account);
  if (safe_strcmp (string, old_string) != 0)
    xaccAccountSetDescription (aw->account, string);

  entry = GTK_ENTRY(GTK_COMBO(aw->currency_picker)->entry);
  string = gtk_entry_get_text(entry);
  old_string = xaccAccountGetCurrency (aw->account);
  if (safe_strcmp (string, old_string) != 0)
    xaccAccountSetCurrency (aw->account, string);

  string = gtk_entry_get_text (GTK_ENTRY(aw->code_entry));
  old_string = xaccAccountGetCode (aw->account);
  if (safe_strcmp (string, old_string) != 0)
    xaccAccountSetCode (aw->account, string);

  if ((STOCK == aw->type) || (MUTUAL == aw->type) || (CURRENCY == aw->type))
  {
    AccInfo *accinfo;
    InvAcct *invacct;

    string = gtk_entry_get_text(GTK_ENTRY(aw->security_picker));
    old_string = xaccAccountGetSecurity (aw->account);
    if (safe_strcmp (string, old_string) != 0)
      xaccAccountSetSecurity (aw->account, string);

    accinfo = xaccAccountGetAccInfo(aw->account);
    invacct = xaccCastToInvAcct(accinfo);
    if (invacct != NULL)
    {
      gint code;

      code = gnc_option_menu_get_active (aw->source_menu);
      string = gnc_get_source_code_name (code);
      old_string = xaccInvAcctGetPriceSrc (invacct);
      if (safe_strcmp (string, old_string) != 0)
        xaccInvAcctSetPriceSrc(invacct, string);
    }
  }

  string = gtk_editable_get_chars (GTK_EDITABLE(aw->notes_text), 0, -1);
  old_string = xaccAccountGetNotes (aw->account);
  if (safe_strcmp (string, old_string) != 0)
    xaccAccountSetNotes (aw->account, string);

  parent_account =
    gnc_account_tree_get_current_account (GNC_ACCOUNT_TREE(aw->parent_tree));
  if (parent_account == aw->top_level_account)
    parent_account = NULL;

  xaccAccountBeginEdit (parent_account, TRUE);

  if (parent_account != NULL)
  {
    if (parent_account != xaccAccountGetParentAccount (aw->account))
      xaccInsertSubAccount (parent_account, aw->account);
  }
  else
    xaccGroupInsertAccount (gncGetCurrentGroup(), aw->account);

  xaccAccountCommitEdit (parent_account);
  xaccAccountCommitEdit (aw->account);
}


static void 
gnc_finish_ok(AccountWindow *aw)
{
  /* make the account changes */
  gnc_ui_to_account(aw);

  /* Refresh all registers so they have this account in their lists */
  gnc_group_ui_refresh(gncGetCurrentGroup());

  /* Refresh the main window. This will also refresh all account lists. */
  gnc_refresh_main_window();

  /* so it doesn't get freed on close */
  aw->account = NULL;

  gnome_dialog_close(GNOME_DIALOG(aw->dialog));
}


/* Record all of the children of the given account as needing their
 * type changed to the one specified. */
static void
gnc_edit_change_account_types(GHashTable *change_type, Account *account,
                              Account *except, GNCAccountType type)
{
  AccountGroup *children;
  int i, num_children;

  if ((change_type == NULL) || (account == NULL))
    return;

  if (account == except)
    return;

  g_hash_table_insert(change_type, account, GINT_TO_POINTER(type));

  children = xaccAccountGetChildren(account);
  if (children == NULL)
    return;

  num_children = xaccGetNumAccounts(children);
  for (i = 0; i < num_children; i++)
  {
    account = xaccGroupGetAccount(children, i);
    gnc_edit_change_account_types(change_type, account, except, type);
  }
}


/* helper function to perform changes to accounts */
static void
change_func(gpointer key, gpointer value, gpointer field_code)
{
  Account *account = key;
  AccountFieldCode field = GPOINTER_TO_INT(field_code);

  if (account == NULL)
    return;

  xaccAccountBeginEdit(account, TRUE);

  switch (field)
  {
    case ACCOUNT_CURRENCY:
      {
        char * string = value;

        xaccAccountSetCurrency(account, string);
      }
      break;
    case ACCOUNT_SECURITY:
      {
        char * string = value;

        xaccAccountSetSecurity(account, string);
      }
      break;
    case ACCOUNT_TYPE:
      {
        int type = GPOINTER_TO_INT(value);

        if (type == xaccAccountGetType(account))
          break;

        /* Just refreshing won't work. */
        xaccDestroyLedgerDisplay(account);

        xaccAccountSetType(account, type);
      }
      break;
    default:
      g_warning("unexpected account field code");
      break;
  }

  xaccAccountCommitEdit(account);
}


/* Perform the changes to accounts dictated by the hash tables */
static void
make_account_changes(GHashTable *change_currency,
                     GHashTable *change_security,
                     GHashTable *change_type)
{
  if (change_currency != NULL)
    g_hash_table_foreach(change_currency, change_func,
                         GINT_TO_POINTER(ACCOUNT_CURRENCY));

  if (change_security != NULL)
    g_hash_table_foreach(change_security, change_func,
                         GINT_TO_POINTER(ACCOUNT_SECURITY));

  if (change_type != NULL)
    g_hash_table_foreach(change_type, change_func,
                         GINT_TO_POINTER(ACCOUNT_TYPE));
}


/* Determine which accounts must have their currency and/or
 * security changed in order to keep things kosher when the
 * given account is changed to have the given currency and
 * security. The changes needed are recorded in the hash
 * tables. */
static void
gnc_account_change_currency_security(Account *account,
                                     GHashTable *change_currency,
                                     GHashTable *change_security,
                                     const char *currency,
                                     const char *security)
{
  const char *old_currency;
  const char *old_security;
  gboolean new_currency;
  gboolean new_security;
  GSList *stack;

  if (account == NULL)
    return;

  old_currency = xaccAccountGetCurrency(account);
  old_security = xaccAccountGetSecurity(account);

  if ((safe_strcmp(currency, old_currency) == 0) &&
      (safe_strcmp(security, old_security) == 0))
    return;

  if (safe_strcmp(currency, old_currency) != 0)
  {
    g_hash_table_insert(change_currency, account, (char *) currency);
    new_currency = TRUE;
  }
  else
    new_currency = FALSE;

  if (safe_strcmp(security, old_security) != 0)
  {
    g_hash_table_insert(change_security, account, (char *) security);
    new_security = TRUE;
  }
  else
    new_security = FALSE;

  stack = g_slist_prepend(NULL, account);

  while (stack != NULL)
  {
    Split *split;
    GSList *pop;
    gint i;

    pop = stack;
    account = pop->data;
    stack = g_slist_remove_link(stack, pop);
    g_slist_free_1(pop);

    i = 0;
    while ((split = xaccAccountGetSplit(account, i++)) != NULL)
    {
      Transaction *trans;
      Split *s;
      gint j;

      trans = xaccSplitGetParent(split);
      if (trans == NULL)
        continue;

      if (xaccTransIsCommonExclSCurrency(trans, currency, split))
        continue;

      if (xaccTransIsCommonExclSCurrency(trans, security, split))
        continue;

      j = 0;
      while ((s = xaccTransGetSplit(trans, j++)) != NULL)
      {
        gboolean add_it = FALSE;
        const char *commodity;
        Account *a;

        a = xaccSplitGetAccount(s);

        if ((a == NULL) || (a == account))
          continue;

        if (g_hash_table_lookup(change_currency, a) != NULL)
          continue;

        if (g_hash_table_lookup(change_security, a) != NULL)
          continue;

        commodity = xaccAccountGetCurrency(a);

        if (new_currency && (safe_strcmp(old_currency, commodity) == 0))
        {
          g_hash_table_insert(change_currency, a, (gpointer) currency);
          add_it = TRUE;
        }

        if (new_security && (safe_strcmp(old_security, commodity) == 0))
        {
          g_hash_table_insert(change_currency, a, (gpointer) security);
          add_it = TRUE;
        }

        commodity = xaccAccountGetSecurity(a);

        if (new_security && (safe_strcmp(old_security, commodity) == 0))
        {
          g_hash_table_insert(change_security, a, (gpointer) security);
          add_it = TRUE;
        }

        if (new_currency && (safe_strcmp(old_currency, commodity) == 0))
        {
          g_hash_table_insert(change_security, a, (gpointer) currency);
          add_it = TRUE;
        }

        if (add_it)
          stack = g_slist_prepend(stack, a);
      }
    }
  }
}

typedef struct
{
  Account *account;
  AccountFieldCode field;
  GtkCList *list;
  guint count;
} FillStruct;

static void
fill_helper(gpointer key, gpointer value, gpointer data)
{
  Account *account = key;
  FillStruct *fs = data;
  gchar *strings[5];

  if (fs == NULL)
    return;

  if (fs->account == account)
    return;

  strings[0] = xaccAccountGetFullName(account, gnc_get_account_separator());
  strings[1] = (gchar *) gnc_ui_get_account_field_name(fs->field);
  strings[2] = (gchar *) gnc_ui_get_account_field_value_string(account,
                                                               fs->field);
  strings[4] = NULL;

  switch (fs->field)
  {
    case ACCOUNT_CURRENCY:
    case ACCOUNT_SECURITY:
      strings[3] = value;
      break;
    case ACCOUNT_TYPE:
      strings[3] = xaccAccountGetTypeStr(GPOINTER_TO_INT(value));
      break;
    default:
      g_warning("unexpected field type");
      free(strings[0]);
      return;
  }

  gtk_clist_append(fs->list, strings);
  free(strings[0]);
  fs->count++;
}

static guint
fill_list(Account *account, GtkCList *list,
          GHashTable *change, AccountFieldCode field)
{
  FillStruct fs;

  if (change == NULL)
    return 0;

  fs.account = account;
  fs.field = field;
  fs.list = list;
  fs.count = 0;

  g_hash_table_foreach(change, fill_helper, &fs);

  return fs.count;
}


/* Present a dialog of proposed account changes for the user's ok */
static gboolean
extra_change_verify(AccountWindow *aw,
                    GHashTable *change_currency,
                    GHashTable *change_security,
                    GHashTable *change_type)
{
  Account *account;
  GtkCList *list;
  gchar *titles[5];
  gboolean result;
  guint size;

  if (aw == NULL)
    return FALSE;

  account = aw->account;

  titles[0] = ACCOUNT_STR;
  titles[1] = FIELD_STR;
  titles[2] = OLD_VALUE_STR;
  titles[3] = NEW_VALUE_STR;
  titles[4] = NULL;

  list = GTK_CLIST(gtk_clist_new_with_titles(4, titles));

  size = 0;
  size += fill_list(account, list, change_currency, ACCOUNT_CURRENCY);
  size += fill_list(account, list, change_security, ACCOUNT_SECURITY);
  size += fill_list(account, list, change_type, ACCOUNT_TYPE);

  if (size == 0)
  {
    gtk_widget_destroy(GTK_WIDGET(list));
    return TRUE;
  }

  gtk_clist_column_titles_passive(list);
  gtk_clist_set_sort_column(list, 0);
  gtk_clist_sort(list);
  gtk_clist_columns_autosize(list);

  {
    GtkWidget *dialog;
    GtkWidget *scroll;
    GtkWidget *label;
    GtkWidget *frame;
    GtkWidget *vbox;

    dialog = gnome_dialog_new(VERIFY_CHANGES_STR,
                              GNOME_STOCK_BUTTON_OK,
                              GNOME_STOCK_BUTTON_CANCEL,
                              NULL);

    gnome_dialog_set_default(GNOME_DIALOG(dialog), 0);
    gnome_dialog_set_close(GNOME_DIALOG(dialog), FALSE);
    gnome_dialog_close_hides(GNOME_DIALOG(dialog), FALSE);
    gnome_dialog_set_parent(GNOME_DIALOG(dialog), GTK_WINDOW(aw->dialog));
    gtk_window_set_policy(GTK_WINDOW(dialog), TRUE, TRUE, TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 0, 300);

    vbox = GNOME_DIALOG(dialog)->vbox;

    label = gtk_label_new(VERIFY_CHANGE_MSG);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    frame = gtk_frame_new(NULL);
    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, 
                                   GTK_POLICY_AUTOMATIC);

    gtk_container_add(GTK_CONTAINER(frame), scroll);
    gtk_container_border_width(GTK_CONTAINER(scroll), 5);
    gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(list));

    gtk_widget_show_all(vbox);

    result = (gnome_dialog_run(GNOME_DIALOG(dialog)) == 0);

    gtk_widget_destroy(dialog);
  }

  return result;
}


static gboolean
gnc_filter_parent_accounts(Account *account, gpointer data)
{
  AccountWindow *aw = data;

  if (account == NULL)
    return FALSE;

  if (account == aw->top_level_account)
    return TRUE;

  if (account == aw->account)
    return FALSE;

  if (xaccAccountHasAncestor(account, aw->account))
    return FALSE;

  return TRUE;
}


static void
gnc_edit_account_ok(AccountWindow *aw)
{
  GHashTable *change_currency;
  GHashTable *change_security;
  GHashTable *change_type;

  gboolean change_children;
  gboolean has_children;
  gboolean change_all;

  GNCAccountTree *tree;

  Account *new_parent;
  Account *account;
  AccountGroup *children;

  GNCAccountType current_type;

  const char *name;
  const char *currency;
  const char *security;

  /* check for valid name */
  name = gtk_entry_get_text(GTK_ENTRY(aw->name_entry));
  if (safe_strcmp(name, "") == 0)
  {
    gnc_error_dialog_parented(GTK_WINDOW(aw->dialog), ACC_NO_NAME_MSG);
    return;
  }

  /* check for valid type */
  if (aw->type == BAD_TYPE)
  {
    gnc_error_dialog_parented(GTK_WINDOW(aw->dialog), ACC_TYPE_MSG);
    return;
  }

  tree = GNC_ACCOUNT_TREE(aw->parent_tree);
  new_parent = gnc_account_tree_get_current_account(tree);

  /* Parent check, probably not needed, but be safe */
  if (!gnc_filter_parent_accounts(new_parent, aw))
  {
    gnc_error_dialog_parented(GTK_WINDOW(aw->dialog), ACC_BAD_PARENT_MSG);
    return;
  }

  account = aw->account;

  change_currency = g_hash_table_new(NULL, NULL);
  change_security = g_hash_table_new(NULL, NULL);
  change_type     = g_hash_table_new(NULL, NULL);

  currency =
    gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(aw->currency_picker)->entry));
  security = gtk_entry_get_text(GTK_ENTRY(aw->security_picker));

  gnc_account_change_currency_security(account,
                                       change_currency,
                                       change_security,
                                       currency,
                                       security);

  children = xaccAccountGetChildren(account);
  if (children == NULL)
    has_children = FALSE;
  else if (xaccGetNumAccounts(children) == 0)
    has_children = FALSE;
  else
    has_children = TRUE;

  current_type = xaccAccountGetType(account);

  /* If the account has children and the new type isn't compatible
   * with the old type, the children's types must be changed. */
  change_children = (has_children &&
                     !xaccAccountTypesCompatible(current_type, aw->type));

  /* If the new parent's type is not compatible with the new type,
   * the whole sub-tree containing the account must be re-typed. */
  if (new_parent != aw->top_level_account)
  {
    int parent_type;

    parent_type = xaccAccountGetType(new_parent);

    if (!xaccAccountTypesCompatible(parent_type, aw->type))
      change_all = TRUE;
    else
      change_all = FALSE;
  }
  else
    change_all = FALSE;

  if (change_children)
    gnc_edit_change_account_types(change_type, account, NULL, aw->type);

  if (change_all)
  {
    Account *ancestor;
    Account *temp;

    temp = new_parent;

    do
    {
      ancestor = temp;
      temp = xaccAccountGetParentAccount(ancestor);
    } while (temp != NULL);

    gnc_edit_change_account_types(change_type, ancestor, account, aw->type);
  }

  if (!extra_change_verify(aw, change_currency, change_security, change_type))
  {
    g_hash_table_destroy(change_currency);
    g_hash_table_destroy(change_security);
    g_hash_table_destroy(change_type);
    return;
  }

  if (current_type != aw->type)
    /* Just refreshing won't work. */
    xaccDestroyLedgerDisplay(account);

  make_account_changes(change_currency, change_security, change_type);

  gnc_finish_ok (aw);

  g_hash_table_destroy(change_currency);
  g_hash_table_destroy(change_security);
  g_hash_table_destroy(change_type);
}


static void
gnc_new_account_ok (AccountWindow *aw)
{
  Account *parent_account;
  char *name;

  /* check for valid name */
  name = gtk_entry_get_text(GTK_ENTRY(aw->name_entry));
  if (safe_strcmp(name, "") == 0)
  {
    gnc_error_dialog_parented(GTK_WINDOW(aw->dialog), ACC_NO_NAME_MSG);
    return;
  }

  parent_account =
    gnc_account_tree_get_current_account(GNC_ACCOUNT_TREE(aw->parent_tree));
  if (parent_account == aw->top_level_account)
    parent_account = NULL;

  /* check for a duplicate name */
  {
    Account *account;
    AccountGroup *group;
    char separator;

    group = gncGetCurrentGroup();

    separator = gnc_get_account_separator();

    if (parent_account == NULL)
      account = xaccGetAccountFromFullName(group, name, separator);
    else
    {
      char *fullname_parent;
      char *fullname;
      char sep_string[2];

      sep_string[0] = separator;
      sep_string[1] = '\0';

      fullname_parent = xaccAccountGetFullName(parent_account, separator);
      fullname = g_strconcat(fullname_parent, sep_string, name, NULL);

      account = xaccGetAccountFromFullName(group, fullname, separator);

      free(fullname_parent);
      g_free(fullname);
    }

    if (account != NULL)
    {
      gnc_error_dialog_parented(GTK_WINDOW(aw->dialog), ACC_DUP_NAME_MSG);
      return;
    }
  }

  /* check for valid type */
  if (aw->type == BAD_TYPE)
  {
    gnc_error_dialog_parented(GTK_WINDOW(aw->dialog), ACC_TYPE_MSG);
    return;
  }

  gnc_finish_ok (aw);
}


static void
gnc_account_window_ok_cb(GtkWidget * widget, gpointer data)
{
  AccountWindow *aw = data; 

  switch (aw->dialog_type)
  {
    case NEW_ACCOUNT:
      gnc_new_account_ok (aw);
      break;
    case EDIT_ACCOUNT:
      gnc_edit_account_ok (aw);
      break;
    default:
      return;
  }
}


static void
gnc_account_window_cancel_cb(GtkWidget * widget, gpointer data)
{
  AccountWindow *aw = data; 

  gnome_dialog_close(GNOME_DIALOG(aw->dialog));
}


static void 
gnc_account_window_help_cb(GtkWidget *widget, gpointer data)
{
  AccountWindow *aw = data;
  char *help_file;

  switch (aw->dialog_type)
  {
    case NEW_ACCOUNT:
      help_file = HH_ACC;
      break;
    case EDIT_ACCOUNT:
      help_file = HH_ACCEDIT;
      break;
    default:
      return;
  }

  helpWindow(NULL, HELP_STR, help_file);
}


static int
gnc_account_window_close_cb(GnomeDialog *dialog, gpointer data)
{
  AccountWindow * aw = data;

  switch (aw->dialog_type)
  {
    case NEW_ACCOUNT:
      new_account_windows = g_list_remove(new_account_windows, dialog);

      if (aw->account != NULL)
      {
        xaccFreeAccount(aw->account);
        aw->account = NULL;
      }

      DEBUG("account add window destroyed\n");

      break;

    case EDIT_ACCOUNT:
      REMOVE_FROM_LIST (AccountWindow, editAccountList, aw->account, account); 
      break;

    default:
      PERR("unexpected dialog type\n");
      return FALSE;
  }

  xaccFreeAccount(aw->top_level_account);
  aw->top_level_account = NULL;

  g_free(aw);

  gdk_window_get_geometry(GTK_WIDGET(dialog)->window, NULL, NULL,
                          &last_width, &last_height, NULL);

  gnc_save_window_size("account_win", last_width, last_height);

  return FALSE;
}


static void 
gnc_type_list_select_cb(GtkCList * type_list, gint row, gint column,
                        GdkEventButton * event, gpointer data)
{
  AccountWindow * aw = data;
  gboolean sensitive;

  if (aw == NULL)
    return;

  if (!gtk_clist_get_selectable(type_list, row))
  {
    gtk_clist_unselect_row(type_list, row, 0);
    return;
  }

  aw->type = row;

  last_used_account_type = row;

  sensitive = (aw->type == STOCK    ||
	       aw->type == MUTUAL   ||
	       aw->type == CURRENCY);

  gtk_widget_set_sensitive(aw->security_picker, sensitive);
  gtk_widget_set_sensitive(aw->source_menu, sensitive);
}


static void 
gnc_type_list_unselect_cb(GtkCList * type_list, gint row, gint column,
                          GdkEventButton * event, gpointer data)
{
  AccountWindow * aw = data;

  aw->type = BAD_TYPE;

  gtk_widget_set_sensitive(aw->security_picker, FALSE);
  gtk_widget_set_sensitive(aw->source_menu, FALSE);
}


static void
gnc_account_list_fill(GtkCList *type_list)
{
  gint row;
  gchar *text[2] = { NULL, NULL };

  gtk_clist_clear(type_list);

  for (row = 0; row < NUM_ACCOUNT_TYPES; row++) 
  {
    text[0] = xaccAccountGetTypeStr(row);
    gtk_clist_append(type_list, text);
  }
}

static void
gnc_account_type_list_create(AccountWindow *aw)
{
  gnc_account_list_fill(GTK_CLIST(aw->type_list));

  gtk_clist_columns_autosize(GTK_CLIST(aw->type_list));

  gtk_signal_connect(GTK_OBJECT(aw->type_list), "select-row",
		     GTK_SIGNAL_FUNC(gnc_type_list_select_cb), aw);

  gtk_signal_connect(GTK_OBJECT(aw->type_list), "unselect-row",
		     GTK_SIGNAL_FUNC(gnc_type_list_unselect_cb), aw);

  switch (aw->dialog_type)
  {
    case NEW_ACCOUNT:
      aw->type = last_used_account_type;
      break;
    case EDIT_ACCOUNT:
      aw->type = xaccAccountGetType(aw->account);
      break;
  }

  gtk_clist_select_row(GTK_CLIST(aw->type_list), aw->type, 0);
}


static void
gnc_type_list_row_set_active(GtkCList *type_list, gint row, gboolean state)
{
  GtkStyle *style = gtk_widget_get_style(GTK_WIDGET(type_list));

  if (state)
  {
    gtk_clist_set_selectable(type_list, row, TRUE);
    gtk_clist_set_background(type_list, row, &style->white);
  }
  else
  {
    gtk_clist_unselect_row(type_list, row, 0);
    gtk_clist_set_selectable(type_list, row, FALSE);
    gtk_clist_set_background(type_list, row, &style->dark[GTK_STATE_NORMAL]);
  }
}


static void
gnc_parent_tree_select(GNCAccountTree *tree,
                       Account * account, 
                       gpointer data)
{
  AccountWindow *aw = data;
  GNCAccountType parent_type;
  gboolean  compatible;
  gint      type;

  if (aw->dialog_type == EDIT_ACCOUNT)
    return;

  account = gnc_account_tree_get_current_account(tree);

  /* Deleselect any or select top account */
  if (account == NULL || account == aw->top_level_account)
    for (type = 0; type < NUM_ACCOUNT_TYPES; type++)
      gnc_type_list_row_set_active(GTK_CLIST(aw->type_list), type, TRUE);
  else /* Some other account was selected */
  {
    parent_type = xaccAccountGetType(account);

    /* set the allowable account types for this parent */
    for (type = 0; type < NUM_ACCOUNT_TYPES; type++)
    {
      compatible = xaccAccountTypesCompatible(parent_type, type);
      gnc_type_list_row_set_active(GTK_CLIST(aw->type_list), type, compatible);
    }

    /* now select a new account type if the account class has changed */
    compatible = xaccAccountTypesCompatible(parent_type, aw->type);
    if (!compatible)
    {
      aw->type = parent_type;
      gtk_clist_select_row(GTK_CLIST(aw->type_list), parent_type, 0);
      gtk_clist_moveto(GTK_CLIST(aw->type_list), parent_type, 0, 0.5, 0);
    }
  }
}


/********************************************************************\
 * gnc_account_window_create                                        *
 *   creates a window to create a new account.                      *
 *                                                                  * 
 * Args:   aw - the information structure for this window           *
 * Return: the created window                                       *
 \*******************************************************************/
static void
gnc_account_window_create(AccountWindow *aw)
{
  GnomeDialog *awd;
  GtkObject *awo;
  GtkWidget *box;

  aw->dialog = create_Account_Dialog();
  awo = GTK_OBJECT(aw->dialog);
  awd = GNOME_DIALOG(awo);

  /* default to ok */
  gnome_dialog_set_default(awd, 0);

  gtk_signal_connect(awo, "close",
                     GTK_SIGNAL_FUNC(gnc_account_window_close_cb), aw);

  gnome_dialog_button_connect
    (awd, 0, GTK_SIGNAL_FUNC(gnc_account_window_ok_cb), aw);
  gnome_dialog_button_connect
    (awd, 1, GTK_SIGNAL_FUNC(gnc_account_window_cancel_cb), aw);
  gnome_dialog_button_connect
    (awd, 2, GTK_SIGNAL_FUNC(gnc_account_window_help_cb), aw);

  aw->name_entry =        gtk_object_get_data(awo, "name_entry");
  aw->description_entry = gtk_object_get_data(awo, "description_entry");
  aw->code_entry =        gtk_object_get_data(awo, "code_entry");
  aw->notes_text =        gtk_object_get_data(awo, "notes_text");

  gnome_dialog_editable_enters(awd, GTK_EDITABLE(aw->name_entry));
  gnome_dialog_editable_enters(awd, GTK_EDITABLE(aw->description_entry));
  gnome_dialog_editable_enters(awd, GTK_EDITABLE(aw->code_entry));

  box = gtk_object_get_data(awo, "currency_box");
  aw->currency_picker = gnc_currency_edit_new();
  gtk_box_pack_start(GTK_BOX(box), aw->currency_picker, TRUE, TRUE, 0);

  aw->security_picker = gtk_object_get_data(awo, "security_entry");

  box = gtk_object_get_data(awo, "source_box");
  aw->source_menu = gnc_ui_source_menu_create(aw->account);
  gtk_box_pack_start(GTK_BOX(box), aw->source_menu, TRUE, TRUE, 0);

  aw->type_list = gtk_object_get_data(awo, "type_list");
  gnc_account_type_list_create (aw);

  box = gtk_object_get_data(awo, "parent_scroll");
  aw->top_level_account = xaccMallocAccount();
  xaccAccountSetName(aw->top_level_account, NEW_TOP_ACCT_STR);
  aw->parent_tree = gnc_account_tree_new_with_root(aw->top_level_account);
  gtk_clist_column_titles_hide(GTK_CLIST(aw->parent_tree));
  gnc_account_tree_hide_all_but_name(GNC_ACCOUNT_TREE(aw->parent_tree));
  gnc_account_tree_refresh(GNC_ACCOUNT_TREE(aw->parent_tree));
  gnc_account_tree_expand_account(GNC_ACCOUNT_TREE(aw->parent_tree),
                                  aw->top_level_account);
  gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(aw->parent_tree));

  gtk_signal_connect(GTK_OBJECT (aw->parent_tree), "select_account",
		     GTK_SIGNAL_FUNC(gnc_parent_tree_select), aw);
  gtk_signal_connect(GTK_OBJECT (aw->parent_tree), "unselect_account",
		     GTK_SIGNAL_FUNC(gnc_parent_tree_select), aw);

  if (last_width == 0)
    gnc_get_window_size("account_win", &last_width, &last_height);

  gtk_window_set_default_size(GTK_WINDOW(aw->dialog),
                              last_width, last_height);

  gtk_widget_grab_focus(GTK_WIDGET(aw->name_entry));
}


/********************************************************************\
 * gnc_ui_new_account_window                                        *
 *   opens up a window to create a new account.                     *
 *                                                                  * 
 * Args:   group - not used                                         *
 * Return: NewAccountWindow object                                  *
 \*******************************************************************/
AccountWindow *
gnc_ui_new_account_window (AccountGroup *this_is_not_used) 
{
  AccountWindow *aw;
  Account *parent;

  aw = g_new0(AccountWindow, 1);

  aw->dialog_type = NEW_ACCOUNT;
  aw->account = xaccMallocAccount();
  aw->type    = last_used_account_type;

  gnc_account_window_create(aw);
  new_account_windows = g_list_prepend(new_account_windows, aw->dialog);

  gnc_currency_edit_set_currency (GNC_CURRENCY_EDIT(aw->currency_picker),
                                  default_currency);

  gtk_widget_show_all(aw->dialog);

  parent = gnc_get_current_account();
  gnc_account_tree_select_account(GNC_ACCOUNT_TREE(aw->parent_tree),
                                  parent, TRUE);

  gnc_window_adjust_for_screen(GTK_WINDOW(aw->dialog));

  return aw;
}


static void
gnc_edit_window_set_name(AccountWindow *aw)
{
  char *fullname;
  char *title;

  fullname = xaccAccountGetFullName(aw->account, gnc_get_account_separator());
  title = g_strconcat(fullname, " - ", EDIT_ACCT_STR, NULL);

  free(fullname);

  gtk_window_set_title(GTK_WINDOW(aw->dialog), title);

  g_free(title);
}


/********************************************************************\
 * gnc_ui_edit_account_window                                       *
 *   opens up a window to edit an account                           * 
 *                                                                  * 
 * Args:   account - the account to edit                            * 
 * Return: EditAccountWindow object                                 *
\********************************************************************/
AccountWindow *
gnc_ui_edit_account_window(Account *account)
{
  AccountWindow * aw;
  Account *parent;

  if (account == NULL)
    return NULL;

  FETCH_FROM_LIST (AccountWindow, editAccountList, account, account, aw);

  aw->dialog_type = EDIT_ACCOUNT;
  aw->account = account;
  gnc_account_window_create(aw);

  gnc_account_to_ui(aw);

  gnc_edit_window_set_name(aw);

  gtk_widget_show_all(aw->dialog);

  parent = xaccAccountGetParentAccount (account);
  if (parent == NULL)
    parent = aw->top_level_account;
  gnc_account_tree_select_account(GNC_ACCOUNT_TREE(aw->parent_tree),
                                  parent, TRUE);

  gnc_window_adjust_for_screen(GTK_WINDOW(aw->dialog));

  return aw;
}


/*********************************************************************\
 * gnc_ui_set_default_new_account_currency                           *
 *   Set the default currency for new accounts                       *
 *   intended to be called by option handling code                   *
 *                                                                   *
 * Args:    currency                                                 *
 * Globals: default_currency, default_currency_dynamically_allocated *
 * Return value: none                                                *
\*********************************************************************/
void 
gnc_ui_set_default_new_account_currency(const char *currency)
{
  if (default_currency_dynamically_allocated)
    g_free(default_currency);

  default_currency = g_strdup(currency);
  default_currency_dynamically_allocated = TRUE;
}


/********************************************************************\
 * Function: gnc_ui_destroy_account_add_windows - destroy all open  *
 *           account add windows.                                   *
 *                                                                  *
 * Args:   none                                                     *
 * Return: none                                                     *
\********************************************************************/
void
gnc_ui_destroy_account_add_windows(void)
{
  GnomeDialog *dialog;

  while (new_account_windows != NULL)
  {
    dialog = GNOME_DIALOG(new_account_windows->data);

    gnome_dialog_close(dialog);
  }
}


/********************************************************************\
 * gnc_ui_refresh_edit_account_window                               *
 *   refreshes the edit window                                      *
 *                                                                  *
 * Args:   account - the account of the window to refresh           *
 * Return: none                                                     *
\********************************************************************/
void
gnc_ui_refresh_edit_account_window(Account *account)
{
  AccountWindow *aw; 

  FIND_IN_LIST (AccountWindow, editAccountList, account, account, aw);
  if (aw == NULL)
    return;

  gnc_edit_window_set_name(aw);
}


void
gnc_ui_destroy_edit_account_window (Account * account)
{
  AccountWindow *aw;

  FIND_IN_LIST (AccountWindow, editAccountList, account, account, aw); 

  if (aw == NULL)
    return;
 
  gnome_dialog_close(GNOME_DIALOG(aw->dialog));
}


/********************************************************************\
 * gnc_ui_edit_account_window_raise                                 *
 *   shows and raises an account editing window                     *
 *                                                                  *
 * Args:   aw - the edit window structure                           *
\********************************************************************/
void
gnc_ui_edit_account_window_raise(AccountWindow * aw)
{
  if (aw == NULL)
    return;

  if (aw->dialog == NULL)
    return;

  gtk_widget_show(aw->dialog);

  if (aw->dialog->window == NULL)
    return;

  gdk_window_raise(aw->dialog->window);
}
