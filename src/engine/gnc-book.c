/********************************************************************\
 * gnc-book.c -- dataset access (set of accounting books)           *
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

/*
 * FILE:
 * gnc-book.c
 *
 * FUNCTION:
 * Encapsulate all the information about a gnucash dataset.
 * See src/doc/books.txt for design overview.
 *
 * HISTORY:
 * Created by Linas Vepstas December 1998
 * Copyright (c) 1998-2001 Linas Vepstas
 * Copyright (c) 2000 Dave Peticolas
 */

#include "config.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>

#include "Backend.h"
#include "BackendP.h"
#include "GroupP.h"
#include "SchedXaction.h"
#include "TransLog.h"
#include "engine-helpers.h"
#include "gnc-engine-util.h"
#include "gnc-pricedb-p.h"
#include "date.h"
#include "gnc-book.h"
#include "gnc-book-p.h"
#include "gnc-engine.h"
#include "gnc-engine-util.h"
#include "gnc-event.h"
#include "gnc-event-p.h"
#include "gnc-module.h"
#include "gncObjectP.h"
#include "QueryObject.h"

static short module = MOD_IO;

/* ====================================================================== */
/* constructor / destructor */

static void
gnc_book_init (GNCBook *book)
{
  if (!book) return;

  book->entity_table = xaccEntityTableNew ();

  xaccGUIDNew(&book->guid, book);
  xaccStoreEntity(book->entity_table, book, &book->guid, GNC_ID_BOOK);

  book->kvp_data = kvp_frame_new ();
  book->topgroup = xaccMallocAccountGroup(book);
  book->pricedb = gnc_pricedb_create(book);

  book->sched_xactions = NULL;
  book->sx_notsaved = FALSE;
  book->template_group = xaccMallocAccountGroup(book);
  book->commodity_table = gnc_commodity_table_new ();

  if(book->commodity_table)
  {
    if(!gnc_commodity_table_add_default_data(book->commodity_table))
      PWARN("unable to initialize book's commodity_table");
  }

  book->data_tables = g_hash_table_new (g_str_hash, g_str_equal);

  book->book_open = 'y';
  book->version = 0;
  book->idata = 0;
}

GNCBook *
gnc_book_new (void)
{
  GNCBook *book;

  ENTER (" ");
  book = g_new0(GNCBook, 1);
  gnc_book_init(book);
  gncObjectBookBegin (book);

#if 0
  gnc_engine_generate_event (&book->guid, GNC_EVENT_CREATE);
#endif
  LEAVE ("book=%p", book);
  return book;
}

void
gnc_book_destroy (GNCBook *book) 
{
  if (!book) return;

  ENTER ("book=%p", book);
  gnc_engine_generate_event (&book->guid, GNC_EVENT_DESTROY);

  gncObjectBookEnd (book);

  xaccAccountGroupBeginEdit (book->topgroup);
  xaccAccountGroupDestroy (book->topgroup);
  book->topgroup = NULL;

  gnc_pricedb_destroy (book->pricedb);
  book->pricedb = NULL;

  gnc_commodity_table_destroy (book->commodity_table);
  book->commodity_table = NULL;

  /* FIXME: destroy SX data members here, too */

  xaccRemoveEntity (book->entity_table, &book->guid);
  xaccEntityTableDestroy (book->entity_table);
  book->entity_table = NULL;

  /* FIXME: Make sure the data_table is empty */
  g_hash_table_destroy (book->data_tables);

  xaccLogEnable();

  g_free (book);
  LEAVE ("book=%p", book);
}

/* ====================================================================== */
/* getters */

const GUID *
gnc_book_get_guid (GNCBook *book)
{
  if (!book) return NULL;
  return &book->guid;
}

kvp_frame *
gnc_book_get_slots (GNCBook *book)
{
  if (!book) return NULL;
  return book->kvp_data;
}

GNCEntityTable *
gnc_book_get_entity_table (GNCBook *book)
{
  if (!book) return NULL;
  return book->entity_table;
}

gnc_commodity_table *
gnc_book_get_commodity_table(GNCBook *book)
{
  if (!book) return NULL;
  return book->commodity_table;
}

AccountGroup * 
gnc_book_get_group (GNCBook *book)
{
   if (!book) return NULL;
   return book->topgroup;
}

GNCPriceDB *
gnc_book_get_pricedb(GNCBook *book)
{
  if (!book) return NULL;
  return book->pricedb;
}

GList *
gnc_book_get_schedxactions( GNCBook *book )
{
        if ( book == NULL ) return NULL;
        return book->sched_xactions;
}

AccountGroup *
gnc_book_get_template_group( GNCBook *book )
{
  if (!book) return NULL;
  return book->template_group;
}

Backend * 
xaccGNCBookGetBackend (GNCBook *book)
{
   if (!book) return NULL;
   return book->backend;
}

/* ====================================================================== */
/* setters */

void
gnc_book_set_guid (GNCBook *book, GUID uid)
{
  if (!book) return;

  if (guid_equal (&book->guid, &uid)) return;

  xaccRemoveEntity(book->entity_table, &book->guid);
  book->guid = uid;
  xaccStoreEntity(book->entity_table, book, &book->guid, GNC_ID_BOOK);
}

void
gnc_book_set_group (GNCBook *book, AccountGroup *grp)
{
  if (!book) return;

  /* Do not free the old topgroup here unless you also fix
   * all the other uses of gnc_book_set_group! 
   */

  if (book->topgroup == grp)
    return;

  if (grp->book != book)
  {
     PERR ("cannot mix and match books freely!");
     return;
  }
  /* Note: code that used to be here to set/reset the book
   * a group belonged to was removed, mostly because it 
   * was wrong: You can't just change the book a group
   * belongs to without also dealing with the entity
   * tables.  The previous code would have allowed entity
   * tables to reside in one book, while the groups were 
   * in another.  Note: please remove this comment sometime 
   * after gnucash-1.8 goes out.
   */

  book->topgroup = grp;
}

void
gnc_book_set_backend (GNCBook *book, Backend *be)
{
  if (!book) return;
  ENTER ("book=%p be=%p", book, be);
  book->backend = be;
}

gpointer gnc_book_get_backend (GNCBook *book)
{
  if (!book) return NULL;
  return (gpointer)book->backend;
}


void
gnc_book_set_pricedb(GNCBook *book, GNCPriceDB *db)
{
  if(!book) return;
  book->pricedb = db;
  if (db) db->book = book;
}

/* ====================================================================== */

void
gnc_book_set_schedxactions( GNCBook *book, GList *newList )
{
  if ( book == NULL ) return;

  book->sched_xactions = newList;
  book->sx_notsaved = TRUE;
}

void
gnc_book_set_template_group (GNCBook *book, AccountGroup *templateGroup)
{
  if (!book) return;

  if (book->template_group == templateGroup)
    return;

  if (templateGroup->book != book)
  {
     PERR ("cannot mix and match books freely!");
     return;
  }

  book->template_group = templateGroup;
}

/* ====================================================================== */
/* dirty flag stuff */

static void
mark_sx_clean(gpointer data, gpointer user_data)
{
  SchedXaction *sx = (SchedXaction *) data;
  xaccSchedXactionSetDirtyness(sx, FALSE);
  return;
}

static void
book_sxns_mark_saved(GNCBook *book)
{
  book->sx_notsaved = FALSE;
  g_list_foreach(gnc_book_get_schedxactions(book),
                 mark_sx_clean, 
                 NULL);
  return;
}

static gboolean
book_sxlist_notsaved(GNCBook *book)
{
  GList *sxlist;
  SchedXaction *sx;
  if(book->sx_notsaved
     ||
     xaccGroupNotSaved(book->template_group)) return TRUE;
 
  for(sxlist = book->sched_xactions;
      sxlist != NULL;
      sxlist = g_list_next(sxlist))
  {
    sx = (SchedXaction *) (sxlist->data);
    if (xaccSchedXactionIsDirty( sx ))
      return TRUE;
  }

  return FALSE;
}
  
void
gnc_book_mark_saved(GNCBook *book)
{
  if (!book) return;

  book->dirty = FALSE;

  xaccGroupMarkSaved(gnc_book_get_group(book));
  gnc_pricedb_mark_clean(gnc_book_get_pricedb(book));

  xaccGroupMarkSaved(gnc_book_get_template_group(book));
  book_sxns_mark_saved(book);

  /* Mark everything as clean */
  gncObjectMarkClean (book);
}

gboolean
gnc_book_not_saved(GNCBook *book)
{
  if (!book) return FALSE;

  return(book->dirty
	 ||
	 xaccGroupNotSaved(book->topgroup)
         ||
         gnc_pricedb_dirty(book->pricedb)
         ||
         book_sxlist_notsaved(book)
         ||
         gncObjectIsDirty (book));
}

void gnc_book_kvp_changed (GNCBook *book)
{
  if (!book) return;
  book->dirty = TRUE;
}

/* ====================================================================== */

gboolean
gnc_book_equal (GNCBook *book_1, GNCBook *book_2)
{
  if (book_1 == book_2) return TRUE;
  if (!book_1 || !book_2) return FALSE;

  if (!xaccGroupEqual (gnc_book_get_group (book_1),
                       gnc_book_get_group (book_2),
                       TRUE))
  {
    PWARN ("groups differ");
    return FALSE;
  }

  if (!gnc_pricedb_equal (gnc_book_get_pricedb (book_1),
                          gnc_book_get_pricedb (book_2)))
  {
    PWARN ("price dbs differ");
    return FALSE;
  }

  if (!gnc_commodity_table_equal (gnc_book_get_commodity_table (book_1),
                                  gnc_book_get_commodity_table (book_2)))
  {
    PWARN ("commodity tables differ");
    return FALSE;
  }

  /* FIXME: do scheduled transactions and template group */

  return TRUE;
}

/* ====================================================================== */

/* Store arbitrary pointers in the GNCBook for data storage extensibility */
void gnc_book_set_data (GNCBook *book, const char *key, gpointer data)
{
  if (!book || !key || !data) return;
  g_hash_table_insert (book->data_tables, (gpointer)key, data);
}

gpointer gnc_book_get_data (GNCBook *book, const char *key)
{
  if (!book || !key) return NULL;
  return g_hash_table_lookup (book->data_tables, (gpointer)key);
}

/* ====================================================================== */

static gboolean
counter_thunk(Transaction *t, void *data)
{
    (*((guint*)data))++;
    return TRUE;
}

guint
gnc_book_count_transactions(GNCBook *book)
{
    guint count = 0;
    xaccGroupForEachTransaction(gnc_book_get_group(book),
                                counter_thunk, (void*)&count);
    return count;
}

/* ====================================================================== */

gint64
gnc_book_get_counter (GNCBook *book, const char *counter_name)
{
  Backend *be;
  kvp_frame *kvp;
  kvp_value *value;
  gint64 counter;

  if (!book) {
    PWARN ("No book!!!");
    return -1;
  }

  if (!counter_name || *counter_name == '\0') {
    PWARN ("Invalid counter name.");
    return -1;
  }

  /* If we've got a backend with a counter method, call it */
  be = book->backend;
  if (be && be->counter)
    return ((be->counter)(be, counter_name));

  /* If not, then use the KVP in the book */
  kvp = gnc_book_get_slots (book);

  if (!kvp) {
    PWARN ("Book has no KVP_Frame");
    return -1;
  }

  value = kvp_frame_get_slot_path (kvp, "counters", counter_name, NULL);
  if (value) {
    /* found it */
    counter = kvp_value_get_gint64 (value);
  } else {
    /* New counter */
    counter = 0;
  }

  /* Counter is now valid; increment it */
  counter++;

  /* Save off the new counter */
  value = kvp_value_new_gint64 (counter);
  kvp_frame_set_slot_path (kvp, value, "counters", counter_name, NULL);
  kvp_value_delete (value);

  /* and return the value */
  return counter;
}

/* gncObject function implementation and registration */
gboolean gnc_book_register (void)
{
  static QueryObjectDef params[] = {
    { BOOK_KVP, QUERYCORE_KVP, (QueryAccess)gnc_book_get_slots },
    { QUERY_PARAM_GUID, QUERYCORE_GUID, (QueryAccess)gnc_book_get_guid },
    { NULL },
  };

  gncQueryObjectRegister (GNC_ID_BOOK, NULL, params);

  return TRUE;
}



/* ========================== END OF FILE =============================== */
