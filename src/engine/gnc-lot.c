/********************************************************************\
 * gnc-lot.c -- AR/AP invoices; inventory lots; stock lots          *
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

/*
 * FILE:
 * gnc-lot.c
 * 
 * FUNCTION:
 * Lots implement the fundamental conceptual idea behind invoices,
 * inventory lots, and stock market investment lots.  See the file
 * src/doc/lots.txt for implmentation overview.
 *
 * XXX Lots are not currently treated in a correct transactional
 * manner.  There's now a per-Lot dirty flag in the QofInstance, but
 * this code still needs to emit the correct signals when a lot has
 * changed.  This is true both in the Scrub2.c and in
 * src/gnome/lot-viewer.c
 *
 * HISTORY:
 * Created by Linas Vepstas May 2002
 * Copyright (c) 2002,2003 Linas Vepstas <linas@linas.org>
 */

#include "Account.h"
#include "gnc-lot.h"
#include "gnc-lot-p.h"
#include "Transaction.h"
#include "TransactionP.h"

/* This static indicates the debugging module that this .o belongs to.  */
static QofLogModule log_module = GNC_MOD_LOT;

/* ============================================================= */

static void
gnc_lot_init (GNCLot *lot, QofBook *book)
{
   ENTER ("(lot=%p, book=%p)", lot, book);
   lot->account = NULL;
   lot->splits = NULL;
   lot->is_closed = -1;
   lot->marker = 0;
  
   qof_instance_init(&lot->inst, GNC_ID_LOT, book);
   LEAVE ("(lot=%p, book=%p)", lot, book);
}

GNCLot *
gnc_lot_new (QofBook *book)
{
   GNCLot *lot;
   g_return_val_if_fail (book, NULL);

   lot = g_new (GNCLot, 1);
   gnc_lot_init (lot, book);
   gnc_engine_gen_event (&lot->inst.entity, GNC_EVENT_CREATE);
   return lot;
}

void 
gnc_lot_destroy (GNCLot *lot)
{
   GList *node;
   if (!lot) return;
   
   ENTER ("(lot=%p)", lot);
   gnc_engine_gen_event (&lot->inst.entity, GNC_EVENT_DESTROY);

   
   for (node=lot->splits; node; node=node->next)
   {
      Split *s = node->data;
      s->lot = NULL;
   }
   g_list_free (lot->splits);
   
   lot->account = NULL;
   lot->is_closed = TRUE;
   qof_instance_release (&lot->inst);
   g_free (lot);
}

/* ============================================================= */

GNCLot *
gnc_lot_lookup (const GUID *guid, QofBook *book)
{
  QofCollection *col;
  if (!guid || !book) return NULL;
  col = qof_book_get_collection (book, GNC_ID_LOT);
  return (GNCLot *) qof_collection_lookup_entity (col, guid);
}

QofBook *
gnc_lot_get_book (GNCLot *lot)
{
    return qof_instance_get_book(QOF_INSTANCE(lot));
}

/* ============================================================= */

gboolean 
gnc_lot_is_closed (GNCLot *lot)
{
   if (!lot) return TRUE;
   if (0 > lot->is_closed) gnc_lot_get_balance (lot);
   return lot->is_closed;
}

Account *
gnc_lot_get_account (GNCLot *lot)
{
   if (!lot) return NULL;
   return lot->account;
}

KvpFrame *
gnc_lot_get_slots (GNCLot *lot)
{
    return qof_instance_get_slots(QOF_INSTANCE(lot));
}

SplitList *
gnc_lot_get_split_list (GNCLot *lot)
{
   if (!lot) return NULL;
   return lot->splits;
}

gint gnc_lot_count_splits (GNCLot *lot)
{
   if (!lot) return 0;
   return g_list_length (lot->splits);
}

/* ============================================================== */
/* Hmm, we should probably inline these. */

const char * 
gnc_lot_get_title (GNCLot *lot)
{
   if (!lot) return NULL;
   return kvp_frame_get_string (lot->inst.kvp_data, "/title");
}

const char * 
gnc_lot_get_notes (GNCLot *lot)
{
   if (!lot) return NULL;
   return kvp_frame_get_string (lot->inst.kvp_data, "/notes");
}

void
gnc_lot_set_title (GNCLot *lot, const char *str)
{
   if (!lot) return;
   qof_instance_set_dirty(QOF_INSTANCE(lot));
   return kvp_frame_set_str (lot->inst.kvp_data, "/title", str);
}

void
gnc_lot_set_notes (GNCLot *lot, const char *str)
{
   if (!lot) return;
   qof_instance_set_dirty(QOF_INSTANCE(lot));
   return kvp_frame_set_str (lot->inst.kvp_data, "/notes", str);
}

/* ============================================================= */

gnc_numeric
gnc_lot_get_balance (GNCLot *lot)
{
   GList *node;
   gnc_numeric zero = gnc_numeric_zero();
   gnc_numeric baln = zero;
   if (!lot) return zero;

   if (!lot->splits) 
   {
      lot->is_closed = FALSE;
      return zero;
   }

   /* Sum over splits; because they all belong to same account
    * they will have same denominator. 
    */
   for (node=lot->splits; node; node=node->next)
   {
      Split *s = node->data;
      gnc_numeric amt = xaccSplitGetAmount (s);
      baln = gnc_numeric_add_fixed (baln, amt);
   }

   /* cache a zero balance as a closed lot */
   if (gnc_numeric_equal (baln, zero))
   {
      lot->is_closed = TRUE;
   }
   else
   {
      lot->is_closed = FALSE;
   }
   
   return baln;
}

/* ============================================================= */

void
gnc_lot_add_split (GNCLot *lot, Split *split)
{
   Account * acc;
   if (!lot || !split) return;

   ENTER ("(lot=%p, split=%p) %s amt=%s val=%s", lot, split,
        gnc_lot_get_title (lot), 
        gnc_num_dbg_to_string (split->amount),
        gnc_num_dbg_to_string (split->value));
   acc = xaccSplitGetAccount (split);
   qof_instance_set_dirty(QOF_INSTANCE(lot));
   if (NULL == lot->account)
   {
      xaccAccountInsertLot (acc, lot);
   }
   else if (lot->account != acc)
   {
      PERR ("splits from different accounts cannot "
            "be added to this lot!\n"
            "\tlot account=\'%s\', split account=\'%s\'\n",
            xaccAccountGetName(lot->account), xaccAccountGetName (acc));
      return;
   }

   if (lot == split->lot) return; /* handle not-uncommon no-op */
   if (split->lot)
   {
      gnc_lot_remove_split (split->lot, split);
   }
   split->lot = lot;

   lot->splits = g_list_append (lot->splits, split);

    /* for recomputation of is-closed */
   lot->is_closed = -1;

   gnc_engine_gen_event (&lot->inst.entity, GNC_EVENT_MODIFY);
}

void
gnc_lot_remove_split (GNCLot *lot, Split *split)
{
   if (!lot || !split) return;

   ENTER ("(lot=%p, split=%p)", lot, split);
   qof_instance_set_dirty(QOF_INSTANCE(lot));
   lot->splits = g_list_remove (lot->splits, split);
   split->lot = NULL;
   lot->is_closed = -1;   /* force an is-closed computation */

   if (NULL == lot->splits)
   {
      xaccAccountRemoveLot (lot->account, lot);
      lot->account = NULL;
   }
   gnc_engine_gen_event (&lot->inst.entity, GNC_EVENT_MODIFY);
}

/* ============================================================== */
/* Utility function, get earliest split in lot */

Split *
gnc_lot_get_earliest_split (GNCLot *lot)
{
   SplitList *node;
   Timespec ts;
   Split *earliest = NULL;

   ts.tv_sec = ((long long) LONG_MAX);
   ts.tv_nsec = 0;
   if (!lot) return NULL;

   for (node=lot->splits; node; node=node->next)
   {
      Split *s = node->data;
      Transaction *trans = s->parent;
      if (!trans) continue;
      if ((ts.tv_sec > trans->date_posted.tv_sec) ||
          ((ts.tv_sec == trans->date_posted.tv_sec) &&
           (ts.tv_nsec > trans->date_posted.tv_nsec)))
          
      {
         ts = trans->date_posted;
         earliest = s;
      }
   }

   return earliest;
}

Split *
gnc_lot_get_latest_split (GNCLot *lot)
{
   SplitList *node;
   Timespec ts;
   Split *latest = NULL;

   ts.tv_sec = -((long long) LONG_MAX);
   ts.tv_nsec = 0;
   if (!lot) return NULL;

   for (node=lot->splits; node; node=node->next)
   {
      Split *s = node->data;
      Transaction *trans = s->parent;
      if (!trans) continue;
      if ((ts.tv_sec < trans->date_posted.tv_sec) ||
          ((ts.tv_sec == trans->date_posted.tv_sec) &&
           (ts.tv_nsec < trans->date_posted.tv_nsec)))
          
      {
         ts = trans->date_posted;
         latest = s;
      }
   }

   return latest;
}

/* ============================================================= */

static QofObject gncLotDesc =
{
    interface_version:  QOF_OBJECT_VERSION,
    e_type:             GNC_ID_LOT,
    type_label:         "Lot",
    create:             (gpointer)gnc_lot_new,
    book_begin:         NULL,
    book_end:           NULL,
    is_dirty:           qof_collection_is_dirty,
    mark_clean:         qof_collection_mark_clean,
    foreach:            qof_collection_foreach,
    printable:          NULL,
    version_cmp:        (int (*)(gpointer,gpointer))qof_instance_version_cmp,
};


gboolean gnc_lot_register (void)
{
    static const QofParam params[] = {
        { LOT_TITLE, QOF_TYPE_STRING, 
          (QofAccessFunc) gnc_lot_get_title, 
          (QofSetterFunc) gnc_lot_set_title },
        { LOT_NOTES, QOF_TYPE_STRING, 
          (QofAccessFunc) gnc_lot_get_notes, 
          (QofSetterFunc) gnc_lot_set_notes },
        { QOF_PARAM_GUID, QOF_TYPE_GUID, 
          (QofAccessFunc) qof_entity_get_guid, NULL },
        { QOF_PARAM_BOOK, QOF_ID_BOOK, 
          (QofAccessFunc) gnc_lot_get_book, NULL },
        { LOT_IS_CLOSED, QOF_TYPE_BOOLEAN, 
          (QofAccessFunc) gnc_lot_is_closed, NULL },
        { LOT_BALANCE, QOF_TYPE_NUMERIC, 
          (QofAccessFunc) gnc_lot_get_balance, NULL },
        { NULL },
    };

    qof_class_register (GNC_ID_LOT, NULL, params);
    return qof_object_register(&gncLotDesc);
}

/* ========================== END OF FILE ========================= */
