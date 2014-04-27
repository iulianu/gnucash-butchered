/********************************************************************\
 * Scrub3.cpp -- Constrain Cap Gains to Track Sources of Gains      *
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

/** @file Scrub3.c
 *  @brief Constrain Cap Gains to Track Sources of Gains
 *  @author Created by Linas Vepstas Sept 2003
 *  @author Copyright (c) 2003,2004 Linas Vepstas <linas@linas.org>
 *
 * Provides a set of functions and utilities for checking and
 * repairing ('scrubbing clean') the usage of Cap Gains
 * transactions in stock and commodity accounts.
 */

#include "config.h"

#include <glib.h>

#include "cap-gains.h"
#include "gnc-commodity.h"
#include "gnc-engine.h"
#include "gnc-lot.h"
#include "policy-p.h"
#include "Account.h"
#include "AccountP.h"
#include "Scrub2.h"
#include "Scrub3.h"
#include "Transaction.h"
#include "TransactionP.h"

static QofLogModule log_module = GNC_MOD_LOT;

/* ================================================================= */
/** Cap gains are possible only if the lot commodity is not the same
 * as the transaction currency.  We assume here that all splits in
 * the lot share the same transaction currency, and so we look at
 * the first split, and see what it's currency is.
 * This routine returns TRUE if cap gains are possible.
 */

static inline bool
gains_possible (GNCLot *lot)
{
    Account *acc;
    Split *split;
    bool comeq;
    gnc_commodity *acc_commodity;

    acc = gnc_lot_get_account (lot);

    SplitList_t slist = gnc_lot_get_split_list (lot);
    if (slist.empty()) return false;
    split = *(slist.begin());

    acc_commodity = xaccAccountGetCommodity(acc);
    comeq = gnc_commodity_equiv (acc_commodity, split->parent->common_currency);
    return (false == comeq);
}

/* ================================================================= */
/* XXX What happens if, as a result of scrubbing, the lot is empty?
 * I don't think this is handled properly.  I think that what will
 * happen is we'll end up with an empty, closed lot ... ?
 */

bool
xaccScrubLot (GNCLot *lot)
{
    bool splits_deleted = FALSE;
    gnc_numeric lot_baln;
    bool opening_baln_is_pos, lot_baln_is_pos;
    Account *acc;
    GNCPolicy *pcy;

    if (!lot) return FALSE;
    ENTER ("(lot=%p) %s", lot, gnc_lot_get_title(lot));

    acc = gnc_lot_get_account (lot);
    pcy = gnc_account_get_policy(acc);
    xaccAccountBeginEdit(acc);
    xaccScrubMergeLotSubSplits (lot);

    /* If the lot balance is zero, we don't need to rebalance */
    lot_baln = gnc_lot_get_balance (lot);
    PINFO ("lot baln=%s for %s", gnc_num_dbg_to_string (lot_baln),
           gnc_lot_get_title(lot));
    if (! gnc_numeric_zero_p (lot_baln))
    {
        gnc_numeric opening_baln;

        /* Get the opening balance for this lot */
        pcy->PolicyGetLotOpening (pcy, lot, &opening_baln, NULL, NULL);
        PINFO ("lot opener baln=%s", gnc_num_dbg_to_string (opening_baln));

        /* If the lot is fat, give the boot to all the non-opening
         * splits, and refill it */
        opening_baln_is_pos = gnc_numeric_positive_p(opening_baln);
        lot_baln_is_pos = gnc_numeric_positive_p(lot_baln);
        if ((opening_baln_is_pos || lot_baln_is_pos) &&
                ((!opening_baln_is_pos) || (!lot_baln_is_pos)))
        {
rethin:
            SplitList_t slist = gnc_lot_get_split_list(lot);
            for (SplitList_t::iterator node = slist.begin(); 
                    node != slist.end(); node++)
            {
                Split *s = *node;
                if (pcy->PolicyIsOpeningSplit (pcy, lot, s)) continue;
                gnc_lot_remove_split (lot, s);
                goto rethin;
            }
        }

        /* At this point the lot is thin, so try to fill it */
        xaccLotFill (lot);

        /* Make sure there are no subsplits. */
        splits_deleted = xaccScrubMergeLotSubSplits (lot);
    }

    /* Now re-compute cap gains, and then double-check that.
     * But we only compute cap-gains if gains are possible;
     * that is if the lot commodity is not the same as the
     * currency. That is, one can't possibly have gains
     * selling dollars for dollars.  The business modules
     * use lots with lot commodity == lot currency.
     */
    if (gains_possible (lot))
    {
        xaccLotComputeCapGains (lot, NULL);
        xaccLotScrubDoubleBalance (lot);
    }
    xaccAccountCommitEdit(acc);

    LEAVE ("(lot=%s, deleted=%d)", gnc_lot_get_title(lot), splits_deleted);
    return splits_deleted;
}

/* ============================================================== */

void
xaccAccountScrubLots (Account *acc)
{
    if (!acc) return;
    if (FALSE == xaccAccountHasTrades (acc)) return;

    ENTER ("(acc=%s)", xaccAccountGetName(acc));
    xaccAccountBeginEdit(acc);
    xaccAccountAssignLots (acc);

    LotList_t lots = xaccAccountGetLotList(acc);
    for (LotList_t::iterator it = lots.begin(); it != lots.end(); it++)
    {
        GNCLot *lot = *it;
        xaccScrubLot (lot);
    }
    xaccAccountCommitEdit(acc);
    LEAVE ("(acc=%s)", xaccAccountGetName(acc));
}

/* ============================================================== */

static void
lot_scrub_cb (Account *acc, gpointer data)
{
    if (FALSE == xaccAccountHasTrades (acc)) return;
    xaccAccountScrubLots (acc);
}

void
xaccAccountTreeScrubLots (Account *acc)
{
    if (!acc) return;

    gnc_account_foreach_descendant(acc, lot_scrub_cb, NULL);
    xaccAccountScrubLots (acc);
}

/* ========================== END OF FILE  ========================= */
