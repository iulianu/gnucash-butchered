/********************************************************************
 * gnc-transaction-sql.h: load and save data to SQL                 *
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
/** @file gnc-transaction-sql.h
 *  @brief load and save data to SQL
 *  @author Copyright (c) 2006-2008 Phil Longstaff <plongstaff@rogers.com>
 *
 * This file implements the top-level QofBackend API for saving/
 * restoring data to/from an SQL database
 */

#ifndef GNC_TRANSACTION_SQL_H_
#define GNC_TRANSACTION_SQL_H_

#include "qof.h"
#include <gmodule.h>

void gnc_sql_init_transaction_handler( void );
void gnc_sql_transaction_commit_splits( GncSqlBackend* be, Transaction* pTx );
gboolean gnc_sql_save_transaction( GncSqlBackend* be, QofInstance* inst );
void gnc_sql_get_account_balances( GncSqlBackend* be, Account* pAccount, 
								    gnc_numeric* start_balance,
								    gnc_numeric* cleared_balance,
									gnc_numeric* reconciled_balance );

typedef struct {
	Account* acct;
	gnc_numeric start_balance;
	gnc_numeric cleared_balance;
	gnc_numeric reconciled_balance;
} acct_balances_t;
GList* gnc_sql_get_account_balances_for_list( GncSqlBackend* be, GList* list );

#endif /* GNC_TRANSACTION_SQL_H_ */
