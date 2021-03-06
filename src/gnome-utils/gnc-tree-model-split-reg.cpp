/********************************************************************\
 * gnc-tree-model-split-reg.cpp -- GtkTreeView implementation to    *
 *                       display registers in a GtkTreeView.        *
 *                                                                  *
 * Copyright (C) 2006-2007 Chris Shoemaker <c.shoemaker@cox.net>    *
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
 *                                                                  *
\********************************************************************/

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include "gnc-tree-model-split-reg.h"
#include "gnc-component-manager.h"
#include "gnc-commodity.h"
#include "gnc-gconf-utils.h"
#include "gnc-engine.h"
#include "gnc-event.h"
#include "gnc-gobject-utils.h"
#include "Query.h"
#include "Transaction.h"
#include "Scrub.h"

#include "gnc-ui-util.h"
#include "engine-helpers.h"

#define TREE_MODEL_SPLIT_REG_CM_CLASS "tree-model-split-reg"

/* Signal codes */
enum
{
    REFRESH_VIEW,
    REFRESH_STATUS_BAR,
    SELECTION_MOVE_DELETE,
    SELECTION_MOVE_FILTER,
    LAST_SIGNAL
};

/** Static Globals *******************************************************/
static QofLogModule log_module = GNC_MOD_LEDGER;

/** Declarations *********************************************************/
static void gnc_tree_model_split_reg_class_init (GncTreeModelSplitRegClass *klass);
static void gnc_tree_model_split_reg_init (GncTreeModelSplitReg *model);
static void gnc_tree_model_split_reg_finalize (GObject *object);
static void gnc_tree_model_split_reg_dispose (GObject *object);

static guint gnc_tree_model_split_reg_signals[LAST_SIGNAL] = {0};

//static const gchar *iter_to_string (GtkTreeIter *iter);
#define iter_to_string(iter) ""

/** Implementation of GtkTreeModel  **************************************/
static void gnc_tree_model_split_reg_tree_model_init (GtkTreeModelIface *iface);

static GtkTreeModelFlags gnc_tree_model_split_reg_get_flags (GtkTreeModel *tree_model);
static int gnc_tree_model_split_reg_get_n_columns (GtkTreeModel *tree_model);
static GType gnc_tree_model_split_reg_get_column_type (GtkTreeModel *tree_model, int index);
static gboolean gnc_tree_model_split_reg_get_iter (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path);
static GtkTreePath *gnc_tree_model_split_reg_get_path (GtkTreeModel *tree_model, GtkTreeIter *iter);
static void gnc_tree_model_split_reg_get_value (GtkTreeModel *tree_model, GtkTreeIter *iter, int column, GValue *value);
static gboolean	gnc_tree_model_split_reg_iter_next (GtkTreeModel *tree_model, GtkTreeIter *iter);
static gboolean	gnc_tree_model_split_reg_iter_children (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent);
static gboolean	gnc_tree_model_split_reg_iter_has_child (GtkTreeModel *tree_model, GtkTreeIter *iter);
static int gnc_tree_model_split_reg_iter_n_children (GtkTreeModel *tree_model, GtkTreeIter *iter);
static gboolean	gnc_tree_model_split_reg_iter_nth_child (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent, int n);
static gboolean	gnc_tree_model_split_reg_iter_parent (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *child);
static void gtm_increment_stamp (GncTreeModelSplitReg *model);

/** Component Manager Callback ******************************************/
static void gnc_tree_model_split_reg_event_handler (QofInstance *entity, QofEventId event_type, GncTreeModelSplitReg *model, GncEventData *ed);

static void update_completion_models (GncTreeModelSplitReg *model);

/** The instance private data for the split register tree model. */
struct GncTreeModelSplitRegPrivate
{
    QofBook *book;              // GNC Book
    Account *anchor;            // Account of register

    GList *tlist;               // List of unique transactions derived from the query slist in same order

    Transaction *btrans;        // The Blank transaction

    Split *bsplit;              // The Blank split
    GList *bsplit_node;         // never added to any list, just for representation of the iter
    GList *bsplit_parent_node;  // this equals the tnode of the transaction with the blank split


    gboolean display_subacc;    // Are we displaying subaccounts
    gboolean display_gl;        // Is this the General ledger



/* vvvv ### This is stuff I have dumped here from old reg #### vvvv */

    /* The template account which template transaction should belong to */
    GncGUID template_account;

    /* User data for users of SplitRegisters */
    gpointer user_data; //FIXME This is used to get parent window, maybe move to view

    /* hook to get parent widget */
    SRGetParentCallback2 get_parent; //FIXME This is used to get parent window, maybe move to view

/* ^^^^ #### This is stuff I have dumped here from old reg #### ^^^^ */

    GtkListStore *description_list;  // description field autocomplete list
    GtkListStore *notes_list;        // notes field autocomplete list
    GtkListStore *memo_list;         // memo field autocomplete list
    GtkListStore *num_list;          // number combo list
    GtkListStore *numact_list;       // number / action combo list
    GtkListStore *account_list;      // Account combo list

    gint event_handler_id;
};


/* Define some background colors for the rows */
#define GREENROW "#BFDEB9"
#define TANROW "#F6FFDA"
#define SPLITROW "#EDE7D3"
#define YELLOWROW "#FFEF98"


#define TROW1 0x1 // Transaction row 1 depth 1
#define TROW2 0x2 // Transaction row 2 depth 2
#define SPLIT 0x4 // Split row         depth 3
#define BLANK 0x8 // Blank tow
#define IS_TROW1(x) (GPOINTER_TO_INT((x)->user_data) & TROW1)
#define IS_TROW2(x) (GPOINTER_TO_INT((x)->user_data) & TROW2)
#define IS_BLANK(x) (GPOINTER_TO_INT((x)->user_data) & BLANK)
#define IS_SPLIT(x) (GPOINTER_TO_INT((x)->user_data) & SPLIT)
#define IS_BLANK_SPLIT(x) (IS_BLANK(x) && IS_SPLIT(x))
#define IS_BLANK_TRANS(x) (IS_BLANK(x) && !IS_SPLIT(x))
/* Meaning of user_data fields in iter struct:
 *
 * user_data:  a bitfield for TROW1, TROW2, SPLIT, BLANK
 * user_data2: a pointer to a node in a GList of Transactions               
 * user_data3: a pointer to a node in a GList of Splits.
 *            
 */


/*FIXME This is the original define 
#define VALID_ITER(model, iter) \
    (GNC_IS_TREE_MODEL_TRANSACTION(model) &&                            \
     ((iter) && (iter)->user_data2) &&                                  \
     ((iter)->stamp == (model)->stamp) &&                               \
     (!IS_SPLIT(iter) ^ ((iter)->user_data3 != NULL)) &&                \
     (!IS_BLANK_SPLIT(iter) ||                                          \
      ((iter)->user_data2 == (model)->priv->bsplit_parent_node))        \
     )
*/

/*FIXME I thought this would work, it does not ????????? */
/* Do we need to test for a valid iter every where, is it enougth to test on make iter ? */
#define VALID_ITER (model, iter) \
 (GNC_IS_TREE_MODEL_SPLIT_REG (model) && \
 ((iter).user_data != NULL) && ((iter).user_data2 != NULL) && (model->stamp == (gint)(iter).stamp) && \
 ( (IS_SPLIT (iter) && (iter).user_data3) || (IS_BLANK_SPLIT (iter) && ((GList *)(iter).user_data2 == model->priv->bsplit_parent_node)) || \
   (!IS_SPLIT (iter) && (iter).user_data2) || (IS_BLANK_TRANS (iter) && (iter).user_data3 == NULL)))

static void* substitute_g_list_find(SplitList_t& slist, Split* split)
{
    SplitList_t::iterator * it = new SplitList_t::iterator;  // TODO someone should free this
    *it = std::find (slist.begin(), slist.end(), split);
    if(*it == slist.end())
    {
        return NULL;
    }
    else
    {
        return it;
    }
}


/* Used in the sort functions */
gboolean
gnc_tree_model_split_reg_is_blank_trans (GncTreeModelSplitReg *model, GtkTreeIter *iter)
{
    return IS_BLANK_TRANS (iter);
}


/* Validate the iter */
static gboolean
gtm_valid_iter (GncTreeModelSplitReg *model, GtkTreeIter *iter)
{
    if (GNC_IS_TREE_MODEL_SPLIT_REG (model) && (iter->user_data != NULL) && (iter->user_data2 != NULL) && (model->stamp == (gint)iter->stamp)
          && ( (IS_SPLIT (iter) && iter->user_data3) || (IS_BLANK_SPLIT (iter) && ((GList *)iter->user_data2 == model->priv->bsplit_parent_node))
          ||  (!IS_SPLIT (iter) && iter->user_data2) || (IS_BLANK_TRANS (iter) && iter->user_data3 == NULL)))
        return TRUE;
    else
        return FALSE;
}


/* Make an iter from the given parameters */
static GtkTreeIter
gtm_make_iter (GncTreeModelSplitReg *model, gint f, GList *tnode, void *snode)
{
    GtkTreeIter iter, *iter_p;
    iter_p = &iter;
    iter.stamp = model->stamp;
    iter.user_data = GINT_TO_POINTER(f);
    iter.user_data2 = tnode;
    iter.user_data3 = snode;

//FIXME If I use this in place of 'if' below it works ??????
//    if (!(GNC_IS_TREE_MODEL_SPLIT_REG (model) && (iter_p->user_data != NULL) && (iter_p->user_data2 != NULL) && (model->stamp == (gint)iter_p->stamp)
//          && ( (IS_SPLIT (iter_p) && iter_p->user_data3) || (IS_BLANK_SPLIT (iter_p) && ((GList *)iter_p->user_data2 == model->priv->bsplit_parent_node))
//          ||  (!IS_SPLIT (iter_p) && iter_p->user_data2) || (IS_BLANK_TRANS (iter_p) && iter_p->user_data3 == NULL) )))

//    if (!VALID_ITER (model, &iter))

    if (!(gtm_valid_iter (model, iter_p)))
        PERR ("Making invalid iter %s", iter_to_string (iter_p));
    return iter;
}


#define GNC_TREE_MODEL_SPLIT_REG_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNC_TYPE_TREE_MODEL_SPLIT_REG, GncTreeModelSplitRegPrivate))

/************************************************************/
/*               g_object required functions                */
/************************************************************/

/** A pointer to the parent class of the split register tree model. */
static GtkObjectClass *parent_class = NULL;

GType
gnc_tree_model_split_reg_get_type (void)
{
    static GType gnc_tree_model_split_reg_type = 0;

    if (gnc_tree_model_split_reg_type == 0)
    {
        static const GTypeInfo our_info =
        {
            sizeof (GncTreeModelSplitRegClass),                 /* class_size */
            NULL,                                               /* base_init */
            NULL,                                               /* base_finalize */
            (GClassInitFunc) gnc_tree_model_split_reg_class_init,
            NULL,                                               /* class_finalize */
            NULL,                                               /* class_data */
            sizeof (GncTreeModelSplitReg),                      /* */
            0,                                                  /* n_preallocs */
            (GInstanceInitFunc) gnc_tree_model_split_reg_init
        };

        static const GInterfaceInfo tree_model_info =
        {
            (GInterfaceInitFunc) gnc_tree_model_split_reg_tree_model_init,
            NULL,
            NULL
        };

        gnc_tree_model_split_reg_type = g_type_register_static (GNC_TYPE_TREE_MODEL,
                                      GNC_TREE_MODEL_SPLIT_REG_NAME,
                                      &our_info, 0);

        g_type_add_interface_static (gnc_tree_model_split_reg_type,
                                     GTK_TYPE_TREE_MODEL,
                                     &tree_model_info);
    }

    return gnc_tree_model_split_reg_type;
}


static void
gnc_tree_model_split_reg_class_init (GncTreeModelSplitRegClass *klass)
{
    GObjectClass *o_class;

    parent_class = g_type_class_peek_parent (klass);

    o_class = G_OBJECT_CLASS (klass);

    /* GObject signals */
    o_class->finalize = gnc_tree_model_split_reg_finalize;
    o_class->dispose = gnc_tree_model_split_reg_dispose;

    gnc_tree_model_split_reg_signals[REFRESH_VIEW] =
        g_signal_new("refresh_view",
                     G_TYPE_FROM_CLASS (o_class),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET (GncTreeModelSplitRegClass, refresh_view),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    gnc_tree_model_split_reg_signals[REFRESH_STATUS_BAR] =
        g_signal_new("refresh_status_bar",
                     G_TYPE_FROM_CLASS (o_class),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET (GncTreeModelSplitRegClass, refresh_status_bar),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    gnc_tree_model_split_reg_signals[SELECTION_MOVE_DELETE] =
        g_signal_new("selection_move_delete",
                     G_TYPE_FROM_CLASS (o_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET (GncTreeModelSplitRegClass, selection_move_delete),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_POINTER);

    gnc_tree_model_split_reg_signals[SELECTION_MOVE_FILTER] =
        g_signal_new("selection_move_filter",
                     G_TYPE_FROM_CLASS (o_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET (GncTreeModelSplitRegClass, selection_move_filter),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_POINTER);

    klass->refresh_view = NULL;
    klass->refresh_status_bar = NULL;
    klass->selection_move_delete = NULL;
    klass->selection_move_filter = NULL;
}


static void
gnc_tree_model_split_reg_gconf_changed (GConfEntry *entry, gpointer user_data)
{
    GncTreeModelSplitReg *model = user_data;

    g_return_if_fail (entry && entry->key);

    if (model == NULL)
        return;
//g_print("gnc_tree_model_split_reg_gconf_changed\n");

    if (g_str_has_suffix (entry->key, KEY_ACCOUNTING_LABELS))
    {
        // FIXME This only works on create, dynamic ? 
        model->use_accounting_labels = gnc_gconf_get_bool (GCONF_GENERAL, KEY_ACCOUNTING_LABELS, NULL);

//g_print("model->use_accounting_labels changed %d\n", model->use_accounting_labels);
    }
    else if (g_str_has_suffix (entry->key, KEY_ACCOUNT_SEPARATOR))
    {
        model->separator_changed = TRUE; // FIXME Not dealt with this
    }
    else
    {
        g_warning("gnc_tree_model_split_reg_gconf_changed: Unknown gconf key %s", entry->key);
    }
}


static void
gnc_tree_model_split_reg_init (GncTreeModelSplitReg *model)
{
    GncTreeModelSplitRegPrivate *priv;

    ENTER("model %p", model);
    while (model->stamp == 0)
    {
        model->stamp = g_random_int ();
    }

    model->priv = g_new0 (GncTreeModelSplitRegPrivate, 1);

    gnc_gconf_general_register_cb (KEY_ACCOUNTING_LABELS,
                                  gnc_tree_model_split_reg_gconf_changed,
                                  model);
    gnc_gconf_general_register_cb (KEY_ACCOUNT_SEPARATOR,
                                  gnc_tree_model_split_reg_gconf_changed,
                                  model);
    LEAVE(" ");
}


static void
gnc_tree_model_split_reg_finalize (GObject *object)
{
    GncTreeModelSplitReg *model;

    ENTER("model split reg %p", object);
    g_return_if_fail (object != NULL);
    g_return_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (object));

    model = GNC_TREE_MODEL_SPLIT_REG (object);

    if (G_OBJECT_CLASS (parent_class)->finalize)
        G_OBJECT_CLASS (parent_class)->finalize (object);
    LEAVE(" ");
}


static void
gnc_tree_model_split_reg_dispose (GObject *object)
{
    GncTreeModelSplitRegPrivate *priv;
    GncTreeModelSplitReg *model;

    ENTER("model split reg %p", object);
    g_return_if_fail (object != NULL);
    g_return_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (object));

    model = GNC_TREE_MODEL_SPLIT_REG (object);
    priv = model->priv;

    if (priv->event_handler_id)
    {
        qof_event_unregister_handler (priv->event_handler_id);
        priv->event_handler_id = 0;
    }

    priv->book = NULL;
    g_list_free (priv->tlist);
    priv->tlist = NULL;

    /* Free the blank split */
    priv->bsplit = NULL;
    priv->bsplit_node = NULL;

    /* Free the blank transaction */
    priv->btrans = NULL;

/*FIXME Other stuff here */

    g_free (priv);

    if (G_OBJECT_CLASS (parent_class)->dispose)
        G_OBJECT_CLASS (parent_class)->dispose (object);
    LEAVE(" ");
}


/************************************************************/
/*                   New Model Creation                     */
/************************************************************/
/* Create a new tree model */
GncTreeModelSplitReg *
gnc_tree_model_split_reg_new (SplitRegisterType2 reg_type, SplitRegisterStyle2 style,
                        gboolean use_double_line, gboolean is_template)
{
    GncTreeModelSplitReg *model;
    GncTreeModelSplitRegPrivate *priv;
    const GList *item;

    ENTER("Create Model");

    model = g_object_new (GNC_TYPE_TREE_MODEL_SPLIT_REG, NULL);

    priv = model->priv;
    priv->book = gnc_get_current_book();
    priv->display_gl = FALSE;
    priv->display_subacc = FALSE;

    model->type = reg_type;
    model->style = style;
    model->use_double_line = use_double_line;
    model->is_template = is_template;

    /* Setup the blank transaction */
    priv->btrans = xaccMallocTransaction (priv->book);

    /* Setup the blank split */
    priv->bsplit = xaccMallocSplit (priv->book);
    priv->bsplit_node = g_list_append (NULL, priv->bsplit);

    /* Setup some config entries */
    model->use_accounting_labels = gnc_gconf_get_bool (GCONF_GENERAL, KEY_ACCOUNTING_LABELS, NULL);
    model->use_theme_colors = gnc_gconf_get_bool (GCONF_GENERAL_REGISTER, "use_theme_colors", NULL);
    model->alt_colors_by_txn = gnc_gconf_get_bool (GCONF_GENERAL_REGISTER, "alternate_color_by_transaction", NULL);
    model->read_only = FALSE;

    /* Create the ListStores for the auto completion / combo's */
    priv->description_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
    priv->notes_list = gtk_list_store_new (1, G_TYPE_STRING);
    priv->memo_list = gtk_list_store_new (1, G_TYPE_STRING);
    priv->num_list = gtk_list_store_new (1, G_TYPE_STRING);
    priv->numact_list = gtk_list_store_new (1, G_TYPE_STRING);
    priv->account_list = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER );

    priv->event_handler_id = qof_event_register_handler
                             ((QofEventHandler)gnc_tree_model_split_reg_event_handler, model);

    LEAVE("model %p", model);
    return model;
}


/* ForEach function to remove all model entries */
static gboolean
gtm_remove_foreach_func (GtkTreeModel *model,
              GtkTreePath  *path,
              GtkTreeIter  *iter,
              GList       **rowref_list)
{
    GtkTreeRowReference  *rowref;
    g_assert ( rowref_list != NULL );

    rowref = gtk_tree_row_reference_new (model, path);
    *rowref_list = g_list_append (*rowref_list, rowref);

    return FALSE; /* do not stop walking the store, call us with next row */
}


/* Remove all model entries */
static void
gtm_remove_all_rows (GncTreeModelSplitReg *model)
{
    GList *rr_list = NULL;    /* list of GtkTreeRowReferences to remove */
    GList *node;

    gtk_tree_model_foreach (GTK_TREE_MODEL(model), (GtkTreeModelForeachFunc)gtm_remove_foreach_func, &rr_list);

    rr_list = g_list_reverse (rr_list);

    for ( node = rr_list;  node != NULL;  node = node->next )
    {
        GtkTreePath *path;
        path = gtk_tree_row_reference_get_path ((GtkTreeRowReference*)node->data);

        if (path)
        {
            gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
            gtk_tree_path_free (path);
        }
    }
    g_list_foreach (rr_list, (GFunc) gtk_tree_row_reference_free, NULL);
    g_list_free (rr_list);
}


/* Load the model with unique transactions based on a GList of splits */
void
gnc_tree_model_split_reg_load (GncTreeModelSplitReg *model, GList *slist, Account *default_account)
{
    GncTreeModelSplitRegPrivate *priv;

    ENTER("Load ModelSplitReg = %p and slist length is %d", model, g_list_length (slist));

    priv = model->priv;

    /* Clear the treeview */
    gtm_remove_all_rows (model);
    priv->tlist = NULL;

    /* Get a list of Unique Transactions from an slist */
    priv->tlist = xaccSplitListGetUniqueTransactions (slist);

    /* Add the blank transaction to the tlist */
    priv->tlist = g_list_append (priv->tlist, priv->btrans);

    /* Update the completion model liststores */
    gnc_tree_model_split_reg_update_completion (model);

    priv->anchor = default_account;
    priv->bsplit_parent_node = NULL;

    LEAVE("Leave Model Load");
}


/*FIXME Not sure about this */
void
gnc_tree_model_split_reg_set_template_account (GncTreeModelSplitReg *model, Account *template_account)
{
    GncTreeModelSplitRegPrivate *priv;

//g_print("gnc_tree_model_split_reg_set_template_account\n");
    priv = model->priv;
    priv->template_account = *xaccAccountGetGUID (template_account);
}


/* Destroy the model */
void
gnc_tree_model_split_reg_destroy (GncTreeModelSplitReg *model)
{

    ENTER("Model is %p", model);

    gnc_gconf_general_remove_cb (KEY_ACCOUNTING_LABELS,
                                gnc_tree_model_split_reg_gconf_changed,
                                model);
    gnc_gconf_general_remove_cb (KEY_ACCOUNT_SEPARATOR,
                                gnc_tree_model_split_reg_gconf_changed,
                                model);
    LEAVE(" ");
}


/* Setup the data to obtain the parent window */
void
gnc_tree_model_split_reg_set_data (GncTreeModelSplitReg *model, gpointer user_data,
                                  SRGetParentCallback2 get_parent)
{
    GncTreeModelSplitRegPrivate *priv;

/*FIXME This is used to get the parent window, mabe move to view */
    priv = model->priv;

    priv->user_data = user_data;
    priv->get_parent = get_parent;
}


/* Update the config of this model */
void
gnc_tree_model_split_reg_config (GncTreeModelSplitReg *model, SplitRegisterType2 newtype,
                                 SplitRegisterStyle2 newstyle, gboolean use_double_line)
{
    model->type = newtype;

    if (model->type >= NUM_SINGLE_REGISTER_TYPES2)
        newstyle = REG2_STYLE_JOURNAL;

    model->style = newstyle;
    model->use_double_line = use_double_line;
}


//FIXME this may not be required in the long run, return TRUE if this is a sub account view
gboolean
gnc_tree_model_split_reg_get_sub_account (GncTreeModelSplitReg *model)
{
    return model->priv->display_subacc;
}

/************************************************************/
/*        Gnc Tree Model Debugging Utility Function         */
/************************************************************/
//#define ITER_STRING_LEN 128
//
//static const gchar *
//iter_to_string (GtkTreeIter *iter)
//{
//#ifdef G_THREADS_ENABLED
//
//    static GPrivate gtmits_buffer_key = G_PRIVATE_INIT(g_free);
//    gchar *string;
//
//    string = g_private_get (&gtmits_buffer_key);
//    if (string == NULL)
//    {
//        string = g_malloc(ITER_STRING_LEN + 1);
//        g_private_set (&gtmits_buffer_key, string);
//    }
//#else
//    static char string[ITER_STRING_LEN + 1];
//#endif
//
//    if (iter)
//        snprintf(
//            string, ITER_STRING_LEN,
//            "[stamp:%x data:%d, %p (%p:%s), %p (%p:%s)]",
//            iter->stamp, GPOINTER_TO_INT(iter->user_data),
//            iter->user_data2,
//            iter->user_data2 ? ((GList *) iter->user_data2)->data : 0,
//            iter->user_data2 ?
//            (QOF_INSTANCE(((GList *) iter->user_data2)->data))->e_type : "",
//            iter->user_data3,
//            iter->user_data3 ? ((GList *) iter->user_data3)->data : 0,
//            iter->user_data3 ?
//            (QOF_INSTANCE(((GList *) iter->user_data3)->data))->e_type : "");
//    else
//        strcpy(string, "(null)");
//    return string;
//}


/************************************************************/
/*       Gtk Tree Model Required Interface Functions        */
/************************************************************/
static void
gnc_tree_model_split_reg_tree_model_init (GtkTreeModelIface *iface)
{
    iface->get_flags       = gnc_tree_model_split_reg_get_flags;
    iface->get_n_columns   = gnc_tree_model_split_reg_get_n_columns;
    iface->get_column_type = gnc_tree_model_split_reg_get_column_type;
    iface->get_iter        = gnc_tree_model_split_reg_get_iter;
    iface->get_path        = gnc_tree_model_split_reg_get_path;
    iface->get_value       = gnc_tree_model_split_reg_get_value;
    iface->iter_next       = gnc_tree_model_split_reg_iter_next;
    iface->iter_children   = gnc_tree_model_split_reg_iter_children;
    iface->iter_has_child  = gnc_tree_model_split_reg_iter_has_child;
    iface->iter_n_children = gnc_tree_model_split_reg_iter_n_children;
    iface->iter_nth_child  = gnc_tree_model_split_reg_iter_nth_child;
    iface->iter_parent     = gnc_tree_model_split_reg_iter_parent;
}


static GtkTreeModelFlags
gnc_tree_model_split_reg_get_flags (GtkTreeModel *tree_model)
{
    /* Returns a set of flags supported by this interface. The flags
       are a bitwise combination of GtkTreeModelFlags. The flags supported
       should not change during the lifecycle of the tree_model. */
    return 0;
}


static int
gnc_tree_model_split_reg_get_n_columns (GtkTreeModel *tree_model)
{
    /* Returns the number of columns supported by tree_model. */
    g_return_val_if_fail(GNC_IS_TREE_MODEL_SPLIT_REG(tree_model), -1);

    return GNC_TREE_MODEL_SPLIT_REG_NUM_COLUMNS;
}


static GType
gnc_tree_model_split_reg_get_column_type (GtkTreeModel *tree_model, int index)
{
    /* Returns the type of the column. */
    g_return_val_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (tree_model), G_TYPE_INVALID);
    g_return_val_if_fail ((index < GNC_TREE_MODEL_SPLIT_REG_NUM_COLUMNS) && (index >= 0), G_TYPE_INVALID);

    switch (index)
    {
    case GNC_TREE_MODEL_SPLIT_REG_COL_GUID:
        return G_TYPE_POINTER;

    case GNC_TREE_MODEL_SPLIT_REG_COL_DATE:
    case GNC_TREE_MODEL_SPLIT_REG_COL_DUEDATE:
    case GNC_TREE_MODEL_SPLIT_REG_COL_NUMACT:
    case GNC_TREE_MODEL_SPLIT_REG_COL_DESCNOTES:
    case GNC_TREE_MODEL_SPLIT_REG_COL_TRANSVOID:
    case GNC_TREE_MODEL_SPLIT_REG_COL_RECN:
    case GNC_TREE_MODEL_SPLIT_REG_COL_DEBIT:
    case GNC_TREE_MODEL_SPLIT_REG_COL_CREDIT:
        return G_TYPE_STRING;

    case GNC_TREE_MODEL_SPLIT_REG_COL_RO:
    case GNC_TREE_MODEL_SPLIT_REG_COL_VIS:
        return G_TYPE_BOOLEAN;

    default:
        g_assert_not_reached ();
        return G_TYPE_INVALID;
    }
}


static gboolean
gnc_tree_model_split_reg_get_iter (GtkTreeModel *tree_model,
                                 GtkTreeIter *iter,
                                 GtkTreePath *path)
{
    /* Sets iter to a valid iterator pointing to path. */
    GncTreeModelSplitReg *model = GNC_TREE_MODEL_SPLIT_REG (tree_model);
    GList *tnode;
    void *snode;
    Split *split;
    gint depth, *indices, flags = 0;

    g_return_val_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (tree_model), FALSE);

    {
        gchar *path_string = gtk_tree_path_to_string (path);
        ENTER("model %p, iter %p, path %s", tree_model, iter, path_string);
        g_free (path_string);
    }

    depth = gtk_tree_path_get_depth (path);

    indices = gtk_tree_path_get_indices (path);

    tnode = g_list_nth (model->priv->tlist, indices[0]);

    SplitList_t slist;
    if (!tnode) {
        DEBUG("path index off end of tlist");
        goto fail;
    }

    slist = xaccTransGetSplitList (tnode->data);

    if (depth == 1) {      /* Trans Row 1 */
        flags = TROW1;
        /* Check if this is the blank trans */
        if (tnode->data == model->priv->btrans) {
            flags |= BLANK;
            snode = NULL;
        }
        else
        {
            split = xaccTransGetSplit (tnode->data, 0); // else first split
            snode = substitute_g_list_find(slist, split);
        }


    }
    else if (depth == 2) { /* Trans Row 2 */
        flags = TROW2;
        /* Check if this is the blank trans */
        if (tnode->data == model->priv->btrans) {
            flags |= BLANK;
            snode = NULL;
        }
        else
        {
            split = xaccTransGetSplit (tnode->data, 0); // else first split
            snode = substitute_g_list_find(slist, split);
        }
    }
    else if (depth == 3) /* Split */
    {        
        flags = SPLIT;

        /* Check if this is the blank split */
        if ((tnode == model->priv->bsplit_parent_node) && (xaccTransCountSplits (tnode->data) == indices[2]))
        {
            flags |= BLANK;
            snode = model->priv->bsplit_node;
        }
        else
        {
            split = xaccTransGetSplit (tnode->data, indices[2]);
            snode = substitute_g_list_find(slist, split);
        }

        if (!snode) {
            DEBUG("path index off end of slist");
            goto fail;
        }
    }

    else {
        DEBUG("Invalid path depth");
        goto fail;
    }

    *iter = gtm_make_iter (model, flags, tnode, snode);
/*    g_assert(VALID_ITER(model, iter)); */
    LEAVE("True");
    return TRUE;
 fail:
    iter->stamp = 0;
    LEAVE("False");
    return FALSE;
}


static GtkTreePath *
gnc_tree_model_split_reg_get_path (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
    /* Returns a newly-created GtkTreePath referenced by iter. 
       This path should be freed with gtk_tree_path_free(). */
    GncTreeModelSplitReg *model = GNC_TREE_MODEL_SPLIT_REG (tree_model);
    GtkTreePath *path;
    gint tpos, spos;
    GList *tnode;

    g_return_val_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (model), NULL);

    ENTER("model %p, iter %s", model, iter_to_string (iter));
/*    g_assert(VALID_ITER(model, iter)); */

    path = gtk_tree_path_new();

    tnode = iter->user_data2;


    /* Level 1 */
    tpos = g_list_position (model->priv->tlist, tnode);

    if (tpos == -1)
        goto fail;

    gtk_tree_path_append_index (path, tpos);

    /* Level 2 - All ways 0 */
    if (IS_TROW2 (iter))
        gtk_tree_path_append_index (path, 0);

    /* Level 3 */
    if (IS_SPLIT (iter))
    {
        /* Check if this is the blank split */
        if ((tnode == model->priv->bsplit_parent_node) && (IS_BLANK (iter)))
        {
            spos = xaccTransCountSplits (tnode->data);
        }
        else
        {
            /* Can not use snode position directly as slist length does not follow
               number of splits exactly, especailly if you delete a split */
           SplitList_t::iterator *snode = iter->user_data3;
           spos = xaccTransGetSplitIndex (tnode->data, *(*snode));
        }

        if (spos == -1)
            goto fail;

        gtk_tree_path_append_index (path, 0); /* Add the Level 2 part */
        gtk_tree_path_append_index (path, spos);
    }

    {
        gchar *path_string = gtk_tree_path_to_string (path);
        LEAVE("get path  %s", path_string);
        g_free (path_string);
    }
    return path;

 fail:
    LEAVE("No Valid Path");
    return NULL;
}


/* Return the split account for which ancestor is it's parent */
static Account *
gtm_trans_get_account_for_splits_ancestor (const Transaction *trans, const Account *ancestor)
{
    SplitList_t slist = xaccTransGetSplitList (trans);
    for (SplitList_t::iterator node = slist.begin(); node != slist.end(); node++)
    {
        Split *split = *node;
        Account *split_acc = xaccSplitGetAccount(split);

        if (!xaccTransStillHasSplit(trans, split))
            continue;

        if (ancestor && xaccAccountHasAncestor(split_acc, ancestor))
            return split_acc;
    }
    return NULL;
}


/* Return FALSE if this row should not be shown */
static gboolean
gnc_tree_model_split_reg_get_vis (GncTreeModelSplitReg *model, Transaction *trans, gboolean trow1)
{
    GList *tnode, *tnode_last = NULL;
    gboolean return_value = FALSE;
    char chars[6];
    int i = 0;
    time64 date_posted;
    Account *account;

    tnode = g_list_find (model->priv->tlist, trans); // row trans
    tnode_last = g_list_last (model->priv->tlist); // Blank trans

    if (tnode == tnode_last) // Blank trans is always visable.
       return TRUE;

    // Get the posted date
    date_posted = xaccTransGetDate (trans);

    // Test for posted date earlier than start date.
    if ((date_posted < model->filter_start_time) && trow1 == TRUE)
    {
        g_signal_emit_by_name (model, "selection_move_filter", trans);
        return FALSE;
    }

    // Test for posted date later than end date.
    if ((model->filter_end_time > 0) && (date_posted > model->filter_end_time) && trow1 == TRUE)
    {
        g_signal_emit_by_name (model, "selection_move_filter", trans);
        return FALSE;
    }

    // Test for default status flag.
    if (model->filter_cleared_match == CLEARED_ALL)
        return TRUE;

    // Test for sub account register, if so, check split accounts against ancestor
    if (model->priv->display_subacc)
        account = gtm_trans_get_account_for_splits_ancestor (trans, model->priv->anchor);
    else
        account = model->priv->anchor;

     // Test for the status field.
    if (model->filter_cleared_match & CLEARED_CLEARED)
        chars[i++] = CREC;
    if (model->filter_cleared_match & CLEARED_RECONCILED)
        chars[i++] = YREC;
    if (model->filter_cleared_match & CLEARED_FROZEN)
        chars[i++] = FREC;
    if (model->filter_cleared_match & CLEARED_NO)
        chars[i++] = NREC;
    if (model->filter_cleared_match & CLEARED_VOIDED)
        chars[i++] = VREC;
    chars[i] = '\0';

    // Loop through splits checking state.
    for (i = 0; i < 5; i++)
    {
        if (xaccTransHasSplitsInStateByAccount (trans, chars[i], account))
        {
            return_value = TRUE;
            break;
        }
    }

    if (return_value == FALSE && trow1 == TRUE)
        g_signal_emit_by_name (model, "selection_move_filter", trans);

    return return_value;
}


/* Return TRUE if this row should be marked read only */
gboolean
gnc_tree_model_split_reg_get_read_only (GncTreeModelSplitReg *model, Transaction *trans)
{
    QofBook *book;
    GList *tnode, *tnode_last = NULL;

    book = gnc_get_current_book ();

    tnode = g_list_find (model->priv->tlist, trans);
    tnode_last = g_list_last (model->priv->tlist);

    if (qof_book_is_readonly (book)) // book is read only
        return TRUE;

    if (model->read_only) // register is read only
        return TRUE;

    /* Voided Transaction. */
    if (xaccTransHasSplitsInState (trans, VREC))
        return TRUE;

    if (qof_book_uses_autoreadonly (book)) // use auto read only
    {
        if (tnode == tnode_last) // blank transaction
            return FALSE;
        else
            return xaccTransIsReadonlyByPostedDate (trans);
    }
    return FALSE;
}


/* Returns the row color */
gchar*
gnc_tree_model_split_reg_get_row_color (GncTreeModelSplitReg *model, gboolean is_trow1, gboolean is_trow2, gboolean is_split, gint num)
{

    gchar *cell_color = NULL;

    if (!model->use_theme_colors)
    {
        if (model->use_double_line)
        {
            if (model->alt_colors_by_txn)
            {
                if (num % 2 == 0)
                {
                    if (is_trow1 || is_trow2)
                        cell_color = (gchar*)GREENROW;
                }
                else 
                {
                    if (is_trow1 || is_trow2)
                        cell_color = (gchar*)TANROW;
                }
            }
            else
            {
                if (is_trow1)
                    cell_color = (gchar*)GREENROW;
                else if (is_trow2)
                    cell_color = (gchar*)TANROW;
            }
        }
        else
        {
            if (num % 2 == 0)
            {
                if (is_trow1)
                    cell_color = (gchar*)GREENROW;
                else if (is_trow2)
                    cell_color = (gchar*)TANROW;
            }
            else
            {
                if (is_trow1)
                    cell_color = (gchar*)TANROW;
                else if (is_trow2)
                    cell_color = (gchar*)GREENROW;
            }
        }
        if (is_split)
            cell_color = (gchar*)SPLITROW;
    }
    else
        cell_color = (gchar*)NULL;

    return cell_color;
}


static void
gnc_tree_model_split_reg_get_value (GtkTreeModel *tree_model,
                                  GtkTreeIter *iter,
                                  int column,
                                  GValue *value)
{
    /* Initializes and sets value to that at column. When done with value,
       g_value_unset() needs to be called to free any allocated memory. */
    GncTreeModelSplitReg *model = GNC_TREE_MODEL_SPLIT_REG (tree_model);
    const GncGUID *guid;
    GList *tnode;
    gint depth, *indices;

    g_return_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (model));

    ENTER("model %p, iter %s, col %d", tree_model, iter_to_string (iter), column);

    tnode = (GList *) iter->user_data2;

    g_value_init (value, gnc_tree_model_split_reg_get_column_type (tree_model, column));

    switch (column)
    {
    case GNC_TREE_MODEL_SPLIT_REG_COL_GUID:
        guid = qof_entity_get_guid (QOF_INSTANCE (tnode->data));
        g_value_set_pointer (value, (gpointer) guid);
        break;

    case GNC_TREE_MODEL_SPLIT_REG_COL_DATE:
        break;

    case GNC_TREE_MODEL_SPLIT_REG_COL_DUEDATE:
        break;

    case GNC_TREE_MODEL_SPLIT_REG_COL_NUMACT:
        break;

    case GNC_TREE_MODEL_SPLIT_REG_COL_DESCNOTES:
        break;

    case GNC_TREE_MODEL_SPLIT_REG_COL_TRANSVOID:
        break;

    case GNC_TREE_MODEL_SPLIT_REG_COL_RECN:
        break;

    case GNC_TREE_MODEL_SPLIT_REG_COL_DEBIT:
        break;

    case GNC_TREE_MODEL_SPLIT_REG_COL_CREDIT:
        break;

    case GNC_TREE_MODEL_SPLIT_REG_COL_RO:
            g_value_set_boolean (value, gnc_tree_model_split_reg_get_read_only (model, tnode->data));
        break;

    case GNC_TREE_MODEL_SPLIT_REG_COL_VIS:
            g_value_set_boolean (value, gnc_tree_model_split_reg_get_vis (model, tnode->data, IS_TROW1(iter)));
        break;

    default:
        g_assert_not_reached ();
    }
    LEAVE(" ");
}


static gboolean
gnc_tree_model_split_reg_iter_next (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
    /* Sets iter to point to the node following it at the current level.
       If there is no next iter, FALSE is returned and iter is set to be
       invalid */
    GncTreeModelSplitReg *model = GNC_TREE_MODEL_SPLIT_REG (tree_model);
    Split *split;
    SplitList_t slist;
    GList *tnode = NULL;
    gint flags = 0;
    void *snode = NULL;

    g_return_val_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (model), FALSE);

    ENTER("model %p, iter %s", tree_model, iter_to_string (iter));

    if (IS_BLANK (iter)) {
        LEAVE("Blanks never have a next");
        goto fail;
    }

    if (IS_TROW2 (iter)) {
        LEAVE("Transaction row 2 never has a next");
        goto fail;
    }

    if (IS_TROW1 (iter)) {
        flags = TROW1;
        tnode = iter->user_data2;
        tnode = g_list_next (tnode);

        if (!tnode) {
           LEAVE("last trans has no next");
           goto fail;
        }

        slist = xaccTransGetSplitList (tnode->data);

        /* Check if this is the blank trans */
        if (tnode->data == model->priv->btrans) {
            flags |= BLANK;
            snode = NULL;
        }
        else
        {
            split = xaccTransGetSplit (tnode->data, 0);
            snode = substitute_g_list_find(slist, split);
        }
    }

    if (IS_SPLIT (iter)) {

        gint i = 0;
        flags = SPLIT;
        tnode = iter->user_data2;

        slist = xaccTransGetSplitList (tnode->data);
        snode = iter->user_data3;
        SplitList_t::iterator * it = snode;

        i = xaccTransGetSplitIndex (tnode->data, *(*it));
        i++;
        split = xaccTransGetSplit (tnode->data, i);

        delete it;
        it = new SplitList_t::iterator;  // TODO someone should free this.
        *it = std::find (slist.begin(), slist.end(), split);
        snode = it;

        if (*it == slist.end()) {
            if (tnode == model->priv->bsplit_parent_node) {
                snode = model->priv->bsplit_node;
                flags |= BLANK;
            } else {
                LEAVE("Last non-blank split has no next");
                goto fail;
            }
        }
    }

    *iter = gtm_make_iter (model, flags, tnode, snode);
    LEAVE("iter %s", iter_to_string (iter));
    return TRUE;
 fail:
    iter->stamp = 0;
    return FALSE;
}


static gboolean
gnc_tree_model_split_reg_iter_children (GtkTreeModel *tree_model,
                                      GtkTreeIter *iter,
                                      GtkTreeIter *parent_iter)
{
    /* Sets iter to point to the first child of parent. If parent has no children,
       FALSE is returned and iter is set to be invalid. Parent will remain a valid
       node after this function has been called. */
    GncTreeModelSplitReg *model = GNC_TREE_MODEL_SPLIT_REG (tree_model);
    GList *tnode = NULL, *snode = NULL;
    gint flags = 0;
    Split *split;
    SplitList_t slist;

    g_return_val_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (tree_model), FALSE);
    ENTER("model %p, iter %p , parent %s",
          tree_model, iter, (parent_iter ? iter_to_string (parent_iter) : "(null)"));

    if (!parent_iter)
    {
        /* Get the very first iter */
        tnode = g_list_first (model->priv->tlist);
        if (tnode)
        {
            flags = TROW1;
            slist = xaccTransGetSplitList (tnode->data);
            if (tnode->data == model->priv->btrans)
            {
                flags |= BLANK;
                snode = NULL;
            }
            else
            {
                split = xaccTransGetSplit (tnode->data, 0);
                snode = substitute_g_list_find (slist, split);
            }

            *iter = gtm_make_iter (model, flags, tnode, snode);
            LEAVE("Parent iter NULL, First iter is %s", iter_to_string (iter));
            return TRUE;
        }
        else
        {
            PERR("We should never have a NULL trans list.");
            goto fail;
        }
    }

/*    g_assert(VALID_ITER(model, parent_iter)); */

    if (IS_TROW1 (parent_iter))
    {
        flags = TROW2;
        tnode = parent_iter->user_data2;
        slist = xaccTransGetSplitList (tnode->data);

        if (tnode->data == model->priv->btrans)
        {
            flags |= BLANK;
            snode = NULL;
        }
        else
        {
            split = xaccTransGetSplit (tnode->data, 0);
            snode = substitute_g_list_find (slist, split);
        }
    }

    if (IS_TROW2 (parent_iter))
    {
        tnode = parent_iter->user_data2;

        if ((tnode->data == model->priv->btrans) && (tnode != model->priv->bsplit_parent_node)) // blank trans has no split to start with
            goto fail;
        else if ((tnode->data != model->priv->btrans) && (xaccTransCountSplits (tnode->data) == 0) && (tnode != model->priv->bsplit_parent_node)) // trans has no splits after trans reinit.
            goto fail;
        else
        {
            flags = SPLIT;
            tnode = parent_iter->user_data2;
            slist = xaccTransGetSplitList (tnode->data);

            if (((tnode->data == model->priv->btrans) || (xaccTransCountSplits (tnode->data) == 0)) && (tnode == model->priv->bsplit_parent_node))
            {
                flags |= BLANK;
                snode = model->priv->bsplit_node;
            }
            else
            {
                split = xaccTransGetSplit (tnode->data, 0);
                snode = substitute_g_list_find (slist, split);
            }
        }
    }

    if (IS_SPLIT (parent_iter)) // Splits do not have children
        goto fail;

    *iter = gtm_make_iter (model, flags, tnode, snode);
    LEAVE("First Child iter is %s", iter_to_string (iter));
    return TRUE;
 fail:
    LEAVE("iter has no children");
    iter->stamp = 0;
    return FALSE;
}


static gboolean
gnc_tree_model_split_reg_iter_has_child (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
    /* Returns TRUE if iter has children, FALSE otherwise. */
    GncTreeModelSplitReg *model = GNC_TREE_MODEL_SPLIT_REG (tree_model);
    GList *tnode;

    g_return_val_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (tree_model), FALSE);

    ENTER("model %p, iter %s", tree_model, iter_to_string (iter));

    tnode = iter->user_data2;

    if (IS_TROW1 (iter)) //Normal Transaction TROW1
    {
        LEAVE ("Transaction Row 1 is yes");
        return TRUE;
    }

    if (IS_TROW2 (iter) && !(IS_BLANK (iter))) //Normal Transaction TROW2
    {
        if (xaccTransCountSplits (tnode->data) != 0) // with splits
	{
            LEAVE ("Transaction Row 2 is yes");
            return TRUE;
        }
        else
        {
            if (tnode == model->priv->bsplit_parent_node) // with no splits, just blank split
	    {
                LEAVE ("Transaction Row 2 is yes, blank split");
                return TRUE;
            }
        }
    }

    if (IS_TROW2 (iter) && IS_BLANK (iter) && (tnode == model->priv->bsplit_parent_node)) //Blank Transaction TROW2
    {
        LEAVE ("Blank Transaction Row 2 is yes");
        return TRUE;
    }

    LEAVE ("We have no child");
    return FALSE;
}


static int
gnc_tree_model_split_reg_iter_n_children (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
    /* Returns the number of children that iter has. As a special case,
       if iter is NULL, then the number of toplevel nodes is returned.  */
    GncTreeModelSplitReg *model = GNC_TREE_MODEL_SPLIT_REG (tree_model);
    GList *tnode;
    int i;

    g_return_val_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (tree_model), FALSE);
    ENTER("model %p, iter %s", tree_model, iter_to_string (iter));

    if (iter == NULL) {
        i = g_list_length (model->priv->tlist);
        LEAVE ("toplevel count is %d", i);
        return i;
    }

    if (IS_SPLIT (iter))
        i = 0;

    if (IS_TROW1 (iter))
        i = 1;

    if (IS_TROW2 (iter))
    {
        tnode = iter->user_data2;
        i = xaccTransCountSplits (tnode->data);
        if (tnode == model->priv->bsplit_parent_node)
            i++;
    }

    LEAVE ("The number of children iter has is %d", i);
    return i;
}


static gboolean
gnc_tree_model_split_reg_iter_nth_child (GtkTreeModel *tree_model,
                                       GtkTreeIter *iter,
                                       GtkTreeIter *parent_iter,
                                       int n)
{
    /* Sets iter to be the n'th child of parent, using the given index. 0 > */
    GncTreeModelSplitReg *model = GNC_TREE_MODEL_SPLIT_REG (tree_model);
    Split *split;
    SplitList_t slist;
    GList *tnode;
    void *snode = NULL;
    gint flags = 0;

    ENTER("model %p, iter %s, n %d", tree_model, iter_to_string (parent_iter), n);
    g_return_val_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (tree_model), FALSE);

    if (parent_iter == NULL) {  /* Top-level */
        flags = TROW1;
        tnode = g_list_nth (model->priv->tlist, n);

        if (!tnode) {
            PERR("Trans list should never be NULL.");
            goto fail;
        }

        slist = xaccTransGetSplitList (tnode->data);

        /* Check if this is the blank trans */
        if (tnode->data == model->priv->btrans)
        {
            flags |= BLANK;
            snode = NULL;
        }
        else
        {
            split = xaccTransGetSplit (tnode->data, 0);
            snode = substitute_g_list_find(slist, split);
        }

        *iter = gtm_make_iter (model, flags, tnode, snode);
        LEAVE ("iter (2) %s", iter_to_string (iter));
        return TRUE;
    }

/*    g_assert(VALID_ITER(model, parent_iter)); */

    if (IS_SPLIT (parent_iter))
        goto fail;  /* Splits have no children */

    if (IS_TROW1 (parent_iter) && (n != 0))
        goto fail; /* TROW1 has only one child */

    flags = TROW2;
    snode = NULL;

    tnode = parent_iter->user_data2;

    if (IS_TROW1 (parent_iter) && IS_BLANK (parent_iter))
    {
        flags |= BLANK;
    }

    if (IS_TROW2 (parent_iter) && (n > xaccTransCountSplits (tnode->data)))
    {
        goto fail;
    }
    else
    {
        if (tnode->data == model->priv->btrans)
        {
            snode = NULL;
        }
        else if ((tnode == model->priv->bsplit_parent_node) && (xaccTransCountSplits (tnode->data) == n))
        {
            flags = SPLIT | BLANK;
            snode = model->priv->bsplit_node;
        }
        else
        {
            flags = SPLIT;
            slist = xaccTransGetSplitList (tnode->data);
            split = xaccTransGetSplit (tnode->data, n);
            snode = substitute_g_list_find(slist, split);
        }
    }

    *iter = gtm_make_iter (model, flags, tnode, snode);
    LEAVE("iter of child with index %u is %s", n, iter_to_string (iter));
    return TRUE;
 fail:
    LEAVE("iter has no child with index %u", n);
    iter->stamp = 0;
    return FALSE;
}


static gboolean
gnc_tree_model_split_reg_iter_parent (GtkTreeModel *tree_model,
                                    GtkTreeIter *iter,
                                    GtkTreeIter *child)
{
    /* Sets iter to be the parent of child. If child is at the toplevel,
       and doesn't have a parent, then iter is set to an invalid iterator
       and FALSE is returned. Child will remain a valid node after this 
       function has been called. */
    GncTreeModelSplitReg *model = GNC_TREE_MODEL_SPLIT_REG (tree_model);
    GList *tnode;
    gint flags = TROW1;

    ENTER("model %p, child %s", tree_model, iter_to_string (child));

/*    g_assert(VALID_ITER(model, child)); */
    void *snode;
    tnode = child->user_data2;

    if (IS_TROW1 (child))
        goto fail;

    if (IS_TROW2 (child))
        flags = TROW1;

    if (IS_SPLIT (child))
        flags = TROW2;

    if (tnode->data == model->priv->btrans)
        flags |= BLANK;

    snode = child->user_data3;
    *iter = gtm_make_iter (model, flags, tnode, snode);
    LEAVE("parent iter is %s", iter_to_string (iter));
    return TRUE;
 fail:
    LEAVE("we have no parent");
    iter->stamp = 0;
    return FALSE;
}


/*##########################################################################*/
/* increment the stamp of the model */
static void
gtm_increment_stamp (GncTreeModelSplitReg *model)
{
    do model->stamp++;
    while (model->stamp == 0);
}


/* Return these values based on the model and iter provided */
gboolean
gnc_tree_model_split_reg_get_split_and_trans (
    GncTreeModelSplitReg *model, GtkTreeIter *iter,
    gboolean *is_trow1, gboolean *is_trow2, gboolean *is_split,
    gboolean *is_blank, Split **split, Transaction **trans)
{
    GList *node;

/*    g_return_val_if_fail(VALID_ITER(model, iter), FALSE); */
    ENTER("model pointer is %p", model);
    if (is_trow1)
        *is_trow1 = !!IS_TROW1(iter);
    if (is_trow2)
        *is_trow2 = !!IS_TROW2(iter);
    if (is_split)
        *is_split = !!IS_SPLIT(iter);
    if (is_blank)
        *is_blank = !!IS_BLANK(iter);

    if (trans)
    {
        node = iter->user_data2;
        *trans = node ? (Transaction *) node->data : NULL;
    }

    if (split)
    {
        if(iter->user_data3 == model->priv->bsplit_node)
        {
            *split = ((GList*)(iter->user_data3))->data;
        }
        else
        {
            SplitList_t::iterator *node = iter->user_data3;
            *split = node ? *(*node) : NULL;
        }
    }
    LEAVE("");
    return TRUE;
}


/* Return the tree path of trans and split
   if trans and split NULL, return last in list */
GtkTreePath *
gnc_tree_model_split_reg_get_path_to_split_and_trans (GncTreeModelSplitReg *model, Split *split, Transaction *trans)
{
    GtkTreePath *path;
    SplitList_t slist;
    gint tpos, spos, number;

    ENTER("transaction is %p, split is %p", trans, split);

    path = gtk_tree_path_new();

    number = gnc_tree_model_split_reg_iter_n_children (GTK_TREE_MODEL (model), NULL) - 1;

    if (trans == NULL && split == NULL)
    {
        gtk_tree_path_append_index (path, number);
        LEAVE("path is %s", gtk_tree_path_to_string (path));
        return path;
    }

    if (trans == NULL && split != NULL)
    {
        if (split == model->priv->bsplit)
            trans = model->priv->bsplit_parent_node->data;
        else
            trans = xaccSplitGetParent (split);
    }

    if (trans != NULL)
    {
        /* Level 1 */
        tpos = g_list_index (model->priv->tlist, trans);
        if (tpos == -1)
            tpos = number;
        gtk_tree_path_append_index (path, tpos);
    }

    if (split != NULL)
    {
        slist = xaccTransGetSplitList (trans);
        /* Level 3 */
        spos = xaccTransGetSplitIndex (trans, split);
        if (spos == -1)
        {
            if (model->priv->bsplit == split) // test for blank split
                spos = xaccTransCountSplits (trans);
            else
                spos = -1;
        }
        gtk_tree_path_append_index (path, 0); /* Level 2 */
        if (spos != -1)
            gtk_tree_path_append_index (path, spos);
    }
    LEAVE("path is %s", gtk_tree_path_to_string (path));
    return path;
}


#define get_iter gnc_tree_model_split_reg_get_iter_from_trans_and_split
gboolean
gnc_tree_model_split_reg_get_iter_from_trans_and_split (
    GncTreeModelSplitReg *model, Transaction *trans, Split *split, 
    GtkTreeIter *iter1, GtkTreeIter *iter2)
{
    GncTreeModelSplitRegPrivate *priv;
    GList *tnode;
    gint flags1 = TROW1;
    gint flags2 = TROW2;
    void *snode = NULL;

    g_return_val_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (model), FALSE);
    g_return_val_if_fail (iter1, FALSE);
    g_return_val_if_fail (iter2, FALSE);
    PINFO("get_iter model %p, trans %p, split %p\n", model, trans, split);

    priv = model->priv;
    if (split && !trans)
        trans = xaccSplitGetParent (split);

    if (trans && priv->book != xaccTransGetBook (trans)) return FALSE;
    if (split && priv->book != xaccSplitGetBook (split)) return FALSE;    
    if (split && !xaccTransStillHasSplit (trans, split)) return FALSE;

    tnode = g_list_find (priv->tlist, trans);
    if (!tnode) return FALSE;

    if (trans == priv->btrans)
    {
        flags1 |= BLANK;
        flags2 |= BLANK;
    }

    if (split)
    {
        SplitList_t slist = xaccTransGetSplitList (trans);
        SplitList_t::iterator *it = new SplitList_t::iterator; // TODO someone should free this
        *it = std::find (slist.begin(), slist.end(), split);
        flags1 = SPLIT;
        if ((*it == slist.end()) && split == (Split *) ((GList *)priv->bsplit_node)->data)
        {
            snode = priv->bsplit_node;
            flags1 |= BLANK;
        }
        else
        {
            snode = it; // TODO  somebody should free this.
        }
        if (!snode) return FALSE;
    }

    *iter1 = gtm_make_iter (model, flags1, tnode, snode);
    *iter2 = gtm_make_iter (model, flags2, tnode, snode);

    return TRUE;
}


/* Return the blank split */
Split *
gnc_tree_model_split_get_blank_split (GncTreeModelSplitReg *model)
{
    return model->priv->bsplit;
}


/* Return the blank transaction */
Transaction *
gnc_tree_model_split_get_blank_trans (GncTreeModelSplitReg *model)
{
    return model->priv->btrans;
}

/*##########################################################################*/

/* Update the parent when row changes made */
static void
gtm_update_parent (GncTreeModelSplitReg *model, GtkTreePath *path)
{
    GList *tnode;
    GtkTreeIter iter;

    ENTER(" ");
    if (gtk_tree_path_up (path) && gnc_tree_model_split_reg_get_iter (GTK_TREE_MODEL (model), &iter, path))
    {
        PINFO("row_changed - '%s'", gtk_tree_path_to_string (path));
        gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);

        tnode = iter.user_data2;

        /* If this is the blank transaction, the only split will be deleted, hence toggle has child */
        if (IS_BLANK_TRANS (&iter) && (tnode->data == model->priv->btrans) && (xaccTransCountSplits (model->priv->btrans) == 0))
        {
            PINFO("toggling has_child at row '%s'", gtk_tree_path_to_string (path));
            gtm_increment_stamp (model);
            gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, &iter);
        }
    }
    LEAVE(" ");
}


/* Insert row at iter */
static void
gtm_insert_row_at (GncTreeModelSplitReg *model, GtkTreeIter *iter)
{
    GtkTreePath *path;

//    g_assert (VALID_ITER (model, iter));
    ENTER(" ");
    path = gnc_tree_model_split_reg_get_path (GTK_TREE_MODEL (model), iter);
    if (!path)
        PERR("Null path");

    gtm_increment_stamp (model);
    if (gnc_tree_model_split_reg_get_iter (GTK_TREE_MODEL (model), iter, path))
    {
        gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, iter);
    }
    else
        PERR("Tried to insert with invalid iter.");

    gtm_update_parent (model, path);
    gtk_tree_path_free (path);
    LEAVE(" ");
}


/* Delete row at path */
static void
gtm_delete_row_at_path (GncTreeModelSplitReg *model, GtkTreePath *path)
{
    gint depth;

    ENTER(" ");

    if (!path)
        PERR("Null path");

    gtm_increment_stamp (model);
    gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);

    depth = gtk_tree_path_get_depth (path);

    if (depth == 2)
    {
        gtm_update_parent (model, path);
    }
    else if (depth == 3)
    {
        gtm_update_parent (model, path);
    }
    else
    {
        GtkTreeIter iter;
        if (gnc_tree_model_split_reg_get_iter (GTK_TREE_MODEL (model), &iter, path))
        { 
            GList *tnode = iter.user_data2;
            GncTreeModelSplitRegPrivate *priv = model->priv;
            if (tnode == priv->bsplit_parent_node)
                priv->bsplit_parent_node = NULL;
        }
    }
    LEAVE(" ");
}


/* Delete row at iter */
static void
gtm_delete_row_at (GncTreeModelSplitReg *model, GtkTreeIter *iter)
{
    GtkTreePath *path;
//    g_assert(VALID_ITER (model, iter));

    ENTER(" ");
    path = gnc_tree_model_split_reg_get_path (GTK_TREE_MODEL (model), iter);
    gtm_delete_row_at_path (model, path);
    gtk_tree_path_free (path);
    LEAVE(" ");
}


/* Change row at iter */
static void
gtm_changed_row_at (GncTreeModelSplitReg *model, GtkTreeIter *iter)
{
    GtkTreePath *path;
//    g_assert(VALID_ITER (model, iter));

    ENTER(" ");
    path = gnc_tree_model_split_reg_get_path (GTK_TREE_MODEL (model), iter);
    if (!path)
        PERR ("Null path");

    gtm_increment_stamp (model);
    if (gnc_tree_model_split_reg_get_iter (GTK_TREE_MODEL (model), iter, path))
    {
        gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, iter);
    }
    else 
        PERR ("Tried to change with invalid iter.");

    gtk_tree_path_free (path);
    LEAVE(" ");
}


/* Insert transaction into model */
static void
gtm_insert_trans (GncTreeModelSplitReg *model, Transaction *trans)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    GList *tnode = NULL;

    ENTER("insert transaction %p into model %p", trans, model);
    model->priv->tlist = g_list_prepend (model->priv->tlist, trans);
    tnode = g_list_find (model->priv->tlist, trans);

    iter = gtm_make_iter (model, TROW1, tnode, NULL);
    gtm_insert_row_at (model, &iter);
    path = gnc_tree_model_split_reg_get_path (GTK_TREE_MODEL (model), &iter);
    gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, &iter);
    gtk_tree_path_free (path);

    iter = gtm_make_iter (model, TROW2, tnode, NULL);
    gtm_insert_row_at (model, &iter);
    path = gnc_tree_model_split_reg_get_path (GTK_TREE_MODEL (model), &iter);
    gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, &iter);
    gtk_tree_path_free (path);

    DEBUG("insert %d splits for transaction %p\n", xaccTransCountSplits (trans), trans);

    SplitList_t slist = xaccTransGetSplitList (trans);
    for (SplitList_t::iterator snode = slist.begin(); snode != slist.end(); snode++)
    {
        if (xaccTransStillHasSplit (trans, *snode))
        {
            iter = gtm_make_iter (model, SPLIT, tnode, &snode);
            gtm_insert_row_at (model, &iter);
        }
    }
    LEAVE(" ");
}


/* Delete transaction from model */
static void
gtm_delete_trans (GncTreeModelSplitReg *model, Transaction *trans)
{
    GtkTreeIter iter;
    GList *tnode = NULL, *snode = NULL;

    ENTER("delete trans %p", trans);
    tnode = g_list_find (model->priv->tlist, trans);

    DEBUG("tlist length is %d and no of splits is %d", g_list_length (model->priv->tlist), xaccTransCountSplits (trans));

    if (tnode == model->priv->bsplit_parent_node)
    {
        /* Delete the row where the blank split is. */
        iter = gtm_make_iter (model, SPLIT | BLANK, tnode, model->priv->bsplit_node);
        gtm_delete_row_at (model, &iter);
        model->priv->bsplit_parent_node = NULL;
    }

    SplitList_t slist = xaccTransGetSplitList (trans);
    for (SplitList_t::iterator snode = slist.begin(); snode != slist.end(); snode++)
    {
        if (xaccTransStillHasSplit (trans, *snode))
        {
            iter = gtm_make_iter (model, SPLIT, tnode, &snode);
            gtm_delete_row_at (model, &iter);
        }
    }

    iter = gtm_make_iter (model, TROW2, tnode, NULL);
    gtm_delete_row_at (model, &iter);

    iter = gtm_make_iter (model, TROW1, tnode, NULL);
    gtm_delete_row_at (model, &iter);

    model->priv->tlist = g_list_delete_link (model->priv->tlist, tnode);
    LEAVE(" ");
}


/* Moves the blank split to 'trans' and remove old one. */
gboolean
gnc_tree_model_split_reg_set_blank_split_parent (GncTreeModelSplitReg *model, Transaction *trans, gboolean remove_only)
{
    GList *tnode, *bs_parent_node;
    GncTreeModelSplitRegPrivate *priv;
    GtkTreeIter iter;
    gboolean moved;

    priv = model->priv;

    if (trans == NULL)
        tnode = g_list_last (priv->tlist);
    else
        tnode = g_list_find (priv->tlist, trans);

    ENTER("set blank split %p parent to trans %p and remove_only is %d", priv->bsplit, trans, remove_only);

    bs_parent_node = priv->bsplit_parent_node;

    if (tnode != bs_parent_node || remove_only == TRUE)
    {
        moved = (bs_parent_node != NULL || remove_only == TRUE);
        if (moved)
        {
            /* Delete the row where the blank split used to be. */
            iter = gtm_make_iter (model, SPLIT | BLANK, bs_parent_node, priv->bsplit_node);
            gtm_delete_row_at (model, &iter);
            priv->bsplit_parent_node = NULL;

        }
        if (remove_only == FALSE)
        {
            /* Create the row where the blank split will be. */
            priv->bsplit_parent_node = tnode;
            iter = gtm_make_iter (model, SPLIT | BLANK, tnode, priv->bsplit_node);
            gtm_insert_row_at (model, &iter);
            xaccSplitReinit (priv->bsplit); // set split back to default entries
        }
    }
    else
        moved = FALSE;

    LEAVE(" ");
    return moved;
}


/* Make a new blank split and insert at iter */
static void
gtm_make_new_blank_split (GncTreeModelSplitReg *model)
{
    GtkTreeIter iter;
    Split *split;
    GList *tnode = model->priv->bsplit_parent_node;

    ENTER("");

    split = xaccMallocSplit (model->priv->book);
    model->priv->bsplit = split;
    model->priv->bsplit_node->data = model->priv->bsplit;

    DEBUG("make new blank split %p and insert at trans %p", split, tnode->data);

    /* Insert the new blank split */
    iter = gtm_make_iter (model, BLANK|SPLIT, tnode, model->priv->bsplit_node);
    gtm_insert_row_at (model, &iter);
    LEAVE("");
}


/* Turn the current blank split into a real split.  This function is
 * never called in response to an engine event.  Instead, this
 * function is called from the treeview to tell the model to commit
 * the blank split.
 */
void
gnc_tree_model_split_reg_commit_blank_split (GncTreeModelSplitReg *model)
{
    Split *bsplit;
    GList *tnode;
    GtkTreeIter iter;

    ENTER(" ");

    tnode = model->priv->bsplit_parent_node;
    bsplit = model->priv->bsplit;

    if (!tnode || !tnode->data) {
        LEAVE("blank split has no trans");
        return;
    }

    if (xaccTransGetSplitIndex (tnode->data, bsplit) == -1) {
        LEAVE("blank split has been removed from this trans");
        return;
    }

    SplitList_t splits = xaccTransGetSplitList (tnode->data);
    SplitList_t::iterator snode = std::find (splits.begin(), splits.end(), bsplit);
    if (snode == splits.end()) {
        LEAVE("Failed to turn blank split into real split");
        return;
    }

    /* If we haven't set an amount yet, and there's an imbalance, use that. */
    if (gnc_numeric_zero_p (xaccSplitGetAmount (bsplit)))
    {
        gnc_numeric imbal = gnc_numeric_neg (xaccTransGetImbalanceValue (tnode->data));
        if (!gnc_numeric_zero_p (imbal))
        {
            gnc_numeric amount, rate;
            Account *acct = xaccSplitGetAccount (bsplit);
            xaccSplitSetValue (bsplit, imbal);

            if (gnc_commodity_equal (xaccAccountGetCommodity (acct), xaccTransGetCurrency (tnode->data)))
            {
                amount = imbal;
            }
            else
            {
                rate = xaccTransGetAccountConvRate (tnode->data, acct);
                amount = gnc_numeric_mul (imbal, rate, xaccAccountGetCommoditySCU (acct), GNC_HOW_RND_ROUND);
            }
            if (gnc_numeric_check (amount) == GNC_ERROR_OK)
            {
                xaccSplitSetAmount (bsplit, amount);
            }
        }
    }
    /* Mark the old blank split as changed */
    iter = gtm_make_iter (model, SPLIT, tnode, &snode);
    gtm_changed_row_at (model, &iter);
    gtm_make_new_blank_split (model);

    LEAVE(" ");
}


/* Update the dsiplay sub account and general ledger settings */
void
gnc_tree_model_split_reg_set_display (GncTreeModelSplitReg *model, gboolean subacc, gboolean gl)
{
    GncTreeModelSplitRegPrivate *priv = model->priv;

    priv->display_subacc = subacc;
    priv->display_gl = gl;
}


/* Returns just the path to the transaction if idx_of_split is -1. */
static GtkTreePath *
gtm_get_removal_path (GncTreeModelSplitReg *model, Transaction *trans,
                 gint idx_of_split)
{
    GncTreeModelSplitRegPrivate *priv;
    GList *tnode = NULL;
    GtkTreeIter iter;
    GtkTreePath *path;

    g_return_val_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (model), NULL);
    g_return_val_if_fail (trans, NULL);

    priv = model->priv;
    if (priv->book != xaccTransGetBook (trans))
        return FALSE;

    tnode = g_list_find (priv->tlist, trans);
    if (!tnode)
        return FALSE;

    iter = gtm_make_iter (model, TROW1, tnode, NULL); // TROW1
    path = gnc_tree_model_split_reg_get_path (GTK_TREE_MODEL (model), &iter);

    if (idx_of_split >= 0)
    {
        gtk_tree_path_append_index (path, 0); // TROW2
        gtk_tree_path_append_index (path, idx_of_split); //SPLIT
    }
    else if (idx_of_split != -1)
        PERR("Invalid idx_of_split");

    return path;
}


/*##########################################################################*/
/*             Combo and Autocompletion ListStore functions                 */

Account *
gnc_tree_model_split_reg_get_anchor (GncTreeModelSplitReg *model)
{
    g_return_val_if_fail(GNC_IS_TREE_MODEL_SPLIT_REG(model), NULL);
    return model->priv->anchor;
}

GtkListStore *
gnc_tree_model_split_reg_get_description_list (GncTreeModelSplitReg *model)
{
    g_return_val_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (model), NULL);
    return model->priv->description_list;
}

GtkListStore *
gnc_tree_model_split_reg_get_notes_list (GncTreeModelSplitReg *model)
{
    g_return_val_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (model), NULL);
    return model->priv->notes_list;
}

GtkListStore *
gnc_tree_model_split_reg_get_memo_list (GncTreeModelSplitReg *model)
{
    g_return_val_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (model), NULL);
    return model->priv->memo_list;
}

GtkListStore *
gnc_tree_model_split_reg_get_numact_list (GncTreeModelSplitReg *model)
{
    g_return_val_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (model), NULL);
    return model->priv->numact_list;
}

GtkListStore *
gnc_tree_model_split_reg_get_acct_list (GncTreeModelSplitReg *model)
{
    g_return_val_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (model), NULL);
    return model->priv->account_list;
}


/* Return TRUE if string already exists in the list */
static gboolean
gtm_check_for_duplicates (GtkListStore *liststore, const gchar *string)
{
    GtkTreeIter iter;
    gboolean valid;

    valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (liststore), &iter);
    while (valid)
    {
        gchar *text;
        // Walk through the list, reading each row
        gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter, 0, &text, -1);

        if(!(g_strcmp0 (text, string)))
        {
            g_free(text);
            return TRUE;
        }
        g_free(text);

        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (liststore), &iter);
    }
    return FALSE;
}


/* Update the Auto Complete List Stores.... */
void
gnc_tree_model_split_reg_update_completion (GncTreeModelSplitReg *model)
{
    GncTreeModelSplitRegPrivate *priv;
    GtkTreeIter d_iter, n_iter, m_iter, num_iter;
    GList *tlist_cpy, *tnode;
    int cnt, nSplits;

    ENTER(" ");

    priv = model->priv;

    // Copy the tlist, put it in date order and reverse it.
    tlist_cpy = g_list_copy (priv->tlist);
    tlist_cpy = g_list_sort (tlist_cpy, (GCompareFunc)xaccTransOrder );
    tlist_cpy = g_list_reverse (tlist_cpy);

    /* Clear the liststores */
    gtk_list_store_clear (priv->description_list);
    gtk_list_store_clear (priv->notes_list);
    gtk_list_store_clear (priv->num_list);
    gtk_list_store_clear (priv->memo_list);

    for (tnode = tlist_cpy; tnode; tnode = tnode->next)
    {
        Split       *split;
        const gchar *string;

        nSplits = xaccTransCountSplits (tnode->data);
        SplitList_t slist = xaccTransGetSplitList (tnode->data);
    
        /* Add to the Description list */
        string = xaccTransGetDescription (tnode->data);
        if(g_strcmp0 (string, ""))
        {
            if(gtm_check_for_duplicates (priv->description_list, string) == FALSE)
            {
                gtk_list_store_append (priv->description_list, &d_iter);
                gtk_list_store_set (priv->description_list, &d_iter, 0, string, 1, tnode->data, -1);
            }
        }

        /* Add to the Notes list */
        string = xaccTransGetNotes (tnode->data);
        if(g_strcmp0 (string, ""))
        {
            if(gtm_check_for_duplicates (priv->notes_list, string) == FALSE)
            {
                gtk_list_store_append (priv->notes_list, &n_iter);
                gtk_list_store_set (priv->notes_list, &n_iter, 0, string, -1);
            }
        }

        /* Add to the Num list */
        /* Get transaction-number with gnc_get_num_action which is the same as
         * xaccTransGetNum with these arguments; not sure what is being done
         * here so not sure if this is correct; won't get the same 'num' entered
         * by user in a register if book option to use split-action for 'num'
         * field is set */
        string = gnc_get_num_action (tnode->data, NULL);
        if(g_strcmp0 (string, ""))
        {
            if(gtm_check_for_duplicates (priv->num_list, string) == FALSE)
            {
                gtk_list_store_prepend (priv->num_list, &num_iter);
                gtk_list_store_set (priv->num_list, &num_iter, 0, string, -1);
            }
        }

        /* Loop through the list of splits for each Transaction - **do not free the list** */
        SplitList_t::iterator snode = slist.begin();
        cnt = 0;
        while (cnt < nSplits)
        {
            split = *snode;

            /* Add to the Memo list */
            string = xaccSplitGetMemo (split);
            if(g_strcmp0 (string, ""))
            {
                if(gtm_check_for_duplicates (priv->memo_list, string) == FALSE)
                {
                    gtk_list_store_append (priv->memo_list, &m_iter);
                    gtk_list_store_set (priv->memo_list, &m_iter, 0, string, -1);
                }
            }
            cnt++;
            snode++;
         }
    }

    g_list_free (tlist_cpy);
    DEBUG("desc list is %d long", gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->description_list), NULL));
    DEBUG("notes list is %d long", gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->notes_list), NULL));
    DEBUG("memo list is %d long", gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->memo_list), NULL));
    LEAVE(" ");
}


/* Update the model with entries for the Action field */
void
gnc_tree_model_split_reg_update_action_list (GncTreeModelSplitReg *model)
{
    GncTreeModelSplitRegPrivate *priv;
    GtkListStore *store;
    GtkTreeIter iter;

    priv = model->priv;
    store = priv->numact_list;

//FIXME This may need some more thought ???

    /* Clear the liststore */
    gtk_list_store_clear (store);

    /* setup strings in the action pull-down */
    switch (model->type)
    {
    case BANK_REGISTER2:
        /* broken ! FIXME bg ????????? What is broken */
    case SEARCH_LEDGER2:

        /* Translators: This string has a context prefix; the translation
        	must only contain the part after the | character. */
        gtk_list_store_insert_with_values (store, &iter, 100, 0, Q_("Action Column|Deposit"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Withdraw"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Check"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Interest"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("ATM Deposit"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("ATM Draw"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Teller"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Charge"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Payment"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Receipt"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Increase"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Decrease"), -1);
        /* Action: Point Of Sale */
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("POS"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Phone"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Online"), -1);
        /* Action: Automatic Deposit */
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("AutoDep"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Wire"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Credit"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Direct Debit"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Transfer"), -1);
        break;
    case CASH_REGISTER2:
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Increase"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Decrease"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Buy"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Sell"), -1);
        break;
    case ASSET_REGISTER2:
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Buy"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Sell"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Fee"), -1);
        break;
    case CREDIT_REGISTER2:
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("ATM Deposit"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("ATM Withdraw"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Buy"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Credit"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Fee"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Interest"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Online"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Sell"), -1);
        break;
    case LIABILITY_REGISTER2:
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Buy"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Sell"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Loan"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Interest"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Payment"), -1);
        break;
    case RECEIVABLE_REGISTER2:
    case PAYABLE_REGISTER2:
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Invoice"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Payment"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Interest"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Credit"), -1);
        break;
    case INCOME_LEDGER2:
    case INCOME_REGISTER2:
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Increase"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Decrease"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Buy"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Sell"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Interest"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Payment"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Rebate"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Paycheck"), -1);
        break;
    case EXPENSE_REGISTER2:
    case TRADING_REGISTER2:
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Increase"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Decrease"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Buy"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Sell"), -1);
        break;
    case GENERAL_LEDGER2:
    case EQUITY_REGISTER2:
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Buy"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Sell"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Equity"), -1);
        break;
    case STOCK_REGISTER2:
    case PORTFOLIO_LEDGER2:
    case CURRENCY_REGISTER2:
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Buy"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Sell"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Price"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Fee"), -1);
        /* Action: Dividend */
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Dividend"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Interest"), -1);
        /* Action: Long Term Capital Gains */
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("LTCG"), -1);
        /* Action: Short Term Capital Gains */
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("STCG"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Income"), -1);
        /* Action: Distribution */
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Dist"), -1);
        /* Translators: This string has a disambiguation prefix */
        gtk_list_store_insert_with_values (store, &iter, 100, 0, Q_("Action Column|Split"), -1);
        break;

    default:
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Increase"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Decrease"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Buy"), -1);
        gtk_list_store_insert_with_values (store, &iter, 100, 0, _("Sell"), -1);
        break;
    }
    priv->numact_list = store;
}


/* Update the model with entries for the Number field ... */
void
gnc_tree_model_split_reg_update_num_list (GncTreeModelSplitReg *model)
{
    GncTreeModelSplitRegPrivate *priv;
    GtkTreeIter iter, num_iter;
    gboolean valid;

    priv = model->priv;

    /* Clear the liststore */
    gtk_list_store_clear (priv->numact_list);

    /* Here we copy the num_list to the numact_list */
    valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->num_list), &num_iter);
    while (valid)
    {
        gchar *text;

        // Walk through the list, reading each row
        gtk_tree_model_get (GTK_TREE_MODEL (priv->num_list), &num_iter, 0, &text, -1);
        gtk_list_store_append (priv->numact_list, &iter);
        gtk_list_store_set (priv->numact_list, &iter, 0, text, -1);
        g_free (text);

        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->num_list), &num_iter);
    }
}


/* Return the GtkListstore of Accounts */
void
gnc_tree_model_split_reg_update_account_list (GncTreeModelSplitReg *model)
{
    GncTreeModelSplitRegPrivate *priv;
    Account *root;
    Account *acc;
    GtkTreeIter iter;
    gboolean valid;
    const gchar *name;
    gchar *fname;
    gint i;

    priv = model->priv;

    /* Clear the liststore, Store is short name, full name and account pointer */
    gtk_list_store_clear (priv->account_list);

    root = gnc_book_get_root_account (gnc_get_current_book());
/*FIXME This does not look sorted to me, need to look at this */
    AccountList_t accts = gnc_account_get_descendants_sorted (root);

    AccountList_t::iterator ptr;
    for (ptr = accts.begin(), i = 0; ptr != accts.end(); ptr++, i++)
    {
        acc = *ptr;

        if(!(acc == model->priv->anchor))
        {
            fname = gnc_account_get_full_name (acc);
            name = xaccAccountGetName (acc);
            gtk_list_store_append (priv->account_list, &iter);
            gtk_list_store_set (priv->account_list, &iter, 0, name, 1, fname, 2, acc, -1);
            g_free (fname);
        }
    }
}


/*******************************************************************/
/*   Split Register Tree Model - Engine Event Handling Functions   */
/*******************************************************************/

/** This function is the handler for all event messages from the
 *  engine.  Its purpose is to update the account tree model any time
 *  an account is added to the engine or deleted from the engine.
 *  This change to the model is then propagated to any/all overlying
 *  filters and views.  This function listens to the ADD, REMOVE, and
 *  DESTROY events.
 *
 *  @internal
 *
 *  @warning There is a "Catch 22" situation here.
 *  gtk_tree_model_row_deleted() can't be called until after the item
 *  has been deleted from the real model (which is the engine's
 *  account tree for us), but once the account has been deleted from
 *  the engine we have no way to determine the path to pass to
 *  row_deleted().  This is a PITA, but the only other choice is to
 *  have this model mirror the engine's accounts instead of
 *  referencing them directly.
 *
 *  @param entity The guid of the affected item.
 *
 *  @param type The type of the affected item.  This function only
 *  cares about items of type "account".
 *
 *  @param event type The type of the event. This function only cares
 *  about items of type ADD, REMOVE, MODIFY, and DESTROY.
 *
 *  @param user_data A pointer to the split register tree model.
 */
static void
gnc_tree_model_split_reg_event_handler (QofInstance *entity,
                                      QofEventId event_type,
                                      GncTreeModelSplitReg *model,
                                      GncEventData *event_data)
{
    GncTreeModelSplitRegPrivate *priv = model->priv;
    GncEventData *ed = event_data;
    GtkTreeIter iter1, iter2;
    GtkTreePath *path;
    Transaction *trans;
    Split *split = NULL;
    QofIdType type;
    const gchar *name = NULL;
    GList *tnode;

    g_return_if_fail (GNC_IS_TREE_MODEL_SPLIT_REG (model));

    if (qof_instance_get_book (entity) != priv->book)
        return;
    type = entity->e_type;

    if (g_strcmp0 (type, GNC_ID_SPLIT) == 0)
    {
        /* Get the split.*/
        split = (Split *) entity;
        name = xaccSplitGetMemo (split);

        switch (event_type)
        {
        case QOF_EVENT_MODIFY:
            if (get_iter (model, NULL, split, &iter1, &iter2))
            {
                DEBUG ("change split %p (%s)", split, name);
                gtm_changed_row_at (model, &iter1);

                /* If we change split to different account, remove from view */
                if (priv->anchor != NULL)
                {
                    Split *find_split;
                    Transaction *trans;
                    trans = xaccSplitGetParent (split);
                    find_split = xaccTransFindSplitByAccount (trans, priv->anchor);
                    if (find_split == NULL)
                        gtm_delete_trans (model, trans);
                }
            }
            break;
        default:
            DEBUG ("ignored event for %p (%s)", split, name);
        }
    }
    else if (g_strcmp0(type, GNC_ID_TRANS) == 0)
    {
        /* Get the trans.*/
        trans = (Transaction *) entity;
        name = xaccTransGetDescription (trans);

        switch (event_type)
        {
        case GNC_EVENT_ITEM_ADDED:
            split = (Split *) ed->node;
            /* The blank split will be added to the transaction when
               it's first edited.  That will generate an event, but
               we don't want to emit row_inserted because we were
               already showing the blank split. */
            if (split == priv->bsplit) break;

            if (xaccTransCountSplits (trans) < 2) break;

            /* Tell the filters/views where the new row was added. */
            if (get_iter (model, trans, split, &iter1, &iter2))
            {
                DEBUG ("add split %p (%s)", split, name);
                gtm_insert_row_at (model, &iter1);
            }
            break;
        case GNC_EVENT_ITEM_REMOVED:
            split = (Split *) ed->node;

            path = gtm_get_removal_path (model, trans, ed->idx);
            if (path)
            {
                DEBUG ("remove split %p from trans %p (%s)", split, trans, name);
                if (ed->idx == -1)
                    gtm_delete_trans (model, trans); //Not sure when this would be so
                else
                    gtm_delete_row_at_path (model, path);
                gtk_tree_path_free (path);
            }
            if (split == priv->bsplit)
                gtm_make_new_blank_split (model);
            break;
        case QOF_EVENT_MODIFY:
            /* The blank trans won't emit MODIFY until it's committed */
            if (priv->btrans == trans)
            {
                priv->btrans = xaccMallocTransaction (priv->book);
                priv->tlist = g_list_append (priv->tlist, priv->btrans);

                tnode = g_list_find (priv->tlist, priv->btrans);
                /* Insert a new blank trans */
                iter1 = gtm_make_iter (model, TROW1 | BLANK, tnode, NULL);
                gtm_insert_row_at (model, &iter1);
                iter2 = gtm_make_iter (model, TROW2 | BLANK, tnode, NULL);
                gtm_insert_row_at (model, &iter2);
                g_signal_emit_by_name (model, "refresh_view", NULL);
            }

            if (get_iter (model, trans, NULL, &iter1, &iter2))
            {
                DEBUG ("change trans %p (%s)", trans, name);
                gtm_changed_row_at (model, &iter1);
                gtm_changed_row_at (model, &iter2);
                g_signal_emit_by_name (model, "refresh_view", NULL);
            }

            break;
        case QOF_EVENT_DESTROY:
            if (priv->btrans == trans)
            {
                tnode = g_list_find (priv->tlist, priv->btrans);
                priv->btrans = xaccMallocTransaction (priv->book);
                tnode->data = priv->btrans;
                iter1 = gtm_make_iter (model, TROW1 | BLANK, tnode, NULL);
                gtm_changed_row_at (model, &iter1);
                iter2 = gtm_make_iter (model, TROW2 | BLANK, tnode, NULL);
                gtm_changed_row_at (model, &iter2);
            }
            else if (get_iter (model, trans, NULL, &iter1, &iter2))
            {
                DEBUG("destroy trans %p (%s)", trans, name);
                g_signal_emit_by_name (model, "selection_move_delete", trans);
                gtm_delete_trans (model, trans);
                g_signal_emit_by_name (model, "refresh_view", NULL);
            }
            break;
        default:
            DEBUG("ignored event for %p (%s)", trans, name);
        }
    }
    else if (g_strcmp0 (type, GNC_ID_ACCOUNT) == 0)
    {
        switch (event_type)
        {
            Account *acc;
        case GNC_EVENT_ITEM_ADDED:
            split = (Split *) ed;
            acc = xaccSplitGetAccount (split);
            trans = xaccSplitGetParent (split);

            if (!g_list_find (priv->tlist, trans) && priv->display_gl)
            {
                DEBUG("Insert trans %p for gl (%s)", trans, name);
                gtm_insert_trans (model, trans);
                g_signal_emit_by_name (model, "refresh_view", NULL);
            }
            else if (!g_list_find (priv->tlist, trans) && ((xaccAccountHasAncestor (acc, priv->anchor) && priv->display_subacc) || acc == priv->anchor ))
            {
                DEBUG("Insert trans %p (%s)", trans, name);
                gtm_insert_trans (model, trans);
                g_signal_emit_by_name (model, "refresh_view", NULL);
            }
            break;
        default:
            ;
        }
        /* Lets refresh the status bar */
        g_signal_emit_by_name (model, "refresh_status_bar", NULL);
    }
}


/* Returns the parent Window of the register */
GtkWidget *
gnc_tree_model_split_reg_get_parent (GncTreeModelSplitReg *model)
{
    GncTreeModelSplitRegPrivate *priv;
    GtkWidget *parent = NULL;

    priv = model->priv;

    if (priv->get_parent)
        parent = priv->get_parent (priv->user_data);

    return parent;
}
