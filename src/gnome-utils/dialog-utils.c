/********************************************************************\
 * dialog-utils.c -- utility functions for creating dialogs         *
 *                   for GnuCash                                    *
 * Copyright (C) 1999-2000 Linas Vepstas                            *
 * Copyright (C) 2005 David Hampton <hampton@employees.org>         *
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

#define _GNU_SOURCE 1  /* necessary to get RTLD_DEFAULT on linux */

#include "config.h"

#include <gnome.h>
#include <glade/glade.h>
#include <gmodule.h>
#include <dlfcn.h>

#include "dialog-utils.h"
#include "gnc-commodity.h"
#include "Group.h"
#include "gnc-dir.h"
#include "gnc-engine.h"
#include "gnc-euro.h"
#include "gnc-ui-util.h"
#include "gnc-gconf-utils.h"

/* This static indicates the debugging module that this .o belongs to. */
static QofLogModule log_module = GNC_MOD_GUI;

#define WINDOW_POSITION		"window_position"
#define WINDOW_GEOMETRY		"window_geometry"

#define DESKTOP_GNOME_INTERFACE "/desktop/gnome/interface"
#define KEY_TOOLBAR_STYLE	"toolbar_style"

/* =========================================================== */

static void
gnc_option_menu_cb(GtkWidget *w, gpointer data)
{
  GNCOptionCallback cb;
  gpointer _index;
  gint index;

  cb = g_object_get_data(G_OBJECT(w), "gnc_option_cb");

  _index = g_object_get_data(G_OBJECT(w), "gnc_option_index");
  index = GPOINTER_TO_INT(_index);

  cb(w, index, data);
}

static void
option_menu_destroy_cb (GtkObject *obj, gpointer data)
{
  GtkTooltips *tips = data;

  g_object_unref (tips);
}

/********************************************************************\
 * gnc_build_option_menu:                                           *
 *   create an GTK "option menu" given the option structure         *
 *                                                                  *
 * Args: option_info - the option structure to use                  *
 *       num_options - the number of options                        *
 * Returns: void                                                    *
 \*******************************************************************/
GtkWidget *
gnc_build_option_menu(GNCOptionInfo *option_info, gint num_options)
{
  GtkTooltips *tooltips;
  GtkWidget *omenu;
  GtkWidget *menu;
  GtkWidget *menu_item;
  gint i;

  omenu = gtk_option_menu_new();
  gtk_widget_show(omenu);

  menu = gtk_menu_new();
  gtk_widget_show(menu);

  tooltips = gtk_tooltips_new();

  g_object_ref (tooltips);
  gtk_object_sink (GTK_OBJECT (tooltips));

  for (i = 0; i < num_options; i++)
  {
    menu_item = gtk_menu_item_new_with_label(option_info[i].name);
    gtk_tooltips_set_tip(tooltips, menu_item, option_info[i].tip, NULL);
    gtk_widget_show(menu_item);

    g_object_set_data(G_OBJECT(menu_item),
		      "gnc_option_cb",
		      option_info[i].callback);

    g_object_set_data(G_OBJECT(menu_item),
		      "gnc_option_index",
		      GINT_TO_POINTER(i));

    g_object_set_data(G_OBJECT(menu_item),
		      "gnc_option_menu",
		      omenu);

    if (option_info[i].callback != NULL)
      g_signal_connect(menu_item, "activate",
		       G_CALLBACK(gnc_option_menu_cb),
		       option_info[i].user_data);

    gtk_menu_append(GTK_MENU(menu), menu_item);
  }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);

  g_signal_connect (omenu, "destroy",
		    G_CALLBACK (option_menu_destroy_cb), tooltips);

  return omenu;
}


/********************************************************************\
 * gnc_get_toolbar_style                                            *
 *   returns the current toolbar style for gnucash toolbars         *
 *                                                                  *
 * Args: none                                                       *
 * Returns: toolbar style                                           *
 \*******************************************************************/
GtkToolbarStyle
gnc_get_toolbar_style(void)
{
  GtkToolbarStyle tbstyle = GTK_TOOLBAR_BOTH;
  char *style_string;

  style_string = gnc_gconf_get_string(GCONF_GENERAL,
				      KEY_TOOLBAR_STYLE, NULL);
  if (!style_string || strcmp(style_string, "system") == 0) {
    if (style_string)
      g_free(style_string);
    style_string = gnc_gconf_get_string(DESKTOP_GNOME_INTERFACE,
					KEY_TOOLBAR_STYLE, NULL);
  }

  if (style_string == NULL)
    return GTK_TOOLBAR_BOTH;
  tbstyle = gnc_enum_from_nick(GTK_TYPE_TOOLBAR_STYLE, style_string,
			       GTK_TOOLBAR_BOTH);
  free(style_string);

  return tbstyle;
}

/********************************************************************\
 * gnc_get_deficit_color                                            *
 *   fill in the 3 color values for the color of deficit values     *
 *                                                                  *
 * Args: color - color structure                                    *
 * Returns: none                                                    *
 \*******************************************************************/
void
gnc_get_deficit_color(GdkColor *color)
{
  color->red   = 50000;
  color->green = 0;
  color->blue  = 0;
}


/********************************************************************\
 * gnc_set_label_color                                              *
 *   sets the color of the label given the value                    *
 *                                                                  *
 * Args: label - gtk label widget                                   *
 *       value - value to use to set color                          *
 * Returns: none                                                    *
 \*******************************************************************/
void
gnc_set_label_color(GtkWidget *label, gnc_numeric value)
{
  gboolean deficit;
  GdkColormap *cm;
  GtkStyle *style;

  if (!gnc_gconf_get_bool(GCONF_GENERAL, "red_for_negative", NULL))
    return;

  cm = gtk_widget_get_colormap(GTK_WIDGET(label));
  gtk_widget_ensure_style(GTK_WIDGET(label));
  style = gtk_widget_get_style(GTK_WIDGET(label));

  style = gtk_style_copy(style);

  deficit = gnc_numeric_negative_p (value);

  if (deficit)
  {
    gnc_get_deficit_color(&style->fg[GTK_STATE_NORMAL]);
    gdk_colormap_alloc_color(cm, &style->fg[GTK_STATE_NORMAL], FALSE, TRUE);
  }
  else
    style->fg[GTK_STATE_NORMAL] = style->black;

  gtk_widget_set_style(label, style);

  g_object_unref(style);
}


/********************************************************************\
 * gnc_restore_window_size                                          *
 *   returns the window size to use for the given option prefix,    *
 *   if window sizes are being saved, otherwise returns 0 for both. *
 *                                                                  *
 * Args: prefix - the option name prefix                            *
 *       width  - pointer to width                                  *
 *       height - pointer to height                                 *
 * Returns: nothing                                                 *
 \*******************************************************************/
void
gnc_restore_window_size(const char *section, GtkWindow *window)
{
  GSList *coord_list;
  gint coords[2];

  g_return_if_fail(section != NULL);
  g_return_if_fail(window != NULL);

  if (!gnc_gconf_get_bool(GCONF_GENERAL, KEY_SAVE_GEOMETRY, NULL))
    return;
  
  coord_list = gnc_gconf_get_list(section, WINDOW_POSITION,
				  GCONF_VALUE_INT, NULL);
  if (coord_list) {
    coords[0] = GPOINTER_TO_INT(g_slist_nth_data(coord_list, 0));
    coords[1] = GPOINTER_TO_INT(g_slist_nth_data(coord_list, 1));
    gtk_window_move(window, coords[0], coords[1]);
    g_slist_free(coord_list);
  }

  coord_list = gnc_gconf_get_list(section, WINDOW_GEOMETRY,
				  GCONF_VALUE_INT, NULL);
  if (coord_list) {
    coords[0] = GPOINTER_TO_INT(g_slist_nth_data(coord_list, 0));
    coords[1] = GPOINTER_TO_INT(g_slist_nth_data(coord_list, 1));
    if ((coords[0] != 0) && (coords[1] != 0))
      gtk_window_resize(window, coords[0], coords[1]);
    g_slist_free(coord_list);
  }
}


/********************************************************************\
 * gnc_save_window_size                                             *
 *   save the window size into options whose names are determined   *
 *   by the string prefix.                                          *
 *                                                                  *
 * Args: prefix - determines the options used to save the values    *
 *       width  - width of the window to save                       *
 *       height - height of the window to save                      *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_save_window_size(const char *section, GtkWindow *window)
{
  GSList *coord_list = NULL;
  gint coords[2];

  g_return_if_fail(section != NULL);
  g_return_if_fail(window != NULL);

  if (GTK_OBJECT_FLAGS(window) & GTK_IN_DESTRUCTION)
    return;

  if (!gnc_gconf_get_bool(GCONF_GENERAL, KEY_SAVE_GEOMETRY, NULL))
    return;

  gtk_window_get_size(GTK_WINDOW(window), &coords[0], &coords[1]);
  coord_list = g_slist_append(coord_list, GUINT_TO_POINTER(coords[0]));
  coord_list = g_slist_append(coord_list, GUINT_TO_POINTER(coords[1]));
  gnc_gconf_set_list(section, WINDOW_GEOMETRY, GCONF_VALUE_INT,
		     coord_list, NULL);
  g_slist_free(coord_list);
  coord_list = NULL;

  gtk_window_get_position(GTK_WINDOW(window), &coords[0], &coords[1]);
  coord_list = g_slist_append(coord_list, GUINT_TO_POINTER(coords[0]));
  coord_list = g_slist_append(coord_list, GUINT_TO_POINTER(coords[1]));
  gnc_gconf_set_list(section, WINDOW_POSITION, GCONF_VALUE_INT,
		     coord_list, NULL);
  g_slist_free(coord_list);
}


/********************************************************************\
 * gnc_fill_menu_with_data                                          *
 *   fill the user data values in the menu structure with the given *
 *   value. The filling is done recursively.                        *
 *                                                                  *
 * Args: info - the menu to fill                                    *
 *       data - the value to fill with                              *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_fill_menu_with_data(GnomeUIInfo *info, gpointer data)
{
  if (info == NULL)
    return;

  while (1)
  {
    switch (info->type)
    {
      case GNOME_APP_UI_RADIOITEMS:
      case GNOME_APP_UI_SUBTREE:
      case GNOME_APP_UI_SUBTREE_STOCK:
        gnc_fill_menu_with_data((GnomeUIInfo *) info->moreinfo, data);
        break;
      case GNOME_APP_UI_ENDOFINFO:
        return;
      default:
        info->user_data = data;
        break;
    }

    info++;
  }
}


void
gnc_option_menu_init(GtkWidget * w)
{
  GtkWidget * menu;
  GtkWidget * active;
  unsigned int i;

  menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(w));

  for(i = 0; i < g_list_length(GTK_MENU_SHELL(menu)->children); i++)
  {
    gtk_option_menu_set_history(GTK_OPTION_MENU(w), i);
    active = gtk_menu_get_active(GTK_MENU(menu));
    g_object_set_data(G_OBJECT(active), 
		      "option_index",
		      GINT_TO_POINTER(i));
  }

  gtk_option_menu_set_history(GTK_OPTION_MENU(w), 0);
}

typedef struct {
  int i;
  GCallback f;
  gpointer cb_data;
} menu_init_data;

static void
gnc_option_menu_set_one_item (gpointer loop_data, gpointer user_data)
{
  GObject *item = G_OBJECT(loop_data);
  menu_init_data *args = (menu_init_data *) user_data;
  
  g_object_set_data(item, "option_index", GINT_TO_POINTER(args->i++));
  g_signal_connect(item, "activate", args->f, args->cb_data);
}


void
gnc_option_menu_init_w_signal(GtkWidget * w, GCallback f, gpointer cb_data)
{
  GtkWidget * menu;
  menu_init_data foo;

  foo.i = 0;
  foo.f = f;
  foo.cb_data = cb_data;

  menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(w));
  g_list_foreach(GTK_MENU_SHELL(menu)->children,
		 gnc_option_menu_set_one_item, &foo);
  gtk_option_menu_set_history(GTK_OPTION_MENU(w), 0);
}


int
gnc_option_menu_get_active(GtkWidget * w)
{
  GtkWidget * menu;
  GtkWidget * menuitem;

  menu     = gtk_option_menu_get_menu(GTK_OPTION_MENU(w));
  menuitem = gtk_menu_get_active(GTK_MENU(menu));

  return GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem),
					   "option_index"));
}


/********************************************************************\
 * gnc_window_adjust_for_screen                                     *
 *   adjust the window size if it is bigger than the screen size.   *
 *                                                                  *
 * Args: window - the window to adjust                              *
 * Returns: nothing                                                 *
\********************************************************************/
void
gnc_window_adjust_for_screen(GtkWindow * window)
{
  gint screen_width;
  gint screen_height;
  gint width;
  gint height;

  if (window == NULL)
    return;

  g_return_if_fail(GTK_IS_WINDOW(window));
  if (GTK_WIDGET(window)->window == NULL)
    return;

  screen_width = gdk_screen_width();
  screen_height = gdk_screen_height();
  gdk_window_get_size(GTK_WIDGET(window)->window, &width, &height);

  if ((width <= screen_width) && (height <= screen_height))
    return;

  width = MIN(width, screen_width - 10);
  width = MAX(width, 0);

  height = MIN(height, screen_height - 10);
  height = MAX(height, 0);

  gdk_window_resize(GTK_WIDGET(window)->window, width, height);
  gtk_widget_queue_resize(GTK_WIDGET(window));
}

gboolean
gnc_handle_date_accelerator (GdkEventKey *event,
                             struct tm *tm,
                             const char *date_str)
{
  GDate gdate;

  g_return_val_if_fail (event != NULL, FALSE);
  g_return_val_if_fail (tm != NULL, FALSE);
  g_return_val_if_fail (date_str != NULL, FALSE);

  if (event->type != GDK_KEY_PRESS)
    return FALSE;

  if ((tm->tm_mday <= 0) || (tm->tm_mon == -1) || (tm->tm_year == -1))
    return FALSE;

  g_date_set_dmy (&gdate, 
                  tm->tm_mday,
                  tm->tm_mon + 1,
                  tm->tm_year + 1900);

  /* 
   * Check those keys where the code does different things depending
   * upon the modifiers.
   */
  switch (event->keyval)
  {
    case GDK_KP_Add:
    case GDK_plus:
    case GDK_equal:
      if (event->state & GDK_SHIFT_MASK)
        g_date_add_days (&gdate, 7);
      else if (event->state & GDK_MOD1_MASK)
        g_date_add_months (&gdate, 1);
      else if (event->state & GDK_CONTROL_MASK)
        g_date_add_years (&gdate, 1);
      else
        g_date_add_days (&gdate, 1);
      g_date_to_struct_tm (&gdate, tm);
      return TRUE;

    case GDK_minus:
      if ((strlen (date_str) != 0) && (dateSeparator () == '-'))
      {
        const char *c;
        gunichar uc;
        int count = 0;

        /* rough check for existing date */
        c = date_str;
        while (*c)
        {
          uc = g_utf8_get_char (c);
          if (uc == '-')
            count++;
          c = g_utf8_next_char (c);          
        }

        if (count < 2)
          return FALSE;
      }

      /* fall through */
    case GDK_KP_Subtract:
    case GDK_underscore:
      if (event->state & GDK_SHIFT_MASK)
        g_date_subtract_days (&gdate, 7);
      else if (event->state & GDK_MOD1_MASK)
        g_date_subtract_months (&gdate, 1);
      else if (event->state & GDK_CONTROL_MASK)
        g_date_subtract_years (&gdate, 1);
      else
        g_date_subtract_days (&gdate, 1);
      g_date_to_struct_tm (&gdate, tm);
      return TRUE;

    default:
      break;
  }

  /*
   * Control and Alt key combinations should be ignored by this
   * routine so that the menu system gets to handle them.  This
   * prevents weird behavior of the menu accelerators (i.e. work in
   * some widgets but not others.)
   */
  if (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK))
    return FALSE;

  /* Now check for the remaining keystrokes. */
  switch (event->keyval)
  {
    case GDK_braceright:
    case GDK_bracketright:
      /* increment month */
      g_date_add_months (&gdate, 1);
      break;

    case GDK_braceleft:
    case GDK_bracketleft:
      /* decrement month */
      g_date_subtract_months (&gdate, 1);
      break;

    case GDK_M:
    case GDK_m:
      /* beginning of month */
      g_date_set_day (&gdate, 1);
      break;

    case GDK_H:
    case GDK_h:
      /* end of month */
      g_date_set_day (&gdate, 1);
      g_date_add_months (&gdate, 1);
      g_date_subtract_days (&gdate, 1);
      break;

    case GDK_Y:
    case GDK_y:
      /* beginning of year */
      g_date_set_day (&gdate, 1);
      g_date_set_month (&gdate, 1);
      break;

    case GDK_R:
    case GDK_r:
      /* end of year */
      g_date_set_day (&gdate, 1);
      g_date_set_month (&gdate, 1);
      g_date_add_years (&gdate, 1);
      g_date_subtract_days (&gdate, 1);
      break;

    case GDK_T:
    case GDK_t:
      {
        /* today */
        GTime gtime;

        gtime = time (NULL);
        g_date_set_time (&gdate, gtime);
        break;
      }

    default:
      return FALSE;
  }

  g_date_to_struct_tm (&gdate, tm);

  return TRUE;
}


typedef struct
{
  int row;
  int col;
  gboolean checked;
} GNCCListCheckNode;

typedef struct
{
  GdkPixmap *on_pixmap;
  GdkPixmap *off_pixmap;
  GdkBitmap *mask;

  GList *pending_checks;
} GNCCListCheckInfo;

static void
free_check_list (GList *list)
{
  GList *node;

  for (node = list; node; node = node->next)
    g_free (node->data);

  g_list_free (list);
}

static void
check_realize (GtkWidget *widget, gpointer user_data)
{
  GNCCListCheckInfo *check_info = user_data;
  GdkGCValues gc_values;
  GtkCList *clist;
  gint font_height;
  gint check_size;
  GdkColormap *cm;
  GtkStyle *style;
  GList *list;
  GList *node;
  GdkGC *gc;
  GdkFont *font;

  if (check_info->mask)
    return;

  style = gtk_widget_get_style (widget);
  font = gdk_font_from_description(style->font_desc);

  font_height = font->ascent + font->descent;
  check_size = (font_height > 0) ? font_height - 3 : 9;

  check_info->mask = gdk_pixmap_new (NULL, check_size, check_size, 1);

  check_info->on_pixmap = gdk_pixmap_new (widget->window,
                                          check_size, check_size, -1);

  check_info->off_pixmap = gdk_pixmap_new (widget->window,
                                           check_size, check_size, -1);

  gc_values.foreground = style->white;
  gc = gtk_gc_get (1, gtk_widget_get_colormap (widget),
                   &gc_values, GDK_GC_FOREGROUND);

  gdk_draw_rectangle (check_info->mask, gc, TRUE,
                      0, 0, check_size, check_size);

  gtk_gc_release (gc);

  gc = style->base_gc[GTK_STATE_NORMAL];

  gdk_draw_rectangle (check_info->on_pixmap, gc, TRUE,
                      0, 0, check_size, check_size);
  gdk_draw_rectangle (check_info->off_pixmap, gc, TRUE,
                      0, 0, check_size, check_size);

  cm = gtk_widget_get_colormap (widget);

  gc_values.foreground.red = 0;
  gc_values.foreground.green = 65535 / 2;
  gc_values.foreground.blue = 0;

  gdk_colormap_alloc_color (cm, &gc_values.foreground, FALSE, TRUE);

  gc = gdk_gc_new_with_values (widget->window, &gc_values, GDK_GC_FOREGROUND);

  gdk_draw_line (check_info->on_pixmap, gc,
                 1, check_size / 2,
                 check_size / 3, check_size - 5);
  gdk_draw_line (check_info->on_pixmap, gc,
                 1, check_size / 2 + 1,
                 check_size / 3, check_size - 4);
        
  gdk_draw_line (check_info->on_pixmap, gc,
                 check_size / 3, check_size - 5,
                 check_size - 3, 2);
  gdk_draw_line (check_info->on_pixmap, gc,
                 check_size / 3, check_size - 4,
                 check_size - 3, 1);

  gdk_gc_unref (gc);

  clist = GTK_CLIST (widget);

  list = check_info->pending_checks;
  check_info->pending_checks = NULL;

  /* reverse so we apply in the order of the calls */
  list = g_list_reverse (list);

  for (node = list; node; node = node->next)
  {
    GNCCListCheckNode *cl_node = node->data;

    gnc_clist_set_check (clist, cl_node->row, cl_node->col, cl_node->checked);
  }

  free_check_list (list);
}

static void
check_unrealize (GtkWidget *widget, gpointer user_data)
{
  GNCCListCheckInfo *check_info = user_data;

  if (check_info->mask)
    gdk_bitmap_unref (check_info->mask);

  if (check_info->on_pixmap)
    gdk_pixmap_unref (check_info->on_pixmap);

  if (check_info->off_pixmap)
    gdk_pixmap_unref (check_info->off_pixmap);

  check_info->mask = NULL;
  check_info->on_pixmap = NULL;
  check_info->off_pixmap = NULL;
}

static void
check_destroy (GtkWidget *widget, gpointer user_data)
{
  GNCCListCheckInfo *check_info = user_data;

  free_check_list (check_info->pending_checks);
  check_info->pending_checks = NULL;

  g_free (check_info);
}

static GNCCListCheckInfo *
gnc_clist_add_check (GtkCList *list)
{
  GNCCListCheckInfo *check_info;
  GObject *object;

  object = G_OBJECT (list);

  check_info = g_object_get_data (object, "gnc-check-info");
  if (check_info)
  {
    PWARN ("clist already has check");
    return check_info;
  }

  check_info = g_new0 (GNCCListCheckInfo, 1);

  g_object_set_data (object, "gnc-check-info", check_info);

  g_signal_connect (object, "realize",
		    G_CALLBACK (check_realize), check_info);
  g_signal_connect (object, "unrealize",
		    G_CALLBACK (check_unrealize), check_info);
  g_signal_connect (object, "destroy",
		    G_CALLBACK (check_destroy), check_info);

  if (GTK_WIDGET_REALIZED (GTK_WIDGET (list)))
    check_realize (GTK_WIDGET (list), check_info);

  return check_info;
}


void
gnc_clist_set_check (GtkCList *list, int row, int col, gboolean checked)
{
  GNCCListCheckInfo *check_info;
  GdkPixmap *pixmap;

  g_return_if_fail (GTK_IS_CLIST (list));

  check_info = g_object_get_data (G_OBJECT (list), "gnc-check-info");
  if (!check_info)
    check_info = gnc_clist_add_check (list);

  if (!GTK_WIDGET_REALIZED (GTK_WIDGET (list)))
  {
    GNCCListCheckNode *node;

    node = g_new0 (GNCCListCheckNode, 1);

    node->row = row;
    node->col = col;
    node->checked = checked;

    check_info->pending_checks =
      g_list_prepend (check_info->pending_checks, node);

    return;
  }

  pixmap = checked ? check_info->on_pixmap : check_info->off_pixmap;

  if (checked)
    gtk_clist_set_pixmap (list, row, col, pixmap, check_info->mask);
  else
    gtk_clist_set_text (list, row, col, "");
}

/*   Glade Stuff
 *
 *
 */

static gboolean glade_inited = FALSE;

/* gnc_glade_xml_new: a convenience wrapper for glade_xml_new
 *   - takes care of glade initialization, if needed
 *   - takes care of finding the directory for glade files
 */
GladeXML *
gnc_glade_xml_new (const char *filename, const char *root)
{
  GladeXML *xml;
  char *fname;

  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (root != NULL, NULL);

  if (!glade_inited)
  {
    glade_gnome_init ();
    glade_inited = TRUE;
  }

  fname = g_strconcat (GNC_GLADE_DIR, "/", filename, (char *)NULL);

  xml = glade_xml_new (fname, root, NULL);

  g_free (fname);

  return xml;
}

/* gnc_glade_lookup_widget:  Given a root (or at least ancestor) widget,
 *   find the child widget with the given name.
 */
GtkWidget *
gnc_glade_lookup_widget (GtkWidget *widget, const char *name)
{
  GladeXML *xml;

  if (!widget || !name) return NULL;

  xml = glade_get_widget_tree (widget);
  if (!xml) return NULL;

  return glade_xml_get_widget (xml, name);
}

/*
 * The following function is built from a couple of glade functions.
 */
GModule *allsymbols = NULL;

void
gnc_glade_autoconnect_full_func(const gchar *handler_name,
				GObject *signal_object,
				const gchar *signal_name,
				const gchar *signal_data,
				GObject *other_object,
				gboolean signal_after,
				gpointer user_data)
{
  GCallback func;
  GCallback *p_func = &func;

  if (allsymbols == NULL) {
    /* get a handle on the main executable -- use this to find symbols */
    allsymbols = g_module_open(NULL, 0);
  }

  if (!g_module_symbol(allsymbols, handler_name, (gpointer *)p_func)) {
    /* Fallback to dlsym -- necessary for *BSD linkers */
    func = dlsym(RTLD_DEFAULT, handler_name);
    if (func == NULL) {
      g_warning("ggaff: could not find signal handler '%s'.", handler_name);
      return;
    }
  }

  if (other_object) {
    if (signal_after)
      g_signal_connect_object (signal_object, signal_name, func,
			       other_object, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
    else
      g_signal_connect_swapped (signal_object, signal_name, func, other_object);
  } else {
    if (signal_after)
      g_signal_connect_after(signal_object, signal_name, func, user_data);
    else
      g_signal_connect(signal_object, signal_name, func, user_data);
  }
}

void
gnc_gtk_dialog_add_button (GtkWidget *dialog, const gchar *label, const gchar *stock_id, guint response)
{
  GtkWidget *button;

  button = gtk_button_new_with_label(label);
#ifdef HAVE_GLIB26
  if (stock_id) {
    GtkWidget *image;

    image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(button), image);
  }
#else
  gtk_button_set_use_underline(GTK_BUTTON(button), TRUE);
#endif
  gtk_widget_show_all(button);
  gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button, response);
}
