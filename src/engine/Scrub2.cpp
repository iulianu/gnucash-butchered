/********************************************************************\
 * Scrub2.cpp -- Convert Stock Accounts to use Lots                 *
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

/** @file Scrub2.c
 *  @brief Utilities to Convert Stock Accounts to use Lots
 *  @author Created by Linas Vepstas March 2003
 *  @author Copyright (c) 2003 Linas Vepstas <linas@linas.org>
 *
 * Provides a set of functions and utilities for checking and
 * repairing ('scrubbing clean') the usage of Lots and lot balances
 * in stock and commodity accounts.  Broken lots are repaired using
 * the accounts specific accounting policy (probably FIFO).
 */

#include "config.h"

#include <glib.h>

#include "qof.h"
#include "Account.h"
#include "AccountP.h"
#include "Transaction.h"
#include "TransactionP.h"
#include "Scrub2.h"
#include "ScrubP.h"
#include "cap-gains.h"
#include "gnc-engine.h"
#include "gnc-lot.h"
#include "policy-p.h"

static QofLogModule log_module = GNC_MOD_LOT;

/* ============================================================== */
/** Loop over all splits, and make sure that every split
 * belongs to some lot.  If a split does not belong to
 * any lots, poke it into one.
 */

void
xaccAccountAssignLots (Account *acc)
{
    if (!acc) return;

    ENTER ("acc=%s", xaccAccountGetName(acc));
    xaccAccountBeginEdit (acc);

restart_loop:
    SplitList_t splits = xaccAccountGetSplitList(acc);
    for (SplitList_t::iterator node = splits.begin();
            node != splits.end(); node++)
    {
        Split * split = *node;

        /* If already in lot, then no-op */
        if (split->lot) continue;

        /* Skip voided transactions */
        if (gnc_numeric_zero_p (split->amount) &&
                xaccTransGetVoidStatus(split->parent)) continue;

        if (xaccSplitAssign (split)) goto restart_loop;
    }
    xaccAccountCommitEdit (acc);
    LEAVE ("acc=%s", xaccAccountGetName(acc));
}

/* ============================================================== */

/** The xaccLotFill() routine attempts to assign splits to the
 *  indicated lot until the lot balance goes to zero, or until
 *  there are no suitable (i.e. unassigned) splits left in the
 *  account.  It uses the default accounting policy to choose
 *  the splits to fill out the lot.
 */

void
xaccLotFill (GNCLot *lot)
{
    Account *acc;
    Split *split;
    GNCPolicy *pcy;

    if (!lot) return;
    acc = gnc_lot_get_account(lot);
    pcy = gnc_account_get_policy(acc);

    ENTER ("(lot=%s, acc=%s)", gnc_lot_get_title(lot), xaccAccountGetName(acc));

    /* If balance already zero, we have nothing to do. */
    if (gnc_lot_is_closed (lot)) return;

    split = pcy->PolicyGetSplit (pcy, lot);
    if (!split) return;   /* Handle the common case */

    /* Reject voided transactions */
    if (gnc_numeric_zero_p(split->amount) &&
            xaccTransGetVoidStatus(split->parent)) return;

    xaccAccountBeginEdit (acc);

    /* Loop until we've filled up the lot, (i.e. till the
     * balance goes to zero) or there are no splits left.  */
    while (1)
    {
        Split *subsplit;

        subsplit = xaccSplitAssignToLot (split, lot);
        if (subsplit == split)
        {
            PERR ("Accounting Policy gave us a split that "
                  "doesn't fit into this lot\n"
                  "lot baln=%s, isclosed=%d, aplit amt=%s",
                  gnc_num_dbg_to_string (gnc_lot_get_balance(lot)),
                  gnc_lot_is_closed (lot),
                  gnc_num_dbg_to_string (split->amount));
            break;
        }

        if (gnc_lot_is_closed (lot)) break;

        split = pcy->PolicyGetSplit (pcy, lot);
        if (!split) break;
    }
    xaccAccountCommitEdit (acc);
    LEAVE ("(lot=%s, acc=%s)", gnc_lot_get_title(lot), xaccAccountGetName(acc));
}

/* ============================================================== */

void
xaccLotScrubDoubleBalance (GNCLot *lot)
{
    gnc_commodity *currency = NULL;
    gnc_numeric zero = gnc_numeric_zero();
    gnc_numeric value = zero;

    if (!lot) return;

    ENTER ("lot=%s", kvp_frame_get_string (gnc_lot_get_slots (lot), "/title"));

    SplitList_t slist = gnc_lot_get_split_list(lot);
    for (SplitList_t::iterator snode = slist.begin(); snode != slist.end(); snode++)
    {
        Split *s = *snode;
        xaccSplitComputeCapGains (s, NULL);
    }

    /* We double-check only closed lots */
    if (FALSE == gnc_lot_is_closed (lot)) return;

    slist = gnc_lot_get_split_list(lot);
    for (SplitList_t::iterator snode = slist.begin(); snode != slist.end(); snode++)
    {
        Split *s = *snode;
        Transaction *trans = s->parent;

        /* Check to make sure all splits in the lot have a common currency */
        if (NULL == currency)
        {
            currency = trans->common_currency;
        }
        if (FALSE == gnc_commodity_equiv (currency, trans->common_currency))
        {
            /* This lot has mixed currencies. Can't double-balance.
             * Silently punt */
            PWARN ("Lot with multiple currencies:\n"
                   "\ttrans=%s curr=%s", xaccTransGetDescription(trans),
                   gnc_commodity_get_fullname(trans->common_currency));
            break;
        }

        /* Now, total up the values */
        value = gnc_numeric_add (value, xaccSplitGetValue (s),
                                 GNC_DENOM_AUTO, GNC_HOW_DENOM_EXACT);
        PINFO ("Split=%p value=%s Accum Lot value=%s", s,
               gnc_num_dbg_to_string (s->value),
               gnc_num_dbg_to_string (value));

    }

    if (FALSE == gnc_numeric_equal (value, zero))
    {
        /* Unhandled error condition. Not sure what to do here,
         * Since the ComputeCapGains should have gotten it right.
         * I suppose there might be small rounding errors, a penny or two,
         * the ideal thing would to figure out why there's a rounding
         * error, and fix that.
         */
        PERR ("Closed lot fails to double-balance !! lot value=%s",
              gnc_num_dbg_to_string (value));
        SplitList_t nlist = gnc_lot_get_split_list(lot);
        for (SplitList_t::iterator node = nlist.begin(); node != nlist.end(); node++)
        {
            Split *s = *node;
            PERR ("s=%p amt=%s val=%s", s,
                  gnc_num_dbg_to_string(s->amount),
                  gnc_num_dbg_to_string(s->value));
        }
    }

    LEAVE ("lot=%s", kvp_frame_get_string (gnc_lot_get_slots (lot), "/title"));
}

/* ================================================================= */

static inline bool
is_subsplit (Split *split)
{
    KvpValue *kval;

    /* generic stop-progress conditions */
    if (!split) return FALSE;
    g_return_val_if_fail (split->parent, FALSE);

    /* If there are no sub-splits, then there's nothing to do. */
    kval = kvp_frame_get_slot (split->kvp_data, "lot-split");
    if (!kval) return FALSE;

    return TRUE;
}

/* ================================================================= */


/* ================================================================= */

/* Remove the guid of b from a.  Note that a may not contain the guid
 * of b, (and v.v.) in which case, it will contain other guids which
 * establish the links. So merge them back in. */

static void
remove_guids (Split *sa, Split *sb)
{
    KvpFrame *ksub;

    /* Find and remove the matching guid's */
    ksub = (KvpFrame*)gnc_kvp_bag_find_by_guid (sa->kvp_data, "lot-split",
            "peer_guid", qof_instance_get_guid(sb));
    if (ksub)
    {
        gnc_kvp_bag_remove_frame (sa->kvp_data, "lot-split", ksub);
        kvp_frame_delete (ksub);
    }

    /* Now do it in the other direction */
    ksub = (KvpFrame*)gnc_kvp_bag_find_by_guid (sb->kvp_data, "lot-split",
            "peer_guid", qof_instance_get_guid(sa));
    if (ksub)
    {
        gnc_kvp_bag_remove_frame (sb->kvp_data, "lot-split", ksub);
        kvp_frame_delete (ksub);
    }

    /* Finally, merge b's lot-splits, if any, into a's */
    /* This is an important step, if it got busted into many pieces. */
    gnc_kvp_bag_merge (sa->kvp_data, "lot-split",
                       sb->kvp_data, "lot-split");
}

/* The merge_splits() routine causes the amount & value of sb
 * to be merged into sa; it then destroys sb.  It also performs
 * some other misc cleanup */

static void
merge_splits (Split *sa, Split *sb)
{
    Account *act;
    Transaction *txn;
    gnc_numeric amt, val;

    act = xaccSplitGetAccount (sb);
    xaccAccountBeginEdit (act);

    txn = sa->parent;
    xaccTransBeginEdit (txn);

    /* Remove the guid of sb from the 'gemini' of sa */
    remove_guids (sa, sb);

    /* Add amount of sb into sa, ditto for value. */
    amt = xaccSplitGetAmount (sa);
    amt = gnc_numeric_add_fixed (amt, xaccSplitGetAmount (sb));
    xaccSplitSetAmount (sa, amt);

    val = xaccSplitGetValue (sa);
    val = gnc_numeric_add_fixed (val, xaccSplitGetValue (sb));
    xaccSplitSetValue (sa, val);

    /* Set reconcile to no; after this much violence,
     * no way its reconciled. */
    xaccSplitSetReconcile (sa, NREC);

    /* If sb has associated gains splits, trash them. */
    if ((sb->gains_split) &&
            (sb->gains_split->gains & GAINS_STATUS_GAINS))
    {
        Transaction *t = sb->gains_split->parent;
        xaccTransBeginEdit (t);
        xaccTransDestroy (t);
        xaccTransCommitEdit (t);
    }

    /* Finally, delete sb */
    xaccSplitDestroy(sb);

    xaccTransCommitEdit (txn);
    xaccAccountCommitEdit (act);
}

bool
xaccScrubMergeSubSplits (Split *split)
{
    bool rc = FALSE;
    Transaction *txn;
    GNCLot *lot;
    const GncGUID *guid;

    if (FALSE == is_subsplit (split)) return FALSE;

    txn = split->parent;
    lot = xaccSplitGetLot (split);

    ENTER ("(Lot=%s)", gnc_lot_get_title(lot));
restart:
    for (SplitList_t::iterator node = txn->splits.begin();
            node != txn->splits.end(); node++)
    {
        Split *s = *node;
        if (xaccSplitGetLot (s) != lot) continue;
        if (s == split) continue;
        if (qof_instance_get_destroying(s)) continue;

        /* OK, this split is in the same lot (and thus same account)
         * as the indicated split.  Make sure it is really a subsplit
         * of the split we started with.  It's possible to have two
         * splits in the same lot and transaction that are not subsplits
         * of each other, the test-period test suite does this, for
         * example.  Only worry about adjacent sub-splits.  By
         * repeatedly merging adjacent subsplits, we'll get the non-
         * adjacent ones too. */
        guid = qof_instance_get_guid(s);
        if (gnc_kvp_bag_find_by_guid (split->kvp_data, "lot-split",
                                      "peer_guid", guid) == NULL)
            continue;

        merge_splits (split, s);
        rc = TRUE;
        goto restart;
    }
    if (gnc_numeric_zero_p (split->amount))
    {
        PWARN ("Result of merge has zero amt!");
    }
    LEAVE (" splits merged=%d", rc);
    return rc;
}

bool
xaccScrubMergeLotSubSplits (GNCLot *lot)
{
    bool rc = FALSE;

    if (!lot) return FALSE;

    ENTER (" ");
restart:
    SplitList_t slist = gnc_lot_get_split_list(lot);
    for (SplitList_t::const_iterator node = slist.begin(); 
            node != slist.end(); node++)
    {
        Split *s = *node;
        if (!xaccScrubMergeSubSplits(s)) continue;

        rc = TRUE;
        goto restart;
    }
    LEAVE (" splits merged=%d", rc);
    return rc;
}

/* =========================== END OF FILE ======================= */
