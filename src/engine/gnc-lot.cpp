/********************************************************************\
 * gnc-lot.cpp -- AR/AP invoices; inventory lots; stock lots        *
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
 * src/doc/lots.txt for implementation overview.
 *
 * XXX Lots are not currently treated in a correct transactional
 * manner.  There's now a per-Lot dirty flag in the QofInstance, but
 * this code still needs to emit the correct signals when a lot has
 * changed.  This is true both in the Scrub2.cpp and in
 * src/gnome/dialog-lot-viewer.cpp
 *
 * HISTORY:
 * Created by Linas Vepstas May 2002
 * Copyright (c) 2002,2003 Linas Vepstas <linas@linas.org>
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "Account.h"
#include "AccountP.h"
#include "gnc-lot.h"
#include "gnc-lot-p.h"
#include "cap-gains.h"
#include "Transaction.h"
#include "TransactionP.h"

/* This static indicates the debugging module that this .o belongs to.  */
static QofLogModule log_module = GNC_MOD_LOT;


struct LotPrivate
{
    /* Account to which this lot applies.  All splits in the lot must
     * belong to this account.
     */
    Account * account;

    /* List of splits that belong to this lot. */
    SplitList_t splits;

    /* Handy cached value to indicate if lot is closed. */
    /* If value is negative, then the cache is invalid. */
    signed char is_closed;
#define LOT_CLOSED_UNKNOWN (-1)

    /* traversal marker, handy for preventing recursion */
    unsigned char marker;
};

#define GET_PRIVATE(o) \
    (o->priv)

#define gnc_lot_set_guid(L,G)  qof_instance_set_guid(QOF_INSTANCE(L),&(G))

/* ============================================================= */

/* GObject Initialization */
//G_DEFINE_TYPE(GNCLot, gnc_lot, QOF_TYPE_INSTANCE)
//

GNCLot::GNCLot()
{
    priv = new LotPrivate;
    priv->account = NULL;
    priv->is_closed = LOT_CLOSED_UNKNOWN;
    priv->marker = 0;
}

GNCLot::~GNCLot()
{
    delete priv;
}



GNCLot *
gnc_lot_new (QofBook *book)
{
    GNCLot *lot;
    g_return_val_if_fail (book, NULL);

    lot = new GNCLot; //g_object_new (GNC_TYPE_LOT, NULL);
    qof_instance_init_data(QOF_INSTANCE(lot), GNC_ID_LOT, book);
    qof_event_gen (QOF_INSTANCE(lot), QOF_EVENT_CREATE, NULL);
    return lot;
}

static void
gnc_lot_free(GNCLot* lot)
{
    LotPrivate* priv;
    if (!lot) return;

    ENTER ("(lot=%p)", lot);
    qof_event_gen (QOF_INSTANCE(lot), QOF_EVENT_DESTROY, NULL);

    priv = GET_PRIVATE(lot);
    for (SplitList_t::iterator node = priv->splits.begin();
            node != priv->splits.end(); node++)
    {
        Split *s = *node;
        s->lot = NULL;
    }

    priv->account = NULL;
    priv->is_closed = TRUE;
    /* qof_instance_release (&lot->inst); */
//    g_object_unref (lot);
    delete lot;
}

void
gnc_lot_destroy (GNCLot *lot)
{
    if (!lot) return;

    gnc_lot_begin_edit(lot);
    qof_instance_set_destroying(lot, TRUE);
    gnc_lot_commit_edit(lot);
}

/* ============================================================= */

void
gnc_lot_begin_edit (GNCLot *lot)
{
    qof_begin_edit(QOF_INSTANCE(lot));
}

static void commit_err (QofInstance *inst, QofBackendError errcode)
{
    PERR ("Failed to commit: %d", errcode);
    gnc_engine_signal_commit_error( errcode );
}

static void lot_free(QofInstance* inst)
{
    GNCLot* lot = dynamic_cast<GNCLot*>(inst);

    gnc_lot_free(lot);
}

static void noop (QofInstance *inst) {}

void
gnc_lot_commit_edit (GNCLot *lot)
{
    if (!qof_commit_edit (QOF_INSTANCE(lot))) return;
    qof_commit_edit_part2 (QOF_INSTANCE(lot), commit_err, noop, lot_free);
}

/* ============================================================= */

GNCLot *
gnc_lot_lookup (const GncGUID *guid, QofBook *book)
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

bool
gnc_lot_is_closed (GNCLot *lot)
{
    LotPrivate* priv;
    if (!lot) return TRUE;
    priv = GET_PRIVATE(lot);
    if (0 > priv->is_closed) gnc_lot_get_balance (lot);
    return priv->is_closed;
}

Account *
gnc_lot_get_account (const GNCLot *lot)
{
    LotPrivate* priv;
    if (!lot) return NULL;
    priv = GET_PRIVATE(lot);
    return priv->account;
}

void
gnc_lot_set_account(GNCLot* lot, Account* account)
{
    if (lot != NULL)
    {
        LotPrivate* priv;
        priv = GET_PRIVATE(lot);
        priv->account = account;
    }
}

void
gnc_lot_set_closed_unknown(GNCLot* lot)
{
    LotPrivate* priv;
    if (lot != NULL)
    {
        priv = GET_PRIVATE(lot);
        priv->is_closed = LOT_CLOSED_UNKNOWN;
    }
}

KvpFrame *
gnc_lot_get_slots (const GNCLot *lot)
{
    return qof_instance_get_slots(QOF_INSTANCE(lot));
}

SplitList_t
gnc_lot_get_split_list (const GNCLot *lot)
{
    LotPrivate* priv;
    if (!lot) return SplitList_t();
    priv = GET_PRIVATE(lot);
    return priv->splits;
}

int gnc_lot_count_splits (const GNCLot *lot)
{
    LotPrivate* priv;
    if (!lot) return 0;
    priv = GET_PRIVATE(lot);
    return priv->splits.size();
}

/* ============================================================== */
/* Hmm, we should probably inline these. */

const char *
gnc_lot_get_title (const GNCLot *lot)
{
    if (!lot) return NULL;
    return kvp_frame_get_string (gnc_lot_get_slots(lot), "/title");
}

const char *
gnc_lot_get_notes (const GNCLot *lot)
{
    if (!lot) return NULL;
    return kvp_frame_get_string (gnc_lot_get_slots(lot), "/notes");
}

void
gnc_lot_set_title (GNCLot *lot, const char *str)
{
    if (!lot) return;
    qof_begin_edit(QOF_INSTANCE(lot));
    qof_instance_set_dirty(QOF_INSTANCE(lot));
    kvp_frame_set_str (gnc_lot_get_slots(lot), "/title", str);
    gnc_lot_commit_edit(lot);
}

void
gnc_lot_set_notes (GNCLot *lot, const char *str)
{
    if (!lot) return;
    gnc_lot_begin_edit(lot);
    qof_instance_set_dirty(QOF_INSTANCE(lot));
    kvp_frame_set_str (gnc_lot_get_slots(lot), "/notes", str);
    gnc_lot_commit_edit(lot);
}

/* ============================================================= */

gnc_numeric
gnc_lot_get_balance (GNCLot *lot)
{
    LotPrivate* priv;
    gnc_numeric zero = gnc_numeric_zero();
    gnc_numeric baln = zero;
    if (!lot) return zero;

    priv = GET_PRIVATE(lot);
    if (priv->splits.empty())
    {
        priv->is_closed = FALSE;
        return zero;
    }

    /* Sum over splits; because they all belong to same account
     * they will have same denominator.
     */
    for (SplitList_t::iterator node = priv->splits.begin();
            node != priv->splits.end(); node++)
    {
        Split *s = *node;
        gnc_numeric amt = xaccSplitGetAmount (s);
        baln = gnc_numeric_add_fixed (baln, amt);
    }

    /* cache a zero balance as a closed lot */
    if (gnc_numeric_equal (baln, zero))
    {
        priv->is_closed = TRUE;
    }
    else
    {
        priv->is_closed = FALSE;
    }

    return baln;
}

/* ============================================================= */

void
gnc_lot_get_balance_before (const GNCLot *lot, const Split *split,
                            gnc_numeric *amount, gnc_numeric *value)
{
    LotPrivate* priv;
    gnc_numeric zero = gnc_numeric_zero();
    gnc_numeric amt = zero;
    gnc_numeric val = zero;

    *amount = amt;
    *value = val;
    if (lot == NULL) return;

    priv = GET_PRIVATE(lot);
    if (! priv->splits.empty())
    {
        Transaction *ta, *tb;
        const Split *target;
        /* If this is a gains split, find the source of the gains and use
           its transaction for the comparison.  Gains splits are in separate
           transactions that may sort after non-gains transactions.  */
        target = xaccSplitGetGainsSourceSplit (split);
        if (target == NULL)
            target = split;
        tb = xaccSplitGetParent (target);
        for (SplitList_t::iterator node = priv->splits.begin();
                node != priv->splits.end(); node++)
        {
            Split *s = *node;
            Split *source = xaccSplitGetGainsSourceSplit (s);
            if (source == NULL)
                source = s;
            ta = xaccSplitGetParent (source);
            if ((ta == tb && source != target) ||
                    xaccTransOrder (ta, tb) < 0)
            {
                gnc_numeric tmpval = xaccSplitGetAmount (s);
                amt = gnc_numeric_add_fixed (amt, tmpval);
                tmpval = xaccSplitGetValue (s);
                val = gnc_numeric_add_fixed (val, tmpval);
            }
        }
    }

    *amount = amt;
    *value = val;
}

/* ============================================================= */

void
gnc_lot_add_split (GNCLot *lot, Split *split)
{
    LotPrivate* priv;
    Account * acc;
    if (!lot || !split) return;
    priv = GET_PRIVATE(lot);

    ENTER ("(lot=%p, split=%p) %s amt=%s val=%s", lot, split,
           gnc_lot_get_title (lot),
           gnc_num_dbg_to_string (split->amount),
           gnc_num_dbg_to_string (split->value));
    gnc_lot_begin_edit(lot);
    acc = xaccSplitGetAccount (split);
    qof_instance_set_dirty(QOF_INSTANCE(lot));
    if (NULL == priv->account)
    {
        xaccAccountInsertLot (acc, lot);
    }
    else if (priv->account != acc)
    {
        PERR ("splits from different accounts cannot "
              "be added to this lot!\n"
              "\tlot account=\'%s\', split account=\'%s\'\n",
              xaccAccountGetName(priv->account), xaccAccountGetName (acc));
        gnc_lot_commit_edit(lot);
        LEAVE("different accounts");
        return;
    }

    if (lot == split->lot)
    {
        gnc_lot_commit_edit(lot);
        LEAVE("already in lot");
        return; /* handle not-uncommon no-op */
    }
    if (split->lot)
    {
        gnc_lot_remove_split (split->lot, split);
    }
    xaccSplitSetLot(split, lot);

    priv->splits.push_back(split);

    /* for recomputation of is-closed */
    priv->is_closed = LOT_CLOSED_UNKNOWN;
    gnc_lot_commit_edit(lot);

    qof_event_gen (QOF_INSTANCE(lot), QOF_EVENT_MODIFY, NULL);
    LEAVE("added to lot");
}

void
gnc_lot_remove_split (GNCLot *lot, Split *split)
{
    LotPrivate* priv;
    if (!lot || !split) return;
    priv = GET_PRIVATE(lot);

    ENTER ("(lot=%p, split=%p)", lot, split);
    gnc_lot_begin_edit(lot);
    qof_instance_set_dirty(QOF_INSTANCE(lot));
    priv->splits.remove(split);
    xaccSplitSetLot(split, NULL);
    priv->is_closed = LOT_CLOSED_UNKNOWN;   /* force an is-closed computation */

    if (priv->splits.empty())
    {
        xaccAccountRemoveLot (priv->account, lot);
        priv->account = NULL;
    }
    gnc_lot_commit_edit(lot);
    qof_event_gen (QOF_INSTANCE(lot), QOF_EVENT_MODIFY, NULL);
}

/* ============================================================== */
/* Utility function, get earliest split in lot */

Split *
gnc_lot_get_earliest_split (GNCLot *lot)
{
    LotPrivate* priv;
    if (!lot) return NULL;
    priv = GET_PRIVATE(lot);
    if (priv->splits.empty()) return NULL;
    priv->splits.sort(xaccSplitOrderDateOnlyStrictWeak);
    return *(priv->splits.begin());
}

/* Utility function, get latest split in lot */
Split *
gnc_lot_get_latest_split (GNCLot *lot)
{
    LotPrivate* priv;
    if (!lot) return NULL;
    priv = GET_PRIVATE(lot);
    if (priv->splits.empty()) return NULL;
    priv->splits.sort(xaccSplitOrderDateOnlyStrictWeak);
    return *(priv->splits.rbegin());
}

/* ============================================================= */

static void
destroy_lot_on_book_close(QofInstance *ent, gpointer data)
{
    GNCLot* lot = dynamic_cast<GNCLot*>(ent);

    gnc_lot_destroy(lot);
}

static void
gnc_lot_book_end(QofBook* book)
{
    QofCollection *col;

    col = qof_book_get_collection(book, GNC_ID_LOT);
    qof_collection_foreach(col, destroy_lot_on_book_close, NULL);
}

/* A C++ compiler doesn't have C99 "designated initializers"
 * so we wrap them in a macro that is empty. */
# define DI(x) /* */
static QofObject gncLotDesc =
{
    DI(.interface_version = ) QOF_OBJECT_VERSION,
    DI(.e_type            = ) GNC_ID_LOT,
    DI(.type_label        = ) "Lot",
    DI(.create            = ) (gpointer)gnc_lot_new,
    DI(.book_begin        = ) NULL,
    DI(.book_end          = ) gnc_lot_book_end,
    DI(.is_dirty          = ) qof_collection_is_dirty,
    DI(.mark_clean        = ) qof_collection_mark_clean,
    DI(.foreach           = ) qof_collection_foreach,
    DI(.printable         = ) NULL,
    DI(.version_cmp       = ) (int (*)(gpointer, gpointer))qof_instance_version_cmp,
};


bool gnc_lot_register (void)
{
//    static const QofParam params[] =
//    {
//        {
//            LOT_TITLE, QOF_TYPE_STRING,
//            (QofAccessFunc) gnc_lot_get_title,
//            (QofSetterFunc) gnc_lot_set_title
//        },
//        {
//            LOT_NOTES, QOF_TYPE_STRING,
//            (QofAccessFunc) gnc_lot_get_notes,
//            (QofSetterFunc) gnc_lot_set_notes
//        },
//        {
//            QOF_PARAM_GUID, QOF_TYPE_GUID,
//            (QofAccessFunc) qof_entity_get_guid, NULL
//        },
//        {
//            QOF_PARAM_BOOK, QOF_ID_BOOK,
//            (QofAccessFunc) gnc_lot_get_book, NULL
//        },
//        {
//            LOT_IS_CLOSED, QOF_TYPE_BOOLEAN,
//            (QofAccessFunc) gnc_lot_is_closed, NULL
//        },
//        {
//            LOT_BALANCE, QOF_TYPE_NUMERIC,
//            (QofAccessFunc) gnc_lot_get_balance, NULL
//        },
//        { NULL },
//    };
//
//    qof_class_register (GNC_ID_LOT, NULL, params);
    return qof_object_register(&gncLotDesc);
}

GNCLot * gnc_lot_make_default (Account *acc)
{
    GNCLot * lot;
    gint64 id;
    char buff[200];

    lot = gnc_lot_new (qof_instance_get_book(acc));

    /* Provide a reasonable title for the new lot */
    xaccAccountBeginEdit (acc);
    id = kvp_frame_get_gint64 (xaccAccountGetSlots (acc), "/lot-mgmt/next-id");
    snprintf (buff, 200, ("%s %" G_GINT64_FORMAT), _("Lot"), id);
    kvp_frame_set_str (gnc_lot_get_slots (lot), "/title", buff);
    id ++;
    kvp_frame_set_gint64 (xaccAccountGetSlots (acc), "/lot-mgmt/next-id", id);
    qof_instance_set_dirty (QOF_INSTANCE(acc));
    xaccAccountCommitEdit (acc);
    return lot;
}

/* ========================== END OF FILE ========================= */
