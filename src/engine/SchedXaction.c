/********************************************************************\
 * SchedXaction.c -- Scheduled Transaction implementation.          *
 * Copyright (C) 2001,2007 Joshua Sled <jsled@asynchronous.org>     *
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
#include <string.h>

#include "qof.h"

#include "FreqSpec.h"
#include "Account.h"
#include "gnc-book.h"
#include "SX-book.h"
#include "SX-ttinfo.h"
#include "SchedXaction.h"
#include "Transaction.h"
#include "gnc-engine.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "gnc.engine.sx"

/* Local Prototypes *****/

void sxprivtransactionListMapDelete( gpointer data, gpointer user_data );

static void
xaccSchedXactionInit(SchedXaction *sx, QofBook *book)
{
   Account        *ra;

   qof_instance_init (&sx->inst, GNC_ID_SCHEDXACTION, book);

   sx->schedule = NULL;
   sx->freq = xaccFreqSpecMalloc(book);

   g_date_clear( &sx->last_date, 1 );
   g_date_clear( &sx->start_date, 1 );
   g_date_clear( &sx->end_date, 1 );

   sx->enabled = 1;
   sx->num_occurances_total = 0;
   sx->autoCreateOption = FALSE;
   sx->autoCreateNotify = FALSE;
   sx->advanceCreateDays = 0;
   sx->advanceRemindDays = 0;
   sx->instance_num = 0;
   sx->deferredList = NULL;

   /* create a new template account for our splits */
   sx->template_acct = xaccMallocAccount(book);
   xaccAccountSetName( sx->template_acct,
                       guid_to_string( &sx->inst.entity.guid ));
   xaccAccountSetCommodity
     (sx->template_acct,
      gnc_commodity_new( book,
                         "template", "template",
                         "template", "template", 1 ) );
   xaccAccountSetType( sx->template_acct, ACCT_TYPE_BANK );
   ra = gnc_book_get_template_root( book );
   gnc_account_append_child( ra, sx->template_acct );
}

SchedXaction*
xaccSchedXactionMalloc(QofBook *book)
{
   SchedXaction *sx;

   g_return_val_if_fail (book, NULL);

   sx = g_new0( SchedXaction, 1 );
   xaccSchedXactionInit( sx, book );
   qof_event_gen( &sx->inst.entity, QOF_EVENT_CREATE , NULL);

   return sx;
}

static void
sxprivTransMapDelete( gpointer data, gpointer user_data )
{
  Transaction *t = (Transaction *) data;
  xaccTransBeginEdit( t );
  xaccTransDestroy( t );
  xaccTransCommitEdit( t );
  return;
}

static void
delete_template_trans(SchedXaction *sx)
{
  GList *templ_acct_splits, *curr_split_listref;
  Split *curr_split;
  Transaction *split_trans;
  GList *templ_acct_transactions = NULL;

  templ_acct_splits 
    = xaccAccountGetSplitList(sx->template_acct);
  
  for(curr_split_listref = templ_acct_splits;
      curr_split_listref;
      curr_split_listref = curr_split_listref->next)
  {
    curr_split = (Split *) curr_split_listref->data;
    split_trans = xaccSplitGetParent(curr_split);
    if(! (g_list_find(templ_acct_transactions, split_trans)))
    {
      templ_acct_transactions 
   = g_list_prepend(templ_acct_transactions, split_trans);
    }
  }
  
  g_list_foreach(templ_acct_transactions,
       sxprivTransMapDelete, 
       NULL);
  
  return;
}

void
sx_set_template_account (SchedXaction *sx, Account *account)
{
  Account *old;

  old = sx->template_acct;
  sx->template_acct = account;
  if (old) {
    xaccAccountBeginEdit(old);
    xaccAccountDestroy(old);
  }
}

void
xaccSchedXactionFree( SchedXaction *sx )
{
  GList *l;
      
  if ( sx == NULL ) return;
  
  xaccFreqSpecFree( sx->freq );
  qof_event_gen( &sx->inst.entity, QOF_EVENT_DESTROY , NULL);
  
  if ( sx->name )
    g_free( sx->name );

  /* 
   * we have to delete the transactions in the 
   * template account ourselves
   */
  
  delete_template_trans( sx );
  
  /*
   * xaccAccountDestroy removes the account from
   * its group for us AFAICT
   */
  
  xaccAccountBeginEdit(sx->template_acct);
  xaccAccountDestroy(sx->template_acct);

  for ( l = sx->deferredList; l; l = l->next ) {
          gnc_sx_destroy_temporal_state( l->data );
          l->data = NULL;
  }
  if ( sx->deferredList ) {
          g_list_free( sx->deferredList );
          sx->deferredList = NULL;
  }
  
  qof_instance_release (&sx->inst);
  g_free( sx );
}

/* ============================================================ */

void
gnc_sx_begin_edit (SchedXaction *sx)
{
  qof_begin_edit (&sx->inst);
}

static void commit_err (QofInstance *inst, QofBackendError errcode)
{
     g_critical("Failed to commit: %d", errcode);
}

static void commit_done(QofInstance *inst)
{
  qof_event_gen (&inst->entity, QOF_EVENT_MODIFY, NULL);
}

static void noop(QofInstance *inst) {}

void
gnc_sx_commit_edit (SchedXaction *sx)
{
  if (!qof_commit_edit (QOF_INSTANCE(sx))) return;
  qof_commit_edit_part2 (&sx->inst, commit_err, commit_done, noop);
}

/* ============================================================ */

FreqSpec *
xaccSchedXactionGetFreqSpec( SchedXaction *sx )
{
   return sx->freq;
}

void
xaccSchedXactionSetFreqSpec( SchedXaction *sx, FreqSpec *fs )
{
   g_return_if_fail( fs );

   gnc_sx_begin_edit(sx);
   xaccFreqSpecFree( sx->freq );
   sx->freq = fs;
   qof_instance_set_dirty(&sx->inst);
   gnc_sx_commit_edit(sx);
}

GList*
gnc_sx_get_schedule(SchedXaction *sx)
{
   return sx->schedule;
}

void
gnc_sx_set_schedule(SchedXaction *sx, GList *schedule)
{
   g_return_if_fail(sx && schedule);
   gnc_sx_begin_edit(sx);
   sx->schedule = schedule;
   qof_instance_set_dirty(&sx->inst);
   gnc_sx_commit_edit(sx);
}

gchar *
xaccSchedXactionGetName( SchedXaction *sx )
{
   return sx->name;
}

void
xaccSchedXactionSetName( SchedXaction *sx, const gchar *newName )
{
   g_return_if_fail( newName != NULL );
   gnc_sx_begin_edit(sx);
   if ( sx->name != NULL ) {
           g_free( sx->name );
           sx->name = NULL;
   }
   sx->name = g_strdup( newName );
   qof_instance_set_dirty(&sx->inst);
   gnc_sx_commit_edit(sx);
}

GDate*
xaccSchedXactionGetStartDate( SchedXaction *sx )
{
   return &sx->start_date;
}

void
xaccSchedXactionSetStartDate( SchedXaction *sx, GDate* newStart )
{
   gnc_sx_begin_edit(sx);
   sx->start_date = *newStart;
   qof_instance_set_dirty(&sx->inst);
   gnc_sx_commit_edit(sx);
}

gboolean
xaccSchedXactionHasEndDate( SchedXaction *sx )
{
   return g_date_valid( &sx->end_date );
}

GDate*
xaccSchedXactionGetEndDate( SchedXaction *sx )
{
   return &sx->end_date;
}

void
xaccSchedXactionSetEndDate( SchedXaction *sx, GDate *newEnd )
{
  if ( g_date_valid( newEnd )
       && g_date_compare( newEnd, &sx->start_date ) < 0 ) {
    /* XXX: I reject the bad data - is this the right 
     * thing to do <rgmerk>.
     * This warning is only human readable - the caller
     * doesn't know the call failed.  This is bad
     */
    g_critical("New end date before start date"); 
    return;
  }

  gnc_sx_begin_edit(sx);
  sx->end_date = *newEnd;
  qof_instance_set_dirty(&sx->inst);
  gnc_sx_commit_edit(sx);
}

GDate*
xaccSchedXactionGetLastOccurDate( SchedXaction *sx )
{
   return &sx->last_date;
}

void
xaccSchedXactionSetLastOccurDate( SchedXaction *sx, GDate* newLastOccur )
{
  gnc_sx_begin_edit(sx);
  sx->last_date = *newLastOccur;
  qof_instance_set_dirty(&sx->inst);
  gnc_sx_commit_edit(sx);
}

gboolean
xaccSchedXactionHasOccurDef( SchedXaction *sx )
{
  return ( xaccSchedXactionGetNumOccur( sx ) != 0 );
}

gint
xaccSchedXactionGetNumOccur( SchedXaction *sx )
{
  return sx->num_occurances_total;
}

void
xaccSchedXactionSetNumOccur( SchedXaction *sx, gint newNum )
{
  gnc_sx_begin_edit(sx);
  sx->num_occurances_remain = sx->num_occurances_total = newNum;
  qof_instance_set_dirty(&sx->inst);
  gnc_sx_commit_edit(sx);
}

gint
xaccSchedXactionGetRemOccur( SchedXaction *sx )
{
  return sx->num_occurances_remain;
}

void
xaccSchedXactionSetRemOccur( SchedXaction *sx,
                             gint numRemain )
{
  /* FIXME This condition can be tightened up */
  if ( numRemain > sx->num_occurances_total ) 
  {
    g_warning("The number remaining is greater than the total occurrences");
  }
  else
  {
    gnc_sx_begin_edit(sx);
    sx->num_occurances_remain = numRemain;
    qof_instance_set_dirty(&sx->inst);
    gnc_sx_commit_edit(sx);
  }
}


KvpValue *
xaccSchedXactionGetSlot( SchedXaction *sx, const char *slot )
{
  if (!sx) return NULL;

  return kvp_frame_get_slot(sx->inst.kvp_data, slot);
}

void
xaccSchedXactionSetSlot( SchedXaction *sx, 
          const char *slot,
          const KvpValue *value )
{
  if (!sx) return;

  gnc_sx_begin_edit(sx);
  kvp_frame_set_slot( sx->inst.kvp_data, slot, value );
  qof_instance_set_dirty(&sx->inst);
  gnc_sx_commit_edit(sx);
}

gboolean
xaccSchedXactionGetEnabled( SchedXaction *sx )
{
    return sx->enabled;
}

void
xaccSchedXactionSetEnabled( SchedXaction *sx, gboolean newEnabled)
{
  gnc_sx_begin_edit(sx);
  sx->enabled = newEnabled;
  qof_instance_set_dirty(&sx->inst);
  gnc_sx_commit_edit(sx);
}

void
xaccSchedXactionGetAutoCreate( SchedXaction *sx,
                               gboolean *outAutoCreate,
                               gboolean *outNotify )
{
  if (outAutoCreate != NULL)
    *outAutoCreate = sx->autoCreateOption;
  if (outNotify != NULL)
    *outNotify     = sx->autoCreateNotify;
  return;
}

void
xaccSchedXactionSetAutoCreate( SchedXaction *sx,
                               gboolean newAutoCreate,
                               gboolean newNotify )
{
 
  gnc_sx_begin_edit(sx);
  sx->autoCreateOption = newAutoCreate;
  sx->autoCreateNotify = newNotify; 
  qof_instance_set_dirty(&sx->inst);
  gnc_sx_commit_edit(sx);
  return;
}

gint
xaccSchedXactionGetAdvanceCreation( SchedXaction *sx )
{
  return sx->advanceCreateDays;
}

void
xaccSchedXactionSetAdvanceCreation( SchedXaction *sx, gint createDays )
{
  gnc_sx_begin_edit(sx);
  sx->advanceCreateDays = createDays;
  qof_instance_set_dirty(&sx->inst);
  gnc_sx_commit_edit(sx);
}

gint
xaccSchedXactionGetAdvanceReminder( SchedXaction *sx )
{
   return sx->advanceRemindDays;
}

void
xaccSchedXactionSetAdvanceReminder( SchedXaction *sx, gint reminderDays )
{
  gnc_sx_begin_edit(sx);
  sx->advanceRemindDays = reminderDays;
  qof_instance_set_dirty(&sx->inst);
  gnc_sx_commit_edit(sx);
}


GDate
xaccSchedXactionGetNextInstance( SchedXaction *sx, void *stateData )
{
   GDate    last_occur, next_occur, tmpDate;

   g_date_clear( &last_occur, 1 );
   g_date_clear( &next_occur, 1 );
   g_date_clear( &tmpDate, 1 );

   if ( g_date_valid( &sx->last_date ) ) {
      last_occur = sx->last_date;
   } 

   if ( stateData != NULL ) {
      temporalStateData *tsd = (temporalStateData*)stateData;
      last_occur = tsd->last_date;
   }

   if ( g_date_valid( &sx->start_date ) ) {
      if ( g_date_valid(&last_occur) ) {
         last_occur =
            ( g_date_compare( &last_occur,
                    &sx->start_date ) > 0 ?
              last_occur : sx->start_date );
      } else {
         /* Think about this for a second, and you realize that if the
          * start date is _today_, we need a last-occur date such that
          * the 'next instance' is after that date, and equal to the
          * start date... one day should be good.
          *
          * This only holds for the first instance [read: if the
          * last[-occur]_date is invalid] */
         last_occur = sx->start_date;
         g_date_subtract_days( &last_occur, 1 );
      }
   }

   recurrenceListNextInstance(sx->schedule, &last_occur, &next_occur);

   /* out-of-bounds check */
   if ( xaccSchedXactionHasEndDate( sx ) ) {
      GDate *end_date = xaccSchedXactionGetEndDate( sx );
      if ( g_date_compare( &next_occur, end_date ) > 0 ) {
         g_debug("next_occur past end date");
         g_date_clear( &next_occur, 1 );
      }
   } else if ( xaccSchedXactionHasOccurDef( sx ) ) {
      if ( stateData ) {
         temporalStateData *tsd = (temporalStateData*)stateData;
         if ( tsd->num_occur_rem == 0 ) {
            g_debug("no more occurances remain");
            g_date_clear( &next_occur, 1 );
         }
      } else {
         if ( sx->num_occurances_remain == 0 ) {
            g_date_clear( &next_occur, 1 );
         }
      }
   }

   return next_occur;
}

GDate
xaccSchedXactionGetInstanceAfter( SchedXaction *sx,
                                  GDate *date,
                                  void *stateData )
{
   GDate prev_occur, next_occur;

   g_date_clear( &prev_occur, 1 );
   if ( date ) {
      prev_occur = *date;
   }

   if ( stateData != NULL ) {
      temporalStateData *tsd = (temporalStateData*)stateData;
      prev_occur = tsd->last_date;
   }

   if ( ! g_date_valid( &prev_occur ) ) {
      /* We must be at the beginning. */
      prev_occur = sx->start_date;
      g_date_subtract_days( &prev_occur, 1 );
   }

   recurrenceListNextInstance(sx->schedule, &prev_occur, &next_occur);

   if ( xaccSchedXactionHasEndDate( sx ) ) {
      GDate *end_date;

      end_date = xaccSchedXactionGetEndDate( sx );
      if ( g_date_compare( &next_occur, end_date ) > 0 ) {
         g_date_clear( &next_occur, 1 );
      }
   } else if ( xaccSchedXactionHasOccurDef( sx ) ) {
      if ( stateData ) {
         temporalStateData *tsd = (temporalStateData*)stateData;
         if ( tsd->num_occur_rem == 0 ) {
            g_date_clear( &next_occur, 1 );
         }
      } else {
         if ( sx->num_occurances_remain == 0 ) {
            g_date_clear( &next_occur, 1 );
         }
      }
   }
   return next_occur;
}

gint
gnc_sx_get_instance_count( SchedXaction *sx, void *stateData )
{
  gint toRet = -1;
  temporalStateData *tsd;

  if ( stateData ) {
    tsd = (temporalStateData*)stateData;
    toRet = tsd->num_inst;
  } else {
    toRet = sx->instance_num;
  }

  return toRet;
}

void
gnc_sx_set_instance_count( SchedXaction *sx, gint instanceNum )
{
  g_return_if_fail( sx );
  sx->instance_num = instanceNum;
}

GList *
xaccSchedXactionGetSplits( SchedXaction *sx )
{
  g_return_val_if_fail( sx, NULL );
  return xaccAccountGetSplitList(sx->template_acct);
}

static Split *
pack_split_info (TTSplitInfo *s_info, Account *parent_acct,
       Transaction *parent_trans, QofBook *book)
{
  Split *split;
  KvpFrame *split_frame;
  KvpValue *tmp_value;
  const GUID *acc_guid; 
  
  split = xaccMallocSplit(book);

  xaccSplitSetMemo(split, 
         gnc_ttsplitinfo_get_memo(s_info));

  xaccSplitSetAction(split,
           gnc_ttsplitinfo_get_action(s_info));
  

  xaccAccountInsertSplit(parent_acct, 
          split);

  split_frame = xaccSplitGetSlots(split);
  
  tmp_value 
    = kvp_value_new_string(gnc_ttsplitinfo_get_credit_formula(s_info));

  kvp_frame_set_slot_path(split_frame, 
           tmp_value,
           GNC_SX_ID,
           GNC_SX_CREDIT_FORMULA,
           NULL);
  kvp_value_delete(tmp_value);
            
  tmp_value
    = kvp_value_new_string(gnc_ttsplitinfo_get_debit_formula(s_info));
  
  kvp_frame_set_slot_path(split_frame,
           tmp_value,
           GNC_SX_ID,
           GNC_SX_DEBIT_FORMULA,
           NULL);

  kvp_value_delete(tmp_value);

  acc_guid = xaccAccountGetGUID(gnc_ttsplitinfo_get_account(s_info));

  tmp_value = kvp_value_new_guid(acc_guid);

  kvp_frame_set_slot_path(split_frame,
           tmp_value,
           GNC_SX_ID,
           GNC_SX_ACCOUNT,
           NULL);

  kvp_value_delete(tmp_value);

  return split;
}
  

void
xaccSchedXactionSetTemplateTrans(SchedXaction *sx, GList *t_t_list,
             QofBook *book)
{
  Transaction *new_trans;
  TTInfo *tti;
  TTSplitInfo *s_info;
  Split *new_split;
  GList *split_list;

  g_return_if_fail (book);

  /* delete any old transactions, if there are any */
  delete_template_trans( sx );

  for(;t_t_list != NULL; t_t_list = t_t_list->next)
  {
    tti = t_t_list->data;

    new_trans = xaccMallocTransaction(book);

    xaccTransBeginEdit(new_trans);

    xaccTransSetDescription(new_trans, 
             gnc_ttinfo_get_description(tti));

    xaccTransSetNum(new_trans,
          gnc_ttinfo_get_num(tti));
    xaccTransSetCurrency( new_trans,
           gnc_ttinfo_get_currency(tti) );

    for(split_list = gnc_ttinfo_get_template_splits(tti);
   split_list;
   split_list = split_list->next)
    {
      s_info = split_list->data;
      new_split = pack_split_info(s_info, sx->template_acct,
              new_trans, book);
      xaccTransAppendSplit(new_trans, new_split);
    }
    xaccTransCommitEdit(new_trans);
  }
}

void*
gnc_sx_create_temporal_state( SchedXaction *sx )
{
   temporalStateData *toRet =
      g_new0( temporalStateData, 1 );
   toRet->last_date       = sx->last_date;
   toRet->num_occur_rem   = sx->num_occurances_remain;
   toRet->num_inst   = sx->instance_num;
   return (void*)toRet;
}

void
gnc_sx_incr_temporal_state( SchedXaction *sx, void *stateData )
{
   GDate unused;
   temporalStateData *tsd = (temporalStateData*)stateData;

   g_date_clear( &unused, 1 );
   tsd->last_date =
      xaccSchedXactionGetInstanceAfter( sx,
                    &unused,
                    stateData );
   if ( xaccSchedXactionHasOccurDef( sx ) ) {
      tsd->num_occur_rem -= 1;
   }
   tsd->num_inst += 1;
}

void
gnc_sx_revert_to_temporal_state( SchedXaction *sx, void *stateData )
{
   temporalStateData *tsd = (temporalStateData*)stateData;
   gnc_sx_begin_edit(sx);
   sx->last_date        = tsd->last_date;
   sx->num_occurances_remain = tsd->num_occur_rem;
   sx->instance_num     = tsd->num_inst;
   qof_instance_set_dirty(&sx->inst);
   gnc_sx_commit_edit(sx);
}

void
gnc_sx_destroy_temporal_state( void *stateData )
{
   g_free( (temporalStateData*)stateData );
}

void*
gnc_sx_clone_temporal_state( void *stateData )
{
   temporalStateData *toRet, *tsd;
   tsd = (temporalStateData*)stateData;
   toRet = g_memdup( tsd, sizeof( temporalStateData ) );
   return (void*)toRet;
}

static
gint
_temporal_state_data_cmp( gconstpointer a, gconstpointer b )
{
   temporalStateData *tsd_a, *tsd_b;
   tsd_a = (temporalStateData*)a;
   tsd_b = (temporalStateData*)b;

   if ( !tsd_a && !tsd_b )
      return 0;
   if ( !tsd_a )
      return 1;
   if ( !tsd_b )
      return -1;
   return g_date_compare( &tsd_a->last_date,
                &tsd_b->last_date );
}

/**
 * Adds an instance to the deferred list of the SX.  Added instances are
 * added in (date-)sorted order.
 **/
void
gnc_sx_add_defer_instance( SchedXaction *sx, void *deferStateData )
{
   sx->deferredList = g_list_insert_sorted( sx->deferredList,
                   deferStateData,
                   _temporal_state_data_cmp );
}

/**
 * Removes an instance from the deferred list.  If the instance is no longer
 * useful; gnc_sx_destroy_temporal_state() it.
 **/
void
gnc_sx_remove_defer_instance( SchedXaction *sx, void *deferStateData )
{
   sx->deferredList = g_list_remove( sx->deferredList, deferStateData );
}

/**
 * Returns the defer list from the SX; this is a (date-)sorted
 * temporal-state-data instance list.  The list should not be modified by the
 * caller; use the gnc_sx_{add,remove}_defer_instance() functions to modifiy
 * the list.
 **/
GList*
gnc_sx_get_defer_instances( SchedXaction *sx )
{
   return sx->deferredList;
}

static QofObject SXDesc = 
{
	interface_version : QOF_OBJECT_VERSION,
	e_type            : GNC_SX_ID,
	type_label        : "Scheduled Transaction",
	create            : (gpointer)xaccSchedXactionMalloc,
	book_begin        : NULL,
	book_end          : NULL,
	is_dirty          : qof_collection_is_dirty,
	mark_clean        : qof_collection_mark_clean,
	foreach           : qof_collection_foreach,
	printable         : NULL,
	version_cmp       : (int (*)(gpointer, gpointer)) qof_instance_version_cmp,
};

gboolean
SXRegister(void)
{
	static QofParam params[] = {
	 { GNC_SX_FREQ_SPEC, QOF_ID_FREQSPEC, (QofAccessFunc)xaccSchedXactionGetFreqSpec,
		 (QofSetterFunc)xaccSchedXactionSetFreqSpec },
	 { GNC_SX_NAME, QOF_TYPE_STRING, (QofAccessFunc)xaccSchedXactionGetName,
		 (QofSetterFunc)xaccSchedXactionSetName },
	 { GNC_SX_START_DATE, QOF_TYPE_DATE, (QofAccessFunc)xaccSchedXactionGetStartDate,
		 (QofSetterFunc)xaccSchedXactionSetStartDate },
	 { GNC_SX_LAST_DATE, QOF_TYPE_DATE, (QofAccessFunc)xaccSchedXactionGetLastOccurDate,
		 (QofSetterFunc)xaccSchedXactionSetLastOccurDate },
	 { GNC_SX_NUM_OCCUR, QOF_TYPE_INT64, (QofAccessFunc)xaccSchedXactionGetNumOccur,
		 (QofSetterFunc)xaccSchedXactionSetNumOccur },
	 { GNC_SX_REM_OCCUR, QOF_TYPE_INT64, (QofAccessFunc)xaccSchedXactionGetRemOccur,
		 (QofSetterFunc)xaccSchedXactionSetRemOccur },
	 { QOF_PARAM_BOOK, QOF_ID_BOOK, (QofAccessFunc)qof_instance_get_book, NULL },
	 { QOF_PARAM_GUID, QOF_TYPE_GUID, (QofAccessFunc)qof_instance_get_guid, NULL },
	 { NULL },
	};
	qof_class_register(GNC_SX_ID, NULL, params);
	return qof_object_register(&SXDesc);
}
