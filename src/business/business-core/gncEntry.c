/*
 * gncEntry.c -- the Core Business Entry Interface
 * Copyright (C) 2001,2002 Derek Atkins
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#include "config.h"

#include <glib.h>

#include "messages.h"
#include "gnc-book.h"
#include "gnc-commodity.h"
#include "gnc-engine-util.h"
#include "gnc-event-p.h"
#include "gnc-numeric.h"
#include "GNCIdP.h"
#include "QueryObject.h"
#include "gnc-be-utils.h"

#include "gncBusiness.h"
#include "gncEntry.h"
#include "gncEntryP.h"
#include "gncInvoice.h"
#include "gncOrder.h"

struct _gncEntry {
  GNCBook *	book;

  GUID		guid;
  Timespec	date;
  Timespec	date_entered;
  char *	desc;
  char *	action;
  char *	notes;
  gnc_numeric 	quantity;

  /* customer invoice data */
  Account *	i_account;
  gnc_numeric 	i_price;
  gboolean	i_taxable;
  gboolean	i_taxincluded;
  GncTaxTable *	i_tax_table;
  gnc_numeric 	i_discount;
  GncAmountType	i_disc_type;
  GncDiscountHow i_disc_how;

  /* vendor bill data */
  Account *	b_account;
  gnc_numeric 	b_price;
  gboolean	b_taxable;
  gboolean	b_taxincluded;
  GncTaxTable *	b_tax_table;
  gboolean	billable;
  GncOwner	billto;

  /* employee bill data */
  GncEntryPaymentType b_payment;

  /* my parent(s) */
  GncOrder *	order;
  GncInvoice *	invoice;
  GncInvoice *	bill;

  int		editlevel;
  gboolean	do_free;
  gboolean	dirty;

  /* CACHED VALUES */

  gboolean	values_dirty;

  /* customer invoice */
  gnc_numeric	i_value;
  gnc_numeric	i_value_rounded;
  GList *	i_tax_values;
  gnc_numeric	i_tax_value;
  gnc_numeric	i_tax_value_rounded;
  gnc_numeric	i_disc_value;
  gnc_numeric	i_disc_value_rounded;
  Timespec	i_taxtable_modtime;

  /* vendor bill */
  gnc_numeric	b_value;
  gnc_numeric	b_value_rounded;
  GList *	b_tax_values;
  gnc_numeric	b_tax_value;
  gnc_numeric	b_tax_value_rounded;
  Timespec	b_taxtable_modtime;
};

static short	module = MOD_BUSINESS;

/* You must edit the functions in this block in tandem.  KEEP THEM IN
   SYNC! */

#define GNC_RETURN_ENUM_AS_STRING(x,s) case (x): return (s);
const char *
gncEntryDiscountHowToString (GncDiscountHow how)
{
  switch(how)
  {
    GNC_RETURN_ENUM_AS_STRING(GNC_DISC_PRETAX, "PRETAX");
    GNC_RETURN_ENUM_AS_STRING(GNC_DISC_SAMETIME, "SAMETIME");
    GNC_RETURN_ENUM_AS_STRING(GNC_DISC_POSTTAX, "POSTTAX");
    default:
      g_warning ("asked to translate unknown discount-how %d.\n", how);
      break;
  }
  return(NULL);
}

const char * gncEntryPaymentTypeToString (GncEntryPaymentType type)
{
  switch(type)
  {
    GNC_RETURN_ENUM_AS_STRING(GNC_PAYMENT_CASH, "CASH");
    GNC_RETURN_ENUM_AS_STRING(GNC_PAYMENT_CARD, "CARD");
    default:
      g_warning ("asked to translate unknown payment type %d.\n", type);
      break;
  }
  return(NULL);
}
#undef GNC_RETURN_ENUM_AS_STRING
#define GNC_RETURN_ON_MATCH(s,x,r) \
  if(safe_strcmp((s), (str)) == 0) { *(r) = x; return(TRUE); }
gboolean gncEntryDiscountStringToHow (const char *str, GncDiscountHow *how)
{
  GNC_RETURN_ON_MATCH ("PRETAX", GNC_DISC_PRETAX, how);
  GNC_RETURN_ON_MATCH ("SAMETIME", GNC_DISC_SAMETIME, how);
  GNC_RETURN_ON_MATCH ("POSTTAX", GNC_DISC_POSTTAX, how);
  g_warning ("asked to translate unknown discount-how string %s.\n",
       str ? str : "(null)");

  return(FALSE);
}
gboolean gncEntryPaymentStringToType (const char *str, GncEntryPaymentType *type)
{
  GNC_RETURN_ON_MATCH ("CASH", GNC_PAYMENT_CASH, type);
  GNC_RETURN_ON_MATCH ("CARD", GNC_PAYMENT_CARD, type);
  g_warning ("asked to translate unknown discount-how string %s.\n",
       str ? str : "(null)");

  return(FALSE);
}
#undef GNC_RETURN_ON_MATCH

#define _GNC_MOD_NAME	GNC_ENTRY_MODULE_NAME

#define CACHE_INSERT(str) g_cache_insert(gnc_engine_get_string_cache(), (gpointer)(str));
#define CACHE_REMOVE(str) g_cache_remove(gnc_engine_get_string_cache(), (str));

#define SET_STR(obj, member, str) { \
	char * tmp; \
	\
	if (!safe_strcmp (member, str)) return; \
	gncEntryBeginEdit (obj); \
	tmp = CACHE_INSERT (str); \
	CACHE_REMOVE (member); \
	member = tmp; \
	}

static void addObj (GncEntry *entry);
static void remObj (GncEntry *entry);

G_INLINE_FUNC void mark_entry (GncEntry *entry);
G_INLINE_FUNC void
mark_entry (GncEntry *entry)
{
  entry->dirty = TRUE;
  gncBusinessSetDirtyFlag (entry->book, _GNC_MOD_NAME, TRUE);

  gnc_engine_generate_event (&entry->guid, GNC_EVENT_MODIFY);
}

/* Create/Destroy Functions */

GncEntry *gncEntryCreate (GNCBook *book)
{
  GncEntry *entry;
  gnc_numeric zero = gnc_numeric_zero ();

  if (!book) return NULL;

  entry = g_new0 (GncEntry, 1);
  entry->book = book;

  entry->desc = CACHE_INSERT ("");
  entry->action = CACHE_INSERT ("");
  entry->notes = CACHE_INSERT ("");
  entry->quantity = zero;

  entry->i_price = zero;
  entry->i_taxable = TRUE;
  entry->i_discount = zero;
  entry->i_disc_type = GNC_AMT_TYPE_PERCENT;
  entry->i_disc_how = GNC_DISC_PRETAX;

  entry->b_price = zero;
  entry->b_taxable = TRUE;
  entry->billto.type = GNC_OWNER_CUSTOMER;
  entry->b_payment = GNC_PAYMENT_CASH;

  entry->values_dirty = TRUE;

  xaccGUIDNew (&entry->guid, book);
  addObj (entry);

  gnc_engine_generate_event (&entry->guid, GNC_EVENT_CREATE);

  return entry;
}

void gncEntryDestroy (GncEntry *entry)
{
  if (!entry) return;
  entry->do_free = TRUE;
  gncEntryCommitEdit(entry);
}

static void gncEntryFree (GncEntry *entry)
{
  if (!entry) return;

  gnc_engine_generate_event (&entry->guid, GNC_EVENT_DESTROY);

  CACHE_REMOVE (entry->desc);
  CACHE_REMOVE (entry->action);
  CACHE_REMOVE (entry->notes);
  if (entry->i_tax_values)
    gncAccountValueDestroy (entry->i_tax_values);
  if (entry->b_tax_values)
    gncAccountValueDestroy (entry->b_tax_values);
  if (entry->i_tax_table)
    gncTaxTableDecRef (entry->i_tax_table);
  if (entry->b_tax_table)
    gncTaxTableDecRef (entry->b_tax_table);
  remObj (entry);

  g_free (entry);
}

/* Set Functions */

void gncEntrySetGUID (GncEntry *entry, const GUID *guid)
{
  if (!entry || !guid) return;
  if (guid_equal (guid, &entry->guid)) return;

  gncEntryBeginEdit (entry);
  remObj (entry);
  entry->guid = *guid;
  addObj (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetDate (GncEntry *entry, Timespec date)
{
  if (!entry) return;
  if (timespec_equal (&entry->date, &date)) return;
  gncEntryBeginEdit (entry);
  entry->date = date;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetDateEntered (GncEntry *entry, Timespec date)
{
  if (!entry) return;
  if (timespec_equal (&entry->date_entered, &date)) return;
  gncEntryBeginEdit (entry);
  entry->date_entered = date;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetDescription (GncEntry *entry, const char *desc)
{
  if (!entry || !desc) return;
  SET_STR (entry, entry->desc, desc);
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetAction (GncEntry *entry, const char *action)
{
  if (!entry || !action) return;
  SET_STR (entry,entry->action, action);
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetNotes (GncEntry *entry, const char *notes)
{
  if (!entry || !notes) return;
  SET_STR (entry, entry->notes, notes);
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetQuantity (GncEntry *entry, gnc_numeric quantity)
{
  if (!entry) return;
  if (gnc_numeric_eq (entry->quantity, quantity)) return;
  gncEntryBeginEdit (entry);
  entry->quantity = quantity;
  entry->values_dirty = TRUE;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

/* Customer Invoices */

void gncEntrySetInvAccount (GncEntry *entry, Account *acc)
{
  if (!entry) return;
  if (entry->i_account == acc) return;
  gncEntryBeginEdit (entry);
  entry->i_account = acc;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetInvPrice (GncEntry *entry, gnc_numeric price)
{
  if (!entry) return;
  if (gnc_numeric_eq (entry->i_price, price)) return;
  gncEntryBeginEdit (entry);
  entry->i_price = price;
  entry->values_dirty = TRUE;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetInvTaxable (GncEntry *entry, gboolean taxable)
{
  if (!entry) return;
  if (entry->i_taxable == taxable) return;
  gncEntryBeginEdit (entry);
  entry->i_taxable = taxable;
  entry->values_dirty = TRUE;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetInvTaxIncluded (GncEntry *entry, gboolean taxincluded)
{
  if (!entry) return;
  if (entry->i_taxincluded == taxincluded) return;
  gncEntryBeginEdit (entry);
  entry->i_taxincluded = taxincluded;
  entry->values_dirty = TRUE;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetInvTaxTable (GncEntry *entry, GncTaxTable *table)
{
  if (!entry) return;
  if (entry->i_tax_table == table) return;
  gncEntryBeginEdit (entry);
  if (entry->i_tax_table)
    gncTaxTableDecRef (entry->i_tax_table);
  if (table)
    gncTaxTableIncRef (table);
  entry->i_tax_table = table;
  entry->values_dirty = TRUE;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetInvDiscount (GncEntry *entry, gnc_numeric discount)
{
  if (!entry) return;
  if (gnc_numeric_eq (entry->i_discount, discount)) return;
  gncEntryBeginEdit (entry);
  entry->i_discount = discount;
  entry->values_dirty = TRUE;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetInvDiscountType (GncEntry *entry, GncAmountType type)
{
  if (!entry) return;
  if (entry->i_disc_type == type) return;

  gncEntryBeginEdit (entry);
  entry->i_disc_type = type;
  entry->values_dirty = TRUE;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetInvDiscountHow (GncEntry *entry, GncDiscountHow how)
{
  if (!entry) return;
  if (entry->i_disc_how == how) return;

  gncEntryBeginEdit (entry);
  entry->i_disc_how = how;
  entry->values_dirty = TRUE;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

/* Vendor Bills */

void gncEntrySetBillAccount (GncEntry *entry, Account *acc)
{
  if (!entry) return;
  if (entry->b_account == acc) return;
  gncEntryBeginEdit (entry);
  entry->b_account = acc;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetBillPrice (GncEntry *entry, gnc_numeric price)
{
  if (!entry) return;
  if (gnc_numeric_eq (entry->b_price, price)) return;
  gncEntryBeginEdit (entry);
  entry->b_price = price;
  entry->values_dirty = TRUE;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetBillTaxable (GncEntry *entry, gboolean taxable)
{
  if (!entry) return;
  if (entry->b_taxable == taxable) return;
  gncEntryBeginEdit (entry);
  entry->b_taxable = taxable;
  entry->values_dirty = TRUE;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetBillTaxIncluded (GncEntry *entry, gboolean taxincluded)
{
  if (!entry) return;
  if (entry->b_taxincluded == taxincluded) return;
  gncEntryBeginEdit (entry);
  entry->b_taxincluded = taxincluded;
  entry->values_dirty = TRUE;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetBillTaxTable (GncEntry *entry, GncTaxTable *table)
{
  if (!entry) return;
  if (entry->b_tax_table == table) return;
  gncEntryBeginEdit (entry);
  if (entry->b_tax_table)
    gncTaxTableDecRef (entry->b_tax_table);
  if (table)
    gncTaxTableIncRef (table);
  entry->b_tax_table = table;
  entry->values_dirty = TRUE;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetBillable (GncEntry *entry, gboolean billable)
{
  if (!entry) return;
  if (entry->billable == billable) return;

  gncEntryBeginEdit (entry);
  entry->billable = billable;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetBillTo (GncEntry *entry, GncOwner *billto)
{
  if (!entry || !billto) return;
  if (gncOwnerEqual (&entry->billto, billto)) return;

  gncEntryBeginEdit (entry);
  gncOwnerCopy (billto, &entry->billto);
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetBillPayment (GncEntry *entry, GncEntryPaymentType type)
{
  if (!entry) return;
  if (entry->b_payment == type) return;
  gncEntryBeginEdit (entry);
  entry->b_payment = type;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

/* Called from gncOrder when we're added to the Order */
void gncEntrySetOrder (GncEntry *entry, GncOrder *order)
{
  if (!entry) return;
  if (entry->order == order) return;
  gncEntryBeginEdit (entry);
  entry->order = order;
  mark_entry (entry);
  gncEntryCommitEdit (entry);

  /* Generate an event modifying the Order's end-owner */
  gnc_engine_generate_event (gncOwnerGetEndGUID (gncOrderGetOwner (order)),
			     GNC_EVENT_MODIFY);
}

/* called from gncInvoice when we're added to the Invoice */
void gncEntrySetInvoice (GncEntry *entry, GncInvoice *invoice)
{
  if (!entry) return;
  if (entry->invoice == invoice) return;
  gncEntryBeginEdit (entry);
  entry->invoice = invoice;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

/* called from gncInvoice when we're added to the Invoice/Bill */
void gncEntrySetBill (GncEntry *entry, GncInvoice *bill)
{
  if (!entry) return;
  if (entry->bill == bill) return;
  gncEntryBeginEdit (entry);
  entry->bill = bill;
  mark_entry (entry);
  gncEntryCommitEdit (entry);
}

void gncEntrySetDirty (GncEntry *entry, gboolean dirty)
{
  if (!entry) return;
  entry->dirty = dirty;
}

void gncEntryCopy (const GncEntry *src, GncEntry *dest)
{
  if (!src || !dest) return;

  gncEntryBeginEdit (dest);
  dest->date 			= src->date;
  dest->date_entered		= src->date_entered; /* ??? */
  gncEntrySetDescription (dest, src->desc);
  gncEntrySetAction (dest, src->action);
  gncEntrySetNotes (dest, src->notes);
  dest->quantity		= src->quantity;

  dest->i_account		= src->i_account;
  dest->i_price			= src->i_price;
  dest->i_taxable		= src->i_taxable;
  dest->i_taxincluded		= src->i_taxincluded;
  dest->i_discount		= src->i_discount;
  dest->i_disc_type		= src->i_disc_type;
  dest->i_disc_how		= src->i_disc_how;

  /* vendor bill data */
  dest->b_account		= src->b_account;
  dest->b_price			= src->b_price;
  dest->b_taxable		= src->b_taxable;
  dest->b_taxincluded		= src->b_taxincluded;
  dest->billable		= src->billable;
  dest->billto			= src->billto;

  if (src->i_tax_table)
    gncEntrySetInvTaxTable (dest, src->i_tax_table);

  if (src->b_tax_table)
    gncEntrySetBillTaxTable (dest, src->b_tax_table);

  if (src->order)
    gncOrderAddEntry (src->order, dest);

  if (src->invoice)
    gncInvoiceAddEntry (src->invoice, dest);

  if (src->bill)
    gncBillAddEntry (src->bill, dest);

  dest->values_dirty = TRUE;
  gncEntryCommitEdit (dest);
}

/* Get Functions */

GNCBook * gncEntryGetBook (GncEntry *entry)
{
  if (!entry) return NULL;
  return entry->book;
}

const GUID * gncEntryGetGUID (GncEntry *entry)
{
  if (!entry) return NULL;
  return &(entry->guid);
}

Timespec gncEntryGetDate (GncEntry *entry)
{
  Timespec ts; ts.tv_sec = 0; ts.tv_nsec = 0;
  if (!entry) return ts;
  return entry->date;
}

Timespec gncEntryGetDateEntered (GncEntry *entry)
{
  Timespec ts; ts.tv_sec = 0; ts.tv_nsec = 0;
  if (!entry) return ts;
  return entry->date_entered;
}

const char * gncEntryGetDescription (GncEntry *entry)
{
  if (!entry) return NULL;
  return entry->desc;
}

const char * gncEntryGetAction (GncEntry *entry)
{
  if (!entry) return NULL;
  return entry->action;
}

const char * gncEntryGetNotes (GncEntry *entry)
{
  if (!entry) return NULL;
  return entry->notes;
}

gnc_numeric gncEntryGetQuantity (GncEntry *entry)
{
  if (!entry) return gnc_numeric_zero();
  return entry->quantity;
}

/* Customer Invoice */

Account * gncEntryGetInvAccount (GncEntry *entry)
{
  if (!entry) return NULL;
  return entry->i_account;
}

gnc_numeric gncEntryGetInvPrice (GncEntry *entry)
{
  if (!entry) return gnc_numeric_zero();
  return entry->i_price;
}

gnc_numeric gncEntryGetInvDiscount (GncEntry *entry)
{
  if (!entry) return gnc_numeric_zero();
  return entry->i_discount;
}

GncAmountType gncEntryGetInvDiscountType (GncEntry *entry)
{
  if (!entry) return 0;
  return entry->i_disc_type;
}

GncDiscountHow gncEntryGetInvDiscountHow (GncEntry *entry)
{
  if (!entry) return 0;
  return entry->i_disc_how;
}

gboolean gncEntryGetInvTaxable (GncEntry *entry)
{
  if (!entry) return FALSE;
  return entry->i_taxable;
}

gboolean gncEntryGetInvTaxIncluded (GncEntry *entry)
{
  if (!entry) return FALSE;
  return entry->i_taxincluded;
}

GncTaxTable * gncEntryGetInvTaxTable (GncEntry *entry)
{
  if (!entry) return NULL;
  return entry->i_tax_table;
}

/* vendor bills */

Account * gncEntryGetBillAccount (GncEntry *entry)
{
  if (!entry) return NULL;
  return entry->b_account;
}

gnc_numeric gncEntryGetBillPrice (GncEntry *entry)
{
  if (!entry) return gnc_numeric_zero();
  return entry->b_price;
}

gboolean gncEntryGetBillTaxable (GncEntry *entry)
{
  if (!entry) return FALSE;
  return entry->b_taxable;
}

gboolean gncEntryGetBillTaxIncluded (GncEntry *entry)
{
  if (!entry) return FALSE;
  return entry->b_taxincluded;
}

GncTaxTable * gncEntryGetBillTaxTable (GncEntry *entry)
{
  if (!entry) return NULL;
  return entry->b_tax_table;
}

gboolean gncEntryGetBillable (GncEntry *entry)
{
  if (!entry) return FALSE;
  return entry->billable;
}

GncOwner * gncEntryGetBillTo (GncEntry *entry)
{
  if (!entry) return NULL;
  return &entry->billto;
}

GncEntryPaymentType gncEntryGetBillPayment (GncEntry* entry)
{
  if (!entry) return 0;
  return entry->b_payment;
}

GncInvoice * gncEntryGetInvoice (GncEntry *entry)
{
  if (!entry) return NULL;
  return entry->invoice;
}

GncInvoice * gncEntryGetBill (GncEntry *entry)
{
  if (!entry) return NULL;
  return entry->bill;
}

GncOrder * gncEntryGetOrder (GncEntry *entry)
{
  if (!entry) return NULL;
  return entry->order;
}

GncEntry * gncEntryLookup (GNCBook *book, const GUID *guid)
{
  if (!book || !guid) return NULL;
  return xaccLookupEntity (gnc_book_get_entity_table (book),
			   guid, _GNC_MOD_NAME);
}

/*
 * This is the logic of computing the total for an Entry, so you know
 * what values to put into various Splits or to display in the ledger.
 * In other words, we combine the quantity, unit-price, discount and
 * taxes together, depending on various flags.
 *
 * There are four potental ways to combine these numbers:
 * Discount:     Pre-Tax   Post-Tax
 *   Tax   :     Included  Not-Included
 *
 * The process is relatively simple:
 *
 *  1) compute the agregate price (price*qty)
 *  2) if taxincluded, then back-compute the agregate pre-tax price
 *  3) apply discount and taxes in the appropriate order
 *  4) return the requested results.
 *
 * step 2 can be done with agregate taxes; no need to compute them all
 * unless the caller asked for the tax_value.
 *
 * Note that the returned "value" is such that value + tax == "total
 * to pay," which means in the case of tax-included that the returned
 * "value" may be less than the agregate price, even without a
 * discount.  If you want to display the tax-included value, you need
 * to add the value and taxes together.  In other words, the value is
 * the amount the merchant gets; the taxes are the amount the gov't
 * gets, and the customer pays the sum or value + taxes.
 *
 * The discount return value is just for entertainment -- you may way
 * to let a consumer know how much they saved.
 */
void gncEntryComputeValue (gnc_numeric qty, gnc_numeric price,
			   GncTaxTable *tax_table, gboolean tax_included,
			   gnc_numeric discount, GncAmountType discount_type,
			   GncDiscountHow discount_how,
			   gnc_numeric *value, gnc_numeric *discount_value,
			   GList **tax_value)
{
  gnc_numeric	aggregate;
  gnc_numeric	pretax;
  gnc_numeric	result;
  gnc_numeric	tax;
  gnc_numeric	percent = gnc_numeric_create (100, 1);
  gnc_numeric	tpercent = gnc_numeric_zero ();
  gnc_numeric	tvalue = gnc_numeric_zero ();

  GList * 	entries = gncTaxTableGetEntries (tax_table);
  GList * 	node;

  /* Step 1: compute the aggregate price */

  aggregate = gnc_numeric_mul (qty, price, GNC_DENOM_AUTO, GNC_DENOM_LCD);

  /* Step 2: compute the pre-tax aggregate */

  /* First, compute the aggregate tpercent and tvalue numbers */
  for (node = entries; node; node = node->next) {
    GncTaxTableEntry *entry = node->data;
    gnc_numeric amount = gncTaxTableEntryGetAmount (entry);

    switch (gncTaxTableEntryGetType (entry)) {
    case GNC_AMT_TYPE_VALUE:
      tvalue = gnc_numeric_add (tvalue, amount, GNC_DENOM_AUTO,
				GNC_DENOM_LCD);
      break;
    case GNC_AMT_TYPE_PERCENT:
      tpercent = gnc_numeric_add (tpercent, amount, GNC_DENOM_AUTO,
				  GNC_DENOM_LCD);
      break;
    default:
      g_warning ("Unknown tax type: %d", gncTaxTableEntryGetType (entry));
    }
  }
  /* now we need to convert from 5% -> .05 */
  tpercent = gnc_numeric_div (tpercent, percent, GNC_DENOM_AUTO,
			      GNC_DENOM_LCD);

  /* Next, actually compute the pre-tax aggregate value based on the
   * taxincluded flag.
   */
  if (tax_table && tax_included) {
    /* Back-compute the pre-tax aggregate value.
     * We know that aggregate = pretax + pretax*tpercent + tvalue, so
     * pretax = (aggregate-tvalue)/(1+tpercent)
     */
    pretax = gnc_numeric_sub (aggregate, tvalue, GNC_DENOM_AUTO,
			      GNC_DENOM_LCD);
    pretax = gnc_numeric_div (pretax,
			      gnc_numeric_add (tpercent,
					       gnc_numeric_create (1, 1),
					       GNC_DENOM_AUTO, GNC_DENOM_LCD),
			      GNC_DENOM_AUTO, GNC_DENOM_LCD);
  } else {
    pretax = aggregate;
  }

  /* Step 3:  apply discount and taxes in the appropriate order */

  /*
   * There are two ways to apply discounts and taxes.  In one way, you
   * always compute the discount off the pretax number, and compute
   * the taxes off of either the pretax value or "pretax-discount"
   * value.  In the other way, you always compute the tax on "pretax",
   * and compute the discount on either "pretax" or "pretax+taxes".
   *
   * I don't know which is the "correct" way.
   */

  /*
   * Type:	discount	tax
   * PRETAX	pretax		pretax-discount
   * SAMETIME	pretax		pretax
   * POSTTAX	pretax+tax	pretax
   */

  switch (discount_how) {
  case GNC_DISC_PRETAX:
  case GNC_DISC_SAMETIME:
    /* compute the discount from pretax */

    if (discount_type == GNC_AMT_TYPE_PERCENT) {
      discount = gnc_numeric_div (discount, percent, GNC_DENOM_AUTO, 
				  GNC_DENOM_LCD);
      discount = gnc_numeric_mul (pretax, discount, GNC_DENOM_AUTO,
				  GNC_DENOM_LCD);
    }

    result = gnc_numeric_sub (pretax, discount, GNC_DENOM_AUTO, GNC_DENOM_LCD);

    /* Figure out when to apply the tax, pretax or pretax-discount */
    if (discount_how == GNC_DISC_PRETAX)
      pretax = result;
    break;

  case GNC_DISC_POSTTAX:
    /* compute discount on pretax+taxes */

    if (discount_type == GNC_AMT_TYPE_PERCENT) {
      gnc_numeric after_tax;

      tax = gnc_numeric_mul (pretax, tpercent, GNC_DENOM_AUTO, GNC_DENOM_LCD);
      after_tax = gnc_numeric_add (pretax, tax, GNC_DENOM_AUTO, GNC_DENOM_LCD);
      after_tax = gnc_numeric_add (after_tax, tvalue, GNC_DENOM_AUTO,
				   GNC_DENOM_LCD);
      discount = gnc_numeric_div (discount, percent, GNC_DENOM_AUTO,
				  GNC_DENOM_LCD);
      discount = gnc_numeric_mul (after_tax, discount, GNC_DENOM_AUTO,
				  GNC_DENOM_LCD);
    }

    result = gnc_numeric_sub (pretax, discount, GNC_DENOM_AUTO, GNC_DENOM_LCD);
    break;

  default:
    g_warning ("unknown DiscountHow value: %d", discount_how);
  }

  /* Step 4:  return the requested results. */

  /* result == amount merchant gets
   * discount == amount of discount
   * need to compute taxes (based on 'pretax') if the caller wants it.
   */

  if (discount_value != NULL)
    *discount_value = discount;

  if (value != NULL)
    *value = result;

  /* Now... Compute the list of tax values (if the caller wants it) */

  if (tax_value != NULL) {
    GList *	taxes = NULL;

    for (node = entries; node; node = node->next) {
      GncTaxTableEntry *entry = node->data;
      Account *acc = gncTaxTableEntryGetAccount (entry);
      gnc_numeric amount = gncTaxTableEntryGetAmount (entry);

      g_return_if_fail (acc);

      switch (gncTaxTableEntryGetType (entry)) {
      case GNC_AMT_TYPE_VALUE:
	taxes = gncAccountValueAdd (taxes, acc, amount);
	break;
      case GNC_AMT_TYPE_PERCENT:
	amount = gnc_numeric_div (amount, percent, GNC_DENOM_AUTO,
				  GNC_DENOM_LCD);
	tax = gnc_numeric_mul (pretax, amount, GNC_DENOM_AUTO, GNC_DENOM_LCD);
	taxes = gncAccountValueAdd (taxes, acc, tax);
	break;
      default:
       break;
      }
    }
    *tax_value = taxes;
  }

  return;
}

static int
get_commodity_denom (GncEntry *entry)
{
  gnc_commodity *c;
  if (!entry)
    return 0;
  if (entry->invoice) {
    c = gncInvoiceGetCurrency (entry->invoice);
    if (c)
      return (gnc_commodity_get_fraction (c));
  }
  if (entry->bill) {
    c = gncInvoiceGetCurrency (entry->bill);
    if (c)
      return (gnc_commodity_get_fraction (c));
  }
  return 100000;
}

static void
gncEntryRecomputeValues (GncEntry *entry)
{
  int denom;

  /* See if either tax table changed since we last computed values */
  if (entry->i_tax_table) {
    Timespec modtime = gncTaxTableLastModified (entry->i_tax_table);
    if (timespec_cmp (&entry->i_taxtable_modtime, &modtime)) {
      entry->values_dirty = TRUE;
      entry->i_taxtable_modtime = modtime;
    }
  }
  if (entry->b_tax_table) {
    Timespec modtime = gncTaxTableLastModified (entry->b_tax_table);
    if (timespec_cmp (&entry->b_taxtable_modtime, &modtime)) {
      entry->values_dirty = TRUE;
      entry->b_taxtable_modtime = modtime;
    }
  }

  if (!entry->values_dirty)
    return;

  /* Clear the last-computed tax values */
  if (entry->i_tax_values) {
    gncAccountValueDestroy (entry->i_tax_values);
    entry->i_tax_values = NULL;
  }
  if (entry->b_tax_values) {
    gncAccountValueDestroy (entry->b_tax_values);
    entry->b_tax_values = NULL;
  }

  /* Compute the invoice values */
  gncEntryComputeValue (entry->quantity, entry->i_price,
			(entry->i_taxable ? entry->i_tax_table : NULL),
			entry->i_taxincluded,
			entry->i_discount, entry->i_disc_type,
			entry->i_disc_how,
			&(entry->i_value), &(entry->i_disc_value),
			&(entry->i_tax_values));

  /* Compute the bill values */
  gncEntryComputeValue (entry->quantity, entry->b_price,
			(entry->b_taxable ? entry->b_tax_table : NULL),
			entry->b_taxincluded,
			gnc_numeric_zero(), GNC_AMT_TYPE_VALUE, GNC_DISC_PRETAX,
			&(entry->b_value), NULL, &(entry->b_tax_values));

  denom = get_commodity_denom (entry);
  entry->i_value_rounded = gnc_numeric_convert (entry->i_value, denom,
						GNC_RND_ROUND);
  entry->i_disc_value_rounded = gnc_numeric_convert (entry->i_disc_value, denom,
						     GNC_RND_ROUND);
  entry->i_tax_value = gncAccountValueTotal (entry->i_tax_values);
  entry->i_tax_value_rounded = gnc_numeric_convert (entry->i_tax_value, denom,
						    GNC_RND_ROUND);

  entry->b_value_rounded = gnc_numeric_convert (entry->b_value, denom,
						GNC_RND_ROUND);
  entry->b_tax_value = gncAccountValueTotal (entry->b_tax_values);
  entry->b_tax_value_rounded = gnc_numeric_convert (entry->b_tax_value, denom,
						    GNC_RND_ROUND);
  entry->values_dirty = FALSE;
}

void gncEntryGetValue (GncEntry *entry, gboolean is_inv, gnc_numeric *value,
		       gnc_numeric *discount_value, gnc_numeric *tax_value,
		       GList **tax_values)
{
  if (!entry) return;
  gncEntryRecomputeValues (entry);
  if (value)
    *value = (is_inv ? entry->i_value : entry->b_value);
  if (discount_value)
    *discount_value = (is_inv ? entry->i_disc_value : gnc_numeric_zero());
  if (tax_value)
    *tax_value = (is_inv ? entry->i_tax_value : entry->b_tax_value);
  if (tax_values)
    *tax_values = (is_inv ? entry->i_tax_values : entry->b_tax_values);
}

gnc_numeric gncEntryReturnValue (GncEntry *entry, gboolean is_inv)
{
  if (!entry) return gnc_numeric_zero();
  gncEntryRecomputeValues (entry);
  return (is_inv ? entry->i_value_rounded : entry->b_value_rounded);
}

gnc_numeric gncEntryReturnTaxValue (GncEntry *entry, gboolean is_inv)
{
  if (!entry) return gnc_numeric_zero();
  gncEntryRecomputeValues (entry);
  return (is_inv ? entry->i_tax_value_rounded : entry->b_tax_value_rounded);
}

GList * gncEntryReturnTaxValues (GncEntry *entry, gboolean is_inv)
{
  if (!entry) return NULL;
  gncEntryRecomputeValues (entry);
  return (is_inv ? entry->i_tax_values : entry->b_tax_values);
}

gnc_numeric gncEntryReturnDiscountValue (GncEntry *entry, gboolean is_inv)
{
  if (!entry) return gnc_numeric_zero();
  gncEntryRecomputeValues (entry);
  return (is_inv ? entry->i_disc_value_rounded : gnc_numeric_zero());
}

gboolean gncEntryIsOpen (GncEntry *entry)
{
  if (!entry) return FALSE;
  return (entry->editlevel > 0);
}

void gncEntryBeginEdit (GncEntry *entry)
{
  GNC_BEGIN_EDIT (entry, _GNC_MOD_NAME);
}

static void gncEntryOnError (GncEntry *entry, GNCBackendError errcode)
{
  PERR("Entry Backend Failure: %d", errcode);
}

static void gncEntryOnDone (GncEntry *entry)
{
  entry->dirty = FALSE;
}

void gncEntryCommitEdit (GncEntry *entry)
{
  GNC_COMMIT_EDIT_PART1 (entry);
  GNC_COMMIT_EDIT_PART2 (entry, _GNC_MOD_NAME, gncEntryOnError,
			 gncEntryOnDone, gncEntryFree);
}

int gncEntryCompare (GncEntry *a, GncEntry *b)
{
  int compare;

  if (a == b) return 0;
  if (!a && b) return -1;
  if (a && !b) return 1;

  compare = timespec_cmp (&(a->date), &(b->date));
  if (compare) return compare;

  compare = timespec_cmp (&(a->date_entered), &(b->date_entered));
  if (compare) return compare;

  compare = safe_strcmp (a->desc, b->desc);
  if (compare) return compare;

  compare = safe_strcmp (a->action, b->action);
  if (compare) return compare;

  return guid_compare (&(a->guid), &(b->guid));
}

/* Package-Private functions */

static void addObj (GncEntry *entry)
{
  gncBusinessAddObject (entry->book, _GNC_MOD_NAME, entry, &entry->guid);
}

static void remObj (GncEntry *entry)
{
  gncBusinessRemoveObject (entry->book, _GNC_MOD_NAME, &entry->guid);
}

static void _gncEntryCreate (GNCBook *book)
{
  gncBusinessCreate (book, _GNC_MOD_NAME);
}

static void _gncEntryDestroy (GNCBook *book)
{
  gncBusinessDestroy (book, _GNC_MOD_NAME);
}

static gboolean _gncEntryIsDirty (GNCBook *book)
{
  return gncBusinessIsDirty (book, _GNC_MOD_NAME);
}

static void _gncEntryMarkClean (GNCBook *book)
{
  gncBusinessSetDirtyFlag (book, _GNC_MOD_NAME, FALSE);
}

static void _gncEntryForeach (GNCBook *book, foreachObjectCB cb,
			      gpointer user_data)
{
  gncBusinessForeach (book, _GNC_MOD_NAME, cb, user_data);
}

static GncObject_t gncEntryDesc = {
  GNC_OBJECT_VERSION,
  _GNC_MOD_NAME,
  "Order/Invoice/Bill Entry",
  _gncEntryCreate,
  _gncEntryDestroy,
  _gncEntryIsDirty,
  _gncEntryMarkClean,
  _gncEntryForeach,
  NULL				/* printable */
};

gboolean gncEntryRegister (void)
{
  static QueryObjectDef params[] = {
    { ENTRY_DATE, QUERYCORE_DATE, (QueryAccess)gncEntryGetDate },
    { ENTRY_DATE_ENTERED, QUERYCORE_DATE, (QueryAccess)gncEntryGetDateEntered },
    { ENTRY_DESC, QUERYCORE_STRING, (QueryAccess)gncEntryGetDescription },
    { ENTRY_ACTION, QUERYCORE_STRING, (QueryAccess)gncEntryGetAction },
    { ENTRY_NOTES, QUERYCORE_STRING, (QueryAccess)gncEntryGetNotes },
    { ENTRY_QTY, QUERYCORE_NUMERIC, (QueryAccess)gncEntryGetQuantity },
    { ENTRY_IPRICE, QUERYCORE_NUMERIC, (QueryAccess)gncEntryGetInvPrice },
    { ENTRY_BPRICE, QUERYCORE_NUMERIC, (QueryAccess)gncEntryGetBillPrice },
    { ENTRY_INVOICE, GNC_INVOICE_MODULE_NAME, (QueryAccess)gncEntryGetInvoice },
    { ENTRY_BILL, GNC_INVOICE_MODULE_NAME, (QueryAccess)gncEntryGetBill },
    { ENTRY_BILLABLE, QUERYCORE_BOOLEAN, (QueryAccess)gncEntryGetBillable },
    { ENTRY_BILLTO, GNC_OWNER_MODULE_NAME, (QueryAccess)gncEntryGetBillTo },
    { ENTRY_ORDER, GNC_ORDER_MODULE_NAME, (QueryAccess)gncEntryGetOrder },
    { QUERY_PARAM_BOOK, GNC_ID_BOOK, (QueryAccess)gncEntryGetBook },
    { QUERY_PARAM_GUID, QUERYCORE_GUID, (QueryAccess)gncEntryGetGUID },
    { NULL },
  };

  gncQueryObjectRegister (_GNC_MOD_NAME, (QuerySort)gncEntryCompare, params);

  return gncObjectRegister (&gncEntryDesc);
}
