/********************************************************************\
 * gncTaxTable.cpp -- the Gnucash Tax Table interface               *
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
 * Copyright (C) 2002 Derek Atkins
 * Copyright (C) 2003 Linas Vepstas <linas@linas.org>
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#include "config.h"

#include <glib.h>
#include <typeinfo>

#include "gncTaxTableP.h"

GncTaxTable::GncTaxTable()
{
    name = NULL;
    entries = NULL;
    modtime = {0,0};
    refcount = 0;
    parent = NULL;
    child = NULL;
    invisible = false;
    children = NULL;
}


class GncTaxTableEntry
{
public:
    GncTaxTable *   table;
    Account *       account;
    GncAmountType   type;
    gnc_numeric     amount;
    
    GncTaxTableEntry()
    {
        table = NULL;
        account = NULL;
        type = 0;
        amount = gnc_numeric_zero();
    }
};

struct _book_info
{
    GList *         tables;          /* visible tables */
};

static QofLogModule log_module = GNC_MOD_BUSINESS;

/* =============================================================== */
/* You must edit the functions in this block in tandem.  KEEP THEM IN
   SYNC! */

#define GNC_RETURN_ENUM_AS_STRING(x,s) case (x): return (s);
const char *
gncAmountTypeToString (GncAmountType type)
{
    switch (type)
    {
        GNC_RETURN_ENUM_AS_STRING(GNC_AMT_TYPE_VALUE, "VALUE");
        GNC_RETURN_ENUM_AS_STRING(GNC_AMT_TYPE_PERCENT, "PERCENT");
    default:
        g_warning ("asked to translate unknown amount type %d.\n", type);
        break;
    }
    return(NULL);
}

const char *
gncTaxIncludedTypeToString (GncTaxIncluded type)
{
    switch (type)
    {
        GNC_RETURN_ENUM_AS_STRING(GNC_TAXINCLUDED_YES, "YES");
        GNC_RETURN_ENUM_AS_STRING(GNC_TAXINCLUDED_NO, "NO");
        GNC_RETURN_ENUM_AS_STRING(GNC_TAXINCLUDED_USEGLOBAL, "USEGLOBAL");
    default:
        g_warning ("asked to translate unknown taxincluded type %d.\n", type);
        break;
    }
    return(NULL);
}
#undef GNC_RETURN_ENUM_AS_STRING
#define GNC_RETURN_ON_MATCH(s,x) \
  if(g_strcmp0((s), (str)) == 0) { *type = x; return(TRUE); }
bool
gncAmountStringToType (const char *str, GncAmountType *type)
{
    GNC_RETURN_ON_MATCH ("VALUE", GNC_AMT_TYPE_VALUE);
    GNC_RETURN_ON_MATCH ("PERCENT", GNC_AMT_TYPE_PERCENT);
    g_warning ("asked to translate unknown amount type string %s.\n",
               str ? str : "(null)");

    return(FALSE);
}

bool
gncTaxIncludedStringToType (const char *str, GncTaxIncluded *type)
{
    GNC_RETURN_ON_MATCH ("YES", GNC_TAXINCLUDED_YES);
    GNC_RETURN_ON_MATCH ("NO", GNC_TAXINCLUDED_NO);
    GNC_RETURN_ON_MATCH ("USEGLOBAL", GNC_TAXINCLUDED_USEGLOBAL);
    g_warning ("asked to translate unknown taxincluded type string %s.\n",
               str ? str : "(null)");

    return(FALSE);
}
#undef GNC_RETURN_ON_MATCH

/* =============================================================== */
/* Misc inline functions */

#define _GNC_MOD_NAME        GNC_ID_TAXTABLE

#define SET_STR(obj, member, str) { \
        char * tmp; \
        \
        if (!g_strcmp0 (member, str)) return; \
        gncTaxTableBeginEdit (obj); \
        tmp = CACHE_INSERT (str); \
        CACHE_REMOVE (member); \
        member = tmp; \
        }

static inline void
mark_table (GncTaxTable *table)
{
    qof_instance_set_dirty(table);
    qof_event_gen (table, QOF_EVENT_MODIFY, NULL);
}

static inline void
maybe_resort_list (GncTaxTable *table)
{
    struct _book_info *bi;

    if (table->parent || table->invisible) return;
    bi = qof_book_get_data (qof_instance_get_book(table), _GNC_MOD_NAME);
    bi->tables = g_list_sort (bi->tables, (GCompareFunc)gncTaxTableCompare);
}

static inline void
mod_table (GncTaxTable *table)
{
    timespecFromTime64 (&table->modtime, gnc_time (NULL));
}

static inline void addObj (GncTaxTable *table)
{
    struct _book_info *bi;
    bi = qof_book_get_data (qof_instance_get_book(table), _GNC_MOD_NAME);
    bi->tables = g_list_insert_sorted (bi->tables, table,
                                       (GCompareFunc)gncTaxTableCompare);
}

static inline void remObj (GncTaxTable *table)
{
    struct _book_info *bi;
    bi = qof_book_get_data (qof_instance_get_book(table), _GNC_MOD_NAME);
    bi->tables = g_list_remove (bi->tables, table);
}

static inline void
gncTaxTableAddChild (GncTaxTable *table, GncTaxTable *child)
{
    g_return_if_fail(table);
    g_return_if_fail(child);
    g_return_if_fail(qof_instance_get_destroying(table) == FALSE);

    table->children = g_list_prepend(table->children, child);
}

static inline void
gncTaxTableRemoveChild (GncTaxTable *table, const GncTaxTable *child)
{
    g_return_if_fail(table);
    g_return_if_fail(child);

    if (qof_instance_get_destroying(table)) return;

    table->children = g_list_remove(table->children, child);
}

/* =============================================================== */

//
///** Return displayable name */
//static gchar*
//impl_get_display_name(const QofInstance* inst)
//{
//    GncTaxTable* tt;
//
//    g_return_val_if_fail(inst != NULL, FALSE);
////    g_return_val_if_fail(GNC_IS_TAXTABLE(inst), FALSE);
//    if(!inst)
//        return NULL;
//    if(typeid(*inst) != typeid(GncTaxTable))
//        return NULL;
//
//    tt = dynamic_cast<const GncTaxTable*>(inst);
//    return g_strdup_printf("Tax table %s", tt->name);
//}
//
///** Does this object refer to a specific object */
//static bool
//impl_refers_to_object(const QofInstance* inst, const QofInstance* ref)
//{
//    GncTaxTable* tt;
//
//    g_return_val_if_fail(inst != NULL, FALSE);
//    g_return_val_if_fail(GNC_IS_TAXTABLE(inst), FALSE);
//
//    tt = GNC_TAXTABLE(inst);
//
//    if (GNC_IS_ACCOUNT(ref))
//    {
//        GList* node;
//
//        for (node = tt->entries; node != NULL; node = node->next)
//        {
//            GncTaxTableEntry* tte = node->data;
//
//            if (tte->account == GNC_ACCOUNT(ref))
//            {
//                return TRUE;
//            }
//        }
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
//    if (!GNC_IS_ACCOUNT(ref))
//    {
//        return NULL;
//    }
//
//    return qof_instance_get_referring_object_list_from_collection(qof_instance_get_collection(inst), ref);
//}

/* Create/Destroy Functions */
GncTaxTable *
gncTaxTableCreate (QofBook *book)
{
    GncTaxTable *table;
    if (!book) return NULL;

    table = new GncTaxTable; //g_object_new (GNC_TYPE_TAXTABLE, NULL);
    qof_instance_init_data (table, _GNC_MOD_NAME, book);
    table->name = CACHE_INSERT ("");
    addObj (table);
    qof_event_gen (table, QOF_EVENT_CREATE, NULL);
    return table;
}

void
gncTaxTableDestroy (GncTaxTable *table)
{
    if (!table) return;
    qof_instance_set_destroying(table, TRUE);
    qof_instance_set_dirty (table);
    gncTaxTableCommitEdit (table);
}

static void
gncTaxTableFree (GncTaxTable *table)
{
    GList *list;
    GncTaxTable *child;

    if (!table) return;

    qof_event_gen (table, QOF_EVENT_DESTROY, NULL);
    CACHE_REMOVE (table->name);
    remObj (table);

    /* destroy the list of entries */
    for (list = table->entries; list; list = list->next)
        gncTaxTableEntryDestroy (list->data);
    g_list_free (table->entries);

    if (!qof_instance_get_destroying(table))
        PERR("free a taxtable without do_free set!");

    /* disconnect from parent */
    if (table->parent)
        gncTaxTableRemoveChild(table->parent, table);

    /* disconnect from the children */
    for (list = table->children; list; list = list->next)
    {
        child = list->data;
        gncTaxTableSetParent(child, NULL);
    }
    g_list_free(table->children);

    /* qof_instance_release (&table->inst); */
//    g_object_unref (table);
    delete table;
}

/* =============================================================== */

GncTaxTableEntry * gncTaxTableEntryCreate (void)
{
    GncTaxTableEntry *entry;
    entry = new GncTaxTableEntry;//g_new0 (GncTaxTableEntry, 1);
    entry->amount = gnc_numeric_zero ();
    return entry;
}

void gncTaxTableEntryDestroy (GncTaxTableEntry *entry)
{
    if (!entry) return;
//    g_free (entry);
    delete entry;
}

/* =============================================================== */
/* Set Functions */

void gncTaxTableSetName (GncTaxTable *table, const char *name)
{
    if (!table || !name) return;
    SET_STR (table, table->name, name);
    mark_table (table);
    maybe_resort_list (table);
    gncTaxTableCommitEdit (table);
}

void gncTaxTableSetParent (GncTaxTable *table, GncTaxTable *parent)
{
    if (!table) return;
    gncTaxTableBeginEdit (table);
    if (table->parent)
        gncTaxTableRemoveChild(table->parent, table);
    table->parent = parent;
    if (parent)
        gncTaxTableAddChild(parent, table);
    table->refcount = 0;
    gncTaxTableMakeInvisible (table);
    gncTaxTableCommitEdit (table);
}

void gncTaxTableSetChild (GncTaxTable *table, GncTaxTable *child)
{
    if (!table) return;
    gncTaxTableBeginEdit (table);
    table->child = child;
    gncTaxTableCommitEdit (table);
}

void gncTaxTableIncRef (GncTaxTable *table)
{
    if (!table) return;
    if (table->parent || table->invisible) return;        /* children dont need refcounts */
    gncTaxTableBeginEdit (table);
    table->refcount++;
    gncTaxTableCommitEdit (table);
}

void gncTaxTableDecRef (GncTaxTable *table)
{
    if (!table) return;
    if (table->parent || table->invisible) return;        /* children dont need refcounts */
    gncTaxTableBeginEdit (table);
    table->refcount--;
    g_return_if_fail (table->refcount >= 0);
    gncTaxTableCommitEdit (table);
}

void gncTaxTableSetRefcount (GncTaxTable *table, gint64 refcount)
{
    if (!table) return;
    table->refcount = refcount;
}

void gncTaxTableMakeInvisible (GncTaxTable *table)
{
    struct _book_info *bi;
    if (!table) return;
    gncTaxTableBeginEdit (table);
    table->invisible = TRUE;
    bi = qof_book_get_data (qof_instance_get_book(table), _GNC_MOD_NAME);
    bi->tables = g_list_remove (bi->tables, table);
    gncTaxTableCommitEdit (table);
}

void gncTaxTableEntrySetAccount (GncTaxTableEntry *entry, Account *account)
{
    if (!entry || !account) return;
    if (entry->account == account) return;
    entry->account = account;
    if (entry->table)
    {
        mark_table (entry->table);
        mod_table (entry->table);
    }
}

void gncTaxTableEntrySetType (GncTaxTableEntry *entry, GncAmountType type)
{
    if (!entry) return;
    if (entry->type == type) return;
    entry->type = type;
    if (entry->table)
    {
        mark_table (entry->table);
        mod_table (entry->table);
    }
}

void gncTaxTableEntrySetAmount (GncTaxTableEntry *entry, gnc_numeric amount)
{
    if (!entry) return;
    if (gnc_numeric_eq (entry->amount, amount)) return;
    entry->amount = amount;
    if (entry->table)
    {
        mark_table (entry->table);
        mod_table (entry->table);
    }
}

void gncTaxTableAddEntry (GncTaxTable *table, GncTaxTableEntry *entry)
{
    if (!table || !entry) return;
    if (entry->table == table) return; /* already mine */

    gncTaxTableBeginEdit (table);
    if (entry->table)
        gncTaxTableRemoveEntry (entry->table, entry);

    entry->table = table;
    table->entries = g_list_insert_sorted (table->entries, entry,
                                           (GCompareFunc)gncTaxTableEntryCompare);
    mark_table (table);
    mod_table (table);
    gncTaxTableCommitEdit (table);
}

void gncTaxTableRemoveEntry (GncTaxTable *table, GncTaxTableEntry *entry)
{
    if (!table || !entry) return;
    gncTaxTableBeginEdit (table);
    entry->table = NULL;
    table->entries = g_list_remove (table->entries, entry);
    mark_table (table);
    mod_table (table);
    gncTaxTableCommitEdit (table);
}

void gncTaxTableChanged (GncTaxTable *table)
{
    if (!table) return;
    gncTaxTableBeginEdit (table);
    table->child = NULL;
    gncTaxTableCommitEdit (table);
}

/* =============================================================== */

void gncTaxTableBeginEdit (GncTaxTable *table)
{
    qof_begin_edit(table);
}

static void gncTaxTableOnError (QofInstance *inst, QofBackendError errcode)
{
    PERR("TaxTable QofBackend Failure: %d", errcode);
    gnc_engine_signal_commit_error( errcode );
}

static void gncTaxTableOnDone (QofInstance *inst) {}

static void table_free (QofInstance *inst)
{
    GncTaxTable *table = dynamic_cast<GncTaxTable *>(inst);
    gncTaxTableFree (table);
}

void gncTaxTableCommitEdit (GncTaxTable *table)
{
    if (!qof_commit_edit (QOF_INSTANCE(table))) return;
    qof_commit_edit_part2 (table, gncTaxTableOnError,
                           gncTaxTableOnDone, table_free);
}


/* =============================================================== */
/* Get Functions */

GncTaxTable *gncTaxTableLookupByName (QofBook *book, const char *name)
{
    GList *list = gncTaxTableGetTables (book);

    for ( ; list; list = list->next)
    {
        GncTaxTable *table = list->data;
        if (!g_strcmp0 (table->name, name))
            return list->data;
    }
    return NULL;
}

GList * gncTaxTableGetTables (QofBook *book)
{
    struct _book_info *bi;
    if (!book) return NULL;

    bi = qof_book_get_data (book, _GNC_MOD_NAME);
    return bi->tables;
}

const char *gncTaxTableGetName (const GncTaxTable *table)
{
    if (!table) return NULL;
    return table->name;
}

static GncTaxTableEntry *gncTaxTableEntryCopy (const GncTaxTableEntry *entry)
{
    GncTaxTableEntry *e;
    if (!entry) return NULL;

    e = gncTaxTableEntryCreate ();
    gncTaxTableEntrySetAccount (e, entry->account);
    gncTaxTableEntrySetType (e, entry->type);
    gncTaxTableEntrySetAmount (e, entry->amount);

    return e;
}

static GncTaxTable *gncTaxTableCopy (const GncTaxTable *table)
{
    GncTaxTable *t;
    GList *list;

    if (!table) return NULL;
    t = gncTaxTableCreate (qof_instance_get_book(table));
    gncTaxTableSetName (t, table->name);
    for (list = table->entries; list; list = list->next)
    {
        GncTaxTableEntry *entry, *e;
        entry = list->data;
        e = gncTaxTableEntryCopy (entry);
        gncTaxTableAddEntry (t, e);
    }
    return t;
}

GncTaxTable *gncTaxTableReturnChild (GncTaxTable *table, bool make_new)
{
    GncTaxTable *child = NULL;

    if (!table) return NULL;
    if (table->child) return table->child;
    if (table->parent || table->invisible) return table;
    if (make_new)
    {
        child = gncTaxTableCopy (table);
        gncTaxTableSetChild (table, child);
        gncTaxTableSetParent (child, table);
    }
    return child;
}

GncTaxTable *gncTaxTableGetParent (const GncTaxTable *table)
{
    if (!table) return NULL;
    return table->parent;
}

GncTaxTableEntryList* gncTaxTableGetEntries (const GncTaxTable *table)
{
    if (!table) return NULL;
    return table->entries;
}

gint64 gncTaxTableGetRefcount (const GncTaxTable *table)
{
    if (!table) return 0;
    return table->refcount;
}

Timespec gncTaxTableLastModified (const GncTaxTable *table)
{
    Timespec ts = { 0 , 0 };
    if (!table) return ts;
    return table->modtime;
}

bool gncTaxTableGetInvisible (const GncTaxTable *table)
{
    if (!table) return FALSE;
    return table->invisible;
}

Account * gncTaxTableEntryGetAccount (const GncTaxTableEntry *entry)
{
    if (!entry) return NULL;
    return entry->account;
}

GncAmountType gncTaxTableEntryGetType (const GncTaxTableEntry *entry)
{
    if (!entry) return 0;
    return entry->type;
}

gnc_numeric gncTaxTableEntryGetAmount (const GncTaxTableEntry *entry)
{
    if (!entry) return gnc_numeric_zero();
    return entry->amount;
}

/* This is a semi-private function (meaning that it's not declared in
 * the header) used for SQL Backend testing. */
GncTaxTable* gncTaxTableEntryGetTable( const GncTaxTableEntry* entry )
{
    if (!entry) return NULL;
    return entry->table;
}

int gncTaxTableEntryCompare (const GncTaxTableEntry *a, const GncTaxTableEntry *b)
{
    char *name_a, *name_b;
    int retval;

    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;

    name_a = gnc_account_get_full_name (a->account);
    name_b = gnc_account_get_full_name (b->account);
    retval = g_strcmp0(name_a, name_b);
    g_free(name_a);
    g_free(name_b);

    if (retval)
        return retval;

    return gnc_numeric_compare (a->amount, b->amount);
}

int gncTaxTableCompare (const GncTaxTable *a, const GncTaxTable *b)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return g_strcmp0 (a->name, b->name);
}

bool gncTaxTableEntryEqual(const GncTaxTableEntry *a, const GncTaxTableEntry *b)
{
    if (a == NULL && b == NULL) return TRUE;
    if (a == NULL || b == NULL) return FALSE;

    if (!xaccAccountEqual(a->account, b->account, TRUE))
    {
        PWARN("accounts differ");
        return FALSE;
    }

    if (a->type != b->type)
    {
        PWARN("types differ");
        return FALSE;
    }

    if (!gnc_numeric_equal(a->amount, b->amount))
    {
        PWARN("amounts differ");
        return FALSE;
    }

    return TRUE;
}

bool gncTaxTableEqual(const GncTaxTable *a, const GncTaxTable *b)
{
    if (a == NULL && b == NULL) return TRUE;
    if (a == NULL || b == NULL) return FALSE;

//    g_return_val_if_fail(GNC_IS_TAXTABLE(a), FALSE);
//    g_return_val_if_fail(GNC_IS_TAXTABLE(b), FALSE);

    if (g_strcmp0(a->name, b->name) != 0)
    {
        PWARN("Names differ: %s vs %s", a->name, b->name);
        return FALSE;
    }

    if (a->invisible != b->invisible)
    {
        PWARN("invisible flags differ");
        return FALSE;
    }

    if ((a->entries != NULL) != (b->entries != NULL))
    {
        PWARN("only one has entries");
        return FALSE;
    }

    if (a->entries != NULL && b->entries != NULL)
    {
        GncTaxTableEntryList* a_node;
        GncTaxTableEntryList* b_node;

        for (a_node = a->entries, b_node = b->entries;
                a_node != NULL && b_node != NULL;
                a_node = a_node->next, b_node = b_node->next)
        {
            if (!gncTaxTableEntryEqual((GncTaxTableEntry*)a_node->data,
                                       (GncTaxTableEntry*)b_node->data))
            {
                PWARN("entries differ");
                return FALSE;
            }
        }

        if (a_node != NULL || b_node != NULL)
        {
            PWARN("Unequal number of entries");
            return FALSE;
        }
    }

#if 0
    /* See src/doc/business.txt for an explanation of the following */
    /* Code that handles this is *identical* to that in gncBillTerm */
    gint64          refcount;
    GncTaxTable *   parent;       /* if non-null, we are an immutable child */
    GncTaxTable *   child;        /* if non-null, we have not changed */
    GList *         children;     /* list of children for disconnection */
#endif

    return TRUE;
}

/*
 * This will add value to the account-value for acc, creating a new
 * list object if necessary
 */
GList *gncAccountValueAdd (GList *list, Account *acc, gnc_numeric value)
{
    GList *li;
    GncAccountValue *res = NULL;

    g_return_val_if_fail (acc, list);
    g_return_val_if_fail (gnc_numeric_check (value) == GNC_ERROR_OK, list);

    /* Try to find the account in the list */
    for (li = list; li; li = li->next)
    {
        res = li->data;
        if (res->account == acc)
        {
            res->value = gnc_numeric_add (res->value, value, GNC_DENOM_AUTO,
                                          GNC_HOW_DENOM_LCD);
            return list;
        }
    }
    /* Nope, didn't find it. */

    res = new GncAccountValue;//g_new0 (GncAccountValue, 1);
    res->account = acc;
    res->value = value;
    return g_list_prepend (list, res);
}

/* Merge l2 into l1.  l2 is not touched. */
GList *gncAccountValueAddList (GList *l1, GList *l2)
{
    GList *li;

    for (li = l2; li; li = li->next )
    {
        GncAccountValue *val = li->data;
        l1 = gncAccountValueAdd (l1, val->account, val->value);
    }

    return l1;
}

/* return the total for this list */
gnc_numeric gncAccountValueTotal (GList *list)
{
    gnc_numeric total = gnc_numeric_zero ();

    for ( ; list ; list = list->next)
    {
        GncAccountValue *val = list->data;
        total = gnc_numeric_add (total, val->value, GNC_DENOM_AUTO, GNC_HOW_DENOM_LCD);
    }
    return total;
}

/* Destroy a list of accountvalues */
void gncAccountValueDestroy (GList *list)
{
    GList *node;
    for ( node = list; node ; node = node->next)
        delete (GncAccountValue*)(node->data);
        //g_free ( (node->data);
    
    g_list_free (list);
}

/* Package-Private functions */

static void _gncTaxTableCreate (QofBook *book)
{
    struct _book_info *bi;

    if (!book) return;

    bi = new struct _book_info;//g_new0 (struct _book_info, 1);
    bi->tables = NULL;
    qof_book_set_data (book, _GNC_MOD_NAME, bi);
}

static void _gncTaxTableDestroy (QofBook *book)
{
    struct _book_info *bi;

    if (!book) return;

    bi = qof_book_get_data (book, _GNC_MOD_NAME);

    g_list_free (bi->tables);
//    g_free (bi);
    delete bi;
}

static QofObject gncTaxTableDesc =
{
    DI(.interface_version = ) QOF_OBJECT_VERSION,
    DI(.e_type            = ) _GNC_MOD_NAME,
    DI(.type_label        = ) "Tax Table",
    DI(.create            = ) (gpointer)gncTaxTableCreate,
    DI(.book_begin        = ) _gncTaxTableCreate,
    DI(.book_end          = ) _gncTaxTableDestroy,
    DI(.is_dirty          = ) qof_collection_is_dirty,
    DI(.mark_clean        = ) qof_collection_mark_clean,
    DI(.foreach           = ) qof_collection_foreach,
    DI(.printable         = ) NULL,
    DI(.version_cmp       = ) (int (*)(gpointer, gpointer)) qof_instance_version_cmp,
};

bool gncTaxTableRegister (void)
{
//    static QofParam params[] =
//    {
//        { GNC_TT_NAME, 		QOF_TYPE_STRING, 	(QofAccessFunc)gncTaxTableGetName, 		(QofSetterFunc)gncTaxTableSetName },
//        { GNC_TT_REFCOUNT, 	QOF_TYPE_INT64, 	(QofAccessFunc)gncTaxTableGetRefcount, 	(QofSetterFunc)gncTaxTableSetRefcount },
//        { QOF_PARAM_BOOK, 	QOF_ID_BOOK, 		(QofAccessFunc)qof_instance_get_book, 	NULL },
//        { QOF_PARAM_GUID, 	QOF_TYPE_GUID, 		(QofAccessFunc)qof_instance_get_guid, 	NULL },
//        { NULL },
//    };
//
//    qof_class_register (_GNC_MOD_NAME, (QofSortFunc)gncTaxTableCompare, params);

    return qof_object_register (&gncTaxTableDesc);
}

/* need a QOF tax table entry object */
//gncTaxTableEntrySetType_q int32
//gint gncTaxTableEntryGetType_q (GncTaxTableEntry *entry);
