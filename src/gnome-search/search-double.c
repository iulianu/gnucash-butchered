/*
 *  Copyright (C) 2002 Derek Atkins
 *
 *  Authors: Derek Atkins <warlord@MIT.EDU>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "gnc-amount-edit.h"
#include "QueryCore.h"

#include "search-double.h"

#define d(x)

static void editable_enters (GNCSearchCoreType *fe, GnomeDialog *dialog);
static void grab_focus (GNCSearchCoreType *fe);
static GNCSearchCoreType *gncs_clone(GNCSearchCoreType *fe);
static gboolean gncs_validate (GNCSearchCoreType *fe);
static GtkWidget *gncs_get_widget(GNCSearchCoreType *fe);
static QueryPredData_t gncs_get_predicate (GNCSearchCoreType *fe);

static void gnc_search_double_class_init	(GNCSearchDoubleClass *class);
static void gnc_search_double_init	(GNCSearchDouble *gspaper);
static void gnc_search_double_finalise	(GtkObject *obj);

#define _PRIVATE(x) (((GNCSearchDouble *)(x))->priv)

struct _GNCSearchDoublePrivate {
  GtkWidget * entry;
};

static GNCSearchCoreTypeClass *parent_class;

enum {
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
gnc_search_double_get_type (void)
{
  static guint type = 0;
	
  if (!type) {
    GtkTypeInfo type_info = {
      "GNCSearchDouble",
      sizeof(GNCSearchDouble),
      sizeof(GNCSearchDoubleClass),
      (GtkClassInitFunc)gnc_search_double_class_init,
      (GtkObjectInitFunc)gnc_search_double_init,
      (GtkArgSetFunc)NULL,
      (GtkArgGetFunc)NULL
    };
		
    type = gtk_type_unique(gnc_search_core_type_get_type (), &type_info);
  }
	
  return type;
}

static void
gnc_search_double_class_init (GNCSearchDoubleClass *class)
{
  GtkObjectClass *object_class;
  GNCSearchCoreTypeClass *gnc_search_core_type = (GNCSearchCoreTypeClass *)class;

  object_class = (GtkObjectClass *)class;
  parent_class = gtk_type_class(gnc_search_core_type_get_type ());

  object_class->finalize = gnc_search_double_finalise;

  /* override methods */
  gnc_search_core_type->editable_enters = editable_enters;
  gnc_search_core_type->grab_focus = grab_focus;
  gnc_search_core_type->validate = gncs_validate;
  gnc_search_core_type->get_widget = gncs_get_widget;
  gnc_search_core_type->get_predicate = gncs_get_predicate;
  gnc_search_core_type->clone = gncs_clone;

  /* signals */

  gtk_object_class_add_signals(object_class, signals, LAST_SIGNAL);
}

static void
gnc_search_double_init (GNCSearchDouble *o)
{
  o->priv = g_malloc0 (sizeof (*o->priv));
  o->how = COMPARE_LT;
}

static void
gnc_search_double_finalise (GtkObject *obj)
{
  GNCSearchDouble *o = (GNCSearchDouble *)obj;
  g_assert (IS_GNCSEARCH_DOUBLE (o));

  g_free(o->priv);
	
  ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * gnc_search_double_new:
 *
 * Create a new GNCSearchDouble object.
 * 
 * Return value: A new #GNCSearchDouble object.
 **/
GNCSearchDouble *
gnc_search_double_new (void)
{
  GNCSearchDouble *o = (GNCSearchDouble *)gtk_type_new(gnc_search_double_get_type ());
  return o;
}

void
gnc_search_double_set_value (GNCSearchDouble *fi, double value)
{
  g_return_if_fail (fi);
  g_return_if_fail (IS_GNCSEARCH_DOUBLE (fi));
	
  fi->value = value;
}

void
gnc_search_double_set_how (GNCSearchDouble *fi, query_compare_t how)
{
  g_return_if_fail (fi);
  g_return_if_fail (IS_GNCSEARCH_DOUBLE (fi));
  fi->how = how;
}

static gboolean
gncs_validate (GNCSearchCoreType *fe)
{
  GNCSearchDouble *fi = (GNCSearchDouble *)fe;
  gboolean valid = TRUE;

  g_return_val_if_fail (fi, FALSE);
  g_return_val_if_fail (IS_GNCSEARCH_DOUBLE (fi), FALSE);
	
  /* XXX */

  return valid;
}

static void
option_changed (GtkWidget *widget, GNCSearchDouble *fe)
{
  fe->how = (query_compare_t)
    gtk_object_get_data (GTK_OBJECT (widget), "option");
}

static void
entry_changed (GNCAmountEdit *entry, GNCSearchDouble *fe)
{
  fe->value = gnc_amount_edit_get_damount (entry);
}

static GtkWidget *
add_menu_item (GtkWidget *menu, gpointer user_data, char *label,
	       query_compare_t option)
{
  GtkWidget *item = gtk_menu_item_new_with_label (label);
  gtk_object_set_data (GTK_OBJECT (item), "option", (gpointer) option);
  gtk_signal_connect (GTK_OBJECT (item), "activate", option_changed, user_data);
  gtk_menu_append (GTK_MENU (menu), item);
  gtk_widget_show (item);
  return item;
}

#define ADD_MENU_ITEM(str,op) { \
	item = add_menu_item (menu, fe, str, op); \
	if (fi->how == op) { current = index; first = item; } \
	index++; \
} 

static GtkWidget *
make_menu (GNCSearchCoreType *fe)
{
  GNCSearchDouble *fi = (GNCSearchDouble *)fe;
  GtkWidget *menu, *item, *first, *opmenu;
  int current = 0, index = 0;

  menu = gtk_menu_new ();

  ADD_MENU_ITEM (_("is less than"), COMPARE_LT);
  first = item;			/* Force one */
  ADD_MENU_ITEM (_("is less than or equal to"), COMPARE_LTE);
  ADD_MENU_ITEM (_("equals"), COMPARE_EQUAL);
  ADD_MENU_ITEM (_("does not equal"), COMPARE_NEQ);
  ADD_MENU_ITEM (_("is greater than"), COMPARE_GT);
  ADD_MENU_ITEM (_("is greater than or equal to"), COMPARE_GTE);

  opmenu = gtk_option_menu_new ();
  gtk_option_menu_set_menu (GTK_OPTION_MENU (opmenu), menu);

  gtk_signal_emit_by_name (GTK_OBJECT (first), "activate", fe);
  gtk_option_menu_set_history (GTK_OPTION_MENU (opmenu), current);

  return opmenu;
}

static void
grab_focus (GNCSearchCoreType *fe)
{
  GNCSearchDouble *fi = (GNCSearchDouble *)fe;

  g_return_if_fail (fi);
  g_return_if_fail (IS_GNCSEARCH_DOUBLE (fi));

  if (fi->priv->entry)
    gtk_widget_grab_focus (fi->priv->entry);
}

static void
editable_enters (GNCSearchCoreType *fe, GnomeDialog *dialog)
{
  GNCSearchDouble *fi = (GNCSearchDouble *)fe;

  g_return_if_fail (fi);
  g_return_if_fail (IS_GNCSEARCH_DOUBLE (fi));
  g_return_if_fail (dialog);

  if (fi->priv->entry)
    gnome_dialog_editable_enters (dialog, GTK_EDITABLE (fi->priv->entry));
}

static GtkWidget *
gncs_get_widget (GNCSearchCoreType *fe)
{
  GtkWidget *entry, *menu, *box;
  GNCSearchDouble *fi = (GNCSearchDouble *)fe;
	
  g_return_val_if_fail (fi, NULL);
  g_return_val_if_fail (IS_GNCSEARCH_DOUBLE (fi), NULL);

  box = gtk_hbox_new (FALSE, 3);

  /* Build and connect the option menu */
  menu = make_menu (fe);
  gtk_box_pack_start (GTK_BOX (box), menu, FALSE, FALSE, 3);

  /* Build and connect the entry window */
  entry = gnc_amount_edit_new ();
  if (fi->value)
    gnc_amount_edit_set_damount (GNC_AMOUNT_EDIT (entry), fi->value);
  gtk_signal_connect (GTK_OBJECT (entry), "amount_changed", entry_changed, fe);
  gtk_box_pack_start (GTK_BOX (box), entry, FALSE, FALSE, 3);
  fi->priv->entry = gnc_amount_edit_gtk_entry (GNC_AMOUNT_EDIT (entry));

  /* And return the box */
  return box;
}

static QueryPredData_t gncs_get_predicate (GNCSearchCoreType *fe)
{
  GNCSearchDouble *fi = (GNCSearchDouble *)fe;

  g_return_val_if_fail (fi, NULL);
  g_return_val_if_fail (IS_GNCSEARCH_DOUBLE (fi), NULL);

  return gncQueryDoublePredicate (fi->how, fi->value);
}

static GNCSearchCoreType *gncs_clone(GNCSearchCoreType *fe)
{
  GNCSearchDouble *se, *fse = (GNCSearchDouble *)fe;

  g_return_val_if_fail (fse, NULL);
  g_return_val_if_fail (IS_GNCSEARCH_DOUBLE (fse), NULL);

  se = gnc_search_double_new ();
  gnc_search_double_set_value (se, fse->value);
  gnc_search_double_set_how (se, fse->how);

  return (GNCSearchCoreType *)se;
}
