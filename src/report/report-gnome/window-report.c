/********************************************************************
 * window-report.c                                                  *
 * Copyright (C) 1997 Robin D. Clark                                *
 * Copyright (C) 1998 Linas Vepstas                                 *
 * Copyright (C) 1999 Jeremy Collins ( gtk-xmhtml port )            *
 * Copyright (C) 2000 Dave Peticolas                                *
 * Copyright (C) 2000 Bill Gribble                                  *
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
 ********************************************************************/

#include "config.h"

#include <gnome.h>
#include <guile/gh.h>
#include <sys/stat.h>

#include <g-wrap-wct.h>

#include "dialog-options.h"
#include "dialog-utils.h"
#include "file-utils.h"
#include "gnc-component-manager.h"
#include "gnc-engine-util.h"
#include "gnc-file-dialog.h"
#include "gnc-gui-query.h"
#include "gnc-html-history.h"
#include "gnc-html.h"
#include "gnc-report.h"
#include "gnc-ui.h"
#include "option-util.h"
#include "window-help.h"
#include "window-report.h"

#define WINDOW_REPORT_CM_CLASS "window-report"

struct gnc_report_window_s
{
  GNCMDIChildInfo * mc; 
  GtkWidget    * container;   

  /* the report that's currently being shown.  For any option change
   * we need to rerun the report */
  SCM          cur_report;      
  GNCOptionDB  * cur_odb;
  SCM          option_change_cb_id;
  
  /* initial_report is special; it's the one that's saved and 
   * restored.  The name_change_callback only gets called when
   * the initial_report name is changed. */
  SCM          initial_report; 
  GNCOptionDB  * initial_odb;
  SCM          name_change_cb_id;
  
  SCM          edited_reports;

  gnc_html     * html;
};


static void gnc_report_window_print_cb(GtkWidget * w, gpointer data);
static void gnc_report_window_reload_cb(GtkWidget * w, gpointer data);

/********************************************************************
 * REPORT WINDOW FUNCTIONS 
 * creating/managing report-window mdi children
 ********************************************************************/

/********************************************************************
 * gnc_report_window_view_labeler
 * label the window/tab/menu item with the report name 
 ********************************************************************/

static GtkWidget * 
gnc_report_window_view_labeler(GnomeMDIChild * child, GtkWidget * current,
                               gpointer user_data)
{
  GNCMDIChildInfo  * rwin = gtk_object_get_user_data(GTK_OBJECT(child));
  gnc_report_window * report;
  SCM    get_name = gh_eval_str("gnc:report-name");
  char   * name = NULL; 
  
  if(rwin) {
    report = rwin->user_data;
    if(report->initial_report != SCM_BOOL_F) {
      name = gh_scm2newstr(gh_call1(get_name, 
                                    report->initial_report),
                           NULL);
    }
    else {
      name = strdup(_("(Report not found)"));
    }
    g_free(rwin->title);
    rwin->title = g_strdup(name);    
  }
  else {
    name = strdup(_("Report"));
  }

  if(current == NULL) {
    current = gtk_label_new(name);    
    free(name);
  }
  else {
    gtk_label_set_text(GTK_LABEL(current), name);
    free(name);
  }
  gtk_misc_set_alignment (GTK_MISC(current), 0.0, 0.5);
  return current;
}

static void
gnc_report_window_view_destroy(GtkObject * obj, gpointer user_data)
{
  GNCMDIChildInfo  * mc = user_data;

  gnc_mdi_remove_child(gnc_mdi_get_current(), mc);
  g_free(mc->toolbar_info);
  g_free(mc->menu_info);
  g_free(mc->title);
  g_free(mc);
}

static void
gnc_report_window_menu_tweaking (GNCMDIChildInfo * mc)
{
  /* Do not i18n these strings!!! */
}

/********************************************************************
 * report_view_new
 * create a new report view.  
 ********************************************************************/

static GtkWidget *
gnc_report_window_view_new(GnomeMDIChild * child, gpointer user_data)
{
  GNCMDIInfo        * maininfo = user_data;
  GNCMDIChildInfo    * mc = g_new0(GNCMDIChildInfo, 1);
  gnc_report_window  * win = gnc_report_window_new(mc);
  URLType            type;
  char               * url_location = NULL;
  char               * url_label = NULL;

  mc->contents     = gnc_report_window_get_container(win);
  mc->app          = NULL;
  mc->toolbar      = NULL;
  mc->user_data    = win;
  mc->child        = child; 
  mc->title        = g_strdup("Report");
  mc->menu_tweaking  = gnc_report_window_menu_tweaking;
  gnc_mdi_add_child (maininfo, mc);

  /* Override a couple of standard buttons. */
  gnc_mdi_set_dispatch_cb(mc, GNC_DISP_PRINT,
			  gnc_report_window_print_cb, win);
  gnc_mdi_set_dispatch_cb(mc, GNC_DISP_REFRESH,
			  gnc_report_window_reload_cb, win);
  type = gnc_html_parse_url(gnc_report_window_get_html(win),
                            child->name, &url_location, &url_label);
  
  gnc_html_show_url(gnc_report_window_get_html(win), 
                    type, url_location, url_label, 0);
  
  gtk_object_set_user_data(GTK_OBJECT(child), mc);
  
  /* make sure menu entry and label get refreshed */
  gnome_mdi_child_set_name(child, child->name);

  gtk_signal_connect(GTK_OBJECT(child), "destroy", 
                     gnc_report_window_view_destroy, mc);
  
  gnc_report_window_create_menu(win, mc); 
  gnc_report_window_create_toolbar(win, mc);
  gnc_mdi_create_child_toolbar(maininfo, mc);
  
  if(mc->menu_info) {
    gnome_mdi_child_set_menu_template(child, mc->menu_info);  
  }
  
  g_free(url_location);
  g_free(url_label);
  return mc->contents;
}


/********************************************************************
 * gnc_report_window_create_child()
 * return an MDI child that will create views of the specified report 
 * (configstring is the report URL)
 ********************************************************************/

GnomeMDIChild * 
gnc_report_window_create_child(const gchar * configstring)
{
  GnomeMDIGenericChild * reportchild = 
    gnome_mdi_generic_child_new(configstring);
  GNCMDIInfo * maininfo = gnc_mdi_get_current();
  
  gnome_mdi_generic_child_set_label_func(reportchild, 
                                         gnc_report_window_view_labeler,
                                         maininfo);
  gnome_mdi_generic_child_set_view_creator(reportchild, 
                                           gnc_report_window_view_new,
                                           maininfo);
  return GNOME_MDI_CHILD(reportchild);
}


/********************************************************************
 * gnc_main_window_open_report()
 * open an report in a top level window from an ID number 
 ********************************************************************/

void
gnc_main_window_open_report(int report_id, gint toplevel)
{
  char * id_name = g_strdup_printf("id=%d", report_id);  
  char * child_name = gnc_build_url (URL_TYPE_REPORT, id_name, NULL);
  g_free (id_name);
  gnc_main_window_open_report_url(child_name, toplevel);
}


void
gnc_main_window_open_report_url(const char * url, gint toplevel)
{
  GnomeMDIChild * reportchild = gnc_report_window_create_child(url);
  GNCMDIInfo   * maininfo = gnc_mdi_get_current();
  
  gnome_mdi_add_child(GNOME_MDI(maininfo->mdi), 
                      GNOME_MDI_CHILD(reportchild));  
  if(toplevel) {
    gnome_mdi_add_toplevel_view(GNOME_MDI(maininfo->mdi), 
                                GNOME_MDI_CHILD(reportchild));
  }
  else {
    GNCMDIChildInfo * childwin;

    gnome_mdi_add_view(GNOME_MDI(maininfo->mdi), 
                       GNOME_MDI_CHILD(reportchild));

    childwin = gtk_object_get_user_data(GTK_OBJECT(reportchild));
    if (childwin && childwin->app && GTK_WIDGET (childwin->app)->window)
      gdk_window_raise (GTK_WIDGET (childwin->app)->window);
  }
}


/********************************************************************
 * gnc_report_window_check_urltype
 * is it OK to show a certain URLType in this window?
 ********************************************************************/

static int
gnc_report_window_check_urltype(URLType t)
{
  if (!safe_strcmp (t, URL_TYPE_REPORT)) {
    return TRUE;
  } else {
    return FALSE;
  }
}


/********************************************************************
 * toolbar callbacks
 ********************************************************************/

static int 
gnc_report_window_fwd_cb(GtkWidget * w, gpointer data)
{
  gnc_report_window     * report = data;
  gnc_html_history_node * node = NULL;

  gnc_html_history_forward(gnc_html_get_history(report->html));
  node = gnc_html_history_get_current(gnc_html_get_history(report->html));
  if(node) {
    gnc_html_show_url(report->html, node->type, node->location, 
                      node->label, 0);
  }
  return TRUE;
}

static int 
gnc_report_window_back_cb(GtkWidget * w, gpointer data)
{
  gnc_report_window     * report = data;
  gnc_html_history_node * node;
  
  gnc_html_history_back(gnc_html_get_history(report->html));
  node = gnc_html_history_get_current(gnc_html_get_history(report->html));
  if(node) {
    gnc_html_show_url(report->html, node->type, node->location, 
                      node->label, 0);
  }
  return TRUE;
}


static int
gnc_report_window_stop_button_cb(GtkWidget * w, gpointer data)
{
  gnc_report_window * report = data;
  gnc_html_cancel(report->html);
  return TRUE;
}

/* Returns SCM_BOOL_F if cancel. Returns SCM_BOOL_T if html.
 * Otherwise returns pair from export_types. */
static SCM
gnc_get_export_type_choice (SCM export_types)
{
  GList * choices = NULL;
  gboolean bad = FALSE;
  GList * node;
  int choice;
  SCM tail;

  if (!gh_list_p (export_types))
    return SCM_BOOL_F;

  for (tail = export_types; !gh_null_p (tail); tail = gh_cdr (tail))
  {
    SCM pair = gh_car (tail);
    char * name;
    SCM scm;

    if (!gh_pair_p (pair))
    {
      g_warning ("unexpected list element");
      bad = TRUE;
      break;
    }

    scm = gh_car (pair);
    if (!gh_string_p (scm))
    {
      g_warning ("unexpected pair element");
      bad = TRUE;
      break;
    }

    name = gh_scm2newstr (scm, NULL);
    choices = g_list_prepend (choices, g_strdup (name));
    if (name) free (name);
  }

  if (!bad)
  {
    choices = g_list_reverse (choices);

    choices = g_list_prepend (choices, g_strdup (_("HTML")));

    choice = gnc_choose_radio_option_dialog_parented
      (NULL, _("Choose export format"),
       _("Choose the export format for this report:"), 0, choices);
  }
  else
    choice = -1;

  for (node = choices; node; node = node->next)
    g_free (node->data);
  g_list_free (choices);

  if (choice < 0)
    return SCM_BOOL_F;

  if (choice == 0)
    return SCM_BOOL_T;

  choice--;
  if (choice >= gh_length (export_types))
    return SCM_BOOL_F;

  return gh_list_ref (export_types, gh_int2scm (choice));
}

static const char *
gnc_get_export_filename (SCM choice)
{
  const char * filepath;
  struct stat statbuf;
  char * title;
  char * type;
  int rc;

  if (choice == SCM_BOOL_T)
    type = g_strdup (_("HTML"));
  else
  {
    char * s = gh_scm2newstr (gh_car (choice), NULL);
    type = g_strdup (s);
    if (s) free (s);
  }

  /* %s is the type of what is about to be saved, e.g. "HTML". */
  title = g_strdup_printf (_("Save %s To File"), type);

  filepath = gnc_file_dialog (title, NULL, NULL);

  g_free (title);
  g_free (type);

  if (!filepath)
    return NULL;

  rc = stat (filepath, &statbuf);

  /* Check for an error that isn't a non-existant file. */
  if (rc != 0 && errno != ENOENT)
  {
    /* %s is the strerror(3) string of the error that occurred. */
    const char *format = _("You cannot save to that filename.\n\n%s");

    gnc_error_dialog (format, strerror(errno));
    return NULL;
  }

  /* Check for a file that isn't a regular file. */
  if (rc == 0 && !S_ISREG (statbuf.st_mode))
  {
    const char *message = _("You cannot save to that file.");

    gnc_error_dialog (message);
    return NULL;
  }

  if (rc == 0)
  {
    const char *format = _("The file \n    %s\n already exists.\n"
                           "Are you sure you want to overwrite it?");

    if (!gnc_verify_dialog (FALSE, format, filepath))
      return NULL;
  }

  return filepath;
}

static int
gnc_report_window_export_button_cb(GtkWidget * w, gpointer data)
{
  gnc_report_window * report = data;
  const char * filepath;
  SCM export_types;
  SCM export_thunk;
  gboolean result;
  SCM choice;

  export_types = gh_call1 (gh_eval_str ("gnc:report-export-types"),
                           report->cur_report);

  export_thunk = gh_call1 (gh_eval_str ("gnc:report-export-thunk"),
                           report->cur_report);

  if (gh_list_p (export_types) && gh_procedure_p (export_thunk))
    choice = gnc_get_export_type_choice (export_types);
  else
    choice = SCM_BOOL_T;

  if (choice == SCM_BOOL_F)
    return TRUE;

  filepath = gnc_get_export_filename (choice);
  if (!filepath)
    return TRUE;

  if (gh_pair_p (choice))
  {
    SCM file_scm;
    SCM res;

    choice = gh_cdr (choice);
    file_scm = gh_str02scm ((char *) filepath);

    res = gh_call3 (export_thunk, report->cur_report, choice, file_scm);

    result = (res != SCM_BOOL_F);
  }
  else
    result = gnc_html_export (report->html, filepath);

  if (!result)
  {
    const char *fmt = _("Could not open the file\n"
                        "     %s\n%s");
    gnc_error_dialog (fmt, filepath ? filepath : "(null)",
		      strerror (errno) ? strerror (errno) : "");
  }

  return TRUE;
}


static int
gnc_report_window_params_cb(GtkWidget * w, gpointer data)
{
  gnc_report_window * report = data;
  SCM start_editor = gh_eval_str("gnc:report-edit-options");

  if(report->cur_report != SCM_BOOL_F)
  {
    if(gh_call1(start_editor, report->cur_report) == SCM_BOOL_F) {
      gnc_warning_dialog(_("There are no options for this report."));
    }
    else {
      gnc_report_window_add_edited_report(report, report->cur_report);
    }
  }

  return TRUE;
}

static void
gnc_report_window_reload_cb(GtkWidget * unused, gpointer data)
{
  gnc_report_window * report = data;
  SCM               dirty_report = gh_eval_str("gnc:report-set-dirty?!");

  if(report->cur_report != SCM_BOOL_F) {
    gh_call2(dirty_report, report->cur_report, SCM_BOOL_T);
    gnc_html_reload(report->html);
  }
}

static void
gnc_report_window_set_back_button(gnc_report_window * win, int enabled)
{
  GnomeApp    * app = win->mc->app;
  GnomeUIInfo * info;

  /* the code below is broken, so just return */
  return;

  if(app) {
    info = gnome_mdi_get_child_menu_info(app);
    if(info) gtk_widget_set_sensitive(info[0].widget, enabled);
  }
}

static void
gnc_report_window_set_fwd_button(gnc_report_window * win, int enabled)
{
  GnomeApp    * app = win->mc->app;
  GnomeUIInfo * info;

  /* the code below is broken, so just return */
  return;

  if(app) {
    info = gnome_mdi_get_child_menu_info(app);
    if(info) gtk_widget_set_sensitive(info[1].widget, enabled);
  }
}

void
gnc_report_window_reload(gnc_report_window * win)
{
  gnc_html_reload(win->html);
}

static void
gnc_report_window_option_change_cb(gpointer data)
{
  gnc_report_window * report = data;
  SCM               dirty_report = gh_eval_str("gnc:report-set-dirty?!");

  if(report->cur_report != SCM_BOOL_F) {
    /* it's probably already dirty, but make sure */
    gh_call2(dirty_report, report->cur_report, SCM_BOOL_T);
    gnc_html_reload(report->html);
  }
}

/********************************************************************
 * gnc_report_window_load_cb
 * called after a report is loaded into the gnc_html widget 
 ********************************************************************/

static void
gnc_report_window_refresh (gpointer data)
{
  gnc_mdi_child_refresh (data);
}

static void 
gnc_report_window_load_cb(gnc_html * html, URLType type, 
                          const gchar * location, const gchar * label, 
                          gpointer data)
{
  gnc_report_window * win = data;
  int  report_id;
  SCM  find_report = gh_eval_str("gnc:find-report");
  SCM  get_options = gh_eval_str("gnc:report-options");
  SCM  set_needs_save = gh_eval_str("gnc:report-set-needs-save?!");
  SCM  inst_report;
  
  /* we get this callback if a new report is requested to be loaded OR
   * if any URL is clicked.  If an options URL is clicked, we want to
   * know about it */
  if(!safe_strcmp (type, URL_TYPE_REPORT) && 
     location && (strlen(location) > 3) && 
     !strncmp("id=", location, 3)) {
    sscanf(location+3, "%d", &report_id);
  }
  else if(!safe_strcmp( type, URL_TYPE_OPTIONS)
	  && location && (strlen(location) > 10) &&
          !strncmp("report-id=", location, 10)) {
    sscanf(location+10, "%d", &report_id);
    inst_report = gh_call1(find_report, gh_int2scm(report_id));
    if(inst_report != SCM_BOOL_F) {
      gnc_report_window_add_edited_report(win, inst_report);      
    }
    return;
  }
  else {
    return;
  }
  
  /* get the inst-report from the Scheme-side hash, and get its
   * options and editor thunk */
  if((inst_report = gh_call1(find_report, gh_int2scm(report_id))) ==
     SCM_BOOL_F) {
    return;
  }

  if(win->initial_report == SCM_BOOL_F) {    
    scm_unprotect_object(win->initial_report);
    win->initial_report = inst_report;
    scm_protect_object(win->initial_report);
    
    gh_call2(set_needs_save, inst_report, SCM_BOOL_T);

    win->initial_odb = gnc_option_db_new(gh_call1(get_options, inst_report));  
    win->name_change_cb_id = 
      gnc_option_db_register_change_callback(win->initial_odb,
                                             gnc_report_window_refresh,
                                             win->mc,
                                             "General", "Report name");
  }
  
  if((win->cur_report != SCM_BOOL_F) && (win->cur_odb != NULL)) {
    gnc_option_db_unregister_change_callback_id(win->cur_odb,
                                                win->option_change_cb_id);
    gnc_option_db_destroy(win->cur_odb);
    win->cur_odb = NULL;
  }
  
  if(win->cur_report != SCM_BOOL_F)
    scm_unprotect_object(win->cur_report);
  win->cur_report = inst_report;
  scm_protect_object(win->cur_report);

  win->cur_odb = gnc_option_db_new(gh_call1(get_options, inst_report));  
  win->option_change_cb_id = 
    gnc_option_db_register_change_callback(win->cur_odb,
                                           gnc_report_window_option_change_cb,
                                           win, NULL, NULL);
  
  if(gnc_html_history_forward_p(gnc_html_get_history(win->html))) {
    gnc_report_window_set_fwd_button(win, TRUE); 
  }
  else {
    gnc_report_window_set_fwd_button(win, FALSE); 
  }

  if(gnc_html_history_back_p(gnc_html_get_history(win->html))) {
    gnc_report_window_set_back_button(win, TRUE); 
  }
  else {
    gnc_report_window_set_back_button(win, FALSE); 
  }
}


/********************************************************************
 * gnc_report_window_destroy_cb
 ********************************************************************/

static void
gnc_report_window_destroy_cb(GtkWidget * w, gpointer data)
{
  gnc_report_window * win = data;
  /* make sure we don't get a double dose -o- destruction */ 
  gtk_signal_disconnect_by_data(GTK_OBJECT(win->container), 
                                data);
  
  gnc_report_window_destroy(win);
}


/********************************************************************
 * gnc_report_window_print_cb
 ********************************************************************/

static void
gnc_report_window_print_cb(GtkWidget * w, gpointer data)
{
  gnc_report_window * win = data;
  gnc_html_print(win->html);
}

static void 
gnc_report_window_history_destroy_cb(gnc_html_history_node * node, 
                                     gpointer user_data)
{
  static SCM         remover = SCM_BOOL_F;
  int                report_id;
  
  if(remover == SCM_BOOL_F) {
    remover = gh_eval_str("gnc:report-remove-by-id");
  }
  
  if(node && 
     !safe_strcmp (node->type, URL_TYPE_REPORT) && 
     !strncmp("id=", node->location, 3)) {
    sscanf(node->location+3, "%d", &report_id);
    /*    printf("unreffing report %d and children\n", report_id);
          gh_call1(remover, gh_int2scm(report_id)); */
  }
  else {
    return;
  }
}

static void
close_handler (gpointer user_data)
{
  gnc_report_window *win = user_data;  
  printf("in close handler\n");
  gnc_report_window_destroy (win);
}

/********************************************************************
 * gnc_report_window_new 
 * allocates and opens up a report window
 ********************************************************************/

gnc_report_window *
gnc_report_window_new(GNCMDIChildInfo * mc)
{
  gnc_report_window * report = g_new0(gnc_report_window, 1);
  GtkObject         * tlo; 

  report->mc               = mc;
  report->html             = gnc_html_new();
  report->cur_report       = SCM_BOOL_F;
  report->initial_report   = SCM_BOOL_F;
  report->edited_reports   = SCM_EOL;
  report->name_change_cb_id = SCM_BOOL_F;

  scm_protect_object(report->cur_report);
  scm_protect_object(report->initial_report);
  scm_protect_object(report->edited_reports);

  gnc_html_history_set_node_destroy_cb(gnc_html_get_history(report->html),
                                       gnc_report_window_history_destroy_cb,
                                       (gpointer)report);
  

  report->container = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(report->container), GTK_SHADOW_NONE);
  
  tlo = GTK_OBJECT(report->container);
  gtk_container_add(GTK_CONTAINER(report->container), 
                    gnc_html_get_widget(report->html));
  
  gnc_register_gui_component (WINDOW_REPORT_CM_CLASS, NULL,
                              close_handler, report);
  
  gnc_html_set_urltype_cb(report->html, gnc_report_window_check_urltype);
  gnc_html_set_load_cb(report->html, gnc_report_window_load_cb, report);
  
  gtk_signal_connect(GTK_OBJECT(report->container), "destroy",
                     GTK_SIGNAL_FUNC(gnc_report_window_destroy_cb),
                     report);
  
  gtk_widget_show_all(report->container);
  
  return report;
}


/********************************************************************
 * gnc_report_window_create_toolbar
 * make a toolbar for the top-level MDI app 
 ********************************************************************/

void
gnc_report_window_create_toolbar(gnc_report_window * win,
                                 GNCMDIChildInfo * child)
{
  GnomeUIInfo       toolbar_data[] = 
  {
    { GNOME_APP_UI_ITEM,
      _("Back"),
      _("Move back one step in the history"),
      gnc_report_window_back_cb, win,
      NULL,
      GNOME_APP_PIXMAP_STOCK, 
      GNOME_STOCK_PIXMAP_BACK,
      0, 0, NULL
    },
    { GNOME_APP_UI_ITEM,
      _("Forward"),
      _("Move forward one step in the history"),
      gnc_report_window_fwd_cb, win,
      NULL,
      GNOME_APP_PIXMAP_STOCK, 
      GNOME_STOCK_PIXMAP_FORWARD,
      0, 0, NULL
    },
    { GNOME_APP_UI_ITEM,
      N_("Reload"),
      N_("Reload the current report"),
      gnc_report_window_reload_cb, win,
      NULL,
      GNOME_APP_PIXMAP_STOCK, 
      GNOME_STOCK_PIXMAP_REFRESH,
      0, 0, NULL
    },
    { GNOME_APP_UI_ITEM,
      N_("Stop"),
      N_("Cancel outstanding HTML requests"),
      gnc_report_window_stop_button_cb, win,
      NULL,
      GNOME_APP_PIXMAP_STOCK, 
      GNOME_STOCK_PIXMAP_STOP,
      0, 0, NULL
    },
    GNOMEUIINFO_SEPARATOR,
    { GNOME_APP_UI_ITEM,
      _("Export"),
      _("Export HTML-formatted report to file"),
      gnc_report_window_export_button_cb, win,
      NULL,
      GNOME_APP_PIXMAP_STOCK,
      GNOME_STOCK_PIXMAP_CONVERT,
      0, 0, NULL
    },    
    { GNOME_APP_UI_ITEM,
      _("Options"),
      _("Edit report options"),
      gnc_report_window_params_cb, win,
      NULL,
      GNOME_APP_PIXMAP_STOCK,
      GNOME_STOCK_PIXMAP_PROPERTIES,
      0, 0, NULL
    },    
    { GNOME_APP_UI_ITEM,
      _("Print"),
      _("Print report window"),
      gnc_report_window_print_cb, win,
      NULL,
      GNOME_APP_PIXMAP_STOCK, 
      GNOME_STOCK_PIXMAP_PRINT,
      0, 0, NULL
    },
    GNOMEUIINFO_END
  };
  
  child->toolbar_info = g_memdup (toolbar_data, sizeof(toolbar_data));
}


/********************************************************************
 * gnc_report_window_create_menu
 * child menu for reports (none currently)
 ********************************************************************/
void
gnc_report_window_create_menu(gnc_report_window * report, 
                              GNCMDIChildInfo * child)
{
  child->menu_info = NULL;
}


/********************************************************************
 * gnc_report_window_destroy 
 * free and destroy a window 
 ********************************************************************/

void
gnc_report_window_destroy(gnc_report_window * win)
{

  SCM  get_editor = gh_eval_str("gnc:report-editor-widget");
  SCM  set_editor = gh_eval_str("gnc:report-set-editor-widget!");
  SCM  edited, editor; 

  gnc_unregister_gui_component_by_data (WINDOW_REPORT_CM_CLASS, win);

  /* close any open editors */
  for(edited = scm_list_copy(win->edited_reports); !gh_null_p(edited); 
      edited = gh_cdr(edited)) {
    editor = gh_call1(get_editor, gh_car(edited));
    gh_call2(set_editor, gh_car(edited), SCM_BOOL_F);
    if(editor != SCM_BOOL_F) {
      gtk_widget_destroy(GTK_WIDGET(gw_wcp_get_ptr(editor)));
    }
  }

  if(win->initial_odb) {
    gnc_option_db_unregister_change_callback_id(win->initial_odb, 
                                                win->name_change_cb_id);
    
    gnc_option_db_destroy(win->initial_odb);
    win->initial_odb = NULL;
  }

  gnc_html_destroy(win->html);
  
  win->container     = NULL;
  win->html          = NULL;
  
  scm_unprotect_object(win->cur_report);
  scm_unprotect_object(win->edited_reports);
  
  g_free(win);
}

gnc_html *
gnc_report_window_get_html(gnc_report_window * report)
{
  return report->html;
}

GtkWidget *
gnc_report_window_get_container(gnc_report_window * report)
{
  return report->container;
}

SCM
gnc_report_window_get_report(gnc_report_window * report)
{
  return report->cur_report;
}

void
gnc_report_window_show_report(gnc_report_window * report, int report_id)
{
  char * location = g_strdup_printf("id=%d", report_id);  
  gnc_html_show_url(report->html, URL_TYPE_REPORT, location, NULL, 0);
  g_free(location);
}

void
reportWindow(int report_id)
{
  gnc_set_busy_cursor (NULL, TRUE);
  gnc_main_window_open_report(report_id, FALSE);
  gnc_unset_busy_cursor (NULL);
}

void
gnc_print_report (int report_id)
{
  gnc_html *html;
  char * location;

  html = gnc_html_new ();

  gnc_set_busy_cursor (NULL, TRUE);
  location = g_strdup_printf("id=%d", report_id);  
  gnc_html_show_url(html, URL_TYPE_REPORT, location, NULL, FALSE);
  g_free(location);
  gnc_unset_busy_cursor (NULL);

  gnc_html_print (html);

  gnc_html_destroy (html);
}


/********************************************************************
 * default parameters editor handling 
 ********************************************************************/

struct report_default_params_data {
  GNCOptionWin * win;
  GNCOptionDB  * db;
  SCM          scm_options;
  SCM          cur_report;
};


static void
gnc_options_dialog_apply_cb(GNCOptionWin * propertybox,
                            gpointer user_data)
{
  SCM  dirty_report = gh_eval_str("gnc:report-set-dirty?!");
  struct report_default_params_data * win = user_data;
  
  if(!win) return;
  gnc_option_db_commit(win->db);
  gh_call2(dirty_report, win->cur_report, SCM_BOOL_T);
}

static void
gnc_options_dialog_help_cb(GNCOptionWin * propertybox,
                           gpointer user_data)
{
  gnome_ok_dialog(_("Set the report options you want using this dialog."));
}

static void
gnc_options_dialog_close_cb(GNCOptionWin * propertybox,
                            gpointer user_data)
{
  struct report_default_params_data * win = user_data;
  SCM    set_editor = gh_eval_str("gnc:report-set-editor-widget!");
  
  gh_call2(set_editor, win->cur_report, SCM_BOOL_F);
  gnc_option_db_destroy(win->db);
  scm_unprotect_object(win->scm_options);
  gnc_options_dialog_destroy(win->win);
  g_free(win);
}


GtkWidget * 
gnc_report_window_default_params_editor(SCM options, SCM report)
{
  SCM get_editor = gh_eval_str("gnc:report-editor-widget");
  SCM get_title = gh_eval_str("gnc:report-type");
  SCM ptr;
  
  char *title = NULL;

  ptr = gh_call1(get_editor, report);
  if(ptr != SCM_BOOL_F) {
    GtkWindow * w = gw_wcp_get_ptr(ptr);
    gtk_window_present(w);
    return NULL;
  }
  else {
    struct report_default_params_data * prm = 
      g_new0(struct report_default_params_data, 1);
    
    prm->scm_options = options;
    prm->cur_report  = report;
    prm->db          = gnc_option_db_new(prm->scm_options);

    ptr = gh_call1(get_title, report);
    if (ptr != SCM_BOOL_F) {
      title = gh_scm2newstr(ptr, NULL);
    }
    prm->win         = gnc_options_dialog_new(TRUE, title);
    
    if (title) {
      free(title);
    }

    scm_protect_object(prm->scm_options);
    scm_protect_object(prm->cur_report);
    
    gnc_build_options_dialog_contents(prm->win, prm->db);
    gnc_option_db_clean(prm->db);

    gnc_options_dialog_set_apply_cb(prm->win, 
                                    gnc_options_dialog_apply_cb,
                                    (gpointer)prm);
    gnc_options_dialog_set_help_cb(prm->win, 
                                   gnc_options_dialog_help_cb,
                                   (gpointer)prm);
    gnc_options_dialog_set_close_cb(prm->win, 
                                    gnc_options_dialog_close_cb,
                                    (gpointer)prm);
    return gnc_options_dialog_widget(prm->win);
  }
}

void
gnc_report_window_remove_edited_report(gnc_report_window * win, SCM report)
{ 
  SCM new_edited = scm_delete(win->edited_reports, report);
  scm_unprotect_object(win->edited_reports);
  win->edited_reports = new_edited;
  scm_protect_object(win->edited_reports);
}

void
gnc_report_window_add_edited_report(gnc_report_window * win, SCM report)
{
  SCM new_edited = gh_cons(report, win->edited_reports);
  scm_unprotect_object(win->edited_reports);
  win->edited_reports = new_edited;
  scm_protect_object(win->edited_reports);
}

void
gnc_report_raise_editor(SCM report)
{
  SCM get_editor = gh_eval_str("gnc:report-editor-widget");
  SCM editor = gh_call1(get_editor, report);
  gtk_window_present(gw_wcp_get_ptr(editor));
}

static gboolean
gnc_html_file_stream_cb (const char *location, char ** data, int *len)
{
  *len = gncReadFile (location, data);
  return (*len > 0);
}

static gboolean
gnc_html_report_stream_cb (const char *location, char ** data, int *len)
{
  gboolean ok;

  ok = gnc_run_report_id_string (location, data);

  if (!ok)
    *data = g_strdup_printf ("<html><body><h3>%s</h3>"
			     "<p>%s</p></body></html>", 
			     _("Report error"),
			     _("An error occurred while running the report."));

  *len = strlen(*data);
  return ok;
}

static gboolean
gnc_html_options_url_cb (const char *location, const char *label,
                         gboolean new_window, GNCURLResult *result)
{
  SCM find_report  = gh_eval_str ("gnc:find-report");
  SCM start_editor = gh_eval_str ("gnc:report-edit-options");
  SCM report;
  int report_id;

  g_return_val_if_fail (location != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  result->load_to_stream = FALSE;

  /* href="gnc-options:report-id=2676" */
  if (strncmp ("report-id=", location, 10) == 0)
  {
    if (sscanf (location + 10, "%d", &report_id) != 1)
    {
      result->error_message =
        g_strdup_printf (_("Badly formed options URL: %s"), location);

      return FALSE;
    }

    report = gh_call1 (find_report, gh_int2scm (report_id));
    if (report == SCM_UNDEFINED ||
        report == SCM_BOOL_F)
    {
      result->error_message =
        g_strdup_printf (_("Badly report id: %s"), location);

      return FALSE;
    }

    gh_call1 (start_editor, report);

    return TRUE;
  }
  else
  {
    result->error_message =
      g_strdup_printf (_("Badly formed options URL: %s"), location);

    return FALSE;
  }
}

static gboolean
gnc_html_report_url_cb (const char *location, const char *label,
                        gboolean new_window, GNCURLResult *result)
{
  g_return_val_if_fail (location != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  /* make a new window if necessary */ 
  if (new_window)
  {
    char *url;

    url = gnc_build_url (URL_TYPE_REPORT, location, label);
    gnc_main_window_open_report_url (url, FALSE);
    g_free (url);

    result->load_to_stream = FALSE;
  }
  else
  {
    result->load_to_stream = TRUE;
  }

  return TRUE;
}

static gboolean
gnc_html_help_url_cb (const char *location, const char *label,
                      gboolean new_window, GNCURLResult *result)
{
  g_return_val_if_fail (location != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  if (new_window)
  {
    gnc_help_window * help;

    help = gnc_help_window_new ();
    gnc_help_window_show_help (help, location, label);

    result->load_to_stream = FALSE;
  }
  else
    result->load_to_stream = TRUE;

  return TRUE;
}

void
gnc_report_init (void)
{
  gnc_html_register_stream_handler (URL_TYPE_HELP, gnc_html_file_stream_cb);
  gnc_html_register_stream_handler (URL_TYPE_FILE, gnc_html_file_stream_cb);
  gnc_html_register_stream_handler (URL_TYPE_REPORT, gnc_html_report_stream_cb);

  gnc_html_register_url_handler (URL_TYPE_OPTIONS, gnc_html_options_url_cb);
  gnc_html_register_url_handler (URL_TYPE_REPORT, gnc_html_report_url_cb);
  gnc_html_register_url_handler (URL_TYPE_HELP, gnc_html_help_url_cb);
}
