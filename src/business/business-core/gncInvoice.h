/*
 * gncInvoice.h -- the Core Business Invoice Interface
 * Copyright (C) 2001 Derek Atkins
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#ifndef GNC_INVOICE_H_
#define GNC_INVOICE_H_

struct _gncInvoice;
typedef struct _gncInvoice GncInvoice;

#include "gncBillTerm.h"
#include "gncEntry.h"
#include "gncOwner.h"
#include "gnc-lot.h"

#define GNC_INVOICE_MODULE_NAME "gncInvoice"

/* Create/Destroy Functions */

GncInvoice *gncInvoiceCreate (GNCBook *book);
void gncInvoiceDestroy (GncInvoice *invoice);

/* Set Functions */

void gncInvoiceSetID (GncInvoice *invoice, const char *id);
void gncInvoiceSetOwner (GncInvoice *invoice, GncOwner *owner);
void gncInvoiceSetDateOpened (GncInvoice *invoice, Timespec date);
void gncInvoiceSetDatePosted (GncInvoice *invoice, Timespec date);
void gncInvoiceSetTerms (GncInvoice *invoice, GncBillTerm *terms);
void gncInvoiceSetBillingID (GncInvoice *invoice, const char *billing_id);
void gncInvoiceSetNotes (GncInvoice *invoice, const char *notes);
void gncInvoiceSetCommonCommodity (GncInvoice *invoice, gnc_commodity *com);
void gncInvoiceSetActive (GncInvoice *invoice, gboolean active);
void gncInvoiceSetBillTo (GncInvoice *invoice, GncOwner *billto);

void gncInvoiceAddEntry (GncInvoice *invoice, GncEntry *entry);
void gncInvoiceRemoveEntry (GncInvoice *invoice, GncEntry *entry);

/* Call this function when adding an entry to a bill instead of an invoice */
void gncBillAddEntry (GncInvoice *bill, GncEntry *entry);
void gncBillRemoveEntry (GncInvoice *bill, GncEntry *entry);

/* Get Functions */

GNCBook * gncInvoiceGetBook (GncInvoice *invoice);
const GUID * gncInvoiceGetGUID (GncInvoice *invoice);
const char * gncInvoiceGetID (GncInvoice *invoice);
GncOwner * gncInvoiceGetOwner (GncInvoice *invoice);
Timespec gncInvoiceGetDateOpened (GncInvoice *invoice);
Timespec gncInvoiceGetDatePosted (GncInvoice *invoice);
Timespec gncInvoiceGetDateDue (GncInvoice *invoice);
GncBillTerm * gncInvoiceGetTerms (GncInvoice *invoice);
const char * gncInvoiceGetBillingID (GncInvoice *invoice);
const char * gncInvoiceGetNotes (GncInvoice *invoice);
const char * gncInvoiceGetType (GncInvoice *invoice); 
gnc_commodity * gncInvoiceGetCommonCommodity (GncInvoice *invoice);
GncOwner * gncInvoiceGetBillTo (GncInvoice *invoice);
gboolean gncInvoiceGetActive (GncInvoice *invoice);

GNCLot * gncInvoiceGetPostedLot (GncInvoice *invoice);
Transaction * gncInvoiceGetPostedTxn (GncInvoice *invoice);
Account * gncInvoiceGetPostedAcc (GncInvoice *invoice);

/* return the "total" amount of the invoice */
gnc_numeric gncInvoiceGetTotal (GncInvoice *invoice);

GList * gncInvoiceGetEntries (GncInvoice *invoice);

/* Post this invoice to an account.  Returns the new Transaction
 * that is tied to this invoice.   The transaction is set with
 * the supplied posted date, due date, and memo.  The Transaction
 * description is set to the name of the company.
 */
Transaction *
gncInvoicePostToAccount (GncInvoice *invoice, Account *acc,
			 Timespec *posted_date, Timespec *due_date,
			 const char *memo);

/*
 * UNpost this invoice.  This will destroy the posted transaction
 * and return the invoice to its unposted state.  It may leave empty
 * lots out there.
 *
 * Returns TRUE if successful, FALSE if there is a problem.
 */
gboolean
gncInvoiceUnpost (GncInvoice *invoice);

/*
 * Apply a payment of "amount" for the owner, between the xfer_account
 * (bank or other asset) and the posted_account (A/R or A/P).
 *
 * XXX: yes, this should be in gncOwner, but all the other logic is
 * in gncInvoice...
 */
Transaction *
gncOwnerApplyPayment (GncOwner *owner, Account *posted_acc, Account *xfer_acc,
		      gnc_numeric amount, Timespec date,
		      const char *memo, const char *num);


/* Given a transaction, find and return the Invoice */
GncInvoice * gncInvoiceGetInvoiceFromTxn (Transaction *txn);

/* Given a LOT, find and return the Invoice attached to the lot */
GncInvoice * gncInvoiceGetInvoiceFromLot (GNCLot *lot);

GUID gncInvoiceRetGUID (GncInvoice *invoice);
GncInvoice * gncInvoiceLookupDirect (GUID guid, GNCBook *book);

GncInvoice * gncInvoiceLookup (GNCBook *book, const GUID *guid);
gboolean gncInvoiceIsDirty (GncInvoice *invoice);
void gncInvoiceBeginEdit (GncInvoice *invoice);
void gncInvoiceCommitEdit (GncInvoice *invoice);
int gncInvoiceCompare (GncInvoice *a, GncInvoice *b);
gboolean gncInvoiceIsPosted (GncInvoice *invoice);
gboolean gncInvoiceIsPaid (GncInvoice *invoice);

#define INVOICE_ID	"id"
#define INVOICE_OWNER	"owner"
#define INVOICE_OPENED	"date_opened"
#define INVOICE_POSTED	"date_posted"
#define INVOICE_DUE	"date_due"
#define INVOICE_IS_POSTED	"is_posted?"
#define INVOICE_TERMS	"terms"
#define INVOICE_BILLINGID	"billing_id"
#define INVOICE_NOTES	"notes"
#define INVOICE_ACC	"account"
#define INVOICE_POST_TXN	"posted_txn"
#define INVOICE_TYPE	"type"
#define INVOICE_BILLTO	"bill-to"

#define INVOICE_FROM_LOT	"invoice-from-lot"
#define INVOICE_FROM_TXN	"invoice-from-txn"

#endif /* GNC_INVOICE_H_ */
