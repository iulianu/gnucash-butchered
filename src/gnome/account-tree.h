/*******************************************************************\
 * account-tree.h -- GNOME account tree functions                   *
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

#ifndef __GNC_ACCOUNT_TREE_H__
#define __GNC_ACCOUNT_TREE_H__

#include <gtk/gtkctree.h>

#include "dialog-utils.h"

#ifdef __cplusplus
extern "C" {
#endif				/* __cplusplus */

#define GTK_TYPE_GNC_ACCOUNT_TREE (gnc_account_tree_get_type ())
#define GNC_ACCOUNT_TREE(obj)     (GTK_CHECK_CAST ((obj), GTK_TYPE_GNC_ACCOUNT_TREE, GNCAccountTree))
#define GNC_ACCOUNT_TREE_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_GNC_ACCOUNT_TREE, GNCAccountTreeClass))
#define GTK_IS_GNC_ACCOUNT_TREE(obj)  (GTK_CHECK_TYPE ((obj), GTK_TYPE_GNC_ACCOUNT_TREE))
#define GTK_IS_GNC_ACCOUNT_TREE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_GNC_ACCOUNT_TREE))

typedef struct _GNCAccountTree      GNCAccountTree;
typedef struct _GNCAccountTreeClass GNCAccountTreeClass;
typedef struct _AccountViewInfo     AccountViewInfo;

struct _AccountViewInfo
{
  gboolean include_type[NUM_ACCOUNT_TYPES];

  gboolean show_field[NUM_ACCOUNT_FIELDS];
};

struct _GNCAccountTree
{
  GtkCTree ctree;

  AccountViewInfo avi;

  gint    num_columns;
  gint    balance_column;
  gint    column_fields[NUM_ACCOUNT_FIELDS];
  gchar * column_headings[NUM_ACCOUNT_FIELDS + 1];

  GtkStyle *deficit_style;

  Account *root_account;

  GList   *current_accounts;

  gboolean ignore_unselect;
};

struct _GNCAccountTreeClass
{
  GtkCTreeClass parent_class;

  void (*select_account)   (GNCAccountTree *tree,
                            Account        *account);

  void (*unselect_account) (GNCAccountTree *tree,
                            Account        *account);

  void (*activate_account) (GNCAccountTree *tree,
                            Account        *account);
};

/***********************************************************
 *                public functions                         *
 ***********************************************************/

GtkType gnc_account_tree_get_type (void);

GtkWidget * gnc_account_tree_new (void);

GtkWidget * gnc_account_tree_new_with_root (Account *account);

void gnc_account_tree_refresh (GNCAccountTree *tree);

void gnc_account_tree_set_view_info(GNCAccountTree *tree,
				    AccountViewInfo *info);

gboolean gnc_account_tree_select_account (GNCAccountTree *tree,
					  Account *account,
                                          gboolean show_account);

gboolean gnc_account_tree_select_accounts(GNCAccountTree *tree,
                                          GList          *account_list,
                                          gboolean        show_last);

void gnc_account_tree_insert_account (GNCAccountTree *tree,
				      Account *account);

void gnc_account_tree_remove_account (GNCAccountTree *tree,
				      Account *account);

void gnc_account_tree_show_categories (GNCAccountTree *tree);

void gnc_account_tree_hide_categories (GNCAccountTree *tree);

Account * gnc_account_tree_get_current_account (GNCAccountTree *tree);
GList *   gnc_account_tree_get_current_accounts (GNCAccountTree *tree);

void gnc_account_tree_hide_all_but_name (GNCAccountTree *tree);

void gnc_init_account_view_info(AccountViewInfo *avi);

void gnc_account_tree_set_view_info (GNCAccountTree *tree,
				     AccountViewInfo *info);

void gnc_account_tree_get_view_info (GNCAccountTree *tree,
				     AccountViewInfo *info);


#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* __GNC_ACCOUNT_TREE_H__ */
