/********************************************************************\
 * gnc-budget.h -- Budget public handling routines.                 *
 * Copyright (C) 04 sep 2003    Darin Willits <darin@willits.ca>    *
 * Copyright (C) 2005  Chris Shoemaker <c.shoemaker@cox.net>        *
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

/** @addtogroup budget 
 @{
*/
/** @file gnc-budget.h
 * @brief GnuCash Budgets
 *
 *   Design decisions:
 *
 *  - The budget values that the user enters (and that are stored) for
 *  each account are inclusive of any sub-accounts.
 *
 *     - Reason: This allows the user to budget an amount for a
 *     "parent" account, while tracking (e.g.) expenses with more
 *     detail in sub-accounts.
 *
 *     - Implication: when reporting budgeted values for parent
 *     accounts, just show the stored value, not a sum.
 *
 *  - Budget values are always in the account's commodity.
 *
 *
 *
 *  Accounts with sub-accounts can have a value budgeted.  For those
 *  accounts,
 *
 *    Option 1: when setting or getting budgeted values, the value is
 *    *always* exclusive of sub-account values.  Pro: consistent
 *    values in all contexts.  Con: no summary behavior.
 *
 *    Option 2: make setting only for account proper, but always
 *    report summaries. Con: value can change as soon as it is
 *    entered; forces entry from bottom-up.
 *
 *    * Option 3: value is always *inclusive* of sub-accounts, although
 *    potentially in a different commodity.  Pro: allows top-down
 *    entry; no auto value update. Con: ?  [ This option was selected. ]
 *
 */

#ifndef __GNC_BUDGET_H__
#define __GNC_BUDGET_H__

#include <glib.h>

/** The budget data.*/
typedef struct gnc_budget_private GncBudget;

#include "qof.h"
#include "Account.h"
#include "Recurrence.h"

#define GNC_IS_BUDGET(obj)  (QOF_CHECK_TYPE((obj), GNC_ID_BUDGET))
#define GNC_BUDGET(obj)     (QOF_CHECK_CAST((obj), GNC_ID_BUDGET, GncBudget))

#define GNC_BUDGET_MAX_NUM_PERIODS_DIGITS 3 // max num periods == 999

gboolean gnc_budget_register(void);

/**
 * Creates and initializes a Budget.
 **/
GncBudget *gnc_budget_new(QofBook *book);

/** Deletes the given budget object.*/
void gnc_budget_destroy(GncBudget* budget);

const GUID* gnc_budget_get_guid(GncBudget* budget);
#define gnc_budget_return_guid(X) \
  (X ? *(qof_entity_get_guid(QOF_ENTITY(X))) : *(guid_null()))

/** Set/Get the name of the Budget */
void gnc_budget_set_name(GncBudget* budget, const gchar* name);
const gchar* gnc_budget_get_name(GncBudget* budget);

/** Set/Get the description of the Budget */
void gnc_budget_set_description(GncBudget* budget, const gchar* description);
const gchar* gnc_budget_get_description(GncBudget* budget);

/** Set/Get the number of periods in the Budget */
void gnc_budget_set_num_periods(GncBudget* budget, guint num_periods);
guint gnc_budget_get_num_periods(GncBudget* budget);

void gnc_budget_set_recurrence(GncBudget *budget, const Recurrence *r);
const Recurrence * gnc_budget_get_recurrence(GncBudget *budget);

/** Set/Get the starting date of the Budget */
Timespec gnc_budget_get_period_start_date(GncBudget* budget, guint period_num);

/* Period indices are zero-based. */
void gnc_budget_set_account_period_value(
    GncBudget* budget, Account* account, guint period_num, gnc_numeric val);
void gnc_budget_unset_account_period_value(
    GncBudget* budget, Account* account, guint period_num);

gboolean gnc_budget_is_account_period_value_set(
    GncBudget *budget, Account *account, guint period_num);

gnc_numeric gnc_budget_get_account_period_value(
    GncBudget *budget, Account *account, guint period_num);
gnc_numeric gnc_budget_get_account_period_actual_value(
    GncBudget *budget, Account *account, guint period_num);

/** Get the book that this budget is associated with. */
QofBook* gnc_budget_get_book(GncBudget* budget);

/* Returns some budget in the book, or NULL. */
GncBudget* gnc_budget_get_default(QofBook *book);

/* Get the budget associated with the given GUID from the given book. */
GncBudget* gnc_budget_lookup (const GUID *guid, QofBook *book);
#define  gnc_budget_lookup_direct(g,b) gnc_budget_lookup(&(g),(b))

#endif // __BUDGET_H__

/** @} */
/** @} */
