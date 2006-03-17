/********************************************************************\
 * account-quickfill.h -- Create an account-name quick-fill         *
 * Copyright (C) 2004 Linas Vepstas <linas@linas.org>               *
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
#include "account-quickfill.h"
#include "gnc-gconf-utils.h"
#include "gnc-engine.h"
#include "gnc-ui-util.h"

/* This static indicates the debugging module that this .o belongs to. */
static QofLogModule log_module = GNC_MOD_REGISTER;

static void shared_quickfill_gconf_changed (GConfEntry *entry, gpointer qfb);
static void listen_for_account_events  (QofEntity *entity,  QofEventId event_type,
					gpointer user_data, gpointer event_data);

/* Column indices for the list store */
#define ACCOUNT_NAME        0
#define ACCOUNT_POINTER     1
#define NUM_ACCOUNT_COLUMNS 2

/* ===================================================================== */
/* In order to speed up register starts for registers htat have a huge
 * number of accounts in them (where 'huge' is >500) we build a quickfill
 * cache of account names.  This cache is needed because some users on 
 * some machines experience register open times in the tens of seconds
 * type timescales.  Building the quickfill list accounts for almost
 * all of that cpu time (about 90% of the xfer_cell build time for 600 
 * accounts).
 */

typedef struct {
  QuickFill *qf;
  gboolean load_list_store;
  GtkListStore *list_store;
  QofBook *book;
  AccountGroup *group;
  gint  listener;
  AccountBoolCB dont_add_cb;
  gpointer dont_add_data;
} QFB;

static void 
shared_quickfill_destroy (QofBook *book, gpointer key, gpointer user_data)
{
  QFB *qfb = user_data;
  gnc_gconf_general_remove_cb(KEY_ACCOUNT_SEPARATOR,
			      shared_quickfill_gconf_changed,
			      qfb);
  gnc_quickfill_destroy (qfb->qf);
  g_object_unref(qfb->list_store);
  qof_event_unregister_handler (qfb->listener);
  g_free (qfb);
}


typedef struct find_data {
  Account *account;
  GtkTreePath *found;
} find_data;

static gboolean
shared_quickfill_find_account (GtkTreeModel *model,
			       GtkTreePath *path,
			       GtkTreeIter *iter,
			       gpointer user_data)
{
  Account *account = NULL;
  find_data *data = user_data;

  gtk_tree_model_get(model, iter, ACCOUNT_POINTER, &account, -1);
  if (data->account == account) {
    data->found = gtk_tree_path_copy(path);
    return TRUE;
  }
  return FALSE;
}


/* Splat the account name into the shared quickfill object */
static gpointer
load_shared_qf_cb (Account *account, gpointer data)
{
  QFB *qfb = data;
  char *name;
  GtkTreeIter iter;

  if (qfb->dont_add_cb)
  {
     gboolean skip = (qfb->dont_add_cb) (account, qfb->dont_add_data);
     if (skip) return NULL;
  }

  name = xaccAccountGetFullName (account);
  if (NULL == name) return NULL;
  gnc_quickfill_insert (qfb->qf, name, QUICKFILL_ALPHA);
  if (qfb->load_list_store) {
    gtk_list_store_append (qfb->list_store, &iter);
    gtk_list_store_set (qfb->list_store, &iter,
			ACCOUNT_NAME, name,
			ACCOUNT_POINTER, account,
			-1);
  }
  g_free(name);

  return NULL;
}

static void
shared_quickfill_gconf_changed (GConfEntry *entry, gpointer user_data)
{
  QFB *qfb = user_data;

  /* Reload the quickfill */
  gnc_quickfill_purge(qfb->qf);
  gtk_list_store_clear(qfb->list_store);
  qfb->load_list_store = TRUE;
  xaccGroupForEachAccount (qfb->group, load_shared_qf_cb, qfb, TRUE);
  qfb->load_list_store = FALSE;
}


/* Build the quickfill list out of account names. 
 * Essentially same loop as in gnc_load_xfer_cell() above.
 */
static QFB *
build_shared_quickfill (QofBook *book, AccountGroup *group, const char * key,
                        AccountBoolCB cb, gpointer data)
{
  QFB *qfb;

  qfb = g_new0(QFB, 1);
  qfb->qf = gnc_quickfill_new ();
  qfb->book = book;
  qfb->group = group;
  qfb->listener = 0;
  qfb->dont_add_cb = cb;
  qfb->dont_add_data = data;
  qfb->load_list_store = TRUE;
  qfb->list_store =
    gtk_list_store_new (NUM_ACCOUNT_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER);

  gnc_gconf_general_register_cb(KEY_ACCOUNT_SEPARATOR,
				shared_quickfill_gconf_changed,
				qfb);

  xaccGroupForEachAccount (group, load_shared_qf_cb, qfb, TRUE);
  qfb->load_list_store = FALSE;

  qfb->listener = 
     qof_event_register_handler (listen_for_account_events, qfb);

  qof_book_set_data_fin (book, key, qfb, shared_quickfill_destroy);

  return qfb;
}

QuickFill *
gnc_get_shared_account_name_quickfill (AccountGroup *group, 
                                       const char * key, 
                                       AccountBoolCB cb, gpointer cb_data)
{
  QFB *qfb;
  QofBook *book;

  book = xaccGroupGetBook (group);
  qfb = qof_book_get_data (book, key);

  if (qfb) return qfb->qf;

  qfb = build_shared_quickfill (book, group, key, cb, cb_data);
  return qfb->qf;
}

GtkListStore *
gnc_get_shared_account_name_list_store (AccountGroup *group, 
					const char * key, 
					AccountBoolCB cb, gpointer cb_data)
{
  QFB *qfb;
  QofBook *book;

  book = xaccGroupGetBook (group);
  qfb = qof_book_get_data (book, key);

  if (qfb) return qfb->list_store;

  qfb = build_shared_quickfill (book, group, key, cb, cb_data);
  return qfb->list_store;
}

/* Since we are maintaining a 'global' quickfill list, we need to 
 * update it whenever the user creates a new account.  So listen
 * for account modification events, and add new accounts.
 */
static void
listen_for_account_events  (QofEntity *entity,  QofEventId event_type,
			    gpointer user_data, gpointer event_data)
{
  QFB *qfb = user_data;
  QuickFill *qf = qfb->qf;
  QuickFill *match;
  char * name;
  const char *match_str;
  Account *account;
  GtkTreeIter iter;
  find_data data;
  gboolean skip;

  if (0 == (event_type & (QOF_EVENT_MODIFY | QOF_EVENT_ADD | QOF_EVENT_REMOVE)))
    return;

  if (!GNC_IS_ACCOUNT (entity))
    return;
  account = GNC_ACCOUNT (entity);

  ENTER("entity %p, event type %x, user data %p, ecent data %p",
	entity, event_type, user_data, event_data);

  if (xaccAccountGetRoot(account) != qfb->group) {
       LEAVE("root group mismatch");
    return;
  }

  name = xaccAccountGetFullName (account);
  if (NULL == name) {
    LEAVE("account has no name");
    return;
  }

  /* Does the account exist in the model? */
  data.account = account;
  data.found = NULL;
  gtk_tree_model_foreach(GTK_TREE_MODEL(qfb->list_store),
			 shared_quickfill_find_account, &data);

  /* Should the account exist in the model? */
  if (qfb->dont_add_cb) {
    skip = (qfb->dont_add_cb) (account, qfb->dont_add_data);
  } else {
    skip = FALSE;
  }

  /* Synthesize new events to make the following case statement
   * simpler. */
  if (event_type == QOF_EVENT_MODIFY) {
    if (skip && data.found) {
      DEBUG("existing account now filtered");
      event_type = QOF_EVENT_REMOVE;
    } else if (!skip && !data.found) {
      DEBUG("existing account no longer filtered");
      event_type = QOF_EVENT_ADD;
    }
  }

  switch (event_type) {
    case QOF_EVENT_MODIFY:
      DEBUG("modify %s", name);

      /* Update qf */
      gnc_quickfill_purge(qfb->qf);
      xaccGroupForEachAccount (qfb->group, load_shared_qf_cb, qfb, TRUE);

      /* Update list store */
      if (data.found) {
	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(qfb->list_store),
				    &iter, data.found)) {
	  gtk_list_store_set(qfb->list_store, &iter,
			     ACCOUNT_NAME, name,
			     ACCOUNT_POINTER, account,
			     -1);
	}
      }
      break;

    case QOF_EVENT_REMOVE:
      DEBUG("remove %s", name);

      /* Remove from qf */
      gnc_quickfill_purge(qfb->qf);
      xaccGroupForEachAccount (qfb->group, load_shared_qf_cb, qfb, TRUE);

      /* Remove from list store */
      if (data.found) {
	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(qfb->list_store),
				    &iter, data.found)) {
	  gtk_list_store_remove(qfb->list_store, &iter);
	}
      }
      break;

    case QOF_EVENT_ADD:
      DEBUG("add %s", name);
      match = gnc_quickfill_get_string_match (qf, name);
      if (match) {
	match_str = gnc_quickfill_string (match);
	if (match_str && (safe_strcmp(match_str, name) != 0)) {
	  PINFO ("got match for %s", name);
	  break;
	}
      }

      PINFO ("insert new account %s into qf=%p", name, qf);
      gnc_quickfill_insert (qf, name, QUICKFILL_ALPHA);
      gtk_list_store_append (qfb->list_store, &iter);
      gtk_list_store_set (qfb->list_store, &iter,
			  ACCOUNT_NAME, name,
			  ACCOUNT_POINTER, account,
			  -1);
      break;

    default:
      DEBUG("other %s", name);
      break;
  }
  if (data.found)
    gtk_tree_path_free(data.found);
  g_free(name);
  LEAVE(" ");
}

/* ====================== END OF FILE ================================== */
