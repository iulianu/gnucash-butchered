/*
 * gncCustomer.c -- the Core Customer Interface
 * Copyright (C) 2001,2002 Derek Atkins
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#include "config.h"

#include <glib.h>
#include <string.h>

#include "messages.h"
#include "gnc-engine-util.h"
#include "GNCIdP.h"
#include "gnc-book-p.h"
#include "gnc-numeric.h"
#include "gncObject.h"
#include "QueryObject.h"
#include "gnc-event-p.h"
#include "gnc-be-utils.h"

#include "gncBusiness.h"
#include "gncCustomer.h"
#include "gncCustomerP.h"
#include "gncAddress.h"

struct _gncCustomer {
  GNCBook *	book;
  GUID		guid;
  char *	id;
  char *	name;
  char *	notes;
  GncBillTerm *	terms;
  GncAddress *	addr;
  GncAddress *	shipaddr;
  gnc_commodity	* currency;
  gnc_numeric	discount;
  gnc_numeric	credit;
  GncTaxIncluded taxincluded;
  gboolean	active;
  GList *	jobs;

  int		editlevel;
  gboolean	do_free;

  GncTaxTable*	taxtable;
  gboolean	taxtable_override;
  gboolean	dirty;
};

static short	module = MOD_BUSINESS;

#define _GNC_MOD_NAME	GNC_CUSTOMER_MODULE_NAME

#define CACHE_INSERT(str) g_cache_insert(gnc_engine_get_string_cache(), (gpointer)(str));
#define CACHE_REMOVE(str) g_cache_remove(gnc_engine_get_string_cache(), (str));

static void addObj (GncCustomer *cust);
static void remObj (GncCustomer *cust);

G_INLINE_FUNC void mark_customer (GncCustomer *customer);
G_INLINE_FUNC void
mark_customer (GncCustomer *customer)
{
  customer->dirty = TRUE;
  gncBusinessSetDirtyFlag (customer->book, _GNC_MOD_NAME, TRUE);

  gnc_engine_generate_event (&customer->guid, GNC_EVENT_MODIFY);
}

/* Create/Destroy Functions */

GncCustomer *gncCustomerCreate (GNCBook *book)
{
  GncCustomer *cust;

  if (!book) return NULL;

  cust = g_new0 (GncCustomer, 1);
  cust->book = book;
  cust->dirty = FALSE;
  cust->id = CACHE_INSERT ("");
  cust->name = CACHE_INSERT ("");
  cust->notes = CACHE_INSERT ("");
  cust->addr = gncAddressCreate (book, &cust->guid);
  cust->shipaddr = gncAddressCreate (book, &cust->guid);
  cust->discount = gnc_numeric_zero();
  cust->credit = gnc_numeric_zero();
  cust->taxincluded = GNC_TAXINCLUDED_USEGLOBAL;
  cust->active = TRUE;
  cust->jobs = NULL;

  xaccGUIDNew (&cust->guid, book);
  addObj (cust);

  gnc_engine_generate_event (&cust->guid, GNC_EVENT_CREATE);

  return cust;
}

void gncCustomerDestroy (GncCustomer *cust)
{
  if (!cust) return;
  cust->do_free = TRUE;
  gncCustomerCommitEdit (cust);
}

static void gncCustomerFree (GncCustomer *cust)
{
  if (!cust) return;

  gnc_engine_generate_event (&cust->guid, GNC_EVENT_DESTROY);

  CACHE_REMOVE (cust->id);
  CACHE_REMOVE (cust->name);
  CACHE_REMOVE (cust->notes);
  gncAddressDestroy (cust->addr);
  gncAddressDestroy (cust->shipaddr);
  g_list_free (cust->jobs);

  remObj (cust);

  g_free (cust);
}

/* Set Functions */

#define SET_STR(obj, member, str) { \
	char * tmp; \
	\
	if (!safe_strcmp (member, str)) return; \
	gncCustomerBeginEdit (obj); \
	tmp = CACHE_INSERT (str); \
	CACHE_REMOVE (member); \
	member = tmp; \
	}

void gncCustomerSetID (GncCustomer *cust, const char *id)
{
  if (!cust) return;
  if (!id) return;
  SET_STR(cust, cust->id, id);
  mark_customer (cust);
  gncCustomerCommitEdit (cust);
}

void gncCustomerSetName (GncCustomer *cust, const char *name)
{
  if (!cust) return;
  if (!name) return;
  SET_STR(cust, cust->name, name);
  mark_customer (cust);
  gncCustomerCommitEdit (cust);
}

void gncCustomerSetNotes (GncCustomer *cust, const char *notes)
{
  if (!cust) return;
  if (!notes) return;
  SET_STR(cust, cust->notes, notes);
  mark_customer (cust);
  gncCustomerCommitEdit (cust);
}

void gncCustomerSetGUID (GncCustomer *cust, const GUID *guid)
{
  if (!cust || !guid) return;
  if (guid_equal (guid, &cust->guid)) return;

  gncCustomerBeginEdit (cust);
  remObj (cust);
  cust->guid = *guid;
  addObj (cust);
  gncCustomerCommitEdit (cust);
}

void gncCustomerSetTerms (GncCustomer *cust, GncBillTerm *terms)
{
  if (!cust) return;
  if (cust->terms == terms) return;

  gncCustomerBeginEdit (cust);
  if (cust->terms)
    gncBillTermDecRef (cust->terms);
  cust->terms = terms;
  if (cust->terms)
    gncBillTermIncRef (cust->terms);
  mark_customer (cust);
  gncCustomerCommitEdit (cust);
}

void gncCustomerSetTaxIncluded (GncCustomer *cust, GncTaxIncluded taxincl)
{
  if (!cust) return;
  if (taxincl == cust->taxincluded) return;
  gncCustomerBeginEdit (cust);
  cust->taxincluded = taxincl;
  mark_customer (cust);
  gncCustomerCommitEdit (cust);
}

void gncCustomerSetActive (GncCustomer *cust, gboolean active)
{
  if (!cust) return;
  if (active == cust->active) return;
  gncCustomerBeginEdit (cust);
  cust->active = active;
  mark_customer (cust);
  gncCustomerCommitEdit (cust);
}

void gncCustomerSetDiscount (GncCustomer *cust, gnc_numeric discount)
{
  if (!cust) return;
  if (gnc_numeric_equal (discount, cust->discount)) return;
  gncCustomerBeginEdit (cust);
  cust->discount = discount;
  mark_customer (cust);
  gncCustomerCommitEdit (cust);
}

void gncCustomerSetCredit (GncCustomer *cust, gnc_numeric credit)
{
  if (!cust) return;
  if (gnc_numeric_equal (credit, cust->credit)) return;
  gncCustomerBeginEdit (cust);
  cust->credit = credit;
  mark_customer (cust);
  gncCustomerCommitEdit (cust);
}

void gncCustomerSetCurrency (GncCustomer *cust, gnc_commodity *currency)
{
  if (!cust || !currency) return;
  if (cust->currency && gnc_commodity_equal (cust->currency, currency)) return;
  gncCustomerBeginEdit (cust);
  cust->currency = currency;
  mark_customer (cust);
  gncCustomerCommitEdit (cust);
}

void gncCustomerSetTaxTableOverride (GncCustomer *customer, gboolean override)
{
  if (!customer) return;
  if (customer->taxtable_override == override) return;
  gncCustomerBeginEdit (customer);
  customer->taxtable_override = override;
  mark_customer (customer);
  gncCustomerCommitEdit (customer);
}

void gncCustomerSetTaxTable (GncCustomer *customer, GncTaxTable *table)
{
  if (!customer) return;
  if (customer->taxtable == table) return;

  gncCustomerBeginEdit (customer);
  if (customer->taxtable)
    gncTaxTableDecRef (customer->taxtable);
  if (table)
    gncTaxTableIncRef (table);
  customer->taxtable = table;
  mark_customer (customer);
  gncCustomerCommitEdit (customer);
}

/* Note that JobList changes do not affect the "dirtiness" of the customer */
void gncCustomerAddJob (GncCustomer *cust, GncJob *job)
{
  if (!cust) return;
  if (!job) return;

  if (g_list_index(cust->jobs, job) == -1)
    cust->jobs = g_list_insert_sorted (cust->jobs, job,
				       (GCompareFunc)gncJobCompare);

  gnc_engine_generate_event (&cust->guid, GNC_EVENT_MODIFY);
}

void gncCustomerRemoveJob (GncCustomer *cust, GncJob *job)
{
  GList *node;

  if (!cust) return;
  if (!job) return;

  node = g_list_find (cust->jobs, job);
  if (!node) {
    /*    PERR ("split not in account"); */
  } else {
    cust->jobs = g_list_remove_link (cust->jobs, node);
    g_list_free_1 (node);
  }
  gnc_engine_generate_event (&cust->guid, GNC_EVENT_MODIFY);
}

void gncCustomerBeginEdit (GncCustomer *cust)
{
  GNC_BEGIN_EDIT (cust, _GNC_MOD_NAME);
}

static void gncCustomerOnError (GncCustomer *cust, GNCBackendError errcode)
{
  PERR("Customer Backend Failure: %d", errcode);
}

static void gncCustomerOnDone (GncCustomer *cust)
{
  cust->dirty = FALSE;
  gncAddressClearDirty (cust->addr);
  gncAddressClearDirty (cust->shipaddr);
}

void gncCustomerCommitEdit (GncCustomer *cust)
{
  GNC_COMMIT_EDIT_PART1 (cust);
  GNC_COMMIT_EDIT_PART2 (cust, _GNC_MOD_NAME, gncCustomerOnError,
			 gncCustomerOnDone, gncCustomerFree);
}

/* Get Functions */

GNCBook * gncCustomerGetBook (GncCustomer *cust)
{
  if (!cust) return NULL;
  return cust->book;
}

const GUID * gncCustomerGetGUID (GncCustomer *cust)
{
  if (!cust) return NULL;
  return &cust->guid;
}

const char * gncCustomerGetID (GncCustomer *cust)
{
  if (!cust) return NULL;
  return cust->id;
}

const char * gncCustomerGetName (GncCustomer *cust)
{
  if (!cust) return NULL;
  return cust->name;
}

GncAddress * gncCustomerGetAddr (GncCustomer *cust)
{
  if (!cust) return NULL;
  return cust->addr;
}

GncAddress * gncCustomerGetShipAddr (GncCustomer *cust)
{
  if (!cust) return NULL;
  return cust->shipaddr;
}

const char * gncCustomerGetNotes (GncCustomer *cust)
{
  if (!cust) return NULL;
  return cust->notes;
}

GncBillTerm * gncCustomerGetTerms (GncCustomer *cust)
{
  if (!cust) return NULL;
  return cust->terms;
}

GncTaxIncluded gncCustomerGetTaxIncluded (GncCustomer *cust)
{
  if (!cust) return GNC_TAXINCLUDED_USEGLOBAL;
  return cust->taxincluded;
}

gnc_commodity * gncCustomerGetCurrency (GncCustomer *cust)
{
  if (!cust) return NULL;
  return cust->currency;
}

gboolean gncCustomerGetActive (GncCustomer *cust)
{
  if (!cust) return FALSE;
  return cust->active;
}

gnc_numeric gncCustomerGetDiscount (GncCustomer *cust)
{
  if (!cust) return gnc_numeric_zero();
  return cust->discount;
}

gnc_numeric gncCustomerGetCredit (GncCustomer *cust)
{
  if (!cust) return gnc_numeric_zero();
  return cust->credit;
}

gboolean gncCustomerGetTaxTableOverride (GncCustomer *customer)
{
  if (!customer) return FALSE;
  return customer->taxtable_override;
}

GncTaxTable* gncCustomerGetTaxTable (GncCustomer *customer)
{
  if (!customer) return NULL;
  return customer->taxtable;
}

GList * gncCustomerGetJoblist (GncCustomer *cust, gboolean show_all)
{
  if (!cust) return NULL;

  if (show_all) {
    return (g_list_copy (cust->jobs));
  } else {
    GList *list = NULL, *iterator;
    for (iterator = cust->jobs; iterator; iterator=iterator->next) {
      GncJob *j = iterator->data;
      if (gncJobGetActive (j))
	list = g_list_append (list, j);
    }
    return list;
  }
}

GUID gncCustomerRetGUID (GncCustomer *customer)
{
  if (!customer)
    return *xaccGUIDNULL();

  return customer->guid;
}

GncCustomer * gncCustomerLookupDirect (GUID guid, GNCBook *book)
{
  if (!book) return NULL;
  return gncCustomerLookup (book, &guid);
}

GncCustomer * gncCustomerLookup (GNCBook *book, const GUID *guid)
{
  if (!book || !guid) return NULL;
  return xaccLookupEntity (gnc_book_get_entity_table (book),
			   guid, _GNC_MOD_NAME);
}

gboolean gncCustomerIsDirty (GncCustomer *cust)
{
  if (!cust) return FALSE;
  return (cust->dirty ||
	  gncAddressIsDirty (cust->addr) ||
	  gncAddressIsDirty (cust->shipaddr));
}

/* Other functions */

int gncCustomerCompare (GncCustomer *a, GncCustomer *b)
{
  if (!a && !b) return 0;
  if (!a && b) return 1;
  if (a && !b) return -1;

  return(strcmp(a->name, b->name));
}

/* Package-Private functions */

static void addObj (GncCustomer *cust)
{
  gncBusinessAddObject (cust->book, _GNC_MOD_NAME, cust, &cust->guid);
}

static void remObj (GncCustomer *cust)
{
  gncBusinessRemoveObject (cust->book, _GNC_MOD_NAME, &cust->guid);
}

static void _gncCustomerCreate (GNCBook *book)
{
  gncBusinessCreate (book, _GNC_MOD_NAME);
}

static void _gncCustomerDestroy (GNCBook *book)
{
  return gncBusinessDestroy (book, _GNC_MOD_NAME);
}

static gboolean _gncCustomerIsDirty (GNCBook *book)
{
  return gncBusinessIsDirty (book, _GNC_MOD_NAME);
}

static void _gncCustomerMarkClean (GNCBook *book)
{
  gncBusinessSetDirtyFlag (book, _GNC_MOD_NAME, FALSE);
}

static void _gncCustomerForeach (GNCBook *book, foreachObjectCB cb,
				 gpointer user_data)
{
  gncBusinessForeach (book, _GNC_MOD_NAME, cb, user_data);
}

static const char * _gncCustomerPrintable (gpointer item)
{
  GncCustomer *c;

  if (!item) return NULL;

  c = item;
  return c->name;
}

static GncObject_t gncCustomerDesc = {
  GNC_OBJECT_VERSION,
  _GNC_MOD_NAME,
  "Customer",
  _gncCustomerCreate,
  _gncCustomerDestroy,
  _gncCustomerIsDirty,
  _gncCustomerMarkClean,
  _gncCustomerForeach,
  _gncCustomerPrintable,
};

gboolean gncCustomerRegister (void)
{
  static QueryObjectDef params[] = {
    { CUSTOMER_ID, QUERYCORE_STRING, (QueryAccess)gncCustomerGetID },
    { CUSTOMER_NAME, QUERYCORE_STRING, (QueryAccess)gncCustomerGetName },
    { CUSTOMER_ADDR, GNC_ADDRESS_MODULE_NAME, (QueryAccess)gncCustomerGetAddr },
    { CUSTOMER_SHIPADDR, GNC_ADDRESS_MODULE_NAME, (QueryAccess)gncCustomerGetShipAddr },
    { QUERY_PARAM_BOOK, GNC_ID_BOOK, (QueryAccess)gncCustomerGetBook },
    { QUERY_PARAM_GUID, QUERYCORE_GUID, (QueryAccess)gncCustomerGetGUID },
    { QUERY_PARAM_ACTIVE, QUERYCORE_BOOLEAN, (QueryAccess)gncCustomerGetActive },
    { NULL },
  };

  gncQueryObjectRegister (_GNC_MOD_NAME, (QuerySort)gncCustomerCompare,params);

  return gncObjectRegister (&gncCustomerDesc);
}

gint64 gncCustomerNextID (GNCBook *book)
{
  return gnc_book_get_counter (book, _GNC_MOD_NAME);
}
