/********************************************************************\
 * dialog-transfer.c -- transfer dialog for GnuCash                 *
 * Copyright (C) 1999 Linas Vepstas                                 *
 * Copyright (C) 2000 Dave Peticolas                                *
 * Copyright (C) 2000 Herbert Thoma                                 *
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

#include "dialog-transfer.h"
#include "dialog-utils.h"
#include "global-options.h"
#include "gnc-account-tree.h"
#include "gnc-amount-edit.h"
#include "gnc-component-manager.h"
#include "gnc-date-edit.h"
#include "gnc-engine-util.h"
#include "gnc-euro.h"
#include "gnc-exp-parser.h"
#include "gnc-gui-query.h"
#include "gnc-ui.h"
#include "gnc-pricedb.h"
#include "messages.h"
#include "Transaction.h"
#include "Account.h"


#define DIALOG_TRANSFER_CM_CLASS "dialog-transfer"

#define PRECISION 100000
typedef enum
{
  XFER_DIALOG_FROM,
  XFER_DIALOG_TO
} XferDirection;


/* This static indicates the debugging module that this .o belongs to.  */
static short module = MOD_GUI;

struct _xferDialog
{
  GtkWidget * dialog;

  GtkWidget * amount_edit;
  GtkWidget * date_entry;
  GtkWidget * num_entry;
  GtkWidget * description_entry;
  GtkWidget * memo_entry;
  GtkWidget * conv_forward;
  GtkWidget * conv_reverse;

  GtkWidget *	from_window;
  GNCAccountTree * from;
  gnc_commodity *	from_commodity;
  GtkWidget *	to_window;
  GNCAccountTree * to;
  gnc_commodity *	to_commodity;

  QuickFill * qf;     /* Quickfill on transfer descriptions, 
                         defaults to matching on the "From" account. */

  gboolean quickfill_to;  /* match on the "To" account instead. */

  /* stored data for the description quickfill functionality */
  gint desc_start_selection;
  gint desc_end_selection;
  gint desc_cursor_position;
  gboolean desc_didquickfill;

  GtkWidget * from_currency_label;
  GtkWidget * to_currency_label;

  GtkWidget * from_show_button;
  GtkWidget * to_show_button;

  GtkWidget * curr_transfer_frame;

  GtkWidget * price_edit;
  GtkWidget * to_amount_edit;

  GtkWidget * price_radio;
  GtkWidget * amount_radio;

  GtkTooltips *tips;

  GNCBook *	book;
  GNCPriceDB *	pricedb;

  /* Where to store the "exchange_rate" at exit (in lieu of
   * creating a transaction)
   */
  gnc_numeric * exch_rate;

  /* a place to store the result quality (ok or cancel) because gnome_dialog_run
   * doesn't seem to work right for this function
   */
  gboolean *	result_p;
};

struct _acct_list_item
{
  char *acct_full_name;
  Account *acct;
};
typedef struct _acct_list_item acct_list_item;


/** Prototypes ***************************************************/
static void gnc_xfer_update_to_amount (XferDialog *xferData);
static void gnc_xfer_dialog_update_conv_info(XferDialog *xferData);


/** Implementations **********************************************/

/* (maybe) update the price from the pricedb. */
static void
gnc_xfer_dialog_update_price (XferDialog *xferData)
{
  GNCPrice *prc;
  gnc_numeric price;
  Timespec date;
  gnc_commodity *from = xferData->from_commodity;
  gnc_commodity *to = xferData->to_commodity;

  if (!xferData) return;
  if (!xferData->from_commodity || ! xferData->to_commodity) return;
  if (gnc_commodity_equal (xferData->from_commodity, xferData->to_commodity))
    return;
  if (!xferData->pricedb) return;

  /* when do we update, and when do we NOT update? */

  /* XXX: I'm ALWAYS going to update whenver we get called */

  /* grab the price nearest to the DATE out of the pricedb */
  date = gnc_date_edit_get_date_ts (GNC_DATE_EDIT (xferData->date_entry));
  prc = gnc_pricedb_lookup_nearest_in_time (xferData->pricedb,
					    from, to, date);

  if (prc) {
    /* grab the price from the pricedb */
    price = gnc_price_get_value (prc);
    PINFO("Found price: 1 %s = %f %s", gnc_commodity_get_mnemonic(from),
	  gnc_numeric_to_double(price), gnc_commodity_get_mnemonic(to));
  } else {
    prc = gnc_pricedb_lookup_nearest_in_time (xferData->pricedb,
					      to, from, date);
    if (!prc)
      return;
    price = gnc_price_get_value (prc);
    PINFO("Found reverse price: 1 %s = %f %s", gnc_commodity_get_mnemonic(to),
	  gnc_numeric_to_double(price), gnc_commodity_get_mnemonic(from));
    price = gnc_numeric_div (gnc_numeric_create (1, 1), price,
			     GNC_DENOM_AUTO, GNC_DENOM_REDUCE);
  }

  /* and set the price entry */
  gnc_amount_edit_set_amount (GNC_AMOUNT_EDIT (xferData->price_edit), price);

  /* And then update the to_amount */
  gnc_xfer_update_to_amount (xferData);
}

static void
gnc_xfer_dialog_toggle_cb(GtkToggleButton *button, gpointer data)
{
  GNCAccountTree *tree = GNC_ACCOUNT_TREE(data);

  if (gtk_toggle_button_get_active(button))
    gnc_account_tree_show_income_expense(tree);
  else
    gnc_account_tree_hide_income_expense(tree);
}

static void
gnc_xfer_dialog_set_price_auto (XferDialog *xferData,
                                gboolean currency_active,
                                const gnc_commodity *from_currency,
                                const gnc_commodity *to_currency)
{
  gnc_numeric from_rate;
  gnc_numeric to_rate;
  gnc_numeric price;

  if (!currency_active)
  {
    GtkEntry *entry;

    gnc_amount_edit_set_amount(GNC_AMOUNT_EDIT(xferData->price_edit),
                               gnc_numeric_zero ());
    entry = GTK_ENTRY(gnc_amount_edit_gtk_entry
                      (GNC_AMOUNT_EDIT(xferData->price_edit)));
    gtk_entry_set_text(entry, "");

    gnc_xfer_update_to_amount (xferData);

    return;
  }

  if (!gnc_is_euro_currency (from_currency) ||
      !gnc_is_euro_currency (to_currency))
    return gnc_xfer_dialog_update_price (xferData);

  from_rate = gnc_euro_currency_get_rate (from_currency);
  to_rate = gnc_euro_currency_get_rate (to_currency);

  if (gnc_numeric_zero_p (from_rate) || gnc_numeric_zero_p (to_rate))
    gnc_xfer_dialog_update_price (xferData);

  price = gnc_numeric_div (to_rate, from_rate, GNC_DENOM_AUTO, 
                           GNC_DENOM_SIGFIGS(6) | GNC_RND_ROUND);

  gnc_amount_edit_set_amount (GNC_AMOUNT_EDIT(xferData->price_edit), price);

  gnc_xfer_update_to_amount (xferData);
}

static void
gnc_xfer_dialog_curr_acct_activate(XferDialog *xferData)
{
  Account *to_account;
  Account *from_account;
  gboolean curr_active;

  from_account = 
    gnc_account_tree_get_current_account(GNC_ACCOUNT_TREE(xferData->from));

  to_account = 
    gnc_account_tree_get_current_account(GNC_ACCOUNT_TREE(xferData->to));

  curr_active = (xferData->exch_rate ||
		 ((from_account != NULL) && (to_account != NULL)))
		 && !gnc_commodity_equiv(xferData->from_commodity,
					 xferData->to_commodity);

  gtk_widget_set_sensitive(xferData->curr_transfer_frame, curr_active);
  gtk_widget_set_sensitive(xferData->price_edit,
			   curr_active && gtk_toggle_button_get_active
			   (GTK_TOGGLE_BUTTON(xferData->price_radio)));
  gtk_widget_set_sensitive(xferData->to_amount_edit, 
			   curr_active && gtk_toggle_button_get_active
			   (GTK_TOGGLE_BUTTON(xferData->amount_radio)));
  gtk_widget_set_sensitive(xferData->price_radio, curr_active);
  gtk_widget_set_sensitive(xferData->amount_radio, curr_active);

  gnc_xfer_dialog_set_price_auto (xferData, curr_active,
                                  xferData->from_commodity, xferData->to_commodity);
  gnc_xfer_dialog_update_conv_info(xferData);

  if (!curr_active)
  {
    GtkEntry *entry;

    gnc_amount_edit_set_amount(GNC_AMOUNT_EDIT(xferData->to_amount_edit),
                               gnc_numeric_zero ());
    entry = GTK_ENTRY(gnc_amount_edit_gtk_entry
		      (GNC_AMOUNT_EDIT(xferData->to_amount_edit)));
    gtk_entry_set_text(entry, "");
  }
}


static void
price_amount_radio_toggled_cb(GtkToggleButton *togglebutton, gpointer data)
{
  XferDialog *xferData = data;

  gtk_widget_set_sensitive(xferData->price_edit, gtk_toggle_button_get_active
			   (GTK_TOGGLE_BUTTON(xferData->price_radio)));
  gtk_widget_set_sensitive(xferData->to_amount_edit,
			   gtk_toggle_button_get_active
			   (GTK_TOGGLE_BUTTON(xferData->amount_radio)));
}


/* Reload the xferDialog quickfill with the descriptions
 * from the currently selected from account.  Note that this
 * doesn't use the initial account passed into gnc_xfer_dialog,
 * because that's NULL if no account is selected in the main
 * account window tree view.
 */
static void
gnc_xfer_dialog_reload_quickfill( XferDialog *xferData )
{
  GList *splitlist, *node;
  Split *split;
  Transaction *trans;
  Account *account;
  
  if( xferData->quickfill_to )
    account = gnc_account_tree_get_current_account(
                                      GNC_ACCOUNT_TREE(xferData->to));
  else
    account = gnc_account_tree_get_current_account(
                                      GNC_ACCOUNT_TREE(xferData->from));

  /* get a new QuickFill to use */
  gnc_quickfill_destroy( xferData->qf );
  xferData->qf = gnc_quickfill_new();

  splitlist = xaccAccountGetSplitList( account );

  for( node = splitlist; node; node = node->next )
  {
    split = node->data;
    trans = xaccSplitGetParent( split );
    gnc_quickfill_insert( xferData->qf,
                          xaccTransGetDescription (trans), QUICKFILL_LIFO);
  }
}


static void
gnc_xfer_dialog_from_tree_select_cb(GNCAccountTree *tree,
				    Account *account, gpointer data)
{
  XferDialog *xferData = data;
  GNCPrintAmountInfo print_info;
  gnc_commodity *commodity;

  account = gnc_account_tree_get_current_account(tree);
  commodity = xaccAccountGetCommodity(account);
  gtk_label_set_text(GTK_LABEL(xferData->from_currency_label), 
		     gnc_commodity_get_printname(commodity));

  xferData->from_commodity = commodity;

  gnc_xfer_dialog_curr_acct_activate(xferData);

  print_info = gnc_account_print_info (account, FALSE);

  gnc_amount_edit_set_print_info (GNC_AMOUNT_EDIT (xferData->amount_edit),
                                  print_info);
  gnc_amount_edit_set_fraction (GNC_AMOUNT_EDIT (xferData->amount_edit),
                                xaccAccountGetCommoditySCU (account));

  /* Reload the xferDialog quickfill if it is based on the from account */
  if( !xferData->quickfill_to )
    gnc_xfer_dialog_reload_quickfill(xferData);
}


static void
gnc_xfer_dialog_to_tree_select_cb(GNCAccountTree *tree,
				  Account *account, gpointer data)
{
  XferDialog *xferData = data;
  GNCPrintAmountInfo print_info;
  gnc_commodity *commodity;

  account = gnc_account_tree_get_current_account(tree);
  commodity = xaccAccountGetCommodity(account);
  gtk_label_set_text(GTK_LABEL(xferData->to_currency_label),
		     gnc_commodity_get_printname(commodity));

  xferData->to_commodity = commodity;

  gnc_xfer_dialog_curr_acct_activate(xferData);

  print_info = gnc_account_print_info (account, FALSE);

  gnc_amount_edit_set_print_info (GNC_AMOUNT_EDIT (xferData->to_amount_edit),
                                  print_info);
  gnc_amount_edit_set_fraction (GNC_AMOUNT_EDIT (xferData->to_amount_edit),
                                xaccAccountGetCommoditySCU (account));

  /* Reload the xferDialog quickfill if it is based on the to account */
  if( xferData->quickfill_to )
    gnc_xfer_dialog_reload_quickfill(xferData);
}


static void
gnc_xfer_dialog_fill_tree_frame(XferDialog *xferData,
                                XferDirection direction)
{
  const char *show_inc_exp_message = _("Show the income and expense accounts");
  GNCAccountTree *atree;
  GtkWidget *scroll_win;
  GtkWidget *button;
  GtkWidget *tree;

  tree = gnc_account_tree_new();
  atree = GNC_ACCOUNT_TREE (tree);

  if (direction == XFER_DIALOG_TO)
    xferData->to = atree;
  else
    xferData->from = atree;
  gtk_clist_column_titles_hide(GTK_CLIST(tree));
  gnc_account_tree_hide_all_but_name(GNC_ACCOUNT_TREE(tree));
  gnc_account_tree_hide_income_expense(GNC_ACCOUNT_TREE(tree));
  gnc_account_tree_refresh(GNC_ACCOUNT_TREE(tree));

  scroll_win = gnc_glade_lookup_widget (xferData->dialog,
                                        (direction == XFER_DIALOG_TO) ?
                                        "to_window" : "from_window");

  if (direction == XFER_DIALOG_TO)
    xferData->to_window = scroll_win;
  else
    xferData->from_window = scroll_win;

  gtk_container_add(GTK_CONTAINER(scroll_win), tree);

  {
    GtkStyle *st = gtk_widget_get_style(tree);
    GdkFont *font = NULL;
    gint height;

    if (st != NULL)
      font = st->font;

    if (font != NULL)
    {
      height = gdk_char_height(font, 'X');
      gtk_widget_set_usize(scroll_win, 0, (height + 6) * 10);
    }
  }

  button = gnc_glade_lookup_widget (xferData->dialog,
                                    (direction == XFER_DIALOG_TO) ?
                                    "to_show_button" : "from_show_button");

  if (direction == XFER_DIALOG_TO)
    xferData->to_show_button = button;
  else
    xferData->from_show_button = button;

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
  gtk_tooltips_set_tip(xferData->tips, button, show_inc_exp_message, NULL);

  gtk_signal_connect(GTK_OBJECT(button), "toggled",
		     GTK_SIGNAL_FUNC(gnc_xfer_dialog_toggle_cb), tree);
}


static void
gnc_parse_error_dialog (XferDialog *xferData, const char *error_string)
{
  const char * parse_error_string;

  parse_error_string = gnc_exp_parser_error_string ();
  if (parse_error_string == NULL)
    parse_error_string = "";

  if (error_string == NULL)
    error_string = "";

  gnc_error_dialog_parented (GTK_WINDOW(xferData->dialog),
			     "%s\n\n%s: %s.",
			     error_string, _("Error"),
			     parse_error_string);
}

/*** Callbacks for description quickfill. ***/

/* gnc_xfer_dialog_quickfill will update the fields of the dialog
 * based on the contents of the Description entry.  Returns TRUE
 * if the fields were updated, or FALSE if the fields were already
 * updated or if the Description couldn't be matched and no updates
 * were made.
 */
static gboolean
gnc_xfer_dialog_quickfill( XferDialog *xferData )
{
  char *desc;
  Account *match_account;  /* the matched text was from this account */
  Split *split;            /* the split to autocomplete from */
  Split *other = NULL;     /* the other split of the transaction */
  Account *other_acct = NULL;   /* the Account of the other split */
  gboolean changed = FALSE;

  if( !xferData )
    return( FALSE );

  if( xferData->quickfill_to )
    match_account = gnc_account_tree_get_current_account( xferData->to );
  else
    match_account = gnc_account_tree_get_current_account( xferData->from );

  desc = gtk_entry_get_text( GTK_ENTRY(xferData->description_entry) );

  if( !desc || desc[0] == '\0' )   /* no description to match */
    return( FALSE );

  split = xaccAccountFindSplitByDesc( match_account, desc );

  if( !split )
    return( FALSE );

  /* Now update any blank fields of the transfer dialog with
   * the memo and amount from the split, and the description
   * we were passed (assumed to match the split's transaction).
   */

  if( gnc_numeric_zero_p(
           gnc_amount_edit_get_amount(GNC_AMOUNT_EDIT(xferData->amount_edit))))
  {
    gnc_numeric amt = xaccSplitGetValue( split );

    /* If we've matched a previous transfer, it will appear
     * to be negative in the from account.
     * Need to swap the sign in order for this value
     * to be posted as a withdrawal from the "from" account.
     */
    if( gnc_numeric_negative_p( amt ) )
      amt = gnc_numeric_neg( amt );

    gnc_amount_edit_set_amount( GNC_AMOUNT_EDIT(xferData->amount_edit), amt );
    changed = TRUE;
  }

  if( !safe_strcmp(gtk_entry_get_text(GTK_ENTRY(xferData->memo_entry)),"" ))
  {
    gtk_entry_set_text( GTK_ENTRY(xferData->memo_entry),
                        xaccSplitGetMemo( split ) );
    changed = TRUE;
  }

  /* Since we're quickfilling off of one account (either from or to)
   * that account must be the account of the matched split.
   * Find the other account from the other split,
   * and select that account in the appropriate account tree.
   */
  if( ( other = xaccSplitGetOtherSplit( split ) ) &&
      ( other_acct = xaccSplitGetAccount( other ) ) )
  {
    GNCAccountTree *other_tree;
    GNCAccountType other_type;
    GtkWidget *other_button;
    
    if( xferData->quickfill_to )
    {
      other_tree = xferData->from;
      other_button = xferData->from_show_button;
    }
    else
    {
      other_tree = xferData->to;
      other_button = xferData->to_show_button;
    }

    other_type = xaccAccountGetType(other_acct);

    /* Don't want to deactivate the button just because this
     * isn't an income or expense account
     */
    if( (other_type == EXPENSE) || (other_type == INCOME) )
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(other_button), TRUE);

    gnc_account_tree_select_account(other_tree, other_acct, TRUE);

    changed = TRUE;
  }

  return( changed );
}

/* The insert_cb will do the insert and quickfill if possible, but won't
 * set the selection or cursor position since the entry widget seems to
 * change these itself.  Instead, a flag will be set and either the
 * key_press_cb or the button_release_cb will set the selection and
 * cursor position stored off from the insert_cb.
 */
static void
gnc_xfer_description_insert_cb(GtkEntry *entry,
                               const gchar *insert_text,
                               const gint insert_text_len,
                               gint *start_pos,
                               XferDialog *xferData)
{
  GdkWChar *change_text_w, *old_text_w, *new_text_w;
  int change_text_len, old_text_len, new_text_len, old_pos;
  char *new_text;
  const char *old_text, *match_str = NULL;
  QuickFill *match;
  int i;

  xferData->desc_didquickfill = FALSE;

  if ( insert_text_len <= 0 )
    return;

  old_text = gtk_entry_get_text (entry);
  if (!old_text)
    old_text = "";

  old_text_len = gnc_mbstowcs (&old_text_w, old_text);
  if (old_text_len < 0)
    return;

   /* If we are inserting in the middle, do nothing */
  if( *start_pos < old_text_len )
    return;

  /* insert_text is not NULL-terminated, how annoying */
  {
    char *temp;

    temp = g_new (char, insert_text_len + 1);
    strncpy (temp, insert_text, insert_text_len);
    temp[insert_text_len] = '\0';

    change_text_w = g_new0 (GdkWChar, insert_text_len + 1);
    change_text_len = gdk_mbstowcs (change_text_w, temp,
                                    insert_text_len);

    g_free (temp);
  }

  if (change_text_len < 0)
  {
    PERR ("bad change text conversion");
    g_free (change_text_w);
      return;
  }

  old_pos = *start_pos;

  /* Construct what the new value of the text entry will be */
  new_text_len = old_text_len + change_text_len;
  new_text_w = g_new0 (GdkWChar, new_text_len + 1);

  for (i = 0; i < *start_pos; i++)
          new_text_w[i] = old_text_w[i];

  for (i = *start_pos; i < *start_pos + change_text_len; i++)
          new_text_w[i] = change_text_w[i - *start_pos];

  for (i = *start_pos + change_text_len; i < new_text_len; i++)
          new_text_w[i] = old_text_w[i - change_text_len];

  new_text_w[new_text_len] = 0;

  new_text = gnc_wcstombs (new_text_w);

  if( ( match = gnc_quickfill_get_string_match( xferData->qf, new_text_w ) )
   && ( match_str = gnc_quickfill_string( match ) ) 
   && safe_strcmp( new_text, old_text ) )
  {
    gtk_signal_handler_block_by_data (GTK_OBJECT (entry), xferData );

    gtk_entry_set_text( entry, match_str );

    gtk_signal_handler_unblock_by_data (GTK_OBJECT (entry), xferData );

    /* stop the current insert */
    gtk_signal_emit_stop_by_name( GTK_OBJECT( entry ), "insert_text" );

    /* This doesn't seem to fix the selection problems, why? */
    gtk_entry_select_region( entry, 0, 0 );
    gtk_editable_claim_selection( GTK_EDITABLE(entry),
                                  FALSE,
                                  GDK_CURRENT_TIME );

    /* Store off data for the key_press_cb or
     * the button_release_cb to make use of. */
    xferData->desc_cursor_position = new_text_len;
    xferData->desc_start_selection = new_text_len;
    xferData->desc_end_selection = -1;
    xferData->desc_didquickfill = TRUE;
  }

  g_free( new_text );
  g_free( new_text_w );

}

/* This common post-key press and post-button release handler fixes
 * up the selection and cursor position changes that may be necessary
 * if a quickfill occurred in the insert_cb.
 */
static gboolean
common_post_quickfill_handler(guint32 time, XferDialog *xferData )
{
  GtkEntry *entry = GTK_ENTRY(xferData->description_entry);
  gint current_pos   = gtk_editable_get_position( GTK_EDITABLE(entry) );
  gint current_start = GTK_EDITABLE(entry)->selection_start_pos;
  gint current_end   = GTK_EDITABLE(entry)->selection_end_pos;
  gboolean did_something = FALSE;   /* was the selection or position changed? */

  if( current_pos != xferData->desc_cursor_position )
  {
    gtk_entry_set_position( entry, xferData->desc_cursor_position );
    did_something = TRUE;
  }

  if( ( current_start != xferData->desc_start_selection ||
        current_end   != xferData->desc_end_selection      ) &&
      ( xferData->desc_start_selection != xferData->desc_end_selection ||
        xferData->desc_start_selection == 0 ) )
  {
    gtk_entry_select_region( entry, xferData->desc_start_selection,
                                    xferData->desc_end_selection );
    gtk_editable_claim_selection( GTK_EDITABLE(entry), TRUE, time );
    did_something = TRUE;
  }

  if( did_something ) 
  {
    /* Make sure we don't try to change things again based on these values. */
    xferData->desc_start_selection = current_start;
    xferData->desc_end_selection = current_end;
    xferData->desc_cursor_position = current_pos;
  }

  /* Make sure a new quickfill must occur before coming back through here,
   * whether or not we actually did anything in this function.
   */
  xferData->desc_didquickfill = FALSE;

  return( did_something );
}

static gboolean
gnc_xfer_description_key_press_cb( GtkEntry *entry,
                                   GdkEventKey *event,
                                   XferDialog *xferData )
{
  gboolean done_with_input = FALSE;

  /* Most "special" keys are allowed to be handled directly by
   * the entry's key press handler, but in some cases that doesn't
   * seem to work right, so handle it here.
   */
  switch( event->keyval )
  {
    case GDK_Left:        /* right/left cause a focus change which is bad */
    case GDK_KP_Left:
    case GDK_Right:
    case GDK_KP_Right:
      done_with_input = TRUE;
      break;

    case GDK_Return:      /* On the first activate, need to
                           * do the quickfill completion */
    case GDK_KP_Enter:
      if( gnc_xfer_dialog_quickfill( xferData ) )
        done_with_input = TRUE;
      /* Else if no updates were done, allow the keypress to go through,
       * which will result in an activate signal for the dialog.
       */

      break;

    case GDK_Tab:
    case GDK_ISO_Left_Tab:
      if( !( event->state & GDK_SHIFT_MASK) )    /* Complete on Tab,
                                                  * but not Shift-Tab */
      {
        gnc_xfer_dialog_quickfill( xferData );
        /* NOT done with input, though, since we need to focus to the next
         * field.  Unselect the current field, though.
         */
        gtk_entry_select_region( GTK_ENTRY(xferData->description_entry), 0, 0 );
        gtk_editable_claim_selection( GTK_EDITABLE(xferData->description_entry),
                                      FALSE, event->time );
      }
      break;
  }

  /* Common handling for both key presses and button releases
   * to fix up the selection and cursor position at this point.
   */
  if( !done_with_input && xferData->desc_didquickfill )
    done_with_input = common_post_quickfill_handler( event->time, xferData );

  if( done_with_input )
    gtk_signal_emit_stop_by_name( GTK_OBJECT(entry), "key_press_event" );

  return( done_with_input );
}

static gboolean
gnc_xfer_description_button_release_cb( GtkEntry *entry,
                                        GdkEventButton *event,
                                        XferDialog *xferData )
{
  if( xferData->desc_didquickfill )
  {
    /* Common handling for both key presses and button presses
     * to fix up the selection and cursor position at this point.
     */
    common_post_quickfill_handler( event->time, xferData );
  }

  return( FALSE );
}

/*** End of quickfill-specific callbacks ***/

static void
gnc_xfer_dialog_update_conv_info (XferDialog *xferData)
{
  const gchar *to_mnemonic, *from_mnemonic;
  gchar *string;
  gnc_numeric price;

  from_mnemonic = gnc_commodity_get_mnemonic(xferData->from_commodity);
  to_mnemonic = gnc_commodity_get_mnemonic(xferData->to_commodity);

  price = gnc_amount_edit_get_amount(GNC_AMOUNT_EDIT(xferData->price_edit));
  if (gnc_numeric_zero_p(price)) {
    string = g_strdup_printf("1 %s = x %s", from_mnemonic, to_mnemonic);
    gtk_label_set_text(GTK_LABEL(xferData->conv_forward), string);
    g_free(string);

    string = g_strdup_printf("1 %s = x %s", to_mnemonic, from_mnemonic);
    gtk_label_set_text(GTK_LABEL(xferData->conv_reverse), string);
    g_free(string);
  } else {
    string = g_strdup_printf("1 %s = %f %s", from_mnemonic,
			     gnc_numeric_to_double(price), to_mnemonic);
    gtk_label_set_text(GTK_LABEL(xferData->conv_forward), string);
    g_free(string);

    price = gnc_numeric_div(gnc_numeric_create (1, 1), price,
			    GNC_DENOM_AUTO, GNC_DENOM_REDUCE);
    string = g_strdup_printf("1 %s = %f %s", to_mnemonic,
			     gnc_numeric_to_double(price), from_mnemonic);
    gtk_label_set_text(GTK_LABEL(xferData->conv_reverse), string);
    g_free(string);
  }
}

static gboolean
gnc_xfer_amount_update_cb(GtkWidget *widget, GdkEventFocus *event,
			  gpointer data)
{
  XferDialog * xferData = data;

  gnc_amount_edit_evaluate (GNC_AMOUNT_EDIT (xferData->amount_edit));

  gnc_xfer_update_to_amount (xferData);

  return FALSE;
}


static void
gnc_xfer_update_to_amount (XferDialog *xferData)
{
  gnc_numeric amount, price, to_amount;
  Account *account;

  account = gnc_account_tree_get_current_account(xferData->to);
  if (account == NULL)
    account = gnc_account_tree_get_current_account(xferData->from);

  if (account == NULL)
  {
    GtkEntry *entry;

    gnc_amount_edit_set_amount(GNC_AMOUNT_EDIT(xferData->to_amount_edit),
                               gnc_numeric_zero ());
    entry = GTK_ENTRY(gnc_amount_edit_gtk_entry
                      (GNC_AMOUNT_EDIT(xferData->to_amount_edit)));
    gtk_entry_set_text(entry, "");
  }

  gnc_amount_edit_evaluate (GNC_AMOUNT_EDIT (xferData->price_edit));

  amount = gnc_amount_edit_get_amount(GNC_AMOUNT_EDIT(xferData->amount_edit));
  price = gnc_amount_edit_get_amount(GNC_AMOUNT_EDIT(xferData->price_edit));

  if (gnc_numeric_zero_p (price))
    to_amount = gnc_numeric_zero ();
  else
    to_amount = gnc_numeric_mul (amount, price,
                                 xaccAccountGetCommoditySCU (account),
                                 GNC_RND_ROUND);

  gnc_amount_edit_set_amount(GNC_AMOUNT_EDIT(xferData->to_amount_edit),
                             to_amount);

  if (gnc_numeric_zero_p (to_amount))
  {
    GtkEntry *entry;

    entry = GTK_ENTRY(gnc_amount_edit_gtk_entry
                      (GNC_AMOUNT_EDIT(xferData->to_amount_edit)));
    gtk_entry_set_text(entry, "");
  }

  gnc_xfer_dialog_update_conv_info(xferData);
}


static gboolean
gnc_xfer_price_update_cb(GtkWidget *widget, GdkEventFocus *event,
			 gpointer data)
{
  XferDialog *xferData = data;

  gnc_xfer_update_to_amount (xferData);

  return FALSE;
}

static gboolean
gnc_xfer_date_changed_cb(GtkWidget *widget, gpointer data)
{
  XferDialog *xferData = data;

  if (xferData)
    gnc_xfer_dialog_update_price (xferData);

  return FALSE;
}

static gboolean
gnc_xfer_to_amount_update_cb(GtkWidget *widget, GdkEventFocus *event,
                             gpointer data)
{
  XferDialog *xferData = data;
  gnc_numeric amount, price, to_amount;
  Account *account;

  account = gnc_account_tree_get_current_account(xferData->to);
  if (account == NULL)
    account = gnc_account_tree_get_current_account(xferData->from);

  gnc_amount_edit_evaluate (GNC_AMOUNT_EDIT (xferData->to_amount_edit));

  amount = gnc_amount_edit_get_amount(GNC_AMOUNT_EDIT(xferData->amount_edit));
  to_amount = gnc_amount_edit_get_amount
    (GNC_AMOUNT_EDIT(xferData->to_amount_edit));
  price = gnc_numeric_div (to_amount, amount, PRECISION, GNC_RND_ROUND);
  gnc_amount_edit_set_amount(GNC_AMOUNT_EDIT(xferData->price_edit), price);
  gnc_xfer_dialog_update_conv_info(xferData);

  return FALSE;
}


static void
gnc_xfer_dialog_select_account(XferDialog *xferData, Account *account,
                               XferDirection direction)
{
  GNCAccountTree *tree;
  GtkWidget *show_button;
  gboolean is_income_expense;
  GNCAccountType type;

  if (xferData == NULL)
    return;
  if (account == NULL)
    return;

  switch (direction)
  {
    case XFER_DIALOG_FROM:
      tree = xferData->from;
      show_button = xferData->from_show_button;
      break;
    case XFER_DIALOG_TO:
      tree = xferData->to;
      show_button = xferData->to_show_button;
      break;
    default:
      return;
  }

  type = xaccAccountGetType(account);
  is_income_expense = (type == EXPENSE) || (type == INCOME);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_button),
                               is_income_expense);

  gnc_account_tree_select_account(tree, account, TRUE);
}


/********************************************************************\
 * gnc_xfer_dialog_select_from_account                              *
 *   select the from account in a xfer dialog                       *
 *                                                                  *
 * Args:   xferData - xfer dialog structure                         *
 *         account  - account to select                             *
 * Return: none                                                     *
\********************************************************************/
void
gnc_xfer_dialog_select_from_account(XferDialog *xferData, Account *account)
{
  gnc_xfer_dialog_select_account(xferData, account, XFER_DIALOG_FROM);
}


/********************************************************************\
 * gnc_xfer_dialog_select_to_account                                *
 *   select the to account in a xfer dialog                         *
 *                                                                  *
 * Args:   xferData - xfer dialog structure                         *
 *         account  - account to select                             *
 * Return: none                                                     *
\********************************************************************/
void
gnc_xfer_dialog_select_to_account(XferDialog *xferData, Account *account)
{
  gnc_xfer_dialog_select_account(xferData, account, XFER_DIALOG_TO);
}

void
gnc_xfer_dialog_select_from_currency(XferDialog *xferData, gnc_commodity *cur)
{
  if (!xferData) return;
  if (!cur) return;

  gtk_label_set_text(GTK_LABEL(xferData->from_currency_label), 
		     gnc_commodity_get_printname(cur));

  gnc_amount_edit_set_fraction (GNC_AMOUNT_EDIT (xferData->amount_edit),
                                gnc_commodity_get_fraction (cur));

  xferData->from_commodity = cur;
  gnc_xfer_dialog_curr_acct_activate(xferData);
}

void
gnc_xfer_dialog_select_to_currency(XferDialog *xferData, gnc_commodity *cur)
{
  gtk_label_set_text(GTK_LABEL(xferData->to_currency_label),
		     gnc_commodity_get_printname(cur));

  gnc_amount_edit_set_fraction (GNC_AMOUNT_EDIT (xferData->to_amount_edit),
				gnc_commodity_get_fraction (cur));

  xferData->to_commodity = cur;
  gnc_xfer_dialog_curr_acct_activate(xferData);
}

static void
gnc_xfer_dialog_lock_account_tree(XferDialog *xferData,
                                  XferDirection direction,
				  gboolean hide)
{
  GNCAccountTree *tree;
  GtkWidget *show_button;
  GtkWidget *scroll_win;

  if (xferData == NULL)
    return;

  switch (direction)
  {
    case XFER_DIALOG_FROM:
      tree = xferData->from;
      scroll_win = xferData->from_window;
      show_button = xferData->from_show_button;
      break;
    case XFER_DIALOG_TO:
      tree = xferData->to;
      scroll_win = xferData->to_window;
      show_button = xferData->to_show_button;
      break;
    default:
      return;
  }

  gtk_widget_set_sensitive( GTK_WIDGET(tree), FALSE );
  gtk_widget_set_sensitive( GTK_WIDGET(show_button), FALSE );

  if (hide) {
    gtk_widget_hide( scroll_win );
    gtk_widget_hide( GTK_WIDGET(show_button) );
  }
}


/********************************************************************\
 * gnc_xfer_dialog_lock_from_account_tree                           *
 *   prevent changes to the from account tree in an xfer dialog     *
 *                                                                  *
 * Args:   xferData - xfer dialog structure                         *
 * Return: none                                                     *
\********************************************************************/
void
gnc_xfer_dialog_lock_from_account_tree(XferDialog *xferData)
{
  gnc_xfer_dialog_lock_account_tree(xferData, XFER_DIALOG_FROM, FALSE);
}


/********************************************************************\
 * gnc_xfer_dialog_lock_to_account_tree                             *
 *   prevent changes to the to account tree in an xfer dialog       *
 *                                                                  *
 * Args:   xferData - xfer dialog structure                         *
 * Return: none                                                     *
\********************************************************************/
void
gnc_xfer_dialog_lock_to_account_tree(XferDialog *xferData)
{
  gnc_xfer_dialog_lock_account_tree(xferData, XFER_DIALOG_TO, FALSE);
}


/********************************************************************\
 * gnc_xfer_dialog_hide_from_account_tree                           *
 *   prevent changes to the from account tree in an xfer dialog     *
 *                                                                  *
 * Args:   xferData - xfer dialog structure                         *
 * Return: none                                                     *
\********************************************************************/
void
gnc_xfer_dialog_hide_from_account_tree(XferDialog *xferData)
{
  gnc_xfer_dialog_lock_account_tree(xferData, XFER_DIALOG_FROM, TRUE);
}


/********************************************************************\
 * gnc_xfer_dialog_hide_to_account_tree                             *
 *   prevent changes to the to account tree in an xfer dialog       *
 *                                                                  *
 * Args:   xferData - xfer dialog structure                         *
 * Return: none                                                     *
\********************************************************************/
void
gnc_xfer_dialog_hide_to_account_tree(XferDialog *xferData)
{
  gnc_xfer_dialog_lock_account_tree(xferData, XFER_DIALOG_TO, TRUE);
}


/********************************************************************\
 * gnc_xfer_dialog_is_exchange_dialog                               *
 *   set the dialog as an "exchange-dialog", which means that the   *
 *   Transfer Information frame is read-only (and the dialog        *
 *   will NOT create a transaction when it is closed)               *
 *                                                                  *
 * Args:   xferData - xfer dialog structure                         *
 *         exch_rate - place to store the exchange rate at exit     *
 * Return: none                                                     *
\********************************************************************/
void
gnc_xfer_dialog_is_exchange_dialog (XferDialog *xferData, gnc_numeric *exch_rate)
{
  GNCAmountEdit *gae;

  if (!xferData) return;

  gtk_widget_set_sensitive (xferData->amount_edit, FALSE);
  gtk_widget_set_sensitive (xferData->date_entry, FALSE);
  gtk_widget_set_sensitive (xferData->num_entry, FALSE);
  gtk_widget_set_sensitive (xferData->description_entry, FALSE);
  gtk_widget_set_sensitive (xferData->memo_entry, FALSE);


  gae = GNC_AMOUNT_EDIT (xferData->price_edit);
  gtk_widget_grab_focus (gnc_amount_edit_gtk_entry (gae));

  xferData->exch_rate = exch_rate;
}

/********************************************************************\
 * gnc_xfer_dialog_set_amount                                       *
 *   set the amount in the given xfer dialog                        *
 *                                                                  *
 * Args:   xferData - xfer dialog structure                         *
 *         amount   - the amount to set                             *
 * Return: none                                                     *
\********************************************************************/
void
gnc_xfer_dialog_set_amount(XferDialog *xferData, gnc_numeric amount)
{
  Account * account;

  if (xferData == NULL)
    return;

  account = gnc_account_tree_get_current_account(xferData->from);
  if (account == NULL)
    account = gnc_account_tree_get_current_account(xferData->to);

  gnc_amount_edit_set_amount (GNC_AMOUNT_EDIT (xferData->amount_edit), amount);
}


/********************************************************************\
 * gnc_xfer_dialog_set_description                                  *
 *   set the description in the given xfer dialog                   *
 *                                                                  *
 * Args:   xferData    - xfer dialog structure                      *
 *         description - the description to set                     *
 * Return: none                                                     *
\********************************************************************/
void
gnc_xfer_dialog_set_description(XferDialog *xferData, const char *description)
{
  if (xferData == NULL)
    return;

  gtk_entry_set_text(GTK_ENTRY(xferData->description_entry), description);
  gnc_quickfill_insert( xferData->qf, description, QUICKFILL_LIFO );
}

/********************************************************************\
 * gnc_xfer_dialog_set_memo                                         *
 *   set the memo in the given xfer dialog                          *
 *                                                                  *
 * Args:   xferData    - xfer dialog structure                      *
 *         memo        - the memo to set                            *
 * Return: none                                                     *
\********************************************************************/
void
gnc_xfer_dialog_set_memo(XferDialog *xferData, const char *memo)
{
  if (xferData == NULL)
    return;

  gtk_entry_set_text(GTK_ENTRY(xferData->memo_entry), memo);
  /* gnc_quickfill_insert( xferData->qf, memo, QUICKFILL_LIFO ); */
}

/********************************************************************\
 * gnc_xfer_dialog_set_num                                          *
 *   set the num in the given xfer dialog                           *
 *                                                                  *
 * Args:   xferData    - xfer dialog structure                      *
 *         num        - the num to set                              *
 * Return: none                                                     *
\********************************************************************/
void
gnc_xfer_dialog_set_num(XferDialog *xferData, const char *num)
{
  if (xferData == NULL)
    return;

  gtk_entry_set_text(GTK_ENTRY(xferData->num_entry), num);
  /* gnc_quickfill_insert( xferData->qf, num, QUICKFILL_LIFO ); */
}

/********************************************************************\
 * gnc_xfer_dialog_set_date                                         *
 *   set the date in the given xfer dialog                          *
 *                                                                  *
 * Args:   xferData    - xfer dialog structure                      *
 *         set_date    - the date to set                            *
 * Return: none                                                     *
\********************************************************************/
void
gnc_xfer_dialog_set_date(XferDialog *xferData, time_t set_date)
{
   if (xferData == NULL)
      return;

   gnc_date_edit_set_time( GNC_DATE_EDIT(xferData->date_entry), set_date );
}

void
gnc_xfer_dialog_set_exchange_rate(XferDialog *xferData, gnc_numeric exchange_rate)
{
  if (xferData == NULL)
    return;

  if (gnc_numeric_zero_p (exchange_rate))
    return;

  gnc_amount_edit_set_amount (GNC_AMOUNT_EDIT (xferData->price_edit),
			      exchange_rate);
  
  gnc_xfer_update_to_amount (xferData);
}

static void
gnc_xfer_dialog_ok_cb(GtkWidget * widget, gpointer data)
{
  XferDialog *xferData = data;
  Account *to_account;
  Account *from_account;
  gnc_commodity *from_commodity;
  gnc_commodity *to_commodity;
  gnc_numeric amount, to_amount;
  char * string;
  Timespec ts;

  gboolean curr_trans;

  Transaction *trans;
  Split *from_split;
  Split *to_split;

  from_account = gnc_account_tree_get_current_account(xferData->from);
  to_account   = gnc_account_tree_get_current_account(xferData->to);

  if (xferData->exch_rate == NULL)
  {
    if ((from_account == NULL) || (to_account == NULL))
    {
      const char *message = _("You must specify an account to transfer from,\n"
			      "or to, or both, for this transaction.\n"
			      "Otherwise, it will not be recorded.");
      gnc_error_dialog_parented(GTK_WINDOW(xferData->dialog), message);
      return;
    }

    if (from_account == to_account)
    {
      const char *message = _("You can't transfer from and to the same "
			      "account!");
      gnc_error_dialog_parented(GTK_WINDOW(xferData->dialog), message);
      return;
    }

    if (xaccAccountGetPlaceholder(from_account) ||
	xaccAccountGetPlaceholder(to_account))
    {
      const char *placeholder_format =
	_("The account %s\ndoes not allow transactions.\n");
      char *name;

      if (xaccAccountGetPlaceholder(from_account))
	name = xaccAccountGetFullName(from_account,
				      gnc_get_account_separator ());
      else
	name = xaccAccountGetFullName(to_account,
				      gnc_get_account_separator ());
      gnc_error_dialog_parented(GTK_WINDOW(xferData->dialog), placeholder_format, name);
      g_free(name);
      return;
    }

    if (safe_strcmp (gnc_commodity_get_namespace (xferData->from_commodity),
		     GNC_COMMODITY_NS_ISO))
    {
      const char *message = _("You can't transfer from a non-currency account.  "
			      "Try reversing the \"from\" and \"to\" accounts "
			      "and making the \"amount\" negative.");
      gnc_error_dialog_parented(GTK_WINDOW(xferData->dialog), message);
      return;
    }
  }

  if (!gnc_amount_edit_evaluate (GNC_AMOUNT_EDIT (xferData->amount_edit)))
  {
    gnc_parse_error_dialog (xferData, _("You must enter a valid amount."));
    return;
  }

  from_commodity = xferData->from_commodity;
  to_commodity = xferData->to_commodity;

  curr_trans = !gnc_commodity_equiv(from_commodity, to_commodity);

  amount = gnc_amount_edit_get_amount(GNC_AMOUNT_EDIT(xferData->amount_edit));

  if (gnc_numeric_zero_p (amount))
  {
    const char *message = _("You must enter an amount to transfer.");
    gnc_error_dialog_parented(GTK_WINDOW(xferData->dialog), message);
    return;
  }

  ts = gnc_date_edit_get_date_ts(GNC_DATE_EDIT(xferData->date_entry));

  if (curr_trans)
  {
    if (!gnc_amount_edit_evaluate (GNC_AMOUNT_EDIT (xferData->price_edit)))
    {
      if (gtk_toggle_button_get_active
          (GTK_TOGGLE_BUTTON(xferData->price_radio)))
      {
	gnc_parse_error_dialog (xferData, _("You must enter a valid price."));
	return;
      }
    }

    if (!gnc_amount_edit_evaluate (GNC_AMOUNT_EDIT (xferData->to_amount_edit)))
    {
      if (gtk_toggle_button_get_active
          (GTK_TOGGLE_BUTTON(xferData->amount_radio)))
      {
	gnc_parse_error_dialog (xferData,
                                _("You must enter a valid `to' amount."));
	return;
      }
    }

    to_amount = gnc_amount_edit_get_amount
      (GNC_AMOUNT_EDIT(xferData->to_amount_edit));
  }
  else
    to_amount = amount;

  gnc_suspend_gui_refresh ();

  if (xferData->exch_rate)
  {
    gnc_numeric to_amt, from_amt;

    from_amt = gnc_amount_edit_get_amount (GNC_AMOUNT_EDIT(xferData->amount_edit));
    to_amt = gnc_amount_edit_get_amount (GNC_AMOUNT_EDIT(xferData->to_amount_edit));

    *(xferData->exch_rate) =
      gnc_numeric_abs (gnc_numeric_div (to_amount, amount, GNC_DENOM_AUTO,
					GNC_DENOM_REDUCE));
  }
  else
  {
    /* Create the transaction */
    trans = xaccMallocTransaction(xferData->book);

    xaccTransBeginEdit(trans);

    xaccTransSetCurrency(trans, from_commodity);
    xaccTransSetDatePostedTS(trans, &ts);

    string = gtk_entry_get_text(GTK_ENTRY(xferData->num_entry));
    xaccTransSetNum(trans, string);

    string = gtk_entry_get_text(GTK_ENTRY(xferData->description_entry));
    xaccTransSetDescription(trans, string);

    /* create from split */
    from_split = xaccMallocSplit(xferData->book);
    xaccTransAppendSplit(trans, from_split); 

    /* create to split */
    to_split = xaccMallocSplit(xferData->book);
    xaccTransAppendSplit(trans, to_split); 

    xaccAccountBeginEdit(from_account);
    xaccAccountInsertSplit(from_account, from_split);

    xaccAccountBeginEdit(to_account);
    xaccAccountInsertSplit(to_account, to_split);

    xaccSplitSetBaseValue(from_split, gnc_numeric_neg (amount), from_commodity);
    xaccSplitSetBaseValue(to_split, amount, from_commodity);
    xaccSplitSetBaseValue(to_split, to_amount, to_commodity);

    /* Set the memo fields */
    string = gtk_entry_get_text(GTK_ENTRY(xferData->memo_entry));
    xaccSplitSetMemo(from_split, string);
    xaccSplitSetMemo(to_split, string);

    /* finish transaction */
    xaccTransCommitEdit(trans);
    xaccAccountCommitEdit(from_account);
    xaccAccountCommitEdit(to_account);
  }

  /* try to save this to the pricedb */
  if (xferData->pricedb) {
    gnc_commodity *from = xferData->from_commodity;
    gnc_commodity *to = xferData->to_commodity;

    /* only continue if the currencies are DIFFERENT and are
     * not both euroland currencies 
     */
    if (!gnc_commodity_equal (from, to) &&
	!(gnc_is_euro_currency (from) && gnc_is_euro_currency (to)))
    {
      GNCPrice *price;
      GList *prices;

      /* First see if an entry exists at time ts */
      prices = gnc_pricedb_lookup_at_time (xferData->pricedb, from, to, ts);
      if (prices) {
	PINFO("Found price for %s in %s", gnc_commodity_get_mnemonic(from),
	      gnc_commodity_get_mnemonic(to));
      } else {
	prices = gnc_pricedb_lookup_at_time (xferData->pricedb, to, from, ts);
	if (prices) {
	  PINFO("Found reverse price for %s in %s", gnc_commodity_get_mnemonic(to),
		gnc_commodity_get_mnemonic(from));
	}
      }

      /* If so, do nothing (well, destroy the list).  if not, create one. */
      if (prices) {
	gnc_price_list_destroy (prices);
      } else {
	gnc_commodity *tmp;
	gnc_numeric value;

	/* compute the price -- maybe we need to swap? */
	value = gnc_amount_edit_get_amount(GNC_AMOUNT_EDIT(xferData->price_edit));
	value = gnc_numeric_abs (value);

	/* Try to be consistent about how quotes are installed. */
	if (from == gnc_default_currency()) {
	  tmp = from; from = to; to = tmp;
	  value = gnc_numeric_div (gnc_numeric_create(1,1), value,
				   PRECISION, GNC_RND_ROUND);
	} else if ((to != gnc_default_currency()) &&
		   (strcmp (gnc_commodity_get_mnemonic(from),
			    gnc_commodity_get_mnemonic(to)) < 0)) {
	  tmp = from; from = to; to = tmp;
	  value = gnc_numeric_div (gnc_numeric_create(1,1), value,
				   PRECISION, GNC_RND_ROUND);
	}

	price = gnc_price_create (xferData->book);
	gnc_price_begin_edit (price);
	gnc_price_set_commodity (price, from);
	gnc_price_set_currency (price, to);
	gnc_price_set_time (price, ts);
	gnc_price_set_source (price, "user:xfer-dialog");
	gnc_price_set_value (price, value);
	gnc_pricedb_add_price (xferData->pricedb, price);
	gnc_price_commit_edit (price);
	gnc_price_unref (price);
	PINFO("Created price: 1 %s = %f %s", gnc_commodity_get_mnemonic(from),
	      gnc_numeric_to_double(value), gnc_commodity_get_mnemonic(to));
      }
    }
  }

  /* Refresh everything */
  gnc_resume_gui_refresh ();

  /* Tell the caller that this is "ok" */
  if (xferData->result_p)
    *(xferData->result_p) = TRUE;

  gnc_close_gui_component_by_data (DIALOG_TRANSFER_CM_CLASS, xferData);
}


static void
gnc_xfer_dialog_cancel_cb(GtkWidget * widget, gpointer data)
{
  XferDialog *xferData = data; 

  gnc_close_gui_component_by_data (DIALOG_TRANSFER_CM_CLASS, xferData);
}


static int
gnc_xfer_dialog_close_cb(GnomeDialog *dialog, gpointer data)
{
  XferDialog * xferData = data;
  GtkWidget *entry;

  entry = gnc_amount_edit_gtk_entry(GNC_AMOUNT_EDIT(xferData->amount_edit));
  gtk_signal_disconnect_by_data(GTK_OBJECT(entry), xferData);

  entry = gnc_amount_edit_gtk_entry(GNC_AMOUNT_EDIT(xferData->price_edit));
  gtk_signal_disconnect_by_data(GTK_OBJECT(entry), xferData);

  entry = gnc_amount_edit_gtk_entry(GNC_AMOUNT_EDIT(xferData->to_amount_edit));
  gtk_signal_disconnect_by_data(GTK_OBJECT(entry), xferData);

  entry = xferData->description_entry;
  gtk_signal_disconnect_by_data(GTK_OBJECT(entry), xferData);

  gtk_object_unref (GTK_OBJECT (xferData->tips));

  gnc_unregister_gui_component_by_data (DIALOG_TRANSFER_CM_CLASS, xferData);

  gnc_quickfill_destroy (xferData->qf);
  xferData->qf = NULL;

  g_free(xferData);

  DEBUG("xfer dialog destroyed\n");

  return FALSE;
}


static void
gnc_xfer_dialog_create(GtkWidget * parent, XferDialog *xferData)
{
  GtkWidget *dialog;
  GladeXML  *xml;

  xml = gnc_glade_xml_new ("transfer.glade", "Transfer Dialog");

  dialog = glade_xml_get_widget (xml, "Transfer Dialog");
  xferData->dialog = dialog;

  /* parent */
  if (parent != NULL)
    gnome_dialog_set_parent(GNOME_DIALOG(dialog), GTK_WINDOW(parent));

  /* default to ok */
  gnome_dialog_set_default(GNOME_DIALOG(dialog), 0);

  gnome_dialog_button_connect(GNOME_DIALOG(dialog), 0,
                              GTK_SIGNAL_FUNC(gnc_xfer_dialog_ok_cb),
                              xferData);

  gnome_dialog_button_connect(GNOME_DIALOG(dialog), 1,
                              GTK_SIGNAL_FUNC(gnc_xfer_dialog_cancel_cb),
                              xferData);

  gtk_signal_connect(GTK_OBJECT(dialog), "close",
                     GTK_SIGNAL_FUNC(gnc_xfer_dialog_close_cb), xferData);

  xferData->tips = gtk_tooltips_new();

  gtk_object_ref (GTK_OBJECT (xferData->tips));
  gtk_object_sink (GTK_OBJECT (xferData->tips));

  /* amount & date widgets */
  {
    GtkWidget *amount;
    GtkWidget *entry;
    GtkWidget *date;
    GtkWidget *hbox;

    amount = gnc_amount_edit_new();
    hbox = glade_xml_get_widget (xml, "amount_hbox");
    gtk_box_pack_end(GTK_BOX(hbox), amount, TRUE, TRUE, 0);
    gnc_amount_edit_set_evaluate_on_enter (GNC_AMOUNT_EDIT (amount), TRUE);
    xferData->amount_edit = amount;

    entry = gnc_amount_edit_gtk_entry (GNC_AMOUNT_EDIT (amount));
    gtk_signal_connect(GTK_OBJECT(entry), "focus-out-event",
                       GTK_SIGNAL_FUNC(gnc_xfer_amount_update_cb), xferData);

    date = gnc_date_edit_new(time(NULL), FALSE, FALSE);
    hbox = glade_xml_get_widget (xml, "date_hbox");

    gtk_box_pack_end(GTK_BOX(hbox), date, TRUE, TRUE, 0);
    xferData->date_entry = date;
    gtk_signal_connect(GTK_OBJECT(date), "date_changed",
		       GTK_SIGNAL_FUNC(gnc_xfer_date_changed_cb), xferData);
  }

  {
    GtkWidget *entry;

    entry = glade_xml_get_widget (xml, "num_entry");
    xferData->num_entry = entry;
    gnome_dialog_editable_enters(GNOME_DIALOG(dialog), GTK_EDITABLE(entry));

    entry = glade_xml_get_widget (xml, "description_entry");
    xferData->description_entry = entry;

    /* Get signals from the Description for quickfill. */
    gtk_signal_connect(GTK_OBJECT(entry), "insert_text",
                       GTK_SIGNAL_FUNC(gnc_xfer_description_insert_cb),
                       xferData);
    gtk_signal_connect(GTK_OBJECT(entry), "button_release_event",
                       GTK_SIGNAL_FUNC(gnc_xfer_description_button_release_cb),
                       xferData);
    gtk_signal_connect_after
      (GTK_OBJECT(entry), "key_press_event",
       GTK_SIGNAL_FUNC(gnc_xfer_description_key_press_cb), xferData);

    entry = glade_xml_get_widget (xml, "memo_entry");
    xferData->memo_entry = entry;
    gnome_dialog_editable_enters(GNOME_DIALOG(dialog), GTK_EDITABLE(entry));
  }

  /* from and to */
  {
    GtkWidget *label;

    gnc_xfer_dialog_fill_tree_frame(xferData, XFER_DIALOG_TO);
    gnc_xfer_dialog_fill_tree_frame(xferData, XFER_DIALOG_FROM);

    gtk_signal_connect(GTK_OBJECT(xferData->from), "select_account",
		       GTK_SIGNAL_FUNC(gnc_xfer_dialog_from_tree_select_cb),
		       xferData);
    gtk_signal_connect(GTK_OBJECT(xferData->to), "select_account",
		       GTK_SIGNAL_FUNC(gnc_xfer_dialog_to_tree_select_cb),
		       xferData);

    label = glade_xml_get_widget (xml, "from_currency_label");
    xferData->from_currency_label = label;

    label = glade_xml_get_widget (xml, "to_currency_label");
    xferData->to_currency_label = label;

    label = glade_xml_get_widget (xml, "conv_forward");
    xferData->conv_forward = label;

    label = glade_xml_get_widget (xml, "conv_reverse");
    xferData->conv_reverse = label;
  }

  /* optional intermediate currency account */
  {
    GtkWidget *frame;
    GtkWidget *entry;
    GtkWidget *edit;
    GtkWidget *hbox;
    GtkWidget *button;

    frame = glade_xml_get_widget (xml, "curr_transfer_frame");
    xferData->curr_transfer_frame = frame;

    edit = gnc_amount_edit_new();
    gnc_amount_edit_set_print_info(GNC_AMOUNT_EDIT(edit),
                                   gnc_default_price_print_info ());
    gnc_amount_edit_set_fraction(GNC_AMOUNT_EDIT(edit), PRECISION);
    hbox = glade_xml_get_widget (xml, "price_hbox");
    gtk_box_pack_start(GTK_BOX(hbox), edit, TRUE, TRUE, 0);
    xferData->price_edit = edit;
    entry = gnc_amount_edit_gtk_entry (GNC_AMOUNT_EDIT (edit));
    gtk_signal_connect(GTK_OBJECT(entry), "focus-out-event",
                       GTK_SIGNAL_FUNC(gnc_xfer_price_update_cb), xferData);
    gnome_dialog_editable_enters(GNOME_DIALOG(dialog), GTK_EDITABLE(entry));

    edit = gnc_amount_edit_new();
    hbox = glade_xml_get_widget (xml, "to_amount_hbox");
    gtk_box_pack_start(GTK_BOX(hbox), edit, TRUE, TRUE, 0);
    xferData->to_amount_edit = edit;
    entry = gnc_amount_edit_gtk_entry (GNC_AMOUNT_EDIT (edit));
    gtk_signal_connect(GTK_OBJECT(entry), "focus-out-event",
                       GTK_SIGNAL_FUNC(gnc_xfer_to_amount_update_cb),
		       xferData);
    gnome_dialog_editable_enters(GNOME_DIALOG(dialog), GTK_EDITABLE(entry));

    button = glade_xml_get_widget (xml, "price_radio");
    xferData->price_radio = button;
    gtk_signal_connect(GTK_OBJECT(xferData->price_radio), "toggled",
		       GTK_SIGNAL_FUNC(price_amount_radio_toggled_cb),
		       xferData);

    button = glade_xml_get_widget (xml, "amount_radio");
    xferData->amount_radio = button;
    gtk_signal_connect(GTK_OBJECT(xferData->amount_radio), "toggled",
		       GTK_SIGNAL_FUNC(price_amount_radio_toggled_cb),
		       xferData);
  }

  gtk_clist_set_selection_mode(GTK_CLIST(xferData->from),
                               GTK_SELECTION_BROWSE);
  gtk_clist_set_selection_mode(GTK_CLIST(xferData->to),
                               GTK_SELECTION_BROWSE);

  /* default to quickfilling off of the "From" account. */
  xferData->quickfill_to = FALSE;
}

static void
close_handler (gpointer user_data)
{
  XferDialog *xferData = user_data;

  gnome_dialog_close (GNOME_DIALOG (xferData->dialog));
}

/********************************************************************\
 * gnc_xfer_dialog                                                  *
 *   opens up a window to do an automatic transfer between accounts *
 *                                                                  * 
 * Args:   parent  - the parent of the window to be created         *
 *         initial - the initial account in the from/to fields      *
 * Return: XferDialog structure                                     *
\********************************************************************/
XferDialog *
gnc_xfer_dialog (GtkWidget * parent, Account * initial)
{
  XferDialog *xferData;
  GNCAmountEdit *gae;
  GtkWidget *amount_entry;
  GNCBook *book = NULL;

  xferData = g_new0 (XferDialog, 1);

  xferData->desc_cursor_position = 0;
  xferData->desc_start_selection = 0;
  xferData->desc_end_selection = 0;
  xferData->desc_didquickfill = FALSE;

  if (initial) {
    book = xaccAccountGetBook (initial);
  } else {
    book = gnc_get_current_book ();
  }

  xferData->book = book;
  xferData->pricedb = gnc_book_get_pricedb (book);

  gnc_xfer_dialog_create(parent, xferData);

  gnc_register_gui_component (DIALOG_TRANSFER_CM_CLASS,
                              NULL, close_handler, xferData);

  gae = GNC_AMOUNT_EDIT(xferData->amount_edit);
  amount_entry = gnc_amount_edit_gtk_entry (gae);

  gtk_widget_grab_focus(amount_entry);

  gnc_xfer_dialog_select_from_account(xferData, initial);
  gnc_xfer_dialog_select_to_account(xferData, initial);

  gnc_xfer_dialog_curr_acct_activate(xferData);

  gtk_widget_show_all(xferData->dialog);

  gnc_window_adjust_for_screen(GTK_WINDOW(xferData->dialog));

  return xferData;
}

void
gnc_xfer_dialog_close( XferDialog *xferData )
{
  if( xferData )
    gnc_close_gui_component_by_data (DIALOG_TRANSFER_CM_CLASS, xferData);
}

void
gnc_xfer_dialog_set_title( XferDialog *xferData, const gchar *title )
{
  if( xferData && title )
  {
    gtk_window_set_title (GTK_WINDOW (xferData->dialog), title);
  }
}

void
gnc_xfer_dialog_set_information_frame_label( XferDialog *xferData,
                                             const gchar *label )
{
  if( xferData && label )
  {
    GtkWidget *frame = gnc_glade_lookup_widget (xferData->dialog,
                                                "transferinfo-frame" );
    gtk_frame_set_label( GTK_FRAME(frame), label );
  }
}


static void
gnc_xfer_dialog_set_account_frame_label( XferDialog *xferData,
                                         const gchar *label,
                                         XferDirection direction)
{
  if( xferData && label )
  {
    /* "frame34" is the from frame, "frame35" is the to frame */
    GtkWidget *frame = gnc_glade_lookup_widget (xferData->dialog,
                                             ( direction == XFER_DIALOG_FROM ?
                                               "transferfrom-frame" :
                                               "transferto-frame" ));
    gtk_frame_set_label( GTK_FRAME(frame), label );
  }
}

void
gnc_xfer_dialog_set_from_account_frame_label( XferDialog *xferData,
                                              const gchar *label )
{
  gnc_xfer_dialog_set_account_frame_label( xferData, label, XFER_DIALOG_FROM );
}

void
gnc_xfer_dialog_set_to_account_frame_label( XferDialog *xferData,
                                            const gchar *label )
{
  gnc_xfer_dialog_set_account_frame_label( xferData, label, XFER_DIALOG_TO );
}

void
gnc_xfer_dialog_set_from_show_button_active( XferDialog *xferData,
                                             gboolean set_value )
{
  if( xferData && xferData->from_show_button )
  {
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(xferData->from_show_button),
                                                    set_value );
  }
}

void
gnc_xfer_dialog_set_to_show_button_active( XferDialog *xferData,
                                           gboolean set_value )
{
  if( xferData && xferData->to_show_button )
  {
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(xferData->to_show_button),
                                  set_value );
  }
}

/* Add a button with a user-specified label and "clicked" callback */
void gnc_xfer_dialog_add_user_specified_button( XferDialog *xferData,
                                                const gchar *label,
                                                GtkSignalFunc callback,
                                                gpointer user_data )
{
  if( xferData && label && callback )
  {
    GtkWidget *button = gtk_button_new_with_label( label );
    GtkWidget *box    = gnc_glade_lookup_widget (xferData->dialog,
                                                 "transfermain-vbox" );
    gtk_box_pack_end( GTK_BOX(box), button, FALSE, FALSE, 0 );
    gtk_signal_connect( GTK_OBJECT(button), "clicked",
                        GTK_SIGNAL_FUNC( callback ), user_data );
    gtk_widget_show( button );
  }
}

void gnc_xfer_dialog_toggle_currency_frame( XferDialog *xferData, 
                                            gboolean show_frame )
{
  if( xferData && xferData->curr_transfer_frame )
  {
    if( show_frame )
      gtk_widget_show( xferData->curr_transfer_frame );
    else
      gtk_widget_hide( xferData->curr_transfer_frame );
  }
}


/* helper function */
static gboolean
find_xfer (gpointer find_data, gpointer user_data)
{
  return( find_data == user_data );
}

/* Run the dialog until the user has either successfully completed the
 * transaction (just clicking OK doesn't always count) or clicked Cancel.
 * Return TRUE if the transaction was a success, FALSE otherwise.
 */
gboolean gnc_xfer_dialog_run_until_done( XferDialog *xferData )
{
  gboolean result_ok = FALSE;

  if( xferData )
  {
    xferData->result_p = &result_ok;
    while( TRUE )
    {
      gnome_dialog_run( GNOME_DIALOG(xferData->dialog) );

      if( result_ok == TRUE )
      {
        /* See if the dialog is still there.  For various reasons, the
         * user could have hit OK but remained in the dialog.  We don't
         * want to return processing back to anyone else until we clear
         * off this dialog, so if the dialog is still there we'll just
         * run it again.
         */
  
        if( !gnc_find_first_gui_component( DIALOG_TRANSFER_CM_CLASS,
                                           find_xfer, xferData ) )
        {
          /* no more dialog, and OK was clicked, so assume it's all good */
          return( TRUE );
        }
        /* else run the dialog again */
  
      }
      else   /* result was Cancel */
      {
        return( FALSE );
      }
    }
  }
    
  return( FALSE );
}


/* Indicate that the dialog should quickfill based on the "To" account,
 * rather than the default which is the "From" account.
 */

void
gnc_xfer_dialog_quickfill_to_account(XferDialog *xferData,
                                     gboolean qf_to_account )
{
  gboolean old = xferData->quickfill_to;
  xferData->quickfill_to = qf_to_account;

  /* reload the quickfill if necessary */
  if( old != qf_to_account )
    gnc_xfer_dialog_reload_quickfill( xferData );
}
