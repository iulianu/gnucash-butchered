/********************************************************************\
 * Scrub.h -- convert single-entry accounts to clean double-entry   *
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
\********************************************************************/

/** @file Scrub.h
 *
 * Provides a set of functions and utilities for checking and
 * repairing (formerly called 'scrubbing clean') single-entry accounts
 * so that they can be promoted into self-consistent, clean
 * double-entry accounts. Basically and additionally, this file
 * collects all functions that turn old (deprecated) data structures
 * into the current new data model.
 *
 * HISTORY:
 * Created by Linas Vepstas December 1998
 * Copyright (c) 1998-2000, 2003 Linas Vepstas <linas@linas.org>
 */

#ifndef XACC_SCRUB_H
#define XACC_SCRUB_H

#include "Group.h"
#include "gnc-engine.h"

/** The ScrubOrphans() methods search for transacations that contain
 *    splits that do not have a parent account. These "orphaned splits"
 *    are placed into an "orphan account" which the user will have to 
 *    go into and clean up.  Kind of like the unix "Lost+Found" directory
 *    for orphaned inodes.
 *
 * The xaccTransScrubOrphans() method scrubs only the splits in the
 *    given transaction. 
 *
 * The xaccAccountScrubOrphans() method performs this scrub only for the 
 *    indicated account, and not for any of its children.
 *
 * The xaccAccountTreeScrubOrphans() method performs this scrub for the 
 *    indicated account and its children.
 *
 * The xaccGroupScrubOrphans() method performs this scrub for the 
 *    child accounts of this group.
 */
void xaccTransScrubOrphans (Transaction *trans);
void xaccAccountScrubOrphans (Account *acc);
void xaccAccountTreeScrubOrphans (Account *acc);
void xaccGroupScrubOrphans (AccountGroup *grp);

/** The xaccSplitScrub method ensures that if this split has the same
 *   commodity and currency, then it will have the same amount and value.  
 *   If the commoidty is the currency, the split->amount is set to the 
 *   split value.  In addition, if this split is an orphan, that is
 *   fixed first.  If the split account doesn't have a commodity declared,
 *   an attempt is made to fix that first.
 */
void xaccSplitScrub (Split *split);

/** The xacc*ScrubSplits() calls xaccSplitScrub() on each split
 *    in the respective structure: transaction, account, 
 *    account & it's children, account-group.
 */
void xaccTransScrubSplits (Transaction *trans);
void xaccAccountScrubSplits (Account *account);
void xaccAccountTreeScrubSplits (Account *account);
void xaccGroupScrubSplits (AccountGroup *group);

/** The xaccScrubImbalance() method searches for transactions that do
 *    not balance to zero. If any such transactions are found, a split
 *    is created to offset this amount and is added to an "imbalance"
 *    account.
 */
void xaccTransScrubImbalance (Transaction *trans, AccountGroup *root,
                              Account *parent);
void xaccAccountScrubImbalance (Account *acc);
void xaccAccountTreeScrubImbalance (Account *acc);
void xaccGroupScrubImbalance (AccountGroup *grp);

/** The xaccTransScrubCurrency method fixes transactions without a
 * common_currency by using the old account currency and security
 * fields of the parent accounts of the transaction's splits. */
void xaccTransScrubCurrency (Transaction *trans);

/** The xaccAccountScrubCommodity method fixed accounts without
 * a commodity by using the old account currency and security. */
void xaccAccountScrubCommodity (Account *account);

/** The xaccGroupScrubCommodities will scrub the currency/commodity
 * of all accounts & transactions in the group. */
void xaccGroupScrubCommodities (AccountGroup *group);

#endif /* XACC_SCRUB_H */
