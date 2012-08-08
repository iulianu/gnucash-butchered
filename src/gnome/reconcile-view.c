/********************************************************************\
 * reconcile-view.c -- A view of accounts to be reconciled for      *
 *                     GnuCash.                                     *
 * Copyright (C) 1998,1999 Jeremy Collins	                    *
 * Copyright (C) 1998-2000 Linas Vepstas                            *
 * Copyright (C) 2012 Robert Fewell                                 *
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
#include <gdk/gdkkeysyms.h>

#include "gnc-date.h"
#include "qof.h"
#include "Transaction.h"
#include "gnc-ui-util.h"
#include "gnc-gconf-utils.h"
#include "reconcile-view.h"
#include "search-param.h"
#include "gnc-component-manager.h"

/* Signal codes */
enum
{
    TOGGLE_RECONCILED,
    LINE_SELECTED,
    DOUBLE_CLICK_SPLIT,
    LAST_SIGNAL
};


/** Static Globals ****************************************************/
static GNCQueryViewClass *parent_class = NULL;
static guint reconcile_view_signals[LAST_SIGNAL] = {0};

/** Static function declarations **************************************/
static void gnc_reconcile_view_init (GNCReconcileView *view);
static void gnc_reconcile_view_class_init (GNCReconcileViewClass *klass);
static void gnc_reconcile_view_finalize (GObject *object);
static gpointer gnc_reconcile_view_is_reconciled (gpointer item, gpointer user_data);
static void gnc_reconcile_view_line_toggled (GNCQueryView *qview, gpointer item, gpointer user_data);
static void gnc_reconcile_view_double_click_entry (GNCQueryView *qview, gpointer item, gpointer user_data);
static void gnc_reconcile_view_row_selected (GNCQueryView *qview, gpointer item, gpointer user_data);
static gboolean gnc_reconcile_view_key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer user_data);


GType
gnc_reconcile_view_get_type (void)
{
    static GType gnc_reconcile_view_type = 0;

    if (gnc_reconcile_view_type == 0)
    {
        static const GTypeInfo gnc_reconcile_view_info =
        {
            sizeof (GNCReconcileViewClass),
            NULL,
            NULL,
            (GClassInitFunc) gnc_reconcile_view_class_init,
            NULL,
            NULL,
            sizeof (GNCReconcileView),
            0,
            (GInstanceInitFunc) gnc_reconcile_view_init
        };

        gnc_reconcile_view_type = g_type_register_static (GNC_TYPE_QUERY_VIEW,
                                  "GncReconcileView",
                                  &gnc_reconcile_view_info, 0);
    }

    return gnc_reconcile_view_type;
}


/****************************************************************************\
 * gnc_reconcile_view_new                                                   *
 *   creates the account tree                                               *
 *                                                                          *
 * Args: account        - the account to use in filling up the splits.      *
 *       type           - the type of view, RECLIST_DEBIT or RECLIST_CREDIT *
 *       statement_date - date of statement                                 *
 * Returns: the account tree widget, or NULL if there was a problem.        *
\****************************************************************************/
static void
gnc_reconcile_view_construct (GNCReconcileView *view, Query *query)
{
    GNCQueryView      *qview = GNC_QUERY_VIEW (view);
    GtkTreeViewColumn *col;
    GtkTreeSelection  *selection;
    gboolean           inv_sort = FALSE;

    if (view->view_type == RECLIST_CREDIT)
        inv_sort = TRUE;

    /* Construct the view */
    gnc_query_view_construct (qview, view->column_list, query);
    gnc_query_view_set_numerics (qview, TRUE, inv_sort);

    /* Set the description field to have spare space */
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (qview), 2);
    gtk_tree_view_column_set_expand (col, TRUE);

    /* Set the selection method */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (qview));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

    /* Now set up the signals for the QueryView */
    g_signal_connect (G_OBJECT (qview), "column_toggled",
                      G_CALLBACK (gnc_reconcile_view_line_toggled), view);
    g_signal_connect (G_OBJECT (qview), "double_click_entry",
                      G_CALLBACK (gnc_reconcile_view_double_click_entry), view);
    g_signal_connect (G_OBJECT (qview), "row_selected",
                      G_CALLBACK (gnc_reconcile_view_row_selected), view);
    g_signal_connect(G_OBJECT (qview), "key_press_event",
                     G_CALLBACK(gnc_reconcile_view_key_press_cb), view);
}

GtkWidget *
gnc_reconcile_view_new (Account *account, GNCReconcileViewType type,
                       time_t statement_date)
{
    GNCReconcileView *view;
    GtkListStore     *liststore;
    gboolean          include_children, auto_check;
    GList            *accounts = NULL;
    GList            *splits;
    Query            *query;

    g_return_val_if_fail (account, NULL);
    g_return_val_if_fail ((type == RECLIST_DEBIT) ||
                         (type == RECLIST_CREDIT), NULL);

    view = g_object_new (GNC_TYPE_RECONCILE_VIEW, NULL);

    /* Create the list store with 6 columns and add to treeview,
       column 0 will be a pointer to the entry */
    liststore = gtk_list_store_new (6, G_TYPE_POINTER, G_TYPE_STRING, 
                G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN );
    gtk_tree_view_set_model (GTK_TREE_VIEW (view), GTK_TREE_MODEL (liststore));
    g_object_unref (liststore);

    view->account = account;
    view->view_type = type;
    view->statement_date = statement_date;

    query = qof_query_create_for (GNC_ID_SPLIT);
    qof_query_set_book (query, gnc_get_current_book ());

    include_children = xaccAccountGetReconcileChildrenStatus (account);
    if (include_children)
        accounts = gnc_account_get_descendants (account);

    /* match the account */
    accounts = g_list_prepend (accounts, account);

    xaccQueryAddAccountMatch (query, accounts, QOF_GUID_MATCH_ANY, QOF_QUERY_AND);

    g_list_free (accounts);

    /* limit the matches to CREDITs and DEBITs only, depending on the type */
    if (type == RECLIST_CREDIT)
        xaccQueryAddValueMatch(query, gnc_numeric_zero (),
                               QOF_NUMERIC_MATCH_CREDIT,
                               QOF_COMPARE_GTE, QOF_QUERY_AND);
    else
        xaccQueryAddValueMatch(query, gnc_numeric_zero (),
                               QOF_NUMERIC_MATCH_DEBIT,
                               QOF_COMPARE_GTE, QOF_QUERY_AND);

    /* limit the matches only to Cleared and Non-reconciled splits */
    xaccQueryAddClearedMatch (query, CLEARED_NO | CLEARED_CLEARED, QOF_QUERY_AND);

    /* Initialize the QueryList */
    gnc_reconcile_view_construct (view, query);

    /* find the list of splits to auto-reconcile */
    auto_check = gnc_gconf_get_bool (GCONF_RECONCILE_SECTION,
                                    "check_cleared", NULL);

    if (auto_check)
    {
        for (splits = qof_query_run (query); splits; splits = splits->next)
        {
            Split *split = splits->data;
            char recn = xaccSplitGetReconcile (split);
            time_t trans_date = xaccTransGetDate (xaccSplitGetParent (split));

            /* Just an extra verification that our query is correct ;) */
            g_assert (recn == NREC || recn == CREC);

            if (recn == CREC &&
                    difftime (trans_date, statement_date) <= 0)
                g_hash_table_insert (view->reconciled, split, split);
        }
    }

    /* Free the query -- we don't need it anymore */
    qof_query_destroy (query);

    return GTK_WIDGET (view);
}


static void
gnc_reconcile_view_init (GNCReconcileView *view)
{
    GNCSearchParam *param;
    GList          *columns = NULL;

    view->reconciled = g_hash_table_new (NULL, NULL);
    view->account = NULL;
    view->sibling = NULL;

    param = gnc_search_param_new();
    gnc_search_param_set_param_fcn (param, QOF_TYPE_BOOLEAN,
                                    gnc_reconcile_view_is_reconciled, view);
    gnc_search_param_set_title (param, _("Reconciled:R") + 11);
    gnc_search_param_set_justify (param, GTK_JUSTIFY_CENTER);
    gnc_search_param_set_passive (param, TRUE);
    gnc_search_param_set_non_resizeable (param, TRUE);
    columns = g_list_prepend (columns, param);
    columns = gnc_search_param_prepend_with_justify (columns, _("Amount"),
              GTK_JUSTIFY_RIGHT,
              NULL, GNC_ID_SPLIT,
              SPLIT_AMOUNT, NULL);
    columns = gnc_search_param_prepend (columns, _("Description"), NULL,
                                        GNC_ID_SPLIT, SPLIT_TRANS,
                                        TRANS_DESCRIPTION, NULL);
    columns = gnc_search_param_prepend_with_justify (columns, _("Num"),
              GTK_JUSTIFY_CENTER,
              NULL, GNC_ID_SPLIT,
              SPLIT_TRANS, TRANS_NUM, NULL);
    columns = gnc_search_param_prepend (columns, _("Date"), NULL, GNC_ID_SPLIT,
                                        SPLIT_TRANS, TRANS_DATE_POSTED, NULL);

    view->column_list = columns;
}


static void
gnc_reconcile_view_class_init (GNCReconcileViewClass *klass)
{
    GObjectClass    *object_class;

    object_class =  G_OBJECT_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    reconcile_view_signals[TOGGLE_RECONCILED] =
        g_signal_new("toggle_reconciled",
                     G_OBJECT_CLASS_TYPE (object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET (GNCReconcileViewClass,
                                      toggle_reconciled),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE, 1,
                     G_TYPE_POINTER);

    reconcile_view_signals[LINE_SELECTED] =
        g_signal_new("line_selected",
                     G_OBJECT_CLASS_TYPE (object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET (GNCReconcileViewClass,
                                      line_selected),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE, 1,
                     G_TYPE_POINTER);

    reconcile_view_signals[DOUBLE_CLICK_SPLIT] =
        g_signal_new("double_click_split",
                     G_OBJECT_CLASS_TYPE (object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET (GNCReconcileViewClass,
                                      double_click_split),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE, 1,
                     G_TYPE_POINTER);

    object_class->finalize = gnc_reconcile_view_finalize;

    klass->toggle_reconciled = NULL;
    klass->line_selected = NULL;
    klass->double_click_split = NULL;
}


static void
gnc_reconcile_view_toggle_split (GNCReconcileView *view, Split *split)
{
    Split *current;

    g_return_if_fail (GNC_IS_RECONCILE_VIEW (view));
    g_return_if_fail (view->reconciled != NULL);

    current = g_hash_table_lookup (view->reconciled, split);

    if (current == NULL)
        g_hash_table_insert (view->reconciled, split, split);
    else
        g_hash_table_remove (view->reconciled, split);
}


static void
gnc_reconcile_view_toggle_children (Account *account, GNCReconcileView *view, Split *split)
{
    GList       *child_accounts, *node;
    Transaction *transaction;

    /*
     * Need to get all splits in this transaction and identify any that are
     * in the same heirarchy as the account being reconciled (not necessarily
     * the account this split is from.)
     *
     * For each of these splits toggle them all to the same state.
     */
    child_accounts = gnc_account_get_descendants (account);
    child_accounts = g_list_prepend (child_accounts, account);
    transaction = xaccSplitGetParent (split);
    for (node = xaccTransGetSplitList (transaction); node; node = node->next)
    {
        Split *other_split;
        Account *other_account;
        GNCReconcileView *current_view;

        other_split = node->data;
        other_account = xaccSplitGetAccount (other_split);
        if (other_split == split)
            continue;
        /* Check this 'other' account in in the same heirarchy */
        if (!g_list_find (child_accounts, other_account))
            continue;
        /* Search our sibling view for this split first.  We search the
         * sibling list first because that it where it is most likely to be.
         */
        current_view = view->sibling;
        if (!gnc_query_view_item_in_view (GNC_QUERY_VIEW (current_view), other_split))
        {
            /* Not in the sibling view, try this view */
            current_view = view;
            if (!gnc_query_view_item_in_view (GNC_QUERY_VIEW (current_view), other_split))
                /* We can't find it, nothing more I can do about it */
                continue;
        }
        gnc_reconcile_view_toggle_split (current_view, other_split);
    }
    g_list_free (child_accounts);
}


static void
gnc_reconcile_view_toggle (GNCReconcileView *view, Split *split)
{
    gboolean include_children;

    g_return_if_fail (GNC_IS_RECONCILE_VIEW (view));
    g_return_if_fail (view->reconciled != NULL);

    gnc_reconcile_view_toggle_split (view, split);

    include_children = xaccAccountGetReconcileChildrenStatus (view->account);
    if (include_children)
        gnc_reconcile_view_toggle_children (view->account, view, split);

    g_signal_emit (G_OBJECT (view),
                   reconcile_view_signals[TOGGLE_RECONCILED], 0, split);
}


static void
gnc_reconcile_view_line_toggled (GNCQueryView *qview,
                                 gpointer item,
                                 gpointer user_data)
{
    GNCReconcileView *view;
    GtkTreeModel     *model;
    GtkTreeIter       iter;
    gpointer          entry;

    g_return_if_fail (user_data);
    g_return_if_fail (GNC_IS_QUERY_VIEW (qview));

    view = user_data;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (qview));
    gtk_tree_model_iter_nth_child (model, &iter, NULL, qview->toggled_row);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, qview->toggled_column, GPOINTER_TO_INT(item), -1);
    gtk_tree_model_get (model, &iter, 0, &entry, -1);

    gnc_reconcile_view_toggle (view, entry);
}


static void
gnc_reconcile_view_double_click_entry (GNCQueryView *qview,
                                       gpointer item,
                                       gpointer user_data)
{
    GNCReconcileView *view;

    g_return_if_fail (user_data);
    g_return_if_fail (GNC_IS_QUERY_VIEW (qview));

    view = user_data;

    g_signal_emit(G_OBJECT (view),
                  reconcile_view_signals[DOUBLE_CLICK_SPLIT], 0, item);
}


static void
gnc_reconcile_view_row_selected (GNCQueryView *qview,
                                 gpointer item,
                                 gpointer user_data)
{
    GNCReconcileView *view;

    g_return_if_fail(user_data);
    g_return_if_fail(GNC_IS_QUERY_VIEW(qview));

    view = user_data;

    g_signal_emit(G_OBJECT(view),
                  reconcile_view_signals[LINE_SELECTED], 0, item);
}


static gboolean
gnc_reconcile_view_key_press_cb (GtkWidget *widget, GdkEventKey *event,
                            gpointer user_data)
{
    GNCReconcileView  *view = GNC_RECONCILE_VIEW(user_data);
    GNCQueryView      *qview = GNC_QUERY_VIEW(widget);
    GtkTreeModel      *model;
    GtkTreeIter        iter;
    gpointer           entry, pointer;
    gboolean           valid, toggle;

    switch (event->keyval)
    {
    case GDK_space:
        g_signal_stop_emission_by_name (widget, "key_press_event");

        entry = gnc_query_view_get_selected_entry (qview);

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (qview));
        valid = gtk_tree_model_get_iter_first (model, &iter);

        while (valid)
        {
            /* Walk through the list, reading each row, column 0
               has a pointer to the required entry */
            gtk_tree_model_get (model, &iter, 0, &pointer, -1);

            if(pointer == entry)
            {
                /* Column 5 is the toggle column */
                gtk_tree_model_get (model, &iter, 5, &toggle, -1);

                if(toggle)
                    gtk_list_store_set (GTK_LIST_STORE (model), &iter, 5, 0, -1);
                else
                    gtk_list_store_set (GTK_LIST_STORE (model), &iter, 5, 1, -1);
            }
            valid = gtk_tree_model_iter_next (model, &iter);
        }
        gnc_reconcile_view_toggle (view, entry);

        return TRUE;
        break;

    default:
        return FALSE;
    }
}


static void
gnc_reconcile_view_finalize (GObject *object)
{
    GNCReconcileView *view = GNC_RECONCILE_VIEW (object);

    if (view->reconciled != NULL)
    {
        g_hash_table_destroy (view->reconciled);
        view->reconciled = NULL;
    }

    G_OBJECT_CLASS (parent_class)->finalize (object);
}


gint
gnc_reconcile_view_get_num_splits (GNCReconcileView *view)
{
    g_return_val_if_fail (view != NULL, 0);
    g_return_val_if_fail (GNC_IS_RECONCILE_VIEW (view), 0);

    return gnc_query_view_get_num_entries (GNC_QUERY_VIEW (view));
}


Split *
gnc_reconcile_view_get_current_split (GNCReconcileView *view)
{
    g_return_val_if_fail (view != NULL, NULL);
    g_return_val_if_fail (GNC_IS_RECONCILE_VIEW (view), NULL);

    return gnc_query_view_get_selected_entry (GNC_QUERY_VIEW (view));
}


/********************************************************************\
 * gnc_reconcile_view_is_reconciled                                 *
 *   Is the item a reconciled split?                                *
 *                                                                  *
 * Args: item      - the split to be checked                        *
 *       user_data - a pointer to the GNCReconcileView              *
 * Returns: whether the split is to be reconciled.                  *
\********************************************************************/
static gpointer
gnc_reconcile_view_is_reconciled (gpointer item, gpointer user_data)
{
    GNCReconcileView *view = user_data;
    Split *current;

    g_return_val_if_fail (item, NULL);
    g_return_val_if_fail (view, NULL);
    g_return_val_if_fail (GNC_IS_RECONCILE_VIEW (view), NULL);

    if (!view->reconciled)
        return NULL;

    current = g_hash_table_lookup (view->reconciled, item);
    return GINT_TO_POINTER (current != NULL);
}


/********************************************************************\
 * gnc_reconcile_view_refresh                                       *
 *   refreshes the view                                             *
 *                                                                  *
 * Args: view - view to refresh                                     *
 * Returns: nothing                                                 *
\********************************************************************/
static void
grv_refresh_helper (gpointer key, gpointer value, gpointer user_data)
{
    GNCReconcileView *view = user_data;
    GNCQueryView *qview = GNC_QUERY_VIEW (view);

    if (!gnc_query_view_item_in_view (qview, key))
        g_hash_table_remove (view->reconciled, key);
}

void
gnc_reconcile_view_refresh (GNCReconcileView *view)
{
    GNCQueryView *qview;

    g_return_if_fail (view != NULL);
    g_return_if_fail (GNC_IS_RECONCILE_VIEW (view));

    qview = GNC_QUERY_VIEW (view);
    gnc_query_view_refresh (qview);

    /* Now verify that everything in the reconcile hash is still in qview */
    if (view->reconciled)
        g_hash_table_foreach (view->reconciled, grv_refresh_helper, view);
}


/********************************************************************\
 * gnc_reconcile_view_reconciled_balance                            *
 *   returns the reconciled balance of the view                     *
 *                                                                  *
 * Args: view - view to get reconciled balance of                   *
 * Returns: reconciled balance (gnc_numeric)                        *
\********************************************************************/
static void
grv_balance_hash_helper (gpointer key, gpointer value, gpointer user_data)
{
    Split *split = key;
    gnc_numeric *total = user_data;

    *total = gnc_numeric_add_fixed (*total, xaccSplitGetAmount (split));
}

gnc_numeric
gnc_reconcile_view_reconciled_balance (GNCReconcileView *view)
{
    gnc_numeric total = gnc_numeric_zero ();

    g_return_val_if_fail (view != NULL, total);
    g_return_val_if_fail (GNC_IS_RECONCILE_VIEW (view), total);

    if (view->reconciled == NULL)
        return total;

    g_hash_table_foreach (view->reconciled, grv_balance_hash_helper, &total);

    return gnc_numeric_abs (total);
}


/********************************************************************\
 * gnc_reconcile_view_commit                                        *
 *   Commit the reconcile information in the view. Only change the  *
 *   state of those items marked as reconciled.  All others should  *
 *   retain their previous state (none, cleared, voided, etc.).     *
 *                                                                  *
 * Args: view - view to commit                                      *
 *       date - date to set as the reconcile date                   *
 * Returns: nothing                                                 *
\********************************************************************/
static void
grv_commit_hash_helper (gpointer key, gpointer value, gpointer user_data)
{
    Split *split = key;
    time_t *date = user_data;

    xaccSplitSetReconcile (split, YREC);
    xaccSplitSetDateReconciledSecs (split, *date);
}

void
gnc_reconcile_view_commit (GNCReconcileView *view, time_t date)
{
    g_return_if_fail (view != NULL);
    g_return_if_fail (GNC_IS_RECONCILE_VIEW (view));

    if (view->reconciled == NULL)
        return;

    gnc_suspend_gui_refresh();
    g_hash_table_foreach (view->reconciled, grv_commit_hash_helper, &date);
    gnc_resume_gui_refresh();
}


/********************************************************************\
 * gnc_reconcile_view_postpone                                      *
 *   postpone the reconcile information in the view by setting      *
 *   reconciled splits to cleared status                            *
 *                                                                  *
 * Args: view - view to commit                                      *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_reconcile_view_postpone (GNCReconcileView *view)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;
    int           num_splits;
    int           i;
    gpointer      entry = NULL;

    g_return_if_fail (view != NULL);
    g_return_if_fail (GNC_IS_RECONCILE_VIEW (view));

    if (view->reconciled == NULL)
        return;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (GNC_QUERY_VIEW (view)));
    gtk_tree_model_get_iter_first (model, &iter);

    num_splits = gnc_query_view_get_num_entries (GNC_QUERY_VIEW (view));

    gnc_suspend_gui_refresh();
    for (i = 0; i < num_splits; i++)
    {
        char recn;

        gtk_tree_model_get (model, &iter, 0, &entry, -1);

        // Don't change splits past reconciliation date that haven't been
        // set to be reconciled
        if ( difftime(view->statement_date,
                      xaccTransGetDate (xaccSplitGetParent (entry))) >= 0 ||
                g_hash_table_lookup (view->reconciled, entry))
        {
            recn = g_hash_table_lookup (view->reconciled, entry) ? CREC : NREC;
            xaccSplitSetReconcile (entry, recn);
        }
        gtk_tree_model_iter_next (model, &iter);
    }
    gnc_resume_gui_refresh();
}


/********************************************************************\
 * gnc_reconcile_view_unselect_all                                  *
 *   unselect all splits in the view                                *
 *                                                                  *
 * Args: view - view to unselect all                                *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_reconcile_view_unselect_all(GNCReconcileView *view)
{
    g_return_if_fail (view != NULL);
    g_return_if_fail (GNC_IS_RECONCILE_VIEW (view));

    gnc_query_view_unselect_all (GNC_QUERY_VIEW (view));
}


/********************************************************************\
 * gnc_reconcile_view_changed                                       *
 *   returns true if any splits have been reconciled                *
 *                                                                  *
 * Args: view - view to get changed status for                      *
 * Returns: true if any reconciled splits                           *
\********************************************************************/
gboolean
gnc_reconcile_view_changed (GNCReconcileView *view)
{
    g_return_val_if_fail (view != NULL, FALSE);
    g_return_val_if_fail (GNC_IS_RECONCILE_VIEW (view), FALSE);

    return g_hash_table_size (view->reconciled) != 0;
}

