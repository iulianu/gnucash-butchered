/*
 * dialog-invoice.c -- Dialog for Invoice entry
 * Copyright (C) 2001,2002 Derek Atkins
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#include "config.h"

#include <gnome.h>
#include <g-wrap-wct.h>
#include <libguile.h>

#include "dialog-utils.h"
#include "global-options.h"
#include "gnc-component-manager.h"
#include "gnc-ui.h"
#include "gnc-gui-query.h"
#include "gnc-ui-util.h"
#include "gnc-engine-util.h"
#include "gnc-date-edit.h"
#include "gnucash-sheet.h"
#include "window-help.h"
#include "window-report.h"
#include "dialog-search.h"
#include "search-param.h"

#include "gncObject.h"
#include "gncInvoice.h"
#include "gncInvoiceP.h"

#include "gncEntryLedger.h"

#include "gnc-general-search.h"
#include "dialog-date-close.h"
#include "dialog-invoice.h"
#include "dialog-job.h"
#include "business-utils.h"
#include "dialog-payment.h"

#define DIALOG_NEW_INVOICE_CM_CLASS "dialog-new-invoice"
#define DIALOG_VIEW_INVOICE_CM_CLASS "dialog-view-invoice"

typedef enum
{
  NEW_INVOICE,
  MOD_INVOICE,
  EDIT_INVOICE,
  VIEW_INVOICE
} InvoiceDialogType;

struct _invoice_select_window {
  GNCBook *	book;
  GncOwner *	owner;
  QueryNew *	q;
  GncOwner	owner_def;
};

struct _invoice_window {
  GladeXML *	xml;

  GtkWidget *	dialog;

  GtkWidget *	menubar_dock;
  GtkWidget *	menubar;
  GtkWidget *	toolbar_dock;
  GtkWidget *	toolbar;
  GtkWidget *	statusbar;
  GtkWidget *	edit_button;
  GtkWidget *	print_button;
  GtkWidget *	post_button;

  GtkWidget *	id_entry;
  GtkWidget *	notes_text;
  GtkWidget *	opened_date;
  GtkWidget *	posted_date_hbox;
  GtkWidget *	posted_date;
  GtkWidget *	active_check;

  GtkWidget *	owner_box;
  GtkWidget *	owner_label;
  GtkWidget *	owner_choice;
  GtkWidget *	job_box;
  GtkWidget *	job_choice;
  GtkWidget *	billing_id_entry;
  GtkWidget *	terms_entry;

  gint		width;

  GnucashRegister *	reg;
  GncEntryLedger *	ledger;

  InvoiceDialogType	dialog_type;
  GUID		invoice_guid;
  gint		component_id;
  GNCBook *	book;
  GncInvoice *	created_invoice;
  GncOwner	owner;
  GncOwner	job;

};

#define WIDTH_PREFIX "invoice_reg"
static int last_width = 0;

static void gnc_invoice_update_window (InvoiceWindow *iw);
static InvoiceWindow * gnc_ui_invoice_modify (GncInvoice *invoice);

static GncInvoice *
iw_get_invoice (InvoiceWindow *iw)
{
  if (!iw)
    return NULL;

  return gncInvoiceLookup (iw->book, &iw->invoice_guid);
}

static void gnc_ui_to_invoice (InvoiceWindow *iw, GncInvoice *invoice)
{
  Timespec ts;

  if (iw->dialog_type == VIEW_INVOICE)
    return;

  gnc_suspend_gui_refresh ();
  
  if (iw->active_check)
    gncInvoiceSetActive (invoice, gtk_toggle_button_get_active
			 (GTK_TOGGLE_BUTTON (iw->active_check)));

  gncInvoiceSetNotes (invoice, gtk_editable_get_chars
		      (GTK_EDITABLE (iw->notes_text), 0, -1));

  /* Only set these values for NEW/MOD INVOICE types */
  if (iw->dialog_type != EDIT_INVOICE) {
    gncInvoiceSetID (invoice, gtk_editable_get_chars
		     (GTK_EDITABLE (iw->id_entry), 0, -1));
    gncInvoiceSetBillingID (invoice, gtk_editable_get_chars
			    (GTK_EDITABLE (iw->billing_id_entry), 0, -1));
    gncInvoiceSetTerms (invoice, gtk_editable_get_chars
			(GTK_EDITABLE (iw->terms_entry), 0, -1));

    ts = gnc_date_edit_get_date_ts (GNC_DATE_EDIT (iw->opened_date));
    gncInvoiceSetDateOpened (invoice, ts);

    gnc_owner_get_owner (iw->owner_choice, &(iw->owner));
    if (iw->job_choice)
      gnc_owner_get_owner (iw->job_choice, &(iw->job));

    /* Only set the job if we've actually got one */
    if (gncOwnerGetJob (&(iw->job)))
      gncInvoiceSetOwner (invoice, &(iw->job));
    else
      gncInvoiceSetOwner (invoice, &(iw->owner));
  }

  gncInvoiceCommitEdit (invoice);
  gnc_resume_gui_refresh ();
}

static gboolean
gnc_invoice_window_verify_ok (InvoiceWindow *iw)
{
  const char *res;

  /* save the current entry in the ledger? */
  if (!gnc_entry_ledger_check_close (iw->dialog, iw->ledger))
    return FALSE;

  /* Check the Owner */
  gnc_owner_get_owner (iw->owner_choice, &(iw->owner));
  res = gncOwnerGetName (&(iw->owner));
  if (res == NULL || safe_strcmp (res, "") == 0) {
    gnc_error_dialog_parented (GTK_WINDOW (iw->dialog),
  			       _("You need to supply Billing Information."));
    return FALSE;
  }

  /* Check the ID; set one if necessary */
  res = gtk_entry_get_text (GTK_ENTRY (iw->id_entry));
  if (safe_strcmp (res, "") == 0) {
  gtk_entry_set_text (GTK_ENTRY (iw->id_entry),
		      g_strdup_printf ("%.6lld", gncInvoiceNextID(iw->book)));
  }
      
  return TRUE;
}

static gboolean
gnc_invoice_window_ok_save (InvoiceWindow *iw)
{
  if (!gnc_invoice_window_verify_ok (iw))
    return FALSE;

  {
    GncInvoice *invoice = iw_get_invoice (iw);
    if (invoice) {
      gnc_ui_to_invoice (iw, invoice);
    }
    /* Save the invoice to return it later. */
    iw->created_invoice = invoice;
  }
  return TRUE;
}

static void
gnc_invoice_window_ok_cb (GtkWidget *widget, gpointer data)
{
  InvoiceWindow *iw = data;

  if (!gnc_invoice_window_ok_save (iw))
    return;

  /* Ok, we don't need this anymore */
  iw->invoice_guid = *xaccGUIDNULL ();

  /* if this is a NEW_INVOICE, and created_invoice is NON-NULL, the
   * open up a new window with the invoice.  This used to be done
   * in gnc_ui_invoice_new() but cannot be done anymore
   */
  if (iw->dialog_type == NEW_INVOICE && iw->created_invoice)
    gnc_ui_invoice_edit (iw->created_invoice);

  gnc_close_gui_component (iw->component_id);
}

static void
gnc_invoice_window_cancel_cb (GtkWidget *widget, gpointer data)
{
  InvoiceWindow *iw = data;

  gnc_close_gui_component (iw->component_id);
}

static void
gnc_invoice_window_help_cb (GtkWidget *widget, gpointer data)
{
  char *help_file = "";		/* XXX: FIXME */

  helpWindow(NULL, NULL, help_file);
}

static void
gnc_invoice_window_print_invoice_cb (GtkWidget *widget, gpointer data)
{
  InvoiceWindow *iw = data;
  GncInvoice *invoice = iw_get_invoice (iw);
  SCM func, arg;
  SCM args = SCM_EOL;
  int report_id;

  g_return_if_fail (invoice);

  func = gh_eval_str ("gnc:invoice-report-create");
  g_return_if_fail (gh_procedure_p (func));

  arg = gw_wcp_assimilate_ptr (invoice, gh_eval_str("<gnc:GncInvoice*>"));
  args = gh_cons (arg, args);

  /* scm_protect_object(func); */

  arg = gh_apply (func, args);
  g_return_if_fail (gh_exact_p (arg));
  report_id = gh_scm2int (arg);

  /* scm_unprotect_object(func); */
  if (report_id >= 0)
    reportWindow (report_id);
}

static void
gnc_invoice_window_post_invoice_cb (GtkWidget *widget, gpointer data)
{
  InvoiceWindow *iw = data;
  GncInvoice *invoice;
  char *message, *memo, *ddue_label, *post_label, *acct_label;
  Account *acc = NULL;
  GList * acct_types = NULL;
  Timespec ddue, postdate;

  /* Make sure the invoice is ok */
  if (!gnc_invoice_window_verify_ok (iw))
      return;

  invoice = iw_get_invoice (iw);
  if (!invoice)
    return;

  /* Check that there is at least one Entry */
  invoice = iw_get_invoice (iw);
  if (gncInvoiceGetEntries (invoice) == NULL) {
    gnc_error_dialog_parented (GTK_WINDOW (iw->dialog),
			       _("The Invoice must have at least one Entry."));
    return;
  }

  /* Ok, we can post this invoice.  Ask for verification, set the due date,
   * post date, and posted account
   */
  message = _("Do you really want to post the invoice?");
  ddue_label = _("Due Date");
  post_label = _("Post Date");
  acct_label = _("Post to Account");

  /* Determine the type of account to post to */
  acct_types = gnc_business_account_types (&(iw->owner));

  /* Get the due date and posted account */
  timespecFromTime_t (&postdate, time(NULL));
  ddue = postdate;
  ddue.tv_sec += 3600*24*30;	/* XXX: due in 30 days */
  memo = NULL;

  if (!gnc_dialog_dates_acct_parented (iw->dialog, message, ddue_label,
				      post_label, acct_label, TRUE, acct_types,
				      iw->book, &ddue, &postdate, &memo, &acc))
    return;

  /* Yep, we're posting.  So, save the invoice... 
   * Note that we can safely ignore the return value; we checked
   * the verify_ok earlier, so we know it's ok.
   */
  gnc_suspend_gui_refresh ();
  gnc_invoice_window_ok_save (iw);

  /* ... post it; post date is set to now ... */
  gncInvoicePostToAccount (invoice, acc, &postdate, &ddue, memo);
  gnc_resume_gui_refresh ();

  if (memo)
    g_free (memo);

  /* Reset the type; change to read-only! */
  iw->dialog_type = VIEW_INVOICE;
  gnc_entry_ledger_set_readonly (iw->ledger);

  /* ... and redisplay here. */
  gnc_invoice_update_window (iw);
  gnc_table_refresh_gui (gnc_entry_ledger_get_table (iw->ledger), TRUE);
}

static void
gnc_invoice_window_destroy_cb (GtkWidget *widget, gpointer data)
{
  InvoiceWindow *iw = data;
  GncInvoice *invoice = iw_get_invoice (iw);

  gnc_suspend_gui_refresh ();

  if (iw->dialog_type == NEW_INVOICE && invoice != NULL) {
    gncInvoiceDestroy (invoice);
    iw->invoice_guid = *xaccGUIDNULL ();
  }

  gnc_entry_ledger_destroy (iw->ledger);
  gnc_unregister_gui_component (iw->component_id);
  gnc_resume_gui_refresh ();

  g_free (iw);
}

static void
recordCB (GtkWidget *widget, gpointer data)
{
  InvoiceWindow *iw = data;

  if (!iw || !iw->ledger)
    return;

  if (!gnc_entry_ledger_commit_entry (iw->ledger))
    return;

  gnucash_register_goto_next_virt_row (iw->reg);
}

static void
cancelCB (GtkWidget *widget, gpointer data)
{
  InvoiceWindow *iw = data;

  if (!iw || !iw->ledger)
    return;

  gnc_entry_ledger_cancel_cursor_changes (iw->ledger);
}

static void
deleteCB (GtkWidget *widget, gpointer data)
{
  InvoiceWindow *iw = data;
  GncEntry *entry;

  if (!iw || !iw->ledger)
    return;

  /* get the current entry based on cursor position */
  entry = gnc_entry_ledger_get_current_entry (iw->ledger);
  if (!entry) {
    gnc_entry_ledger_cancel_cursor_changes (iw->ledger);
    return;
  }

  /* deleting the blank entry just cancels */
  if (entry == gnc_entry_ledger_get_blank_entry (iw->ledger)) {
    gnc_entry_ledger_cancel_cursor_changes (iw->ledger);
    return;
  }

  /* Verify that the user really wants to delete this entry */
  {
    const char *message = _("Are you sure you want to delete the "
			    "current entry?");
    const char *order_warn = _("This entry is attached to an order and "
			       "will be deleted from that as well!");
    char *msg;
    gboolean result;

    if (gncEntryGetOrder (entry))
      msg = g_strconcat (message, "\n\n", order_warn, NULL);
    else
      msg = g_strdup (message);

    result = gnc_verify_dialog_parented (iw->dialog, FALSE, msg);
    g_free (msg);

    if (!result)
      return;
  }

  /* Yep, let's delete */
  gnc_entry_ledger_delete_current_entry (iw->ledger);
  return;
}

static void
duplicateCB (GtkWidget *widget, gpointer data)
{
  InvoiceWindow *iw = data;

  if (!iw || !iw->ledger)
    return;

  gnc_entry_ledger_duplicate_current_entry (iw->ledger);
}

static void
blank_entry_cb (GtkWidget *widget, gpointer data)
{
  InvoiceWindow *iw = data;

  if (!iw || !iw->ledger)
    return;

  if (!gnc_entry_ledger_commit_entry (iw->ledger))
    return;

  {
    VirtualCellLocation vcell_loc;
    GncEntry *blank;

    blank = gnc_entry_ledger_get_blank_entry (iw->ledger);
    if (blank == NULL)
      return;

    if (gnc_entry_ledger_get_entry_virt_loc (iw->ledger, blank, &vcell_loc))
      gnucash_register_goto_virt_cell (iw->reg, vcell_loc);
  }
}

static void
gnc_invoice_window_edit_cb (GtkWidget *widget, gpointer data)
{
  InvoiceWindow *iw = data;
  GncInvoice *invoice = iw_get_invoice (iw);

  if (invoice)
    gnc_ui_invoice_modify (invoice);
}

static GtkWidget *
gnc_invoice_window_create_toolbar (InvoiceWindow *iw)
{
  GtkWidget *toolbar;

  GnomeUIInfo toolbar_info[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("Close"),
      N_("Close this invoice window"),
      gnc_invoice_window_ok_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_CLOSE,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Edit"),
      N_("Edit the invoice"),
      gnc_invoice_window_edit_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_PREFERENCES,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("Enter"),
      N_("Record the current entry"),
      recordCB, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_ADD,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Cancel"),
      N_("Cancel the current entry"),
      cancelCB, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_UNDELETE,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Delete"),
      N_("Delete the current entry"),
      deleteCB, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_TRASH,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("Duplicate"),
      N_("Make a copy of the current entry"),
      duplicateCB, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_COPY,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Blank"),
      N_("Move to the blank entry at the "
         "bottom of the invoice"),
      blank_entry_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_NEW,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("Print"),
      N_("Print this invoice"),
      gnc_invoice_window_print_invoice_cb,
      NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_PRINT,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Post"),
      N_("Post this invoice"),
      gnc_invoice_window_post_invoice_cb,
      NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_JUMP_TO,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);

  gnome_app_fill_toolbar_with_data (GTK_TOOLBAR(toolbar), toolbar_info,
                                    NULL, iw);

  iw->toolbar = toolbar;

  iw->edit_button = toolbar_info[1].widget;
  iw->print_button = toolbar_info[10].widget;
  iw->post_button = toolbar_info[11].widget;

  return toolbar;
}

static GtkWidget *
gnc_invoice_window_create_popup_menu (InvoiceWindow *iw)
{
  GtkWidget *popup;

  GnomeUIInfo transaction_menu[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("_Enter"),
      N_("Record the current entry"),
      recordCB, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Cancel"),
      N_("Cancel the current entry"),
      cancelCB, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Delete"),
      N_("Delete the current entry"),
      deleteCB, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("D_uplicate"),
      N_("Make a copy of the current entry"),
      duplicateCB, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("_Blank"),
      N_("Move to the blank entry at the "
         "bottom of the register"),
      blank_entry_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  gnc_fill_menu_with_data (transaction_menu, iw);

  popup = gnome_popup_menu_new (transaction_menu);

  return popup;
}

static void
gnc_invoice_save_size (InvoiceWindow *iw)
{
  gdk_window_get_geometry (iw->dialog->window, NULL, NULL, &last_width,
			   NULL, NULL);
  gnc_save_window_size (WIDTH_PREFIX, last_width, 0);
}

static void
size_allocate (GtkWidget *widget,
               GtkAllocation *allocation,
               gpointer user_data)
{
  InvoiceWindow *iw = user_data;
  gboolean resize = FALSE;

  /* HACK ALERT. this seems to be the only thing to get the
   * freekin register window to stop freekin resizing itself
   * all the freekin time.
   *
   * NOTE: Only resize on the SECOND time through.  I don't know why,
   * but this really seems to have an effect on the window the
   * _second_ time you pop one up.
   */

  if (iw->width == allocation->width)
    return;

  if (iw->width > 0)
    resize = TRUE;

  iw->width = allocation->width;

  if (resize)
    gtk_window_set_default_size (GTK_WINDOW(iw->dialog), iw->width, 0);
}

static int
gnc_invoice_job_changed_cb (GtkWidget *widget, gpointer data)
{
  InvoiceWindow *iw = data;
  GncInvoice *invoice;
  char const *msg = "";
  
  if (!iw)
    return FALSE;

  if (iw->dialog_type == VIEW_INVOICE)
    return FALSE;

  gnc_owner_get_owner (iw->job_choice, &(iw->job));
  invoice = iw_get_invoice (iw);

  if (iw->dialog_type == EDIT_INVOICE)
    return FALSE;

  msg = gncJobGetReference (gncOwnerGetJob (&(iw->job)));
  gtk_entry_set_text (GTK_ENTRY (iw->billing_id_entry), msg ? msg : "");

  return FALSE;

}

static GNCSearchWindow *
gnc_invoice_select_job_cb (gpointer jobp, gpointer user_data)
{
  GncJob *j = jobp;
  InvoiceWindow *iw = user_data;
  GncOwner owner, *ownerp;

  if (!iw) return NULL;

  if (j) {
    ownerp = gncJobGetOwner (j);
    gncOwnerCopy (ownerp, &owner);
  } else
    gncOwnerCopy (&(iw->owner), &owner);

  return gnc_job_search (j, &owner, iw->book);
}

static void
gnc_invoice_update_job_choice (InvoiceWindow *iw)
{
  if (iw->job_choice) {
    gtk_container_remove (GTK_CONTAINER (iw->job_box), iw->job_choice);
  }

  /* If we don't have a real owner, then we obviously can't have a job */
  if (iw->owner.owner.undefined == NULL) {
    iw->job_choice = NULL;

  } else
    switch (iw->dialog_type) {
    case VIEW_INVOICE:
    case EDIT_INVOICE:
      iw->job_choice =
	gnc_owner_edit_create (NULL, iw->job_box, iw->book, &(iw->job));
    break;
  case NEW_INVOICE:
  case MOD_INVOICE:
    iw->job_choice =
      gnc_general_search_new (GNC_JOB_MODULE_NAME, _("Select..."),
			      gnc_invoice_select_job_cb, iw);

    gnc_general_search_set_selected (GNC_GENERAL_SEARCH (iw->job_choice),
				     gncOwnerGetJob (&iw->job));
    gnc_general_search_allow_clear (GNC_GENERAL_SEARCH (iw->job_choice),
				    TRUE);
    gtk_box_pack_start (GTK_BOX (iw->job_box), iw->job_choice,
			TRUE, TRUE, 0);

    gtk_signal_connect (GTK_OBJECT (iw->job_choice), "changed",
			GTK_SIGNAL_FUNC (gnc_invoice_job_changed_cb),
			iw);
    break;
  }

  if (iw->job_choice)
    gtk_widget_show_all (iw->job_choice);
}

static int
gnc_invoice_owner_changed_cb (GtkWidget *widget, gpointer data)
{
  InvoiceWindow *iw = data;
  char const *msg = "";
  GncOwner owner;
  
  if (!iw)
    return FALSE;

  if (iw->dialog_type == VIEW_INVOICE)
    return FALSE;

  gncOwnerCopy (&(iw->owner), &owner);
  gnc_owner_get_owner (iw->owner_choice, &owner);

  /* If this owner really changed, then reset ourselves */
  if (!gncOwnerEqual (&owner, &(iw->owner))) {
    GncInvoice *invoice;

    gncOwnerCopy (&owner, &(iw->owner));
    gncOwnerInitJob (&(iw->job), NULL);
    invoice = iw_get_invoice (iw);
    gnc_entry_ledger_reset_query (iw->ledger);
  }

  if (iw->dialog_type == EDIT_INVOICE)
    return FALSE;

  switch (gncOwnerGetType (&(iw->owner))) {
  case GNC_OWNER_CUSTOMER:
    msg = gncCustomerGetTerms (gncOwnerGetCustomer (&(iw->owner)));
    break;
  case GNC_OWNER_VENDOR:
    msg = gncVendorGetTerms (gncOwnerGetVendor (&(iw->owner)));
    break;
  default:
    g_warning ("Unknown owner type: %d\n", gncOwnerGetType (&(iw->owner)));
    break;
  }

  gtk_entry_set_text (GTK_ENTRY (iw->terms_entry), msg ? msg : "");

  gnc_invoice_update_job_choice (iw);

  return FALSE;
}

static void
gnc_invoice_dialog_close_handler (gpointer user_data)
{
  InvoiceWindow *iw = user_data;

  if (iw)
    gnome_dialog_close (GNOME_DIALOG (iw->dialog));
}

static void
gnc_invoice_window_close_handler (gpointer user_data)
{
  InvoiceWindow *iw = user_data;

  if (iw) {
    switch (iw->dialog_type) {
    case VIEW_INVOICE:
    case EDIT_INVOICE:
      gnc_invoice_save_size (iw);
      break;
    default:
      break;
    }
    gtk_widget_destroy (iw->dialog);
  }
}

static void
gnc_invoice_redraw_all_cb (GnucashRegister *g_reg, gpointer data)
{
  //  InvoiceWindow *iw = data;

  //  if (iw)
  //    gnc_invoice_update_window (iw);
}

static void
gnc_invoice_redraw_help_cb (GnucashRegister *g_reg, gpointer data)
{
  InvoiceWindow *iw = data;
  const char *status;
  char *help;

  if (!iw)
    return;

  help = gnc_table_get_help (gnc_entry_ledger_get_table (iw->ledger));
  status = help ? help : "";
  gnome_appbar_set_default (GNOME_APPBAR (iw->statusbar), status);
  g_free (help);
}

static void
gnc_invoice_window_refresh_handler (GHashTable *changes, gpointer user_data)
{
  InvoiceWindow *iw = user_data;
  const EventInfo *info;
  GncInvoice *invoice = iw_get_invoice (iw);
  GncOwner *owner;

 /* If there isn't a invoice behind us, close down */
  if (!invoice) {
    gnc_close_gui_component (iw->component_id);
    return;
  }

  /* Next, close if this is a destroy event */
  if (changes) {
    info = gnc_gui_get_entity_events (changes, &iw->invoice_guid);
    if (info && (info->event_mask & GNC_EVENT_DESTROY)) {
      gnc_close_gui_component (iw->component_id);
      return;
    }
  }

  /* Check the owners, and see if they have changed */
  owner = gncInvoiceGetOwner (invoice);

  /* Copy the owner information into our window */
  gncOwnerCopy (gncOwnerGetEndOwner (owner), &(iw->owner));
  gncOwnerInitJob (&(iw->job), gncOwnerGetJob (owner));

  /* Ok, NOW let's refresh ourselves */
  gnc_invoice_update_window (iw);
}

static void
gnc_configure_register_colors (void)
{
  GncEntryLedgerColors reg_colors;

  reg_colors.header_bg_color =
    gnc_lookup_color_option_argb("Register Colors",
                                 "Header color",
                                 0xffffff);

  reg_colors.primary_bg_color =
    gnc_lookup_color_option_argb("Register Colors",
                                 "Primary color",
                                 0xffffff);

  reg_colors.secondary_bg_color =
    gnc_lookup_color_option_argb("Register Colors",
                                 "Secondary color",
                                 0xffffff);

  reg_colors.primary_active_bg_color =
    gnc_lookup_color_option_argb("Register Colors",
                                 "Primary active color",
                                 0xffffff);

  reg_colors.secondary_active_bg_color =
    gnc_lookup_color_option_argb("Register Colors",
                                 "Secondary active color",
                                 0xffffff);

  gnc_entry_ledger_set_colors (reg_colors);
}

static void
gnc_invoice_update_window (InvoiceWindow *iw)
{
  GtkWidget *acct_entry;
  GncInvoice *invoice;
  gboolean is_posted = FALSE;

  invoice = iw_get_invoice (iw);

  if (iw->owner_choice) {
    gtk_container_remove (GTK_CONTAINER (iw->owner_box), iw->owner_choice);
  }

  switch (iw->dialog_type) {
  case VIEW_INVOICE:
  case EDIT_INVOICE:
    iw->owner_choice =
      gnc_owner_edit_create (iw->owner_label, iw->owner_box, iw->book,
			     &(iw->owner));
    break;
  case NEW_INVOICE:
  case MOD_INVOICE:
    iw->owner_choice =
      gnc_owner_select_create (iw->owner_label, iw->owner_box, iw->book,
			       &(iw->owner));

    gtk_signal_connect (GTK_OBJECT (iw->owner_choice), "changed",
			GTK_SIGNAL_FUNC (gnc_invoice_owner_changed_cb),
			iw);

    break;
  }

  gnc_invoice_update_job_choice (iw);


  switch (iw->dialog_type) {
  case VIEW_INVOICE:
  case EDIT_INVOICE:
    if (last_width == 0)
      gnc_get_window_size (WIDTH_PREFIX, &last_width, NULL);

    gtk_window_set_default_size (GTK_WINDOW (iw->dialog), last_width, 0);
    break;
  default:
    break;
  }

  gtk_widget_show_all (iw->dialog);

  acct_entry = glade_xml_get_widget (iw->xml, "acct_entry");

  /* We know that "invoice" (and "owner") exist now */
  do {
    const char *string;
    Timespec ts, ts_zero = {0,0};
    Account *acct;
    gint pos = 0;

    gtk_entry_set_text (GTK_ENTRY (iw->id_entry), gncInvoiceGetID (invoice));

    gtk_entry_set_text (GTK_ENTRY (iw->billing_id_entry),
			gncInvoiceGetBillingID (invoice));

    gtk_entry_set_text (GTK_ENTRY (iw->terms_entry),
			gncInvoiceGetTerms (invoice));

    string = gncInvoiceGetNotes (invoice);
    gtk_editable_delete_text (GTK_EDITABLE (iw->notes_text), 0, -1);
    gtk_editable_insert_text (GTK_EDITABLE (iw->notes_text), string,
			      strlen (string), &pos);

    if (iw->active_check)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (iw->active_check),
				    gncInvoiceGetActive (invoice));

    ts = gncInvoiceGetDateOpened (invoice);
    if (timespec_equal (&ts, &ts_zero)) {
      gnc_date_edit_set_time (GNC_DATE_EDIT (iw->opened_date), time(NULL));
    } else {
      gnc_date_edit_set_time_ts (GNC_DATE_EDIT (iw->opened_date), ts);
    }

    /*
     * Next, figure out if we've been posted, and if so set the
     * appropriate bits of information.. Then work on hiding or
     * unhiding as necessary.
     */

    acct = gncInvoiceGetPostedAcc (invoice);
    if (!acct)
      break;

    /* Ok, it's definitely posted. Setup the 'posted-invoice' fields now */
    is_posted = TRUE;
    
    ts = gncInvoiceGetDatePosted (invoice);
    gnc_date_edit_set_time_ts (GNC_DATE_EDIT (iw->posted_date), ts);

    string = xaccAccountGetFullName (acct, gnc_get_account_separator ());
    gtk_entry_set_text (GTK_ENTRY (acct_entry), string);


  } while (FALSE);

  if (iw->dialog_type == NEW_INVOICE || iw->dialog_type == MOD_INVOICE)
    return;

  /* Hide/show the appropriate widgets based on our posted/paid state */

  {
    GtkWidget *hide;

    if (is_posted == TRUE) {
      hide = glade_xml_get_widget (iw->xml, "hide3");
      gtk_widget_hide_all (hide);
      hide = glade_xml_get_widget (iw->xml, "hide4");
      gtk_widget_hide_all (hide);

      /* Hide the edit and post buttons */
      gtk_widget_hide_all (iw->post_button);
      gtk_widget_hide_all (iw->edit_button);

    } else {			/* ! posted */
      hide = glade_xml_get_widget (iw->xml, "posted_label");
      gtk_widget_hide_all (hide);
      gtk_widget_hide_all (iw->posted_date_hbox);

      hide = glade_xml_get_widget (iw->xml, "acct_label");
      gtk_widget_hide_all (hide);
      gtk_widget_hide_all (acct_entry);

      hide = glade_xml_get_widget (iw->xml, "hide1");
      gtk_widget_hide_all (hide);
      hide = glade_xml_get_widget (iw->xml, "hide2");
      gtk_widget_hide_all (hide);

      /* Hide the print button */
      gtk_widget_hide_all (iw->print_button);
    }
  }

  if (is_posted) {
    //    GtkWidget *hide;

    /* Setup viewer for read-only access */
    /*
    gtk_widget_set_sensitive (iw->id_entry, FALSE);
    gtk_widget_set_sensitive (iw->terms_entry, FALSE);
    gtk_widget_set_sensitive (iw->opened_date, FALSE);
    gtk_widget_set_sensitive (iw->notes_text, FALSE); *//* XXX: should notes remain writable? */

  }  
}

static gboolean
find_handler (gpointer find_data, gpointer user_data)
{
  const GUID *invoice_guid = find_data;
  InvoiceWindow *iw = user_data;

  return(iw && guid_equal(&iw->invoice_guid, invoice_guid));
}

static InvoiceWindow *
gnc_invoice_new_window (GNCBook *bookp, InvoiceDialogType type,
			GncInvoice *invoice, GncOwner *owner)
{
  InvoiceWindow *iw;
  GladeXML *xml;
  GtkWidget *hbox;
  GncEntryLedger *entry_ledger = NULL;

  g_assert (type != NEW_INVOICE && type != MOD_INVOICE);

  /*
   * Find an existing window for this invoice.  If found, bring it to
   * the front.
   */
  if (invoice) {
    GUID invoice_guid;

    invoice_guid = *gncInvoiceGetGUID (invoice);
    iw = gnc_find_first_gui_component (DIALOG_VIEW_INVOICE_CM_CLASS,
				       find_handler, &invoice_guid);
    if (iw) {
      gtk_window_present (GTK_WINDOW(iw->dialog));
      return(iw);
    }
  }
  
  /*
   * No existing invoice window found.  Build a new one.
   */
  gnc_configure_register_colors ();

  iw = g_new0 (InvoiceWindow, 1);
  iw->book = bookp;
  iw->dialog_type = type;
  iw->width = -1;

  /* Save this for later */
  gncOwnerCopy (gncOwnerGetEndOwner (owner), &(iw->owner));
  gncOwnerInitJob (&(iw->job), gncOwnerGetJob (owner));

  /* Find the dialog */
  iw->xml = xml = gnc_glade_xml_new ("invoice.glade", "Invoice Entry Window");
  iw->dialog = glade_xml_get_widget (xml, "Invoice Entry Window");

  gtk_object_set_data (GTK_OBJECT (iw->dialog), "dialog_info", iw);

  /* Grab the widgets */
  iw->id_entry = glade_xml_get_widget (xml, "id_entry");
  iw->billing_id_entry = glade_xml_get_widget (xml, "billing_id_entry");
  iw->terms_entry = glade_xml_get_widget (xml, "terms_entry");
  iw->notes_text = glade_xml_get_widget (xml, "notes_text");
  iw->active_check = glade_xml_get_widget (xml, "active_check");
  iw->owner_box = glade_xml_get_widget (xml, "owner_hbox");
  iw->owner_label = glade_xml_get_widget (xml, "owner_label");
  iw->job_box = glade_xml_get_widget (xml, "job_hbox");

  iw->menubar_dock = glade_xml_get_widget (xml, "menu_dock");
  iw->toolbar_dock = glade_xml_get_widget (xml, "toolbar_dock");
  iw->statusbar = glade_xml_get_widget (xml, "status_bar");

  hbox = glade_xml_get_widget (xml, "date_opened_hbox");
  iw->opened_date = gnc_date_edit_new (time(NULL), FALSE, FALSE);
  gtk_box_pack_start (GTK_BOX(hbox), iw->opened_date, TRUE, TRUE, 0);

  iw->posted_date_hbox = glade_xml_get_widget (xml, "date_posted_hbox");
  iw->posted_date = gnc_date_edit_new (time(NULL), FALSE, FALSE);
  gtk_box_pack_start (GTK_BOX(iw->posted_date_hbox), iw->posted_date,
		      TRUE, TRUE, 0);

  /* Build the ledger */
  switch (type) {
  case EDIT_INVOICE:
    entry_ledger = gnc_entry_ledger_new (iw->book, GNCENTRY_INVOICE_ENTRY);
    break;
  case VIEW_INVOICE:
  default:
    entry_ledger = gnc_entry_ledger_new (iw->book, GNCENTRY_INVOICE_VIEWER);
    break;
  }

  /* Save the ledger... */
  iw->ledger = entry_ledger;
  gnc_entry_ledger_set_parent (entry_ledger, iw->dialog);

  /* Set the entry_ledger's invoice */
  gnc_entry_ledger_set_default_invoice (entry_ledger, invoice);

  /* load the menu bar */
  /*
  {
    GtkWidget *menubar;

    menubar = gnc_invoice_window_create_menubar (iw);
    gtk_container_set_border_width (GTK_CONTAINER (menubar), 2);
    gtk_container_add (GTK_CONTAINER (iw->menubar_dock), menubar);
  }
  */

  /* Load the tool bar */
  {
    GtkWidget *toolbar;

    toolbar = gnc_invoice_window_create_toolbar (iw);
    gtk_container_set_border_width (GTK_CONTAINER (toolbar), 2);
    gtk_container_add (GTK_CONTAINER (iw->toolbar_dock), toolbar);
  }

  /* Setup initial values */
  iw->invoice_guid = *gncInvoiceGetGUID (invoice);

  iw->component_id =
    gnc_register_gui_component (DIALOG_VIEW_INVOICE_CM_CLASS,
				gnc_invoice_window_refresh_handler,
				gnc_invoice_window_close_handler,
				iw);

  gnc_gui_component_watch_entity_type (iw->component_id,
				       GNC_INVOICE_MODULE_NAME,
				       GNC_EVENT_MODIFY | GNC_EVENT_DESTROY);

  gtk_signal_connect (GTK_OBJECT (iw->dialog), "destroy",
		      GTK_SIGNAL_FUNC(gnc_invoice_window_destroy_cb), iw);
  gtk_signal_connect (GTK_OBJECT (iw->dialog), "size-allocate",
		      GTK_SIGNAL_FUNC(size_allocate), iw);

  /* Create the register */
  {
    GtkWidget *regWidget, *frame;
    GtkWidget *popup;
    guint num_rows;

    num_rows = (guint) gnc_lookup_number_option ("Invoice",
                                                 "Number of Rows", 10.0);
    gnucash_register_set_initial_rows( num_rows );

    /* Watch the order of operations, here... */
    regWidget = gnucash_register_new (gnc_entry_ledger_get_table
				      (entry_ledger));
    gnc_table_init_gui( regWidget, entry_ledger );

    frame = glade_xml_get_widget (xml, "ledger_frame");
    gtk_container_add (GTK_CONTAINER (frame), regWidget);
    
    iw->reg = GNUCASH_REGISTER (regWidget);
    GNUCASH_SHEET (iw->reg->sheet)->window = iw->dialog;

    gtk_signal_connect (GTK_OBJECT(regWidget), "activate_cursor",
			GTK_SIGNAL_FUNC(recordCB), iw);
    gtk_signal_connect (GTK_OBJECT(regWidget), "redraw_all",
			GTK_SIGNAL_FUNC(gnc_invoice_redraw_all_cb), iw);
    gtk_signal_connect (GTK_OBJECT(regWidget), "redraw_help",
			GTK_SIGNAL_FUNC(gnc_invoice_redraw_help_cb), iw);

    popup = gnc_invoice_window_create_popup_menu (iw);
    gnucash_register_attach_popup (GNUCASH_REGISTER (regWidget),
				   popup, iw);

  }

  gnc_table_realize_gui (gnc_entry_ledger_get_table (entry_ledger));

  /* Now fill in a lot of the pieces and display properly */
  gnc_invoice_update_window (iw);

  gnc_table_refresh_gui (gnc_entry_ledger_get_table (iw->ledger), TRUE);
  gnc_window_adjust_for_screen (GTK_WINDOW(iw->dialog));

  return iw;
}

static InvoiceWindow *
gnc_invoice_window_new_invoice (GNCBook *bookp, GncOwner *owner,
				GncInvoice *invoice)
{
  InvoiceWindow *iw;
  GladeXML *xml;
  GnomeDialog *iwd;
  GtkWidget *hbox;

  if (invoice) {
    /*
     * Try to find an existing window for this invoice.  If found,
     * bring it to the front.
     */
    GUID invoice_guid;

    invoice_guid = *gncInvoiceGetGUID (invoice);
    iw = gnc_find_first_gui_component (DIALOG_NEW_INVOICE_CM_CLASS,
				       find_handler, &invoice_guid);
    if (iw) {
      gtk_window_present (GTK_WINDOW(iw->dialog));
      return(iw);
    }
  }

  /*
   * No existing invoice window found.  Build a new one.
   */

  iw = g_new0 (InvoiceWindow, 1);

  if (invoice == NULL) {
    iw->dialog_type = NEW_INVOICE;
    invoice = gncInvoiceCreate (bookp);
    gncInvoiceSetCommonCommodity (invoice, gnc_default_currency ()); /* XXX */
    iw->book = bookp;
  } else {
    iw->dialog_type = MOD_INVOICE;
    owner = gncInvoiceGetOwner (invoice);
    iw->book = gncInvoiceGetBook (invoice);
  }

  /* Save this for later */
  gncOwnerCopy (gncOwnerGetEndOwner(owner), &(iw->owner));
  gncOwnerInitJob (&(iw->job), gncOwnerGetJob (owner));

  /* Find the dialog */
  iw->xml = xml = gnc_glade_xml_new ("invoice.glade", "New Invoice Dialog");
  iw->dialog = glade_xml_get_widget (xml, "New Invoice Dialog");
  iwd = GNOME_DIALOG (iw->dialog);

  gtk_object_set_data (GTK_OBJECT (iw->dialog), "dialog_info", iw);

  /* Grab the widgets */
  iw->id_entry = glade_xml_get_widget (xml, "id_entry");
  iw->billing_id_entry = glade_xml_get_widget (xml, "billing_id_entry");
  iw->terms_entry = glade_xml_get_widget (xml, "terms_entry");
  iw->notes_text = glade_xml_get_widget (xml, "notes_text");
  iw->owner_box = glade_xml_get_widget (xml, "owner_hbox");
  iw->owner_label = glade_xml_get_widget (xml, "owner_label");
  iw->job_box = glade_xml_get_widget (xml, "job_hbox");


  hbox = glade_xml_get_widget (xml, "date_opened_hbox");
  iw->opened_date = gnc_date_edit_new (time(NULL), FALSE, FALSE);
  gtk_box_pack_start (GTK_BOX(hbox), iw->opened_date, TRUE, TRUE, 0);

  /* default to ok */
  gnome_dialog_editable_enters (iwd, GTK_EDITABLE (iw->id_entry));
  gnome_dialog_set_default (iwd, 0);

  gtk_signal_connect (GTK_OBJECT (iw->dialog), "destroy",
		      GTK_SIGNAL_FUNC(gnc_invoice_window_destroy_cb), iw);

  gnome_dialog_button_connect (iwd, 0,
  			       GTK_SIGNAL_FUNC(gnc_invoice_window_ok_cb), iw);
  gnome_dialog_button_connect (iwd, 1,
  			       GTK_SIGNAL_FUNC(gnc_invoice_window_cancel_cb), iw);
  gnome_dialog_button_connect (iwd, 2,
  			       GTK_SIGNAL_FUNC(gnc_invoice_window_help_cb), iw);

  /* Setup initial values */
  iw->invoice_guid = *gncInvoiceGetGUID (invoice);

  iw->component_id =
    gnc_register_gui_component (DIALOG_NEW_INVOICE_CM_CLASS,
				gnc_invoice_window_refresh_handler,
				gnc_invoice_dialog_close_handler,
				iw);

  gnc_gui_component_watch_entity_type (iw->component_id,
				       GNC_INVOICE_MODULE_NAME,
				       GNC_EVENT_MODIFY | GNC_EVENT_DESTROY);

  /* Now fill in a lot of the pieces and display properly */
  gnc_invoice_update_window (iw);
  gnc_table_refresh_gui (gnc_entry_ledger_get_table (iw->ledger), TRUE);

  return iw;
}

InvoiceWindow *
gnc_ui_invoice_edit (GncInvoice *invoice)
{
  InvoiceWindow *iw;
  InvoiceDialogType type;

  if (!invoice) return NULL;

  /* Immutable once we've been posted */
  if (gncInvoiceGetPostedAcc (invoice))
    type = VIEW_INVOICE;
  else
    type = EDIT_INVOICE;

  iw = gnc_invoice_new_window (gncInvoiceGetBook(invoice), type,
			       invoice, gncInvoiceGetOwner (invoice));

  return iw;
}

static InvoiceWindow *
gnc_ui_invoice_modify (GncInvoice *invoice)
{
  InvoiceWindow *iw;
  if (!invoice) return NULL;

  iw = gnc_invoice_window_new_invoice (NULL, NULL, invoice);
  return iw;
}

InvoiceWindow *
gnc_ui_invoice_new (GncOwner *ownerp, GNCBook *bookp)
{
  InvoiceWindow *iw;
  GncOwner owner;

  if (ownerp) {
    gncOwnerCopy (ownerp, &owner);
  } else
    gncOwnerInitCustomer (&owner, NULL); /* XXX: pass in the owner type? */

  /* Make sure required options exist */
  if (!bookp) return NULL;

  iw = gnc_invoice_window_new_invoice (bookp, &owner, NULL);

  return iw;
}

/* Functions for invoice selection widgets */

static void
edit_invoice_cb (gpointer *invoice_p, gpointer user_data)
{
  GncInvoice *invoice;

  g_return_if_fail (invoice_p && user_data);

  invoice = *invoice_p;

  if (!invoice)
    return;

  gnc_ui_invoice_edit (invoice);
}

static void
pay_invoice_cb (gpointer *invoice_p, gpointer user_data)
{
  struct _invoice_select_window *sw = user_data;
  GncInvoice *invoice;

  g_return_if_fail (invoice_p && user_data);

  invoice = *invoice_p;

  if (!invoice)
    return;

  gnc_ui_payment_new (gncInvoiceGetOwner (invoice), sw->book);
}

static gpointer
new_invoice_cb (gpointer user_data)
{
  struct _invoice_select_window *sw = user_data;
  InvoiceWindow *iw;
  
  g_return_val_if_fail (user_data, NULL);

  iw = gnc_ui_invoice_new (sw->owner, sw->book);
  return iw_get_invoice (iw);
}

static void
free_invoice_cb (gpointer user_data)
{
  struct _invoice_select_window *sw = user_data;

  g_return_if_fail (sw);

  gncQueryDestroy (sw->q);
  g_free (sw);
}

GNCSearchWindow *
gnc_invoice_search (GncInvoice *start, GncOwner *owner, GNCBook *book)
{
  GNCIdType type = GNC_INVOICE_MODULE_NAME;
  struct _invoice_select_window *sw;
  QueryNew *q, *q2 = NULL;
  static GList *params = NULL;
  static GList *columns = NULL;
  static GNCSearchCallbackButton buttons[] = { 
    { N_("View/Edit Invoice"), edit_invoice_cb},
    { N_("Process Payment"), pay_invoice_cb},
    { NULL },
  };

  g_return_val_if_fail (book, NULL);

  /* Build parameter list in reverse order */
  if (params == NULL) {
    params = gnc_search_param_prepend (params, _("Invoice Owner"), NULL, type,
				       INVOICE_OWNER, NULL);
    params = gnc_search_param_prepend (params, _("Invoice Notes"), NULL, type,
				       INVOICE_NOTES, NULL);
    params = gnc_search_param_prepend (params, _("Billing ID"), NULL, type,
				       INVOICE_BILLINGID, NULL);
    params = gnc_search_param_prepend (params, _("Date Posted"), NULL, type,
				       INVOICE_POSTED, NULL);
    params = gnc_search_param_prepend (params, _("Is Posted?"), NULL, type,
				       INVOICE_IS_POSTED, NULL);
    params = gnc_search_param_prepend (params, _("Date Opened"), NULL, type,
				       INVOICE_OPENED, NULL);
    params = gnc_search_param_prepend (params, _("Company Name "), NULL, type,
				       INVOICE_OWNER, OWNER_PARENT,
				       OWNER_NAME, NULL);
    params = gnc_search_param_prepend (params, _("Invoice ID"), NULL, type,
				       INVOICE_ID, NULL);
  }

  /* Build the column list in reverse order */
  if (columns == NULL) {
    columns = gnc_search_param_prepend (columns, _("Billing ID"), NULL, type,
					INVOICE_BILLINGID, NULL);
    columns = gnc_search_param_prepend (columns, _("Company"), NULL, type,
					INVOICE_OWNER, OWNER_PARENT,
					OWNER_NAME, NULL);
    columns = gnc_search_param_prepend (columns, _("Type"), NULL, type,
					INVOICE_TYPE, NULL);
    columns = gnc_search_param_prepend (columns, _("Posted"), NULL, type,
					INVOICE_POSTED, NULL);
    columns = gnc_search_param_prepend (columns, _("Opened"), NULL, type,
					INVOICE_OPENED, NULL);
    columns = gnc_search_param_prepend (columns, _("Num"), NULL, type,
					INVOICE_ID, NULL);
  }

  /* Build the queries */
  q = gncQueryCreate ();
  gncQuerySetBook (q, book);

  /* If owner is supplied, limit all searches to invoices who's owner
   * (or parent) is the supplied owner!  Show all invoices by this owner.
   */
  if (owner && gncOwnerGetGUID (owner)) {
    gncQueryAddGUIDMatch (q, g_slist_prepend
			  (g_slist_prepend (NULL, QUERY_PARAM_GUID),
			   INVOICE_OWNER),
			  gncOwnerGetGUID (owner), QUERY_AND);

    q2 = gncQueryCopy (q);
  }

  if (start) {
    if (q2 == NULL)
      q2 = gncQueryCopy (q);

    gncQueryAddGUIDMatch (q2, g_slist_prepend (NULL, QUERY_PARAM_GUID),
			  gncInvoiceGetGUID (start), QUERY_AND);
  }

  /* launch select dialog and return the result */
  sw = g_new0 (struct _invoice_select_window, 1);

  if (owner) {
    gncOwnerCopy (owner, &(sw->owner_def));
    sw->owner = &(sw->owner_def);
  }
  sw->book = book;
  sw->q = q;

  return gnc_search_dialog_create (type, params, columns, q, q2,
				   buttons, NULL, new_invoice_cb,
				   sw, free_invoice_cb);

}

GNCSearchWindow *
gnc_invoice_search_select (gpointer start, gpointer book)
{
  GncInvoice *i = start;
  GncOwner owner, *ownerp;

  if (!book) return NULL;

  if (i) {
    ownerp = gncInvoiceGetOwner (i);
    gncOwnerCopy (ownerp, &owner);
  } else
    gncOwnerInitCustomer (&owner, NULL); /* XXX */

  return gnc_invoice_search (start, NULL, book);
}

GNCSearchWindow *
gnc_invoice_search_edit (gpointer start, gpointer book)
{
  if (start)
    gnc_ui_invoice_edit (start);

  return NULL;
}
