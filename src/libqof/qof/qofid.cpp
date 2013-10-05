/********************************************************************\
 * qofid.cpp -- QOF entity identifier implementation                *
 * Copyright (C) 2000 Dave Peticolas <dave@krondo.com>              *
 * Copyright (C) 2003 Linas Vepstas <linas@linas.org>               *
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

#include "config.h"

#include <string.h>
#include <glib.h>

#include "qof.h"
#include "qofid-p.h"

static QofLogModule log_module = QOF_MOD_ENGINE;
static bool qof_alt_dirty_mode = false;

class QofCollection
{
public:
    QofIdType    e_type;
    bool         is_dirty;

    GHashTable * hash_of_entities;
    void       * data;       /* place where object class can hang arbitrary data */
    
    QofCollection()
    {
        e_type = 0;
        is_dirty = false;
        hash_of_entities = guid_hash_table_new();
        data = NULL;
    }
    
    ~QofCollection()
    {
        g_hash_table_destroy(hash_of_entities);
        hash_of_entities = NULL;
    }
};

/* =============================================================== */

bool
qof_get_alt_dirty_mode (void)
{
    return qof_alt_dirty_mode;
}

void
qof_set_alt_dirty_mode (bool enabled)
{
    qof_alt_dirty_mode = enabled;
}

/* =============================================================== */

QofCollection *
qof_collection_new (QofIdType type)
{
    QofCollection *col = new QofCollection;
    col->e_type = CACHE_INSERT (type);
    col->data = NULL;
    return col;
}

void
qof_collection_destroy (QofCollection *col)
{
    CACHE_REMOVE (col->e_type);
    col->e_type = NULL;
    col->data = NULL;   /** XXX there should be a destroy notifier for this */
    delete col;
}

/* =============================================================== */
/* getters */

QofIdType
qof_collection_get_type (const QofCollection *col)
{
    return col->e_type;
}

/* =============================================================== */

void
qof_collection_remove_entity (QofInstance *ent)
{
    QofCollection *col;
    const GncGUID *guid;

    if (!ent) return;
    col = qof_instance_get_collection(ent);
    if (!col) return;
    guid = qof_instance_get_guid(ent);
    g_hash_table_remove (col->hash_of_entities, guid);
    if (!qof_alt_dirty_mode)
        qof_collection_mark_dirty(col);
    qof_instance_set_collection(ent, NULL);
}

void qof_collection_remove_entity_upon_destruction (QofCollection * col, GncGUID * guid)
{
    g_hash_table_remove (col->hash_of_entities, guid);
}

void
qof_collection_insert_entity (QofCollection *col, QofInstance *ent)
{
    const GncGUID *guid;

    if (!col || !ent) return;
    guid = qof_instance_get_guid(ent);
    if (guid_equal(guid, guid_null())) return;
    g_return_if_fail (col->e_type == ent->e_type);
    qof_collection_remove_entity (ent);
    g_hash_table_insert (col->hash_of_entities, (gpointer)guid, ent);
    if (!qof_alt_dirty_mode)
        qof_collection_mark_dirty(col);
    qof_instance_set_collection(ent, col);
}

bool
qof_collection_add_entity (QofCollection *coll, QofInstance *ent)
{
    QofInstance *e;
    const GncGUID *guid;

    e = NULL;
    if (!coll || !ent)
    {
        return false;
    }
    guid = qof_instance_get_guid(ent);
    if (guid_equal(guid, guid_null()))
    {
        return false;
    }
    g_return_val_if_fail (coll->e_type == ent->e_type, FALSE);
    e = qof_collection_lookup_entity(coll, guid);
    if ( e != NULL )
    {
        return false;
    }
    g_hash_table_insert (coll->hash_of_entities, (gpointer)guid, ent);
    if (!qof_alt_dirty_mode)
        qof_collection_mark_dirty(coll);
    return true;
}


static void
collection_compare_cb (QofInstance *ent, void * user_data)
{
    QofCollection *target;
    QofInstance *e;
    const GncGUID *guid;
    gint value;

    e = NULL;
    target = (QofCollection*)user_data;
    if (!target || !ent)
    {
        return;
    }
    value = *(gint*)qof_collection_get_data(target);
    if (value != 0)
    {
        return;
    }
    guid = qof_instance_get_guid(ent);
    if (guid_equal(guid, guid_null()))
    {
        value = -1;
        qof_collection_set_data(target, &value);
        return;
    }
    g_return_if_fail (target->e_type == ent->e_type);
    e = qof_collection_lookup_entity(target, guid);
    if ( e == NULL )
    {
        value = 1;
        qof_collection_set_data(target, &value);
        return;
    }
    value = 0;
    qof_collection_set_data(target, &value);
}

int
qof_collection_compare (QofCollection *target, QofCollection *merge)
{
    int value;

    value = 0;
    if (!target && !merge)
    {
        return 0;
    }
    if (target == merge)
    {
        return 0;
    }
    if (!target && merge)
    {
        return -1;
    }
    if (target && !merge)
    {
        return 1;
    }
    if (target->e_type != merge->e_type)
    {
        return -1;
    }
    qof_collection_set_data(target, &value);
    qof_collection_foreach(merge, collection_compare_cb, target);
    value = *(gint*)qof_collection_get_data(target);
    if (value == 0)
    {
        qof_collection_set_data(merge, &value);
        qof_collection_foreach(target, collection_compare_cb, merge);
        value = *(gint*)qof_collection_get_data(merge);
    }
    return value;
}

QofInstance *
qof_collection_lookup_entity (const QofCollection *col, const GncGUID * guid)
{
    QofInstance *ent;
    g_return_val_if_fail (col, NULL);
    if (guid == NULL) return NULL;
    ent = g_hash_table_lookup (col->hash_of_entities, guid);
    return ent;
}

QofCollection *
qof_collection_from_glist (QofIdType type, const GList *glist)
{
    QofCollection *coll;
    QofInstance *ent;
    const GList *list;

    coll = qof_collection_new(type);
    for (list = glist; list != NULL; list = list->next)
    {
        ent = QOF_INSTANCE(list->data);
        if (FALSE == qof_collection_add_entity(coll, ent))
        {
            return NULL;
        }
    }
    return coll;
}

unsigned int
qof_collection_count (const QofCollection *col)
{
    guint c;

    c = g_hash_table_size(col->hash_of_entities);
    return c;
}

/* =============================================================== */

bool
qof_collection_is_dirty (const QofCollection *col)
{
    return col ? col->is_dirty : false;
}

void
qof_collection_mark_clean (QofCollection *col)
{
    if (col)
    {
        col->is_dirty = false;
    }
}

void
qof_collection_mark_dirty (QofCollection *col)
{
    if (col)
    {
        col->is_dirty = true;
    }
}

void
qof_collection_print_dirty (const QofCollection *col, void * dummy)
{
    if (col->is_dirty)
        printf("%s collection is dirty.\n", col->e_type);
    qof_collection_foreach(col, (QofInstanceForeachCB)qof_instance_print_dirty, NULL);
}

/* =============================================================== */

void *
qof_collection_get_data (const QofCollection *col)
{
    return col ? col->data : NULL;
}

void
qof_collection_set_data (QofCollection *col, void * user_data)
{
    if (col)
    {
        col->data = user_data;
    }
}

/* =============================================================== */

struct _iterate
{
    QofInstanceForeachCB      fcn;
    void *                   data;
};

static void
foreach_cb (void * item, void * arg)
{
    struct _iterate *iter = arg;
    QofInstance *ent = item;

    iter->fcn (ent, iter->data);
}

void
qof_collection_foreach (const QofCollection *col, QofInstanceForeachCB cb_func,
                        void * user_data)
{
    struct _iterate iter;
    GList *entries;

    g_return_if_fail (col);
    g_return_if_fail (cb_func);

    iter.fcn = cb_func;
    iter.data = user_data;

    PINFO("Hash Table size of %s before is %d", col->e_type, g_hash_table_size(col->hash_of_entities));

    entries = g_hash_table_get_values (col->hash_of_entities);
    g_list_foreach (entries, foreach_cb, &iter);
    g_list_free (entries);

    PINFO("Hash Table size of %s after is %d", col->e_type, g_hash_table_size(col->hash_of_entities));
}
/* =============================================================== */
