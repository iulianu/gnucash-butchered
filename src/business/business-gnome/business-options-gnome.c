/*
 * business-options.c -- Initialize Business Options
 *
 * Written By: Derek Atkins <warlord@MIT.EDU>
 * Copyright (C) 2002 Derek Atkins
 */

#include "config.h"

#include <gnome.h>
#include <g-wrap-wct.h>
#include <libguile.h>

#include "gnc-ui-util.h"
#include "gnc-engine-util.h"
#include "option-util.h"
#include "gnc-general-search.h"

#include "dialog-options.h"
#include "business-options-gnome.h"
#include "business-gnome-utils.h"
#include "dialog-invoice.h"

static int
option_changed_cb (GtkWidget *widget, gpointer data)
{
  GNCOption *option = data;

  gnc_option_set_changed (option, TRUE);
  gnc_option_call_option_widget_changed_proc (option);
  gnc_options_dialog_changed_internal (widget);

  return FALSE;
}

static GtkWidget *
create_owner_widget (GNCOption *option, GncOwnerType type, GtkWidget *hbox)
{
  GtkWidget *widget;
  GncOwner owner;

  switch (type) {
  case GNC_OWNER_CUSTOMER:
    gncOwnerInitCustomer (&owner, NULL);
    break;
  case GNC_OWNER_VENDOR:
    gncOwnerInitVendor (&owner, NULL);
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

  gtk_signal_connect (GTK_OBJECT (widget), "changed", 
		      GTK_SIGNAL_FUNC (option_changed_cb), option);

  return widget;
}

static GtkWidget *
make_name_label (char *name)
{
  GtkWidget *label;
  gchar *colon_name;

  colon_name = g_strconcat (name, ":", NULL);
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
  SCM odata = gnc_option_get_option_data (option);
  SCM conv_func;

  conv_func = gh_eval_str ("gw:enum-<gnc:GncOwnerType>-val->int");
  odata = gh_call1 (conv_func, odata);

  return gh_scm2long (odata);
}


/* Function to set the UI widget based upon the option */
static GtkWidget *
owner_set_widget (GNCOption *option, GtkBox *page_box,
		  GtkTooltips *tooltips,
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

  gnc_option_set_ui_value (option, FALSE);

  gtk_widget_show_all (*enclosing);
  return value;
}

/* Function to set the UI Value for a particular option */
static gboolean
owner_set_value (GNCOption *option, gboolean use_default,
		 GtkWidget *widget, SCM value)
{
  GncOwner owner_def;
  GncOwner *owner;

  if (!gw_wcp_p (value))
    scm_misc_error("business_options:owner_set_value",
		   "Item is not a gw:wcp.", value);

  owner = gw_wcp_get_ptr (value);

  /* XXX: should we verify that the owner type is correct? */
  if (!owner) {
    owner_def.type = get_owner_type_from_option (option);
    owner_def.owner.undefined = NULL;
    owner = &owner_def;
  }

  widget = gnc_option_get_widget (option);
  gnc_owner_set_owner (widget, owner);
  return FALSE;
}

/* Function to get the UI Value for a particular option */
static SCM
owner_get_value (GNCOption *option, GtkWidget *widget)
{
  static GncOwner owner;	/* XXX: might cause trouble? */
  GncOwnerType type;

  type = get_owner_type_from_option (option);
  owner.type = type;
  gnc_owner_get_owner (widget, &owner);

  return gw_wcp_assimilate_ptr (&owner, gh_eval_str("<gnc:GncOwner*>"));
}


/********************************************************************/
/* "Customer" Option functions */


/* Function to set the UI widget based upon the option */
static GtkWidget *
customer_set_widget (GNCOption *option, GtkBox *page_box,
		     GtkTooltips *tooltips,
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

  gnc_option_set_ui_value (option, FALSE);

  gtk_widget_show_all (*enclosing);
  return value;
}

/* Function to set the UI Value for a particular option */
static gboolean
customer_set_value (GNCOption *option, gboolean use_default,
		    GtkWidget *widget, SCM value)
{
  GncOwner owner;
  GncCustomer *customer;

  if (!gw_wcp_p (value))
    scm_misc_error("business_options:customer_set_value",
		   "Item is not a gw:wcp.", value);

  customer = gw_wcp_get_ptr (value);
  gncOwnerInitCustomer (&owner, customer);

  widget = gnc_option_get_widget (option);
  gnc_owner_set_owner (widget, &owner);
  return FALSE;
}

/* Function to get the UI Value for a particular option */
static SCM
customer_get_value (GNCOption *option, GtkWidget *widget)
{
  GncOwner owner;

  gnc_owner_get_owner (widget, &owner);

  return gw_wcp_assimilate_ptr (owner.owner.undefined,
				gh_eval_str("<gnc:GncCustomer*>"));
}


/********************************************************************/
/* "Vendor" Option functions */


/* Function to set the UI widget based upon the option */
static GtkWidget *
vendor_set_widget (GNCOption *option, GtkBox *page_box,
		     GtkTooltips *tooltips,
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

  gnc_option_set_ui_value (option, FALSE);

  gtk_widget_show_all (*enclosing);
  return value;
}

/* Function to set the UI Value for a particular option */
static gboolean
vendor_set_value (GNCOption *option, gboolean use_default,
		    GtkWidget *widget, SCM value)
{
  GncOwner owner;
  GncVendor *vendor;

  if (!gw_wcp_p (value))
    scm_misc_error("business_options:vendor_set_value",
		   "Item is not a gw:wcp.", value);

  vendor = gw_wcp_get_ptr (value);
  gncOwnerInitVendor (&owner, vendor);

  widget = gnc_option_get_widget (option);
  gnc_owner_set_owner (widget, &owner);
  return FALSE;
}

/* Function to get the UI Value for a particular option */
static SCM
vendor_get_value (GNCOption *option, GtkWidget *widget)
{
  GncOwner owner;

  gnc_owner_get_owner (widget, &owner);

  return gw_wcp_assimilate_ptr (owner.owner.undefined,
				gh_eval_str("<gnc:GncVendor*>"));
}

/********************************************************************/
/* "Invoice" Option functions */


static GtkWidget *
create_invoice_widget (GNCOption *option, GtkWidget *hbox)
{
  GtkWidget *widget;

  widget = gnc_general_search_new (GNC_INVOICE_MODULE_NAME,
				   _("Select..."),
				   gnc_invoice_search_select,
				   gnc_get_current_book ());

  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  gnc_option_set_widget (option, widget);
  gtk_signal_connect (GTK_OBJECT (widget), "changed", 
		      GTK_SIGNAL_FUNC (option_changed_cb), option);

  return widget;
}

/* Function to set the UI widget based upon the option */
static GtkWidget *
invoice_set_widget (GNCOption *option, GtkBox *page_box,
		     GtkTooltips *tooltips,
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

  gnc_option_set_ui_value (option, FALSE);

  gtk_widget_show_all (*enclosing);
  return value;
}

/* Function to set the UI Value for a particular option */
static gboolean
invoice_set_value (GNCOption *option, gboolean use_default,
		    GtkWidget *widget, SCM value)
{
  GncInvoice *invoice;

  if (!gw_wcp_p (value))
    scm_misc_error("business_options:invoice_set_value",
		   "Item is not a gw:wcp.", value);

  invoice = gw_wcp_get_ptr (value);

  widget = gnc_option_get_widget (option);
  gnc_general_search_set_selected (GNC_GENERAL_SEARCH (widget), invoice);
  return FALSE;
}

/* Function to get the UI Value for a particular option */
static SCM
invoice_get_value (GNCOption *option, GtkWidget *widget)
{
  GncInvoice *invoice;

  invoice = gnc_general_search_get_selected (GNC_GENERAL_SEARCH (widget));
  return gw_wcp_assimilate_ptr (invoice, gh_eval_str("<gnc:GncInvoice*>"));
}


/********************************************************************/
/* "Tax Table" Option functions */


static GtkWidget *
create_taxtable_widget (GNCOption *option, GtkWidget *hbox)
{
  GtkWidget *widget;

  widget = gtk_option_menu_new ();

  gnc_ui_taxtables_optionmenu (widget, gnc_get_current_book (), TRUE, NULL);
  
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  gnc_option_set_widget (option, widget);

  gnc_ui_optionmenu_set_changed_callback (widget,
					  (void(*)(GtkWidget*,gpointer))option_changed_cb,
					  option);

  return widget;
}

/* Function to set the UI widget based upon the option */
static GtkWidget *
taxtable_set_widget (GNCOption *option, GtkBox *page_box,
		     GtkTooltips *tooltips,
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

  gnc_option_set_ui_value (option, FALSE);

  gtk_widget_show_all (*enclosing);
  return value;
}

/* Function to set the UI Value for a particular option */
static gboolean
taxtable_set_value (GNCOption *option, gboolean use_default,
		    GtkWidget *widget, SCM value)
{
  GncTaxTable *taxtable;

  if (!gw_wcp_p (value))
    scm_misc_error("business_options:taxtable_set_value",
		   "Item is not a gw:wcp.", value);

  taxtable = gw_wcp_get_ptr (value);

  widget = gnc_option_get_widget (option);
  gnc_ui_optionmenu_set_value (widget, taxtable);
  return FALSE;
}

/* Function to get the UI Value for a particular option */
static SCM
taxtable_get_value (GNCOption *option, GtkWidget *widget)
{
  GncTaxTable *taxtable;

  taxtable = gnc_ui_optionmenu_get_value (widget);
  return gw_wcp_assimilate_ptr (taxtable, gh_eval_str("<gnc:GncTaxTable*>"));
}




void
gnc_business_options_gnome_initialize (void)
{
  int i;
  static GNCOptionDef_t options[] = {
    { "owner", owner_set_widget, owner_set_value, owner_get_value },
    { "customer", customer_set_widget, customer_set_value,
      customer_get_value },
    { "vendor", vendor_set_widget, vendor_set_value, vendor_get_value },
    { "invoice", invoice_set_widget, invoice_set_value, invoice_get_value },
    { "taxtable", taxtable_set_widget, taxtable_set_value, taxtable_get_value },
    { NULL }
  };

  for (i = 0; options[i].option_name; i++)
    gnc_options_ui_register_option (&(options[i]));
}
