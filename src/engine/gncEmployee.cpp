/********************************************************************\
 * gncEmployee.cpp -- the Core Employee Interface                   *
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

#include "Account.h"
#include "gnc-commodity.h"
#include "gncAddressP.h"
#include "gncEmployee.h"
#include "gncEmployeeP.h"

static gint gs_address_event_handler_id = 0;
static void listen_for_address_events(QofInstance *entity, QofEventId event_type,
                                      gpointer user_data, gpointer event_data);

static QofLogModule log_module = GNC_MOD_BUSINESS;

#define _GNC_MOD_NAME        GNC_ID_EMPLOYEE

GncEmployee::GncEmployee()
{
    id = NULL;
    username = NULL;
    addr = NULL;
    currency = NULL;
    active = false;
    language = NULL;
    acl = NULL;
    workday = gnc_numeric_zero();
    rate = gnc_numeric_zero();
    ccard_acc = NULL;
}

GncEmployee::~GncEmployee()
{
    
}

G_INLINE_FUNC void mark_employee (GncEmployee *employee);
void mark_employee (GncEmployee *employee)
{
    qof_instance_set_dirty(employee);
    qof_event_gen (employee, QOF_EVENT_MODIFY, NULL);
}

/* ============================================================== */

///** Does this object refer to a specific object */
//static bool
//impl_refers_to_object(const QofInstance* inst, const QofInstance* ref)
//{
//    GncEmployee* emp;
//
//    g_return_val_if_fail(inst != NULL, FALSE);
////    g_return_val_if_fail(GNC_IS_EMPLOYEE(inst), FALSE);
//
//    emp = (GncEmployee*)(inst);
//
//    if (GNC_IS_COMMODITY(ref))
//    {
//        return (emp->currency == GNC_COMMODITY(ref));
//    }
//    else if (GNC_IS_ACCOUNT(ref))
//    {
//        return (emp->ccard_acc == GNC_ACCOUNT(ref));
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
//    if (!GNC_IS_COMMODITY(ref) && !GNC_IS_ACCOUNT(ref))
//    {
//        return NULL;
//    }
//
//    return qof_instance_get_referring_object_list_from_collection(qof_instance_get_collection(inst), ref);
//}
//

/* Create/Destroy Functions */
GncEmployee *gncEmployeeCreate (QofBook *book)
{
    GncEmployee *employee;

    if (!book) return NULL;

    employee = new GncEmployee;// g_object_new (GNC_TYPE_EMPLOYEE, NULL);
    qof_instance_init_data (employee, _GNC_MOD_NAME, book);

    employee->id = CACHE_INSERT ("");
    employee->username = CACHE_INSERT ("");
    employee->language = CACHE_INSERT ("");
    employee->acl = CACHE_INSERT ("");
    employee->addr = gncAddressCreate (book, employee);
    employee->workday = gnc_numeric_zero();
    employee->rate = gnc_numeric_zero();
    employee->active = TRUE;

    if (gs_address_event_handler_id == 0)
    {
        gs_address_event_handler_id = qof_event_register_handler(listen_for_address_events, NULL);
    }

    qof_event_gen (employee, QOF_EVENT_CREATE, NULL);

    return employee;
}

void gncEmployeeDestroy (GncEmployee *employee)
{
    if (!employee) return;
    qof_instance_set_destroying(employee, TRUE);
    gncEmployeeCommitEdit(employee);
}

static void gncEmployeeFree (GncEmployee *employee)
{
    if (!employee) return;

    qof_event_gen (employee, QOF_EVENT_DESTROY, NULL);

    CACHE_REMOVE (employee->id);
    CACHE_REMOVE (employee->username);
    CACHE_REMOVE (employee->language);
    CACHE_REMOVE (employee->acl);
    gncAddressBeginEdit (employee->addr);
    gncAddressDestroy (employee->addr);

    /* qof_instance_release (&employee->inst); */
//    g_object_unref (employee);
    delete employee;
}

/* ============================================================== */
/* Set Functions */

#define SET_STR(obj, member, str) { \
        char * tmp; \
        \
        if (!g_strcmp0 (member, str)) return; \
        gncEmployeeBeginEdit (obj); \
        tmp = CACHE_INSERT (str); \
        CACHE_REMOVE (member); \
        member = tmp; \
        }

void gncEmployeeSetID (GncEmployee *employee, const char *id)
{
    if (!employee) return;
    if (!id) return;
    SET_STR(employee, employee->id, id);
    mark_employee (employee);
    gncEmployeeCommitEdit (employee);
}

void gncEmployeeSetUsername (GncEmployee *employee, const char *username)
{
    if (!employee) return;
    if (!username) return;
    SET_STR(employee, employee->username, username);
    mark_employee (employee);
    gncEmployeeCommitEdit (employee);
}

/* Employees don't have a name property defined, but
 * in order to get a consistent interface with other owner types,
 * this function fakes one by setting the name property of
 * the employee's address.
 */
void gncEmployeeSetName (GncEmployee *employee, const char *name)
{
    if (!employee) return;
    if (!name) return;
    gncAddressSetName (gncEmployeeGetAddr (employee), name);
}

void gncEmployeeSetLanguage (GncEmployee *employee, const char *language)
{
    if (!employee) return;
    if (!language) return;
    SET_STR(employee, employee->language, language);
    mark_employee (employee);
    gncEmployeeCommitEdit (employee);
}

void gncEmployeeSetAcl (GncEmployee *employee, const char *acl)
{
    if (!employee) return;
    if (!acl) return;
    SET_STR(employee, employee->acl, acl);
    mark_employee (employee);
    gncEmployeeCommitEdit (employee);
}

void gncEmployeeSetWorkday (GncEmployee *employee, gnc_numeric workday)
{
    if (!employee) return;
    if (gnc_numeric_equal (workday, employee->workday)) return;
    gncEmployeeBeginEdit (employee);
    employee->workday = workday;
    mark_employee (employee);
    gncEmployeeCommitEdit (employee);
}

void gncEmployeeSetRate (GncEmployee *employee, gnc_numeric rate)
{
    if (!employee) return;
    if (gnc_numeric_equal (rate, employee->rate)) return;
    gncEmployeeBeginEdit (employee);
    employee->rate = rate;
    mark_employee (employee);
    gncEmployeeCommitEdit (employee);
}

void gncEmployeeSetCurrency (GncEmployee *employee, gnc_commodity *currency)
{
    if (!employee || !currency) return;
    if (employee->currency &&
            gnc_commodity_equal (employee->currency, currency))
        return;
    gncEmployeeBeginEdit (employee);
    employee->currency = currency;
    mark_employee (employee);
    gncEmployeeCommitEdit (employee);
}

void gncEmployeeSetActive (GncEmployee *employee, bool active)
{
    if (!employee) return;
    if (active == employee->active) return;
    gncEmployeeBeginEdit (employee);
    employee->active = active;
    mark_employee (employee);
    gncEmployeeCommitEdit (employee);
}

void gncEmployeeSetCCard (GncEmployee *employee, Account* ccard_acc)
{
    if (!employee) return;
    if (ccard_acc == employee->ccard_acc) return;
    gncEmployeeBeginEdit (employee);
    employee->ccard_acc = ccard_acc;
    mark_employee (employee);
    gncEmployeeCommitEdit (employee);
}

void
qofEmployeeSetAddr (GncEmployee *employee, QofInstance *addr_ent)
{
    GncAddress *addr;

    if (!employee || !addr_ent)
    {
        return;
    }
    addr = (GncAddress*)addr_ent;
    if (addr == employee->addr)
    {
        return;
    }
    if (employee->addr != NULL)
    {
        gncAddressBeginEdit(employee->addr);
        gncAddressDestroy(employee->addr);
    }
    gncEmployeeBeginEdit(employee);
    employee->addr = addr;
    gncEmployeeCommitEdit(employee);
}

/* ============================================================== */
/* Get Functions */
const char * gncEmployeeGetID (const GncEmployee *employee)
{
    if (!employee) return NULL;
    return employee->id;
}

const char * gncEmployeeGetUsername (const GncEmployee *employee)
{
    if (!employee) return NULL;
    return employee->username;
}

/* Employees don't have a name property defined, but
 * in order to get a consistent interface with other owner types,
 * this function fakes one by returning the name property of
 * the employee's address.
 */
const char * gncEmployeeGetName (const GncEmployee *employee)
{
    if (!employee) return NULL;
    return gncAddressGetName ( gncEmployeeGetAddr (employee));
}

GncAddress * gncEmployeeGetAddr (const GncEmployee *employee)
{
    if (!employee) return NULL;
    return employee->addr;
}

const char * gncEmployeeGetLanguage (const GncEmployee *employee)
{
    if (!employee) return NULL;
    return employee->language;
}

const char * gncEmployeeGetAcl (const GncEmployee *employee)
{
    if (!employee) return NULL;
    return employee->acl;
}

gnc_numeric gncEmployeeGetWorkday (const GncEmployee *employee)
{
    if (!employee) return gnc_numeric_zero();
    return employee->workday;
}

gnc_numeric gncEmployeeGetRate (const GncEmployee *employee)
{
    if (!employee) return gnc_numeric_zero();
    return employee->rate;
}

gnc_commodity * gncEmployeeGetCurrency (const GncEmployee *employee)
{
    if (!employee) return NULL;
    return employee->currency;
}

bool gncEmployeeGetActive (const GncEmployee *employee)
{
    if (!employee) return FALSE;
    return employee->active;
}

Account * gncEmployeeGetCCard (const GncEmployee *employee)
{
    if (!employee) return NULL;
    return employee->ccard_acc;
}

bool gncEmployeeIsDirty (const GncEmployee *employee)
{
    if (!employee) return FALSE;
    return (qof_instance_get_dirty_flag(employee)
            || gncAddressIsDirty (employee->addr));
}

void gncEmployeeBeginEdit (GncEmployee *employee)
{
    qof_begin_edit(employee);
}

static void gncEmployeeOnError (QofInstance *employee, QofBackendError errcode)
{
    PERR("Employee QofBackend Failure: %d", errcode);
    gnc_engine_signal_commit_error( errcode );
}

static void gncEmployeeOnDone (QofInstance *inst)
{
    GncEmployee *employee = dynamic_cast<GncEmployee *>(inst);
    gncAddressClearDirty (employee->addr);
}

static void emp_free (QofInstance *inst)
{
    GncEmployee *employee = dynamic_cast<GncEmployee *>(inst);
    gncEmployeeFree (employee);
}


void gncEmployeeCommitEdit (GncEmployee *employee)
{
    if (!qof_commit_edit (QOF_INSTANCE(employee))) return;
    qof_commit_edit_part2 (employee, gncEmployeeOnError,
                           gncEmployeeOnDone, emp_free);
}

/* ============================================================== */
/* Other functions */

int gncEmployeeCompare (const GncEmployee *a, const GncEmployee *b)
{
    if (!a && !b) return 0;
    if (!a && b) return 1;
    if (a && !b) return -1;

    return(strcmp(a->username, b->username));
}

bool gncEmployeeEqual(const GncEmployee* a, const GncEmployee* b)
{
    if (a == NULL && b == NULL) return TRUE;
    if (a == NULL || b == NULL ) return FALSE;

//    g_return_val_if_fail(GNC_IS_EMPLOYEE(a), FALSE);
//    g_return_val_if_fail(GNC_IS_EMPLOYEE(b), FALSE);

    if (g_strcmp0(a->id, b->id) != 0)
    {
        PWARN("IDs differ: %s vs %s", a->id, b->id);
        return FALSE;
    }

    if (g_strcmp0(a->username, b->username) != 0)
    {
        PWARN("Usernames differ: %s vs %s", a->username, b->username);
        return FALSE;
    }

    if (!gncAddressEqual(a->addr, b->addr))
    {
        PWARN("Addresses differ");
        return FALSE;
    }

    if (!gnc_commodity_equal(a->currency, b->currency))
    {
        PWARN("Currencies differ");
        return FALSE;
    }

    if (a->active != b->active)
    {
        PWARN("Active flags differ");
        return FALSE;
    }

    if (g_strcmp0(a->language, b->language) != 0)
    {
        PWARN("Languages differ: %s vs %s", a->language, b->language);
        return FALSE;
    }

    if (g_strcmp0(a->acl, b->acl) != 0)
    {
        PWARN("ACLs differ: %s vs %s", a->acl, b->acl);
        return FALSE;
    }

    if (!xaccAccountEqual(a->ccard_acc, b->ccard_acc, TRUE))
    {
        PWARN("Accounts differ");
        return FALSE;
    }

    if (!gnc_numeric_equal(a->workday, b->workday))
    {
        PWARN("Workdays differ");
        return FALSE;
    }

    if (!gnc_numeric_equal(a->rate, b->rate))
    {
        PWARN("Rates differ");
        return FALSE;
    }

    return TRUE;
}

/* Package-Private functions */

static const char * _gncEmployeePrintable (gpointer item)
{
    GncEmployee *v = item;
    if (!item) return NULL;
    return gncAddressGetName(v->addr);
}

/**
 * Listens for MODIFY events from addresses.   If the address belongs to an employee,
 * mark the employee as dirty.
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
    GncEmployee* empl;

    if ((event_type & QOF_EVENT_MODIFY) == 0)
    {
        return;
    }
    if(!entity)
        return;
    if (typeid(*entity) != typeid(GncAddress))
    {
        return;
    }
//    if (typeid(event_data) != typeid(GncEmployee*))
//    {
//        return;
//    }
    // TODO: insufficient type checks
    empl = reinterpret_cast<GncEmployee*>(event_data);
    gncEmployeeBeginEdit(empl);
    mark_employee(empl);
    gncEmployeeCommitEdit(empl);
}

static void
destroy_employee_on_book_close(QofInstance *ent, gpointer data)
{
    GncEmployee* e = dynamic_cast<GncEmployee*>(ent);

    gncEmployeeBeginEdit(e);
    gncEmployeeDestroy(e);
}

/** Handles book end - frees all employees from the book
 *
 * @param book Book being closed
 */
static void
gnc_employee_book_end(QofBook* book)
{
    QofCollection *col;

    col = qof_book_get_collection(book, GNC_ID_EMPLOYEE);
    qof_collection_foreach(col, destroy_employee_on_book_close, NULL);
}

static QofObject gncEmployeeDesc =
{
    DI(.interface_version = ) QOF_OBJECT_VERSION,
    DI(.e_type            = ) _GNC_MOD_NAME,
    DI(.type_label        = ) "Employee",
    DI(.create            = ) (gpointer)gncEmployeeCreate,
    DI(.book_begin        = ) NULL,
    DI(.book_end          = ) gnc_employee_book_end,
    DI(.is_dirty          = ) qof_collection_is_dirty,
    DI(.mark_clean        = ) qof_collection_mark_clean,
    DI(.foreach           = ) qof_collection_foreach,
    DI(.printable         = ) _gncEmployeePrintable,
    DI(.version_cmp       = ) (int (*)(gpointer, gpointer)) qof_instance_version_cmp,
};

bool gncEmployeeRegister (void)
{
//    static QofParam params[] =
//    {
//        { EMPLOYEE_ID, QOF_TYPE_STRING, (QofAccessFunc)gncEmployeeGetID, (QofSetterFunc)gncEmployeeSetID },
//        {
//            EMPLOYEE_USERNAME, QOF_TYPE_STRING, (QofAccessFunc)gncEmployeeGetUsername,
//            (QofSetterFunc)gncEmployeeSetUsername
//        },
//        {
//            EMPLOYEE_NAME, QOF_TYPE_STRING, (QofAccessFunc)gncEmployeeGetName,
//            (QofSetterFunc)gncEmployeeSetName
//        },
//        {
//            EMPLOYEE_LANGUAGE, QOF_TYPE_STRING, (QofAccessFunc)gncEmployeeGetLanguage,
//            (QofSetterFunc)gncEmployeeSetLanguage
//        },
//        { EMPLOYEE_ACL, QOF_TYPE_STRING, (QofAccessFunc)gncEmployeeGetAcl, (QofSetterFunc)gncEmployeeSetAcl },
//        {
//            EMPLOYEE_WORKDAY, QOF_TYPE_NUMERIC, (QofAccessFunc)gncEmployeeGetWorkday,
//            (QofSetterFunc)gncEmployeeSetWorkday
//        },
//        { EMPLOYEE_RATE, QOF_TYPE_NUMERIC, (QofAccessFunc)gncEmployeeGetRate, (QofSetterFunc)gncEmployeeSetRate },
//        { EMPLOYEE_ADDR, GNC_ID_ADDRESS, (QofAccessFunc)gncEmployeeGetAddr, (QofSetterFunc)qofEmployeeSetAddr },
//        { EMPLOYEE_CC,  GNC_ID_ACCOUNT, (QofAccessFunc)gncEmployeeGetCCard, (QofSetterFunc)gncEmployeeSetCCard },
//        { QOF_PARAM_ACTIVE, QOF_TYPE_BOOLEAN, (QofAccessFunc)gncEmployeeGetActive, (QofSetterFunc)gncEmployeeSetActive },
//        { QOF_PARAM_BOOK, QOF_ID_BOOK, (QofAccessFunc)qof_instance_get_book, NULL },
//        { QOF_PARAM_GUID, QOF_TYPE_GUID, (QofAccessFunc)qof_instance_get_guid, NULL },
//        { NULL },
//    };
//
//    qof_class_register (_GNC_MOD_NAME, (QofSortFunc)gncEmployeeCompare, params);

    return qof_object_register (&gncEmployeeDesc);
}

gchar *gncEmployeeNextID (QofBook *book)
{
    return qof_book_increment_and_format_counter (book, _GNC_MOD_NAME);
}
