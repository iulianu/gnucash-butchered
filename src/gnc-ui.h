/********************************************************************\
 * gnc-ui.h - High level UI functions for GnuCash                   *
 * Copyright (C) 1997 Robin D. Clark                                *
 * Copyright (C) 1999, 2000 Rob Browning <rlb@cs.utexas.edu>        *
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
 * along with this program; if not, write to the Free Software      *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.        *
\********************************************************************/

#ifndef GNC_UI_H
#define GNC_UI_H

#include <glib.h>

#include "gnc-ui-common.h"
#include "Account.h"


/** Help Files ******************************************************/
#define HH_ACC               "gnucash-help/usage.html#acct-create"
#define HH_ACCEDIT           "gnucash-help/usage.html#acct-edit"
#define HH_COMMODITY         "gnucash-help/usage.html#tool-commodity"
#define HH_CUSTOMER          "gnucash-help/usage.html"
#define HH_EMPLOYEE          "gnucash-help/usage.html"
#define HH_FIND_TRANSACTIONS "gnucash-help/usage.html#tool-find"
#define HH_GLOBPREFS         "gnucash-help/custom-gnucash.html#set-prefs"
#define HH_HELP              "gnucash-help/help.html"
#define HH_INVOICE           "gnucash-help/usage.html"
#define HH_JOB               "gnucash-help/usage.html"
#define HH_MAIN              "gnucash-guide/index.html"
#define HH_ORDER             "gnucash-help/usage.html"
#define HH_PRINTCHECK        "gnucash-help/usage.html#print-check"
#define HH_QUICKSTART        "gnucash-guide/index.html"
#define HH_RECNWIN           "gnucash-help/usage.html#acct-reconcile"
#define HH_SXEDITOR          "gnucash-help/usage.html#tran-sched"
#define HH_VENDOR            "gnucash-help/usage.html"

/* Dialog windows ***************************************************/

typedef enum
{
  GNC_VERIFY_NO,
  GNC_VERIFY_YES,
  GNC_VERIFY_CANCEL,
  GNC_VERIFY_OK
} GNCVerifyResult;

extern GNCVerifyResult
gnc_verify_cancel_dialog(GNCVerifyResult default_result,
			 const char *format, ...) G_GNUC_PRINTF (2, 3);
extern GNCVerifyResult
gnc_verify_cancel_dialog_parented(gncUIWidget parent,
				  GNCVerifyResult default_result,
				  const char *format, ...) G_GNUC_PRINTF (3,4);



extern gboolean
gnc_verify_dialog(gboolean yes_is_default,
		  const char *format, ...) G_GNUC_PRINTF (2, 3);
extern gboolean
gnc_verify_dialog_parented(gncUIWidget parent,
			   gboolean yes_is_default,
			   const char *format, ...) G_GNUC_PRINTF (3, 4);



extern GNCVerifyResult
gnc_ok_cancel_dialog(GNCVerifyResult default_result,
		     const char *format, ...) G_GNUC_PRINTF (2, 3);
extern GNCVerifyResult
gnc_ok_cancel_dialog_parented(gncUIWidget parent,
			      GNCVerifyResult default_result,
			      const char *format, ...) G_GNUC_PRINTF (3,4);



extern void
gnc_warning_dialog_parented(gncUIWidget parent,
			    const char *forrmat, ...) G_GNUC_PRINTF (2, 3);



extern void
gnc_error_dialog(const char *format, ...) G_GNUC_PRINTF (1, 2);
extern void
gnc_error_dialog_parented(GtkWindow *parent,
			  const char *forrmat, ...) G_GNUC_PRINTF (2, 3);



int      gnc_choose_radio_option_dialog_parented (gncUIWidget parent,
                                                  const char *title,
                                                  const char *msg,
                                                  int default_value,
                                                  GList *radio_list);

gboolean gnc_dup_trans_dialog (gncUIWidget parent, time_t *date_p,
                               const char *num, char **out_num);
void     gnc_tax_info_dialog (gncUIWidget parent);
void     gnc_stock_split_dialog (Account * initial);

typedef enum
{
  GNC_PRICE_EDIT,
  GNC_PRICE_NEW,
} GNCPriceEditType;

GNCPrice* gnc_price_edit_dialog (gncUIWidget parent, GNCPrice *price,
				GNCPriceEditType type);
GNCPrice * gnc_price_edit_by_guid (GtkWidget * parent, const GUID * guid);
void     gnc_prices_dialog (gncUIWidget parent);
void     gnc_commodities_dialog (gncUIWidget parent);

/* Open a dialog asking for username and password. The heading and
 * either 'initial_*' arguments may be NULL. If the dialog returns
 * TRUE, the user pressed OK and the entered strings are stored in the
 * output variables. They should be g_freed when no longer needed. If
 * the dialog returns FALSE, the user pressed CANCEL and NULL was
 * stored in username and password. */
gboolean gnc_get_username_password (gncUIWidget parent,
                                    const char *heading,
                                    const char *initial_username,
                                    const char *initial_password,
                                    char **username,
                                    char **password);

/* Managing the GUI Windows *****************************************/

gncUIWidget gnc_ui_get_toplevel (void);

/* Changing the GUI Cursor ******************************************/

void gnc_set_busy_cursor(gncUIWidget w, gboolean update_now);
void gnc_unset_busy_cursor(gncUIWidget w);

/* QIF Import Windows ***********************************************/

typedef struct _qifimportwindow QIFImportWindow;

QIFImportWindow * gnc_ui_qif_import_dialog_make(void);
void              gnc_ui_qif_import_dialog_destroy(QIFImportWindow * window);


#endif
