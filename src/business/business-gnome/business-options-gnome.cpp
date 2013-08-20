/*
 * business-options.cpp -- Initialize Business Options
 *
 * Written By: Derek Atkins <warlord@MIT.EDU>
 * Copyright (C) 2002,2006 Derek Atkins
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact:
 *
 * Free Software Foundation           Voice:  +1-617-542-5942
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652
 * Boston, MA  02110-1301,  USA       gnu@gnu.org
 */

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gnc-ui-util.h"
#include "dialog-utils.h"
#include "qof.h"
#include "option-util.h"
#include "gnc-general-search.h"

#include "dialog-options.h"
#include "business-options-gnome.h"
#include "business-gnome-utils.h"
#include "dialog-invoice.h"

#define FUNC_NAME G_STRFUNC

static GtkWidget *
create_owner_widget (GNCOption *option, GncOwnerType type, GtkWidget *hbox)
{
    GtkWidget *widget;
    GncOwner owner;

    switch (type)
    {
    case GNC_OWNER_CUSTOMER:
        gncOwnerInitCustomer (&owner, NULL);
        break;
    case GNC_OWNER_VENDOR:
        gncOwnerInitVendor (&owner, NULL);
        break;
    case GNC_OWNER_EMPLOYEE:
        gncOwnerInitEmployee (&owner, NULL);
        break;
    case GNC_OWNER_JOB:
        gncOwnerInitJob (&owner, NULL);
        break;
    default:
        return NULL;
    }

    widget = gnc_owner_select_create (NULL, hbox,
                                      gnc_get_current_book (), &owner);
    gnc_option_set_widget (option, widget);

    g_signal_connect (G_OBJECT (widget), "changed",
                      G_CALLBACK (gnc_option_changed_option_cb), option);

    return widget;
}

static GtkWidget *
make_name_label (char *name)
{
    GtkWidget *label;
    gchar *colon_name;

    colon_name = g_strconcat (name, ":", (char *)NULL);
    label = gtk_label_new (colon_name);
    gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
    g_free (colon_name);

    return label;
}

/********************************************************************/
/* "Owner" Option functions */


static GncOwnerType
get_owner_type_from_option (GNCOption *option)
{
    return GNC_OWNER_UNDEFINED;
//    SCM odata = gnc_option_get_option_data (option);
//
//    /* The option data is enum-typed.  It's just the enum value. */
//    return (GncOwnerType) scm_to_int(odata);
}


/* Function to set the UI widget based upon the option */
static GtkWidget *
owner_set_widget (GNCOption *option, GtkBox *page_box,
                  char *name, char *documentation,
                  /* Return values */
                  GtkWidget **enclosing, gboolean *packed)
{
    GtkWidget *value;
    GtkWidget *label;

    *enclosing = gtk_hbox_new (FALSE, 5);
    label = make_name_label (name);
    gtk_box_pack_start (GTK_BOX (*enclosing), label, FALSE, FALSE, 0);

    value = create_owner_widget (option, get_owner_type_from_option (option),
                                 *enclosing);

//    gnc_option_set_ui_value (option, FALSE);

    gtk_widget_show_all (*enclosing);
    return value;
}

/********************************************************************/
/* "Customer" Option functions */


/* Function to set the UI widget based upon the option */
static GtkWidget *
customer_set_widget (GNCOption *option, GtkBox *page_box,
                     char *name, char *documentation,
                     /* Return values */
                     GtkWidget **enclosing, gboolean *packed)
{
    GtkWidget *value;
    GtkWidget *label;

    *enclosing = gtk_hbox_new (FALSE, 5);
    label = make_name_label (name);
    gtk_box_pack_start (GTK_BOX (*enclosing), label, FALSE, FALSE, 0);

    value = create_owner_widget (option, GNC_OWNER_CUSTOMER, *enclosing);

//    gnc_option_set_ui_value (option, FALSE);

    gtk_widget_show_all (*enclosing);
    return value;
}



/********************************************************************/
/* "Vendor" Option functions */


/* Function to set the UI widget based upon the option */
static GtkWidget *
vendor_set_widget (GNCOption *option, GtkBox *page_box,
                   char *name, char *documentation,
                   /* Return values */
                   GtkWidget **enclosing, gboolean *packed)
{
    GtkWidget *value;
    GtkWidget *label;

    *enclosing = gtk_hbox_new (FALSE, 5);
    label = make_name_label (name);
    gtk_box_pack_start (GTK_BOX (*enclosing), label, FALSE, FALSE, 0);

    value = create_owner_widget (option, GNC_OWNER_VENDOR, *enclosing);

//    gnc_option_set_ui_value (option, FALSE);

    gtk_widget_show_all (*enclosing);
    return value;
}

/********************************************************************/
/* "Employee" Option functions */


/* Function to set the UI widget based upon the option */
static GtkWidget *
employee_set_widget (GNCOption *option, GtkBox *page_box,
                     char *name, char *documentation,
                     /* Return values */
                     GtkWidget **enclosing, gboolean *packed)
{
    GtkWidget *value;
    GtkWidget *label;

    *enclosing = gtk_hbox_new (FALSE, 5);
    label = make_name_label (name);
    gtk_box_pack_start (GTK_BOX (*enclosing), label, FALSE, FALSE, 0);

    value = create_owner_widget (option, GNC_OWNER_EMPLOYEE, *enclosing);

//    gnc_option_set_ui_value (option, FALSE);

    gtk_widget_show_all (*enclosing);
    return value;
}


/********************************************************************/
/* "Invoice" Option functions */


static GtkWidget *
create_invoice_widget (GNCOption *option, GtkWidget *hbox)
{
    GtkWidget *widget;

    /* No owner or starting invoice here, but that's okay. */
    widget = gnc_invoice_select_create (hbox, gnc_get_current_book(),
                                        NULL, NULL, NULL);

    gnc_option_set_widget (option, widget);
    g_signal_connect (G_OBJECT (widget), "changed",
                      G_CALLBACK (gnc_option_changed_option_cb), option);

    return widget;
}

/* Function to set the UI widget based upon the option */
static GtkWidget *
invoice_set_widget (GNCOption *option, GtkBox *page_box,
                    char *name, char *documentation,
                    /* Return values */
                    GtkWidget **enclosing, gboolean *packed)
{
    GtkWidget *value;
    GtkWidget *label;

    *enclosing = gtk_hbox_new (FALSE, 5);
    label = make_name_label (name);
    gtk_box_pack_start (GTK_BOX (*enclosing), label, FALSE, FALSE, 0);

    value = create_invoice_widget (option, *enclosing);

//    gnc_option_set_ui_value (option, FALSE);

    gtk_widget_show_all (*enclosing);
    return value;
}


/********************************************************************/
/* "Tax Table" Option functions */


static GtkWidget *
create_taxtable_widget (GNCOption *option, GtkWidget *hbox)
{
    GtkWidget *widget;
    GtkBuilder *builder;

    builder = gtk_builder_new();
    gnc_builder_add_from_file (builder, "business-options-gnome.glade", "taxtable_store");
    gnc_builder_add_from_file (builder, "business-options-gnome.glade", "taxtable_menu");

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "taxtable_menu"));
    gnc_taxtables_combo (GTK_COMBO_BOX(widget), gnc_get_current_book (), TRUE, NULL);
    gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

    gnc_option_set_widget (option, widget);

    g_signal_connect (widget, "changed",
                      G_CALLBACK (gnc_option_changed_option_cb), option);

    g_object_unref(G_OBJECT(builder));
    return widget;
}

/* Function to set the UI widget based upon the option */
static GtkWidget *
taxtable_set_widget (GNCOption *option, GtkBox *page_box,
                     char *name, char *documentation,
                     /* Return values */
                     GtkWidget **enclosing, gboolean *packed)
{
    GtkWidget *value;
    GtkWidget *label;

    *enclosing = gtk_hbox_new (FALSE, 5);
    label = make_name_label (name);
    gtk_box_pack_start (GTK_BOX (*enclosing), label, FALSE, FALSE, 0);

    value = create_taxtable_widget (option, *enclosing);

//    gnc_option_set_ui_value (option, FALSE);

    gtk_widget_show_all (*enclosing);
    return value;
}


void
gnc_business_options_gnome_initialize (void)
{
    int i;
    static GNCOptionDef_t options[] =
    {
        { "owner", owner_set_widget  },
        {
            "customer", customer_set_widget
        },
        { "vendor", vendor_set_widget },
        { "employee", employee_set_widget },
        { "invoice", invoice_set_widget },
        { "taxtable", taxtable_set_widget },
        { NULL }
    };

    for (i = 0; options[i].option_name; i++)
        gnc_options_ui_register_option (&(options[i]));
}
