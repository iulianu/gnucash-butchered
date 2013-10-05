/********************************************************************\
 * gncAddress.cpp -- an Address object                                *
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
 * Copyright (C) 2001 Derek Atkins
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#include "config.h"

#include <glib.h>

#include "gncAddress.h"
#include "gncAddressP.h"
#include "gncCustomerP.h"

//struct _gncAddressClass
//{
//    QofInstanceClass parent_class;
//};

static QofLogModule log_module = GNC_MOD_BUSINESS;

#define _GNC_MOD_NAME	GNC_ADDRESS_MODULE_NAME

G_INLINE_FUNC void mark_address (GncAddress *address);
void mark_address (GncAddress *address)
{
    address->dirty = TRUE;

    qof_event_gen (QOF_INSTANCE(address), QOF_EVENT_MODIFY, address->parent);
    qof_event_gen (address->parent, QOF_EVENT_MODIFY, NULL);
}
    
GncAddress::GncAddress()
{
    book = NULL;
    parent = NULL;
    dirty = false;
    name = NULL;
    addr1 = NULL;
    addr2 = NULL;
    addr3 = NULL;
    addr4 = NULL;
    phone = NULL;
    fax = NULL;
    email = NULL;
}
    
GncAddress::~GncAddress()
{
    
}


///** Returns a list of my type of object which refers to an object.  For example, when called as
//        qof_instance_get_typed_referring_object_list(taxtable, account);
//    it will return the list of taxtables which refer to a specific account.  The result should be the
//    same regardless of which taxtable object is used.  The list must be freed by the caller but the
//    objects on the list must not.
// */
//static GList*
//impl_get_typed_referring_object_list(const QofInstance* inst, const QofInstance* ref)
//{
//    /* Refers to nothing.  The parent field doesn't really count since the parent knows which address
//       belongs to it. */
//    return NULL;
//}

/* Create/Destroy functions */

GncAddress *
gncAddressCreate (QofBook *book, QofInstance *prnt)
{
    GncAddress *addr;

    if (!book) return NULL;

    addr = new GncAddress; //g_object_new (GNC_TYPE_ADDRESS, NULL);
    qof_instance_init_data(addr, GNC_ID_ADDRESS, book);
    addr->book = book;
    addr->dirty = FALSE;
    addr->parent = prnt;

    addr->name = CACHE_INSERT ("");
    addr->addr1 = CACHE_INSERT ("");
    addr->addr2 = CACHE_INSERT ("");
    addr->addr3 = CACHE_INSERT ("");
    addr->addr4 = CACHE_INSERT ("");
    addr->phone = CACHE_INSERT ("");
    addr->fax = CACHE_INSERT ("");
    addr->email = CACHE_INSERT ("");

    return addr;
}

static GncAddress *
qofAddressCreate (QofBook *book)
{
    /* The address will get set later by another function */
    return gncAddressCreate(book, NULL);
}

static void
qofAddressSetOwner(GncAddress *addr, QofInstance *ent)
{
    if (!addr || !ent)
    {
        return;
    }
    if (addr->parent == NULL)
    {
        addr->parent = ent;
    }
}

static QofInstance*
qofAddressGetOwner(const GncAddress *addr)
{

    if (!addr)
    {
        return NULL;
    }
    return addr->parent;
}

void
gncAddressDestroy (GncAddress *addr)
{
    if (!addr) return;
    qof_instance_set_destroying(addr, TRUE);
    gncAddressCommitEdit (addr);
}

static void
gncAddressFree (GncAddress *addr)
{
    if (!addr) return;

    qof_event_gen (addr, QOF_EVENT_DESTROY, NULL);

    CACHE_REMOVE (addr->name);
    CACHE_REMOVE (addr->addr1);
    CACHE_REMOVE (addr->addr2);
    CACHE_REMOVE (addr->addr3);
    CACHE_REMOVE (addr->addr4);
    CACHE_REMOVE (addr->phone);
    CACHE_REMOVE (addr->fax);
    CACHE_REMOVE (addr->email);

    /* qof_instance_release (&addr->inst); */
//    g_object_unref (addr);
    delete addr;
}


/* Set functions */

#define SET_STR(obj, member, str) { \
	char * tmp; \
	\
	if (member == str) return; \
	if (!g_strcmp0 (member, str)) return; \
	gncAddressBeginEdit (obj); \
	tmp = CACHE_INSERT (str); \
	CACHE_REMOVE (member); \
	member = tmp; \
	}

void gncAddressSetName (GncAddress *addr, const char *name)
{
    if (!addr) return;
    if (!name) return;
    SET_STR(addr, addr->name, name);
    mark_address (addr);
    gncAddressCommitEdit (addr);
}

void gncAddressSetAddr1 (GncAddress *addr, const char *addr1)
{
    if (!addr) return;
    if (!addr1) return;
    SET_STR(addr, addr->addr1, addr1);
    mark_address (addr);
    gncAddressCommitEdit (addr);
}

void gncAddressSetAddr2 (GncAddress *addr, const char *addr2)
{
    if (!addr) return;
    if (!addr2) return;
    SET_STR(addr, addr->addr2, addr2);
    mark_address (addr);
    gncAddressCommitEdit (addr);
}

void gncAddressSetAddr3 (GncAddress *addr, const char *addr3)
{
    if (!addr) return;
    if (!addr3) return;
    SET_STR(addr, addr->addr3, addr3);
    mark_address (addr);
    gncAddressCommitEdit (addr);
}

void gncAddressSetAddr4 (GncAddress *addr, const char *addr4)
{
    if (!addr) return;
    if (!addr4) return;
    SET_STR(addr, addr->addr4, addr4);
    mark_address (addr);
    gncAddressCommitEdit (addr);
}

void gncAddressSetPhone (GncAddress *addr, const char *phone)
{
    if (!addr) return;
    if (!phone) return;
    SET_STR(addr, addr->phone, phone);
    mark_address (addr);
    gncAddressCommitEdit (addr);
}

void gncAddressSetFax (GncAddress *addr, const char *fax)
{
    if (!addr) return;
    if (!fax) return;
    SET_STR(addr, addr->fax, fax);
    mark_address (addr);
    gncAddressCommitEdit (addr);
}

void gncAddressSetEmail (GncAddress *addr, const char *email)
{
    if (!addr) return;
    if (!email) return;
    SET_STR(addr, addr->email, email);
    mark_address (addr);
    gncAddressCommitEdit (addr);
}

void gncAddressBeginEdit (GncAddress *addr)
{
    qof_begin_edit (addr);
}

static void gncAddressOnError (QofInstance *inst, QofBackendError errcode)
{
    PERR("Address QofBackend Failure: %d", errcode);
    gnc_engine_signal_commit_error( errcode );
}

static void gncAddressOnDone (QofInstance *addr) { }

static void address_free (QofInstance *inst)
{
    GncAddress *addr = dynamic_cast<GncAddress *>(inst);
    gncAddressFree (addr);
}

void gncAddressCommitEdit (GncAddress *addr)
{
    if (!qof_commit_edit (addr)) return;
    qof_commit_edit_part2 (addr, gncAddressOnError,
                           gncAddressOnDone, address_free);
}


/* Get Functions */

const char * gncAddressGetName (const GncAddress *addr)
{
    if (!addr) return NULL;
    return addr->name;
}

const char * gncAddressGetAddr1 (const GncAddress *addr)
{
    if (!addr) return NULL;
    return addr->addr1;
}

const char * gncAddressGetAddr2 (const GncAddress *addr)
{
    if (!addr) return NULL;
    return addr->addr2;
}

const char * gncAddressGetAddr3 (const GncAddress *addr)
{
    if (!addr) return NULL;
    return addr->addr3;
}

const char * gncAddressGetAddr4 (const GncAddress *addr)
{
    if (!addr) return NULL;
    return addr->addr4;
}

const char * gncAddressGetPhone (const GncAddress *addr)
{
    if (!addr) return NULL;
    return addr->phone;
}

const char * gncAddressGetFax (const GncAddress *addr)
{
    if (!addr) return NULL;
    return addr->fax;
}

const char * gncAddressGetEmail (const GncAddress *addr)
{
    if (!addr) return NULL;
    return addr->email;
}

gboolean gncAddressIsDirty (const GncAddress *addr)
{
    if (!addr) return FALSE;
    return addr->dirty;
}

void gncAddressClearDirty (GncAddress *addr)
{
    if (!addr) return;
    addr->dirty = FALSE;
}

int gncAddressCompare (const GncAddress *a, const GncAddress *b)
{
    if (!a && !b) return 0;
    if (!a && b) return 1;
    if (a && !b) return -1;

    return g_strcmp0 (a->name, b->name);
}

gboolean
gncAddressEqual(const GncAddress* a, const GncAddress* b)
{
    if (a == NULL && b == NULL) return TRUE;
    if (a == NULL || b == NULL) return FALSE;

//    g_return_val_if_fail(GNC_IS_ADDRESS(a), FALSE);
//    g_return_val_if_fail(GNC_IS_ADDRESS(b), FALSE);

    if (g_strcmp0(a->name, b->name) != 0)
    {
        PWARN("names differ: %s vs %s", a->name, b->name);
        return FALSE;
    }
    if (g_strcmp0(a->addr1, b->addr1) != 0)
    {
        PWARN("address lines 1 differ: %s vs %s", a->addr1, b->addr1);
        return FALSE;
    }
    if (g_strcmp0(a->addr2, b->addr2) != 0)
    {
        PWARN("address lines 2 differ: %s vs %s", a->addr2, b->addr1);
        return FALSE;
    }
    if (g_strcmp0(a->addr3, b->addr3) != 0)
    {
        PWARN("address lines 3 differ: %s vs %s", a->addr3, b->addr3);
        return FALSE;
    }
    if (g_strcmp0(a->addr4, b->addr4) != 0)
    {
        PWARN("address lines 4 differ: %s vs %s", a->addr4, b->addr4);
        return FALSE;
    }
    if (g_strcmp0(a->phone, b->phone) != 0)
    {
        PWARN("phone numbers differ: %s vs %s", a->phone, b->phone);
        return FALSE;
    }
    if (g_strcmp0(a->fax, b->fax) != 0)
    {
        PWARN("fax numbers differ: %s vs %s", a->fax, b->fax);
        return FALSE;
    }
    if (g_strcmp0(a->email, b->email) != 0)
    {
        PWARN("email addresses differ: %s vs %s", a->email, b->email);
        return FALSE;
    }

    return TRUE;
}

static QofObject GncAddressDesc =
{
    DI(.interface_version = ) QOF_OBJECT_VERSION,
    DI(.e_type            = ) GNC_ID_ADDRESS,
    DI(.type_label        = ) "Address",
    DI(.create            = ) (gpointer)qofAddressCreate,
    DI(.book_begin        = ) NULL,
    DI(.book_end          = ) NULL,
    DI(.is_dirty          = ) qof_collection_is_dirty,
    DI(.mark_clean        = ) qof_collection_mark_clean,
    DI(.foreach           = ) qof_collection_foreach,
    DI(.printable         = ) NULL,
    DI(.version_cmp       = ) (int (*)(gpointer, gpointer)) qof_instance_version_cmp,
};

gboolean gncAddressRegister (void)
{
    static QofParam params[] =
    {

        { ADDRESS_NAME,  QOF_TYPE_STRING, (QofAccessFunc)gncAddressGetName,  (QofSetterFunc)gncAddressSetName },
        { ADDRESS_ONE,   QOF_TYPE_STRING, (QofAccessFunc)gncAddressGetAddr1, (QofSetterFunc)gncAddressSetAddr1 },
        { ADDRESS_TWO,   QOF_TYPE_STRING, (QofAccessFunc)gncAddressGetAddr2, (QofSetterFunc)gncAddressSetAddr2 },
        { ADDRESS_THREE, QOF_TYPE_STRING, (QofAccessFunc)gncAddressGetAddr3, (QofSetterFunc)gncAddressSetAddr3 },
        { ADDRESS_FOUR,  QOF_TYPE_STRING, (QofAccessFunc)gncAddressGetAddr4, (QofSetterFunc)gncAddressSetAddr4 },
        { ADDRESS_PHONE, QOF_TYPE_STRING, (QofAccessFunc)gncAddressGetPhone, (QofSetterFunc)gncAddressSetPhone },
        { ADDRESS_FAX,   QOF_TYPE_STRING, (QofAccessFunc)gncAddressGetFax,   (QofSetterFunc)gncAddressSetFax },
        { ADDRESS_EMAIL, QOF_TYPE_STRING, (QofAccessFunc)gncAddressGetEmail, (QofSetterFunc)gncAddressSetEmail },
        { ADDRESS_OWNER, QOF_TYPE_CHOICE, (QofAccessFunc)qofAddressGetOwner, (QofSetterFunc)qofAddressSetOwner },
        { QOF_PARAM_BOOK, QOF_ID_BOOK,   (QofAccessFunc)qof_instance_get_book, NULL },
        { QOF_PARAM_GUID, QOF_TYPE_GUID, (QofAccessFunc)qof_instance_get_guid, NULL },
        { NULL },
    };

    qof_class_register (GNC_ID_ADDRESS, (QofSortFunc)gncAddressCompare, params);
    if (!qof_choice_add_class(GNC_ID_CUSTOMER, GNC_ID_ADDRESS, ADDRESS_OWNER))
    {
        return FALSE;
    }

    return qof_object_register(&GncAddressDesc);
}
