/********************************************************************\
 * account-tree.c -- The tree of accounts in the main window, and   *
 *                   associated helper and callback functions for   *
 *                   GnuCash.                                       *
 * Copyright (C) 1998,1999 Jeremy Collins	                    *
 * Copyright (C) 1998,1999 Linas Vepstas                            *
 * Copyright (C) 2000 Dave Peticolas <peticola@cs.ucdavis.edu>      *
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

#include "config.h"

#include <gnome.h>

#include "gnucash.h"
#include "messages.h"
#include "AccWindow.h"
#include "FileDialog.h"
#include "window-main.h"
#include "dialog-utils.h"
#include "account-tree.h"
#include "gnc-engine-util.h"

/* Signal codes */
enum
{
  SELECT_ACCOUNT,
  UNSELECT_ACCOUNT,
  ACTIVATE_ACCOUNT,
  LAST_SIGNAL
};


/** Static Globals ****************************************************/
static GtkCTreeClass *parent_class = NULL;
static guint account_tree_signals[LAST_SIGNAL];
static GSList *account_trees = NULL;


/** Static function declarations **************************************/
static void gnc_account_tree_init(GNCAccountTree *tree);
static void gnc_account_tree_class_init(GNCAccountTreeClass *klass);
static gint gnc_account_tree_key_press(GtkWidget *widget, GdkEventKey *event);
static gint gnc_account_tree_button_press(GtkWidget *widget,
 					  GdkEventButton *event);
static void gnc_account_tree_select_row(GtkCTree *ctree,
					GtkCTreeNode *row,
					gint column);
static void gnc_account_tree_unselect_row(GtkCTree *ctree,
					  GtkCTreeNode *row,
					  gint column);
static GtkCTreeNode * gnc_account_tree_insert_row(GNCAccountTree *tree,
						  GtkCTreeNode *parent,
						  GtkCTreeNode *sibling,
						  Account *acc);
static void gnc_account_tree_fill(GNCAccountTree *tree,
                                  GHashTable *expanded_accounts,
				  GtkCTreeNode *parent,
				  AccountGroup *accts);
static void gnc_account_tree_set_view_info_real(GNCAccountTree *tree);
static void gnc_account_tree_update_column_visibility (GNCAccountTree *tree);
static void gnc_account_tree_destroy(GtkObject *object);


GtkType
gnc_account_tree_get_type()
{
  static GtkType gnc_account_tree_type = 0;

  if (!gnc_account_tree_type)
  {
    static const GtkTypeInfo gnc_account_tree_info =
    {
      "GNCAccountTree",
      sizeof (GNCAccountTree),
      sizeof (GNCAccountTreeClass),
      (GtkClassInitFunc) gnc_account_tree_class_init,
      (GtkObjectInitFunc) gnc_account_tree_init,
      /* reserved_1 */ NULL,
      /* reserved_2 */ NULL,
      (GtkClassInitFunc) NULL
    };

    gnc_account_tree_type = gtk_type_unique (GTK_TYPE_CTREE,
					     &gnc_account_tree_info);
  }

  return gnc_account_tree_type;
}


/********************************************************************\
 * gnc_account_tree_new                                             *
 *   creates the account tree                                       *
 *                                                                  *
 * Returns: the account tree widget, or NULL if there was a problem.*
\********************************************************************/
GtkWidget *
gnc_account_tree_new()
{
  GtkWidget *tree;

  tree = GTK_WIDGET(gtk_type_new(gnc_account_tree_get_type()));

  account_trees = g_slist_prepend(account_trees, tree);

  return tree;
}


/********************************************************************\
 * gnc_account_tree_new_with_root                                   *
 *   creates the account tree with a root                           *
 *   the root is always visible and has a NULL associated account   *
 *   for callbacks. All other fields are blank for the root.        *
 *                                                                  *
 * Args: root_name - the string used for the root                   *
 *                                                                  *
 * Returns: the account tree widget, or NULL if there was a problem.*
\********************************************************************/
GtkWidget *
gnc_account_tree_new_with_root(Account * root)
{
  GNCAccountTree *tree;

  tree = GNC_ACCOUNT_TREE(gnc_account_tree_new());
  tree->root_account = root;

  return GTK_WIDGET(tree);
}


static void
gnc_account_tree_init(GNCAccountTree *tree)
{
  tree->root_account     = NULL;
  tree->current_accounts = NULL;
  tree->ignore_unselect  = FALSE;
  tree->filter           = NULL;
  tree->filter_data      = NULL;

  gnc_init_account_view_info(&tree->avi);

  gnc_account_tree_set_view_info_real(tree);

  gtk_ctree_construct(GTK_CTREE(tree),
                      tree->num_columns, 0,
                      (gchar **) tree->column_headings);

  gtk_clist_set_shadow_type(GTK_CLIST(tree), GTK_SHADOW_IN);
  gtk_clist_column_titles_passive(GTK_CLIST(tree));
  gtk_clist_set_column_auto_resize(GTK_CLIST(tree), 0, TRUE);
  gtk_clist_set_column_justification(GTK_CLIST(tree),
				     tree->balance_column,
				     GTK_JUSTIFY_RIGHT);
  gtk_clist_set_column_justification(GTK_CLIST(tree),
				     tree->balance_column + 1,
				     GTK_JUSTIFY_RIGHT);
  gtk_clist_set_column_justification(GTK_CLIST(tree),
				     tree->total_column,
				     GTK_JUSTIFY_RIGHT);
  gtk_clist_set_column_justification(GTK_CLIST(tree),
				     tree->total_column + 1,
				     GTK_JUSTIFY_RIGHT);

  {
    GtkStyle *style = gtk_widget_get_style(GTK_WIDGET(tree));
    GdkFont *font = NULL;
    gint width;
    gint i;

    if (style != NULL)
      font = style->font;

    if (font != NULL)
      for (i = 0; i < tree->num_columns; i++)
      {
	width = gdk_string_width(font, tree->column_headings[i]);
	gtk_clist_set_column_min_width(GTK_CLIST(tree), i, width + 5);
      }
  }

  tree->deficit_style = NULL;

  if (gnc_color_deficits())
  {
    GdkColormap *cm = gtk_widget_get_colormap(GTK_WIDGET(tree));
    GtkStyle *style = gtk_widget_get_style(GTK_WIDGET(tree));

    tree->deficit_style = gtk_style_copy(style);
    style = tree->deficit_style;

    gnc_get_deficit_color(&style->fg[GTK_STATE_NORMAL]);

    gdk_colormap_alloc_color(cm, &style->fg[GTK_STATE_NORMAL], FALSE, TRUE);
  }
}

static void
gnc_account_tree_class_init(GNCAccountTreeClass *klass)
{
  GtkObjectClass    *object_class;
  GtkWidgetClass    *widget_class;
  GtkContainerClass *container_class;
  GtkCListClass     *clist_class;
  GtkCTreeClass     *ctree_class;

  object_class =    (GtkObjectClass*) klass;
  widget_class =    (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;
  clist_class =     (GtkCListClass*) klass;
  ctree_class =     (GtkCTreeClass*) klass;

  parent_class = gtk_type_class(GTK_TYPE_CTREE);

  account_tree_signals[SELECT_ACCOUNT] =
    gtk_signal_new("select_account",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GNCAccountTreeClass,
				     select_account),
		   gtk_marshal_NONE__POINTER,
		   GTK_TYPE_NONE, 1,
		   GTK_TYPE_POINTER);

  account_tree_signals[UNSELECT_ACCOUNT] =
    gtk_signal_new("unselect_account",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GNCAccountTreeClass,
				     unselect_account),
		   gtk_marshal_NONE__POINTER,
		   GTK_TYPE_NONE, 1,
		   GTK_TYPE_POINTER);

  account_tree_signals[ACTIVATE_ACCOUNT] =
    gtk_signal_new("activate_account",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GNCAccountTreeClass,
				     activate_account),
		   gtk_marshal_NONE__POINTER,
		   GTK_TYPE_NONE, 1,
		   GTK_TYPE_POINTER);

  gtk_object_class_add_signals(object_class,
			       account_tree_signals,
			       LAST_SIGNAL);

  object_class->destroy = gnc_account_tree_destroy;

  widget_class->key_press_event = gnc_account_tree_key_press;
  widget_class->button_press_event = gnc_account_tree_button_press;

  ctree_class->tree_select_row   = gnc_account_tree_select_row;
  ctree_class->tree_unselect_row = gnc_account_tree_unselect_row;

  klass->select_account   = NULL;
  klass->unselect_account = NULL;
  klass->activate_account = NULL;
}

/********************************************************************\
 * gnc_account_tree_save_expanded                                   *
 *   saves the expanded accounts in a hash table, indexed by        *
 *   account pointers. only expanded accounts go in the table       *
 *                                                                  *
 * Args: tree - the tree to save expanded accounts                  *
 * Returns: hash table containing expanded accounts                 *
\********************************************************************/
static GHashTable *
gnc_account_tree_save_expanded(GNCAccountTree * tree)
{
  GtkCTree *ctree = GTK_CTREE(tree);
  gboolean expanded;
  GHashTable *ht;
  GtkCTreeNode *node;
  Account *account;
  gint row = 0;

  ht = g_hash_table_new(NULL, NULL);

  while ((node = gtk_ctree_node_nth(ctree, row++)) != NULL)
  {
    gtk_ctree_get_node_info(ctree, node, NULL, NULL, NULL, NULL,
                            NULL, NULL, NULL, &expanded);

    if (!expanded)
      continue;

    account = gtk_ctree_node_get_row_data(ctree, node);
    g_hash_table_insert(ht, account, account);
  }

  return ht;
}

/********************************************************************\
 * gnc_account_tree_refresh                                         *
 *   refreshes the account tree                                     *
 *                                                                  *
 * Args: tree - the tree to refresh                                 *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_account_tree_refresh(GNCAccountTree * tree)
{
  GtkCList      *clist = GTK_CLIST(tree);
  GHashTable    *expanded_accounts;
  GList         *current_accounts;
  GtkAdjustment *adjustment;
  gfloat         save_value = 0.0;

  adjustment = gtk_clist_get_vadjustment(GTK_CLIST(tree));
  if (adjustment != NULL)
    save_value = adjustment->value;

  expanded_accounts = gnc_account_tree_save_expanded(tree);
  current_accounts = tree->current_accounts;
  tree->current_accounts = NULL;

  gtk_clist_freeze(clist);

  gtk_clist_clear(clist);

  gnc_account_tree_fill(tree, expanded_accounts,
			gnc_account_tree_insert_row(tree, NULL, NULL,
						    tree->root_account),
			gncGetCurrentGroup());

  gtk_clist_columns_autosize(clist);

  gnc_account_tree_update_column_visibility(tree);

  gnc_account_tree_select_accounts(tree, current_accounts, FALSE);

  if (adjustment != NULL)
  {
    save_value = CLAMP(save_value, adjustment->lower,
                       adjustment->upper - adjustment->page_size);
    gtk_adjustment_set_value(adjustment, save_value);
  }

  gtk_clist_thaw(clist);

  g_hash_table_destroy(expanded_accounts);
  g_list_free(current_accounts);
}


static void
refresh_helper(gpointer tree, gpointer unused)
{
  gnc_account_tree_refresh(GNC_ACCOUNT_TREE(tree));
}

/********************************************************************\
 * gnc_account_tree_refresh_all                                     *
 *   refreshes the account tree                                     *
 *                                                                  *
 * Args: none                                                       *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_account_tree_refresh_all()
{
  g_slist_foreach(account_trees, refresh_helper, NULL);
}


/********************************************************************\
 * gnc_account_tree_set_view_info                                   *
 *   installs a new view information and refreshes the tree         *
 *                                                                  *
 * Args: tree - the tree to install new info                        *
 *       info - the view info structure                             *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_account_tree_set_view_info(GNCAccountTree *tree, AccountViewInfo *info)
{
  assert(IS_GNC_ACCOUNT_TREE(tree));
  assert(info != NULL);

  tree->avi = *info;

  gnc_account_tree_refresh(tree);
}


/********************************************************************\
 * gnc_account_tree_get_view_info                                   *
 *   retrieves the current view information for a tree              *
 *                                                                  *
 * Args: tree - the tree to get view info for                       *
 *       info - the view info structure to fill                     *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_account_tree_get_view_info(GNCAccountTree *tree, AccountViewInfo *info)
{
  assert(IS_GNC_ACCOUNT_TREE(tree));
  assert(info != NULL);

  *info = tree->avi;
}


/********************************************************************\
 * gnc_account_tree_expand_account                                  *
 *   expand the account node                                        *
 *                                                                  *
 * Args: tree   - the tree to expand an account for                 *
 *       accout - the account to expand                             *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_account_tree_expand_account(GNCAccountTree *tree, Account *account)
{
  GtkCTree *ctree;
  GtkCTreeNode *node;

  g_return_if_fail(tree != NULL);
  g_return_if_fail(IS_GNC_ACCOUNT_TREE(tree));

  ctree = GTK_CTREE(tree);

  /* Get the node with the account */
  node = gtk_ctree_find_by_row_data(ctree, NULL, account);
  if (node == NULL)
    return;

  gtk_ctree_expand(ctree, node);
}


/********************************************************************\
 * gnc_account_tree_toggle_account_expansion                        *
 *   toggle the expansion status for the given account              *
 *                                                                  *
 * Args: tree   - the tree to toggle expansion status for           *
 *       accout - the account to toggle expansion status            *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_account_tree_toggle_account_expansion (GNCAccountTree *tree,
                                           Account *account)
{
  GtkCTree *ctree;
  GtkCTreeNode *node;

  g_return_if_fail(tree != NULL);
  g_return_if_fail(IS_GNC_ACCOUNT_TREE(tree));

  ctree = GTK_CTREE(tree);

  /* Get the node with the account */
  node = gtk_ctree_find_by_row_data(ctree, NULL, account);
  if (node == NULL)
    return;

  gtk_ctree_toggle_expansion(ctree, node);
}


/********************************************************************\
 * gnc_account_tree_select_account                                  *
 *   select an account in the tree and expands the tree to make     *
 *   sure it could be visible. It may also scroll the tree to       *
 *   ensure it is visible                                           *
 *                                                                  *
 * Args: tree    - tree to be modified                              *
 *       account - account to be selected                           *
 *       show    - if true, scroll the tree                         *
 * Returns: true if the account was found                           *
\********************************************************************/
gboolean
gnc_account_tree_select_account(GNCAccountTree *tree,
                                Account        *account,
                                gboolean        show)
{
  GtkCTree *ctree = GTK_CTREE(tree);
  GtkCTreeNode *node, *n;
  GtkCTreeRow  *row;

  /* Get the node with the account */
  node = gtk_ctree_find_by_row_data(ctree, NULL, account);

  if (node == NULL)
    return FALSE;

  /* Select it */
  gtk_ctree_select(ctree, node);

  /* Expand all the parents */
  row = GTK_CTREE_ROW(node);
  while ((n = row->parent) != NULL)
  {
    gtk_ctree_expand(ctree, n);
    row = GTK_CTREE_ROW(n);
  }

  if (!show)
    return TRUE;

  /* Make sure it's visible */
  if (gtk_ctree_node_is_visible(ctree, node) != GTK_VISIBILITY_FULL)
    gtk_ctree_node_moveto(ctree, node, 0, 0.5, 0.0);

  return TRUE;
}


/********************************************************************\
 * gnc_account_tree_select_accounts                                 *
 *   select a list of accounts in the tree, expanding the parents   *
 *   of each one. If 'show' is true, the last one is made visible.  *
 *                                                                  *
 * Args: tree         - tree to be modified                         *
 *       account_list - list of accounts to be selected             *
 *       show         - determines if last account is made visible  *
 * Returns: true if the last account was found in the list          *
\********************************************************************/
gboolean
gnc_account_tree_select_accounts(GNCAccountTree *tree,
                                 GList          *account_list,
                                 gboolean        show_last)
{
  Account *account;
  gboolean real_show;
  gboolean result = FALSE;

  gtk_clist_freeze(GTK_CLIST(tree));

  while (account_list != NULL)
  {
    account = account_list->data;

    real_show = (account_list->next == NULL) ? show_last : FALSE;

    result = gnc_account_tree_select_account(tree, account, real_show);

    account_list = account_list->next;
  }

  gtk_clist_thaw(GTK_CLIST(tree));

  return result;
}


/********************************************************************\
 * gnc_account_tree_remove_account                                  *
 *   removes an account from the tree                               *
 *                                                                  *
 * Args: tree - tree to be modified                                 *
 *       account - account to be inserted                           *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_account_tree_remove_account(GNCAccountTree *tree, Account *account)
{
  GtkCTreeNode *node;

  node = gtk_ctree_find_by_row_data(GTK_CTREE(tree),
				    NULL, account);

  if (node != NULL)
    gtk_ctree_remove_node(GTK_CTREE(tree), node);
}


static void
remove_helper(gpointer tree, gpointer account)
{
  gnc_account_tree_remove_account(GNC_ACCOUNT_TREE(tree), account);
}

/********************************************************************\
 * gnc_account_tree_remove_account_all                              *
 *   removes an account from all account trees                      *
 *                                                                  *
 * Args: account - account to be inserted                           *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_account_tree_remove_account_all(Account *account)
{
  g_slist_foreach(account_trees, remove_helper, account);
}


/********************************************************************\
 * gnc_account_tree_insert_account                                  *
 *   inserts a new account into the tree                            *
 *                                                                  *
 * Args: tree - tree to insert account                              *
 *       account - account to be inserted                           *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_account_tree_insert_account(GNCAccountTree *tree, Account *account)
{
  /* for now, just punt. Maybe we'll do it faster later. */
  gnc_account_tree_refresh(tree);
}


/********************************************************************\
 * gnc_account_tree_show_income_expense                             *
 *   shows the income/expense accounts in a tree                    *
 *                                                                  *
 * Args: tree - tree to show income/expense accounts                *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_account_tree_show_income_expense(GNCAccountTree *tree)
{
  tree->avi.include_type[EXPENSE] = TRUE;
  tree->avi.include_type[INCOME] = TRUE;

  gnc_account_tree_refresh(tree);
}


/********************************************************************\
 * gnc_account_tree_hide_income_expense                             *
 *   hides the income/expense accounts in a tree                    *
 *                                                                  *
 * Args: tree - tree to hide income/expense accounts                *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_account_tree_hide_income_expense(GNCAccountTree *tree)
{
  tree->avi.include_type[EXPENSE] = FALSE;
  tree->avi.include_type[INCOME] = FALSE;

  gnc_account_tree_refresh(tree);
}


/********************************************************************\
 * gnc_account_tree_hide_all_but_names                              *
 *   hides all fields but the name in a tree                        *
 *                                                                  *
 * Args: tree - tree to hide all but names                          *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_account_tree_hide_all_but_name(GNCAccountTree *tree)
{
  int i;

  for (i = 0; i < NUM_ACCOUNT_FIELDS; i++)
    tree->avi.show_field[i] = (i == ACCOUNT_NAME);

  gnc_account_tree_update_column_visibility(tree);
}


static void
gnc_account_tree_update_column_visibility(GNCAccountTree *tree)
{
  gint i;

  for (i = 0; i < tree->num_columns; i++)
    gtk_clist_set_column_visibility
      (GTK_CLIST(tree), i, tree->avi.show_field[tree->column_fields[i]]);
}


/********************************************************************\
 * gnc_account_tree_get_current_account                             *
 *   returns the first account selected, or NULL if none            *
 *                                                                  *
 * Args: tree - tree to get current account from                    *
 * Returns: current account                                         *
\********************************************************************/
Account *
gnc_account_tree_get_current_account (GNCAccountTree *tree)
{
  if (tree == NULL)
    return NULL;

  if (tree->current_accounts == NULL)
    return NULL;

  return tree->current_accounts->data;
}


/********************************************************************\
 * gnc_account_tree_get_current_accounts                            *
 *   returns a g_malloc'd GList of the selected accounts            *
 *                                                                  *
 * Args: tree - tree to get current accounts from                   *
 * Returns: GList of selected accounts                              *
\********************************************************************/
GList *
gnc_account_tree_get_current_accounts (GNCAccountTree *tree)
{
  if (tree == NULL)
    return NULL;

  return g_list_copy(tree->current_accounts);
}


/********************************************************************\
 * gnc_init_account_view_info                                       *
 *   initialize an account view info structure with default values  *
 *                                                                  *
 * Args: avi - structure to initialize                              *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_init_account_view_info(AccountViewInfo *avi)
{
  int i;

  for (i = 0; i < NUM_ACCOUNT_TYPES; i++)
    avi->include_type[i] = TRUE;

  for (i = 0; i < NUM_ACCOUNT_FIELDS; i++)
    avi->show_field[i] = FALSE;

  avi->show_field[ACCOUNT_NAME] = TRUE;
  avi->show_field[ACCOUNT_DESCRIPTION] = TRUE;
  avi->show_field[ACCOUNT_TOTAL] = TRUE;
}

/********************************************************************\
 * gnc_account_tree_set_filter                                      *
 *   sets the account filter to use with the tree                   *
 *                                                                  *
 * Args: tree      - the tree to set the filter on                  *
 *       filter    - the filter function to use                     *
 *       user_data - the user_data for the callback                 *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_account_tree_set_filter(GNCAccountTree *tree,
                            AccountFilter filter,
                            gpointer user_data)
{
  g_return_if_fail(tree != NULL);
  g_return_if_fail(IS_GNC_ACCOUNT_TREE(tree));

  tree->filter = filter;
  tree->filter_data = user_data;
}

static void
gnc_account_tree_set_view_info_real(GNCAccountTree *tree)
{
  int i = 0;

  tree->column_fields[i++] = ACCOUNT_NAME;
  tree->column_fields[i++] = ACCOUNT_TYPE;
  tree->column_fields[i++] = ACCOUNT_CURRENCY;
  tree->column_fields[i++] = ACCOUNT_SECURITY;
  tree->column_fields[i++] = ACCOUNT_CODE;
  tree->column_fields[i++] = ACCOUNT_DESCRIPTION;

  tree->balance_column = i;
  tree->column_fields[i++] = ACCOUNT_BALANCE;
  tree->column_fields[i++] = ACCOUNT_BALANCE_EURO;

  tree->total_column = i;
  tree->column_fields[i++] = ACCOUNT_TOTAL;
  tree->column_fields[i++] = ACCOUNT_TOTAL_EURO;

  tree->column_fields[i++] = ACCOUNT_NOTES;

  tree->num_columns = i;

  for (i = 0; i < tree->num_columns; i++)
    tree->column_headings[i] =
      gnc_ui_get_account_field_name(tree->column_fields[i]);
}

static gint
gnc_account_tree_key_press(GtkWidget *widget, GdkEventKey *event)
{
  GNCAccountTree *tree = GNC_ACCOUNT_TREE(widget);
  Account *account = gnc_account_tree_get_current_account(tree);

  if ((event->keyval == GDK_Return) && (account != NULL))
  {
    gtk_signal_emit(GTK_OBJECT(tree),
                    account_tree_signals[ACTIVATE_ACCOUNT],
                    account);

    return TRUE;
  }

  if (GTK_WIDGET_CLASS(parent_class)->key_press_event != NULL)
    return GTK_WIDGET_CLASS(parent_class)->key_press_event(widget, event);

  return FALSE;
}

static gint
gnc_account_tree_button_press(GtkWidget *widget,
			      GdkEventButton *event)
{
  GtkCTree *ctree = GTK_CTREE(widget);
  GtkCList *clist = GTK_CLIST(widget);
  GtkCTreeNode *node;
  Account *account;
  gint x, y, row, column;
	
  if (event->window == clist->clist_window)
  {
    x = event->x;
    y = event->y;

    if (!gtk_clist_get_selection_info(clist, x, y, &row, &column))
      return FALSE;

    if (event->type == GDK_2BUTTON_PRESS)
    {
      node = gtk_ctree_node_nth(ctree, row);
      account = gtk_ctree_node_get_row_data(ctree, node);

      GNC_ACCOUNT_TREE(ctree)->ignore_unselect = TRUE;

      gtk_signal_emit(GTK_OBJECT(widget),
		      account_tree_signals[ACTIVATE_ACCOUNT],
                      account);

      return TRUE;
    }
  }

  if (GTK_WIDGET_CLASS(parent_class)->button_press_event != NULL)
    return GTK_WIDGET_CLASS(parent_class)->button_press_event(widget, event);

  return FALSE;
}

static void
gnc_account_tree_select_row(GtkCTree *ctree,
			    GtkCTreeNode *row,
			    gint column)
{
  GNCAccountTree *tree = GNC_ACCOUNT_TREE(ctree);
  Account *account;
  GList *node;

  tree->ignore_unselect = FALSE;

  account = gtk_ctree_node_get_row_data(ctree, GTK_CTREE_NODE(row));

  if (account != NULL)
  {
    node = g_list_find(tree->current_accounts, account);

    if (node == NULL)
      tree->current_accounts = g_list_prepend(tree->current_accounts, account);

    gtk_signal_emit(GTK_OBJECT(ctree),
                    account_tree_signals[SELECT_ACCOUNT],
                    account);
  }

  GTK_CTREE_CLASS(parent_class)->tree_select_row(ctree, row, column);
}

static void
gnc_account_tree_unselect_row(GtkCTree *ctree,
			      GtkCTreeNode *row,
			      gint column)
{
  GNCAccountTree *tree = GNC_ACCOUNT_TREE(ctree);
  Account *account;
  GList *node;

  if (tree->ignore_unselect)
  {
    tree->ignore_unselect = FALSE;
    return;
  }

  account = gtk_ctree_node_get_row_data(ctree, GTK_CTREE_NODE(row));

  node = g_list_find(tree->current_accounts, account);
  while (node != NULL)
  {
    tree->current_accounts = g_list_remove_link(tree->current_accounts, node);
    g_list_free_1(node);

    node = g_list_find(tree->current_accounts, account);
  }

  gtk_signal_emit(GTK_OBJECT(ctree),
		  account_tree_signals[UNSELECT_ACCOUNT],
		  account);

  GTK_CTREE_CLASS(parent_class)->tree_unselect_row(ctree, row, column);
}

static void
gnc_account_tree_fill(GNCAccountTree *tree,
                      GHashTable     *expanded_accounts,
		      GtkCTreeNode   *parent,
                      AccountGroup   *accts)
{
  Account *account;
  AccountGroup *acc_children;
  GtkCTreeNode *node;
  gint totalAccounts = xaccGroupGetNumAccounts(accts);
  gint currentAccount;
  gint type;

  /* Add each account to the tree */  
  for ( currentAccount = 0;
        currentAccount < totalAccounts;
        currentAccount++ )
  {
    account = xaccGroupGetAccount(accts, currentAccount);

    if (tree->filter != NULL)
      if (!tree->filter(account, tree->filter_data))
        continue;

    type = xaccAccountGetType(account);

    if (!tree->avi.include_type[type])
      continue;

    node = gnc_account_tree_insert_row(tree, parent, NULL, account);

    if (g_hash_table_lookup(expanded_accounts, account) != NULL)
      gtk_ctree_expand(GTK_CTREE(tree), node);

    /* If this account has children,
     * then we need to build a subtree and fill it.
     */
    acc_children = xaccAccountGetChildren(account);
    if (xaccAccountGetChildren(account) != NULL)
      gnc_account_tree_fill(tree, expanded_accounts, node, acc_children);
  }
}

static GtkCTreeNode *
gnc_account_tree_insert_row(GNCAccountTree *tree,
			    GtkCTreeNode *parent,
			    GtkCTreeNode *sibling,
			    Account *acc)
{
  gchar *text[NUM_ACCOUNT_FIELDS + 1];
  GtkCTreeNode *node;
  gint i;

  if (acc == NULL)
    return NULL;

  for (i = 0; i < tree->num_columns; i++) {
    text[i] =
      g_strdup(gnc_ui_get_account_field_value_string(acc,
                                                     tree->column_fields[i]));

    /* Since string fields like notes can be NULL */
    if(!text[i]) {
      text[i] = g_strdup("");
    }
  }

  text[tree->num_columns] = NULL;

  node = gtk_ctree_insert_node(GTK_CTREE(tree), parent, sibling,
			       text, 0, NULL, NULL, NULL, NULL,
			       FALSE, FALSE);

  for (i = 0; i < tree->num_columns; i++)
    g_free(text[i]);

  if (gnc_color_deficits())
  {
    GtkStyle *style;
    double balance;
    gboolean deficit;

    balance = gnc_ui_account_get_balance(acc, FALSE);
    deficit = (balance < 0) && !DEQ(balance, 0);

    if (deficit)
      style = tree->deficit_style;
    else
      style = gtk_widget_get_style(GTK_WIDGET(tree));

    if (style != NULL)
      gtk_ctree_node_set_cell_style(GTK_CTREE(tree), node,
				    tree->balance_column, style);

    balance = gnc_ui_account_get_balance(acc, TRUE);
    deficit = (balance < 0) && !DEQ(balance, 0);

    if (deficit)
      style = tree->deficit_style;
    else
      style = gtk_widget_get_style(GTK_WIDGET(tree));

    if (style != NULL)
    {
      gtk_ctree_node_set_cell_style(GTK_CTREE(tree), node,
				    tree->total_column, style);
      gtk_ctree_node_set_cell_style(GTK_CTREE(tree), node,
				    tree->total_column + 1, style);
    }
  }

  /* Set the user_data for the tree item to the account it */
  /* represents.                                           */
  gtk_ctree_node_set_row_data(GTK_CTREE(tree), node, acc);

  return node;
}

static void
gnc_account_tree_destroy(GtkObject *object)
{
  GNCAccountTree *tree = GNC_ACCOUNT_TREE(object);

  account_trees = g_slist_remove(account_trees, tree);

  if (tree->deficit_style != NULL)
  {
    gtk_style_unref(tree->deficit_style);
    tree->deficit_style = NULL;
  }

  g_list_free(tree->current_accounts);
  tree->current_accounts = NULL;

  if (GTK_OBJECT_CLASS(parent_class)->destroy)
    (* GTK_OBJECT_CLASS(parent_class)->destroy) (object);
}
