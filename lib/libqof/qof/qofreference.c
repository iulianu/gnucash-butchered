/***************************************************************************
 *            qofreference.c
 *
 *  Mon Feb 13 21:06:44 2006
 *  Copyright  2006  Neil Williams
 *  linux@codehelp.co.uk
 ****************************************************************************/
/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
 
#include "qofreference.h"

static void
entity_set_reference_cb(QofEntity *ent, gpointer user_data)
{
	void (*reference_setter) (QofEntity*, QofEntity*);
	void (*choice_setter) (QofEntity*, QofEntity*);
	void (*collect_setter)(QofEntity*, QofCollection*);
	QofEntityReference *ref;
	GList *book_ref_list;
	QofCollection *coll;
	QofIdType type;
	QofEntity *reference;
	QofBook *partial_book;
	
	partial_book = (QofBook*)user_data;
	g_return_if_fail(partial_book || ent);
	reference = NULL;
	coll = NULL;
	book_ref_list = qof_book_get_data(partial_book, ENTITYREFERENCE);
	while(book_ref_list)
	{
		ref = (QofEntityReference*)book_ref_list->data;
		if(0 == guid_compare(ref->ref_guid, qof_entity_get_guid(ent)))
		{
			/* avoid setting the entity's own guid as a reference. */
			book_ref_list = g_list_next(book_ref_list);
			continue;
		}
		if(qof_object_is_choice(ent->e_type)) { type = ref->choice_type; }
		type = ref->param->param_type;
		coll = qof_book_get_collection(partial_book, type);
		reference = qof_collection_lookup_entity(coll, ref->ref_guid);
		reference_setter = (void(*)(QofEntity*, QofEntity*))ref->param->param_setfcn;
		if((reference) && (reference_setter))
		{
			qof_begin_edit((QofInstance*)ent);
			qof_begin_edit((QofInstance*)reference);
			reference_setter(ent, reference);
			qof_commit_edit((QofInstance*)ent);
			qof_commit_edit((QofInstance*)reference);
		}
		/* collect and choice handling */
		collect_setter = (void(*)(QofEntity*, QofCollection*))ref->param->param_setfcn;
		choice_setter = (void(*)(QofEntity*, QofEntity*))ref->param->param_setfcn;
		if ((0 == safe_strcmp(ref->param->param_type, QOF_TYPE_COLLECT)) &&
			(0 == guid_compare(qof_entity_get_guid(ent), ref->ent_guid)) &&
			(0 == safe_strcmp(ref->type, ent->e_type)))
		{
			QofCollection *temp_col;
			char cm_sa[GUID_ENCODING_LENGTH + 1];
			
			temp_col = ref->param->param_getfcn(ent, ref->param);
			coll = qof_book_get_collection(partial_book, 
				qof_collection_get_type(temp_col));
			guid_to_string_buff(ref->ref_guid, cm_sa);
			reference = qof_collection_lookup_entity(coll, ref->ref_guid);
			if(reference) {
				qof_collection_add_entity(temp_col, reference);
				qof_begin_edit((QofInstance*)ent);
				qof_begin_edit((QofInstance*)reference);
				if(collect_setter) { collect_setter(ent, temp_col); }
				qof_commit_edit((QofInstance*)ent);
				qof_commit_edit((QofInstance*)reference);
				qof_collection_destroy(temp_col);
			}
		}
		if(0 == safe_strcmp(ref->param->param_type, QOF_TYPE_CHOICE))
		{
			coll = qof_book_get_collection(partial_book, ref->type);
			reference = qof_collection_lookup_entity(coll, ref->ref_guid);
			qof_begin_edit((QofInstance*)ent);
			qof_begin_edit((QofInstance*)reference);
			if(choice_setter) { choice_setter(ent, reference); }
			qof_commit_edit((QofInstance*)ent);
			qof_commit_edit((QofInstance*)reference);
		}
		book_ref_list = g_list_next(book_ref_list);
	}
}

static void
set_each_type(QofObject *obj, gpointer user_data)
{
	QofBook *book;

	book = (QofBook*)user_data;
	qof_object_foreach(obj->e_type, book, entity_set_reference_cb, book);
}

static QofEntityReference*
create_reference(QofEntity *ent, const QofParam *param)
{
	QofEntityReference *reference;
	QofEntity          *ref_ent;
	const GUID         *cm_guid;
	char                cm_sa[GUID_ENCODING_LENGTH + 1];
	gchar              *cm_string;

	ref_ent = (QofEntity*)param->param_getfcn(ent, param);
	if(!ref_ent) { return NULL; }
	reference = g_new0(QofEntityReference, 1);
	reference->type = ent->e_type;
	reference->ref_guid = g_new(GUID, 1);
	reference->ent_guid = &ent->guid;
	if(qof_object_is_choice(ent->e_type)) 
	{ 
		reference->choice_type = ref_ent->e_type; 
	}
	reference->param = param;
	cm_guid = qof_entity_get_guid(ref_ent);
	guid_to_string_buff(cm_guid, cm_sa);
	cm_string = g_strdup(cm_sa);
	if(TRUE == string_to_guid(cm_string, reference->ref_guid)) {
		g_free(cm_string);
		return reference;
	}
	g_free(cm_string);
	return NULL;
}

QofEntityReference*
qof_entity_get_reference_from(QofEntity *ent, const QofParam *param)
{
	g_return_val_if_fail(param, NULL);
	param = qof_class_get_parameter(ent->e_type, param->param_name);
	g_return_val_if_fail(0 != safe_strcmp(param->param_type, QOF_TYPE_COLLECT), NULL);
	return create_reference(ent, param);
}

void qof_book_set_references(QofBook *book)
{
	gboolean partial;

	partial =
	  (gboolean)GPOINTER_TO_INT(qof_book_get_data(book, PARTIAL_QOFBOOK));
	g_return_if_fail(partial);
	qof_object_foreach_type(set_each_type, book);
}
