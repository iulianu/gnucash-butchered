/********************************************************************\
 * gnc-regwidget.c -- A widget for the common register look-n-feel. *
 * Copyright (C) 1997 Robin D. Clark                                *
 * Copyright (C) 1997-1998 Linas Vepstas <linas@linas.org>          *
 * Copyright (C) 1998 Rob Browning <rlb@cs.utexas.edu>              *
 * Copyright (C) 1999-2000 Dave Peticolas <dave@krondo.com>         *
 * Copyright (C) 2001 Gnumatic, Inc.                                *
 * Copyright (C) 2002 Joshua Sled <jsled@asynchronous.org>          *
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
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
\********************************************************************/

#define _GNU_SOURCE

#include "config.h"

#include <gnome.h>
#include <time.h>

#include "AccWindow.h"
#include "Scrub.h"
#include "global-options.h"
#include "gnc-component-manager.h"
#include "gnc-date-edit.h"
#include "gnc-engine-util.h"
#include "gnc-euro.h"
#include "gnc-gui-query.h"
#include "gnc-ledger-display.h"
#include "gnc-pricedb.h"
#include "gnc-ui-util.h"
#include "gnc-ui.h"
#include "gnucash-sheet.h"
#include "messages.h"
#include "table-allgui.h"

#include <libguile.h>
#include "gnc-regwidget.h"
#include "gnc-engine-util.h"
#include "dialog-utils.h"

static short module = MOD_SX;

/** PROTOTYPES ******************************************************/
static void gnc_register_redraw_all_cb (GnucashRegister *g_reg, gpointer data);
static void gnc_register_redraw_help_cb (GnucashRegister *g_reg,
                                         gpointer data);
static void gnc_reg_refresh_toolbar(GNCRegWidget *regData);
static void regDestroy(GNCLedgerDisplay *ledger);
static void gnc_register_check_close(GNCRegWidget *regData);

static void cutCB(GtkWidget *w, gpointer data);
static void copyCB(GtkWidget *w, gpointer data);
static void pasteCB(GtkWidget *w, gpointer data);
static void cutTransCB(GtkWidget *w, gpointer data);
static void copyTransCB(GtkWidget *w, gpointer data);
static void pasteTransCB(GtkWidget *w, gpointer data);

static void deleteCB(GNCRegWidget *rw, gpointer data);
static void duplicateCB(GNCRegWidget *rw, gpointer data);
static void recordCB(GNCRegWidget *rw, gpointer data);
static void cancelCB(GNCRegWidget *rw, gpointer data);
static void expand_ent_cb(GNCRegWidget *rw, gpointer data);
static void new_trans_cb(GNCRegWidget *rw, gpointer data);
static void jump_cb(GNCRegWidget *rw, gpointer data );

static void gnc_register_jump_to_blank (GNCRegWidget *rw);

static void gnc_regWidget_class_init (GNCRegWidgetClass *);
static void gnc_regWidget_init (GNCRegWidget *);
static void gnc_regWidget_init2( GNCRegWidget *rw, GNCLedgerDisplay *ledger, GtkWindow *win );
static void emit_cb( GNCRegWidget *rw, const char *signal_name, gpointer ud );

static void emit_enter_ent_cb( GtkWidget *widget, gpointer data );
static void emit_cancel_ent_cb( GtkWidget *widget, gpointer data );
static void emit_delete_ent_cb( GtkWidget *widget, gpointer data );
static void emit_dup_ent_cb( GtkWidget *widget, gpointer data );
static void emit_schedule_ent_cb( GtkWidget *widget, gpointer data );
static void emit_expand_ent_cb( GtkWidget *widget, gpointer data );
static void emit_blank_cb( GtkWidget *widget, gpointer data );
static void emit_jump_cb( GtkWidget *widget, gpointer data );

/* Easy way to pass the sort-type */
typedef enum {
  BY_NONE = 0,
  BY_STANDARD,
  BY_DATE,
  BY_DATE_ENTERED,
  BY_DATE_RECONCILED,
  BY_NUM,
  BY_AMOUNT,
  BY_MEMO,
  BY_DESC
} sort_type_t;

guint
gnc_regWidget_get_type ()
{
  static guint gnc_regWidget_type = 0;

  if (!gnc_regWidget_type)
    {
      GtkTypeInfo gnc_regWidget_info =
      {
	"GNCRegWidget",
	sizeof (GNCRegWidget),
	sizeof (GNCRegWidgetClass),
	(GtkClassInitFunc) gnc_regWidget_class_init,
	(GtkObjectInitFunc) gnc_regWidget_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL
      };

      gnc_regWidget_type = gtk_type_unique (GTK_TYPE_VBOX, &gnc_regWidget_info);
    }

  return gnc_regWidget_type;
}

/* SIGNALS */
enum gnc_regWidget_signal_enum {
  ENTER_ENT_SIGNAL,
  CANCEL_ENT_SIGNAL,
  DELETE_ENT_SIGNAL,
  DUP_ENT_SIGNAL,
  SCHEDULE_ENT_SIGNAL,
  EXPAND_ENT_SIGNAL,
  BLANK_SIGNAL,
  JUMP_SIGNAL,
#if 0
  ACTIVATE_CURSOR_SIGNAL,
  REDRAW_ALL_SIGNAL,
  REDRAW_HELP_SIGNAL,
#endif /* 0 */
  LAST_SIGNAL
};

static gint gnc_regWidget_signals[LAST_SIGNAL] = { 0 };

static void
gnc_regWidget_class_init (GNCRegWidgetClass *class)
{
  int i;
  GtkObjectClass *object_class;
  static struct similar_signal_info {
    enum gnc_regWidget_signal_enum s;
    const char *signal_name;
    guint offset;
  } signals[] = {
    { ENTER_ENT_SIGNAL, "enter_ent", GTK_SIGNAL_OFFSET( GNCRegWidgetClass, enter_ent_cb ) },
    { CANCEL_ENT_SIGNAL, "cancel_ent", GTK_SIGNAL_OFFSET( GNCRegWidgetClass, cancel_ent_cb ) },
    { DELETE_ENT_SIGNAL, "delete_ent", GTK_SIGNAL_OFFSET( GNCRegWidgetClass, delete_ent_cb ) },
    { DUP_ENT_SIGNAL, "dup_ent", GTK_SIGNAL_OFFSET( GNCRegWidgetClass, dup_ent_cb ) },
    { SCHEDULE_ENT_SIGNAL, "schedule_ent", GTK_SIGNAL_OFFSET( GNCRegWidgetClass, schedule_ent_cb ) },
    { EXPAND_ENT_SIGNAL, "expand_ent", GTK_SIGNAL_OFFSET( GNCRegWidgetClass, expand_ent_cb ) },
    { BLANK_SIGNAL, "blank", GTK_SIGNAL_OFFSET( GNCRegWidgetClass, blank_cb ) },
    { JUMP_SIGNAL, "jump", GTK_SIGNAL_OFFSET( GNCRegWidgetClass, jump_cb ) },
#if 0
    { ACTIVATE_CURSOR_SIGNAL, "activate_cursor", GTK_SIGNAL_OFFSET( GNCRegWidgetClass, activate_cursor ) },
    { REDRAW_ALL_SIGNAL, "redraw_all", GTK_SIGNAL_OFFSET( GNCRegWidgetClass, redraw_all ) },
    { REDRAW_HELP_SIGNAL, "redraw_help", GTK_SIGNAL_OFFSET( GNCRegWidgetClass, redraw_help ) },
#endif /* 0 */
    { LAST_SIGNAL, NULL, 0 }
  };

  object_class = (GtkObjectClass*) class;

  for ( i=0; signals[i].signal_name != NULL; i++ ) {
    gnc_regWidget_signals[ signals[i].s ] =
      gtk_signal_new( signals[i].signal_name,
                      GTK_RUN_FIRST,
                      object_class->type, signals[i].offset,
                      gtk_signal_default_marshaller, GTK_TYPE_NONE, 0 );
  }

  gtk_object_class_add_signals (object_class, gnc_regWidget_signals, LAST_SIGNAL);
}

GtkWidget*
gnc_regWidget_new( GNCLedgerDisplay *ld,
                   GtkWindow *win,
                   int disallowCapabilities )
{
  GNCRegWidget *rw;
  rw = GNC_REGWIDGET( gtk_type_new( gnc_regWidget_get_type() ) );
  rw->disallowedCaps = disallowCapabilities;
  /* IMPORTANT: If we set this to anything other than GTK_RESIZE_QUEUE, we
   * enter into a very bad back-and-forth between the sheet and a containing
   * GnomeDruid [in certain conditions and circumstances not detailed here],
   * resulting in either a single instance of the Druid resizing or infinite
   * instances of the Druid resizing without bound.  Contact
   * jsled@asynchronous.org for details. -- 2002.04.15
   */
  gtk_container_set_resize_mode( GTK_CONTAINER( rw ), GTK_RESIZE_QUEUE );
  gnc_regWidget_init2( rw, ld, win );
  return GTK_WIDGET( rw );
}

/********************************************************************\
 * gnc_register_raise                                               *
 *   raise an existing register window to the front                 *
 *                                                                  *
 * Args:   rw - the register widget data structure                  *
 * Return: nothing                                                  *
\********************************************************************/
static void
gnc_register_raise (GNCRegWidget *rw)
{
  if (rw == NULL)
    return;

  if (rw->window == NULL)
    return;

  gtk_window_present (GTK_WINDOW(rw->window));
}

/********************************************************************\
 * gnc_register_jump_to_split                                       *
 *   move the cursor to the split, if present in register           *
 *                                                                  *
 * Args:   rw - the register widget data structure                  *
 *         split   - the split to jump to                           *
 * Return: nothing                                                  *
\********************************************************************/
static void
gnc_register_jump_to_split(GNCRegWidget *rw, Split *split)
{
  Transaction *trans;
  VirtualCellLocation vcell_loc;
  SplitRegister *reg;

  if (!rw) return;

  trans = xaccSplitGetParent(split);
  if (trans != NULL)
#if 0   /* jsled: If we don't know about dates, what do we do? */
    if (gnc_register_include_date(rw, xaccTransGetDate(trans)))
    {
      gnc_ledger_display_refresh (rw->ledger);
    }
#endif /* 0 -- we don't know about dates.  */

  reg = gnc_ledger_display_get_split_register (rw->ledger);

  if (gnc_split_register_get_split_virt_loc(reg, split, &vcell_loc))
    gnucash_register_goto_virt_cell(rw->reg, vcell_loc);
}

static void
gnc_register_change_style (GNCRegWidget *rw, SplitRegisterStyle style)
{
  SplitRegister *reg = gnc_ledger_display_get_split_register (rw->ledger);

  if (style == reg->style)
    return;

  gnc_split_register_config (reg, reg->type, style, reg->use_double_line);

  gnc_ledger_display_refresh (rw->ledger);
}

static void
gnc_register_style_ledger_cb (GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  if (!GTK_CHECK_MENU_ITEM (w)->active)
    return;

  gnc_register_change_style (rw, REG_STYLE_LEDGER);
}

static void
gnc_register_style_auto_ledger_cb (GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  if (!GTK_CHECK_MENU_ITEM (w)->active)
    return;

  gnc_register_change_style (rw, REG_STYLE_AUTO_LEDGER);
}

static void
gnc_register_style_journal_cb (GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  if (!GTK_CHECK_MENU_ITEM (w)->active)
    return;

  gnc_register_change_style (rw, REG_STYLE_JOURNAL);
}

static void
gnc_register_double_line_cb (GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;
  SplitRegister *reg = gnc_ledger_display_get_split_register (rw->ledger);
  gboolean use_double_line;

  use_double_line = GTK_CHECK_MENU_ITEM(w)->active;

  if (use_double_line == reg->use_double_line)
    return;

  gnc_split_register_config (reg, reg->type, reg->style, use_double_line);

  gnc_ledger_display_refresh (rw->ledger);
}

static void
gnc_register_sort (GNCRegWidget *rw, sort_type_t sort_code)
{
  Query *query = gnc_ledger_display_get_query (rw->ledger);
  gboolean show_present_divider = FALSE;
  GSList *p1 = NULL, *p2 = NULL, *p3 = NULL, *standard;
  SplitRegister *reg;

  if (rw->sort_type == sort_code)
    return;

  standard = g_slist_prepend (NULL, QUERY_DEFAULT_SORT);

  switch (sort_code)
  {
    case BY_STANDARD:
      p1 = standard;
      show_present_divider = TRUE;
      break;
    case BY_DATE:
      p1 = g_slist_prepend (p1, TRANS_DATE_POSTED);
      p1 = g_slist_prepend (p1, SPLIT_TRANS);
      p2 = standard;
      show_present_divider = TRUE;
      break;
    case BY_DATE_ENTERED:
      p1 = g_slist_prepend (p1, TRANS_DATE_ENTERED);
      p1 = g_slist_prepend (p1, SPLIT_TRANS);
      p2 = standard;
      break;
    case BY_DATE_RECONCILED:
      p1 = g_slist_prepend (p1, SPLIT_RECONCILE);
      p2 = g_slist_prepend (p2, SPLIT_DATE_RECONCILED);
      p3 = standard;
      break;
    case BY_NUM:
      p1 = g_slist_prepend (p1, TRANS_NUM);
      p1 = g_slist_prepend (p1, SPLIT_TRANS);
      p2 = standard;
      break;
    case BY_AMOUNT:
      p1 = g_slist_prepend (p1, SPLIT_VALUE);
      p2 = standard;
      break;
    case BY_MEMO:
      p1 = g_slist_prepend (p1, SPLIT_MEMO);
      p2 = standard;
      break;
    case BY_DESC:
      p1 = g_slist_prepend (p1, TRANS_DESCRIPTION);
      p1 = g_slist_prepend (p1, SPLIT_TRANS);
      p2 = standard;
      break;
    default:
      g_slist_free (standard);
      g_return_if_fail (FALSE);
  }

  gncQuerySetSortOrder (query, p1, p2, p3);

  reg = gnc_ledger_display_get_split_register (rw->ledger);

  gnc_split_register_show_present_divider (reg, show_present_divider);

  rw->sort_type = sort_code;

  gnc_ledger_display_refresh(rw->ledger);
}

static void
gnc_register_sort_standard_cb(GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  gnc_register_sort(rw, BY_STANDARD);
}

static void
gnc_register_sort_date_cb(GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  gnc_register_sort(rw, BY_DATE);
}

static void
gnc_register_sort_date_entered_cb(GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  gnc_register_sort(rw, BY_DATE_ENTERED);
}

static void
gnc_register_sort_date_reconciled_cb(GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  gnc_register_sort(rw, BY_DATE_RECONCILED);
}

static void
gnc_register_sort_num_cb(GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  gnc_register_sort(rw, BY_NUM);
}

static void
gnc_register_sort_amount_cb(GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  gnc_register_sort(rw, BY_AMOUNT);
}

static void
gnc_register_sort_memo_cb(GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  gnc_register_sort(rw, BY_MEMO);
}

static void
gnc_register_sort_desc_cb(GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  gnc_register_sort(rw, BY_DESC);
}

static GtkWidget *
gnc_register_create_tool_bar ( GNCRegWidget *rw )
{
  GtkWidget *toolbar;

  static GnomeUIInfo toolbar_info[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("Enter"),
      N_("Record the current transaction"),
      emit_enter_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_ADD,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Cancel"),
      N_("Cancel the current transaction"),
      emit_cancel_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_UNDELETE,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Delete"),
      N_("Delete the current transaction"),
      emit_delete_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_TRASH,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("Duplicate"),
      N_("Make a copy of the current transaction for editing"),
      emit_dup_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_COPY,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Schedule..."),
      N_("Create a Schedule Transaction with the current "
         "transaction as a template"),
      emit_schedule_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_COPY,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_TOGGLEITEM,
      N_("Split"),
      N_("Show all splits in the current transaction"),
      emit_expand_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_BOOK_OPEN,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Blank"),
      N_("Move to the blank transaction at the "
         "bottom of the register"),
      emit_blank_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_NEW,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Jump"),
      N_("Jump to the corresponding transaction in "
         "the other account"),
      emit_jump_cb, NULL, NULL,
      GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_JUMP_TO,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);

  gnome_app_fill_toolbar_with_data (GTK_TOOLBAR(toolbar), toolbar_info,
                                      NULL, rw);

  if ( rw->disallowedCaps & CAP_DELETE ) {
          gtk_widget_set_sensitive( toolbar_info[2].widget, FALSE );
  }
  if ( rw->disallowedCaps & CAP_SCHEDULE ) {
          gtk_widget_set_sensitive( toolbar_info[5].widget, FALSE );
  }
  if ( rw->disallowedCaps & CAP_JUMP ) {
          gtk_widget_set_sensitive( toolbar_info[9].widget, FALSE );
  }

  rw->split_button = toolbar_info[7].widget;

  /* Attach tooltips to the toolbar buttons */
  {
          int i;
          GtkTooltips *tips;

          tips = gtk_tooltips_new();

          for ( i=0; toolbar_info[i].type != GNOME_APP_UI_ENDOFINFO; i++ ) {
                  if ( ! toolbar_info[i].widget ) {
                          continue;
                  }
                  gtk_tooltips_set_tip( tips, toolbar_info[i].widget,
                                        toolbar_info[i].hint, NULL );
          }
          
  }

  return toolbar;
}

static GtkWidget *
gnc_regWidget_create_status_bar (GNCRegWidget *rw)
{
  GtkWidget *statusbar;

  statusbar = gnome_appbar_new (FALSE, /* no progress bar */
                                TRUE,  /* has status area */
                                GNOME_PREFERENCES_USER);
  rw->statusbar = statusbar;

  return statusbar;
}

static void
gnc_register_jump_to_blank (GNCRegWidget *rw)
{
  SplitRegister *reg = gnc_ledger_display_get_split_register (rw->ledger);
  VirtualCellLocation vcell_loc;
  Split *blank;

  blank = gnc_split_register_get_blank_split (reg);
  if (blank == NULL)
    return;

  if (gnc_split_register_get_split_virt_loc (reg, blank, &vcell_loc))
    gnucash_register_goto_virt_cell (rw->reg, vcell_loc);
}


static void
expand_ent_cb (GNCRegWidget *rw, gpointer data)
{
  gboolean expand;
  SplitRegister *reg;

  reg = gnc_ledger_display_get_split_register (rw->ledger);

#if 0
  /* this isn't true. --jsled */
  expand = GTK_TOGGLE_BUTTON (widget)->active;
#else
  expand = TRUE;
#endif /* 0 */

  gnc_split_register_expand_current_trans (reg, expand);
}

/* default handler --jsled */
static void
new_trans_cb (GNCRegWidget *rw, gpointer data)
{
  SplitRegister *reg;

  reg = gnc_ledger_display_get_split_register (rw->ledger);

  if (gnc_split_register_save (reg, TRUE))
    gnc_split_register_redraw (reg);

  gnc_register_jump_to_blank (rw);
}

static void
emit_enter_ent_cb( GtkWidget *widget, gpointer data )
{
  emit_cb( (GNCRegWidget*)data, "enter_ent", widget );
}

static void
emit_cancel_ent_cb( GtkWidget *widget, gpointer data )
{
  emit_cb( (GNCRegWidget*)data, "cancel_ent", widget );
}

static void
emit_delete_ent_cb( GtkWidget *widget, gpointer data )
{
  emit_cb( (GNCRegWidget*)data, "delete_ent", widget );
}

static void
emit_dup_ent_cb( GtkWidget *widget, gpointer data )
{
  emit_cb( (GNCRegWidget*)data, "dup_ent", widget );
}

static
void
emit_schedule_ent_cb( GtkWidget *widget, gpointer data )
{
  emit_cb( (GNCRegWidget*)data, "schedule_ent", widget );
}

static void
emit_expand_ent_cb( GtkWidget *widget, gpointer data )
{
  emit_cb( (GNCRegWidget*)data, "expand_ent", widget );
}

static void
emit_blank_cb( GtkWidget *widget, gpointer data )
{
  emit_cb( (GNCRegWidget*)data, "blank", widget );
}

static void
emit_jump_cb( GtkWidget *widget, gpointer data )
{
  emit_cb( (GNCRegWidget*)data, "jump", widget );
}

static void
emit_cb( GNCRegWidget *rw, const char *signal_name, gpointer ud )
{
  gtk_signal_emit_by_name( GTK_OBJECT(rw), signal_name, NULL );
}

static void
jump_cb(GNCRegWidget *rw, gpointer data)
{
  RegWindow *regData;
  SplitRegister *reg;
  Account *account;
  Account *leader;
  Split *split;

  reg = gnc_ledger_display_get_split_register (rw->ledger);

  split = gnc_split_register_get_current_split (reg);
  if (split == NULL)
    return;

  account = xaccSplitGetAccount(split);
  if (account == NULL)
    return;

  leader = gnc_ledger_display_leader (rw->ledger);

  if (account == leader)
  {
    split = xaccSplitGetOtherSplit(split);
    if (split == NULL)
      return;

    account = xaccSplitGetAccount(split);
    if (account == NULL)
      return;
    if (account == leader)
      return;
  }

  regData = regWindowSimple(account);
  if (regData == NULL)
    return;

  gnc_register_raise (rw);
  gnc_register_jump_to_split (rw, split);
}

static void
gnc_register_create_menus(GNCRegWidget *rw, GtkWidget *statusbar)
{
  GtkAccelGroup *accel_group;

  static GnomeUIInfo style_list[] =
  {
    GNOMEUIINFO_RADIOITEM_DATA(N_("Basic Ledger"),
                               N_("Show transactions on one or two lines"),
                               gnc_register_style_ledger_cb, NULL, NULL),
    GNOMEUIINFO_RADIOITEM_DATA(N_("Auto-Split Ledger"),
                               N_("Show transactions on one or two lines and "
                                  "expand the current transaction"),
                               gnc_register_style_auto_ledger_cb, NULL, NULL),
    GNOMEUIINFO_RADIOITEM_DATA(N_("Transaction Journal"),
                               N_("Show expanded transactions with all "
                                  "splits"),
                               gnc_register_style_journal_cb, NULL, NULL),
    GNOMEUIINFO_END
  };

  static GnomeUIInfo style_menu[] =
  {
    GNOMEUIINFO_RADIOLIST(style_list),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_TOGGLEITEM(N_("_Double Line"),
                           N_("Show two lines of information for each "
                              "transaction"),
                           gnc_register_double_line_cb, NULL),
    GNOMEUIINFO_END
  };

  static GnomeUIInfo sort_list[] =
  {
    GNOMEUIINFO_RADIOITEM_DATA(N_("Standard order"),
                               N_("Keep normal account order"),
                               gnc_register_sort_standard_cb, NULL, NULL),
    GNOMEUIINFO_RADIOITEM_DATA(N_("Sort by Date"),
                               N_("Sort by Date"),
                               gnc_register_sort_date_cb, NULL, NULL),
    GNOMEUIINFO_RADIOITEM_DATA(N_("Sort by date of entry"),
                               N_("Sort by the date of entry"),
                               gnc_register_sort_date_entered_cb,
                               NULL, NULL),
    GNOMEUIINFO_RADIOITEM_DATA(N_("Sort by statement date"),
                               N_("Sort by the statement date "
                                  "(unreconciled items last)"),
                               gnc_register_sort_date_reconciled_cb,
                               NULL, NULL),
    GNOMEUIINFO_RADIOITEM_DATA(N_("Sort by Num"),
                               N_("Sort by Num"),
                               gnc_register_sort_num_cb, NULL, NULL),
    GNOMEUIINFO_RADIOITEM_DATA(N_("Sort by Amount"),
                               N_("Sort by Amount"),
                               gnc_register_sort_amount_cb, NULL, NULL),
    GNOMEUIINFO_RADIOITEM_DATA(N_("Sort by Memo"),
                               N_("Sort by Memo"),
                               gnc_register_sort_memo_cb, NULL, NULL),
    GNOMEUIINFO_RADIOITEM_DATA(N_("Sort by Description"),
                               N_("Sort by Description"),
                               gnc_register_sort_desc_cb, NULL, NULL),
    GNOMEUIINFO_END
  };

  static GnomeUIInfo sort_menu[] =
  {
    GNOMEUIINFO_RADIOLIST(sort_list),
    GNOMEUIINFO_END
  };

  static GnomeUIInfo edit_menu[] =
  {
    GNOMEUIINFO_MENU_CUT_ITEM(cutCB, NULL),
    GNOMEUIINFO_MENU_COPY_ITEM(copyCB, NULL),
    GNOMEUIINFO_MENU_PASTE_ITEM(pasteCB, NULL),
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("Cut Transaction"),
      N_("Cut the selected transaction"),
      cutTransCB, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Copy Transaction"),
      N_("Copy the selected transaction"),
      copyTransCB, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Paste Transaction"),
      N_("Paste the transaction clipboard"),
      pasteTransCB, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  static GnomeUIInfo txn_menu[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("_Enter"),
      N_("Record the current transaction"),
      emit_enter_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Cancel"),
      N_("Cancel the current transaction"),
      emit_cancel_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Delete"),
      N_("Delete the selected transaction"),
      emit_delete_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("D_uplicate"),
      N_("Make a copy of the current transaction"),
      emit_dup_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("Schedule..."),
      N_("Create a Scheduled Transaction with the current "
         "transaction as a template"),
      emit_schedule_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_TOGGLEITEM,
      N_("_Split"),
      N_("Show all splits in the current transaction"),
      emit_expand_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Blank"),
      N_("Move to the blank transaction at the "
         "bottom of the register"),
      emit_blank_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Jump"),
      N_("Jump to the corresponding transaction in "
         "the other account"),
      emit_jump_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  accel_group = gtk_accel_group_new();
  gtk_accel_group_attach(accel_group, GTK_OBJECT(rw->window));

  rw->style_menu = gtk_menu_new();
  gnc_fill_menu_with_data( style_menu, rw );
  gnome_app_fill_menu( GTK_MENU_SHELL(rw->style_menu),
                       style_menu,
                       accel_group, TRUE, 0);
  gnome_app_install_appbar_menu_hints( GNOME_APPBAR(rw->statusbar),
                                       style_menu );

  rw->sort_menu = gtk_menu_new();
  gnc_fill_menu_with_data( sort_menu, rw );
  gnome_app_fill_menu( GTK_MENU_SHELL(rw->sort_menu), sort_menu,
                       accel_group, TRUE, 0 );
  gnome_app_install_appbar_menu_hints( GNOME_APPBAR(rw->statusbar),
                                       sort_menu );

  rw->edit_menu = gtk_menu_new();
  gnc_fill_menu_with_data( edit_menu, rw );
  gnome_app_fill_menu( GTK_MENU_SHELL(rw->edit_menu), edit_menu,
                       accel_group, TRUE, 0 );
  gnome_app_install_appbar_menu_hints( GNOME_APPBAR(rw->statusbar),
                                       edit_menu );

  rw->transaction_menu = gtk_menu_new();
  gnc_fill_menu_with_data( txn_menu, rw );
  gnome_app_fill_menu( GTK_MENU_SHELL(rw->transaction_menu), txn_menu,
                       accel_group, TRUE, 0 );
  gnome_app_install_appbar_menu_hints( GNOME_APPBAR(rw->statusbar),
                                       txn_menu );
  if ( rw->disallowedCaps & CAP_DELETE ) {
          gtk_widget_set_sensitive( txn_menu[2].widget, FALSE );
  }
  if ( rw->disallowedCaps & CAP_SCHEDULE ) {
          gtk_widget_set_sensitive( txn_menu[5].widget, FALSE );
  } 
  if ( rw->disallowedCaps & CAP_JUMP ) {
          gtk_widget_set_sensitive( txn_menu[9].widget, FALSE );
  }

  rw->double_line_check = style_menu[2].widget;
  rw->split_menu_check = txn_menu[7].widget;

  /* Make sure the right style radio item is active */
  {
    SplitRegister *reg;
    GtkWidget *widget;
    int index;

    reg = gnc_ledger_display_get_split_register (rw->ledger);

    switch (reg->style)
    {
      default:
        printf( "default\n" );
      case REG_STYLE_LEDGER:
        index = 0;
        break;
      case REG_STYLE_AUTO_LEDGER:
        index = 1;
        break;
      case REG_STYLE_JOURNAL:
        index = 2;
        break;
    }

    /* registers with more than one account can only use journal mode */
    if (reg->type >= NUM_SINGLE_REGISTER_TYPES)
    {
      widget = style_list[0].widget;
      gtk_widget_set_sensitive (widget, FALSE);

      widget = style_list[1].widget;
      gtk_widget_set_sensitive (widget, FALSE);
    }

    widget = style_list[index].widget;
    gtk_signal_handler_block_by_data(GTK_OBJECT(widget), rw);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), TRUE);
    gtk_signal_handler_unblock_by_data(GTK_OBJECT(widget), rw);
  }
}

static GtkWidget *
gnc_register_create_popup_menu (GNCRegWidget *rw)
{
  GtkWidget *popup;

  GnomeUIInfo transaction_menu[] =
  {
    {
      GNOME_APP_UI_ITEM,
      N_("_Enter"),
      N_("Record the current transaction"),
      emit_enter_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Cancel"),
      N_("Cancel the current transaction"),
      emit_cancel_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Delete"),
      N_("Delete the current transaction"),
      emit_delete_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_ITEM,
      N_("D_uplicate"),
      N_("Make a copy of the current transaction"),
      emit_dup_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Schedule..."), 
      N_("Create a Scheduled Transaction using the current "
         "transaction as a template"),
      emit_schedule_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL, 
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    {
      GNOME_APP_UI_TOGGLEITEM,
      N_("_Split"),
      N_("Show all Stransaction in the current transaction"),
      emit_expand_ent_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Blank"),
      N_("Move to the blank transaction at the "
         "bottom of the register"),
      emit_blank_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    {
      GNOME_APP_UI_ITEM,
      N_("_Jump"),
      N_("Jump to the corresponding transaction in "
         "the other account."),
      emit_jump_cb, NULL, NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };

  gnc_fill_menu_with_data (transaction_menu, rw);

  popup = gnome_popup_menu_new (transaction_menu);

  if ( rw->disallowedCaps & CAP_DELETE ) {
          gtk_widget_set_sensitive( transaction_menu[2].widget, FALSE );
  }
  if ( rw->disallowedCaps & CAP_SCHEDULE ) {
          gtk_widget_set_sensitive( transaction_menu[5].widget, FALSE );
  }
  if ( rw->disallowedCaps & CAP_JUMP ) {
          gtk_widget_set_sensitive( transaction_menu[9].widget, FALSE );
  }

  rw->split_popup_check = transaction_menu[7].widget;

  gnome_app_install_appbar_menu_hints( GNOME_APPBAR(rw->statusbar),
                                       transaction_menu );

  return popup;
}

static void
gnc_register_record (GNCRegWidget *rw)
{
  SplitRegister *reg;
  Transaction *trans;

  reg = gnc_ledger_display_get_split_register (rw->ledger);

  trans = gnc_split_register_get_current_trans (reg);

  if (!gnc_split_register_save (reg, TRUE))
    return;

  gnc_ledger_display_refresh( rw->ledger );
}

static gboolean
gnc_register_match_trans_row (VirtualLocation virt_loc,
                              gpointer user_data)
{
  GNCRegWidget *rw = user_data;
  CursorClass cursor_class;
  SplitRegister *sr;

  sr = gnc_ledger_display_get_split_register (rw->ledger);
  cursor_class = gnc_split_register_get_cursor_class (sr, virt_loc.vcell_loc);

  return (cursor_class == CURSOR_CLASS_TRANS);
}

static void
gnc_register_goto_next_trans_row (GNCRegWidget *rw)
{
  gnucash_register_goto_next_matching_row (rw->reg,
                                           gnc_register_match_trans_row,
                                           rw);
}

static void
gnc_register_enter (GNCRegWidget *rw, gboolean next_transaction)
{
  SplitRegister *sr = gnc_ledger_display_get_split_register (rw->ledger);
  gboolean goto_blank;

  goto_blank = gnc_lookup_boolean_option("Register",
                                         "'Enter' moves to blank transaction",
                                         FALSE);

  /* If we are in single or double line mode and we hit enter
   * on the blank split, go to the blank split instead of the
   * next row. This prevents the cursor from jumping around
   * when you are entering transactions. */
  if (!goto_blank && !next_transaction)
  {
    SplitRegisterStyle style = sr->style;

    if (style == REG_STYLE_LEDGER)
    {
      Split *blank_split;

      blank_split = gnc_split_register_get_blank_split(sr);
      if (blank_split != NULL)
      {
        Split *current_split;

        current_split = gnc_split_register_get_current_split(sr);

        if (blank_split == current_split)
          goto_blank = TRUE;
      }
    }
  }

  /* First record the transaction. This will perform a refresh. */
  gnc_register_record (rw);

  if (!goto_blank && next_transaction)
    gnc_split_register_expand_current_trans (sr, FALSE);

  /* Now move. */
  if (goto_blank)
    gnc_register_jump_to_blank (rw);
  else if (next_transaction)
    gnc_register_goto_next_trans_row (rw);
  else
    gnucash_register_goto_next_virt_row (rw->reg);
}

static void
gnc_register_record_cb (GnucashRegister *reg, gpointer data)
{
  GNCRegWidget *rw = data;

  gnc_register_enter (rw, FALSE);
}

static gboolean
gnc_register_delete_cb(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  GNCRegWidget *rw = data;
  gnc_register_check_close (rw);
  gnc_ledger_display_close (rw->ledger);
  return FALSE; /* let the user handle correctly. */
}

static void
gnc_register_destroy_cb(GtkWidget *widget, gpointer data)
{
  GNCRegWidget *rw = GNC_REGWIDGET(widget);
  SCM id;

  gnc_ledger_display_close (rw->ledger);
  id = rw->toolbar_change_callback_id;
  gnc_unregister_option_change_callback_id(id);
}

static gncUIWidget
gnc_register_get_parent(GNCLedgerDisplay *ledger)
{
  GNCRegWidget *rw = gnc_ledger_display_get_user_data (ledger);

  if (rw == NULL)
    return NULL;

  return rw->window;
}

static void
gnc_toolbar_change_cb (void *data)
{
  GNCRegWidget *rw = data;

  gnc_reg_refresh_toolbar (rw);
}

/********************************************************************\
 * WAS: regWindowLedger( GNCLedgerDisplay *ledger );                *
\********************************************************************/
static void 
gnc_regWidget_init( GNCRegWidget *rw )
{
  rw->sort_type = BY_STANDARD;
  rw->width = -1;
  rw->height = -1;
  rw->disallowedCaps = 0;

  gtk_signal_connect (GTK_OBJECT(rw), "destroy",
                      GTK_SIGNAL_FUNC (gnc_register_destroy_cb), NULL);

}

struct foo {
  char * signal_name;
  void (*handler)();
};

static void 
gnc_regWidget_init2( GNCRegWidget *rw, GNCLedgerDisplay *ledger, GtkWindow *win )
{
  static GSList *date_param = NULL;
  static struct foo bar[] = {
    { "enter_ent", recordCB },
    { "cancel_ent", cancelCB },
    { "delete_ent", deleteCB },
    { "dup_ent", duplicateCB },
    { "expand_ent", expand_ent_cb },
    { "blank", new_trans_cb },
    { "jump", jump_cb },
    { NULL, NULL }
  };
  SplitRegister *reg;
  gboolean show_all;
  gboolean has_date;
  int i;

  rw->window = GTK_WIDGET(win);
  rw->ledger = ledger;

  /* attach predefined handlers */
  for ( i = 0; bar[i].signal_name != NULL; i++ ) {
    gtk_signal_connect( GTK_OBJECT(rw),
                        bar[i].signal_name,
                        GTK_SIGNAL_FUNC(bar[i].handler),
                        NULL );
  }

  rw->statusbar = gnc_regWidget_create_status_bar(rw);

  /* The tool bar */
  {
    SCM id;

    rw->toolbar = gnc_register_create_tool_bar(rw);
    gtk_container_set_border_width(GTK_CONTAINER(rw->toolbar), 2);

    id = gnc_register_option_change_callback(gnc_toolbar_change_cb, rw,
                                             "General", "Toolbar Buttons");
    rw->toolbar_change_callback_id = id;
  }

  gnc_register_create_menus( rw, rw->statusbar );
  rw->popup_menu = gnc_register_create_popup_menu (rw);

  reg = gnc_ledger_display_get_split_register (rw->ledger);
  gnc_ledger_display_set_user_data (rw->ledger, rw);

  gtk_signal_connect (GTK_OBJECT(rw->window), "delete-event",
                      GTK_SIGNAL_FUNC (gnc_register_delete_cb), rw);

  gnc_ledger_display_set_handlers (rw->ledger,
                                   regDestroy,
                                   gnc_register_get_parent);

  show_all = gnc_lookup_boolean_option ("Register",
                                        "Show All Transactions",
                                        TRUE);

  if (date_param == NULL) {
    date_param = g_slist_prepend (NULL, TRANS_DATE_POSTED);
    date_param = g_slist_prepend (date_param, SPLIT_TRANS);
  }

  {
    Query *q = gnc_ledger_display_get_query (rw->ledger);

    has_date = gncQueryHasTermType (q, date_param);
  }

  if (has_date)
    show_all = FALSE;

  /* Now that we have a date range, remove any existing
   * maximum on the number of splits returned. */
  xaccQuerySetMaxSplits (gnc_ledger_display_get_query (rw->ledger), -1);

  /* The CreateTable will do the actual gui init, returning a widget */
  {
    GtkWidget *register_widget;

    /* FIXME_jsled: need to pass in caller's request for the number of
     * lines.  For now, we expect the caller to have called
     * 'set_initial_rows' before creating us. */

    register_widget = gnucash_register_new (reg->table);

    gtk_container_add (GTK_CONTAINER(rw), register_widget);

    rw->reg = GNUCASH_REGISTER (register_widget);

    /* do we _really_ need the window? [--jsled]
     *
     * Seems like we do ... some magic regarding gnucash-sheet sizing is
     * going on with the window's width and height. -- 2002.04.14
     */
    GNUCASH_SHEET(rw->reg->sheet)->window = GTK_WIDGET(win);

    /* jsled: we actually want these... */
    gtk_signal_connect (GTK_OBJECT(register_widget), "activate_cursor",
                        GTK_SIGNAL_FUNC(gnc_register_record_cb), rw);
    gtk_signal_connect (GTK_OBJECT(register_widget), "redraw_all",
                        GTK_SIGNAL_FUNC(gnc_register_redraw_all_cb), rw);
    gtk_signal_connect (GTK_OBJECT(register_widget), "redraw_help",
                        GTK_SIGNAL_FUNC(gnc_register_redraw_help_cb), rw);
    gnucash_register_attach_popup (GNUCASH_REGISTER(register_widget),
                                   rw->popup_menu, rw);

    gnc_table_init_gui (register_widget, reg);
  }

#if 0
  /* jsled: something's not right ... enabling this causes segfault...
   * Gtk-CRITICAL **: file gtkstyle.c: line 515 (gtk_style_attach): assertion `window != NULL' failed.
   * Gdk-CRITICAL **: file gdkwindow.c: line 716 (gdk_window_ref): assertion `window != NULL' failed.
   * Gtk-CRITICAL **: file gtkstyle.c: line 515 (gtk_style_attach): assertion `window != NULL' failed.
   */
  {
    gboolean use_double_line;
    GtkCheckMenuItem *check;

    use_double_line = gnc_lookup_boolean_option ("Register",
                                                 "Double Line Mode",
                                                 FALSE);

    /* be sure to initialize the gui elements associated with the cursor */
    gnc_split_register_config (reg, reg->type, reg->style, use_double_line);

    check = GTK_CHECK_MENU_ITEM (rw->double_line_check);

    gtk_signal_handler_block_by_func
      (GTK_OBJECT (check),
       GTK_SIGNAL_FUNC (gnc_register_double_line_cb), rw);

    gtk_check_menu_item_set_active (check, use_double_line);

    gtk_signal_handler_unblock_by_func
      (GTK_OBJECT (check),
       GTK_SIGNAL_FUNC (gnc_register_double_line_cb), rw);
  }
#endif /* 0 */

  gtk_widget_show_all (GTK_WIDGET(rw));

  gnc_split_register_show_present_divider (reg, TRUE);

  gnc_ledger_display_refresh (rw->ledger);
  gnc_reg_refresh_toolbar (rw);
}

static void
gnc_reg_refresh_toolbar (GNCRegWidget *rw)
{
  GtkToolbarStyle tbstyle;

  if ((rw == NULL) || (rw->toolbar == NULL))
    return;

  tbstyle = gnc_get_toolbar_style ();

  gtk_toolbar_set_style (GTK_TOOLBAR (rw->toolbar), tbstyle);
  gtk_widget_show_all( GTK_WIDGET(rw) );
}

/* jsled: deps: window, summary labels,
 * I think we want to re-emit this signal?
 * Or just tell Users they need get at it? */
static void
gnc_register_redraw_all_cb (GnucashRegister *g_reg, gpointer data)
{
  GNCRegWidget *rw = data;
  Account *leader;
  gboolean expand;
  gboolean sensitive;
  SplitRegister *reg;

  leader = gnc_ledger_display_leader (rw->ledger);

  reg = gnc_ledger_display_get_split_register (rw->ledger);

  expand = gnc_split_register_current_trans_expanded (reg);

  gtk_signal_handler_block_by_data
    (GTK_OBJECT (rw->split_button), rw);
  gtk_toggle_button_set_active
    (GTK_TOGGLE_BUTTON (rw->split_button), expand);
  gtk_signal_handler_unblock_by_data
    (GTK_OBJECT (rw->split_button), rw);

  gtk_signal_handler_block_by_data
    (GTK_OBJECT (rw->split_menu_check), rw);
  gtk_check_menu_item_set_active
    (GTK_CHECK_MENU_ITEM (rw->split_menu_check), expand);
  gtk_signal_handler_unblock_by_data
    (GTK_OBJECT (rw->split_menu_check), rw);

  gtk_check_menu_item_set_active
    (GTK_CHECK_MENU_ITEM (rw->split_popup_check), expand);

  sensitive = reg->style == REG_STYLE_LEDGER;

  gtk_widget_set_sensitive (rw->split_button, sensitive);
  gtk_widget_set_sensitive (rw->split_menu_check, sensitive);
  gtk_widget_set_sensitive (rw->split_popup_check, sensitive);
}

static void
gnc_register_redraw_help_cb (GnucashRegister *g_reg, gpointer data)
{
  GNCRegWidget *rw = data;
  SplitRegister *reg;
  const char *status;
  char *help;

  if (!rw)
    return;

  reg = gnc_ledger_display_get_split_register (rw->ledger);

  help = gnc_table_get_help (reg->table);

  status = help ? help : "";

  gnome_appbar_set_default (GNOME_APPBAR(rw->statusbar), status);

  g_free (help);
}

/********************************************************************\
 * regDestroy()
\********************************************************************/

static void
regDestroy (GNCLedgerDisplay *ledger)
{
  GNCRegWidget *rw = gnc_ledger_display_get_user_data (ledger);

  if (rw)
  {
    SplitRegister *reg;

    reg = gnc_ledger_display_get_split_register (ledger);

    if (reg && reg->table)
      gnc_table_save_state (reg->table);

  }
  gnc_ledger_display_set_user_data (ledger, NULL);
}

/********************************************************************\
 * cutCB -- cut the selection to the clipboard                      *
 *                                                                  *
 * Args:    w - the widget that called us                           *
 *       data - the data struct for this register                   *
 * Return: none                                                     *
\********************************************************************/
static void 
cutCB (GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  gnucash_register_cut_clipboard (rw->reg);
}


/********************************************************************\
 * copyCB -- copy the selection to the clipboard                    *
 *                                                                  *
 * Args:    w - the widget that called us                           *
 *       data - the data struct for this register                   *
 * Return: none                                                     *
\********************************************************************/
static void 
copyCB (GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  gnucash_register_copy_clipboard (rw->reg);
}


/********************************************************************\
 * pasteCB -- paste the clipboard to the selection                  *
 *                                                                  *
 * Args:    w - the widget that called us                           *
 *       data - the data struct for this register                   *
 * Return: none                                                     *
\********************************************************************/
static void 
pasteCB (GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  gnucash_register_paste_clipboard (rw->reg);
}


/********************************************************************\
 * cutTransCB -- cut the current transaction to the clipboard       *
 *                                                                  *
 * Args:    w - the widget that called us                           *
 *       data - the data struct for this register                   *
 * Return: none                                                     *
\********************************************************************/
static void
cutTransCB (GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  gnc_split_register_cut_current
    (gnc_ledger_display_get_split_register (rw->ledger));
}


/********************************************************************\
 * copyTransCB -- copy the current transaction to the clipboard     *
 *                                                                  *
 * Args:    w - the widget that called us                           *
 *       data - the data struct for this register                   *
 * Return: none                                                     *
\********************************************************************/
static void
copyTransCB(GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  gnc_split_register_copy_current
    (gnc_ledger_display_get_split_register (rw->ledger));
}


/********************************************************************\
 * pasteTransCB -- paste the transaction clipboard to the selection *
 *                                                                  *
 * Args:    w - the widget that called us                           *
 *       data - the data struct for this register                   *
 * Return: none                                                     *
\********************************************************************/
static void
pasteTransCB (GtkWidget *w, gpointer data)
{
  GNCRegWidget *rw = data;

  gnc_split_register_paste_current
    (gnc_ledger_display_get_split_register (rw->ledger));
}

/********************************************************************\
 * recordCB                                                         *
 *                                                                  *
 * Args:   w    - the widget that called us                         *
 *         data - the data struct for this register                 *
 * Return: none                                                     *
\********************************************************************/
static void
recordCB (GNCRegWidget *rw, gpointer data)
{
  gnc_register_enter (rw, TRUE);
}


static void
gnc_transaction_delete_toggle_cb(GtkToggleButton *button, gpointer data)
{
  GtkWidget *text = gtk_object_get_user_data(GTK_OBJECT(button));
  gchar *s = data;
  gint pos = 0;

  gtk_editable_delete_text(GTK_EDITABLE(text), 0, -1);
  gtk_editable_insert_text(GTK_EDITABLE(text), s, strlen(s), &pos);
}

/* jsled: seems generic enough... should probably be moved to a
 * better/more-generic place. */

static gboolean
trans_has_reconciled_splits (Transaction *trans)
{
  GList *node;

  for (node = xaccTransGetSplitList (trans); node; node = node->next)
  {
    Split *split = node->data;

    switch (xaccSplitGetReconcile (split))
    {
      case YREC:
      case FREC:
        return TRUE;

      default:
        break;
    }
  }

  return FALSE;
}

typedef enum
{
  DELETE_TRANS,
  DELETE_SPLITS,
  DELETE_CANCEL
} DeleteType;

/********************************************************************\
 * gnc_transaction_delete_query                                     *
 *   creates and displays a dialog which asks the user wheter they  *
 *   want to delete a whole transaction, or just a split.           *
 *   It returns a DeleteType code indicating the user's choice.     *
 *                                                                  *
 * Args: parent - the parent window the dialog should use           *
 * Returns: DeleteType choice indicator                             *
 \*******************************************************************/
static DeleteType
gnc_transaction_delete_query (GtkWindow *parent, Transaction *trans)
{
  GtkWidget *dialog;
  GtkWidget *dvbox;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *trans_button;
  GtkWidget *splits_button;
  GtkWidget *text;
  GSList    *group;
  gint       pos = 0;
  gint       result;
  gboolean   reconciled;

  const char *usual = _("This selection will delete the whole "
                        "transaction. This is what you usually want.");
  const char *usual_recn = _("This selection will delete the whole "
                             "transaction.\n\n"
                             "You would be deleting a transaction "
                             "with reconciled splits!");
  const char *warn  = _("Warning: Just deleting all the other splits will "
                        "make your account unbalanced. You probably "
                        "shouldn't do this unless you're going to "
                        "immediately add another split to bring the "
                        "transaction back into balance.");
  const char *warn_recn = _("You would be deleting reconciled splits!");
  const char *cbuf;
  char *buf;

  DeleteType return_value;

  reconciled = trans_has_reconciled_splits (trans);

  dialog = gnome_dialog_new(_("Delete Transaction"),
                            GNOME_STOCK_BUTTON_OK,
                            GNOME_STOCK_BUTTON_CANCEL,
                            NULL);

  gnome_dialog_set_default(GNOME_DIALOG(dialog), 0);
  gnome_dialog_close_hides(GNOME_DIALOG(dialog), TRUE);
  gnome_dialog_set_parent(GNOME_DIALOG(dialog), parent);

  dvbox = GNOME_DIALOG(dialog)->vbox;

  frame = gtk_frame_new(NULL);
  gtk_container_border_width(GTK_CONTAINER(frame), 5);

  vbox = gtk_vbox_new(TRUE, 3);
  gtk_container_border_width(GTK_CONTAINER(vbox), 5);
  gtk_container_add(GTK_CONTAINER(frame), vbox);

  text = gtk_text_new(NULL, NULL);

  trans_button =
    gtk_radio_button_new_with_label(NULL,
                                    _("Delete the whole transaction"));
  gtk_object_set_user_data(GTK_OBJECT(trans_button), text);
  gtk_box_pack_start(GTK_BOX(vbox), trans_button, TRUE, TRUE, 0);

  gtk_signal_connect(GTK_OBJECT(trans_button), "toggled",
                     GTK_SIGNAL_FUNC(gnc_transaction_delete_toggle_cb),
                     (gpointer) (reconciled ? usual_recn : usual));

  group = gtk_radio_button_group(GTK_RADIO_BUTTON(trans_button));
  splits_button =
    gtk_radio_button_new_with_label(group, _("Delete all the other splits"));
  gtk_object_set_user_data(GTK_OBJECT(splits_button), text);
  gtk_box_pack_start(GTK_BOX(vbox), splits_button, TRUE, TRUE, 0);

  if (reconciled)
    buf = g_strconcat (warn, "\n\n", warn_recn, NULL);
  else
    buf = g_strdup (warn);

  gtk_signal_connect(GTK_OBJECT(splits_button), "toggled",
                     GTK_SIGNAL_FUNC(gnc_transaction_delete_toggle_cb),
                     (gpointer) buf);

  gtk_box_pack_start(GTK_BOX(dvbox), frame, TRUE, TRUE, 0);

  cbuf = reconciled ? usual_recn : usual;
  gtk_editable_insert_text(GTK_EDITABLE(text), cbuf, strlen(cbuf), &pos);
  gtk_text_set_line_wrap(GTK_TEXT(text), TRUE);
  gtk_text_set_word_wrap(GTK_TEXT(text), TRUE);
  gtk_text_set_editable(GTK_TEXT(text), FALSE);
  gtk_box_pack_start(GTK_BOX(dvbox), text, FALSE, FALSE, 0);

  gtk_widget_show_all(dvbox);

  result = gnome_dialog_run_and_close(GNOME_DIALOG(dialog));

  g_free (buf);

  if (result != 0)
    return_value = DELETE_CANCEL;
  else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(trans_button)))
    return_value = DELETE_TRANS;
  else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(splits_button)))
    return_value = DELETE_SPLITS;
  else
    return_value = DELETE_CANCEL;

  gtk_widget_destroy(dialog);

  return return_value;
}

/* This is really tied to the SplitRegister, but that's fine for now. */

/********************************************************************\
 * deleteCB                                                         *
 *                                                                  *
 * Args:   widget - the widget that called us                       *
 *         data   - the data struct for this register               *
 * Return: none                                                     *
\********************************************************************/
static void
deleteCB(GNCRegWidget *rw, gpointer data)
{
  SplitRegisterStyle style;
  CursorClass cursor_class;
  SplitRegister *reg;
  Transaction *trans;
  char *buf = NULL;
  Split *split;
  gint result;

  reg = gnc_ledger_display_get_split_register (rw->ledger);

  /* get the current split based on cursor position */
  split = gnc_split_register_get_current_split(reg);
  if (split == NULL)
  {
    gnc_split_register_cancel_cursor_split_changes (reg);
    return;
  }

  trans = xaccSplitGetParent(split);
  style = reg->style;
  cursor_class = gnc_split_register_get_current_cursor_class (reg);

  /* Deleting the blank split just cancels */
  {
    Split *blank_split = gnc_split_register_get_blank_split (reg);

    if (split == blank_split)
    {
      gnc_split_register_cancel_cursor_trans_changes (reg);
      return;
    }
  }

  if (cursor_class == CURSOR_CLASS_NONE)
    return;

  /* On a split cursor, just delete the one split. */
  if (cursor_class == CURSOR_CLASS_SPLIT)
  {
    const char *format = _("Are you sure you want to delete\n   %s\n"
                           "from the transaction\n   %s ?");
    const char *recn_warn = _("You would be deleting a reconciled split!");
    const char *memo;
    const char *desc;
    char recn;

    memo = xaccSplitGetMemo (split);
    memo = (memo && *memo) ? memo : _("(no memo)");

    desc = xaccTransGetDescription (trans);
    desc = (desc && *desc) ? desc : _("(no description)");

    /* ask for user confirmation before performing permanent damage */
    buf = g_strdup_printf (format, memo, desc);

    recn = xaccSplitGetReconcile (split);
    if (recn == YREC || recn == FREC)
    {
      char *new_buf;

      new_buf = g_strconcat (buf, "\n\n", recn_warn, NULL);
      g_free (buf);
      buf = new_buf;
    }

    result = gnc_verify_dialog_parented (rw->window, FALSE, buf);

    g_free(buf);

    if (!result)
      return;

    gnc_split_register_delete_current_split (reg);
    return;
  }

  g_return_if_fail(cursor_class == CURSOR_CLASS_TRANS);

  /* On a transaction cursor with 2 or fewer splits in single or double
   * mode, we just delete the whole transaction, kerblooie */
  if ((xaccTransCountSplits(trans) <= 2) && (style == REG_STYLE_LEDGER))
  {
    const char *message = _("Are you sure you want to delete the current "
                            "transaction?");
    const char *recn_warn = _("You would be deleting a transaction "
                              "with reconciled splits!");
    char *buf;

    if (trans_has_reconciled_splits (trans))
      buf = g_strconcat (message, "\n\n", recn_warn, NULL);
    else
      buf = g_strdup (message);

    result = gnc_verify_dialog_parented (rw->window, FALSE, buf);

    g_free (buf);

    if (!result)
      return;

    gnc_split_register_delete_current_trans (reg);
    return;
  }

  /* At this point we are on a transaction cursor with more than 2 splits
   * or we are on a transaction cursor in multi-line mode or an auto mode.
   * We give the user two choices: delete the whole transaction or delete
   * all the splits except the transaction split. */
  {
    DeleteType del_type;

    del_type = gnc_transaction_delete_query (GTK_WINDOW(rw->window),
                                             trans);

    if (del_type == DELETE_CANCEL)
      return;

    if (del_type == DELETE_TRANS)
    {
      gnc_split_register_delete_current_trans (reg);
      return;
    }

    if (del_type == DELETE_SPLITS)
    {
      gnc_split_register_emtpy_current_trans (reg);
      return;
    }
  }
}

/********************************************************************\
 * duplicateCB                                                      *
 *                                                                  *
 * Args:   widget - the widget that called us                       *
 *         data   - the data struct for this register               *
 * Return: none                                                     *
\********************************************************************/
static void duplicateCB(GNCRegWidget *rw, gpointer data)
{
  gnc_split_register_duplicate_current
    (gnc_ledger_display_get_split_register (rw->ledger));
}


/********************************************************************\
 * cancelCB                                                         *
 *                                                                  *
 * Args:   w    - the widget that called us                         *
 *         data - the data struct for this register                 *
 * Return: none                                                     *
\********************************************************************/
static void
cancelCB(GNCRegWidget *rw, gpointer data)
{
  gnc_split_register_cancel_cursor_trans_changes
    (gnc_ledger_display_get_split_register (rw->ledger));
}


/********************************************************************\
 * gnc_register_check_close                                         *
 *                                                                  *
 * Args:   regData - the data struct for this register              *
 * Return: none                                                     *
\********************************************************************/
static void
gnc_register_check_close(GNCRegWidget *rw)
{
  gboolean pending_changes;
  SplitRegister *reg;

  reg = gnc_ledger_display_get_split_register (rw->ledger);

  pending_changes = gnc_split_register_changed (reg);
  if (pending_changes)
  {
    const char *message = _("The current transaction has been changed.\n"
                            "Would you like to record it?");
    if (gnc_verify_dialog_parented(rw->window, TRUE, message))
      gnc_register_enter(rw, TRUE);
    else
      gnc_split_register_cancel_cursor_trans_changes (reg);
  }
}

GtkWidget*
gnc_regWidget_get_style_menu( GNCRegWidget *rw )
{
  return rw->style_menu;
}

GtkWidget*
gnc_regWidget_get_sort_menu( GNCRegWidget *rw )
{
  return rw->sort_menu;
}

GtkWidget*
gnc_regWidget_get_edit_menu( GNCRegWidget *rw )
{
  return rw->edit_menu;
}

GtkWidget*
gnc_regWidget_get_transaction_menu( GNCRegWidget *rw )
{
  return rw->transaction_menu;
}

GtkWidget*
gnc_regWidget_get_toolbar( GNCRegWidget *rw )
{
  return rw->toolbar;
}

GtkWidget*
gnc_regWidget_get_statusbar( GNCRegWidget *rw )
{
  return rw->statusbar;
}

GtkWidget*
gnc_regWidget_get_popup( GNCRegWidget *rw )
{
  return rw->popup_menu;
}

GnucashRegister*
gnc_regWidget_get_register( GNCRegWidget *rw )
{
  return rw->reg;
}
