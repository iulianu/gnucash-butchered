/********************************************************************\
 * dialog-filebox.c -- the file dialog box                          *
 * Copyright (C) 1997 Robin D. Clark <rclark@cs.hmc.edu>            *
 * Copyright (C) 1998-99 Rob Browning <rlb@cs.utexas.edu>           *
 * Copyright (C) 2000 Linas Vepstas  <linas@linas.org>              *
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

#include "config.h"

#include <gnome.h>

#include "FileBox.h"
#include "file-history.h"
#include "messages.h"
#include "gnc-engine-util.h"
#include "gnc-ui.h"


typedef struct _FileBoxInfo FileBoxInfo;
struct _FileBoxInfo
{
  GtkFileSelection *file_box;
  char *file_name;
};

/* Global filebox information */
static FileBoxInfo fb_info = {NULL, NULL};

/* This static indicates the debugging module that this .o belongs to.   */
static short module = MOD_GUI;


/** PROTOTYPES ******************************************************/
static void store_filename(GtkWidget *w, gpointer data);
static void gnc_file_box_close_cb(GtkWidget *w, gpointer data);
static gboolean gnc_file_box_delete_cb(GtkWidget *widget, GdkEvent *event,
				       gpointer user_data);


/********************************************************************\
 * fileBox                                                          * 
 *   Pops up a file selection dialog (either a "Save As" or an      * 
 *   "Open"), and returns the name of the file the user selected.   *
 *   (This function does not return until the user selects a file   * 
 *   or presses "Cancel" or the window manager destroy button)      * 
 *                                                                  * 
 * Args:   title        - the title of the window                   *
 *         filter       - the file filter to use                    * 
 *         default_name - the default name to use                   *
 * Return: containing the name of the file the user selected        *
\********************************************************************/
const char *
fileBox (const char * title, const char * filter, const char *default_name)
{
  const char *last_file;

  ENTER("\n");

  /* Set a default title if nothing was passed in */  
  if (title == NULL)
    title = _("Open");

  if (fb_info.file_name != NULL)
    g_free(fb_info.file_name);

  fb_info.file_box = GTK_FILE_SELECTION(gtk_file_selection_new(title));
  fb_info.file_name = NULL;

  last_file = gnc_history_get_last();

  if (default_name)
    gtk_file_selection_set_filename(fb_info.file_box, default_name);
  else if (last_file)
    gtk_file_selection_set_filename(fb_info.file_box, last_file);

  /* hack alert - this was filtering directory names as well as file 
   * names, so I think we should not do this by default (rgmerk) */
#if 0
  if (filter != NULL)
    gtk_file_selection_complete(fb_info.file_box, filter);
#endif

  gtk_window_set_modal(GTK_WINDOW(fb_info.file_box), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(fb_info.file_box),
			       GTK_WINDOW(gnc_get_ui_data()));

  gtk_signal_connect(GTK_OBJECT(fb_info.file_box->ok_button),
		     "clicked", GTK_SIGNAL_FUNC(store_filename),
		     (gpointer) &fb_info);

  /* Ensure that the dialog box is destroyed when the user clicks a button. */
  gtk_signal_connect(GTK_OBJECT(fb_info.file_box->ok_button),
		     "clicked", GTK_SIGNAL_FUNC(gnc_file_box_close_cb),
		     (gpointer) &fb_info);

  gtk_signal_connect(GTK_OBJECT(fb_info.file_box->cancel_button),
		     "clicked", GTK_SIGNAL_FUNC(gnc_file_box_close_cb),
		     (gpointer) &fb_info);

  gtk_signal_connect(GTK_OBJECT(fb_info.file_box), "delete_event",
		     GTK_SIGNAL_FUNC(gnc_file_box_delete_cb), NULL);

  gtk_signal_connect(GTK_OBJECT(fb_info.file_box), "destroy_event",
		     GTK_SIGNAL_FUNC(gnc_file_box_delete_cb), NULL);

  gtk_widget_show(GTK_WIDGET(fb_info.file_box));

  gtk_main();

  gtk_widget_destroy(GTK_WIDGET(fb_info.file_box));

  LEAVE("\n");

  return fb_info.file_name;
}


/********************************************************************\
 * store_filename                                                   * 
 *   callback that saves the name of the file                       * 
 *                                                                  * 
 * Args:   w - the widget that called us                            * 
 *         data - pointer to filebox info structure                 * 
 * Return: none                                                     * 
\********************************************************************/
static void
store_filename(GtkWidget *w, gpointer data)
{
  FileBoxInfo *fb_info = (FileBoxInfo *) data;
  char *file_name;

  file_name = gtk_file_selection_get_filename(fb_info->file_box);

  fb_info->file_name = g_strdup(file_name);
}

static void
gnc_file_box_close_cb(GtkWidget *w, gpointer data)
{
  gtk_widget_hide(GTK_WIDGET(fb_info.file_box));

  gtk_main_quit();
}

static gboolean
gnc_file_box_delete_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  gtk_widget_hide(GTK_WIDGET(fb_info.file_box));

  gtk_main_quit();

  /* Don't delete the window, we'll handle things ourselves. */
  return TRUE;
}

/* ======================== END OF FILE ======================== */
