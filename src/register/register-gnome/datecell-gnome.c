/********************************************************************\
 * datecell-gnome.c -- implement date cell handler in gnome         *
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
 *                                                                  *
\********************************************************************/

/*
 * FILE: datecell-gnome.c
 *
 * FUNCTION: Implement gnome portion of datecell widget
 *           embedded in a table cell.
 *
 * HISTORY:
 * Copyright (c) 2000 Dave Peticolas <dave@krondo.com>
 */

#include "config.h"

#include <ctype.h>
#include <gnome.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "datecell.h"
#include "dialog-utils.h"
#include "gnc-ui-util.h"
#include "gnucash-date-picker.h"
#include "gnucash-item-edit.h"
#include "gnucash-sheet.h"


#define DATE_BUF 30

typedef struct _PopBox
{
  GnucashSheet  *sheet;
  ItemEdit      *item_edit;
  GNCDatePicker *date_picker;

  gboolean signals_connected; /* date picker signals connected? */
  gboolean calendar_popped;   /* calendar is popped up? */
  gboolean in_date_select;

  struct tm date;
} PopBox;


static void block_picker_signals (DateCell *cell);
static void unblock_picker_signals (DateCell *cell);
static void gnc_date_cell_realize (BasicCell *bcell, gpointer w);
static void gnc_date_cell_set_value_internal (BasicCell *bcell,
                                              const char *value);
static void gnc_date_cell_move (BasicCell *bcell);
static void gnc_date_cell_gui_destroy (BasicCell *bcell);
static void gnc_date_cell_destroy (BasicCell *bcell);
static void gnc_date_cell_modify_verify (BasicCell *_cell,
                                         const GdkWChar *change,
                                         int change_len,
                                         const GdkWChar *newval,
                                         int newval_len,
                                         int *cursor_position,
                                         int *start_selection,
                                         int *end_selection);
static gboolean gnc_date_cell_direct_update (BasicCell *bcell,
                                             int *cursor_position,
                                             int *start_selection,
                                             int *end_selection,
                                             void *gui_data);
static gboolean gnc_date_cell_enter (BasicCell *bcell,
                                     int *cursor_position,
                                     int *start_selection,
                                     int *end_selection);
static void gnc_date_cell_leave (BasicCell *bcell);


static void
gnc_parse_date (struct tm *parsed, const char * datestr)
{
  int day, month, year;

  if (!parsed) return;
  if (!datestr) return;

  scanDate (datestr, &day, &month, &year);

  parsed->tm_mday = day;
  parsed->tm_mon  = month - 1;
  parsed->tm_year = year - 1900;

  gnc_tm_set_day_start(parsed);
  if (mktime (parsed) == -1)
    gnc_tm_get_today_start (parsed);
  mktime (parsed);
}

static void
gnc_date_cell_print_date (DateCell *cell, char *buff)
{
  PopBox *box = cell->cell.gui_private;

  printDate (buff,
             box->date.tm_mday,
             box->date.tm_mon + 1,
             box->date.tm_year+1900);
}

static void
gnc_date_cell_init (DateCell *cell)
{
  PopBox *box;
  time_t secs;
  char buff[DATE_BUF];

  gnc_basic_cell_init (&(cell->cell));

  cell->cell.is_popup = TRUE;

  cell->cell.destroy = gnc_date_cell_destroy;

  cell->cell.gui_realize = gnc_date_cell_realize;
  cell->cell.gui_destroy = gnc_date_cell_gui_destroy;
  cell->cell.modify_verify = gnc_date_cell_modify_verify;
  cell->cell.direct_update = gnc_date_cell_direct_update;
  cell->cell.set_value = gnc_date_cell_set_value_internal;

  box = g_new0 (PopBox, 1);

  box->sheet = NULL;
  box->item_edit = NULL;
  box->date_picker = NULL;

  box->signals_connected = FALSE;
  box->calendar_popped = FALSE;
  box->in_date_select = FALSE;

  cell->cell.gui_private = box;

  /* default value is today's date */
  time (&secs);
  box->date = *localtime (&secs);
  gnc_date_cell_print_date (cell, buff);

  gnc_basic_cell_set_value_internal (&cell->cell, buff);
}

BasicCell *
gnc_date_cell_new (void)
{
   DateCell *cell;

   cell = g_new0 (DateCell, 1);

   gnc_date_cell_init (cell);

   return &cell->cell;
}

static void
date_picked_cb (GNCDatePicker *gdp, gpointer data)
{
  DateCell *cell = data;
  PopBox *box = cell->cell.gui_private;
  guint day, month, year;
  char buffer[DATE_BUF];

  gtk_calendar_get_date (gdp->calendar, &year, &month, &day);

  printDate (buffer, day, month + 1, year);

  box->in_date_select = TRUE;
  gnucash_sheet_modify_current_cell (box->sheet, buffer);
  box->in_date_select = FALSE;

  item_edit_hide_popup (box->item_edit);
  box->calendar_popped = FALSE;
}

static void
date_selected_cb (GNCDatePicker *gdp, gpointer data)
{
  DateCell *cell = data;
  PopBox *box = cell->cell.gui_private;
  guint day, month, year;
  char buffer[DATE_BUF];

  gtk_calendar_get_date (gdp->calendar, &year, &month, &day);

  printDate (buffer, day, month + 1, year);

  box->in_date_select = TRUE;
  gnucash_sheet_modify_current_cell (box->sheet, buffer);
  box->in_date_select = FALSE;
}

static void
key_press_item_cb (GNCDatePicker *gdp, GdkEventKey *event, gpointer data)
{
  DateCell *cell = data;
  PopBox *box = cell->cell.gui_private;

  switch(event->keyval)
  {
    case GDK_Escape:
      item_edit_hide_popup (box->item_edit);
      box->calendar_popped = FALSE;
      break;

    default:
      gtk_widget_event(GTK_WIDGET (box->sheet), (GdkEvent *) event);
      break;
  }
}

static void
date_picker_disconnect_signals (DateCell *cell)
{
  PopBox *box = cell->cell.gui_private;

  if (!box->signals_connected)
    return;

  if (GTK_OBJECT_DESTROYED (GTK_OBJECT (box->date_picker)))
    return;

  gtk_signal_disconnect_by_data (GTK_OBJECT (box->date_picker), cell);

  box->signals_connected = FALSE;
}

static void
date_picker_connect_signals (DateCell *cell)
{
  PopBox *box = cell->cell.gui_private;

  if (box->signals_connected)
    return;

  if (GTK_OBJECT_DESTROYED (GTK_OBJECT (box->date_picker)))
    return;

  gtk_signal_connect (GTK_OBJECT(box->date_picker), "date_selected",
                      GTK_SIGNAL_FUNC(date_selected_cb), cell);

  gtk_signal_connect(GTK_OBJECT(box->date_picker), "date_picked",
                     GTK_SIGNAL_FUNC(date_picked_cb), cell);

  gtk_signal_connect(GTK_OBJECT(box->date_picker), "key_press_event",
                     GTK_SIGNAL_FUNC(key_press_item_cb), cell);

  box->signals_connected = TRUE;
}

static void
block_picker_signals (DateCell *cell)
{
  PopBox *box = cell->cell.gui_private;

  if (!box->signals_connected)
    return;

  gtk_signal_handler_block_by_data (GTK_OBJECT (box->date_picker), cell);
}

static void
unblock_picker_signals (DateCell *cell)
{
  PopBox *box = cell->cell.gui_private;

  if (!box->signals_connected)
    return;

  gtk_signal_handler_unblock_by_data (GTK_OBJECT (box->date_picker), cell);
}

static void
gnc_date_cell_gui_destroy (BasicCell *bcell)
{
  PopBox *box = bcell->gui_private;
  DateCell *cell = (DateCell *) bcell;

  if (cell->cell.gui_realize == NULL)
  {
    if (box != NULL && box->date_picker != NULL)
    {
      date_picker_disconnect_signals (cell);
      gtk_object_unref (GTK_OBJECT (box->date_picker));
      box->date_picker = NULL;
    }

    /* allow the widget to be shown again */
    cell->cell.gui_realize = gnc_date_cell_realize;
    cell->cell.gui_move = NULL;
    cell->cell.enter_cell = NULL;
    cell->cell.leave_cell = NULL;
    cell->cell.gui_destroy = NULL;
  }
}

static void
gnc_date_cell_destroy (BasicCell *bcell)
{
  DateCell *cell = (DateCell *) bcell;
  PopBox *box = cell->cell.gui_private;

  gnc_date_cell_gui_destroy (&(cell->cell));

  g_free (box);

  cell->cell.gui_private = NULL;
  cell->cell.gui_realize = NULL;
}

void 
gnc_date_cell_set_value (DateCell *cell, int day, int mon, int year)
{
  PopBox *box = cell->cell.gui_private;
  struct tm dada;
  char buff[DATE_BUF];

  dada.tm_mday = day;
  dada.tm_mon  = mon - 1;
  dada.tm_year = year - 1900;

  gnc_tm_set_day_start(&dada);
  mktime (&dada);

  box->date.tm_mday = dada.tm_mday;
  box->date.tm_mon  = dada.tm_mon;
  box->date.tm_year = dada.tm_year;

  printDate (buff, dada.tm_mday, dada.tm_mon + 1, dada.tm_year + 1900);

  gnc_basic_cell_set_value_internal (&cell->cell, buff);

  if (!box->date_picker)
    return;

  block_picker_signals (cell);
  gnc_date_picker_set_date (box->date_picker, day, mon - 1, year);
  unblock_picker_signals (cell);
}

void 
gnc_date_cell_set_value_secs (DateCell *cell, time_t secs)
{
  PopBox *box = cell->cell.gui_private;
  char buff[DATE_BUF];
  struct tm * stm;

  stm = localtime (&secs);
  box->date = *stm;

  printDate (buff,
             box->date.tm_mday, 
             box->date.tm_mon + 1, 
             box->date.tm_year + 1900);

  gnc_basic_cell_set_value_internal (&cell->cell, buff);

  if (!box->date_picker)
    return;

  block_picker_signals (cell);
  gnc_date_picker_set_date (box->date_picker,
                            box->date.tm_mday,
                            box->date.tm_mon,
                            box->date.tm_year + 1900);
  unblock_picker_signals (cell);
}

void
gnc_date_cell_commit (DateCell *cell)
{
  PopBox *box = cell->cell.gui_private;
  char buff[DATE_BUF];

  if (!cell)
    return;

  gnc_parse_date (&(box->date), cell->cell.value);

  printDate (buff,
             box->date.tm_mday, 
             box->date.tm_mon + 1,
             box->date.tm_year + 1900);

  gnc_basic_cell_set_value_internal (&cell->cell, buff);

  if (!box->date_picker)
    return;

  block_picker_signals (cell);
  gnc_date_picker_set_date (box->date_picker,
                            box->date.tm_mday,
                            box->date.tm_mon,
                            box->date.tm_year + 1900);
  unblock_picker_signals (cell);
}

static gboolean
gnc_date_cell_direct_update (BasicCell *bcell,
                             int *cursor_position,
                             int *start_selection,
                             int *end_selection,
                             void *gui_data)
{
  DateCell *cell = (DateCell *) bcell;
  PopBox *box = cell->cell.gui_private;
  GdkEventKey *event = gui_data;
  char buff[DATE_BUF];

  if (!gnc_handle_date_accelerator (event, &(box->date), bcell->value))
    return FALSE;

  printDate (buff,
             box->date.tm_mday,
             box->date.tm_mon + 1,
             box->date.tm_year + 1900);

  gnc_basic_cell_set_value_internal (&cell->cell, buff);

  *start_selection = 0;
  *end_selection = -1;

  if (!box->date_picker)
    return TRUE;

  block_picker_signals (cell);
  gnc_date_picker_set_date (box->date_picker,
                            box->date.tm_mday,
                            box->date.tm_mon,
                            box->date.tm_year + 1900);
  unblock_picker_signals (cell);

  return TRUE;
}

static void
gnc_date_cell_modify_verify (BasicCell *_cell,
                             const GdkWChar *change,
                             int change_len,
                             const GdkWChar *newval,
                             int newval_len,
                             int *cursor_position,
                             int *start_selection,
                             int *end_selection)
{
  DateCell *cell = (DateCell *) _cell;
  PopBox *box = cell->cell.gui_private;
  gboolean accept = FALSE;

  if (box->in_date_select)
  {
    char *newval_mb = gnc_wcstombs (newval);
    gnc_basic_cell_set_value (_cell, newval_mb);
    g_free (newval_mb);
    return;
  }

  /* if user hit backspace, accept the change */
  if (change == NULL)
    accept = TRUE;
  else if (change_len == 0)
    accept = TRUE;
  else
  {
    int i, count = 0;
    char separator = dateSeparator ();
    gboolean ok = TRUE;

    for (i = 0; i < change_len; i++)
    {
      /* accept only numbers or a date separator. Note that the
       * separator of '-' (for DATE_FORMAT_ISO) takes precedence
       * over the accelerator below! */
      if (!isdigit (change[i]) && (separator != change[i]))
        ok = FALSE;

      if (separator == change[i])
        count++;
    }

    for (i = 0; i < _cell->value_len; i++)
      if (separator == _cell->value_w[i])
        count++;

    if (2 < count)
      ok = FALSE;

    if (ok)
      accept = TRUE;
  }

  /* keep a copy of the new value */
  if (accept)
  {
    char *newval_mb = gnc_wcstombs (newval);

    gnc_basic_cell_set_wcvalue_internal (&cell->cell, newval);
    gnc_parse_date (&(box->date), newval_mb);
    g_free (newval_mb);

    if (!box->date_picker)
      return;

    block_picker_signals (cell);
    gnc_date_picker_set_date (box->date_picker,
                              box->date.tm_mday,
                              box->date.tm_mon,
                              box->date.tm_year + 1900);
    unblock_picker_signals (cell);
  }
}

static void
gnc_date_cell_realize (BasicCell *bcell, gpointer data)
{
  GnucashSheet *sheet = data;
  GnomeCanvasItem *item = sheet->item_editor;
  ItemEdit *item_edit = ITEM_EDIT (item);
  DateCell *cell = (DateCell *) bcell;
  PopBox *box = cell->cell.gui_private;

  /* initialize gui-specific, private data */
  box->sheet = sheet;
  box->item_edit = item_edit;
  box->date_picker = item_edit_new_date_picker (box->item_edit);
  gtk_object_ref (GTK_OBJECT(box->date_picker));
  gtk_object_sink (GTK_OBJECT(box->date_picker));

  /* to mark cell as realized, remove the realize method */
  cell->cell.gui_realize = NULL;
  cell->cell.gui_move = gnc_date_cell_move;
  cell->cell.enter_cell = gnc_date_cell_enter;
  cell->cell.leave_cell = gnc_date_cell_leave;
}

static void
gnc_date_cell_move (BasicCell *bcell)
{
  PopBox *box = bcell->gui_private;

  date_picker_disconnect_signals ((DateCell *) bcell);

  item_edit_set_popup (box->item_edit, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL);

  box->calendar_popped = FALSE;
}

static int
get_popup_height (GnomeCanvasItem *item,
                  int space_available,
                  int row_height,
                  gpointer user_data)
{
  GtkWidget *cal = GTK_WIDGET (GNC_DATE_PICKER (item)->calendar);
  GtkRequisition req;

  req.height = 0;
  req.width = 0;

  gtk_widget_size_request (cal, &req);

  return req.height;
}

static void
popup_set_focus (GnomeCanvasItem *item,
                 gpointer user_data)
{
  gtk_widget_grab_focus (GTK_WIDGET (GNC_DATE_PICKER (item)->calendar));
}

static gboolean
gnc_date_cell_enter (BasicCell *bcell,
                     int *cursor_position,
                     int *start_selection,
                     int *end_selection)
{
  DateCell *cell = (DateCell *) bcell;
  PopBox *box = bcell->gui_private;

  item_edit_set_popup (box->item_edit, GNOME_CANVAS_ITEM (box->date_picker),
                       get_popup_height, NULL, popup_set_focus,
                       NULL, NULL, NULL);

  block_picker_signals (cell);
  gnc_date_picker_set_date (box->date_picker,
                            box->date.tm_mday,
                            box->date.tm_mon,
                            box->date.tm_year + 1900);
  unblock_picker_signals (cell);

  date_picker_connect_signals ((DateCell *) bcell);

  *start_selection = 0;
  *end_selection = -1;

  return TRUE;
}

static void
gnc_date_cell_leave (BasicCell *bcell)
{
  Timespec ts;
  PopBox *box = bcell->gui_private;

  date_picker_disconnect_signals ((DateCell *) bcell);

  item_edit_set_popup (box->item_edit, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL);

  box->calendar_popped = FALSE;

  /* Refresh the date to expand any shortcuts. */
  gnc_date_cell_get_date ((DateCell *)bcell, &ts);
  gnc_date_cell_set_value_secs ((DateCell *)bcell, ts.tv_sec);
}

void
gnc_date_cell_get_date (DateCell *cell, Timespec *ts)
{
  PopBox *box = cell->cell.gui_private;

  if (!cell || !ts)
    return;

  gnc_parse_date (&(box->date), cell->cell.value);

  ts->tv_sec = mktime (&box->date);
  ts->tv_nsec = 0;
}

static void 
gnc_date_cell_set_value_internal (BasicCell *_cell, const char *str)
{
  DateCell *cell = (DateCell *) _cell;
  PopBox *box = cell->cell.gui_private;
  char buff[DATE_BUF];

  gnc_parse_date (&(box->date), str);

  printDate (buff,
             box->date.tm_mday, 
             box->date.tm_mon + 1, 
             box->date.tm_year + 1900);

  gnc_basic_cell_set_value_internal (_cell, buff);

  if (!box->date_picker)
    return;

  block_picker_signals (cell);
  gnc_date_picker_set_date (box->date_picker,
                            box->date.tm_mday,
                            box->date.tm_mon,
                            box->date.tm_year + 1900);
  unblock_picker_signals (cell);
}
