/********************************************************************\
 * gncJob.cpp -- the Core Job Interface                             *
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
 * Copyright (C) 2003 Linas Vepstas <linas@linas.org>
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#include "config.h"

#include <glib.h>
#include <string.h>

#include "gncInvoice.h"
#include "gncJob.h"
#include "gncJobP.h"
#include "gncOwnerP.h"

static QofLogModule log_module = GNC_MOD_BUSINESS;

#define _GNC_MOD_NAME        GNC_ID_JOB

/* ================================================================== */
/* misc inline functions */

G_INLINE_FUNC void mark_job (GncJob *job);
void mark_job (GncJob *job)
{
    qof_instance_set_dirty(job);
    qof_event_gen (job, QOF_EVENT_MODIFY, NULL);
}

/* ================================================================== */

    
GncJob::GncJob()
{
    id = NULL;
    name = NULL;
    desc = NULL;
    active = false;
}

GncJob::~GncJob()
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
//    /* Refers to nothing */
//    return NULL;
//}

/* Create/Destroy Functions */
GncJob *gncJobCreate (QofBook *book)
{
    GncJob *job;

    if (!book) return NULL;

    job = new GncJob; //g_object_new (GNC_TYPE_JOB, NULL);
    qof_instance_init_data (job, _GNC_MOD_NAME, book);

    job->id = CACHE_INSERT ("");
    job->name = CACHE_INSERT ("");
    job->desc = CACHE_INSERT ("");
    job->active = TRUE;

    /* GncOwner not initialized */
    qof_event_gen (job, QOF_EVENT_CREATE, NULL);

    return job;
}

void gncJobDestroy (GncJob *job)
{
    if (!job) return;
    qof_instance_set_destroying(job, TRUE);
    gncJobCommitEdit (job);
}

static void gncJobFree (GncJob *job)
{
    if (!job) return;

    qof_event_gen (job, QOF_EVENT_DESTROY, NULL);

    CACHE_REMOVE (job->id);
    CACHE_REMOVE (job->name);
    CACHE_REMOVE (job->desc);

    switch (gncOwnerGetType (&(job->owner)))
    {
    case GNC_OWNER_CUSTOMER:
        gncCustomerRemoveJob (gncOwnerGetCustomer(&job->owner), job);
        break;
    case GNC_OWNER_VENDOR:
        gncVendorRemoveJob (gncOwnerGetVendor(&job->owner), job);
        break;
    default:
        break;
    }

    /* qof_instance_release (&job->inst); */
//    g_object_unref (job);
    delete job;
}


/* ================================================================== */
/* Set Functions */

#define SET_STR(obj, member, str) { \
        char * tmp; \
        \
        if (!g_strcmp0 (member, str)) return; \
        gncJobBeginEdit (obj); \
        tmp = CACHE_INSERT (str); \
        CACHE_REMOVE (member); \
        member = tmp; \
        }

void gncJobSetID (GncJob *job, const char *id)
{
    if (!job) return;
    if (!id) return;
    SET_STR(job, job->id, id);
    mark_job (job);
    gncJobCommitEdit (job);
}

void gncJobSetName (GncJob *job, const char *name)
{
    if (!job) return;
    if (!name) return;
    SET_STR(job, job->name, name);
    mark_job (job);
    gncJobCommitEdit (job);
}

void gncJobSetReference (GncJob *job, const char *desc)
{
    if (!job) return;
    if (!desc) return;
    SET_STR(job, job->desc, desc);
    mark_job (job);
    gncJobCommitEdit (job);
}

void gncJobSetOwner (GncJob *job, GncOwner *owner)
{
    if (!job) return;
    if (!owner) return;
    if (gncOwnerEqual (owner, &(job->owner))) return;

    switch (gncOwnerGetType (owner))
    {
    case GNC_OWNER_CUSTOMER:
    case GNC_OWNER_VENDOR:
        break;
    default:
        PERR("Unsupported Owner type: %d", gncOwnerGetType(owner));
        return;
    }

    gncJobBeginEdit (job);

    switch (gncOwnerGetType (&(job->owner)))
    {
    case GNC_OWNER_CUSTOMER:
        gncCustomerRemoveJob (gncOwnerGetCustomer(&job->owner), job);
        break;
    case GNC_OWNER_VENDOR:
        gncVendorRemoveJob (gncOwnerGetVendor(&job->owner), job);
        break;
    default:
        break;
    }

    gncOwnerCopy (owner, &(job->owner));

    switch (gncOwnerGetType (&(job->owner)))
    {
    case GNC_OWNER_CUSTOMER:
        gncCustomerAddJob (gncOwnerGetCustomer(&job->owner), job);
        break;
    case GNC_OWNER_VENDOR:
        gncVendorAddJob (gncOwnerGetVendor(&job->owner), job);
        break;
    default:
        break;
    }

    mark_job (job);
    gncJobCommitEdit (job);
}

void gncJobSetActive (GncJob *job, bool active)
{
    if (!job) return;
    if (active == job->active) return;
    gncJobBeginEdit (job);
    job->active = active;
    mark_job (job);
    gncJobCommitEdit (job);
}

static void
qofJobSetOwner (GncJob *job, QofInstance *ent)
{
    if (!job || !ent)
    {
        return;
    }
    qof_begin_edit(job);
    qofOwnerSetEntity(&job->owner, ent);
    mark_job (job);
    qof_commit_edit(job);
}

void gncJobBeginEdit (GncJob *job)
{
    qof_begin_edit(job);
}

static void gncJobOnError (QofInstance *inst, QofBackendError errcode)
{
    PERR("Job QofBackend Failure: %d", errcode);
    gnc_engine_signal_commit_error( errcode );
}

static void job_free (QofInstance *inst)
{
    GncJob *job = dynamic_cast<GncJob *>(inst);
    gncJobFree (job);
}

static void gncJobOnDone (QofInstance *qof) { }

void gncJobCommitEdit (GncJob *job)
{
    if (!qof_commit_edit (QOF_INSTANCE(job))) return;
    qof_commit_edit_part2 (job, gncJobOnError,
                           gncJobOnDone, job_free);
}

/* ================================================================== */
/* Get Functions */

const char * gncJobGetID (const GncJob *job)
{
    if (!job) return NULL;
    return job->id;
}

const char * gncJobGetName (const GncJob *job)
{
    if (!job) return NULL;
    return job->name;
}

const char * gncJobGetReference (const GncJob *job)
{
    if (!job) return NULL;
    return job->desc;
}

GncOwner * gncJobGetOwner (GncJob *job)
{
    if (!job) return NULL;
    return &(job->owner);
}

bool gncJobGetActive (const GncJob *job)
{
    if (!job) return FALSE;
    return job->active;
}

static QofInstance*
qofJobGetOwner (GncJob *job)
{
    if (!job)
    {
        return NULL;
    }
    return QOF_INSTANCE(qofOwnerGetOwner(&job->owner));
}

/* Other functions */

int gncJobCompare (const GncJob * a, const GncJob *b)
{
    if (!a && !b) return 0;
    if (!a && b) return 1;
    if (a && !b) return -1;

    return (g_strcmp0(a->id, b->id));
}

bool gncJobEqual(const GncJob * a, const GncJob *b)
{
    if (a == NULL && b == NULL) return TRUE;
    if (a == NULL || b == NULL) return FALSE;

//    g_return_val_if_fail(GNC_IS_JOB(a), FALSE);
//    g_return_val_if_fail(GNC_IS_JOB(b), FALSE);

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

    if (g_strcmp0(a->desc, b->desc) != 0)
    {
        PWARN("Descriptions differ: %s vs %s", a->desc, b->desc);
        return FALSE;
    }

    if (a->active != b->active)
    {
        PWARN("Active flags differ");
        return FALSE;
    }

    /* FIXME: Need real tests */
#if 0
    GncOwner      owner;
#endif

    return TRUE;
}

/* ================================================================== */
/* Package-Private functions */

static const char * _gncJobPrintable (gpointer item)
{
    GncJob *c;
    if (!item) return NULL;
    c = item;
    return c->name;
}

static QofObject gncJobDesc =
{
    DI(.interface_version = ) QOF_OBJECT_VERSION,
    DI(.e_type            = ) _GNC_MOD_NAME,
    DI(.type_label        = ) "Job",
    DI(.create            = ) (gpointer)gncJobCreate,
    DI(.book_begin        = ) NULL,
    DI(.book_end          = ) NULL,
    DI(.is_dirty          = ) qof_collection_is_dirty,
    DI(.mark_clean        = ) qof_collection_mark_clean,
    DI(.foreach           = ) qof_collection_foreach,
    DI(.printable         = ) _gncJobPrintable,
    DI(.version_cmp       = ) (int (*)(gpointer, gpointer)) qof_instance_version_cmp,
};

bool gncJobRegister (void)
{
    static QofParam params[] =
    {
        { JOB_ID, QOF_TYPE_STRING, (QofAccessFunc)gncJobGetID, (QofSetterFunc)gncJobSetID },
        { JOB_NAME, QOF_TYPE_STRING, (QofAccessFunc)gncJobGetName, (QofSetterFunc)gncJobSetName },
        { JOB_ACTIVE, QOF_TYPE_BOOLEAN, (QofAccessFunc)gncJobGetActive, (QofSetterFunc)gncJobSetActive },
        { JOB_REFERENCE, QOF_TYPE_STRING, (QofAccessFunc)gncJobGetReference, (QofSetterFunc)gncJobSetReference },
#ifdef GNUCASH_MAJOR_VERSION
        { JOB_OWNER, GNC_ID_OWNER, (QofAccessFunc)gncJobGetOwner, NULL },
#else
        { JOB_OWNER, QOF_TYPE_CHOICE, (QofAccessFunc)qofJobGetOwner, (QofSetterFunc)qofJobSetOwner },
#endif
        { QOF_PARAM_ACTIVE, QOF_TYPE_BOOLEAN, (QofAccessFunc)gncJobGetActive, NULL },
        { QOF_PARAM_BOOK, QOF_ID_BOOK, (QofAccessFunc)qof_instance_get_book, NULL },
        { QOF_PARAM_GUID, QOF_TYPE_GUID, (QofAccessFunc)qof_instance_get_guid, NULL },
        { NULL },
    };

    if (!qof_choice_create(GNC_ID_JOB))
    {
        return FALSE;
    }
    if (!qof_choice_add_class(GNC_ID_INVOICE, GNC_ID_JOB, INVOICE_OWNER))
    {
        return FALSE;
    }

    qof_class_register (_GNC_MOD_NAME, (QofSortFunc)gncJobCompare, params);
#ifdef GNUCASH_MAJOR_VERSION
    qofJobGetOwner(NULL);
    qofJobSetOwner(NULL, NULL);
#endif
    return qof_object_register (&gncJobDesc);
}

gchar *gncJobNextID (QofBook *book)
{
    return qof_book_increment_and_format_counter (book, _GNC_MOD_NAME);
}
