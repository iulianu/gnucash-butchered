/********************************************************************\
 * Query.c : api for finding transactions                           *
 * Copyright (C) 2000 Bill Gribble <grib@billgribble.com>           *
 * Copyright (C) 2002 Linas Vepstas <linas@linas.org>               *
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

#include "config.h"

#include <ctype.h>
#include <glib.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>

#include <regex.h>
#include <sys/time.h>
#include <unistd.h>

#include "gnc-engine-util.h"
#include "gnc-numeric.h"
#include "Account.h"
#include "Query.h"
#include "Transaction.h"
#include "TransactionP.h"
#include "gnc-lot.h"

static short module = MOD_QUERY;

static GSList *
build_param_list_internal (const char *first, va_list rest)
{
  GSList *list = NULL;
  char const *param;

  for (param = first; param; param = va_arg (rest, const char *))
    list = g_slist_prepend (list, (gpointer)param);

  return (g_slist_reverse (list));
}

/********************************************************************
 * xaccQueryGetSplitsUniqueTrans 
 * Get splits but no more than one from a given transaction.
 ********************************************************************/

SplitList *
xaccQueryGetSplitsUniqueTrans(Query *q)
{
  GList       * splits = xaccQueryGetSplits(q);
  GList       * current;
  GList       * result = NULL;
  GHashTable  * trans_hash = g_hash_table_new(g_direct_hash, g_direct_equal);

  for (current = splits; current; current = current->next)
  {
    Split *split = current->data;
    Transaction *trans = xaccSplitGetParent (split);

    if (!g_hash_table_lookup (trans_hash, trans))
    {
      g_hash_table_insert (trans_hash, trans, trans);
      result = g_list_prepend (result, split);
    }
  }

  g_hash_table_destroy (trans_hash);

  return g_list_reverse (result);
}

/********************************************************************
 * xaccQueryGetTransactions 
 * Get transactions matching the query terms, specifying whether 
 * we require some or all splits to match 
 ********************************************************************/

static void
query_match_all_filter_func(gpointer key, gpointer value, gpointer user_data) 
{
  Transaction * t = key;
  int         num_matches = GPOINTER_TO_INT(value);
  GList       ** matches = user_data;

  if(num_matches == xaccTransCountSplits(t)) {
    *matches = g_list_prepend(*matches, t);
  }
}

static void
query_match_any_filter_func(gpointer key, gpointer value, gpointer user_data) 
{
  Transaction * t = key;
  GList       ** matches = user_data;
  *matches = g_list_prepend(*matches, t);
}

TransList * 
xaccQueryGetTransactions (Query * q, query_txn_match_t runtype) 
{
  GList       * splits = xaccQueryGetSplits(q);
  GList       * current = NULL;
  GList       * retval = NULL;
  GHashTable  * trans_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
  Transaction * trans = NULL;
  gpointer    val = NULL;
  int         count = 0;

  /* iterate over matching splits, incrementing a match-count in
   * the hash table */
  for(current = splits; current; current=current->next) {
    trans = xaccSplitGetParent((Split *)(current->data));
    
    /* don't waste time looking up unless we need the count 
     * information */
    if(runtype == QUERY_TXN_MATCH_ALL) {
      val   = g_hash_table_lookup(trans_hash, trans);
      count = GPOINTER_TO_INT(val);
    }
    g_hash_table_insert(trans_hash, trans, GINT_TO_POINTER(count + 1));
  }
  
  /* now pick out the transactions that match */
  if(runtype == QUERY_TXN_MATCH_ALL) {
    g_hash_table_foreach(trans_hash, query_match_all_filter_func, 
                         &retval);
  }
  else {
    g_hash_table_foreach(trans_hash, query_match_any_filter_func, 
                         &retval);
  }

  g_hash_table_destroy(trans_hash);

  return retval;
}

/********************************************************************
 * xaccQueryGetLots
 * Get lots matching the query terms, specifying whether 
 * we require some or all splits to match 
 ********************************************************************/

static void
query_match_all_lot_filter_func(gpointer key, gpointer value, gpointer user_data) 
{
  GNCLot *	l = key;
  int		num_matches = GPOINTER_TO_INT(value);
  GList **	matches = user_data;

  if(num_matches == gnc_lot_count_splits(l)) {
    *matches = g_list_prepend(*matches, l);
  }
}

static void
query_match_any_lot_filter_func(gpointer key, gpointer value, gpointer user_data) 
{
  GNCLot *	t = key;
  GList **	matches = user_data;
  *matches = g_list_prepend(*matches, t);
}

LotList * 
xaccQueryGetLots (Query * q, query_txn_match_t runtype) 
{
  GList       * splits = xaccQueryGetSplits(q);
  GList       * current = NULL;
  GList       * retval = NULL;
  GHashTable  * lot_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
  GNCLot      * lot = NULL;
  gpointer    val = NULL;
  int         count = 0;

  /* iterate over matching splits, incrementing a match-count in
   * the hash table */
  for(current = splits; current; current=current->next) {
    lot = xaccSplitGetLot((Split *)(current->data));
    
    /* don't waste time looking up unless we need the count 
     * information */
    if(runtype == QUERY_TXN_MATCH_ALL) {
      val   = g_hash_table_lookup(lot_hash, lot);
      count = GPOINTER_TO_INT(val);
    }
    g_hash_table_insert(lot_hash, lot, GINT_TO_POINTER(count + 1));
  }
  
  /* now pick out the transactions that match */
  if(runtype == QUERY_TXN_MATCH_ALL) {
    g_hash_table_foreach(lot_hash, query_match_all_lot_filter_func, 
                         &retval);
  }
  else {
    g_hash_table_foreach(lot_hash, query_match_any_lot_filter_func, 
                         &retval);
  }

  g_hash_table_destroy(lot_hash);

  return retval;
}

/*******************************************************************
 *  match-adding API 
 *******************************************************************/

void
xaccQueryAddAccountMatch(Query *q, AccountList *acct_list,
			 QofGuidMatch how, QofQueryOp op)
{
  GList *list = NULL;

  if (!q) return;
  for (; acct_list; acct_list = acct_list->next) {
    Account *acc = acct_list->data;
    const GUID *guid;

    if (!acc) {
      PWARN ("acct_list has NULL account");
      continue;
    }

    guid = xaccAccountGetGUID (acc);
    if (!guid) {
      PWARN ("acct returns NULL GUID");
      continue;
    }

    list = g_list_prepend (list, (gpointer)guid);
  }
  xaccQueryAddAccountGUIDMatch (q, list, how, op);
  g_list_free (list);
}

void
xaccQueryAddAccountGUIDMatch(Query *q, AccountGUIDList *guid_list,
			     QofGuidMatch how, QofQueryOp op)
{
  QofQueryPredData *pred_data;
  GSList *param_list = NULL;

  if (!q) return;

  pred_data = qof_query_guid_predicate (how, guid_list);
  if (!pred_data)
    return;

  switch (how) {
  case QOF_GUID_MATCH_ANY:
  case QOF_GUID_MATCH_NONE:
    param_list = qof_query_build_param_list (SPLIT_ACCOUNT, QOF_QUERY_PARAM_GUID, NULL);
    break;
  case QOF_GUID_MATCH_ALL:
    param_list = qof_query_build_param_list (SPLIT_TRANS, TRANS_SPLITLIST,
					 SPLIT_ACCOUNT_GUID, NULL);
    break;
  default:
    PERR ("Invalid match type: %d", how);
  }

  qof_query_add_term (q, param_list, pred_data, op);
}

void
xaccQueryAddSingleAccountMatch(Query *q, Account *acc, QofQueryOp op)
{
  GList *list;
  const GUID *guid;

  if (!q || !acc)
    return;

  guid = xaccAccountGetGUID (acc);
  g_return_if_fail (guid);

  list = g_list_prepend (NULL, (gpointer)guid);
  xaccQueryAddAccountGUIDMatch (q, list, QOF_GUID_MATCH_ANY, op);
  g_list_free (list);
}

void
xaccQueryAddStringMatch (Query* q, const char *matchstring,
			 int case_sens, int use_regexp, QofQueryOp op,
			 const char * path, ...)
{
  QofQueryPredData *pred_data;
  GSList *param_list;
  va_list ap;

  if (!path || !q)
    return;

  pred_data = qof_query_string_predicate (QOF_COMPARE_EQUAL, (char *)matchstring,
				       (case_sens ? QOF_STRING_MATCH_NORMAL :
					QOF_STRING_MATCH_CASEINSENSITIVE),
				       use_regexp);
  if (!pred_data)
    return;

  va_start (ap, path);
  param_list = build_param_list_internal (path, ap);
  va_end (ap);

  qof_query_add_term (q, param_list, pred_data, op);
}

void
xaccQueryAddNumericMatch (Query *q, gnc_numeric amount, QofNumericMatch sign,
			  QofQueryCompare how, QofQueryOp op,
			  const char * path, ...)
{
  QofQueryPredData *pred_data;
  GSList *param_list;
  va_list ap;

  if (!q || !path)
    return;

  pred_data = qof_query_numeric_predicate (how, sign, amount);
  if (!pred_data)
    return;

  va_start (ap, path);
  param_list = build_param_list_internal (path, ap);
  va_end (ap);

  qof_query_add_term (q, param_list, pred_data, op);
}

/* The DateMatch queries match transactions whose posted date
 *    is in a date range.  If use_start is TRUE, then a matching
 *    posted date will be greater than the start date.   If 
 *    use_end is TRUE, then a match occurs for posted dates earlier 
 *    than the end date.  If both flags are set, then *both* 
 *    conditions must hold ('and').  If neither flag is set, then 
 *    all transactions are matched.
 */

void
xaccQueryAddDateMatchTS (Query * q, 
			 int use_start, Timespec sts, 
			 int use_end, Timespec ets,
			 QofQueryOp op)
{
  Query *tmp_q = NULL;
  QofQueryPredData *pred_data;
  GSList *param_list;

  if (!q || (!use_start && !use_end))
    return;

  tmp_q = qof_query_create ();

  if (use_start) {
    pred_data = qof_query_date_predicate (QOF_COMPARE_GTE, QOF_DATE_MATCH_NORMAL, sts);
    if (!pred_data) {
      qof_query_destroy (tmp_q);
      return;
    }

    param_list = qof_query_build_param_list (SPLIT_TRANS, TRANS_DATE_POSTED, NULL);
    qof_query_add_term (tmp_q, param_list, pred_data, QOF_QUERY_AND);
  }

  if (use_end) {
    pred_data = qof_query_date_predicate (QOF_COMPARE_LTE, QOF_DATE_MATCH_NORMAL, ets);
    if (!pred_data) {
      qof_query_destroy (tmp_q);
      return;
    }

    param_list = qof_query_build_param_list (SPLIT_TRANS, TRANS_DATE_POSTED, NULL);
    qof_query_add_term (tmp_q, param_list, pred_data, QOF_QUERY_AND);
  }

  qof_query_merge_in_place (q, tmp_q, op);
  qof_query_destroy (tmp_q);
}

/********************************************************************
 * xaccQueryAddDateMatch
 * Add a date filter to an existing query. 
 ********************************************************************/

void
xaccQueryAddDateMatch(Query * q, 
                      int use_start, int sday, int smonth, int syear,
                      int use_end, int eday, int emonth, int eyear,
                      QofQueryOp op) 
{
  /* gcc -O3 will auto-inline this function, avoiding a call overhead */
  xaccQueryAddDateMatchTS (q, use_start,
                           gnc_dmy2timespec(sday, smonth, syear),
                           use_end,
                           gnc_dmy2timespec_end(eday, emonth, eyear),
                           op);
}

/********************************************************************
 * xaccQueryAddDateMatchTT
 * Add a date filter to an existing query. 
 ********************************************************************/

void
xaccQueryAddDateMatchTT(Query * q, 
                        int    use_start,
                        time_t stt,
                        int    use_end,
                        time_t ett,
                        QofQueryOp op) 
{
  Timespec   sts;
  Timespec   ets;
  
  sts.tv_sec  = (long long)stt;
  sts.tv_nsec = 0;

  ets.tv_sec  = (long long)ett;
  ets.tv_nsec = 0;

  /* gcc -O3 will auto-inline this function, avoiding a call overhead */
  xaccQueryAddDateMatchTS (q, use_start, sts,
                           use_end, ets, op);
  
}

void
xaccQueryAddClearedMatch(Query * q, cleared_match_t how, QofQueryOp op)
{
  QofQueryPredData *pred_data;
  GSList *param_list;
  char chars[6];
  int i = 0;

  if (!q)
    return;

  if (how & CLEARED_CLEARED)
    chars[i++] = CREC;
  if (how & CLEARED_RECONCILED)
    chars[i++] = YREC;
  if (how & CLEARED_FROZEN)
    chars[i++] = FREC;
  if (how & CLEARED_NO)
    chars[i++] = NREC;
  if (how & CLEARED_VOIDED)
    chars[i++] = VREC;
  chars[i] = '\0';

  pred_data = qof_query_char_predicate (QOF_CHAR_MATCH_ANY, chars);
  if (!pred_data)
    return;

  param_list = qof_query_build_param_list (SPLIT_RECONCILE, NULL);

  qof_query_add_term (q, param_list, pred_data, op);
}

void
xaccQueryAddGUIDMatch(Query * q, const GUID *guid,
		      QofIdType id_type, QofQueryOp op)
{
  GSList *param_list = NULL;

  if (!q || !guid || !id_type)
    return;

  if (!safe_strcmp (id_type, GNC_ID_SPLIT)) 
    param_list = qof_query_build_param_list (QOF_QUERY_PARAM_GUID, NULL);
  else if (!safe_strcmp (id_type, GNC_ID_TRANS))
    param_list = qof_query_build_param_list (SPLIT_TRANS, QOF_QUERY_PARAM_GUID, NULL);
  else if (!safe_strcmp (id_type, GNC_ID_ACCOUNT))
    param_list = qof_query_build_param_list (SPLIT_ACCOUNT, QOF_QUERY_PARAM_GUID, NULL);
  else
    PERR ("Invalid match type: %s", id_type);

  qof_query_add_guid_match (q, param_list, guid, op);
}

void
xaccQueryAddGUIDMatchGL (QofQuery *q, GList *param_list,
			 GUID guid, QofQueryOp op)
{
  GSList *params = NULL;
  GList *node;

  for (node = param_list; node; node = node->next)
    params = g_slist_prepend (params, node->data);

  params = g_slist_reverse (params);
  g_list_free (param_list);

  qof_query_add_guid_match (q, params, &guid, op);
}

void
xaccQueryAddKVPMatch(QofQuery *q, GSList *path, const kvp_value *value,
		     QofQueryCompare how, QofIdType id_type,
		     QofQueryOp op)
{
  GSList *param_list = NULL;
  QofQueryPredData *pred_data;

  if (!q || !path || !value || !id_type)
    return;

  pred_data = qof_query_kvp_predicate (how, path, value);
  if (!pred_data)
    return;

  if (!safe_strcmp (id_type, GNC_ID_SPLIT)) 
    param_list = qof_query_build_param_list (SPLIT_KVP, NULL);
  else if (!safe_strcmp (id_type, GNC_ID_TRANS))
    param_list = qof_query_build_param_list (SPLIT_TRANS, TRANS_KVP, NULL);
  else if (!safe_strcmp (id_type, GNC_ID_ACCOUNT))
    param_list = qof_query_build_param_list (SPLIT_ACCOUNT, ACCOUNT_KVP, NULL);
  else
    PERR ("Invalid match type: %s", id_type);

  qof_query_add_term (q, param_list, pred_data, op);
}

/*******************************************************************
 *  xaccQueryGetEarliestDateFound
 *******************************************************************/

time_t
xaccQueryGetEarliestDateFound(Query * q) 
{
  GList * spl;
  Split * sp;
  time_t earliest = LONG_MAX;

  if (!q) return 0;
  spl = qof_query_last_run (q);
  if (!spl) return 0;

  for(; spl; spl=spl->next) {
    sp = spl->data;
    if(sp->parent->date_posted.tv_sec < earliest) {
      earliest = (time_t) sp->parent->date_posted.tv_sec;
    }
  }
  return earliest;
}

/*******************************************************************
 *  xaccQueryGetLatestDateFound
 *******************************************************************/

time_t
xaccQueryGetLatestDateFound(Query * q) 
{
  Split  * sp;
  GList  * spl;
  time_t latest = 0;

  if(!q) return 0;
  spl = qof_query_last_run (q);
  if(!spl) return 0;

  for(; spl; spl=spl->next) {
    sp = spl->data;
    if(sp->parent->date_posted.tv_sec > latest) {
      latest = (time_t) sp->parent->date_posted.tv_sec;
    }
  }
  return latest;
}

void
xaccQuerySetSortOrder(Query *q, GList *p1, GList *p2, GList *p3)
{
  GSList *l1 = NULL, *l2 = NULL, *l3 = NULL;
  GList *node;

  for (node = p1; node; node = node->next)
    l1 = g_slist_prepend (l1, node->data);

  for (node = p2; node; node = node->next)
    l2 = g_slist_prepend (l2, node->data);

  for (node = p3; node; node = node->next)
    l3 = g_slist_prepend (l3, node->data);

  if (l1) l1 = g_slist_reverse (l1);
  if (l2) l2 = g_slist_reverse (l2);
  if (l3) l3 = g_slist_reverse (l3);

  if (p1) g_list_free (p1);
  if (p2) g_list_free (p2);
  if (p3) g_list_free (p3);

  qof_query_set_sort_order (q, l1, l2, l3);
}

/* ======================== END OF FILE ======================= */
