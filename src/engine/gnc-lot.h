/********************************************************************\
 * gnc-lot.h -- AR/AP invoices; inventory lots; stock lots          *
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
/** @addtogroup Engine
    @{ */
/** @addtogroup Lot Lots: Core Function for AR/AP, Inventory, Stock Lots, Cap Gains
 * One often needs to know that the item 'bought' in one transaction
 * is the same one as the item 'sold' in a different transaction.
 * Lots are used to make this association.  One Lot holds all of the
 * splits that involve the same item.   A lot is typically formed when
 * the item is bought, and is closed when the item is sold out. 
 * A lot need not be a single item, it can be a quantity of the same 
 * thing e.g. 500 gallons of paint (sold off a few gallons at a time).
 *
 * Lots are required to correctly implement invoices, inventory,
 * depreciation and stock market investment gains. See the file
 * src/doc/lots.txt for a detailed implementation overview.
 *
 * A lot is "closed" when the number of items in the lot has gone to zero.
 * It is very easy to compute the gains/losses for a closed lot: it is the
 * sum-total of the values of the items put into/taken out of the lot.
 * (Realized) Gains on still-open lots can be computed by pro-rating the 
 * purchase prices. 
 * 
 * Lots are nothing more than a collection or grouping of splits in an
 * account. All of the splits in a lot must belong to the same account;
 * there's no mix-n-match.  Thus, in this sense, a lot belongs to an
 * accunt as well.
 *
 * Lots have an implicit "opening date": the date of the earliest split in
 * the lot. The "close date" is the date of the split that brought the lot
 * item balance down to zero. 
 *
 @{ */

/** @file gnc-lot.h
 * 
 * @author Created by Linas Vepstas May 2002
 * @author Copyright (c) 2002,2003 Linas Vepstas <linas@linas.org>
 */

#ifndef GNC_LOT_H
#define GNC_LOT_H

#include "gnc-engine.h"
#include "gnc-numeric.h"
#include "qofbook.h"
#include "qofid.h"
#include "qofobject.h"

GNCLot * gnc_lot_new (QofBook *);
void gnc_lot_destroy (GNCLot *);

GNCLot * gnc_lot_lookup (const GUID *guid, QofBook *book);
QofBook * gnc_lot_get_book (GNCLot *);		  

/** The gnc_lot_add_split() routine adds a split to this lot.  Note
 *    that *all* splits in a lot must also be in the same account.
 *    Note that this routine adds the split unconditionally, with
 *    no regard for the accounting policy.  To enforce a particular
 *    accounting polciy, use the xaccSplitAssignToLot() routine 
 *    instead.
 */
void gnc_lot_add_split (GNCLot *, Split *);
void gnc_lot_remove_split (GNCLot *, Split *);

/** The gnc_lot_get_split_list() routine returns a GList of all the
 *    splits in this lot.  Do *not* not free this list when done;
 *    it is a pointer straight into the lots intenal list.  Do
 *    *not* add to or remove from this list directly.  Calling
 *    either gnc_lot_add_split() or gnc_lot_remove_split() will
 *    invalidate the returned pointer.
 */
SplitList * gnc_lot_get_split_list (GNCLot *);
gint gnc_lot_count_splits (GNCLot *);

/** The gnc_lot_get_account() routine returns the account with which 
 *    this lot is associated. */
Account * gnc_lot_get_account (GNCLot *);

/** The gnc_lot_get_balance() routine returns the balance of the lot. 
 *    The commodity in which this balance is expressed is the commodity 
 *    of the account. */
gnc_numeric gnc_lot_get_balance (GNCLot *);

/** The gnc_lot_is_closed() routine returns a boolean flag: is this 
 *    lot closed?  A lot is closed if its balance is zero.  This 
 *    routine is faster than using gnc_lot_get_balance() because
 *    once the balance goes to zero, this fact is cached.  
 */
gboolean gnc_lot_is_closed (GNCLot *);

/** The gnc_lot_get_earliest_split() routine is a convenience routine
 *    that helps identify the date this lot was opened.   It simply
 *    loops over all of the splits in the lot, and returns the split
 *    with the earliest split->transaction->date_posted.
 */
Split * gnc_lot_get_earliest_split (GNCLot *lot);

/** The gnc_lot_get_latest_split() routine is a convenience routine
 *    that helps identify the date this lot was closed.   It simply
 *    loops over all of the splits in the lot, and returns the split
 *    with the latest split->transaction->date_posted.
 */
Split * gnc_lot_get_latest_split (GNCLot *lot);

/** Get and set the account title, or the account notes. */
const char * gnc_lot_get_title (GNCLot *);
const char * gnc_lot_get_notes (GNCLot *);
void gnc_lot_set_title (GNCLot *, const char *);
void gnc_lot_set_notes (GNCLot *, const char *);

/** Every lot has a place to hang kvp data.  This routine returns that
 *     place. 
 * */
KvpFrame * gnc_lot_get_slots (GNCLot *);

#define gnc_lot_get_guid(X)  qof_entity_get_guid(QOF_ENTITY(X))

#define LOT_IS_CLOSED	"is-closed?"
#define LOT_BALANCE	"balance"
#define LOT_TITLE		"lot-title"
#define LOT_NOTES		"notes"
#endif /* GNC_LOT_H */
/** @} */
/** @} */
