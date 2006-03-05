/********************************************************************\
 * Split.c -- split implementation                                  *
 * Copyright (C) 1997 Robin D. Clark                                *
 * Copyright (C) 1997-2003 Linas Vepstas <linas@linas.org>          *
 * Copyright (C) 2000 Bill Gribble <grib@billgribble.com>           *
 * Copyright (c) 2006 David Hampton <hampton@employees.org>         *
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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "Split.h"
#include "AccountP.h"
#include "Group.h"
#include "Scrub.h"
#include "Scrub3.h"
#include "TransactionP.h"
#include "TransLog.h"
#include "cap-gains.h"
#include "gnc-commodity.h"
#include "gnc-engine.h"
#include "gnc-lot-p.h"
#include "gnc-lot.h"

const char *void_former_amt_str = "void-former-amount";
const char *void_former_val_str = "void-former-value";

#define PRICE_SIGFIGS 6

/* This static indicates the debugging module that this .o belongs to.  */
static QofLogModule log_module = GNC_MOD_ENGINE;

/********************************************************************\
 * xaccInitSplit
 * Initialize a Split structure
\********************************************************************/

static void
xaccInitSplit(Split * split, QofBook *book)
{
  /* fill in some sane defaults */
  split->acc         = NULL;
  split->orig_acc    = NULL;
  split->parent      = NULL;
  split->lot         = NULL;

  split->action      = CACHE_INSERT("");
  split->memo        = CACHE_INSERT("");
  split->reconciled  = NREC;
  split->amount      = gnc_numeric_zero();
  split->value       = gnc_numeric_zero();

  split->date_reconciled.tv_sec  = 0;
  split->date_reconciled.tv_nsec = 0;

  split->balance             = gnc_numeric_zero();
  split->cleared_balance     = gnc_numeric_zero();
  split->reconciled_balance  = gnc_numeric_zero();

  split->idata = 0;

  split->gains = GAINS_STATUS_UNKNOWN;
  split->gains_split = NULL;

  qof_instance_init(&split->inst, GNC_ID_SPLIT, book);
}

void
xaccSplitReinit(Split * split)
{
  /* fill in some sane defaults */
  split->acc         = NULL;
  split->orig_acc    = NULL;
  split->parent      = NULL;
  split->lot         = NULL;

  CACHE_REPLACE(split->action, "");
  CACHE_REPLACE(split->memo, "");
  split->reconciled  = NREC;
  split->amount      = gnc_numeric_zero();
  split->value       = gnc_numeric_zero();

  split->date_reconciled.tv_sec  = 0;
  split->date_reconciled.tv_nsec = 0;

  split->balance             = gnc_numeric_zero();
  split->cleared_balance     = gnc_numeric_zero();
  split->reconciled_balance  = gnc_numeric_zero();

  if (split->inst.kvp_data)
      kvp_frame_delete(split->inst.kvp_data);
  split->inst.kvp_data = kvp_frame_new();
  split->idata = 0;

  split->gains = GAINS_STATUS_UNKNOWN;
  split->gains_split = NULL;
}

/********************************************************************\
\********************************************************************/

Split *
xaccMallocSplit(QofBook *book)
{
  Split *split;
  g_return_val_if_fail (book, NULL);

  split = g_new0 (Split, 1);
  xaccInitSplit (split, book);

  return split;
}

/********************************************************************\
\********************************************************************/
/* This routine is not exposed externally, since it does weird things, 
 * like not really setting up the parent account correctly, and ditto 
 * the parent transaction.  This routine is prone to programmer error
 * if not used correctly.  It is used only by the edit-rollback code.
 * Don't get duped!
 */

Split *
xaccDupeSplit (const Split *s)
{
  Split *split = g_new0 (Split, 1);

  /* Trash the entity table. We don't want to mistake the cloned
   * splits as something official.  If we ever use this split, we'll
   * have to fix this up.
   */
  split->inst.entity.e_type = NULL;
  split->inst.entity.collection = NULL;
  split->inst.entity.guid = s->inst.entity.guid;
  split->inst.book = s->inst.book;

  split->parent = s->parent;
  split->acc = s->acc;
  split->orig_acc = s->orig_acc;
  split->lot = s->lot;

  split->memo = CACHE_INSERT(s->memo);
  split->action = CACHE_INSERT(s->action);

  split->inst.kvp_data = kvp_frame_copy (s->inst.kvp_data);

  split->reconciled = s->reconciled;
  split->date_reconciled = s->date_reconciled;

  split->value = s->value;
  split->amount = s->amount;

  /* no need to futz with the balances;  these get wiped each time ... 
   * split->balance             = s->balance;
   * split->cleared_balance     = s->cleared_balance;
   * split->reconciled_balance  = s->reconciled_balance;
   */

  return split;
}

Split *
xaccSplitClone (const Split *s)
{
  Split *split = g_new0 (Split, 1);

  split->parent              = NULL;
  split->memo                = CACHE_INSERT(s->memo);
  split->action              = CACHE_INSERT(s->action);
  split->reconciled          = s->reconciled;
  split->date_reconciled     = s->date_reconciled;
  split->value               = s->value;
  split->amount              = s->amount;
  split->balance             = s->balance;
  split->cleared_balance     = s->cleared_balance;
  split->reconciled_balance  = s->reconciled_balance;
  split->idata               = 0;

  split->gains = GAINS_STATUS_UNKNOWN;
  split->gains_split = NULL;

  qof_instance_init(&split->inst, GNC_ID_SPLIT, s->inst.book);
  kvp_frame_delete(split->inst.kvp_data);
  split->inst.kvp_data = kvp_frame_copy(s->inst.kvp_data);

  xaccAccountInsertSplit(s->acc, split);
  if (s->lot) 
  {
      /* FIXME: Doesn't look right.  */
    s->lot->splits = g_list_append (s->lot->splits, split);
    s->lot->is_closed = -1;
  }
  return split;
}

#ifdef DUMP_FUNCTIONS
static void
xaccSplitDump (const Split *split, const char *tag)
{
  printf("  %s Split %p", tag, split);
  printf("    GUID:     %s\n", guid_to_string(&split->guid));
  printf("    Book:     %p\n", split->inst.book);
  printf("    Account:  %p\n", split->acc);
  printf("    Lot:      %p\n", split->lot);
  printf("    Parent:   %p\n", split->parent);
  printf("    Memo:     %s\n", split->memo ? split->memo : "(null)");
  printf("    Action:   %s\n", split->action ? split->action : "(null)");
  printf("    KVP Data: %p\n", split->inst.kvp_data);
  printf("    Recncld:  %c (date %s)\n", split->reconciled, 
         gnc_print_date(split->date_reconciled));
  printf("    Value:    %s\n", gnc_numeric_to_string(split->value));
  printf("    Amount:   %s\n", gnc_numeric_to_string(split->amount));
  printf("    Balance:  %s\n", gnc_numeric_to_string(split->balance));
  printf("    CBalance: %s\n", gnc_numeric_to_string(split->cleared_balance));
  printf("    RBalance: %s\n", 
         gnc_numeric_to_string(split->reconciled_balance));
  printf("    idata:    %x\n", split->idata);
}
#endif

/********************************************************************\
\********************************************************************/

void
xaccFreeSplit (Split *split)
{
  if (!split) return;

  /* Debug double-free's */
  if (((char *) 1) == split->memo)
  {
    PERR ("double-free %p", split);
    return;
  }
  CACHE_REMOVE(split->memo);
  CACHE_REMOVE(split->action);

  /* Just in case someone looks up freed memory ... */
  split->memo        = (char *) 1;
  split->action      = NULL;
  split->reconciled  = NREC;
  split->amount      = gnc_numeric_zero();
  split->value       = gnc_numeric_zero();
  split->parent      = NULL;
  split->lot         = NULL;
  split->acc         = NULL;
  split->orig_acc    = NULL;
  
  split->date_reconciled.tv_sec = 0;
  split->date_reconciled.tv_nsec = 0;

  // Is this right? 
  if (split->gains_split) split->gains_split->gains_split = NULL;
  qof_instance_release(&split->inst);
  g_free(split);
}

static void mark_acc(Account *acc)
{
    if (acc && !acc->inst.do_free) {
        acc->balance_dirty = TRUE;
        acc->sort_dirty = TRUE;
    }
}

void mark_split (Split *s)
{
  mark_acc(s->acc);

  /* set dirty flag on lot too. */
  if (s->lot) s->lot->is_closed = -1;
}

/*
 * Helper routine for xaccSplitEqual.
 */
static gboolean
xaccSplitEqualCheckBal (const char *tag, gnc_numeric a, gnc_numeric b)
{
  char *str_a, *str_b;

  if (gnc_numeric_equal (a, b))
    return TRUE;

  str_a = gnc_numeric_to_string (a);
  str_b = gnc_numeric_to_string (b);

  PWARN ("%sbalances differ: %s vs %s", tag, str_a, str_b);

  g_free (str_a);
  g_free (str_b);

  return FALSE;
}

/********************************************************************
 * xaccSplitEqual
 ********************************************************************/
gboolean
xaccSplitEqual(const Split *sa, const Split *sb,
               gboolean check_guids,
               gboolean check_balances,
               gboolean check_txn_splits)
{
  if (!sa && !sb) return TRUE; /* Arguable. FALSE is better, methinks */

  if (!sa || !sb)
  {
    PWARN ("one is NULL");
    return FALSE;
  }

  if (sa == sb) return TRUE;

  if (check_guids) {
    if (!guid_equal(&(sa->inst.entity.guid), &(sb->inst.entity.guid)))
    {
      PWARN ("GUIDs differ");
      return FALSE;
    }
  }

  /* Since these strings are cached we can just use pointer equality */
  if (sa->memo != sb->memo)
  {
    PWARN ("memos differ: (%p)%s vs (%p)%s",
           sa->memo, sa->memo, sb->memo, sb->memo);
    return FALSE;
  }

  if (sa->action != sb->action)
  {
    PWARN ("actions differ: %s vs %s", sa->action, sb->action);
    return FALSE;
  }

  if (kvp_frame_compare(sa->inst.kvp_data, sb->inst.kvp_data) != 0)
  {
    char *frame_a;
    char *frame_b;

    frame_a = kvp_frame_to_string (sa->inst.kvp_data);
    frame_b = kvp_frame_to_string (sb->inst.kvp_data);

    PWARN ("kvp frames differ:\n%s\n\nvs\n\n%s", frame_a, frame_b);

    g_free (frame_a);
    g_free (frame_b);

    return FALSE;
  }

  if (sa->reconciled != sb->reconciled)
  {
    PWARN ("reconcile flags differ: %c vs %c", sa->reconciled, sb->reconciled);
    return FALSE;
  }

  if (timespec_cmp(&(sa->date_reconciled), &(sb->date_reconciled)))
  {
    PWARN ("reconciled date differs");
    return FALSE;
  }

  if (!gnc_numeric_eq(xaccSplitGetAmount (sa), xaccSplitGetAmount (sb)))
  {
    char *str_a;
    char *str_b;

    str_a = gnc_numeric_to_string (xaccSplitGetAmount (sa));
    str_b = gnc_numeric_to_string (xaccSplitGetAmount (sb));

    PWARN ("amounts differ: %s vs %s", str_a, str_b);

    g_free (str_a);
    g_free (str_b);

    return FALSE;
  }

  if (!gnc_numeric_eq(xaccSplitGetValue (sa), xaccSplitGetValue (sb)))
  {
    char *str_a;
    char *str_b;

    str_a = gnc_numeric_to_string (xaccSplitGetValue (sa));
    str_b = gnc_numeric_to_string (xaccSplitGetValue (sb));

    PWARN ("values differ: %s vs %s", str_a, str_b);

    g_free (str_a);
    g_free (str_b);

    return FALSE;
  }

  if (check_balances) {
    if (!xaccSplitEqualCheckBal ("", sa->balance, sb->balance))
      return FALSE;
    if (!xaccSplitEqualCheckBal ("cleared ", sa->cleared_balance, 
                                 sb->cleared_balance))
      return FALSE;
    if (!xaccSplitEqualCheckBal ("reconciled ", sa->reconciled_balance, 
                                 sb->reconciled_balance))
      return FALSE;
  }

  if (!xaccTransEqual(sa->parent, sb->parent, check_guids, check_txn_splits,
                      check_balances, FALSE))
  {
    PWARN ("transactions differ");
    return FALSE;
  }

  return TRUE;
}

static void
add_keys_to_list(gpointer key, gpointer val, gpointer list)
{
    *(GList **)list = g_list_prepend(*(GList **)list, key);
}

GList *
xaccSplitListGetUniqueTransactions(const GList *splits)
{
    const GList *node;
    GList *transList = NULL;
    GHashTable *transHash = g_hash_table_new(g_direct_hash, g_direct_equal);

    for(node = splits; node; node = node->next) {
        Transaction *trans = xaccSplitGetParent((Split *)(node->data));
        g_hash_table_insert(transHash, trans, trans);
    }
    g_hash_table_foreach(transHash, add_keys_to_list, &transList);
    g_hash_table_destroy(transHash);
    return transList;
}

/********************************************************************
 * Account funcs
 ********************************************************************/

Account *
xaccSplitGetAccount (const Split *s)
{
  return s ? s->acc : NULL;
}

void
xaccSplitSetAccount (Split *s, Account *acc)
{
    Transaction *trans;

    g_return_if_fail(s && acc);
    g_return_if_fail(acc->inst.book == s->inst.book);

    trans = s->parent;
    if (trans)
        xaccTransBeginEdit(trans);

    s->acc = acc;
    qof_instance_set_dirty(QOF_INSTANCE(s));

    if (trans)
        xaccTransCommitEdit(trans);
}

/* An engine-private helper for completing xaccTransCommitEdit(). */
void
xaccSplitCommitEdit(Split *s)
{
    Account *acc, *orig_acc;

    g_return_if_fail(s);
    if (!qof_instance_is_dirty(QOF_INSTANCE(s)))
        return;

    orig_acc = s->orig_acc;
    acc = s->acc;

    /* Possibly remove the split from the original account... */
    if (orig_acc && (orig_acc != acc || s->inst.do_free)) {
        GList *node = g_list_find (orig_acc->splits, s);
        if (node) {
            orig_acc->splits = g_list_delete_link (orig_acc->splits, node);
            /* Remove from lot (but only if it hasn't been moved to
               new lot already) */
            if (s->lot && s->lot->account == orig_acc)
                gnc_lot_remove_split (s->lot, s);
            //FIXME: probably not needed.
            xaccGroupMarkNotSaved (orig_acc->parent);
            //FIXME: find better event type
            gnc_engine_gen_event (&orig_acc->inst.entity, GNC_EVENT_MODIFY);
        } else PERR("Account lost track of moved or deleted split.");
        orig_acc->balance_dirty = TRUE;
        xaccAccountRecomputeBalance(orig_acc);
    }

    /* ... and insert it into the new account if needed */
    if (orig_acc != s->acc && !s->inst.do_free) {
        if (!g_list_find(acc->splits, s)) {
            if (acc->inst.editlevel == 0) {
                acc->splits = g_list_insert_sorted(
                    acc->splits, s, (GCompareFunc)xaccSplitDateOrder);
            } else {
                acc->splits = g_list_prepend(acc->splits, s);
                acc->sort_dirty = TRUE;
            }

            /* If the split's lot belonged to some other account, we
               leave it so. */
            if (s->lot && (NULL == s->lot->account))
                xaccAccountInsertLot (acc, s->lot);

            xaccGroupMarkNotSaved (acc->parent); //FIXME: probably not needed.
            //FIXME: find better event
            gnc_engine_gen_event (&acc->inst.entity, GNC_EVENT_MODIFY);
        } else PERR("Account grabbed split prematurely.");
        acc->balance_dirty = TRUE;
        xaccSplitSetAmount(s, xaccSplitGetAmount(s));
    }

    if (s->orig_parent && s->parent != s->orig_parent) {
        //FIXME: find better event
        gnc_engine_gen_event (&s->orig_parent->inst.entity, GNC_EVENT_MODIFY);
    }
    if (s->lot) {
        /* A change of value/amnt affects gains display, etc. */
        gnc_engine_gen_event (&s->lot->inst.entity, GNC_EVENT_MODIFY);
    }

    /* Important: we save off the original parent transaction and account
       so that when we commit, we can generate signals for both the
       original and new transactions, for the _next_ begin/commit cycle. */
    s->orig_acc = s->acc;
    s->orig_parent = s->parent;
    qof_instance_mark_clean(QOF_INSTANCE(s));

    mark_acc(acc);
    xaccAccountRecomputeBalance(acc);
    if (s->inst.do_free)
        xaccFreeSplit(s);
}

/* An engine-private helper for completing xaccTransRollbackEdit(). */
void
xaccSplitRollbackEdit(Split *s)
{
    s->acc = s->orig_acc;  /* Don't use setters, we want to allow NULL */
    s->parent = s->orig_parent;
}

/********************************************************************\
\********************************************************************/

Split *
xaccSplitLookup (const GUID *guid, QofBook *book)
{
  QofCollection *col;
  if (!guid || !book) return NULL;
  col = qof_book_get_collection (book, GNC_ID_SPLIT);
  return (Split *) qof_collection_lookup_entity (col, guid);
}

/********************************************************************\
\********************************************************************/
/* Routines for marking splits dirty, and for sending out change
 * events.  Note that we can't just mark-n-generate-event in one
 * step, since sometimes we need to mark things up before its suitable
 * to send out a change event.
 */

/* CHECKME: This function modifies the Split without dirtying or
   checking its parent.  Is that correct? */
void
xaccSplitDetermineGainStatus (Split *split)
{
   Split *other;
   KvpValue *val;

   if (GAINS_STATUS_UNKNOWN != split->gains) return;

   other = xaccSplitGetCapGainsSplit (split);
   if (other) 
   {
      split->gains = GAINS_STATUS_A_VDIRTY | GAINS_STATUS_DATE_DIRTY;
      split->gains_split = other;
      return;
   }

   val = kvp_frame_get_slot (split->inst.kvp_data, "gains-source");
   if (!val)
   {  
       // FIXME: This looks bogus.  
      other = xaccSplitGetOtherSplit (split);
      if (other) 
          val = kvp_frame_get_slot (other->inst.kvp_data, "gains-source");
      split->gains = GAINS_STATUS_A_VDIRTY | GAINS_STATUS_DATE_DIRTY;
   } else {
      QofCollection *col;
      col = qof_book_get_collection (split->inst.book, GNC_ID_SPLIT);
      split->gains = GAINS_STATUS_GAINS;
      other = (Split *) qof_collection_lookup_entity (col, 
                  kvp_value_get_guid (val));
      split->gains_split = other;
   }
}

/********************************************************************\
\********************************************************************/

static inline int
get_currency_denom(const Split * s)
{
    if (!s)
    {
        return 0;
    }
    else if(!s->parent || !s->parent->common_currency)
    {
        return 100000;
    }
    else
    {
        return gnc_commodity_get_fraction (s->parent->common_currency);
    }
}

static inline int
get_commodity_denom(const Split * s) 
{
    if (!s)
    {
        return 0;
    }
    else if (!s->acc)
    {
        return 100000;
    }
    else
    {
        return xaccAccountGetCommoditySCU(s->acc);
    }
}

/********************************************************************
 * xaccSplitGetSlots
 ********************************************************************/

KvpFrame * 
xaccSplitGetSlots (const Split * s)
{
    return qof_instance_get_slots(QOF_INSTANCE(s));
}

void
xaccSplitSetSlots_nc(Split *s, KvpFrame *frm)
{
  if (!s || !frm) return;
  xaccTransBeginEdit(s->parent);
  qof_instance_set_slots(QOF_INSTANCE(s), frm);
  xaccTransCommitEdit(s->parent);

}

/********************************************************************\
\********************************************************************/

void 
DxaccSplitSetSharePriceAndAmount (Split *s, double price, double amt)
{
  if (!s) return;
  ENTER (" ");
  xaccTransBeginEdit (s->parent);

  s->amount = double_to_gnc_numeric(amt, get_commodity_denom(s),
                                    GNC_HOW_RND_ROUND);
  s->value  = double_to_gnc_numeric(price * amt, get_currency_denom(s),
                                    GNC_HOW_RND_ROUND);

  SET_GAINS_A_VDIRTY(s);
  mark_split (s);
  qof_instance_set_dirty(QOF_INSTANCE(s));
  xaccTransCommitEdit(s->parent);

}

void 
xaccSplitSetSharePriceAndAmount (Split *s, gnc_numeric price, gnc_numeric amt)
{
  if (!s) return;
  ENTER (" ");
  xaccTransBeginEdit (s->parent);

  s->amount = gnc_numeric_convert(amt, get_commodity_denom(s), 
                                  GNC_HOW_RND_ROUND);
  s->value  = gnc_numeric_mul(s->amount, price, 
                              get_currency_denom(s), GNC_HOW_RND_ROUND);

  SET_GAINS_A_VDIRTY(s);
  mark_split (s);
  qof_instance_set_dirty(QOF_INSTANCE(s));
  xaccTransCommitEdit(s->parent);
}

static void
qofSplitSetSharePrice (Split *split, gnc_numeric price)
{
	g_return_if_fail(split);
	split->value = gnc_numeric_mul(xaccSplitGetAmount(split),
		price, get_currency_denom(split),
		GNC_HOW_RND_ROUND);
}

void 
xaccSplitSetSharePrice (Split *s, gnc_numeric price) 
{
  if (!s) return;
  ENTER (" ");
  xaccTransBeginEdit (s->parent);

  s->value = gnc_numeric_mul(xaccSplitGetAmount(s), 
                             price, get_currency_denom(s),
                             GNC_HOW_RND_ROUND);

  SET_GAINS_VDIRTY(s);
  mark_split (s);
  qof_instance_set_dirty(QOF_INSTANCE(s));
  xaccTransCommitEdit(s->parent);
}

void 
DxaccSplitSetShareAmount (Split *s, double damt) 
{
  gnc_numeric old_price, old_amt;
  int commodity_denom = get_commodity_denom(s);
  gnc_numeric amt = double_to_gnc_numeric(damt, commodity_denom, 
                                          GNC_HOW_RND_ROUND); 
  if (!s) return;
  ENTER (" ");
  xaccTransBeginEdit (s->parent);
  
  old_amt = xaccSplitGetAmount (s);
  if (!gnc_numeric_zero_p(old_amt)) 
  {
    old_price = gnc_numeric_div(xaccSplitGetValue (s), 
                                old_amt, GNC_DENOM_AUTO,
                                GNC_HOW_DENOM_REDUCE);
  }
  else {
    old_price = gnc_numeric_create(1, 1);
  }

  s->amount = gnc_numeric_convert(amt, commodity_denom, 
                                  GNC_HOW_RND_NEVER);
  s->value  = gnc_numeric_mul(s->amount, old_price, 
                              get_currency_denom(s), GNC_HOW_RND_ROUND);

  SET_GAINS_A_VDIRTY(s);
  mark_split (s);
  qof_instance_set_dirty(QOF_INSTANCE(s));
  xaccTransCommitEdit(s->parent);
}

static void
qofSplitSetAmount (Split *split, gnc_numeric amt)
{
	g_return_if_fail(split);
	if (split->acc)
	{
		split->amount = gnc_numeric_convert(amt, 
				get_commodity_denom(split), GNC_HOW_RND_ROUND);
	}
	else { split->amount = amt;	}
}

/* The amount of the split in the _account's_ commodity. */
void 
xaccSplitSetAmount (Split *s, gnc_numeric amt) 
{
  if (!s) return;
  g_return_if_fail(gnc_numeric_check(amt) == GNC_ERROR_OK);
  ENTER ("(split=%p) old amt=%" G_GINT64_FORMAT "/%" G_GINT64_FORMAT
	 " new amt=%" G_GINT64_FORMAT "/%" G_GINT64_FORMAT, s,
	 s->amount.num, s->amount.denom, amt.num, amt.denom);

  xaccTransBeginEdit (s->parent);
  if (s->acc)
    s->amount = gnc_numeric_convert(amt, get_commodity_denom(s), 
				    GNC_HOW_RND_ROUND);
  else
    s->amount = amt;

  SET_GAINS_ADIRTY(s);
  mark_split (s);
  qof_instance_set_dirty(QOF_INSTANCE(s));
  xaccTransCommitEdit(s->parent);
  LEAVE("");
}

static void
qofSplitSetValue (Split *split, gnc_numeric amt)
{
	g_return_if_fail(split);
	split->value = gnc_numeric_convert(amt, 
			get_currency_denom(split), GNC_HOW_RND_ROUND);
}

/* The value of the split in the _transaction's_ currency. */
void 
xaccSplitSetValue (Split *s, gnc_numeric amt) 
{
  gnc_numeric new_val;
  if (!s) return;
  
  g_return_if_fail(gnc_numeric_check(amt) == GNC_ERROR_OK);
  ENTER ("(split=%p) old val=%" G_GINT64_FORMAT "/%" G_GINT64_FORMAT
	 " new val=%" G_GINT64_FORMAT "/%" G_GINT64_FORMAT, s,
	 s->value.num, s->value.denom, amt.num, amt.denom);

  xaccTransBeginEdit (s->parent);
  new_val = gnc_numeric_convert(amt, get_currency_denom(s),
                                GNC_HOW_RND_ROUND);
  if (gnc_numeric_check(new_val) == GNC_ERROR_OK)
      s->value = new_val;
  else PERR("numeric error in converting the split value's denominator");

  SET_GAINS_VDIRTY(s);
  mark_split (s);
  qof_instance_set_dirty(QOF_INSTANCE(s));
  xaccTransCommitEdit(s->parent);
  LEAVE ("");
}

/********************************************************************\
\********************************************************************/

gnc_numeric 
xaccSplitGetBalance (const Split *s) 
{
   return s ? s->balance : gnc_numeric_zero();
}

gnc_numeric 
xaccSplitGetClearedBalance (const Split *s) 
{
   return s ? s->cleared_balance : gnc_numeric_zero();
}

gnc_numeric 
xaccSplitGetReconciledBalance (const Split *s)  
{
   return s ? s->reconciled_balance : gnc_numeric_zero();
}

void
xaccSplitSetBaseValue (Split *s, gnc_numeric value, 
                       const gnc_commodity * base_currency)
{
  const gnc_commodity *currency;
  const gnc_commodity *commodity;

  if (!s) return;
  xaccTransBeginEdit (s->parent);

  if (!s->acc) 
  {
    PERR ("split must have a parent account");
    return;
  }

  currency = xaccTransGetCurrency (s->parent);
  commodity = xaccAccountGetCommodity (s->acc);

  /* If the base_currency is the transaction's commodity ('currency'),
   * set the value.  If it's the account commodity, set the
   * amount. If both, set both. */
  if (gnc_commodity_equiv(currency, base_currency)) {
    if (gnc_commodity_equiv(commodity, base_currency)) {
      s->amount = gnc_numeric_convert(value,
                                      get_commodity_denom(s), 
                                      GNC_HOW_RND_NEVER);
    }
    s->value = gnc_numeric_convert(value, 
                                   get_currency_denom(s),
                                   GNC_HOW_RND_NEVER);
  }
  else if (gnc_commodity_equiv(commodity, base_currency)) {
    s->amount = gnc_numeric_convert(value, get_commodity_denom(s),
                                    GNC_HOW_RND_NEVER);
  }
  else {
    PERR ("inappropriate base currency %s "
          "given split currency=%s and commodity=%s\n",
          gnc_commodity_get_printname(base_currency), 
          gnc_commodity_get_printname(currency), 
          gnc_commodity_get_printname(commodity));
    return;
  }

  SET_GAINS_A_VDIRTY(s);
  mark_split (s);
  qof_instance_set_dirty(QOF_INSTANCE(s));
  xaccTransCommitEdit(s->parent);
}

gnc_numeric
xaccSplitGetBaseValue (const Split *s, const gnc_commodity * base_currency)
{
  if (!s || !s->acc || !s->parent) return gnc_numeric_zero();

  /* be more precise -- the value depends on the currency we want it
   * expressed in.  */
  if (gnc_commodity_equiv(xaccTransGetCurrency(s->parent), base_currency)) 
      return xaccSplitGetValue(s);
  if (gnc_commodity_equiv(xaccAccountGetCommodity(s->acc), base_currency)) 
      return xaccSplitGetAmount(s);

  PERR ("inappropriate base currency %s "
        "given split currency=%s and commodity=%s\n",
        gnc_commodity_get_printname(base_currency), 
        gnc_commodity_get_printname(xaccTransGetCurrency (s->parent)), 
        gnc_commodity_get_printname(xaccAccountGetCommodity(s->acc)));
  return gnc_numeric_zero();
}

/********************************************************************\
\********************************************************************/

gnc_numeric
xaccSplitsComputeValue (GList *splits, const Split * skip_me,
                        const gnc_commodity * base_currency)
{
  GList *node;
  gnc_numeric value = gnc_numeric_zero();

  g_return_val_if_fail (base_currency, value);

  ENTER (" currency=%s", gnc_commodity_get_mnemonic (base_currency));

  for (node = splits; node; node = node->next)
  {
    const Split *s = node->data;
    const gnc_commodity *currency;
    const gnc_commodity *commodity;

    if (s == skip_me) continue;

    /* value = gnc_numeric_add(value, xaccSplitGetBaseValue(s, base_currency),
       GNC_DENOM_AUTO, GNC_HOW_DENOM_LCD); */

    /* The split-editor often sends us 'temp' splits whose account
     * hasn't yet been set.  Be lenient, and assume an implied base
     * currency. If there's a problem later, the scrub routines will
     * pick it up.
     */
    commodity = s->acc ? xaccAccountGetCommodity (s->acc) : base_currency;
    currency = xaccTransGetCurrency (s->parent);

    
    if (gnc_commodity_equiv(currency, base_currency)) 
    {
      value = gnc_numeric_add(value, xaccSplitGetValue(s),
                              GNC_DENOM_AUTO, GNC_HOW_DENOM_LCD);
    }
    else if (gnc_commodity_equiv(commodity, base_currency)) 
    {
      value = gnc_numeric_add(value, xaccSplitGetAmount(s),
                              GNC_DENOM_AUTO, GNC_HOW_DENOM_LCD);
    }
    else {
      PERR ("inconsistent currencies\n"   
            "\tbase = '%s', curr='%s', sec='%s'\n",
             gnc_commodity_get_printname(base_currency),
             gnc_commodity_get_printname(currency),
             gnc_commodity_get_printname(commodity));
      g_return_val_if_fail (FALSE, value);
    }
  }

  /* Note that just because the currencies are equivalent
   * doesn't mean the denominators are the same! */
  value = gnc_numeric_convert(value,
                              gnc_commodity_get_fraction (base_currency),
                              GNC_HOW_RND_ROUND);

  LEAVE (" total=%" G_GINT64_FORMAT "/%" G_GINT64_FORMAT,
	 value.num, value.denom);
  return value;
}

gnc_numeric
xaccSplitConvertAmount (const Split *split, Account * account)
{
  gnc_commodity *acc_com, *to_commodity;
  Transaction *txn;
  gnc_numeric amount, value, convrate;
  Account * split_acc;

  amount = xaccSplitGetAmount (split);

  /* If this split is attached to this account, OR */
  split_acc = xaccSplitGetAccount (split);
  if (split_acc == account)
    return amount;

  /* If split->account->commodity == to_commodity, return the amount */
  acc_com = xaccAccountGetCommodity (split_acc);
  to_commodity = xaccAccountGetCommodity (account);
  if (acc_com && gnc_commodity_equal (acc_com, to_commodity))
    return amount;

  /* Ok, this split is not for the viewed account, and the commodity
   * does not match.  So we need to do some conversion.
   *
   * First, we can cheat.  If this transaction is balanced and has
   * exactly two splits, then we can implicitly determine the exchange
   * rate and just return the 'other' split amount.
   */
  txn = xaccSplitGetParent (split);
  if (txn && gnc_numeric_zero_p (xaccTransGetImbalance (txn))) {
    const Split *osplit = xaccSplitGetOtherSplit (split);

    if (osplit)
      return gnc_numeric_neg (xaccSplitGetAmount (osplit));
  }

  /* ... otherwise, we need to compute the amount from the conversion
   * rate into _this account_.  So, find the split into this account,
   * compute the conversion rate (based on amount/value), and then multiply
   * this times the split value.
   */
  convrate = xaccTransGetAccountConvRate(txn, account);
  value = xaccSplitGetValue (split);
  return gnc_numeric_mul (value, convrate,
			  gnc_commodity_get_fraction (to_commodity),
			  GNC_RND_ROUND);
}

/********************************************************************\
\********************************************************************/

gboolean
xaccSplitDestroy (Split *split)
{
   Account *acc;
   Transaction *trans;

   if (!split) return TRUE;

   acc = split->acc;
   trans = split->parent;
   if (acc && !acc->inst.do_free && xaccTransGetReadOnly (trans))
       return FALSE;

   xaccTransBeginEdit(trans);
   qof_instance_set_dirty(QOF_INSTANCE(split));
   split->inst.do_free = TRUE;
   xaccTransCommitEdit(trans);

   return TRUE;
}

/********************************************************************\
\********************************************************************/

int
xaccSplitDateOrder (const Split *sa, const Split *sb)
{
  int retval;
  int comp;
  char *da, *db;

  if (sa == sb) return 0;
  /* nothing is always less than something */
  if (!sa && sb) return -1;
  if (sa && !sb) return +1;

  retval = xaccTransOrder (sa->parent, sb->parent);
  if (retval) return retval;

  /* otherwise, sort on memo strings */
  da = sa->memo ? sa->memo : "";
  db = sb->memo ? sb->memo : "";
  retval = g_utf8_collate (da, db);
  if (retval)
    return retval;

  /* otherwise, sort on action strings */
  da = sa->action ? sa->action : "";
  db = sb->action ? sb->action : "";
  retval = g_utf8_collate (da, db);
  if (retval != 0)
    return retval;

  /* the reconciled flag ... */
  if (sa->reconciled < sb->reconciled) return -1;
  if (sa->reconciled > sb->reconciled) return +1;

  /* compare amounts */
  comp = gnc_numeric_compare(xaccSplitGetAmount(sa), xaccSplitGetAmount (sb));
  if (comp < 0) return -1;
  if (comp > 0) return +1;

  comp = gnc_numeric_compare(xaccSplitGetValue(sa), xaccSplitGetValue (sb));
  if (comp < 0) return -1;
  if (comp > 0) return +1;

  /* if dates differ, return */
  DATE_CMP(sa,sb,date_reconciled);

  /* else, sort on guid - keeps sort stable. */
  retval = guid_compare(&(sa->inst.entity.guid), &(sb->inst.entity.guid));
  if (retval) return retval;

  return 0;
}

static gboolean
get_corr_account_split(const Split *sa, const Split **retval)
{
 
  const Split *current_split;
  GList *node;
  gnc_numeric sa_value, current_value;
  gboolean sa_value_positive, current_value_positive, seen_different = FALSE;

  *retval = NULL;
  g_return_val_if_fail(sa, TRUE);
  
  sa_value = xaccSplitGetValue (sa);
  sa_value_positive = gnc_numeric_positive_p(sa_value);

  for (node = sa->parent->splits; node; node = node->next)
  {
    current_split = node->data;
    if (current_split == sa) continue;

    if (!xaccTransStillHasSplit(sa->parent, current_split)) continue;
    current_value = xaccSplitGetValue (current_split);
    current_value_positive = gnc_numeric_positive_p(current_value);
    if ((sa_value_positive && !current_value_positive) || 
        (!sa_value_positive && current_value_positive)) {
        if (seen_different) {
            *retval = NULL;
            return TRUE;
        } else {
            *retval = current_split;
            seen_different = TRUE;
        }
    }
  }
  return FALSE;
}

/* TODO: these static consts can be shared. */
const char *
xaccSplitGetCorrAccountName(const Split *sa)
{
  static const char *split_const = NULL;
  const Split *other_split;

  if (get_corr_account_split(sa, &other_split))
  {
    if (!split_const)
      split_const = _("-- Split Transaction --");

    return split_const;
  }

  return xaccAccountGetName(other_split->acc);
}

char *
xaccSplitGetCorrAccountFullName(const Split *sa)
{
  static const char *split_const = NULL;
  const Split *other_split;

  if (get_corr_account_split(sa, &other_split))
  {
    if (!split_const)
      split_const = _("-- Split Transaction --");

    return g_strdup(split_const);
  }
  return xaccAccountGetFullName(other_split->acc);
}

const char *
xaccSplitGetCorrAccountCode(const Split *sa)
{
  static const char *split_const = NULL;
  const Split *other_split;

  if (get_corr_account_split(sa, &other_split))
  {
    if (!split_const)
      split_const = _("Split");

    return split_const;
  }
  return xaccAccountGetCode(other_split->acc);
}

/* TODO: It's not too hard to make this function avoid the malloc/free. */
int 
xaccSplitCompareAccountFullNames(const Split *sa, const Split *sb)
{
  Account *aa, *ab;
  char *full_a, *full_b;
  int retval;
  if (!sa && !sb) return 0;
  if (!sa) return -1;
  if (!sb) return 1;

  aa = sa->acc;
  ab = sb->acc;
  full_a = xaccAccountGetFullName(aa);
  full_b = xaccAccountGetFullName(ab);
  retval = g_utf8_collate(full_a, full_b);
  g_free(full_a);
  g_free(full_b);
  return retval;
}


int 
xaccSplitCompareAccountCodes(const Split *sa, const Split *sb)
{
  Account *aa, *ab;
  if (!sa && !sb) return 0;
  if (!sa) return -1;
  if (!sb) return 1;

  aa = sa->acc;
  ab = sb->acc;
  
  return safe_strcmp(xaccAccountGetName(aa), xaccAccountGetName(ab));
}

int 
xaccSplitCompareOtherAccountFullNames(const Split *sa, const Split *sb)
{
  char *ca, *cb; 
  int retval;
  if (!sa && !sb) return 0;
  if (!sa) return -1;
  if (!sb) return 1;

  /* doesn't matter what separator we use
   * as long as they are the same 
   */

  ca = xaccSplitGetCorrAccountFullName(sa);
  cb = xaccSplitGetCorrAccountFullName(sb);
  retval = safe_strcmp(ca, cb);
  g_free(ca);
  g_free(cb);
  return retval;
}

int
xaccSplitCompareOtherAccountCodes(const Split *sa, const Split *sb)
{
  const char *ca, *cb;
  if (!sa && !sb) return 0;
  if (!sa) return -1;
  if (!sb) return 1;

  ca = xaccSplitGetCorrAccountCode(sa);
  cb = xaccSplitGetCorrAccountCode(sb);
  return safe_strcmp(ca, cb);
}

static void
qofSplitSetMemo (Split *split, const char* memo)
{
    g_return_if_fail(split);
    CACHE_REPLACE(split->memo, memo);
}

void
xaccSplitSetMemo (Split *split, const char *memo)
{
   if (!split || !memo) return;
   xaccTransBeginEdit (split->parent);

   CACHE_REPLACE(split->memo, memo);
   qof_instance_set_dirty(QOF_INSTANCE(split));
   xaccTransCommitEdit(split->parent);

}

static void
qofSplitSetAction (Split *split, const char *actn)
{
	g_return_if_fail(split);
        CACHE_REPLACE(split->action, actn);
}

void
xaccSplitSetAction (Split *split, const char *actn)
{
   if (!split || !actn) return;
   xaccTransBeginEdit (split->parent);

   CACHE_REPLACE(split->action, actn);
   qof_instance_set_dirty(QOF_INSTANCE(split));
   xaccTransCommitEdit(split->parent);

}

static void
qofSplitSetReconcile (Split *split, char recn)
{
	g_return_if_fail(split);
	switch (recn)
	{
		case NREC:
		case CREC:
		case YREC:
		case FREC:
		case VREC:
			split->reconciled = recn;
			mark_split (split);
			xaccAccountRecomputeBalance (split->acc);
			break;
		default:
			PERR("Bad reconciled flag");
	}
}

void
xaccSplitSetReconcile (Split *split, char recn)
{
   if (!split || split->reconciled == recn) return;
   xaccTransBeginEdit (split->parent);

   switch (recn)
   {
   case NREC:
   case CREC:
   case YREC:
   case FREC:
   case VREC: 
     split->reconciled = recn;
     mark_split (split);
     qof_instance_set_dirty(QOF_INSTANCE(split));
     xaccAccountRecomputeBalance (split->acc);
     break;
   default:
     PERR("Bad reconciled flag");
   }
   xaccTransCommitEdit(split->parent);

}

void
xaccSplitSetDateReconciledSecs (Split *split, time_t secs)
{
   if (!split) return;
   xaccTransBeginEdit (split->parent);

   split->date_reconciled.tv_sec = secs;
   split->date_reconciled.tv_nsec = 0;
   qof_instance_set_dirty(QOF_INSTANCE(split));
   xaccTransCommitEdit(split->parent);

}

void
xaccSplitSetDateReconciledTS (Split *split, Timespec *ts)
{
   if (!split || !ts) return;
   xaccTransBeginEdit (split->parent);

   split->date_reconciled = *ts;
   qof_instance_set_dirty(QOF_INSTANCE(split));
   xaccTransCommitEdit(split->parent);

}

void
xaccSplitGetDateReconciledTS (const Split * split, Timespec *ts)
{
   if (!split || !ts) return;
   *ts = (split->date_reconciled);
}

Timespec
xaccSplitRetDateReconciledTS (const Split * split)
{
   Timespec ts = {0,0};
   return split ? split->date_reconciled : ts;
}

/********************************************************************\
\********************************************************************/

/* return the parent transaction of the split */
Transaction * 
xaccSplitGetParent (const Split *split)
{
   return split ? split->parent : NULL;
}

void
xaccSplitSetParent(Split *s, Transaction *t)
{
    Transaction *old_trans;
    g_return_if_fail(s && t);
    if (s->parent == t) return;

    if (s->parent != s->orig_parent)
        PERR("You may not add the split to more than one transaction"
             " during the BeginEdit/CommitEdit block.");
    xaccTransBeginEdit(t);
    old_trans = s->parent;
    xaccTransBeginEdit(old_trans);
    s->parent = t;
    qof_instance_set_dirty(QOF_INSTANCE(s));
    /* Convert split to new transaction's commodity denominator */
    xaccSplitSetValue(s, xaccSplitGetValue(s));

    /* add ourselves to the new transaction's list of pending splits. */
    if (NULL == g_list_find(t->splits, s))
        t->splits = g_list_append(t->splits, s);
    xaccTransCommitEdit(old_trans);
    xaccTransCommitEdit(t);
}


GNCLot *
xaccSplitGetLot (const Split *split)
{
   return split ? split->lot : NULL;
}

const char *
xaccSplitGetMemo (const Split *split)
{
   return split ? split->memo : NULL;
}

const char *
xaccSplitGetAction (const Split *split)
{
   return split ? split->action : NULL;
}

char 
xaccSplitGetReconcile (const Split *split) 
{
   return split ? split->reconciled : ' ';
}


gnc_numeric
xaccSplitGetAmount (const Split * split)
{
   return split ? split->amount : gnc_numeric_zero();
}

gnc_numeric
xaccSplitGetValue (const Split * split) 
{
   return split ? split->value : gnc_numeric_zero();
}

gnc_numeric
xaccSplitGetSharePrice (const Split * split) 
{
  gnc_numeric amt, val, price;
  if (!split) return gnc_numeric_create(1, 1);
  

  /* if amount == 0 and value == 0, then return 1.
   * if amount == 0 and value != 0 then return 0.
   * otherwise return value/amount
   */

  amt = xaccSplitGetAmount(split);
  val = xaccSplitGetValue(split);
  if (gnc_numeric_zero_p(amt))
  {
    if (gnc_numeric_zero_p(val))
      return gnc_numeric_create(1, 1);
    return gnc_numeric_create(0, 1);
  }
  price = gnc_numeric_div(val, amt,
                         GNC_DENOM_AUTO, 
                         GNC_HOW_DENOM_SIGFIGS(PRICE_SIGFIGS) |
                         GNC_HOW_RND_ROUND);

  /* During random checks we can get some very weird prices.  Let's
   * handle some overflow and other error conditions by returning
   * zero.  But still print an error to let us know it happened.
   */
  if (gnc_numeric_check(price)) 
  {
    PERR("Computing share price failed (%d): [ %" G_GINT64_FORMAT " / %"
	 G_GINT64_FORMAT " ] / [ %" G_GINT64_FORMAT " / %" G_GINT64_FORMAT " ]",
	 gnc_numeric_check(price), val.num, val.denom, amt.num, amt.denom);
    return gnc_numeric_create(0,1);
  }

  return price;
}

/********************************************************************\
\********************************************************************/

QofBook *
xaccSplitGetBook (const Split *split)
{
    return qof_instance_get_book(QOF_INSTANCE(split));
}

const char *
xaccSplitGetType(const Split *s)
{
  char *split_type;

  if (!s) return NULL;
  split_type = kvp_frame_get_string(s->inst.kvp_data, "split-type");
  return split_type ? split_type : "normal";
}

/* reconfigure a split to be a stock split - after this, you shouldn't
   mess with the value, just the amount. */
void
xaccSplitMakeStockSplit(Split *s)
{
  xaccTransBeginEdit (s->parent);

  s->value = gnc_numeric_zero();
  kvp_frame_set_str(s->inst.kvp_data, "split-type", "stock-split");
  SET_GAINS_VDIRTY(s);
  mark_split(s);
  qof_instance_set_dirty(QOF_INSTANCE(s));
  xaccTransCommitEdit(s->parent);
}


/********************************************************************\
\********************************************************************/
/* In the old world, the 'other split' was the other split of a
 * transaction that contained only two splits.  In the new world,
 * a split may have been cut up between multiple lots, although
 * in a conceptual sense, if lots hadn't been used, there would be
 * only a pair.  So we handle this conceptual case: we can still
 * identify, unambiguously, the 'other' split when 'this' split
 * as been cut up across lots.  We do this by looking for the 
 * 'lot-split' keyword, which occurs only in cut-up splits.
 */

Split *
xaccSplitGetOtherSplit (const Split *split)
{
  int i;
  Transaction *trans;
  int count, num_splits;
  Split *other = NULL;
  KvpValue *sva;

  if (!split) return NULL;
  trans = split->parent;
  if (!trans) return NULL;

#ifdef OLD_ALGO_HAS_ONLY_TWO_SPLITS
  Split *s1, *s2;
  if (g_list_length (trans->splits) != 2) return NULL;

  s1 = g_list_nth_data (trans->splits, 0);
  s2 = g_list_nth_data (trans->splits, 1);

  if (s1 == split) return s2;
  return s1;
#endif

  num_splits = xaccTransCountSplits(trans);
  count = num_splits;
  sva = kvp_frame_get_slot (split->inst.kvp_data, "lot-split");
  if (!sva && (2 != count)) return NULL;

  for (i = 0; i < num_splits; i++) {
      Split *s = xaccTransGetSplit(trans, i);
      if (s == split) { --count; continue; }
      if (kvp_frame_get_slot (s->inst.kvp_data, "lot-split")) 
          { --count; continue; }
      other = s;
  }
  return (1 == count) ? other : NULL;
}

/********************************************************************\
\********************************************************************/

gboolean
xaccIsPeerSplit (const Split *sa, const Split *sb)
{
   return (sa && sb && (sa->parent == sb->parent));
}

gnc_numeric
xaccSplitVoidFormerAmount(const Split *split)
{
  g_return_val_if_fail(split, gnc_numeric_zero());
  return kvp_frame_get_numeric(split->inst.kvp_data, void_former_amt_str);
}

gnc_numeric
xaccSplitVoidFormerValue(const Split *split)
{
  g_return_val_if_fail(split, gnc_numeric_zero());
  return kvp_frame_get_numeric(split->inst.kvp_data, void_former_val_str);
}

void
xaccSplitVoid(Split *split)
{
    gnc_numeric zero = gnc_numeric_zero();
    KvpFrame *frame = split->inst.kvp_data;

    kvp_frame_set_gnc_numeric(frame, void_former_amt_str, 
                              xaccSplitGetAmount(split));
    kvp_frame_set_gnc_numeric(frame, void_former_val_str, 
                              xaccSplitGetValue(split));

    xaccSplitSetAmount (split, zero);
    xaccSplitSetValue (split, zero);
    xaccSplitSetReconcile(split, VREC);

}

void
xaccSplitUnvoid(Split *split)
{
    KvpFrame *frame = split->inst.kvp_data;
    
    xaccSplitSetAmount (split, xaccSplitVoidFormerAmount(split));
    xaccSplitSetValue (split, xaccSplitVoidFormerValue(split));
    xaccSplitSetReconcile(split, NREC);
    kvp_frame_set_slot(frame, void_former_amt_str, NULL);
    kvp_frame_set_slot(frame, void_former_val_str, NULL);
}

/********************************************************************\
\********************************************************************/
/* QofObject function implementation */

/* Hook into the QofObject registry */

static QofObject split_object_def = {
  interface_version: QOF_OBJECT_VERSION,
  e_type:            GNC_ID_SPLIT,
  type_label:        "Split",
  create:            (gpointer)xaccMallocSplit,
  book_begin:        NULL,
  book_end:          NULL,
  is_dirty:          qof_collection_is_dirty,
  mark_clean:        qof_collection_mark_clean,
  foreach:           qof_collection_foreach,
  printable:         (const char* (*)(gpointer)) xaccSplitGetMemo,
  version_cmp:       (int (*)(gpointer, gpointer)) qof_instance_version_cmp,
};

static gpointer 
split_account_guid_getter (gpointer obj, const QofParam *p)
{
  Split *s = obj;
  Account *acc;

  if (!s) return NULL;
  acc = xaccSplitGetAccount (s);
  if (!acc) return NULL;
  return ((gpointer)xaccAccountGetGUID (acc));
}

static double    /* internal use only */
DxaccSplitGetShareAmount (const Split * split) 
{
  return split ? gnc_numeric_to_double(xaccSplitGetAmount(split)) : 0.0;
}

static gpointer 
no_op (gpointer obj, const QofParam *p)
{
  return obj;
}

static void
qofSplitSetParentTrans(Split *s, QofEntity *ent)
{
    Transaction *trans = (Transaction*)ent;

    g_return_if_fail(trans);
    xaccSplitSetParent(s, trans);
}

static void
qofSplitSetAccount(Split *s, QofEntity *ent)
{
    Account *acc = (Account*)ent;

    g_return_if_fail(acc);
    xaccSplitSetAccount(s, acc);
}

gboolean xaccSplitRegister (void)
{
  static const QofParam params[] = {
    { SPLIT_DATE_RECONCILED, QOF_TYPE_DATE,
      (QofAccessFunc)xaccSplitRetDateReconciledTS, 	
      (QofSetterFunc)xaccSplitSetDateReconciledTS },

    /* d-* are deprecated query params, should not be used in new
     * queries, should be removed from old queries. */
    { "d-share-amount", QOF_TYPE_DOUBLE,  
      (QofAccessFunc)DxaccSplitGetShareAmount, NULL },
    { "d-share-int64", QOF_TYPE_INT64, 
      (QofAccessFunc)qof_entity_get_guid, NULL },
    { SPLIT_BALANCE, QOF_TYPE_NUMERIC, 
      (QofAccessFunc)xaccSplitGetBalance, NULL },
    { SPLIT_CLEARED_BALANCE, QOF_TYPE_NUMERIC,
      (QofAccessFunc)xaccSplitGetClearedBalance, NULL },
    { SPLIT_RECONCILED_BALANCE, QOF_TYPE_NUMERIC,
      (QofAccessFunc)xaccSplitGetReconciledBalance, NULL },
    { SPLIT_MEMO, QOF_TYPE_STRING, 
      (QofAccessFunc)xaccSplitGetMemo, (QofSetterFunc)qofSplitSetMemo },
    { SPLIT_ACTION, QOF_TYPE_STRING, 
      (QofAccessFunc)xaccSplitGetAction, (QofSetterFunc)qofSplitSetAction },
    { SPLIT_RECONCILE, QOF_TYPE_CHAR, 
      (QofAccessFunc)xaccSplitGetReconcile, 
      (QofSetterFunc)qofSplitSetReconcile },
    { SPLIT_AMOUNT, QOF_TYPE_NUMERIC, 
      (QofAccessFunc)xaccSplitGetAmount, (QofSetterFunc)qofSplitSetAmount },
    { SPLIT_SHARE_PRICE, QOF_TYPE_NUMERIC,
      (QofAccessFunc)xaccSplitGetSharePrice, 
      (QofSetterFunc)qofSplitSetSharePrice },
    { SPLIT_VALUE, QOF_TYPE_DEBCRED, 
      (QofAccessFunc)xaccSplitGetValue, (QofSetterFunc)qofSplitSetValue },
    { SPLIT_TYPE, QOF_TYPE_STRING, (QofAccessFunc)xaccSplitGetType, NULL },
    { SPLIT_VOIDED_AMOUNT, QOF_TYPE_NUMERIC, 
      (QofAccessFunc)xaccSplitVoidFormerAmount, NULL },
    { SPLIT_VOIDED_VALUE, QOF_TYPE_NUMERIC,  
      (QofAccessFunc)xaccSplitVoidFormerValue, NULL },
    { SPLIT_LOT, GNC_ID_LOT, (QofAccessFunc)xaccSplitGetLot, NULL },
    { SPLIT_TRANS, GNC_ID_TRANS,     
      (QofAccessFunc)xaccSplitGetParent, 
      (QofSetterFunc)qofSplitSetParentTrans },
    { SPLIT_ACCOUNT, GNC_ID_ACCOUNT,
      (QofAccessFunc)xaccSplitGetAccount, (QofSetterFunc)qofSplitSetAccount },
    { SPLIT_ACCOUNT_GUID, QOF_TYPE_GUID, split_account_guid_getter, NULL },
/*  these are no-ops to register the parameter names (for sorting) but
    they return an allocated object which getters cannot do.  */
    { SPLIT_ACCT_FULLNAME, SPLIT_ACCT_FULLNAME, no_op, NULL },
    { SPLIT_CORR_ACCT_NAME, SPLIT_CORR_ACCT_NAME, no_op, NULL },
    { SPLIT_CORR_ACCT_CODE, SPLIT_CORR_ACCT_CODE, no_op, NULL },
    { SPLIT_KVP, QOF_TYPE_KVP, (QofAccessFunc)xaccSplitGetSlots, NULL },
    { QOF_PARAM_BOOK, QOF_ID_BOOK, (QofAccessFunc)xaccSplitGetBook, NULL },
    { QOF_PARAM_GUID, QOF_TYPE_GUID, 
      (QofAccessFunc)qof_entity_get_guid, NULL },
    { NULL },
  };

  qof_class_register (GNC_ID_SPLIT, (QofSortFunc)xaccSplitDateOrder, params);
  qof_class_register (SPLIT_ACCT_FULLNAME,
                      (QofSortFunc)xaccSplitCompareAccountFullNames, NULL);
  qof_class_register (SPLIT_CORR_ACCT_NAME,
                      (QofSortFunc)xaccSplitCompareOtherAccountFullNames,
                          NULL);
  qof_class_register (SPLIT_CORR_ACCT_CODE,
                      (QofSortFunc)xaccSplitCompareOtherAccountCodes, NULL);

  return qof_object_register (&split_object_def);
}

/************************ END OF ************************************\
\************************* FILE *************************************/
