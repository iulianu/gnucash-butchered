/********************************************************************\
 * gncVendor.c -- the Core Vendor Interface                         *
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

/*
 * Copyright (C) 2001, 2002 Derek Atkins
 * Copyright (C) 2003 <linas@linas.org>
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#include "config.h"

#include <glib.h>
#include <string.h>

#include "gnc-commodity.h"
#include "gncAddressP.h"
#include "gncBillTermP.h"
#include "gncInvoice.h"
#include "gncJobP.h"
#include "gncTaxTableP.h"
#include "gncVendor.h"
#include "gncVendorP.h"

struct _gncVendor 
{
  QofInstance     inst;

  char *          id;
  char *          name;
  char *          notes;
  GncBillTerm *   terms;
  GncAddress *    addr;
  gnc_commodity * currency;
  GncTaxTable*    taxtable;
  gboolean        taxtable_override;
  GncTaxIncluded  taxincluded;
  gboolean        active;
  GList *         jobs;
};

static QofLogModule log_module = GNC_MOD_BUSINESS;

#define _GNC_MOD_NAME        GNC_ID_VENDOR

/* ============================================================ */
/* Misc inline funcs */

G_INLINE_FUNC void mark_vendor (GncVendor *vendor);
void mark_vendor (GncVendor *vendor)
{
  qof_instance_set_dirty(&vendor->inst);
  qof_event_gen (&vendor->inst.entity, QOF_EVENT_MODIFY, NULL);
}

/* ============================================================== */
/* Create/Destroy Functions */

GncVendor *gncVendorCreate (QofBook *book)
{
  GncVendor *vendor;

  if (!book) return NULL;

  vendor = g_new0 (GncVendor, 1);
  qof_instance_init (&vendor->inst, _GNC_MOD_NAME, book);
  
  vendor->id = CACHE_INSERT ("");
  vendor->name = CACHE_INSERT ("");
  vendor->notes = CACHE_INSERT ("");
  vendor->addr = gncAddressCreate (book, &vendor->inst.entity);
  vendor->taxincluded = GNC_TAXINCLUDED_USEGLOBAL;
  vendor->active = TRUE;
  vendor->jobs = NULL;

  qof_event_gen (&vendor->inst.entity, QOF_EVENT_CREATE, NULL);

  return vendor;
}

void gncVendorDestroy (GncVendor *vendor)
{
  if (!vendor) return;
  vendor->inst.do_free = TRUE;
  gncVendorCommitEdit (vendor);
}

static void gncVendorFree (GncVendor *vendor)
{
  if (!vendor) return;

  qof_event_gen (&vendor->inst.entity, QOF_EVENT_DESTROY, NULL);

  CACHE_REMOVE (vendor->id);
  CACHE_REMOVE (vendor->name);
  CACHE_REMOVE (vendor->notes);
  gncAddressDestroy (vendor->addr);
  g_list_free (vendor->jobs);

  if (vendor->terms)
    gncBillTermDecRef (vendor->terms);
  if (vendor->taxtable)
    gncTaxTableDecRef (vendor->taxtable);

  qof_instance_release (&vendor->inst);
  g_free (vendor);
}

/** Create a copy of a vendor, placing the copy into a new book. */
GncVendor *
gncCloneVendor (GncVendor *from, QofBook *book)
{
  GList *node;
  GncVendor *vendor;

  if (!book) return NULL;

  vendor = g_new0 (GncVendor, 1);
  qof_instance_init (&vendor->inst, _GNC_MOD_NAME, book);
  qof_instance_gemini (&vendor->inst, &from->inst);
  
  vendor->id = CACHE_INSERT (from->id);
  vendor->name = CACHE_INSERT (from->name);
  vendor->notes = CACHE_INSERT (from->notes);
  vendor->addr = gncCloneAddress (from->addr, &vendor->inst.entity, book);
  vendor->taxincluded = from->taxincluded;
  vendor->taxtable_override = from->taxtable_override;
  vendor->active = from->active;

  vendor->terms = gncBillTermObtainTwin (from->terms, book);
  gncBillTermIncRef (vendor->terms);

  vendor->currency = gnc_commodity_obtain_twin (from->currency, book);

  vendor->taxtable = gncTaxTableObtainTwin (from->taxtable, book);
  gncTaxTableIncRef (vendor->taxtable);

  vendor->jobs = NULL;
  for (node=g_list_last(from->jobs); node; node=node->prev)
  {
    GncJob *job = node->data;
    job = gncJobObtainTwin (job, book);
    vendor->jobs = g_list_prepend(vendor->jobs, job);
  }

  qof_event_gen (&vendor->inst.entity, QOF_EVENT_CREATE, NULL);

  return vendor;
}

GncVendor *
gncVendorObtainTwin (GncVendor *from, QofBook *book)
{
  GncVendor *vendor;
  if (!book) return NULL;

  vendor = (GncVendor *) qof_instance_lookup_twin (QOF_INSTANCE(from), book);
  if (!vendor)
  {
    vendor = gncCloneVendor (from, book);
  }

  return vendor;
}

/* ============================================================== */
/* Set Functions */

#define SET_STR(obj, member, str) { \
        char * tmp; \
        \
        if (!safe_strcmp (member, str)) return; \
        gncVendorBeginEdit (obj); \
        tmp = CACHE_INSERT (str); \
        CACHE_REMOVE (member); \
        member = tmp; \
        }

void gncVendorSetID (GncVendor *vendor, const char *id)
{
  if (!vendor) return;
  if (!id) return;
  SET_STR(vendor, vendor->id, id);
  mark_vendor (vendor);
  gncVendorCommitEdit (vendor);
}

void gncVendorSetName (GncVendor *vendor, const char *name)
{
  if (!vendor) return;
  if (!name) return;
  SET_STR(vendor, vendor->name, name);
  mark_vendor (vendor);
  gncVendorCommitEdit (vendor);
}

void gncVendorSetNotes (GncVendor *vendor, const char *notes)
{
  if (!vendor) return;
  if (!notes) return;
  SET_STR(vendor,vendor->notes, notes);
  mark_vendor (vendor);
  gncVendorCommitEdit (vendor);
}

void gncVendorSetTerms (GncVendor *vendor, GncBillTerm *terms)
{
  if (!vendor) return;
  if (vendor->terms == terms) return;

  gncVendorBeginEdit (vendor);
  if (vendor->terms)
    gncBillTermDecRef (vendor->terms);
  vendor->terms = terms;
  if (vendor->terms)
    gncBillTermIncRef (vendor->terms);
  mark_vendor (vendor);
  gncVendorCommitEdit (vendor);
}

void gncVendorSetTaxIncluded (GncVendor *vendor, GncTaxIncluded taxincl)
{
  if (!vendor) return;
  if (taxincl == vendor->taxincluded) return;
  gncVendorBeginEdit (vendor);
  vendor->taxincluded = taxincl;
  mark_vendor (vendor);
  gncVendorCommitEdit (vendor);
}

void gncVendorSetCurrency (GncVendor *vendor, gnc_commodity *currency)
{
  if (!vendor || !currency) return;
  if (vendor->currency &&
      gnc_commodity_equal (vendor->currency, currency))
    return;
  gncVendorBeginEdit (vendor);
  vendor->currency = currency;
  mark_vendor (vendor);
  gncVendorCommitEdit (vendor);
}

void gncVendorSetActive (GncVendor *vendor, gboolean active)
{
  if (!vendor) return;
  if (active == vendor->active) return;
  gncVendorBeginEdit (vendor);
  vendor->active = active;
  mark_vendor (vendor);
  gncVendorCommitEdit (vendor);
}

void gncVendorSetTaxTableOverride (GncVendor *vendor, gboolean override)
{
  if (!vendor) return;
  if (vendor->taxtable_override == override) return;
  gncVendorBeginEdit (vendor);
  vendor->taxtable_override = override;
  mark_vendor (vendor);
  gncVendorCommitEdit (vendor);
}

void gncVendorSetTaxTable (GncVendor *vendor, GncTaxTable *table)
{
  if (!vendor) return;
  if (vendor->taxtable == table) return;
  gncVendorBeginEdit (vendor);
  if (vendor->taxtable)
    gncTaxTableDecRef (vendor->taxtable);
  if (table)
    gncTaxTableIncRef (table);
  vendor->taxtable = table;
  mark_vendor (vendor);
  gncVendorCommitEdit (vendor);
}

static void
qofVendorSetAddr (GncVendor *vendor, QofEntity *addr_ent)
{
	GncAddress *addr;

	if(!vendor || !addr_ent) { return; }
	addr = (GncAddress*)addr_ent;
	if(addr == vendor->addr) { return; }
	if(vendor->addr != NULL) { gncAddressDestroy(vendor->addr); }
	gncVendorBeginEdit(vendor);
	vendor->addr = addr;
	gncVendorCommitEdit(vendor);
}

static void
qofVendorSetTaxIncluded(GncVendor *vendor, const char* type_string)
{
	GncTaxIncluded inc;

	if(!gncTaxIncludedStringToType(type_string, &inc)) { return; }
	gncVendorBeginEdit(vendor);
	vendor->taxincluded = inc;
	gncVendorCommitEdit(vendor);
}

/* ============================================================== */
/* Get Functions */

const char * gncVendorGetID (GncVendor *vendor)
{
  if (!vendor) return NULL;
  return vendor->id;
}

const char * gncVendorGetName (GncVendor *vendor)
{
  if (!vendor) return NULL;
  return vendor->name;
}

GncAddress * gncVendorGetAddr (GncVendor *vendor)
{
  if (!vendor) return NULL;
  return vendor->addr;
}

const char * gncVendorGetNotes (GncVendor *vendor)
{
  if (!vendor) return NULL;
  return vendor->notes;
}

GncBillTerm * gncVendorGetTerms (GncVendor *vendor)
{
  if (!vendor) return 0;
  return vendor->terms;
}

GncTaxIncluded gncVendorGetTaxIncluded (GncVendor *vendor)
{
  if (!vendor) return GNC_TAXINCLUDED_USEGLOBAL;
  return vendor->taxincluded;
}

gnc_commodity * gncVendorGetCurrency (GncVendor *vendor)
{
  if (!vendor) return NULL;
  return vendor->currency;
}

gboolean gncVendorGetActive (GncVendor *vendor)
{
  if (!vendor) return FALSE;
  return vendor->active;
}

gboolean gncVendorGetTaxTableOverride (GncVendor *vendor)
{
  if (!vendor) return FALSE;
  return vendor->taxtable_override;
}

GncTaxTable* gncVendorGetTaxTable (GncVendor *vendor)
{
  if (!vendor) return NULL;
  return vendor->taxtable;
}

static const char*
qofVendorGetTaxIncluded(GncVendor *vendor)
{
	return gncTaxIncludedTypeToString(vendor->taxincluded);
}

/* Note that JobList changes do not affect the "dirtiness" of the vendor */
void gncVendorAddJob (GncVendor *vendor, GncJob *job)
{
  if (!vendor) return;
  if (!job) return;

  if (g_list_index(vendor->jobs, job) == -1)
    vendor->jobs = g_list_insert_sorted (vendor->jobs, job,
                                         (GCompareFunc)gncJobCompare);

  qof_event_gen (&vendor->inst.entity, QOF_EVENT_MODIFY, NULL);
}

void gncVendorRemoveJob (GncVendor *vendor, GncJob *job)
{
  GList *node;

  if (!vendor) return;
  if (!job) return;

  node = g_list_find (vendor->jobs, job);
  if (!node) {
    /*    PERR ("split not in account"); */
  } else {
    vendor->jobs = g_list_remove_link (vendor->jobs, node);
    g_list_free_1 (node);
  }

  qof_event_gen (&vendor->inst.entity, QOF_EVENT_MODIFY, NULL);
}

void gncVendorBeginEdit (GncVendor *vendor)
{
  QOF_BEGIN_EDIT (&vendor->inst);
}

static inline void gncVendorOnError (QofInstance *vendor, QofBackendError errcode)
{
  PERR("Vendor QofBackend Failure: %d", errcode);
}

static inline void gncVendorOnDone (QofInstance *inst)
{
  GncVendor *vendor = (GncVendor *) inst;
  gncAddressClearDirty (vendor->addr);
}

static inline void vendor_free (QofInstance *inst)
{
  GncVendor *vendor = (GncVendor *) inst;
  gncVendorFree (vendor);
}

void gncVendorCommitEdit (GncVendor *vendor)
{
  if (!qof_commit_edit (QOF_INSTANCE(vendor))) return;
  qof_commit_edit_part2 (&vendor->inst, gncVendorOnError,
                         gncVendorOnDone, vendor_free);
}

/* ============================================================== */
/* Other functions */

int gncVendorCompare (GncVendor *a, GncVendor *b)
{
  if (!a && !b) return 0;
  if (!a && b) return 1;
  if (a && !b) return -1;

  return(strcmp(a->name, b->name));
}

GList * gncVendorGetJoblist (GncVendor *vendor, gboolean show_all)
{
  if (!vendor) return NULL;

  if (show_all) {
    return (g_list_copy (vendor->jobs));
  } else {
    GList *list = NULL, *iterator;
    for (iterator = vendor->jobs; iterator; iterator=iterator->next) {
      GncJob *j = iterator->data;
      if (gncJobGetActive (j))
        list = g_list_append (list, j);
    }
    return list;
  }
}

gboolean gncVendorIsDirty (GncVendor *vendor)
{
  if (!vendor) return FALSE;
  return (vendor->inst.dirty || gncAddressIsDirty (vendor->addr));
}

/* ============================================================== */
/* Package-Private functions */

static const char * _gncVendorPrintable (gpointer item)
{
  GncVendor *v = item;
  if (!item) return NULL;
  return v->name;
}

static QofObject gncVendorDesc = 
{
  interface_version:  QOF_OBJECT_VERSION,
  e_type:             _GNC_MOD_NAME,
  type_label:         "Vendor",
  create:             (gpointer)gncVendorCreate,
  book_begin:         NULL,
  book_end:           NULL,
  is_dirty:           qof_collection_is_dirty,
  mark_clean:         qof_collection_mark_clean,
  foreach:            qof_collection_foreach,
  printable:          _gncVendorPrintable,
  version_cmp:        (int (*)(gpointer, gpointer)) qof_instance_version_cmp,
};

gboolean gncVendorRegister (void)
{
  static QofParam params[] = {
    { VENDOR_ID, QOF_TYPE_STRING, (QofAccessFunc)gncVendorGetID, (QofSetterFunc)gncVendorSetID },
    { VENDOR_NAME, QOF_TYPE_STRING, (QofAccessFunc)gncVendorGetName, (QofSetterFunc)gncVendorSetName },
    { VENDOR_ADDR,    GNC_ID_ADDRESS, (QofAccessFunc)gncVendorGetAddr, (QofSetterFunc)qofVendorSetAddr },
    { VENDOR_NOTES,   QOF_TYPE_STRING, (QofAccessFunc)gncVendorGetNotes, (QofSetterFunc)gncVendorSetNotes },
    { VENDOR_TERMS,   GNC_ID_BILLTERM, (QofAccessFunc)gncVendorGetTerms, (QofSetterFunc)gncVendorSetTerms },
    { VENDOR_TAX_OVERRIDE, QOF_TYPE_BOOLEAN, (QofAccessFunc)gncVendorGetTaxTableOverride,
		(QofSetterFunc)gncVendorSetTaxTableOverride },
    { VENDOR_TAX_TABLE, GNC_ID_TAXTABLE, (QofAccessFunc)gncVendorGetTaxTable,
		(QofSetterFunc)gncVendorSetTaxTable },
    { VENDOR_TAX_INC, QOF_TYPE_STRING, (QofAccessFunc)qofVendorGetTaxIncluded, 
		(QofSetterFunc)qofVendorSetTaxIncluded},
    { QOF_PARAM_BOOK, QOF_ID_BOOK, (QofAccessFunc)qof_instance_get_book, NULL },
    { QOF_PARAM_GUID, QOF_TYPE_GUID, (QofAccessFunc)qof_instance_get_guid, NULL },
    { QOF_PARAM_ACTIVE, QOF_TYPE_BOOLEAN, (QofAccessFunc)gncVendorGetActive, NULL },
    { NULL },
  };

  if(!qof_choice_add_class(GNC_ID_INVOICE, GNC_ID_VENDOR, INVOICE_OWNER)) { return FALSE; }
  if(!qof_choice_add_class(GNC_ID_JOB, GNC_ID_VENDOR, JOB_OWNER)) { return FALSE; }

  qof_class_register (_GNC_MOD_NAME, (QofSortFunc)gncVendorCompare, params);

  return qof_object_register (&gncVendorDesc);
}

gint64 gncVendorNextID (QofBook *book)
{
  return qof_book_get_counter (book, _GNC_MOD_NAME);
}
