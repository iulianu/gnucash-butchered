/********************************************************************
 * gnc-engine.c  -- top-level initialization for GnuCash Engine     *
 * Copyright 2000 Bill Gribble <grib@billgribble.com>               *
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
 ********************************************************************/

#include "config.h"

#include <glib.h>
#include "gnc-engine.h"
#include "qof.h"
#include "cashobjects.h"
#include "AccountP.h"
#include "GroupP.h"
#include "SX-book-p.h"
#include "gnc-budget.h"
#include "TransactionP.h"
#include "gnc-commodity.h"
#include "gnc-lot-p.h"
#include "SchedXactionP.h"
#include "FreqSpecP.h"
#include "gnc-pricedb-p.h"

/** gnc file backend library name */
#define GNC_LIB_NAME "gnc-backend-file"

/* gnc-backend-file location */
#include "gncla-dir.h"

static GList * engine_init_hooks = NULL;
static int engine_is_initialized = 0;
//static QofLogModule log_module = GNC_MOD_ENGINE;

/* GnuCash version functions */
unsigned int
gnucash_major_version (void)
{
  return GNUCASH_MAJOR_VERSION;
}

unsigned int
gnucash_minor_version (void)
{
  return GNUCASH_MINOR_VERSION;
}

unsigned int
gnucash_micro_version (void)
{
  return GNUCASH_MICRO_VERSION;
}

/********************************************************************
 * gnc_engine_init
 * initialize backend, load any necessary databases, etc. 
 ********************************************************************/

void 
gnc_engine_init(int argc, char ** argv)
{
  static struct {
    const gchar* dir;
    const gchar* lib;
    gboolean required;
  } libs[] = {
    { QOF_LIB_DIR, QSF_BACKEND_LIB, FALSE },
    { GNC_LIBDIR, GNC_LIB_NAME, TRUE },
    /* shouldn't the PG gnc-module do this instead of US doing it? */
    { GNC_LIBDIR, "gnc-backend-postgres", FALSE },
    { NULL, NULL, FALSE } }, *lib;
  gnc_engine_init_hook_t hook;
  GList * cur;

  if (1 == engine_is_initialized) return;

  /* initialize logging to our file. */
  qof_log_init_filename("/tmp/gnucash.trace");
  /* Only set the core log_modules here the rest can be set locally.  */
  qof_log_set_level(GNC_MOD_ENGINE, QOF_LOG_WARNING);
  qof_log_set_level(GNC_MOD_IO, QOF_LOG_WARNING);
  qof_log_set_level(GNC_MOD_GUI, QOF_LOG_WARNING);
  qof_log_set_default(QOF_LOG_WARNING);
  /* initialize QOF */
  qof_init();
  qof_set_alt_dirty_mode(TRUE);

  /* Now register our core types */
  cashobjects_register();

  for (lib = libs; lib->dir && lib->lib ; lib++)
  {
      if (qof_load_backend_library(lib->dir, lib->lib))
      {
          engine_is_initialized = 1;
      }
      else
      {
          g_message("failed to load %s from %s\n", lib->lib, lib->dir);
	  /* If this is a required library, stop now! */
	  if (lib->required)
	  {
	      g_message("required library %s not found.  Exiting.\n",
			lib->lib);  
	      g_assert(FALSE);
	  }
      }
  }

  /* call any engine hooks */
  for (cur = engine_init_hooks; cur; cur = cur->next)
  {
    hook = (gnc_engine_init_hook_t)cur->data;

    if (hook)
      (*hook)(argc, argv);
  }
}

/********************************************************************
 * gnc_engine_shutdown
 * shutdown backend, destroy any global data, etc.
 ********************************************************************/

void
gnc_engine_shutdown (void)
{
  qof_log_shutdown();
  qof_close();
  engine_is_initialized = 0;
}

/********************************************************************
 * gnc_engine_add_init_hook
 * add a startup hook 
 ********************************************************************/

void
gnc_engine_add_init_hook(gnc_engine_init_hook_t h) {
  engine_init_hooks = g_list_append(engine_init_hooks, (gpointer)h);
}

gboolean
gnc_engine_is_initialized (void)
{
    return (engine_is_initialized == 1) ? TRUE : FALSE;
}

/* replicate old gnc-trace enum behaviour
 *
 * these are only here as a convenience, they could be
 * initialised elsewhere as appropriate.
 * */
void gnc_log_default(void)
{
	qof_log_set_default(QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_ENGINE, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_ACCOUNT, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_SX, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_QUERY, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_SCRUB, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_LOT, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_COMMODITY, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_BACKEND, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_PRICE, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_BUSINESS, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_IO, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_BOOK, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_GUI, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_GUILE, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_LEDGER, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_REGISTER, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_HTML, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_PREFS, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_IMPORT, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_DRUID, QOF_LOG_WARNING);
	qof_log_set_level(GNC_MOD_TEST, QOF_LOG_TRACE);
	qof_log_set_level(GNC_MOD_BUDGET, QOF_LOG_WARNING);
}

