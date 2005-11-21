/********************************************************************
 * dialog-style-sheet.c -- window for configuring HTML style        *
 *                         sheets in GnuCash                        *
 * Copyright (C) 2000 Bill Gribble <grib@billgribble.com>           *
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
 ********************************************************************/

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "dialog-style-sheet.h"
#include "dialog-options.h"
#include "dialog-utils.h"
#include "gnc-ui.h"

StyleSheetDialog * gnc_style_sheet_dialog = NULL;

struct _stylesheetdialog {
  GtkWidget  * toplevel;
  GtkTreeView   * list_view;
  GtkListStore  * list_store;
  GtkWidget  * options_frame;
};

typedef struct ss_info {
  GNCOptionWin  * odialog;
  GNCOptionDB   * odb;
  SCM           stylesheet;
  GtkTreeRowReference *row_ref;
} ss_info;

enum {
  COLUMN_NAME,
  COLUMN_STYLESHEET,
  COLUMN_DIALOG,
  N_COLUMNS
};


/************************************************************
 *     Style Sheet Edit Dialog (I.E. an options dialog)     *
 ************************************************************/

static void
gnc_style_sheet_options_apply_cb(GNCOptionWin * propertybox,
                                 gpointer user_data)
{
  ss_info * ssi = (ss_info *)user_data;
  SCM    apply_changes = scm_c_eval_string("gnc:html-style-sheet-apply-changes");
  gnc_option_db_commit(ssi->odb);
  scm_call_1(apply_changes, ssi->stylesheet);
}


static void
gnc_style_sheet_options_close_cb(GNCOptionWin * propertybox,
                                 gpointer user_data)
{
  ss_info * ssi = user_data;
  GtkTreeIter iter;

  if (gtk_tree_row_reference_valid (ssi->row_ref)) {
    StyleSheetDialog * ss = gnc_style_sheet_dialog;
    gtk_tree_model_get_iter (GTK_TREE_MODEL(ss->list_store), &iter,
			     gtk_tree_row_reference_get_path (ssi->row_ref));
    gtk_list_store_set (ss->list_store, &iter,
			COLUMN_DIALOG, NULL,
			-1);
    
  }
  gtk_tree_row_reference_free (ssi->row_ref);
  gnc_option_db_destroy(ssi->odb);
  gnc_options_dialog_destroy(ssi->odialog);
  scm_gc_unprotect_object(ssi->stylesheet);
  g_free(ssi);
}


static ss_info *
gnc_style_sheet_dialog_create(StyleSheetDialog * ss,
			      gchar *name,
			      SCM sheet_info,
			      GtkTreeRowReference *row_ref)
{
  SCM get_options = scm_c_eval_string("gnc:html-style-sheet-options");

    SCM            scm_options = scm_call_1(get_options, sheet_info);
    ss_info        * ssinfo = g_new0(ss_info, 1);
    GtkWidget      * window;
    gchar          * title;

    title = g_strdup_printf("HTML Style Sheet Properties: %s", name);
    ssinfo->odialog = gnc_options_dialog_new(title);
    ssinfo->odb     = gnc_option_db_new(scm_options);
    ssinfo->stylesheet = sheet_info;
    ssinfo->row_ref    = row_ref;
    g_free(title);

    scm_gc_protect_object(ssinfo->stylesheet);
    g_object_ref(gnc_options_dialog_widget(ssinfo->odialog));

    gnc_build_options_dialog_contents(ssinfo->odialog, 
                                      ssinfo->odb);
    gnc_options_dialog_set_apply_cb(ssinfo->odialog, 
                                    gnc_style_sheet_options_apply_cb,
                                    ssinfo);    
    gnc_options_dialog_set_close_cb(ssinfo->odialog, 
                                    gnc_style_sheet_options_close_cb,
                                    ssinfo);
    window = gnc_options_dialog_widget(ssinfo->odialog);
    gtk_window_set_transient_for(GTK_WINDOW(window),
				 GTK_WINDOW(gnc_style_sheet_dialog->toplevel));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(window), TRUE);
    gtk_window_present(GTK_WINDOW(window));

    return(ssinfo);
}


static SCM
gnc_style_sheet_new (StyleSheetDialog * ssd)
{
  SCM              make_ss   = scm_c_eval_string("gnc:make-html-style-sheet");
  SCM              templates = scm_c_eval_string("(gnc:get-html-templates)");
  SCM              t_name = scm_c_eval_string("gnc:html-style-sheet-template-name");
  SCM              new_ss = SCM_BOOL_F;
  GtkWidget        * template_entry;
  GtkWidget        * name_entry;
  GtkWidget        * template_combo;
  GList            * strings=NULL;
  gint             dialog_retval;
  const char       * template_str = NULL;
  const char       * name_str = NULL;

  /* get the new name for the style sheet */
  GladeXML *xml = gnc_glade_xml_new ("report.glade",
                                     "New Style Sheet Dialog");
  GtkWidget * dlg = glade_xml_get_widget (xml, "New Style Sheet Dialog");
  template_entry = glade_xml_get_widget (xml, "template_entry");
  template_combo = glade_xml_get_widget (xml, "template_combo");
  name_entry     = glade_xml_get_widget (xml, "name_entry");

  /* put in the list of style sheet type names */
  for(; !SCM_NULLP(templates); templates=SCM_CDR(templates)) {
    SCM t = SCM_CAR(templates);
    strings = g_list_append(strings, SCM_STRING_CHARS(scm_call_1(t_name, t)));
  }
  
  gtk_combo_set_popdown_strings(GTK_COMBO(template_combo), strings);
  
  /* The strings on the list are const.  Just free the list. */
  g_list_free(strings);

  /* get the name */
  gtk_window_set_transient_for (GTK_WINDOW(dlg), GTK_WINDOW(ssd->toplevel));
  dialog_retval = gtk_dialog_run(GTK_DIALOG(dlg));

  if(dialog_retval == GTK_RESPONSE_OK) {
    template_str = gtk_entry_get_text(GTK_ENTRY(template_entry));
    name_str     = gtk_entry_get_text(GTK_ENTRY(name_entry));
    if(template_str && name_str) {
      new_ss = scm_call_2(make_ss, 
			  scm_makfrom0str(template_str),
			  scm_makfrom0str(name_str));
    }
  }

  gtk_widget_destroy(dlg);
  return(new_ss);
}


/************************************************************
 *               Style Sheet Selection Dialog               *
 ************************************************************/
static void
gnc_style_sheet_select_dialog_add_one(StyleSheetDialog * ss,
				      SCM sheet_info,
				      gboolean select)
{
  SCM get_name, scm_name;
  const gchar *c_name;
  GtkTreeSelection *selection;
  GtkTreeIter iter;

  get_name = scm_c_eval_string("gnc:html-style-sheet-name");
  scm_name = scm_call_1(get_name, sheet_info);
  c_name = SCM_STRING_CHARS(scm_name);
  if (!c_name)
    return;

  /* add the column name */
  scm_gc_protect_object(sheet_info);
  gtk_list_store_append (ss->list_store, &iter);
  gtk_list_store_set (ss->list_store, &iter,
		      COLUMN_NAME, c_name,
		      COLUMN_STYLESHEET, sheet_info,
		      -1);

  if (select) {
    selection = gtk_tree_view_get_selection (ss->list_view);
    gtk_tree_selection_select_iter (selection, &iter);
  }
}

static void
gnc_style_sheet_select_dialog_fill(StyleSheetDialog * ss)
{
  SCM stylesheets = scm_c_eval_string("(gnc:get-html-style-sheets)");
  SCM sheet_info;

  /* pack it full of content */
  for(; !SCM_NULLP(stylesheets); stylesheets=SCM_CDR(stylesheets)) {
    sheet_info = SCM_CAR(stylesheets);
    gnc_style_sheet_select_dialog_add_one(ss, sheet_info, FALSE);
  }
}

static void
gnc_style_sheet_select_dialog_response_cb (GtkDialog *unused,
					   gint response,
					   gpointer user_data)
{
  StyleSheetDialog  * ss;
  GtkTreeSelection  * selection;
  GtkTreeRowReference * row_ref;
  GtkTreeModel      * model;
  GtkTreePath       * path;
  GtkTreeIter         iter;
  ss_info           * ssinfo;
  SCM                 remover;
  SCM sheet_info;
  gchar *name;

  ss = (StyleSheetDialog *)user_data;
  switch (response) {
   case GNC_RESPONSE_NEW:
    sheet_info = gnc_style_sheet_new(ss);
    if (sheet_info == SCM_BOOL_F)
      break;
    gnc_style_sheet_select_dialog_add_one(ss, sheet_info, TRUE);
    /* Fall through */

   case GNC_RESPONSE_EDIT:
    selection = gtk_tree_view_get_selection (ss->list_view);
    if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
      gtk_tree_model_get (model, &iter,
			  COLUMN_NAME, &name,
			  COLUMN_STYLESHEET, &sheet_info,
			  -1);
      /* Fire off options dialog here */
      path = gtk_tree_model_get_path (GTK_TREE_MODEL(ss->list_store), &iter);
      row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL(ss->list_store), path);
      ssinfo = gnc_style_sheet_dialog_create(ss, name, sheet_info, row_ref);
      gtk_list_store_set (ss->list_store, &iter,
			  COLUMN_DIALOG, ssinfo,
			  -1);
    }
    break;
    
   case GNC_RESPONSE_DELETE:
    selection = gtk_tree_view_get_selection (ss->list_view);
    if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
      gtk_tree_model_get (model, &iter,
			  COLUMN_STYLESHEET, &sheet_info,
			  COLUMN_DIALOG, &ssinfo,
			  -1);
      gtk_list_store_remove (ss->list_store, &iter);

      if (ssinfo)
	gnc_style_sheet_options_close_cb(NULL, ssinfo);
      remover = scm_c_eval_string("gnc:html-style-sheet-remove");
      scm_call_1(remover, sheet_info);
      scm_gc_unprotect_object(sheet_info);
    }
    break;

   case GTK_RESPONSE_CLOSE:
   default:
    gnc_style_sheet_dialog = NULL;
    gtk_widget_destroy(ss->toplevel);
    g_free(ss);
    break;
  }
}


static void
gnc_style_sheet_select_dialog_event_cb (GtkWidget *widget,
					GdkEvent *event,
					gpointer user_data)
{
  StyleSheetDialog  * ss = user_data;

  g_return_if_fail(event != NULL);
  g_return_if_fail(ss != NULL);

  if (event->type != GDK_2BUTTON_PRESS)
    return;

  /* Synthesize a click of the edit button */
  gnc_style_sheet_select_dialog_response_cb (NULL, GNC_RESPONSE_EDIT, ss);
}


static StyleSheetDialog *
gnc_style_sheet_select_dialog_create(void)
{
  StyleSheetDialog  * ss = g_new0(StyleSheetDialog, 1);
  GladeXML          * xml;
  GtkCellRenderer   * renderer;
  GtkTreeSelection  * selection;

  xml = gnc_glade_xml_new ("report.glade", "Select Style Sheet Dialog");

  ss->toplevel   = glade_xml_get_widget (xml, "Select Style Sheet Dialog");

  ss->list_view  = GTK_TREE_VIEW(glade_xml_get_widget (xml, "style_sheet_list_view"));
  ss->list_store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_POINTER);
  gtk_tree_view_set_model(ss->list_view, GTK_TREE_MODEL(ss->list_store));
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes(ss->list_view, -1,
					      _("Style Sheet Name"), renderer,
					      "text", COLUMN_NAME,
					      NULL);

  selection = gtk_tree_view_get_selection (ss->list_view);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

  gtk_signal_connect(GTK_OBJECT(ss->toplevel), "response",
                     GTK_SIGNAL_FUNC(gnc_style_sheet_select_dialog_response_cb), ss); 
  gtk_signal_connect(GTK_OBJECT(ss->list_view), "event-after",
                     GTK_SIGNAL_FUNC(gnc_style_sheet_select_dialog_event_cb), ss); 
  
  gnc_style_sheet_select_dialog_fill(ss);

  gtk_widget_show_all(ss->toplevel);

  return ss;
}


void
gnc_style_sheet_dialog_open(void)
{
  if(gnc_style_sheet_dialog) {
    gtk_window_present(GTK_WINDOW(gnc_style_sheet_dialog->toplevel));
  }
  else {
    gnc_style_sheet_dialog = 
      gnc_style_sheet_select_dialog_create();
  }
}

