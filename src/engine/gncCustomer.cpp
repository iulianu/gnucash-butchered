/********************************************************************\
 * gncCustomer.cpp -- the Core Customer Interface                   *
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
 * Copyright (C) 2001,2002 Derek Atkins
 * Copyright (C) 2003 Linas Vepstas <linas@linas.org>
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#include "config.h"

#include <glib.h>
#include <string.h>
#include <typeinfo>

#include "gnc-commodity.h"

#include "gncAddressP.h"
#include "gncBillTermP.h"
#include "gncInvoice.h"
#ifdef GNUCASH_MAJOR_VERSION
#include "gncBusiness.h"
#endif

#include "gncCustomer.h"
#include "gncCustomerP.h"
#include "gncJobP.h"
#include "gncTaxTableP.h"

static gint gs_address_event_handler_id = 0;
static void listen_for_address_events(QofInstance *entity, QofEventId event_type,
                                      gpointer user_data, gpointer event_data);

static QofLogModule log_module = GNC_MOD_BUSINESS;

#define _GNC_MOD_NAME        GNC_ID_CUSTOMER

/* ============================================================== */
/* misc inline funcs */

G_INLINE_FUNC void mark_customer (GncCustomer *customer);
void mark_customer (GncCustomer *customer)
{
    qof_instance_set_dirty(customer);
    qof_event_gen (customer, QOF_EVENT_MODIFY, NULL);
}

/* ============================================================== */

//enum
//{
//    PROP_0,
//    PROP_NAME
//};
//
GncCustomer::GncCustomer()
{
    id = NULL;
    name = NULL;
    notes = NULL;
    terms = NULL;
    addr = NULL;
    currency = NULL;
    taxtable = NULL;
    taxtable_override = false;
    taxincluded = 0;
    active = false;
    jobs = NULL;
    credit = gnc_numeric_zero();
    discount = gnc_numeric_zero();
    shipaddr = NULL;
}

GncCustomer::~GncCustomer()
{
}

///** Return display name for this object */
//static gchar*
//impl_get_display_name(const QofInstance* inst)
//{
//    g_return_val_if_fail(inst != NULL, FALSE);
//    g_return_val_if_fail(typeid(*inst) == typeid(GncCustomer), FALSE);
//
//    const GncCustomer *cust = dynamic_cast<const GncCustomer*>(inst);
//    /* XXX internationalization of "Customer" */
//    return g_strdup_printf("Customer %s", cust->name);
//}
//
///** Does this object refer to a specific object */
//static bool
//impl_refers_to_object(const QofInstance* inst, const QofInstance* ref)
//{
//    GncCustomer* cust;
//
//    g_return_val_if_fail(inst != NULL, FALSE);
//    g_return_val_if_fail(GNC_IS_CUSTOMER(inst), FALSE);
//
//    cust = GNC_CUSTOMER(inst);
//
//    if (GNC_IS_BILLTERM(ref))
//    {
//        return (cust->terms == GNC_BILLTERM(ref));
//    }
//    else if (GNC_IS_TAXTABLE(ref))
//    {
//        return (cust->taxtable == GNC_TAXTABLE(ref));
//    }
//
//    return FALSE;
//}
//
///** Returns a list of my type of object which refers to an object.  For example, when called as
//        qof_instance_get_typed_referring_object_list(taxtable, account);
//    it will return the list of taxtables which refer to a specific account.  The result should be the
//    same regardless of which taxtable object is used.  The list must be freed by the caller but the
//    objects on the list must not.
// */
//static GList*
//impl_get_typed_referring_object_list(const QofInstance* inst, const QofInstance* ref)
//{
//    if (!GNC_IS_BILLTERM(ref) && !GNC_IS_TAXTABLE(ref))
//    {
//        return NULL;
//    }
//
//    return qof_instance_get_referring_object_list_from_collection(qof_instance_get_collection(inst), ref);
//}


/* Create/Destroy Functions */
GncCustomer *gncCustomerCreate (QofBook *book)
{
    GncCustomer *cust;

    if (!book) return NULL;

    cust = new GncCustomer; //g_object_new (GNC_TYPE_CUSTOMER, NULL);
    qof_instance_init_data (cust, _GNC_MOD_NAME, book);

    cust->id = CACHE_INSERT ("");
    cust->name = CACHE_INSERT ("");
    cust->notes = CACHE_INSERT ("");
    cust->addr = gncAddressCreate (book, cust);
    cust->taxincluded = GNC_TAXINCLUDED_USEGLOBAL;
    cust->active = TRUE;
    cust->jobs = NULL;

    cust->discount = gnc_numeric_zero();
    cust->credit = gnc_numeric_zero();
    cust->shipaddr = gncAddressCreate (book, cust);

    if (gs_address_event_handler_id == 0)
    {
        gs_address_event_handler_id = qof_event_register_handler(listen_for_address_events, NULL);
    }

    qof_event_gen (cust, QOF_EVENT_CREATE, NULL);

    return cust;
}

void gncCustomerDestroy (GncCustomer *cust)
{
    if (!cust) return;
    qof_instance_set_destroying(cust, true);
    qof_instance_set_dirty (cust);
    gncCustomerCommitEdit (cust);
}

static void gncCustomerFree (GncCustomer *cust)
{
    if (!cust) return;

    qof_event_gen (cust, QOF_EVENT_DESTROY, NULL);

    CACHE_REMOVE (cust->id);
    CACHE_REMOVE (cust->name);
    CACHE_REMOVE (cust->notes);
    gncAddressBeginEdit (cust->addr);
    gncAddressDestroy (cust->addr);
    gncAddressBeginEdit (cust->shipaddr);
    gncAddressDestroy (cust->shipaddr);
    g_list_free (cust->jobs);

    if (cust->terms)
        gncBillTermDecRef (cust->terms);
    if (cust->taxtable)
    {
        gncTaxTableDecRef (cust->taxtable);
    }

    /* qof_instance_release (&cust->inst); */
//    g_object_unref (cust);
    delete cust;
}

/* ============================================================== */
/* Set Functions */

#define SET_STR(obj, member, str) { \
        char * tmp; \
        \
        if (!g_strcmp0 (member, str)) return; \
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

void gncCustomerSetActive (GncCustomer *cust, bool active)
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

void gncCustomerSetTaxTableOverride (GncCustomer *customer, bool override)
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

    qof_event_gen (cust, QOF_EVENT_MODIFY, NULL);
}

void gncCustomerRemoveJob (GncCustomer *cust, GncJob *job)
{
    GList *node;

    if (!cust) return;
    if (!job) return;

    node = g_list_find (cust->jobs, job);
    if (!node)
    {
        /*    PERR ("split not in account"); */
    }
    else
    {
        cust->jobs = g_list_remove_link (cust->jobs, node);
        g_list_free_1 (node);
    }
    qof_event_gen (cust, QOF_EVENT_MODIFY, NULL);
}

void gncCustomerBeginEdit (GncCustomer *cust)
{
    qof_begin_edit (cust);
}

static void gncCustomerOnError (QofInstance *inst, QofBackendError errcode)
{
    PERR("Customer QofBackend Failure: %d", errcode);
    gnc_engine_signal_commit_error( errcode );
}

static void gncCustomerOnDone (QofInstance *inst)
{
    GncCustomer *cust = dynamic_cast<GncCustomer *>(inst);
    gncAddressClearDirty (cust->addr);
    gncAddressClearDirty (cust->shipaddr);
}

static void cust_free (QofInstance *inst)
{
    GncCustomer *cust = dynamic_cast<GncCustomer *>(inst);
    gncCustomerFree (cust);
}

void gncCustomerCommitEdit (GncCustomer *cust)
{
    if (!qof_commit_edit (QOF_INSTANCE(cust))) return;
    qof_commit_edit_part2 (cust, gncCustomerOnError,
                           gncCustomerOnDone, cust_free);
}

/* ============================================================== */
/* Get Functions */

const char * gncCustomerGetID (const GncCustomer *cust)
{
    if (!cust) return NULL;
    return cust->id;
}

const char * gncCustomerGetName (const GncCustomer *cust)
{
    if (!cust) return NULL;
    return cust->name;
}

GncAddress * gncCustomerGetAddr (const GncCustomer *cust)
{
    if (!cust) return NULL;
    return cust->addr;
}

static void
qofCustomerSetAddr (GncCustomer *cust, QofInstance *addr_ent)
{
    GncAddress *addr;

    if (!cust || !addr_ent)
    {
        return;
    }
    addr = (GncAddress*)addr_ent;
    if (addr == cust->addr)
    {
        return;
    }
    if (cust->addr != NULL)
    {
        gncAddressBeginEdit(cust->addr);
        gncAddressDestroy(cust->addr);
    }
    gncCustomerBeginEdit(cust);
    cust->addr = addr;
    gncCustomerCommitEdit(cust);
}

static void
qofCustomerSetShipAddr (GncCustomer *cust, QofInstance *ship_addr_ent)
{
    GncAddress *ship_addr;

    if (!cust || !ship_addr_ent)
    {
        return;
    }
    ship_addr = (GncAddress*)ship_addr_ent;
    if (ship_addr == cust->shipaddr)
    {
        return;
    }
    if (cust->shipaddr != NULL)
    {
        gncAddressBeginEdit(cust->shipaddr);
        gncAddressDestroy(cust->shipaddr);
    }
    gncCustomerBeginEdit(cust);
    cust->shipaddr = ship_addr;
    gncCustomerCommitEdit(cust);
}

GncAddress * gncCustomerGetShipAddr (const GncCustomer *cust)
{
    if (!cust) return NULL;
    return cust->shipaddr;
}

const char * gncCustomerGetNotes (const GncCustomer *cust)
{
    if (!cust) return NULL;
    return cust->notes;
}

GncBillTerm * gncCustomerGetTerms (const GncCustomer *cust)
{
    if (!cust) return NULL;
    return cust->terms;
}

GncTaxIncluded gncCustomerGetTaxIncluded (const GncCustomer *cust)
{
    if (!cust) return GNC_TAXINCLUDED_USEGLOBAL;
    return cust->taxincluded;
}

gnc_commodity * gncCustomerGetCurrency (const GncCustomer *cust)
{
    if (!cust) return NULL;
    return cust->currency;
}

bool gncCustomerGetActive (const GncCustomer *cust)
{
    if (!cust) return false;
    return cust->active;
}

gnc_numeric gncCustomerGetDiscount (const GncCustomer *cust)
{
    if (!cust) return gnc_numeric_zero();
    return cust->discount;
}

gnc_numeric gncCustomerGetCredit (const GncCustomer *cust)
{
    if (!cust) return gnc_numeric_zero();
    return cust->credit;
}

bool gncCustomerGetTaxTableOverride (const GncCustomer *customer)
{
    if (!customer) return FALSE;
    return customer->taxtable_override;
}

GncTaxTable* gncCustomerGetTaxTable (const GncCustomer *customer)
{
    if (!customer) return NULL;
    return customer->taxtable;
}

GList * gncCustomerGetJoblist (const GncCustomer *cust, bool show_all)
{
    if (!cust) return NULL;

    if (show_all)
    {
        return (g_list_copy (cust->jobs));
    }
    else
    {
        GList *list = NULL, *iterator;
        for (iterator = cust->jobs; iterator; iterator = iterator->next)
        {
            GncJob *j = iterator->data;
            if (gncJobGetActive (j))
                list = g_list_append (list, j);
        }
        return list;
    }
}

bool gncCustomerIsDirty (GncCustomer *cust)
{
    if (!cust) return FALSE;
    return (qof_instance_is_dirty(cust) ||
            gncAddressIsDirty (cust->addr) ||
            gncAddressIsDirty (cust->shipaddr));
}

/* Other functions */

int gncCustomerCompare (const GncCustomer *a, const GncCustomer *b)
{
    if (!a && !b) return 0;
    if (!a && b) return 1;
    if (a && !b) return -1;

    return(strcmp(a->name, b->name));
}

bool
gncCustomerEqual(const GncCustomer *a, const GncCustomer *b)
{
    if (a == NULL && b == NULL) return TRUE;
    if (a == NULL || b == NULL) return FALSE;

//    g_return_val_if_fail(GNC_IS_CUSTOMER(a), FALSE);
//    g_return_val_if_fail(GNC_IS_CUSTOMER(b), FALSE);

    if (g_strcmp0(a->id, b->id) != 0)
    {
        PWARN("IDs differ: %s vs %s", a->id, b->id);
        return FALSE;
    }

    if (g_strcmp0(a->name, b->name) != 0)
    {
        PWARN("Names differ: %s vs %s", a->name, b->name);
        return FALSE;
    }

    if (g_strcmp0(a->notes, b->notes) != 0)
    {
        PWARN("Notes differ: %s vs %s", a->notes, b->notes);
        return FALSE;
    }

    if (!gncBillTermEqual(a->terms, b->terms))
    {
        PWARN("Bill terms differ");
        return FALSE;
    }

    if (!gnc_commodity_equal(a->currency, b->currency))
    {
        PWARN("currencies differ");
        return FALSE;
    }

    if (!gncTaxTableEqual(a->taxtable, b->taxtable))
    {
        PWARN("tax tables differ");
        return FALSE;
    }

    if (a->taxtable_override != b->taxtable_override)
    {
        PWARN("Tax table override flags differ");
        return FALSE;
    }

    if (a->taxincluded != b->taxincluded)
    {
        PWARN("Tax included flags differ");
        return FALSE;
    }

    if (a->active != b->active)
    {
        PWARN("Active flags differ");
        return FALSE;
    }

    if (!gncAddressEqual(a->addr, b->addr))
    {
        PWARN("addresses differ");
        return FALSE;
    }
    if (!gncAddressEqual(a->shipaddr, b->shipaddr))
    {
        PWARN("addresses differ");
        return FALSE;
    }

    if (!gnc_numeric_equal(a->credit, b->credit))
    {
        PWARN("Credit amounts differ");
        return FALSE;
    }

    if (!gnc_numeric_equal(a->discount, b->discount))
    {
        PWARN("Discount amounts differ");
        return FALSE;
    }

    /* FIXME: Need to check jobs list
    GList *         jobs;
    */

    return TRUE;
}

/**
 * Listens for MODIFY events from addresses.   If the address belongs to a customer,
 * mark the customer as dirty.
 *
 * @param entity Entity for the event
 * @param event_type Event type
 * @param user_data User data registered with the handler
 * @param event_data Event data passed with the event.
 */
static void
listen_for_address_events(QofInstance *entity, QofEventId event_type,
                          gpointer user_data, gpointer event_data)
{
    GncCustomer* cust;

    if ((event_type & QOF_EVENT_MODIFY) == 0)
    {
        return;
    }
//    //TODO DEBUG!
//    return;
    if(!entity)
        return;
    if (typeid(*entity) != typeid(GncAddress))
    {
        return;
    }
//    if (typeid(event_data) != typeid(GncCustomer*))
//    {
//        return;
//    }
    if(!event_data)
        return;
    // TODO type-unsafe conversion
    cust = reinterpret_cast<GncCustomer*>(event_data);
    gncCustomerBeginEdit(cust);
    mark_customer(cust);
    gncCustomerCommitEdit(cust);
}
/* ============================================================== */
/* Package-Private functions */
static const char * _gncCustomerPrintable (gpointer item)
{
//  GncCustomer *c = item;
    if (!item) return "failed";
    return gncCustomerGetName((GncCustomer*)item);
}

static void
destroy_customer_on_book_close(QofInstance *ent, gpointer data)
{
    if(!ent)
        return;
    if (typeid(*ent) != typeid(GncCustomer))
        return;
    GncCustomer* c = dynamic_cast<GncCustomer*>(ent);
    gncCustomerBeginEdit(c);
    gncCustomerDestroy(c);
}

/** Handles book end - frees all customers from the book
 *
 * @param book Book being closed
 */
static void
gnc_customer_book_end(QofBook* book)
{
    QofCollection *col;

    col = qof_book_get_collection(book, GNC_ID_CUSTOMER);
    qof_collection_foreach(col, destroy_customer_on_book_close, NULL);
}

static QofObject gncCustomerDesc =
{
    DI(.interface_version = ) QOF_OBJECT_VERSION,
    DI(.e_type            = ) _GNC_MOD_NAME,
    DI(.type_label        = ) "Customer",
    DI(.create            = ) (gpointer)gncCustomerCreate,
    DI(.book_begin        = ) NULL,
    DI(.book_end          = ) gnc_customer_book_end,
    DI(.is_dirty          = ) qof_collection_is_dirty,
    DI(.mark_clean        = ) qof_collection_mark_clean,
    DI(.foreach           = ) qof_collection_foreach,
    DI(.printable         = ) (const char * (*)(gpointer))gncCustomerGetName,
    DI(.version_cmp       = ) (int (*)(gpointer, gpointer)) qof_instance_version_cmp,
};

bool gncCustomerRegister (void)
{
//    static QofParam params[] =
//    {
//        { CUSTOMER_ID, QOF_TYPE_STRING, (QofAccessFunc)gncCustomerGetID, (QofSetterFunc)gncCustomerSetID },
//        { CUSTOMER_NAME, QOF_TYPE_STRING, (QofAccessFunc)gncCustomerGetName, (QofSetterFunc)gncCustomerSetName },
//        { CUSTOMER_NOTES, QOF_TYPE_STRING, (QofAccessFunc)gncCustomerGetNotes, (QofSetterFunc)gncCustomerSetNotes },
//        {
//            CUSTOMER_DISCOUNT, QOF_TYPE_NUMERIC, (QofAccessFunc)gncCustomerGetDiscount,
//            (QofSetterFunc)gncCustomerSetDiscount
//        },
//        {
//            CUSTOMER_CREDIT, QOF_TYPE_NUMERIC, (QofAccessFunc)gncCustomerGetCredit,
//            (QofSetterFunc)gncCustomerSetCredit
//        },
//        { CUSTOMER_ADDR, GNC_ID_ADDRESS, (QofAccessFunc)gncCustomerGetAddr, (QofSetterFunc)qofCustomerSetAddr },
//        { CUSTOMER_SHIPADDR, GNC_ID_ADDRESS, (QofAccessFunc)gncCustomerGetShipAddr, (QofSetterFunc)qofCustomerSetShipAddr },
//        {
//            CUSTOMER_TT_OVER, QOF_TYPE_BOOLEAN, (QofAccessFunc)gncCustomerGetTaxTableOverride,
//            (QofSetterFunc)gncCustomerSetTaxTableOverride
//        },
//        { CUSTOMER_TERMS, GNC_ID_BILLTERM, (QofAccessFunc)gncCustomerGetTerms, (QofSetterFunc)gncCustomerSetTerms },
//        { CUSTOMER_SLOTS, QOF_TYPE_KVP, (QofAccessFunc)qof_instance_get_slots, NULL },
//        { QOF_PARAM_ACTIVE, QOF_TYPE_BOOLEAN, (QofAccessFunc)gncCustomerGetActive, (QofSetterFunc)gncCustomerSetActive },
//        { QOF_PARAM_BOOK, QOF_ID_BOOK, (QofAccessFunc)qof_instance_get_book, NULL },
//        { QOF_PARAM_GUID, QOF_TYPE_GUID, (QofAccessFunc)qof_instance_get_guid, NULL },
//        { NULL },
//    };
//
//    if (!qof_choice_add_class(GNC_ID_INVOICE, GNC_ID_CUSTOMER, INVOICE_OWNER))
//    {
//        return FALSE;
//    }
//    if (!qof_choice_add_class(GNC_ID_JOB, GNC_ID_CUSTOMER, JOB_OWNER))
//    {
//        return FALSE;
//    }
//    qof_class_register (_GNC_MOD_NAME, (QofSortFunc)gncCustomerCompare, params);
//    if (!qof_choice_create(GNC_ID_CUSTOMER))
//    {
//        return FALSE;
//    }
    /* temp */
    _gncCustomerPrintable(NULL);
    return qof_object_register (&gncCustomerDesc);
}

gchar *gncCustomerNextID (QofBook *book)
{
    return qof_book_increment_and_format_counter (book, _GNC_MOD_NAME);
}
