/********************************************************************\
 * dialog-find-transactions.c : locate transactions and show them   *
 * Copyright (C) 2000 Bill Gribble <grib@billgribble.com>           *
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

#define _GNU_SOURCE

#include "config.h"

#include <stdio.h>
#include <gnome.h>
#include <guile/gh.h>
#include <time.h>
#include <assert.h>

#include "messages.h"
#include "gnc-ui.h"
#include "RegWindow.h"
#include "window-register.h"
#include "account-tree.h"
#include "MultiLedger.h"
#include "FileDialog.h"
#include "splitreg.h"
#include "glade-cb-gnc-dialogs.h"
#include "dialog-find-transactions.h"
#include "window-help.h"
#include "Query.h"
#include "gnc-dateedit.h"

/********************************************************************\
 * gnc_ui_find_transactions_dialog_create
 * make a new print check dialog and wait for it.
\********************************************************************/

FindTransactionsDialog * 
gnc_ui_find_transactions_dialog_create(xaccLedgerDisplay * orig_ledg) {
  FindTransactionsDialog * ftd = g_new0(FindTransactionsDialog, 1);  

  /* call the glade-defined creator */
  ftd->dialog = create_Find_Transactions();
  ftd->ledger = orig_ledg;

  if(orig_ledg) {
    ftd->q = orig_ledg->query;
  }
  else {
    ftd->q = NULL;
  }
  
  /* initialize the radiobutton state vars */
  ftd->search_type = 0;

  /* find the important widgets */
  ftd->new_search_radiobutton = 
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "new_search_radiobutton");
  ftd->narrow_search_radiobutton = 
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "narrow_search_radiobutton");
  ftd->add_search_radiobutton = 
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "add_search_radiobutton");
  ftd->delete_search_radiobutton = 
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "delete_search_radiobutton");

  ftd->match_accounts_scroller =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "account_match_scroller");
  ftd->match_accounts_picker = 
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "account_match_picker");

  ftd->date_start_toggle =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "date_start_toggle");
  ftd->date_start_frame =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "date_start_frame");

  ftd->date_start_entry = gnc_date_edit_new(time(NULL), FALSE, FALSE);
  gtk_container_add(GTK_CONTAINER(ftd->date_start_frame), 
                    ftd->date_start_entry);
  gtk_widget_set_sensitive(ftd->date_start_entry, FALSE);

  ftd->date_end_toggle =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "date_end_toggle");
  ftd->date_end_frame =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "date_end_frame");

  ftd->date_end_entry = gnc_date_edit_new(time(NULL), FALSE, FALSE);
  gtk_container_add(GTK_CONTAINER(ftd->date_end_frame), 
                    ftd->date_end_entry);
  gtk_widget_set_sensitive(ftd->date_end_entry, FALSE);

  ftd->description_entry =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "description_entry");
  ftd->description_case_toggle =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "description_case_toggle");
  ftd->description_regexp_toggle =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "description_regexp_toggle");

  ftd->number_entry =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "number_entry");
  ftd->number_case_toggle =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "number_case_toggle");
  ftd->number_regexp_toggle =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "number_regexp_toggle");

  ftd->credit_debit_picker =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "credit_debit_picker");
  ftd->amount_comp_picker =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "amount_comp_picker");
  ftd->amount_entry =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "amount_entry");

  ftd->memo_entry =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "memo_entry");
  ftd->memo_case_toggle =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "memo_case_toggle");
  ftd->memo_regexp_toggle =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "memo_regexp_toggle");

  ftd->shares_comp_picker =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "shares_comp_picker");
  ftd->shares_entry =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "shares_entry");

  ftd->price_comp_picker =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "price_comp_picker");
  ftd->price_entry =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "price_entry");

  ftd->action_entry =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "action_entry");
  ftd->action_case_toggle =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "action_case_toggle");
  ftd->action_regexp_toggle =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "action_regexp_toggle");

  ftd->cleared_cleared_toggle = 
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "cleared_cleared_toggle");
  ftd->cleared_reconciled_toggle = 
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "cleared_reconciled_toggle");
  ftd->cleared_not_cleared_toggle = 
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "cleared_not_cleared_toggle");

  ftd->tag_entry =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "tag_entry");
  ftd->tag_case_toggle =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "tag_case_toggle");
  ftd->tag_regexp_toggle =
    gtk_object_get_data(GTK_OBJECT(ftd->dialog), "tag_regexp_toggle");

  /* add an account picker to the first tab */
  ftd->account_tree = gnc_account_tree_new();
  gtk_clist_column_titles_hide(GTK_CLIST(ftd->account_tree));
  gnc_account_tree_hide_all_but_name(GNC_ACCOUNT_TREE(ftd->account_tree));
  gnc_account_tree_refresh(GNC_ACCOUNT_TREE(ftd->account_tree));
    
  gtk_container_add(GTK_CONTAINER(ftd->match_accounts_scroller), 
                    ftd->account_tree);
  gtk_clist_set_selection_mode(GTK_CLIST(ftd->account_tree),
                               GTK_SELECTION_MULTIPLE);
  gtk_widget_set_usize(GTK_WIDGET(ftd->match_accounts_scroller),
                       300, 300);
  
  /* initialize optionmenus with data indicating which option */
  /* is selected */
  gnc_option_menu_init(ftd->match_accounts_picker);
  gnc_option_menu_init(ftd->credit_debit_picker);
  gnc_option_menu_init(ftd->amount_comp_picker);
  gnc_option_menu_init(ftd->shares_comp_picker);
  gnc_option_menu_init(ftd->price_comp_picker);

  /* set data so we can find the struct in callbacks */
  gtk_object_set_data(GTK_OBJECT(ftd->dialog), "find_transactions_structure",
                      ftd);
  

  /* if there's no original query, make the narrow, add, delete 
   * buttons inaccessible */
  if(!ftd->q) {
    gtk_widget_set_sensitive(GTK_WIDGET(ftd->narrow_search_radiobutton),
                             0);
    gtk_widget_set_sensitive(GTK_WIDGET(ftd->add_search_radiobutton),
                             0);
    gtk_widget_set_sensitive(GTK_WIDGET(ftd->delete_search_radiobutton),
                             0);
  }
  else {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                 (ftd->new_search_radiobutton),
                                 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                 (ftd->narrow_search_radiobutton),
                                 1);
  }
  
  gtk_widget_show_all(ftd->dialog);
  return ftd;
}


/********************************************************************\
 * gnc_ui_find_transactions_dialog_destroy
\********************************************************************/

void
gnc_ui_find_transactions_dialog_destroy(FindTransactionsDialog * ftd) {
  gnome_dialog_close(GNOME_DIALOG(ftd->dialog));
  
  ftd->dialog = NULL;
  g_free(ftd->ymd_format);
  g_free(ftd);
}
  

/********************************************************************\
 * callbacks for radio button selections 
\********************************************************************/

void
gnc_ui_find_transactions_dialog_search_type_cb(GtkToggleButton * tb,
                                               gpointer user_data) {
  GSList * buttongroup = 
    gtk_radio_button_group(GTK_RADIO_BUTTON(tb));
  
  FindTransactionsDialog * ftd = 
    gtk_object_get_data(GTK_OBJECT(user_data), "find_transactions_structure");
    
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tb))) {
    ftd->search_type = 
      g_slist_length(buttongroup) -  g_slist_index(buttongroup, tb) - 1;
  } 

}


/********************************************************************\
 * gnc_ui_find_transactions_dialog_cancel_cb
\********************************************************************/

void
gnc_ui_find_transactions_dialog_cancel_cb(GtkButton * button, 
                                    gpointer user_data) {
  FindTransactionsDialog * ftd =
    gtk_object_get_data(GTK_OBJECT(user_data), "find_transactions_structure");

  gnc_ui_find_transactions_dialog_destroy(ftd);
}


/********************************************************************\
 * gnc_ui_find_transactions_dialog_help_cb
\********************************************************************/

void
gnc_ui_find_transactions_dialog_help_cb(GtkButton * button, 
                                        gpointer user_data) {
  helpWindow(NULL, NULL, HH_FIND_TRANSACTIONS);
}


/********************************************************************\
 * gnc_ui_find_transactions_dialog_early_date_toggle_cb
\********************************************************************/

void
gnc_ui_find_transactions_dialog_early_date_toggle_cb(GtkToggleButton * button, 
                                                     gpointer user_data) {
  FindTransactionsDialog * ftd =
    gtk_object_get_data(GTK_OBJECT(user_data), "find_transactions_structure");
  
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
    gtk_widget_set_sensitive(GTK_WIDGET(ftd->date_start_entry), TRUE);
  }
  else {
    gtk_widget_set_sensitive(GTK_WIDGET(ftd->date_start_entry), FALSE);
  }
}


/********************************************************************\
 * gnc_ui_find_transactions_dialog_late_date_toggle_cb
\********************************************************************/

void
gnc_ui_find_transactions_dialog_late_date_toggle_cb(GtkToggleButton * button, 
                                                     gpointer user_data) {
  FindTransactionsDialog * ftd =
    gtk_object_get_data(GTK_OBJECT(user_data), "find_transactions_structure");
  
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
    gtk_widget_set_sensitive(GTK_WIDGET(ftd->date_end_entry), TRUE);
  }
  else {
    gtk_widget_set_sensitive(GTK_WIDGET(ftd->date_end_entry), FALSE);
  }
}


/********************************************************************\
 * gnc_ui_find_transactions_dialog_ok_cb
\********************************************************************/

void
gnc_ui_find_transactions_dialog_ok_cb(GtkButton * button, 
                                      gpointer user_data) {
  GtkWidget              * dialog = user_data;
  FindTransactionsDialog * ftd = 
    gtk_object_get_data(GTK_OBJECT(dialog), "find_transactions_structure");
  
  GList   * selected_accounts;
  char    * descript_match_text;
  char    * memo_match_text;
  char    * number_match_text;
  char    * action_match_text;
  char    * tag_match_text;

  int     search_type = ftd->search_type;

  float   amt_temp;
  int     amt_type;
  Query   * q, * q2;
  gboolean new_ledger = FALSE;

  int    use_start_date, use_end_date;
  time_t start_date, end_date;

  int c_cleared, c_notcleared, c_reconciled;

  if(search_type == 0) {
    if(ftd->q) xaccFreeQuery(ftd->q);
    ftd->q = xaccMallocQuery();    
  }

  assert(ftd->q);

  q = xaccMallocQuery();
  xaccQuerySetGroup(q, gncGetCurrentGroup());

  /* account selections */
  selected_accounts = 
    gnc_account_tree_get_current_accounts
    (GNC_ACCOUNT_TREE(ftd->account_tree));

  /* description */
  descript_match_text = 
    gtk_entry_get_text(GTK_ENTRY(ftd->description_entry));

  /* memo */
  memo_match_text = 
    gtk_entry_get_text(GTK_ENTRY(ftd->memo_entry));

  /* number */
  number_match_text = 
    gtk_entry_get_text(GTK_ENTRY(ftd->number_entry));

  /* action */
  action_match_text = 
    gtk_entry_get_text(GTK_ENTRY(ftd->action_entry));
  
  /* tag */
  tag_match_text = 
    gtk_entry_get_text(GTK_ENTRY(ftd->tag_entry));
  
  if(selected_accounts) {
    xaccQueryAddAccountMatch(q, selected_accounts,
                             gnc_option_menu_get_active
                             (ftd->match_accounts_picker),
                             QUERY_AND);
  }                             
  
  if(strlen(descript_match_text)) {    
    xaccQueryAddDescriptionMatch(q, descript_match_text, 
                                 gtk_toggle_button_get_active
                                 (GTK_TOGGLE_BUTTON
                                  (ftd->description_case_toggle)),
                                 gtk_toggle_button_get_active
                                 (GTK_TOGGLE_BUTTON
                                  (ftd->description_regexp_toggle)),
                                 QUERY_AND);
  }
  

  if(strlen(number_match_text)) {    
    xaccQueryAddNumberMatch(q, number_match_text, 
                            gtk_toggle_button_get_active
                            (GTK_TOGGLE_BUTTON
                             (ftd->number_case_toggle)),
                            gtk_toggle_button_get_active
                            (GTK_TOGGLE_BUTTON
                             (ftd->number_regexp_toggle)),
                            QUERY_AND);
  }
  

  amt_temp = 
    (double)gtk_spin_button_get_value_as_float
    (GTK_SPIN_BUTTON(ftd->amount_entry));
  
  amt_type = gnc_option_menu_get_active(ftd->credit_debit_picker);
  
  if((amt_temp > 0.00001) || (amt_type != 0)) {
    DxaccQueryAddAmountMatch(q, 
                            amt_temp,
                            amt_type,
                            gnc_option_menu_get_active
                            (ftd->amount_comp_picker),
                            QUERY_AND);
  }
  
  use_start_date = 
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ftd->date_start_toggle));
  use_end_date = 
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ftd->date_end_toggle));

  start_date = gnc_date_edit_get_date(GNC_DATE_EDIT(ftd->date_start_entry));
  end_date   = gnc_date_edit_get_date(GNC_DATE_EDIT(ftd->date_end_entry));

  if(use_start_date || use_end_date) {
    xaccQueryAddDateMatchTT(q, 
                            use_start_date, start_date, 
                            use_end_date, end_date,
                            QUERY_AND);
  }
  
  if(strlen(memo_match_text)) {    
    xaccQueryAddMemoMatch(q, memo_match_text, 
                          gtk_toggle_button_get_active
                          (GTK_TOGGLE_BUTTON(ftd->memo_case_toggle)),
                          gtk_toggle_button_get_active
                          (GTK_TOGGLE_BUTTON(ftd->memo_regexp_toggle)),
                          QUERY_AND);    
  }

  amt_temp = 
    (double)gtk_spin_button_get_value_as_float
    (GTK_SPIN_BUTTON(ftd->price_entry));
  amt_type = 
    gnc_option_menu_get_active(ftd->price_comp_picker);

  if((amt_temp > 0.00001) || (amt_type != 0)) {
    DxaccQueryAddSharePriceMatch(q, 
                                amt_temp,
                                amt_type,
                                QUERY_AND);
  }
  
  amt_temp = 
    (double)gtk_spin_button_get_value_as_float
    (GTK_SPIN_BUTTON(ftd->shares_entry));
  amt_type = 
    gnc_option_menu_get_active(ftd->shares_comp_picker);
  
  if((amt_temp > 0.00001) || (amt_type != 0)) {
    DxaccQueryAddSharesMatch(q, 
                            amt_temp,
                            amt_type,
                            QUERY_AND);
  }

  if(strlen(action_match_text)) {    
    xaccQueryAddActionMatch(q, action_match_text, 
                            gtk_toggle_button_get_active
                            (GTK_TOGGLE_BUTTON
                             (ftd->action_case_toggle)),
                            gtk_toggle_button_get_active
                            (GTK_TOGGLE_BUTTON
                             (ftd->action_regexp_toggle)),
                            QUERY_AND);
  }
  
  c_cleared = gtk_toggle_button_get_active
    (GTK_TOGGLE_BUTTON(ftd->cleared_cleared_toggle));
  c_notcleared = gtk_toggle_button_get_active
    (GTK_TOGGLE_BUTTON(ftd->cleared_not_cleared_toggle));
  c_reconciled = gtk_toggle_button_get_active
    (GTK_TOGGLE_BUTTON(ftd->cleared_reconciled_toggle));
  
  if(c_cleared || c_notcleared || c_reconciled) {
    int how = 0;
    if(c_cleared)    how = how | CLEARED_CLEARED;
    if(c_notcleared) how = how | CLEARED_NO;
    if(c_reconciled) how = how | CLEARED_RECONCILED;
    xaccQueryAddClearedMatch(q, how, QUERY_AND);
  }

  if(!ftd->ledger) {
    new_ledger = TRUE;
    ftd->ledger = xaccLedgerDisplayGeneral(NULL, NULL,
                                           SEARCH_LEDGER,
                                           REG_SINGLE_LINE);
    xaccFreeQuery(ftd->ledger->query);
  }

  switch(search_type) {
  case 0:
    ftd->ledger->query = q;
    break;
  case 1:    
    ftd->ledger->query = xaccQueryMerge(ftd->q, q, QUERY_AND);
    xaccFreeQuery(q);
    break;
  case 2:
    ftd->ledger->query = xaccQueryMerge(ftd->q, q, QUERY_OR);
    xaccFreeQuery(q);
    break;
  case 3:
    q2 = xaccQueryInvert(q);
    ftd->ledger->query = xaccQueryMerge(ftd->q, q2, QUERY_AND);
    xaccFreeQuery(q2);
    xaccFreeQuery(q);
    break;
  }

  ftd->ledger->dirty = 1;
  xaccLedgerDisplayRefresh(ftd->ledger);
  if (new_ledger) regWindowLedger(ftd->ledger);
  
  gnc_ui_find_transactions_dialog_destroy(ftd);
}


