/********************************************************************\
 * ofx-import.c -- OFX files import                                 *
 *                        (GnuCash)                                 *
 * Copyright (C) 2002 Benoit Gr�goire <bock@step.polymtl.ca>        *
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
#include <string.h>
#include <sys/time.h>

#include <glib.h>
#include <guile/gh.h>
#include <gmodule.h>

#include "libofx.h"
#include "gnc-generic-import.h"
#include "Account.h"
#include "Transaction.h"
#include "gnc-ofx-import.h"
#include "gnc-file-dialog.h"
#include "gnc-engine-util.h"
#include "gnc-book.h"
#include "gnc-ui-util.h"

static short module = MOD_IMPORT;
/********************************************************************\
 * gnc_file_ofx_import
 * Entry point
\********************************************************************/

SCM  scm_gnc_file_ofx_import ()
{
  gnc_file_ofx_import();
  return SCM_EOL;
}

void gnc_file_ofx_import (void)
{
  extern int ofx_PARSER_msg;
  extern int ofx_DEBUG_msg;
  extern int ofx_WARNING_msg;
  extern int ofx_ERROR_msg;
  extern int ofx_INFO_msg;
  extern int ofx_STATUS_msg;
  char *filenames[3];
  char file[255] = "/home/bock/pfe/scratch/stmtrs_spec201.xml";
  char *selected_filename;

  ofx_PARSER_msg = false;
  ofx_DEBUG_msg = false;
  ofx_WARNING_msg = true;
  ofx_ERROR_msg = true;
  ofx_INFO_msg = true;
  ofx_STATUS_msg = false;

  gnc_should_log(MOD_IMPORT, GNC_LOG_TRACE);
  DEBUG("gnc_file_ofx_import(): Begin...\n");

selected_filename = gnc_file_dialog ("Select an OFX/QFX file to process",
				NULL,
				NULL);

  if(selected_filename!=NULL)
    {
  /*strncpy(file,selected_filename, 255);*/
  DEBUG("Filename found: %s",selected_filename);
      filenames[0]=NULL;
      filenames[1]= selected_filename;
/*      filenames[1]=file;*/
      filenames[2]=NULL;
      DEBUG("Opening selected file");
      ofx_proc_file(2, filenames);
    }

}

int ofx_proc_status(struct OfxStatusData data)
{
  return 0;
}

int ofx_proc_transaction(struct OfxTransactionData data)
{
  char dest_string[255];
  time_t current_time; 
  Account *account;
  GNCBook *book;
  Transaction *transaction;
  Split *split;
  gchar *notes, *tmp;
  gnc_numeric gnc_amount;
  
  if(data.account_id_valid==true){
    account = gnc_import_select_account(data.account_id, 0, NULL, NULL, NO_TYPE);
    if(account!=NULL)
      {
	book = xaccAccountGetBook(account);
	transaction = xaccMallocTransaction(book);
	xaccTransBeginEdit(transaction);
	
	if(data.fi_id_valid==true){
	  gnc_import_set_trans_online_id(transaction, data.fi_id);
	}
	
	if(data.date_initiated_valid==true){
	  xaccTransSetDateSecs(transaction, data.date_initiated);
	}
	else if(data.date_posted_valid==true){
	  xaccTransSetDateSecs(transaction, data.date_posted);
	}
	
	if(data.date_posted_valid==true){
	  xaccTransSetDatePostedSecs(transaction, data.date_posted);
	}
	
	current_time = time(NULL);
	xaccTransSetDateEnteredSecs(transaction, mktime(localtime(&current_time)));
	
	if(data.check_number_valid==true){
	  xaccTransSetNum(transaction, data.check_number);
	}
	else if(data.reference_number_valid==true){
	  xaccTransSetNum(transaction, data.reference_number);
	}
	/* Put the transaction name in Description, or memo if name is unavailable */ 
	if(data.name_valid==true){
	  xaccTransSetDescription(transaction, data.name);
	}
	else if(data.memo_valid==true){
	  xaccTransSetDescription(transaction, data.memo);
	}
	
	/* Put everything else in the Notes field */
	notes=g_strdup_printf("OFX ext. info: ");
	
	if(data.transactiontype_valid==true){
	  switch(data.transactiontype){
	  case OFX_CREDIT: strncpy(dest_string, "Generic credit", sizeof(dest_string));
	    break;
	  case OFX_DEBIT: strncpy(dest_string, "Generic debit", sizeof(dest_string));
	    break;
	  case OFX_INT: strncpy(dest_string, "Interest earned or paid (Note: Depends on signage of amount)", sizeof(dest_string));
	    break;
	  case OFX_DIV: strncpy(dest_string, "Dividend", sizeof(dest_string));
	    break;
	  case OFX_FEE: strncpy(dest_string, "FI fee", sizeof(dest_string));
	    break;
	  case OFX_SRVCHG: strncpy(dest_string, "Service charge", sizeof(dest_string));
	    break;
	  case OFX_DEP: strncpy(dest_string, "Deposit", sizeof(dest_string));
	    break;
	  case OFX_ATM: strncpy(dest_string, "ATM debit or credit (Note: Depends on signage of amount)", sizeof(dest_string));
	    break;
	  case OFX_POS: strncpy(dest_string, "Point of sale debit or credit (Note: Depends on signage of amount)", sizeof(dest_string));
	    break;
	  case OFX_XFER: strncpy(dest_string, "Transfer", sizeof(dest_string));
	    break;
	  case OFX_CHECK: strncpy(dest_string, "Check", sizeof(dest_string));
	    break;
	  case OFX_PAYMENT: strncpy(dest_string, "Electronic payment", sizeof(dest_string));
	    break;
	  case OFX_CASH: strncpy(dest_string, "Cash withdrawal", sizeof(dest_string));
	    break;
	  case OFX_DIRECTDEP: strncpy(dest_string, "Direct deposit", sizeof(dest_string));
	    break;
	  case OFX_DIRECTDEBIT: strncpy(dest_string, "Merchant initiated debit", sizeof(dest_string));
	    break;
	  case OFX_REPEATPMT: strncpy(dest_string, "Repeating payment/standing order", sizeof(dest_string));
	    break;
	  case OFX_OTHER: strncpy(dest_string, "Other", sizeof(dest_string));
	    break;
	  default : strncpy(dest_string, "Unknown transaction type", sizeof(dest_string));
	    break;
	  }
	  tmp=notes;
	  notes=g_strdup_printf("%s%s%s",tmp,"|Trans type:", dest_string);
	  g_free(tmp);
	}
	if(data.memo_valid==true&&data.name_valid==false){
	  tmp=notes;
	  notes=g_strdup_printf("%s%s%s",tmp, "|Memo:", data.memo);
	  g_free(tmp);
	}
	if(data.date_funds_available_valid==true){
	  strftime(dest_string,sizeof(dest_string),"%c %Z",localtime(&(data.date_funds_available)));
	  tmp=notes;
	  notes=g_strdup_printf("%s%s%s",tmp,"|Date funds available:", dest_string);
	  g_free(tmp);
	}
	if(data.server_transaction_id_valid==true){
	  tmp=notes;
	  notes=g_strdup_printf("%s%s%s",tmp, "|Server trans ID (conf. number):", data.server_transaction_id);
	  g_free(tmp);
	}
	if(data.standard_industrial_code_valid==true){
	  tmp=notes;
	  notes=g_strdup_printf("%s%s%ld",tmp, "|Standard Industrial Code:",data.standard_industrial_code);
	  g_free(tmp);
	  
	}
	if(data.payee_id_valid==true){
	  tmp=notes;
	  notes=g_strdup_printf("%s%s%s",tmp,"|Payee ID:", data.payee_id);
	  g_free(tmp);
	}

	printf("WRITEME: Gnucash ofx_proc_transaction():Add PAYEE and ADRESS here once supported by libofx!\n");

	/* Ideally, gnucash should process the corrected transactions */
	if(data.fi_id_corrected_valid==true){
	  printf("WRITEME: Gnucash ofx_proc_transaction(): WARNING: This transaction corrected a previous transaction, but we created a new one instead!\n");
	  tmp=notes;
	  notes=g_strdup_printf("%s%s%s%s",tmp,"|This corrects transaction #",data.fi_id_corrected,"but Gnucash didn't process the correction!");
	  g_free(tmp);
	}
	xaccTransSetNotes(transaction, notes);
	g_free(notes);
	xaccTransSetCurrency(transaction,xaccAccountGetCommodity(account));
	if(data.amount_valid==true){
	  split=xaccMallocSplit(book);
	  xaccTransAppendSplit(transaction,split);
	  xaccAccountInsertSplit(account,split);
	  gnc_amount = double_to_gnc_numeric(data.amount,xaccAccountGetCommoditySCU(account),GNC_RND_ROUND);
	  xaccSplitSetBaseValue(split, gnc_amount,xaccAccountGetCommodity(account));
	  
	  /* Also put the ofx transaction name in the splits memo field, or ofx memo if name is unavailable */ 
	  if(data.name_valid==true){
	    xaccSplitSetMemo(split, data.name);
	  }
	  else if(data.memo_valid==true){
	    xaccSplitSetMemo(split, data.memo);
	  }
	 /* xaccTransCommitEdit(transaction); */
	  
	  gnc_import_add_trans(transaction);
	}
	else
	  {
	    PERR("The transaction doesn't have a valid amount");
	  }
	
      }
    else
      {
	PERR("Unable to find the account!");
      }
  }
  else
    {
      PERR("account ID for this transaction is unavailable!");
    }
  return 0;
}//end ofx_proc_transaction()

int ofx_proc_statement(struct OfxStatementData data)
{
  return 0;
}//end ofx_proc_statement()

int ofx_proc_account(struct OfxAccountData data)
{
  Account *selected_account;
  gnc_commodity_table * commodity_table;
  gnc_commodity * default_commodity;
  GNCAccountType default_type=NO_TYPE;

  if(data.account_id_valid==true){
    //printf("ofx_proc_account() Now calling gnc_import_select_account()\n");
    printf("WRITEME:  ofx_proc_account() Fill in the account type, default name, currency, etc.  \n"); 
    commodity_table = gnc_get_current_commodities ();
    if( data.currency_valid == true)
      {
	DEBUG("Currency from libofx: %s",data.currency);
	default_commodity = gnc_commodity_table_lookup(commodity_table,
						  GNC_COMMODITY_NS_ISO,
						  data.currency);


      }
    else
      {
	default_commodity = NULL;
      }
    
    if(data.account_type_valid==true){
      switch(data.account_type){
      case OFX_CHECKING : default_type=BANK;
	break;
      case OFX_SAVINGS : default_type=BANK;
	break;
      case OFX_MONEYMRKT : default_type=MONEYMRKT;
	break;
      case OFX_CREDITLINE : default_type=CREDITLINE;
	break;
      case OFX_CMA : default_type=NO_TYPE;
	break;
      case OFX_CREDITCARD : default_type=CREDIT;
	break;
      default: PERR("WRITEME: ofx_proc_account() This is an unknown account type!");
      }
    }

    selected_account = gnc_import_select_account(data.account_id, 1, NULL, default_commodity, default_type);
  }
  else
    {
      PERR("Gnucash ofx_proc_account():FATAL ERROR' account online ID not available\n");
    }

  return 0;
}
