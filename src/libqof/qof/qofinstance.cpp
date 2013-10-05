/********************************************************************\
 * qofinstance.cpp -- handler for fields common to all objects      *
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
 * Object instance holds many common fields that most
 * gnucash objects use.
 *
 * Copyright (C) 2003 Linas Vepstas <linas@linas.org>
 * Copyright (c) 2007 David Hampton <hampton@employees.org>
 */

#include "config.h"
#include <glib.h>
#include "qof.h"
#include "kvp-util-p.h"
#include "qofbook-p.h"
#include "qofid-p.h"
#include "qofinstance-p.h"

static QofLogModule log_module = QOF_MOD_ENGINE;

/* ========================================================== */

class QofInstancePrivate
{
public:
    GncGUID guid;                  /**< GncGUID for the entity */
    QofCollection  *collection; /**< Entity collection */

    /* The entity_table in which this instance is stored */
    QofBook * book;

    /* kvp_data is a key-value pair database for storing arbirtary
     * information associated with this instance.
     * See src/engine/kvp_doc.txt for a list and description of the
     * important keys. */
//    KvpFrame *kvp_data;

    /*  Timestamp used to track the last modification to this
     *  instance.  Typically used to compare two versions of the
     *  same object, to see which is newer.  When used with the
     *  SQL backend, this field is reserved for SQL use, to compare
     *  the version in local memory to the remote, server version.
     */
    Timespec last_update;

    /*  Keep track of nesting level of begin/end edit calls */
    int editlevel;

    /*  In process of being destroyed */
    bool do_free;

    /*  dirty/clean flag. If dirty, then this instance has been modified,
     *  but has not yet been written out to storage (file/database)
     */
    bool dirty;

    /* True iff this instance has never been committed. */
    bool infant;

    /* version number, used for tracking multiuser updates */
    int32_t version;
    uint32_t version_check;  /* data aging timestamp */

    /* -------------------------------------------------------------- */
    /* Backend private expansion data */
    uint32_t  idata;   /* used by the sql backend for kvp management */
    
    QofInstancePrivate()
    {
        guid = * guid_null();
        collection = NULL;
        book = NULL;
        last_update = {0,0};
        editlevel = 0;
        do_free = false;
        dirty = false;
        infant = false;
        version = 0;
        version_check = 0;
        idata = 0;
    }
};

#define GET_PRIVATE(o)  \
   (((QofInstance*)o)->priv)


QofInstance::QofInstance()
{
    init_();
}

void QofInstance::init_()
{
    this->priv = new QofInstancePrivate;
    priv = GET_PRIVATE(this);
    priv->book = NULL;
    this->kvp_data = kvp_frame_new();
    priv->last_update.tv_sec = 0;
    priv->last_update.tv_nsec = -1;
    priv->editlevel = 0;
    priv->do_free = false;
    priv->dirty = false;
    priv->infant = true;
}

QofInstance::QofInstance(const GncGUID* guid)
{
    init_();
    qof_instance_set_guid(this, guid);
}

QofInstance::~QofInstance()
{
    if (priv->collection != NULL)
    {
        qof_collection_remove_entity_upon_destruction(priv->collection, &priv->guid);
    }
    
    CACHE_REMOVE(e_type);
    e_type = NULL;

    kvp_frame_delete(kvp_data);    
    
    delete priv;
}

void
qof_instance_init_data (QofInstance *inst, QofIdType type, QofBook *book)
{
    QofInstancePrivate *priv;
    QofCollection *col;
    QofIdType col_type;

//    g_return_if_fail(QOF_IS_INSTANCE(inst));
    if(!inst) return;
    priv = GET_PRIVATE(inst);
    g_return_if_fail(!priv->book);

    priv->book = book;
    col = qof_book_get_collection (book, type);
    g_return_if_fail(col != NULL);

    /* XXX We passed redundant info to this routine ... but I think that's
     * OK, it might eliminate programming errors. */

    col_type = qof_collection_get_type(col);
    if (g_strcmp0(col_type, type))
    {
        PERR ("attempt to insert \"%s\" into \"%s\"", type, col_type);
        return;
    }
    priv = GET_PRIVATE(inst);
    inst->e_type = CACHE_INSERT (type);

    do
    {
        guid_new(&priv->guid);

        if (NULL == qof_collection_lookup_entity (col, &priv->guid))
            break;

        PWARN("duplicate id created, trying again");
    }
    while (1);

    priv->collection = col;

    qof_collection_insert_entity (col, inst);
}

//static void
//qof_instance_finalize_real (GObject *instp)
//{
//    QofInstancePrivate *priv;
//    QofInstance* inst = QOF_INSTANCE(instp);
//
//    kvp_frame_delete (inst->kvp_data);
//    inst->kvp_data = NULL;
//
//    priv = GET_PRIVATE(inst);
//    priv->editlevel = 0;
//    priv->do_free = FALSE;
//    priv->dirty = FALSE;
//}
//

const GncGUID *
qof_instance_get_guid (const void * inst)
{
    QofInstancePrivate *priv;

    if (!inst) return NULL;
//    g_return_val_if_fail(QOF_IS_INSTANCE(inst), guid_null());
    priv = GET_PRIVATE(inst);
    return &(priv->guid);
}

const GncGUID *
qof_entity_get_guid (const void * ent)
{
    return ent ? qof_instance_get_guid(ent) : guid_null();
}

void
qof_instance_set_guid (void * ptr, const GncGUID *guid)
{
    QofInstancePrivate *priv;
    QofInstance *inst;
    QofCollection *col;

//    g_return_if_fail(QOF_IS_INSTANCE(ptr));
    if(!ptr) return;

    inst = QOF_INSTANCE(ptr);
    priv = GET_PRIVATE(inst);
    if (guid_equal (guid, &priv->guid))
        return;

    col = priv->collection;
    qof_collection_remove_entity(inst);
    priv->guid = *guid;
    qof_collection_insert_entity(col, inst);
}

void
qof_instance_copy_guid (void * to, const void * from)
{
//    g_return_if_fail(QOF_IS_INSTANCE(to));
//    g_return_if_fail(QOF_IS_INSTANCE(from));
    if(!to) return;
    if(!from) return;

    GET_PRIVATE(to)->guid = GET_PRIVATE(from)->guid;
}

int
qof_instance_guid_compare(const void * ptr1, const void * ptr2)
{
    const QofInstancePrivate *priv1, *priv2;

//    g_return_val_if_fail(QOF_IS_INSTANCE(ptr1), -1);
//    g_return_val_if_fail(QOF_IS_INSTANCE(ptr2),  1);
    if(!ptr1) return -1;
    if(!ptr2) return 1;

    priv1 = GET_PRIVATE(ptr1);
    priv2 = GET_PRIVATE(ptr2);

    return guid_compare(&priv1->guid, &priv2->guid);
}

QofCollection *
qof_instance_get_collection (const void * ptr)
{

//    g_return_val_if_fail(QOF_IS_INSTANCE(ptr), NULL);
    if(!ptr) return NULL;
    return GET_PRIVATE(ptr)->collection;
}

void
qof_instance_set_collection (const void * ptr, QofCollection *col)
{
//    g_return_if_fail(QOF_IS_INSTANCE(ptr));
    if(!ptr) return;
    GET_PRIVATE(ptr)->collection = col;
}

QofBook *
qof_instance_get_book (const void * inst)
{
    if (!inst) return NULL;
//    g_return_val_if_fail(QOF_IS_INSTANCE(inst), NULL);
    return GET_PRIVATE(inst)->book;
}

void
qof_instance_set_book (const void * inst, QofBook *book)
{
//    g_return_if_fail(QOF_IS_INSTANCE(inst));
    if(!inst) return;
    GET_PRIVATE(inst)->book = book;
}

void
qof_instance_copy_book (void * ptr1, const void * ptr2)
{
//    g_return_if_fail(QOF_IS_INSTANCE(ptr1));
//    g_return_if_fail(QOF_IS_INSTANCE(ptr2));
    if(!ptr1) return;
    if(!ptr2) return;

    GET_PRIVATE(ptr1)->book = GET_PRIVATE(ptr2)->book;
}

bool
qof_instance_books_equal (const void * ptr1, const void * ptr2)
{
    const QofInstancePrivate *priv1, *priv2;

//    g_return_val_if_fail(QOF_IS_INSTANCE(ptr1), false);
//    g_return_val_if_fail(QOF_IS_INSTANCE(ptr2), false);
    if(!ptr1) return false;
    if(!ptr2) return false;

    priv1 = GET_PRIVATE(ptr1);
    priv2 = GET_PRIVATE(ptr2);

    return (priv1->book == priv2->book);
}

KvpFrame*
qof_instance_get_slots (const QofInstance *inst)
{
    if (!inst) return NULL;
    return inst->kvp_data;
}

void
qof_instance_set_slots (QofInstance *inst, KvpFrame *frm)
{
    QofInstancePrivate *priv;

    if (!inst) return;

    priv = GET_PRIVATE(inst);
    if (inst->kvp_data && (inst->kvp_data != frm))
    {
        kvp_frame_delete(inst->kvp_data);
    }

    priv->dirty = TRUE;
    inst->kvp_data = frm;
}

Timespec qof_instance_get_last_update(const QofInstance *inst)
{
    return GET_PRIVATE(inst)->last_update;
}

void
qof_instance_set_last_update (QofInstance *inst, Timespec ts)
{
    if (!inst) return;
    GET_PRIVATE(inst)->last_update = ts;
}

int
qof_instance_get_editlevel (const void * ptr)
{
//    g_return_val_if_fail(QOF_IS_INSTANCE(ptr), 0);
    if(!ptr) return 0;
    return GET_PRIVATE(ptr)->editlevel;
}

void qof_instance_increase_editlevel (void * ptr)
{
//    g_return_if_fail(QOF_IS_INSTANCE(ptr));
    if(!ptr) return;
    GET_PRIVATE(ptr)->editlevel++;
}

void qof_instance_decrease_editlevel (void * ptr)
{
//    g_return_if_fail(QOF_IS_INSTANCE(ptr));
    if(!ptr) return;
    GET_PRIVATE(ptr)->editlevel--;
}

void qof_instance_reset_editlevel (void * ptr)
{
//    g_return_if_fail(QOF_IS_INSTANCE(ptr));
    if(!ptr) return;
    GET_PRIVATE(ptr)->editlevel = 0;
}

int
qof_instance_version_cmp (const QofInstance *left, const QofInstance *right)
{
    QofInstancePrivate *lpriv, *rpriv;

    if (!left && !right) return 0;
    if (!left) return -1;
    if (!right) return +1;

    lpriv = GET_PRIVATE(left);
    rpriv = GET_PRIVATE(right);
    if (lpriv->last_update.tv_sec  < rpriv->last_update.tv_sec) return -1;
    if (lpriv->last_update.tv_sec  > rpriv->last_update.tv_sec) return +1;
    if (lpriv->last_update.tv_nsec < rpriv->last_update.tv_nsec) return -1;
    if (lpriv->last_update.tv_nsec > rpriv->last_update.tv_nsec) return +1;
    return 0;
}

bool
qof_instance_get_destroying (const void * ptr)
{
//    g_return_val_if_fail(QOF_IS_INSTANCE(ptr), FALSE);
    if(!ptr) return false;
    return GET_PRIVATE(ptr)->do_free;
}

void
qof_instance_set_destroying (QofInstance * ptr, bool value)
{
//    g_return_if_fail(QOF_IS_INSTANCE(ptr));
    if(!ptr) return;
    GET_PRIVATE(ptr)->do_free = value;
}

bool
qof_instance_get_dirty_flag (const void * ptr)
{
//    g_return_val_if_fail(QOF_IS_INSTANCE(ptr), FALSE);
    if(!ptr) return false;
    return GET_PRIVATE(ptr)->dirty;
}

void
qof_instance_set_dirty_flag (const void * inst, bool flag)
{
//    g_return_if_fail(QOF_IS_INSTANCE(inst));
    if(!inst) return;
    GET_PRIVATE(inst)->dirty = flag;
}

void
qof_instance_mark_clean (QofInstance *inst)
{
    if (!inst) return;
    GET_PRIVATE(inst)->dirty = FALSE;
}

void
qof_instance_print_dirty (const QofInstance *inst, void * dummy)
{
    QofInstancePrivate *priv;

    priv = GET_PRIVATE(inst);
    if (priv->dirty)
    {
        printf("%s instance %s is dirty.\n", inst->e_type,
               guid_to_string(&priv->guid));
    }
}

bool
qof_instance_get_dirty (QofInstance *inst)
{
    QofInstancePrivate *priv;
    QofCollection *coll;

    if (!inst)
    {
        return FALSE;
    }

    priv = GET_PRIVATE(inst);
    if (qof_get_alt_dirty_mode())
        return priv->dirty;
    coll = priv->collection;
    if (qof_collection_is_dirty(coll))
    {
        return priv->dirty;
    }
    priv->dirty = FALSE;
    return FALSE;
}

void
qof_instance_set_dirty(QofInstance* inst)
{
    QofInstancePrivate *priv;
    QofCollection *coll;

    priv = GET_PRIVATE(inst);
    priv->dirty = true;
    if (!qof_get_alt_dirty_mode())
    {
        coll = priv->collection;
        qof_collection_mark_dirty(coll);
    }
}

bool
qof_instance_get_infant(const QofInstance *inst)
{
//    g_return_val_if_fail(QOF_IS_INSTANCE(inst), FALSE);
    if(!inst) return false;
    return GET_PRIVATE(inst)->infant;
}

int32_t
qof_instance_get_version (const void * inst)
{
//    g_return_val_if_fail(QOF_IS_INSTANCE(inst), 0);
    if(!inst) return 0;
    return GET_PRIVATE(inst)->version;
}

void
qof_instance_set_version (void * inst, int32_t vers)
{
//    g_return_if_fail(QOF_IS_INSTANCE(inst));
    if(!inst) return;
    GET_PRIVATE(inst)->version = vers;
}

void
qof_instance_copy_version (void * to, const void * from)
{
//    g_return_if_fail(QOF_IS_INSTANCE(to));
//    g_return_if_fail(QOF_IS_INSTANCE(from));
    if(!to) return;
    if(!from) return;
    GET_PRIVATE(to)->version = GET_PRIVATE(from)->version;
}

uint32_t
qof_instance_get_version_check (const void * inst)
{
//    g_return_val_if_fail(QOF_IS_INSTANCE(inst), 0);
    if(!inst) return 0;
    return GET_PRIVATE(inst)->version_check;
}

void
qof_instance_set_version_check (void * inst, uint32_t value)
{
//    g_return_if_fail(QOF_IS_INSTANCE(inst));
    if(!inst) return;
    GET_PRIVATE(inst)->version_check = value;
}

void
qof_instance_copy_version_check (void * to, const void * from)
{
//    g_return_if_fail(QOF_IS_INSTANCE(to));
//    g_return_if_fail(QOF_IS_INSTANCE(from));
    if(!to) return;
    if(!from) return;
    GET_PRIVATE(to)->version_check = GET_PRIVATE(from)->version_check;
}

uint32_t qof_instance_get_idata (const void * inst)
{
    if (!inst)
    {
        return 0;
    }
//    g_return_val_if_fail(QOF_IS_INSTANCE(inst), 0);
    return GET_PRIVATE(inst)->idata;
}

void qof_instance_set_idata(void * inst, uint32_t idata)
{
    if (!inst)
    {
        return;
    }
    if (idata < 0)
    {
        return;
    }
//    g_return_if_fail(QOF_IS_INSTANCE(inst));
    GET_PRIVATE(inst)->idata = idata;
}

/* ========================================================== */

/* Returns a displayable name to represent this object */
const char* QofInstance::get_display_name() const
{
    g_return_val_if_fail( this != NULL, NULL );

    /* Not implemented - return default string */
    return g_strdup_printf("Object %s %p",
                           qof_collection_get_type(qof_instance_get_collection(this)),
                           this);
}
//
//struct GetReferringObjectHelperData
//{
//    const QofInstance* inst;
//    GList* list;
//};
//
//static void
//get_referring_object_instance_helper(QofInstance* inst, void * user_data)
//{
//    QofInstance** pInst = (QofInstance**)user_data;
//
//    if (*pInst == NULL)
//    {
//        *pInst = inst;
//    }
//}
//
//static void
//get_referring_object_helper(QofCollection* coll, void * user_data)
//{
//    QofInstance* first_instance = NULL;
//    GetReferringObjectHelperData* data = (GetReferringObjectHelperData*)user_data;
//
//    qof_collection_foreach(coll, get_referring_object_instance_helper, &first_instance);
//
//    if (first_instance != NULL)
//    {
//        GList* new_list = qof_instance_get_typed_referring_object_list(first_instance, data->inst);
//        data->list = g_list_concat(data->list, new_list);
//    }
//}
//
///* Returns a list of objects referring to this object */
//GList* qof_instance_get_referring_object_list(const QofInstance* inst)
//{
//    GetReferringObjectHelperData data;
//
//    g_return_val_if_fail( inst != NULL, NULL );
//
//    /* scan all collections */
//    data.inst = inst;
//    data.list = NULL;
//
//    qof_book_foreach_collection(qof_instance_get_book(inst),
//                                get_referring_object_helper,
//                                &data);
//    return data.list;
//}

//static void
//get_typed_referring_object_instance_helper(QofInstance* inst, void * user_data)
//{
//    GetReferringObjectHelperData* data = (GetReferringObjectHelperData*)user_data;
//
//    if (qof_instance_refers_to_object(inst, data->inst))
//    {
//        data->list = g_list_prepend(data->list, inst);
//    }
//}
//
//GList*
//qof_instance_get_referring_object_list_from_collection(const QofCollection* coll, const QofInstance* ref)
//{
//    GetReferringObjectHelperData data;
//
//    g_return_val_if_fail( coll != NULL, NULL );
//    g_return_val_if_fail( ref != NULL, NULL );
//
//    data.inst = ref;
//    data.list = NULL;
//
//    qof_collection_foreach(coll, get_typed_referring_object_instance_helper, &data);
//    return data.list;
//}
//
//GList*
//qof_instance_get_typed_referring_object_list(const QofInstance* inst, const QofInstance* ref)
//{
//    g_return_val_if_fail( inst != NULL, NULL );
//    g_return_val_if_fail( ref != NULL, NULL );
//
//    if ( QOF_INSTANCE_GET_CLASS(inst)->get_typed_referring_object_list != NULL )
//    {
//        return QOF_INSTANCE_GET_CLASS(inst)->get_typed_referring_object_list(inst, ref);
//    }
//    else
//    {
//        /* Not implemented - by default, loop through all objects of this object's type and check
//           them individually. */
//        QofCollection* coll;
//
//        coll = qof_instance_get_collection(inst);
//        return qof_instance_get_referring_object_list_from_collection(coll, ref);
//    }
//}
//
///* Check if this object refers to a specific object */
//bool qof_instance_refers_to_object(const QofInstance* inst, const QofInstance* ref)
//{
//    g_return_val_if_fail( inst != NULL, false );
//    g_return_val_if_fail( ref != NULL, false );
//
//    if ( QOF_INSTANCE_GET_CLASS(inst)->refers_to_object != NULL )
//    {
//        return QOF_INSTANCE_GET_CLASS(inst)->refers_to_object(inst, ref);
//    }
//    else
//    {
//        /* Not implemented - default = NO */
//        return false;
//    }
//}

/* =================================================================== */
/* Entity edit and commit utilities */
/* =================================================================== */

bool
qof_begin_edit (QofInstance *inst)
{
    QofInstancePrivate *priv;
    QofBackend * be;

    if (!inst) return false;

    priv = GET_PRIVATE(inst);
    priv->editlevel++;
    if (1 < priv->editlevel) return false;
    if (0 >= priv->editlevel)
        priv->editlevel = 1;

    be = qof_book_get_backend(priv->book);
    if (be && qof_backend_begin_exists(be))
        qof_backend_run_begin(be, inst);
    else
        priv->dirty = true;

    return true;
}

bool qof_commit_edit (QofInstance *inst)
{
    QofInstancePrivate *priv;

    if (!inst) return false;

    priv = GET_PRIVATE(inst);
    priv->editlevel--;
    if (0 < priv->editlevel) return false;

#if 0
    if ((0 == priv->editlevel) && priv->dirty)
    {
        QofBackend * be = qof_book_get_backend(priv->book);
        if (be && qof_backend_commit_exists(be))
        {
            qof_backend_run_commit(be, inst);
        }
    }
#endif
    if (0 > priv->editlevel)
    {
        PERR ("unbalanced call - resetting (was %d)", priv->editlevel);
        priv->editlevel = 0;
    }
    return true;
}

bool
qof_commit_edit_part2(QofInstance *inst,
                      void (*on_error)(QofInstance *, QofBackendError),
                      void (*on_done)(QofInstance *),
                      void (*on_free)(QofInstance *))
{
    QofInstancePrivate *priv;
    QofBackend * be;

    priv = GET_PRIVATE(inst);

    /* See if there's a backend.  If there is, invoke it. */
    be = qof_book_get_backend(priv->book);
    if (be && qof_backend_commit_exists(be))
    {
        QofBackendError errcode;

        /* clear errors */
        do
        {
            errcode = qof_backend_get_error(be);
        }
        while (ERR_BACKEND_NO_ERR != errcode);

        qof_backend_run_commit(be, inst);
        errcode = qof_backend_get_error(be);
        if (ERR_BACKEND_NO_ERR != errcode)
        {
            /* XXX Should perform a rollback here */
            priv->do_free = false;

            /* Push error back onto the stack */
            qof_backend_set_error (be, errcode);
            if (on_error)
                on_error(inst, errcode);
            return false;
        }
        /* XXX the backend commit code should clear dirty!! */
        priv->dirty = false;
    }
//    if (dirty && qof_get_alt_dirty_mode() &&
//        !(priv->infant && priv->do_free)) {
//      qof_collection_mark_dirty(priv->collection);
//      qof_book_mark_dirty(priv->book);
//    }
    priv->infant = false;

    if (priv->do_free)
    {
        if (on_free)
            on_free(inst);
        return true;
    }

    if (on_done)
        on_done(inst);
    return true;
}

/* ========================== END OF FILE ======================= */

