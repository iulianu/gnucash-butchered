/********************************************************************\
 * window-reconcile.c -- the reconcile window                       *
 * Copyright (C) 1997 Robin D. Clark                                *
 * Copyright (C) 1998-2000 Linas Vepstas                            *
 * Copyright (C) 2002 Christian Stimming                            *
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
 *                                                                  *
 *   Author: Rob Clark                                              *
 * Internet: rclark@cs.hmc.edu                                      *
 *  Address: 609 8th Street                                         *
 *           Huntington Beach, CA 92648-4632                        *
\********************************************************************/

#define _GNU_SOURCE

#include "config.h"

#include <gnome.h>
#include <stdio.h>
#include <time.h>
#include <libgnomeui/gnome-window-icon.h>

#include "Scrub.h"
#include "dialog-account.h"
#include "dialog-transfer.h"
#include "dialog-utils.h"
#include "dialog-transfer.h"
#include "gnc-amount-edit.h"
#include "gnc-component-manager.h"
#include "gnc-date.h"
#include "gnc-date-edit.h"
#include "gnc-engine-util.h"
#include "gnc-gconf-utils.h"
#include "gnc-gui-query.h"
#include "gnc-ledger-display.h"
#include "gnc-main-window.h"
#include "gnc-plugin-page.h"
#include "gnc-plugin-page-register.h"
#include "gnc-ui-util.h"
#include "gnc-ui.h"
#include "guile-util.h"
#include "messages.h"
#include "reconcile-list.h"
#include "window-reconcile.h"
#include "top-level.h"

#define WINDOW_RECONCILE_CM_CLASS "window-reconcile"


/** STRUCTS *********************************************************/
struct _RecnWindow
{
  GUID account;             /* The account that we are reconciling  */
  gnc_numeric new_ending;   /* The new ending balance               */
  time_t statement_date;    /* The statement date                   */

  gint component_id;        /* id of component                      */

  GtkWidget *window;        /* The reconcile window                 */

  GtkWidget *toolbar;       /* Toolbar widget                       */
  gint toolbar_change_cb_id;  /* id for toolbar preference change cb  */
  gint toolbar_change_cb_id2; /* id for toolbar preference change cb  */

  GtkWidget *starting;      /* The starting balance                 */
  GtkWidget *ending;        /* The ending balance                   */
  GtkWidget *reconciled;    /* The reconciled balance               */
  GtkWidget *difference;    /* Text field, amount left to reconcile */

  GtkWidget *total_debit;   /* Text field, total debit reconciled   */
  GtkWidget *total_credit;  /* Text field, total credit reconciled  */

  GtkWidget *debit;         /* Debit matrix show unreconciled debit */
  GtkWidget *credit;        /* Credit matrix, shows credits...      */

  GtkWidget *debit_frame;   /* Frame around debit matrix            */
  GtkWidget *credit_frame;  /* Frame around credit matrix           */

  GtkWidget *edit_item;     /* Edit transaction menu item           */
  GtkWidget *delete_item;   /* Delete transaction menu item         */

  GtkWidget *edit_popup;    /* Edit transaction popup menu item     */
  GtkWidget *delete_popup;  /* Delete transaction popup menu item   */

  GtkWidget *edit_button;   /* Edit transaction button              */
  GtkWidget *delete_button; /* Delete transaction button            */
  GtkWidget *finish_button; /* Finish reconciliation button         */

  gboolean delete_refresh;  /* do a refresh upon a window deletion  */
};

/* This structure doesn't contain everything involved in the
 * startRecnWindow, just pointers that have to be passed in to
 * callbacks that need more than one piece of data to operate on.
 * This is also used by the interest transfer dialog code.
 */
typedef struct _startRecnWindowData
{
  Account       *account;         /* the account being reconciled            */
  GNCAccountType account_type;    /* the type of the account                 */

  GtkWidget     *startRecnWindow; /* the startRecnWindow dialog              */
  GtkWidget     *xfer_button;     /* the dialog's interest transfer button   */
  GtkWidget     *date_value;      /* the dialog's ending date field          */
  GNCAmountEdit *end_value;       /* the dialog's ending balance amount edit */
  gnc_numeric    original_value;  /* the dialog's original ending balance    */
  gboolean       user_set_value;  /* the user changed the ending value       */

  XferDialog    *xferData;        /* the interest xfer dialog (if it exists) */
  gboolean       include_children;

  time_t         date;            /* the interest xfer reconcile date        */
} startRecnWindowData;


/* Note: make sure to update the help text for this in prefs.scm if these
 * change!  These macros define the account types for which an auto interest
 * xfer dialog could pop up, if the user's preferences allow it.
 */
#define account_type_has_auto_interest_charge(type)  (((type) == CREDIT) || \
                                                      ((type) == LIABILITY) ||\
						      ((type) == PAYABLE))

#define account_type_has_auto_interest_payment(type) (((type) == BANK)  || \
                                                      ((type) == ASSET) || \
                                                      ((type) == MUTUAL) || \
						      ((type) == RECEIVABLE))

#define account_type_has_auto_interest_xfer(type) \
  (  account_type_has_auto_interest_charge(type) || \
    account_type_has_auto_interest_payment(type) )

/** PROTOTYPES ******************************************************/
static gnc_numeric recnRecalculateBalance (RecnWindow *recnData);

static void   recn_destroy_cb (GtkWidget *w, gpointer data);
static void   recnFinishCB (GtkWidget *w, gpointer data);
static void   recnPostponeCB (GtkWidget *w, gpointer data);
static void   recnCancelCB (GtkWidget *w, gpointer data);

void gnc_start_recn_children_changed (GtkWidget *widget, startRecnWindowData *data);
void gnc_start_recn_interest_clicked_cb(GtkButton *button, startRecnWindowData *data);

static void   gnc_reconcile_window_set_sensitivity(RecnWindow *recnData);
static char * gnc_recn_make_window_name(Account *account);
static void   gnc_recn_set_window_name(RecnWindow *recnData);
static gboolean find_by_account (gpointer find_data, gpointer user_data);


/** GLOBALS *********************************************************/
/* This static indicates the debugging module that this .o belongs to. */
/* static short module = MOD_GUI; */

static time_t gnc_reconcile_last_statement_date = 0;


/** IMPLEMENTATIONS *************************************************/


/********************************************************************\
 * recnRefresh                                                      *
 *   refreshes the transactions in the reconcile window             *
 *                                                                  *
 * Args:   account - the account of the reconcile window to refresh *
 * Return: none                                                     *
\********************************************************************/
static void
recnRefresh (RecnWindow *recnData)
{
  if (recnData == NULL)
    return;

  gnc_reconcile_list_refresh(GNC_RECONCILE_LIST(recnData->debit));
  gnc_reconcile_list_refresh(GNC_RECONCILE_LIST(recnData->credit));

  gnc_reconcile_window_set_sensitivity(recnData);

  gnc_recn_set_window_name(recnData);

  recnRecalculateBalance(recnData);

  gtk_widget_queue_resize(recnData->window);
}


static Account *
recn_get_account (RecnWindow *recnData)
{
  if (!recnData)
    return NULL;

  return xaccAccountLookup (&recnData->account, gnc_get_current_book ());
}

/********************************************************************\
 * recnRecalculateBalance                                           *
 *   refreshes the balances in the reconcile window                 *
 *                                                                  *
 * Args:   recnData -- the reconcile window to refresh              *
 * Return: the difference between the nominal ending balance        *
 *         and the 'effective' ending balance.                      *
\********************************************************************/
static gnc_numeric
recnRecalculateBalance (RecnWindow *recnData)
{
  Account *account;
  const char *amount;
  gnc_numeric debit;
  gnc_numeric credit;
  gnc_numeric starting;
  gnc_numeric ending;
  gnc_numeric reconciled;
  gnc_numeric diff;
  GNCPrintAmountInfo print_info;
  gboolean reverse_balance, include_children;

  account = recn_get_account (recnData);
  if (!account)
    return gnc_numeric_zero ();

  reverse_balance = gnc_reverse_balance(account);

  /* update the starting balance */
  include_children = xaccAccountGetReconcileChildrenStatus(account);
  starting = gnc_ui_account_get_reconciled_balance(account, include_children);
  print_info = gnc_account_print_info (account, TRUE);

  /*
   * Do not reverse the balance here.  It messes up the math in the
   * reconciliation window.  Also, the balance should show up as a
   * positive number in the reconciliation window to match the positive
   * number that shows in the register window.
   */

  amount = xaccPrintAmount(starting, print_info);
  gnc_set_label_color(recnData->starting, starting);
  gtk_label_set_text(GTK_LABEL(recnData->starting), amount);
  if (reverse_balance)
    starting = gnc_numeric_neg (starting);

  /* update the ending balance */
  ending = recnData->new_ending;
  if (reverse_balance)
    ending = gnc_numeric_neg (ending);
  amount = xaccPrintAmount(ending, print_info);
  gnc_set_label_color(recnData->ending, ending);
  gtk_label_set_text(GTK_LABEL(recnData->ending), amount);
  if (reverse_balance)
    ending = gnc_numeric_neg (ending);

  debit = gnc_reconcile_list_reconciled_balance
    (GNC_RECONCILE_LIST(recnData->debit));

  credit = gnc_reconcile_list_reconciled_balance
    (GNC_RECONCILE_LIST(recnData->credit));

  /* Update the total debit and credit fields */
  amount = xaccPrintAmount(debit, print_info);
  gtk_label_set_text(GTK_LABEL(recnData->total_debit), amount);

  amount = xaccPrintAmount(credit, print_info);

  gtk_label_set_text(GTK_LABEL(recnData->total_credit), amount);

  /* update the reconciled balance */
  reconciled = gnc_numeric_add_fixed (starting,
                                      gnc_numeric_sub_fixed (debit, credit));
  if (reverse_balance)
    reconciled = gnc_numeric_neg (reconciled);
  amount = xaccPrintAmount(reconciled, print_info);
  gnc_set_label_color(recnData->reconciled, reconciled);
  gtk_label_set_text(GTK_LABEL(recnData->reconciled), amount);
  if (reverse_balance)
    reconciled = gnc_numeric_neg (reconciled);

  /* update the difference */
  diff = gnc_numeric_sub_fixed (ending, reconciled);
  if (reverse_balance)
    diff = gnc_numeric_neg (diff);
  amount = xaccPrintAmount(diff, print_info);
  gnc_set_label_color(recnData->difference, diff);
  gtk_label_set_text(GTK_LABEL(recnData->difference), amount);
  if (reverse_balance)
    diff = gnc_numeric_neg (diff);

  gtk_widget_set_sensitive(recnData->finish_button, gnc_numeric_zero_p (diff));

  return diff;
}

static gboolean
gnc_start_recn_update_cb(GtkWidget *widget, GdkEventFocus *event,
                         startRecnWindowData *data)
{
  gnc_numeric value;

  gnc_amount_edit_evaluate (GNC_AMOUNT_EDIT(data->end_value));

  value = gnc_amount_edit_get_amount (GNC_AMOUNT_EDIT(data->end_value));
  data->user_set_value = !gnc_numeric_equal(value, data->original_value);
  return FALSE;
}

/* If the user changed the date edit widget, update the
 * ending balance to reflect the ending balance of the account
 * on the date that the date edit was changed to.
 */
static void
gnc_start_recn_date_changed (GtkWidget *widget, startRecnWindowData *data)
{
  GNCDateEdit *gde = GNC_DATE_EDIT (widget);
  gnc_numeric new_balance;
  time_t new_date;

  if (data->user_set_value)
    return;

  new_date = gnc_date_edit_get_date_end (gde);

  /* get the balance for the account as of the new date */
  new_balance = gnc_ui_account_get_balance_as_of_date (data->account, new_date,
						       data->include_children);

  /* update the amount edit with the amount */
  gnc_amount_edit_set_amount (GNC_AMOUNT_EDIT (data->end_value),
                              new_balance);
}

void
gnc_start_recn_children_changed (GtkWidget *widget, startRecnWindowData *data)
{
  data->include_children =
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

  /* Force an update of the ending balance */
  gnc_start_recn_date_changed (data->date_value, data);
}

/* For a given account, determine if an auto interest xfer dialog should be
 * shown, based on both the per-account flag as well as the global reconcile
 * option.  The global option is the default that is used if there is no
 * per-account option.
 */
static gboolean
gnc_recn_interest_xfer_get_auto_interest_xfer_allowed( Account *account )
{
  gboolean auto_xfer;

  auto_xfer = gnc_gconf_get_bool(GCONF_RECONCILE_SECTION,
				 "auto_interest_transfer", NULL);
  return xaccAccountGetAutoInterestXfer( account, auto_xfer );
}

/********************************************************************\
 * recnInterestXferWindow                                           *
 *   opens up a window to prompt the user to enter an interest      *
 *   charge or payment for an account prior to reconciling it.      *
 *   Only to be called for some types of accounts, as defined       *
 *   in the macros at the top of this file.                         *
 *                                                                  *
 * NOTE: This function does not return until the user presses "Ok"  *
 *       or "Cancel", which means that the transaction must be      *
 *       resolved before the startRecnWindow will work.             *
 *                                                                  *
 * Args:   data           - jumbo structure containing info         *
 *                          about the start of the reconcile        *
 *                          process needed by this function.        *
 * Returns:  none.                                                  *
\********************************************************************/

/* helper function */
static char *
gnc_recn_make_interest_window_name(Account *account, char *text)
{
  char *fullname;
  char *title;

  fullname = xaccAccountGetFullName(account, gnc_get_account_separator());
  title = g_strconcat(fullname, " - ", _(text), NULL);

  g_free(fullname);

  return title;
}

/* user clicked button in the interest xfer dialog entitled
 * "No Auto Interest Payments for this Account".
 */
static void
gnc_recn_interest_xfer_no_auto_clicked_cb(GtkButton *button,
                                          startRecnWindowData *data)
{
  /* Indicate that the user doesn't want
   * an auto interest xfer for this account.
   */
  xaccAccountSetAutoInterestXfer( data->account, FALSE );

  /* shut down the interest xfer dialog */
  gnc_xfer_dialog_close( data->xferData );

  /* make the button clickable again */
  if( data->xfer_button )
    gtk_widget_set_sensitive(GTK_WIDGET(data->xfer_button), TRUE);
}

static void
recnInterestXferWindow( startRecnWindowData *data)
{
  gchar *title;

  if( !account_type_has_auto_interest_xfer( data->account_type ) ) return;

  /* get a normal transfer dialog... */
  data->xferData = gnc_xfer_dialog( GTK_WIDGET(data->startRecnWindow),
                                    data->account );

  /* ...and start changing things: */

  /* change title */
  if( account_type_has_auto_interest_payment( data->account_type ) )
    title = gnc_recn_make_interest_window_name( data->account,
                                                _("Interest Payment") );
  else
    title = gnc_recn_make_interest_window_name( data->account,
                                                _("Interest Charge") );

  gnc_xfer_dialog_set_title( data->xferData, title );
  g_free( title );


  /* change frame labels */
  gnc_xfer_dialog_set_information_label( data->xferData,
					 _("Payment Information") );

  /* Interest accrued is a transaction from an income account
   * to a bank account.  Interest charged is a transaction from
   * a credit account to an expense account.  The user isn't allowed
   * to change the account (bank or credit) being reconciled.
   */
  if( account_type_has_auto_interest_payment( data->account_type ) )
  {
    gnc_xfer_dialog_set_from_account_label( data->xferData,
		    			    _("Payment From") );
    gnc_xfer_dialog_set_from_show_button_active( data->xferData, TRUE );

    gnc_xfer_dialog_set_to_account_label( data->xferData,
		    			  _("Reconcile Account") );
    gnc_xfer_dialog_select_to_account( data->xferData, data->account );
    gnc_xfer_dialog_lock_to_account_tree( data->xferData );

    /* Quickfill based on the reconcile account, which is the "To" acct. */
    gnc_xfer_dialog_quickfill_to_account( data->xferData, TRUE );
  }
  else  /* interest charged to account rather than paid to it */
  {
    gnc_xfer_dialog_set_from_account_label( data->xferData,
		    			    _("Reconcile Account") );
    gnc_xfer_dialog_select_from_account( data->xferData, data->account );
    gnc_xfer_dialog_lock_from_account_tree( data->xferData );

    gnc_xfer_dialog_set_to_account_label( data->xferData,
		    			  _("Payment To") );
    gnc_xfer_dialog_set_to_show_button_active( data->xferData, TRUE );

    /* Quickfill based on the reconcile account, which is the "From" acct. */
    gnc_xfer_dialog_quickfill_to_account( data->xferData, FALSE );
  }


  /* add a button to disable auto interest payments for this account */
  gnc_xfer_dialog_add_user_specified_button( data->xferData,
    ( account_type_has_auto_interest_payment( data->account_type ) ?
        _("No Auto Interest Payments for this Account")
       : _("No Auto Interest Charges for this Account") ),
    GTK_SIGNAL_FUNC(gnc_recn_interest_xfer_no_auto_clicked_cb),
    (gpointer) data );

  /* no currency frame */
  gnc_xfer_dialog_toggle_currency_table( data->xferData, FALSE );

  /* set the reconcile date for the transaction date */
  gnc_xfer_dialog_set_date( data->xferData, data->date );

  /* Now run the transfer dialog.  This blocks until done.
   * If the user hit Cancel, make the button clickable so that
   * the user can retry if they want.  We don't make the button
   * clickable if they successfully entered a transaction, since
   * the fact that the button was clickable again might make
   * the user think that the transaction didn't actually go through.
   */
  if( ! gnc_xfer_dialog_run_until_done( data->xferData ) )
    if( data->xfer_button )
      gtk_widget_set_sensitive(GTK_WIDGET(data->xfer_button), TRUE);

  /* done with the XferDialog */
  data->xferData = NULL;
}

/* Set up for the interest xfer window, run the window, and update
 * the startRecnWindow if the interest xfer changed anything that matters.
 */
static void
gnc_reconcile_interest_xfer_run(startRecnWindowData *data)
{
  GtkWidget *entry = gnc_amount_edit_gtk_entry(
                           GNC_AMOUNT_EDIT(data->end_value) );
  gnc_numeric before = gnc_amount_edit_get_amount(
                           GNC_AMOUNT_EDIT(data->end_value) );
  gnc_numeric after = gnc_numeric_zero();

  recnInterestXferWindow( data );

  /* recompute the ending balance */
  after = xaccAccountGetBalanceAsOfDate(data->account, data->date);

  /* update the ending balance in the startRecnWindow if it has changed. */
  if( gnc_numeric_compare( before, after ) )
  {
    if (gnc_reverse_balance(data->account))
      after = gnc_numeric_neg (after);

    gnc_amount_edit_set_amount (GNC_AMOUNT_EDIT (data->end_value), after);
    gtk_widget_grab_focus(GTK_WIDGET(entry));
    gtk_entry_select_region (GTK_ENTRY(entry), 0, -1);
    data->original_value = after;
    data->user_set_value = FALSE;
  }
}

void
gnc_start_recn_interest_clicked_cb(GtkButton *button, startRecnWindowData *data)
{
  /* indicate in account that user wants
   * an auto interest xfer for this account */
  xaccAccountSetAutoInterestXfer( data->account, TRUE );

  /* make the button unclickable since we're popping up the window */
  if( data->xfer_button )
    gtk_widget_set_sensitive(GTK_WIDGET(data->xfer_button), FALSE);

  /* run the account window */
  gnc_reconcile_interest_xfer_run( data );
}

static void
gnc_save_reconcile_interval(Account *account, time_t statement_date)
{
  time_t prev_statement_date;
  int days = 0, months = 0;
  double seconds;

  if (!xaccAccountGetReconcileLastDate (account, &prev_statement_date))
    return;

  /*
   * Compute the number of days difference.
   */
  seconds = difftime(statement_date, prev_statement_date);
  days = (int)(seconds / 60 / 60 / 24);

  /*
   * See if we need to remember days(weeks) or months.  The only trick
   * value is 28 days which could be wither 4 weeks or 1 month.
   */
  if (days == 28) {
    int prev_days = 0, prev_months = 1;

    /* What was it last time? */
    xaccAccountGetReconcileLastInterval (account, &prev_months, &prev_days);
    if (prev_months == 1) {
      months = 1;
      days = 0;
    }
  } else if (days > 28) {
    struct tm current, prev;

    current = * localtime(&statement_date);
    prev = * localtime(&prev_statement_date);
    months = ((12 * current.tm_year + current.tm_mon) -
	      (12 * prev.tm_year + prev.tm_mon));
    days = 0;
  }
  
  /*
   * Remember for next time.
   */
  xaccAccountSetReconcileLastInterval(account, months, days);
}

/********************************************************************\
 * startRecnWindow                                                  *
 *   opens up the window to prompt the user to enter the ending     *
 *   balance from bank statement                                    *
 *                                                                  *
 * NOTE: This function does not return until the user presses "Ok"  *
 *       or "Cancel"                                                *
 *                                                                  *
 * Args:   parent         - the parent of this window               *
 *         account        - the account to reconcile                *
 *         new_ending     - returns the amount for ending balance   *
 *         statement_date - returns date of the statement :)        *
 * Return: True, if the user presses "Ok", else False               *
\********************************************************************/
static gboolean
startRecnWindow(GtkWidget *parent, Account *account,
                gnc_numeric *new_ending, time_t *statement_date,
		gboolean enable_subaccount)
{
  GtkWidget *dialog, *end_value, *date_value, *include_children_button;
  GladeXML *xml;
  startRecnWindowData data = { NULL };
  gboolean auto_interest_xfer_option;
  GNCPrintAmountInfo print_info;
  gnc_numeric ending;
  char *title;
  int result;

  /* Initialize the data structure that will be used for several callbacks
   * throughout this file with the relevant info.  Some initialization is
   * done below as well.  Note that local storage should be OK for this,
   * since any callbacks using it will only work while the startRecnWindow
   * is running.
   */
  data.account = account;
  data.account_type = xaccAccountGetType(account);
  data.date = *statement_date;

  /* whether to have an automatic interest xfer dialog or not */
  auto_interest_xfer_option =
     gnc_recn_interest_xfer_get_auto_interest_xfer_allowed( account );

  data.include_children = xaccAccountGetReconcileChildrenStatus(account);

  ending = gnc_ui_account_get_reconciled_balance(account,
                                                 data.include_children);
  print_info = gnc_account_print_info (account, TRUE);

  /*
   * Do not reverse the balance here.  It messes up the math in the
   * reconciliation window.  Also, the balance should show up as a
   * positive number in the reconciliation window to match the positive
   * number that shows in the register window.
   */

  /* Create the dialog box */
  xml = gnc_glade_xml_new ("reconcile.glade", "Reconcile Start Dialog");
  dialog = glade_xml_get_widget (xml, "Reconcile Start Dialog");
  title = gnc_recn_make_window_name (account);
  gtk_window_set_title(GTK_WINDOW(dialog), title);
  g_free (title);

  data.startRecnWindow = GTK_WIDGET(dialog);

  if (parent != NULL)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));

  {
    GtkWidget *start_value, *box;
    GtkWidget *entry;
    GtkWidget *interest = NULL;

    start_value = glade_xml_get_widget(xml, "start_value");
    gtk_label_set_text(GTK_LABEL(start_value), xaccPrintAmount (ending, print_info));

    include_children_button = glade_xml_get_widget(xml, "subaccount_check");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(include_children_button),
                                 data.include_children);
    gtk_widget_set_sensitive(include_children_button, enable_subaccount);

    date_value = gnc_date_edit_new(*statement_date, FALSE, FALSE);
    data.date_value = date_value;
    box = glade_xml_get_widget(xml, "date_value_box");
    gtk_box_pack_start(GTK_BOX(box), date_value, TRUE, TRUE, 0);

    end_value = gnc_amount_edit_new ();
    data.end_value = GNC_AMOUNT_EDIT(end_value);
    data.original_value = *new_ending;
    data.user_set_value = FALSE;
    box = glade_xml_get_widget(xml, "ending_value_box");
    gtk_box_pack_start(GTK_BOX(box), end_value, TRUE, TRUE, 0);

    /* need to get a callback on date changes to update the recn balance */
    g_signal_connect ( G_OBJECT (date_value), "date_changed",
          G_CALLBACK (gnc_start_recn_date_changed), (gpointer) &data );
    gnc_date_editable_enters(GNC_DATE_EDIT(date_value), TRUE);

    print_info.use_symbol = 0;
    gnc_amount_edit_set_print_info (GNC_AMOUNT_EDIT (end_value), print_info);
    gnc_amount_edit_set_fraction (GNC_AMOUNT_EDIT (end_value),
                                  xaccAccountGetCommoditySCU (account));

    gnc_amount_edit_set_amount (GNC_AMOUNT_EDIT (end_value), *new_ending);

    entry = gnc_amount_edit_gtk_entry (GNC_AMOUNT_EDIT (end_value));
    gtk_entry_select_region (GTK_ENTRY(entry), 0, -1);
    g_signal_connect(G_OBJECT(entry), "focus-out-event",
		     G_CALLBACK(gnc_start_recn_update_cb), (gpointer) &data);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

    /* if it's possible to enter an interest payment or charge for this
     * account, add a button so that the user can pop up the appropriate
     * dialog if it isn't automatically popping up.
     */
    interest = glade_xml_get_widget(xml, "interest_button");
    if( account_type_has_auto_interest_payment( data.account_type ) )
      gtk_button_set_label(GTK_BUTTON(interest), _("Enter Interest Payment...") );
    else if( account_type_has_auto_interest_charge( data.account_type ) )
      gtk_button_set_label(GTK_BUTTON(interest), _("Enter Interest Charge...") );
    else {
      gtk_widget_hide(interest);
      interest = NULL;
    }

    if( interest )
    {
      data.xfer_button = interest;
      if( auto_interest_xfer_option )
       gtk_widget_set_sensitive(GTK_WIDGET(interest), FALSE);
    }

    glade_xml_signal_autoconnect_full(xml, gnc_glade_autoconnect_full_func,
				      &data);

    gtk_widget_show_all(dialog);

    gtk_widget_grab_focus(gnc_amount_edit_gtk_entry
                          (GNC_AMOUNT_EDIT (end_value)));
  }

  /* Allow the user to enter an interest payment
   * or charge prior to reconciling */
  if( account_type_has_auto_interest_xfer( data.account_type ) 
      && auto_interest_xfer_option )
  {
    gnc_reconcile_interest_xfer_run( &data );
  }

  result = gtk_dialog_run(GTK_DIALOG(dialog));
  if (result == GTK_RESPONSE_OK)
  {
    *new_ending = gnc_amount_edit_get_amount (GNC_AMOUNT_EDIT (end_value));
    *statement_date = gnc_date_edit_get_date(GNC_DATE_EDIT(date_value));

    if (gnc_reverse_balance(account))
      *new_ending = gnc_numeric_neg (*new_ending);

    xaccAccountSetReconcileChildrenStatus(account, data.include_children);

    gnc_save_reconcile_interval(account, *statement_date);
  }
  gtk_widget_destroy (dialog);

  return (result == GTK_RESPONSE_OK);
}


static void
gnc_reconcile_window_set_sensitivity(RecnWindow *recnData)
{
  gboolean sensitive = FALSE;
  GNCReconcileList *list;

  list = GNC_RECONCILE_LIST(recnData->debit);
  if (gnc_reconcile_list_get_current_split(list) != NULL)
    sensitive = TRUE;

  list = GNC_RECONCILE_LIST(recnData->credit);
  if (gnc_reconcile_list_get_current_split(list) != NULL)
    sensitive = TRUE;

  gtk_widget_set_sensitive(recnData->edit_item, sensitive);
  gtk_widget_set_sensitive(recnData->delete_item, sensitive);

  gtk_widget_set_sensitive(recnData->edit_popup, sensitive);
  gtk_widget_set_sensitive(recnData->delete_popup, sensitive);

  gtk_widget_set_sensitive(recnData->edit_button, sensitive);
  gtk_widget_set_sensitive(recnData->delete_button, sensitive);
}

static void
gnc_reconcile_window_list_cb(GNCReconcileList *list, Split *split,
                             gpointer data)
{
  RecnWindow *recnData = data;

  gnc_reconcile_window_set_sensitivity(recnData);
  recnRecalculateBalance(recnData);
}

static GNCSplitReg *
gnc_reconcile_window_open_register(RecnWindow *recnData)
{
  Account *account = recn_get_account (recnData);
  GncPluginPage *page;
  GNCSplitReg *gsr;
  gboolean include_children;

  if (!account)
    return(NULL);

  include_children = xaccAccountGetReconcileChildrenStatus (account);
  page = gnc_plugin_page_register_new (account, include_children);
  gnc_main_window_open_page (NULL, page);
  gsr = gnc_plugin_page_register_get_gsr(page);
  return gsr;
}

static void
gnc_reconcile_window_double_click_cb(GNCReconcileList *list, Split *split,
                                     gpointer data)
{
  RecnWindow *recnData = data;
  GNCSplitReg *gsr;

  /* This should never be true, but be paranoid */
  if (split == NULL)
    return;

  gsr = gnc_reconcile_window_open_register(recnData);
  if (gsr == NULL)
    return;
  gnc_split_reg_jump_to_split( gsr, split );
}

static void
gnc_reconcile_window_focus_cb(GtkWidget *widget, GdkEventFocus *event,
                              gpointer data)
{
  RecnWindow *recnData = data;
  GNCReconcileList *this_list, *other_list;
  GNCReconcileList *debit, *credit;

  this_list = GNC_RECONCILE_LIST(widget);

  debit  = GNC_RECONCILE_LIST(recnData->debit);
  credit = GNC_RECONCILE_LIST(recnData->credit);

  other_list = GNC_RECONCILE_LIST(this_list == debit ? credit : debit);

  /* clear the *other* list so we always have no more than one selection */
  gnc_reconcile_list_unselect_all(other_list);
}

static gboolean
gnc_reconcile_key_press_cb (GtkWidget *widget, GdkEventKey *event,
                            gpointer data)
{
  RecnWindow *recnData = data;
  GtkWidget *this_list, *other_list;
  GtkWidget *debit, *credit;

  switch (event->keyval)
  {
    case GDK_Tab:
    case GDK_ISO_Left_Tab:
      break;

    default:
      return FALSE;
  }

  gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "key_press_event");

  this_list = widget;

  debit  = recnData->debit;
  credit = recnData->credit;

  other_list = (this_list == debit ? credit : debit);

  gtk_widget_grab_focus (other_list);

  return TRUE;
}

static void
gnc_reconcile_window_set_titles(RecnWindow *recnData)
{
  gboolean formal;
  gchar *title;

  formal = gnc_gconf_get_bool(GCONF_GENERAL, KEY_ACCOUNTING_LABELS, NULL);

  if (formal)
    title = _("Debits");
  else
    title = gnc_get_debit_string(NO_TYPE);

  gtk_frame_set_label(GTK_FRAME(recnData->debit_frame), title);

  if (!formal)
    g_free(title);

  if (formal)
    title = _("Credits");
  else
    title = gnc_get_credit_string(NO_TYPE);

  gtk_frame_set_label(GTK_FRAME(recnData->credit_frame), title);

  if (!formal)
    g_free(title);
}

static GtkWidget *
gnc_reconcile_window_create_list_box(Account *account,
                                     GNCReconcileListType type,
                                     RecnWindow *recnData,
                                     GtkWidget **list_save,
                                     GtkWidget **total_save)
{
  GtkWidget *frame, *scrollWin, *list, *vbox, *label, *hbox;

  frame = gtk_frame_new(NULL);

  if (type == RECLIST_DEBIT)
    recnData->debit_frame = frame;
  else
    recnData->credit_frame = frame;

  vbox = gtk_vbox_new(FALSE, 5);

  list = gnc_reconcile_list_new(account, type);
  *list_save = list;

  gtk_signal_connect(GTK_OBJECT(list), "toggle_reconciled",
                     GTK_SIGNAL_FUNC(gnc_reconcile_window_list_cb),
                     recnData);
  gtk_signal_connect(GTK_OBJECT(list), "double_click_split",
                     GTK_SIGNAL_FUNC(gnc_reconcile_window_double_click_cb),
                     recnData);
  gtk_signal_connect(GTK_OBJECT(list), "focus_in_event",
                     GTK_SIGNAL_FUNC(gnc_reconcile_window_focus_cb),
                     recnData);
  gtk_signal_connect(GTK_OBJECT(list), "key_press_event",
                     GTK_SIGNAL_FUNC(gnc_reconcile_key_press_cb),
                     recnData);

  scrollWin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrollWin),
				 GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_ALWAYS);
  gtk_container_set_border_width(GTK_CONTAINER(scrollWin), 5);

  gtk_container_add(GTK_CONTAINER(frame), scrollWin);
  gtk_container_add(GTK_CONTAINER(scrollWin), list);
  gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new(_("Total:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

  label = gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  *total_save = label;

  return vbox;
}


static GtkWidget *
gnc_recn_create_status_bar(RecnWindow *recnData)
{
  GtkWidget *statusbar;

  statusbar = gnome_appbar_new(FALSE, /* no progress bar */
			       TRUE,  /* has status area */
			       GNOME_PREFERENCES_USER);

  return statusbar;
}


static Split *
gnc_reconcile_window_get_current_split(RecnWindow *recnData)
{
  GNCReconcileList *list;
  Split *split;

  list = GNC_RECONCILE_LIST(recnData->debit);
  split = gnc_reconcile_list_get_current_split(list);
  if (split != NULL)
    return split;

  list = GNC_RECONCILE_LIST(recnData->credit);
  split = gnc_reconcile_list_get_current_split(list);

  return split;
}

static void
gnc_ui_reconcile_window_help_cb(GtkWidget *widget, gpointer data)
{
  gnc_gnome_help(HF_USAGE, HL_RECNWIN);
}

static void
gnc_ui_reconcile_window_change_cb(GtkButton *button, gpointer data)
{
  RecnWindow *recnData = data;
  Account *account = recn_get_account (recnData);
  gnc_numeric new_ending = recnData->new_ending;
  time_t statement_date = recnData->statement_date;

  if (gnc_reverse_balance (account))
    new_ending = gnc_numeric_neg (new_ending);
  if (startRecnWindow (recnData->window, account, &new_ending, &statement_date,
		       FALSE))
  {
    recnData->new_ending = new_ending;
    recnData->statement_date = statement_date;
    recnRecalculateBalance (recnData);
  }
}

static void
gnc_ui_reconcile_window_new_cb(GtkButton *button, gpointer data)
{
  RecnWindow *recnData = data;
  GNCSplitReg *gsr;

  gsr = gnc_reconcile_window_open_register(recnData);
  if (gsr == NULL)
    return;
  gnc_split_reg_jump_to_blank( gsr );
}

static void
gnc_ui_reconcile_window_delete_cb(GtkButton *button, gpointer data)
{
  RecnWindow *recnData = data;
  Transaction *trans;
  Split *split;

  split = gnc_reconcile_window_get_current_split(recnData);
  /* This should never be true, but be paranoid */
  if (split == NULL)
    return;

  {
    const char *message = _("Are you sure you want to delete the selected "
                            "transaction?");
    gboolean result;

    result = gnc_verify_dialog(recnData->window, FALSE, message);

    if (!result)
      return;
  }

  gnc_suspend_gui_refresh ();

  trans = xaccSplitGetParent(split);

  xaccTransBeginEdit(trans);
  xaccTransDestroy(trans);
  xaccTransCommitEdit(trans);

  gnc_resume_gui_refresh ();
}

static void
gnc_ui_reconcile_window_edit_cb(GtkButton *button, gpointer data)
{
  RecnWindow *recnData = data;
  GNCSplitReg *gsr;
  Split *split;

  split = gnc_reconcile_window_get_current_split (recnData);
  /* This should never be true, but be paranoid */
  if (split == NULL)
    return;

  gsr = gnc_reconcile_window_open_register(recnData);
  if (gsr == NULL)
    return;
  gnc_split_reg_jump_to_split_amount( gsr, split );
}


static char *
gnc_recn_make_window_name(Account *account)
{
  char *fullname;
  char *title;

  fullname = xaccAccountGetFullName(account, gnc_get_account_separator());
  title = g_strconcat(fullname, " - ", _("Reconcile"), NULL);

  g_free(fullname);

  return title;
}

static void
gnc_recn_set_window_name(RecnWindow *recnData)
{
  char *title;

  title = gnc_recn_make_window_name (recn_get_account (recnData));

  gtk_window_set_title (GTK_WINDOW (recnData->window), title);

  g_free (title);
}

static void 
gnc_recn_edit_account_cb(GtkWidget * w, gpointer data)
{
  RecnWindow *recnData = data;
  Account *account = recn_get_account (recnData);

  if (account == NULL)
    return;

  gnc_ui_edit_account_window (account);
}

static void 
gnc_recn_xfer_cb(GtkWidget * w, gpointer data)
{
  RecnWindow *recnData = data;
  Account *account = recn_get_account (recnData);

  if (account == NULL)
    return;

  gnc_xfer_dialog (recnData->window, account);
}

static void
gnc_recn_scrub_cb(GtkWidget *widget, gpointer data)
{
  RecnWindow *recnData = data;
  Account *account = recn_get_account (recnData);

  if (account == NULL)
    return;

  gnc_suspend_gui_refresh ();

  xaccAccountTreeScrubOrphans (account);
  xaccAccountTreeScrubImbalance (account);

  gnc_resume_gui_refresh ();
}

static void
gnc_recn_open_cb(GtkWidget *widget, gpointer data)
{
  RecnWindow *recnData = data;

  gnc_reconcile_window_open_register(recnData);
}

static GtkWidget *
gnc_recn_create_menu_bar(RecnWindow *recnData, GtkWidget *statusbar)
{
  GtkWidget *menubar;
  GtkAccelGroup *accel_group;

  static GnomeUIInfo reconcile_menu[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("_Reconcile Information..."),
      N_("Change the reconcile information "
         "including statement date and ending balance."),
      gnc_ui_reconcile_window_change_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("_Finish"),
      N_("Finish the reconciliation of this account"),
      recnFinishCB, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      'f', GDK_CONTROL_MASK, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Postpone"),
      N_("Postpone the reconciliation of this account"),
      recnPostponeCB, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      'p', GDK_CONTROL_MASK, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Cancel"),
      N_("Cancel the reconciliation of this account"),
      recnCancelCB, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  static GnomeUIInfo account_menu[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("_Open Account"), N_("Open the account"),
      gnc_recn_open_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Edit Account"), N_("Edit the main account for this register"),
      gnc_recn_edit_account_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("_Transfer..."), N_("Transfer funds from one account to another"),
      gnc_recn_xfer_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("_Check & Repair"),
      N_("Check for and repair unbalanced transactions and orphan splits "
	 "in this account"),
      gnc_recn_scrub_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  static GnomeUIInfo transaction_menu[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("_New"), N_("Add a new transaction to the account"),
      gnc_ui_reconcile_window_new_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      'n', GDK_CONTROL_MASK, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Edit"), N_("Edit the current transaction"),
      gnc_ui_reconcile_window_edit_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      'e', GDK_CONTROL_MASK, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Delete"), N_("Delete the selected transaction"),
      gnc_ui_reconcile_window_delete_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      'd', GDK_CONTROL_MASK, NULL
    },
    GNOMEUIINFO_END
  };

  static GnomeUIInfo help_menu[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("_Help"), N_("Open the GnuCash help window"),
      gnc_ui_reconcile_window_help_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  static GnomeUIInfo reconcile_window_menu[] =
  {
    GNOMEUIINFO_SUBTREE(N_("_Reconcile"), reconcile_menu),
    GNOMEUIINFO_SUBTREE(N_("_Account"), account_menu),
    GNOMEUIINFO_SUBTREE(N_("_Transaction"), transaction_menu),
    GNOMEUIINFO_MENU_HELP_TREE(help_menu),
    GNOMEUIINFO_END
  };

  gnc_fill_menu_with_data(reconcile_window_menu, recnData);

  menubar = gtk_menu_bar_new();

  accel_group = gtk_accel_group_new();
  /* GNOME 2 Port (replace this accel_group stuff) */
  /*gtk_accel_group_attach(accel_group, GTK_OBJECT(recnData->window));*/

  gnome_app_fill_menu(GTK_MENU_SHELL(menubar), reconcile_window_menu,
  		      accel_group, TRUE, 0);

  gnome_app_install_appbar_menu_hints(GNOME_APPBAR(statusbar),
                                      reconcile_window_menu);

  recnData->edit_item = transaction_menu[1].widget;
  recnData->delete_item = transaction_menu[2].widget;

  return menubar;
}


static GtkWidget *
gnc_recn_create_popup_menu(RecnWindow *recnData)
{
  GtkWidget *popup;

  GnomeUIInfo transaction_menu[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("_New"), N_("Add a new transaction to the account"),
      gnc_ui_reconcile_window_new_cb, recnData, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_NEW,
      'n', GDK_CONTROL_MASK, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Edit"), N_("Edit the current transaction"),
      gnc_ui_reconcile_window_edit_cb, recnData, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_PROPERTIES,
      'e', GDK_CONTROL_MASK, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Delete"), N_("Delete the selected transaction"),
      gnc_ui_reconcile_window_delete_cb, recnData, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_TRASH,
      'd', GDK_CONTROL_MASK, NULL
    },
    GNOMEUIINFO_END
  };

  popup = gnome_popup_menu_new(transaction_menu);

  recnData->edit_popup = transaction_menu[1].widget;
  recnData->delete_popup = transaction_menu[2].widget;

  return popup;
}


static void
gnc_recn_refresh_toolbar(RecnWindow *recnData)
{
  GtkToolbarStyle tbstyle;

  if ((recnData == NULL) || (recnData->toolbar == NULL))
    return;

  tbstyle = gnc_get_toolbar_style();

  gtk_toolbar_set_style(GTK_TOOLBAR(recnData->toolbar), tbstyle);
}

static void
gnc_toolbar_change_cb (GConfClient *client,
		       guint cnxn_id,
		       GConfEntry *entry,
		       gpointer data)
{
  RecnWindow *recnData = data;
  GConfValue *value;
  const gchar *key, *key_tail;

  key = gconf_entry_get_key(entry);
  value = gconf_entry_get_value(entry);
  if (!key || !value)
    return;

  key_tail = rindex(key, '/');
  if (key_tail != NULL)
    key_tail++;
  if (strcmp(key_tail, KEY_TOOLBAR_STYLE) == 0) {
    gnc_recn_refresh_toolbar(recnData);
  }
}

static GtkWidget *
gnc_recn_create_tool_bar(RecnWindow *recnData)
{
  GtkWidget *toolbar;
  GnomeUIInfo toolbar_info[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("New"), N_("Add a new transaction to the account"),
      gnc_ui_reconcile_window_new_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_NEW,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Edit"), N_("Edit the current transaction"),
      gnc_ui_reconcile_window_edit_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_PROPERTIES,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Delete"), N_("Delete the selected transaction"),
      gnc_ui_reconcile_window_delete_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_TRASH,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("Open"), N_("Open the account"),
      gnc_recn_open_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_JUMP_TO,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("Finish"), N_("Finish the reconciliation of this account"),
      recnFinishCB, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_DOWN,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  toolbar = gtk_toolbar_new ();

  gnome_app_fill_toolbar_with_data(GTK_TOOLBAR(toolbar), toolbar_info,
                                   NULL, recnData);

  recnData->toolbar = toolbar;

  recnData->edit_button = toolbar_info[1].widget;
  recnData->delete_button = toolbar_info[2].widget;
  recnData->finish_button = toolbar_info[6].widget;

  return toolbar;
}

static void
gnc_get_reconcile_info (Account *account,
                        gnc_numeric *new_ending,
                        time_t *statement_date)
{
  struct tm tm;

  if (xaccAccountGetReconcileLastDate (account, statement_date))
  {
    int months = 1, days = 0;

    tm = * localtime (statement_date);

    /* How far should the date be moved?  Args unchanged on failure. */
    xaccAccountGetReconcileLastInterval (account, &months, &days);

    if (months) {
      /*
       * Add in the months and normalize
       */
      date_add_months(&tm, months, TRUE);
    } else {
      /*
       * Add in the days (weeks if multiple of seven).
       */
      tm.tm_mday += days;
    }
    tm.tm_isdst = -1;
    *statement_date = mktime (&tm);
  }

  xaccAccountGetReconcilePostponeDate (account, statement_date);

  if( !xaccAccountGetReconcilePostponeBalance (account, new_ending) )
  {
    /* if the account wasn't previously postponed, try to predict
     * the statement balance based on the statement date.
     */
    *new_ending =
      gnc_ui_account_get_balance_as_of_date
      (account, *statement_date, 
       xaccAccountGetReconcileChildrenStatus(account));
  }
}

static gboolean
find_by_account (gpointer find_data, gpointer user_data)
{
  Account *account = find_data;
  RecnWindow *recnData = user_data;

  if (!recnData)
    return FALSE;

  return guid_equal (&recnData->account, xaccAccountGetGUID (account));
}

static void
recn_set_watches_one_account (gpointer data, gpointer user_data)
{
  Account *account = (Account *)data;
  RecnWindow *recnData = (RecnWindow *)user_data;
  GList *node;

  for (node = xaccAccountGetSplitList (account); node; node = node->next)
  {
    Split *split = node->data;
    Transaction *trans;
    char recn;

    recn = xaccSplitGetReconcile (split);
    switch (recn)
    {
      case NREC:
      case CREC:
        trans = xaccSplitGetParent (split);
        
        gnc_gui_component_watch_entity (recnData->component_id,
                                        xaccTransGetGUID (trans),
                                        GNC_EVENT_MODIFY | GNC_EVENT_DESTROY);
        break;

      default:
        break;
    }
  }
}

static void
recn_set_watches (RecnWindow *recnData)
{
  gboolean include_children;
  Account *account;
  GList *accounts = NULL;

  gnc_gui_component_clear_watches (recnData->component_id);

  gnc_gui_component_watch_entity (recnData->component_id,
                                  &recnData->account,
                                  GNC_EVENT_MODIFY | GNC_EVENT_DESTROY);

  account = recn_get_account (recnData);

  include_children = xaccAccountGetReconcileChildrenStatus(account);
  if (include_children)
    accounts = xaccAccountGetDescendants(account);

  /* match the account */
  accounts = g_list_prepend (accounts, account);

  g_list_foreach(accounts, recn_set_watches_one_account, recnData);

  g_list_free (accounts);
}

static void
refresh_handler (GHashTable *changes, gpointer user_data)
{
  RecnWindow *recnData = user_data;
  const EventInfo *info;
  Account *account;

  account = recn_get_account (recnData);
  if (!account)
  {
    gnc_close_gui_component_by_data (WINDOW_RECONCILE_CM_CLASS, recnData);
    return;
  }

  if (changes)
  {
    info = gnc_gui_get_entity_events (changes, &recnData->account);
    if (info && (info->event_mask & GNC_EVENT_DESTROY))
    {
      gnc_close_gui_component_by_data (WINDOW_RECONCILE_CM_CLASS, recnData);
      return;
    }
  }

  gnc_reconcile_window_set_titles(recnData);
  recn_set_watches (recnData);

  recnRefresh (recnData);
}

static void
close_handler (gpointer user_data)
{
  RecnWindow *recnData = user_data;

  gtk_widget_destroy (recnData->window);
}

/********************************************************************\
 * recnWindow                                                       *
 *   opens up the window to reconcile an account                    *
 *                                                                  *
 * Args:   parent  - the parent of this window                      *
 *         account - the account to reconcile                       *
 * Return: recnData - the instance of this RecnWindow               *
\********************************************************************/
RecnWindow *
recnWindow (GtkWidget *parent, Account *account)
{
  gnc_numeric new_ending;
  time_t statement_date;

  if (account == NULL)
    return NULL;

  /* The last time reconciliation was attempted during the current
   * execution of gnucash, the date was stored. Use that date if
   * possible. This helps with balancing multiple accounts for which
   * statements are issued at the same time, like multiple bank
   * accounts on a single statement. */
  if (!gnc_reconcile_last_statement_date)
     statement_date = time (NULL);
  else
     statement_date = gnc_reconcile_last_statement_date;

  gnc_get_reconcile_info (account, &new_ending, &statement_date);

  /* Popup a little window to prompt the user to enter the
   * ending balance for his/her bank statement */
  if (!startRecnWindow (parent, account, &new_ending, &statement_date, TRUE))
    return NULL;

  return recnWindowWithBalance (parent, account, new_ending, statement_date);
}

/********************************************************************\
 * recnWindowWithBalance                                            
 *
 *   Opens up the window to reconcile an account, but with ending
 *   balance and statement date already given.
 *                                                                  
 * Args:   parent         - The parent widget of the new window
 *         account        - The account to reconcile           
 *         new_ending     - The amount for ending balance      
 *         statement_date - The date of the statement          
 * Return: recnData - the instance of this RecnWindow          
\********************************************************************/
RecnWindow *
recnWindowWithBalance (GtkWidget *parent, Account *account, 
		       gnc_numeric new_ending, time_t statement_date)
{
  RecnWindow *recnData;
  GtkWidget *statusbar;
  GtkWidget *vbox;
  GtkWidget *dock;

  if (account == NULL)
    return NULL;

  recnData = gnc_find_first_gui_component (WINDOW_RECONCILE_CM_CLASS,
                                           find_by_account, account);
  if (recnData)
    return recnData;

  recnData = g_new0 (RecnWindow, 1);

  recnData->account = *xaccAccountGetGUID (account);


  recnData->component_id =
    gnc_register_gui_component (WINDOW_RECONCILE_CM_CLASS,
                                refresh_handler, close_handler,
                                recnData);

  recn_set_watches (recnData);

  gnc_reconcile_last_statement_date = statement_date;

  recnData->new_ending = new_ending;
  recnData->statement_date = statement_date;
  recnData->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  recnData->delete_refresh = FALSE;

  gnome_window_icon_set_from_default (GTK_WINDOW (recnData->window));
  gnc_recn_set_window_name(recnData);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(recnData->window), vbox);

  dock = bonobo_dock_new();
  gtk_box_pack_start(GTK_BOX(vbox), dock, TRUE, TRUE, 0);

  statusbar = gnc_recn_create_status_bar(recnData);
  gtk_box_pack_start(GTK_BOX(vbox), statusbar, FALSE, FALSE, 0);

  gtk_signal_connect (GTK_OBJECT (recnData->window), "destroy",
                      GTK_SIGNAL_FUNC(recn_destroy_cb), recnData);

  /* The menu bar */
  {
    BonoboDockItemBehavior behavior;
    GtkWidget *dock_item;
    GtkWidget *menubar;

    behavior = BONOBO_DOCK_ITEM_BEH_EXCLUSIVE;
    if (!gnc_gconf_menubar_detachable())
      behavior |= BONOBO_DOCK_ITEM_BEH_LOCKED;

    dock_item = bonobo_dock_item_new("menu", behavior);

    menubar = gnc_recn_create_menu_bar(recnData, statusbar);
    gtk_container_set_border_width(GTK_CONTAINER(menubar), 2);
    gtk_container_add(GTK_CONTAINER(dock_item), menubar);

    bonobo_dock_add_item (BONOBO_DOCK(dock), BONOBO_DOCK_ITEM(dock_item),
                         BONOBO_DOCK_TOP, 0, 0, 0, TRUE);
  }

  /* The tool bar */
  {
    BonoboDockItemBehavior behavior;
    GtkWidget *dock_item;
    GtkWidget *toolbar;

    behavior = BONOBO_DOCK_ITEM_BEH_EXCLUSIVE;
    if (!gnc_gconf_toolbar_detachable())
      behavior |= BONOBO_DOCK_ITEM_BEH_LOCKED;

    dock_item = bonobo_dock_item_new("toolbar", behavior);

    toolbar = gnc_recn_create_tool_bar(recnData);
    gtk_container_set_border_width(GTK_CONTAINER(toolbar), 2);
    gtk_container_add(GTK_CONTAINER(dock_item), toolbar);

    recnData->toolbar_change_cb_id =
      gnc_gconf_add_anon_notification(GCONF_GENERAL,
				      gnc_toolbar_change_cb, recnData);
    recnData->toolbar_change_cb_id2 =
      gnc_gconf_add_anon_notification(DESKTOP_GNOME_INTERFACE,
				      gnc_toolbar_change_cb, recnData);

    bonobo_dock_add_item (BONOBO_DOCK(dock), BONOBO_DOCK_ITEM(dock_item),
                          BONOBO_DOCK_TOP, 1, 0, 0, TRUE);
  }

  /* The main area */
  {
    GtkWidget *frame = gtk_frame_new(NULL);
    GtkWidget *main_area = gtk_vbox_new(FALSE, 10);
    GtkWidget *debcred_area = gtk_hbox_new(FALSE, 15);
    GtkWidget *debits_box;
    GtkWidget *credits_box;
    GtkWidget *popup;

    bonobo_dock_set_client_area(BONOBO_DOCK(dock), frame);

    /* Force a reasonable starting size */
    gtk_widget_set_usize(GTK_WIDGET(recnData->window), 800, 600);

    gtk_container_add(GTK_CONTAINER(frame), main_area);
    gtk_container_set_border_width(GTK_CONTAINER(main_area), 10);

    debits_box = gnc_reconcile_window_create_list_box
      (account, RECLIST_DEBIT, recnData,
       &recnData->debit, &recnData->total_debit);

    credits_box = gnc_reconcile_window_create_list_box
      (account, RECLIST_CREDIT, recnData,
       &recnData->credit, &recnData->total_credit);

    GNC_RECONCILE_LIST(recnData->debit)->sibling = GNC_RECONCILE_LIST(recnData->credit);
    GNC_RECONCILE_LIST(recnData->credit)->sibling = GNC_RECONCILE_LIST(recnData->debit);

    popup = gnc_recn_create_popup_menu(recnData);
    gnome_popup_menu_attach(popup, recnData->debit, recnData);
    gnome_popup_menu_attach(popup, recnData->credit, recnData);

    gtk_box_pack_start(GTK_BOX(main_area), debcred_area, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(debcred_area), debits_box, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(debcred_area), credits_box, TRUE, TRUE, 0);

    {
      GtkWidget *hbox, *title_vbox, *value_vbox;
      GtkWidget *totals_hbox, *frame, *title, *value;

      /* lower horizontal bar below reconcile lists */
      hbox = gtk_hbox_new(FALSE, 5);
      gtk_box_pack_start(GTK_BOX(main_area), hbox, FALSE, FALSE, 0);

      /* frame to hold totals */
      frame = gtk_frame_new(NULL);
      gtk_box_pack_end(GTK_BOX(hbox), frame, FALSE, FALSE, 0);

      /* hbox to hold title/value vboxes */
      totals_hbox = gtk_hbox_new(FALSE, 3);
      gtk_container_add(GTK_CONTAINER(frame), totals_hbox);
      gtk_container_set_border_width(GTK_CONTAINER(totals_hbox), 5);

      /* vbox to hold titles */
      title_vbox = gtk_vbox_new(FALSE, 3);
      gtk_box_pack_start(GTK_BOX(totals_hbox), title_vbox, FALSE, FALSE, 0);

      /* vbox to hold values */
      value_vbox = gtk_vbox_new(FALSE, 3);
      gtk_box_pack_start(GTK_BOX(totals_hbox), value_vbox, TRUE, TRUE, 0);

      /* starting balance title/value */
      title = gtk_label_new(_("Starting Balance:"));
      gtk_misc_set_alignment(GTK_MISC(title), 1.0, 0.5);
      gtk_box_pack_start(GTK_BOX(title_vbox), title, FALSE, FALSE, 3);

      value = gtk_label_new("");
      recnData->starting = value;
      gtk_misc_set_alignment(GTK_MISC(value), 1.0, 0.5);
      gtk_box_pack_start(GTK_BOX(value_vbox), value, FALSE, FALSE, 3);

      /* ending balance title/value */
      title = gtk_label_new(_("Ending Balance:"));
      gtk_misc_set_alignment(GTK_MISC(title), 1.0, 0.5);
      gtk_box_pack_start(GTK_BOX(title_vbox), title, FALSE, FALSE, 0);

      value = gtk_label_new("");
      recnData->ending = value;
      gtk_misc_set_alignment(GTK_MISC(value), 1.0, 0.5);
      gtk_box_pack_start(GTK_BOX(value_vbox), value, FALSE, FALSE, 0);

      /* reconciled balance title/value */
      title = gtk_label_new(_("Reconciled Balance:"));
      gtk_misc_set_alignment(GTK_MISC(title), 1.0, 0.5);
      gtk_box_pack_start(GTK_BOX(title_vbox), title, FALSE, FALSE, 0);

      value = gtk_label_new("");
      recnData->reconciled = value;
      gtk_misc_set_alignment(GTK_MISC(value), 1.0, 0.5);
      gtk_box_pack_start(GTK_BOX(value_vbox), value, FALSE, FALSE, 0);

      /* difference title/value */
      title = gtk_label_new(_("Difference:"));
      gtk_misc_set_alignment(GTK_MISC(title), 1.0, 0.5);
      gtk_box_pack_start(GTK_BOX(title_vbox), title, FALSE, FALSE, 0);

      value = gtk_label_new("");
      recnData->difference = value;
      gtk_misc_set_alignment(GTK_MISC(value), 1.0, 0.5);
      gtk_box_pack_start(GTK_BOX(value_vbox), value, FALSE, FALSE, 0);
    }

    /* Set up the data */
    recnRefresh (recnData);

    /* Clamp down on the size */
    {
      GNCReconcileList *rlist;
      gint height, num_debits, num_credits, num_show;

      num_credits = gnc_reconcile_list_get_num_splits
        (GNC_RECONCILE_LIST(recnData->credit));
      num_debits = gnc_reconcile_list_get_num_splits
        (GNC_RECONCILE_LIST(recnData->debit));

      num_show = MAX(num_debits, num_credits);
      num_show = MIN(num_show, 15);
      num_show = MAX(num_show, 8);

      gtk_widget_realize(recnData->credit);
      rlist = GNC_RECONCILE_LIST(recnData->credit);
      height = gnc_reconcile_list_get_needed_height(rlist, num_show);

      gtk_widget_set_usize(recnData->credit, 0, height);
      gtk_widget_set_usize(recnData->debit, 0, height);
    }
  }

  /* Allow resize */
  gtk_window_set_resizable(GTK_WINDOW(recnData->window), TRUE);

  gtk_widget_show_all(recnData->window);

  gnc_reconcile_window_set_titles(recnData);

  recnRecalculateBalance(recnData);

  gnc_recn_refresh_toolbar(recnData);

  gnc_window_adjust_for_screen(GTK_WINDOW(recnData->window));

  gtk_widget_grab_focus (recnData->debit);

  return recnData;
}


/********************************************************************\
 * gnc_ui_reconile_window_raise                                     *
 *   shows and raises an account editing window                     * 
 *                                                                  * 
 * Args:   editAccData - the edit window structure                  * 
\********************************************************************/
void
gnc_ui_reconcile_window_raise(RecnWindow * recnData)
{
  if (recnData == NULL)
    return;

  if (recnData->window == NULL)
    return;

  gtk_window_present(GTK_WINDOW(recnData->window));
}


/********************************************************************\
 * recn_destroy_cb                                                  *
 *   frees memory allocated for an recnWindow, and other cleanup    *
 *   stuff                                                          *
 *                                                                  *
 * Args:   w    - the widget that called us                         *
 *         data - the data struct for this window                   *
 * Return: none                                                     *
\********************************************************************/
static void 
recn_destroy_cb (GtkWidget *w, gpointer data)
{
  RecnWindow *recnData = data;

  gnc_unregister_gui_component_by_data (WINDOW_RECONCILE_CM_CLASS, recnData);

  gnc_gconf_remove_anon_notification(GCONF_GENERAL,
				     recnData->toolbar_change_cb_id);
  gnc_gconf_remove_anon_notification(DESKTOP_GNOME_INTERFACE,
				     recnData->toolbar_change_cb_id2);

  if (recnData->delete_refresh)
    gnc_resume_gui_refresh ();

  g_free (recnData);
}


/********************************************************************\
 * find_payment_account                                             *
 *   find an account that 'looks like' a payment account for the    *
 *   given account. This really only makes sense for credit card    *
 *   accounts.                                                      *
 *                                                                  *
 * Args:   account - the account to look in                         *
 * Return: a candidate payment account or NULL if none was found    *
\********************************************************************/
static Account *
find_payment_account(Account *account)
{
  GList *list;
  GList *node;

  if (account == NULL)
    return NULL;

  list = xaccAccountGetSplitList (account);

  /* Search backwards to find the latest payment */
  for (node = g_list_last (list); node; node = node->prev)
  {
    Transaction *trans;
    Split *split;
    GList *n;

    split = node->data;
    if (split == NULL)
      continue;

    /* ignore 'purchases' */
    if (!gnc_numeric_positive_p (xaccSplitGetAmount(split)))
      continue;

    trans = xaccSplitGetParent(split);
    if (trans == NULL)
      continue;

    for (n = xaccTransGetSplitList (trans); n; n = n->next)
    {
      GNCAccountType type;
      Account *a;
      Split *s;

      s = n->data;
      if ((s == NULL) || (s == split))
        continue;

      a = xaccSplitGetAccount(s);
      if ((a == NULL) || (a == account))
        continue;

      type = xaccAccountGetType(a);
      if ((type == BANK) || (type == CASH) || (type == ASSET))
        return a;
    }
  }

  return NULL;
}


/********************************************************************\
 * recnFinishCB                                                     *
 *   saves reconcile information                                    *
 *                                                                  *
 * Args:   w    - the widget that called us                         *
 *         data - the data struct for this window                   *
 * Return: none                                                     *
\********************************************************************/
static void 
recnFinishCB (GtkWidget *w, gpointer data)
{
  RecnWindow *recnData = data;
  gboolean auto_payment;
  Account *account;
  time_t date;

  if (!gnc_numeric_zero_p (recnRecalculateBalance(recnData)))
  {
    const char *message = _("The account is not balanced.\n"
                            "Are you sure you want to finish?");
    if (!gnc_verify_dialog (recnData->window, FALSE, message))
      return;
  }

  date = recnData->statement_date;

  gnc_suspend_gui_refresh ();

  recnData->delete_refresh = TRUE;

  gnc_reconcile_list_commit(GNC_RECONCILE_LIST(recnData->credit), date);
  gnc_reconcile_list_commit(GNC_RECONCILE_LIST(recnData->debit), date);

  auto_payment = gnc_gconf_get_bool(GCONF_RECONCILE_SECTION,
				    "auto_cc_payment", NULL);

  account = recn_get_account (recnData);

  xaccAccountClearReconcilePostpone (account);
  xaccAccountSetReconcileLastDate (account, date);

  if (auto_payment &&
      (xaccAccountGetType (account) == CREDIT) &&
      (gnc_numeric_negative_p (recnData->new_ending)))
  {
    Account *payment_account;
    XferDialog *xfer;

    xfer = gnc_xfer_dialog(NULL, account);

    gnc_xfer_dialog_set_amount(xfer, gnc_numeric_neg (recnData->new_ending));

    payment_account = find_payment_account (account);
    if (payment_account != NULL)
      gnc_xfer_dialog_select_from_account (xfer, payment_account);
  }

  gnc_close_gui_component_by_data (WINDOW_RECONCILE_CM_CLASS, recnData);
}

/********************************************************************\
 * recnPostponeCB                                                   *
 *   saves reconcile information for later use                      *
 *                                                                  *
 * Args:   w    - the widget that called us                         *
 *         data - the data struct for this window                   *
 * Return: none                                                     *
\********************************************************************/
static void 
recnPostponeCB (GtkWidget *w, gpointer data)
{
  RecnWindow *recnData = data;
  Account *account;

  {
    const char *message = _("Do you want to postpone this reconciliation "
                            "and finish it later?");
    if (!gnc_verify_dialog (recnData->window, FALSE, message))
      return;
  }

  gnc_suspend_gui_refresh ();

  recnData->delete_refresh = TRUE;

  gnc_reconcile_list_postpone (GNC_RECONCILE_LIST(recnData->credit));
  gnc_reconcile_list_postpone (GNC_RECONCILE_LIST(recnData->debit));

  account = recn_get_account (recnData);

  xaccAccountSetReconcilePostponeDate (account, recnData->statement_date);
  xaccAccountSetReconcilePostponeBalance (account, recnData->new_ending);

  gnc_close_gui_component_by_data (WINDOW_RECONCILE_CM_CLASS, recnData);
}

static void 
recnCancelCB (GtkWidget *w, gpointer data)
{
  RecnWindow *recnData = data;
  gboolean changed = FALSE;

  if (gnc_reconcile_list_changed(GNC_RECONCILE_LIST(recnData->credit)))
    changed = TRUE;
  if (gnc_reconcile_list_changed(GNC_RECONCILE_LIST(recnData->debit)))
    changed = TRUE;

  if (changed)
  {
    const char *message = _("You have made changes to this reconcile "
                            "window.\nAre you sure you want to cancel?");
    if (!gnc_verify_dialog(recnData->window, FALSE, message))
      return;
  }

  gnc_close_gui_component_by_data (WINDOW_RECONCILE_CM_CLASS, recnData);
}
