/********************************************************************\
 * FileDialog.c -- file-handling utility dialogs for gnucash.       * 
 *                                                                  *
 * Copyright (C) 1997 Robin D. Clark                                *
 * Copyright (C) 1998, 1999, 2000 Linas Vepstas                     *
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

#include "config.h"

#include <errno.h>
#include <glib.h>
#include <libguile.h>
#include <string.h>
#include <g-wrap-wct.h>

#include "global-options.h"
#include "gnc-commodity.h"
#include "gnc-component-manager.h"
#include "gnc-engine-util.h"
#include "gnc-engine.h"
#include "gnc-event.h"
#include "gnc-file-dialog.h"
#include "gnc-file-history.h"
#include "gnc-file-p.h"
#include "gnc-gui-query.h"
#include "gnc-ui.h"
#include "gnc-ui-util.h"
#include "qofbackend.h"
#include "qofbook.h"
#include "qofsession.h"
#include "messages.h"

/** GLOBALS *********************************************************/
/* This static indicates the debugging module that this .o belongs to.  */
static short module = MOD_GUI;

static GNCCanCancelSaveCB can_cancel_cb = NULL;
static GNCShutdownCB shutdown_cb = NULL;

static GNCHistoryAddFileFunc history_add_file_func = NULL;
static GNCHistoryGetLastFunc history_get_last_func = NULL;

static GNCFileDialogFunc file_dialog_func = NULL;
static GNCFilePercentageFunc file_percentage_func = NULL;


void
gnc_file_set_handlers (GNCHistoryAddFileFunc history_add_file_func_in,
                       GNCHistoryGetLastFunc history_get_last_func_in,
                       GNCFileDialogFunc file_dialog_func_in)
{
  history_add_file_func = history_add_file_func_in;
  history_get_last_func = history_get_last_func_in;
  file_dialog_func = file_dialog_func_in;
}

void
gnc_file_set_pct_handler (GNCFilePercentageFunc file_percentage_func_in)
{
  file_percentage_func = file_percentage_func_in;
}

void
gnc_file_init (void)
{
  /* Make sure we have a current session. */
  qof_session_get_current_session ();
}

static gboolean
show_session_error (QofBackendError io_error, const char *newfile)
{
  GtkWidget *parent = gnc_ui_get_toplevel();
  gboolean uh_oh = TRUE;
  const char *fmt;

  if (NULL == newfile) { newfile = _("(null)"); }

  switch (io_error)
  {
    case ERR_BACKEND_NO_ERR:
      uh_oh = FALSE;
      break;

    case ERR_BACKEND_NO_BACKEND:
      fmt = _("The URL \n    %s\n"
              "is not supported by this version of GnuCash.");
      gnc_error_dialog (parent, fmt, newfile);
      break;

    case ERR_BACKEND_BAD_URL:
      fmt = _("Can't parse the URL\n   %s\n");
      gnc_error_dialog (parent, fmt, newfile);
      break;

    case ERR_BACKEND_CANT_CONNECT:
      fmt = _("Can't connect to\n   %s\n"
              "The host, username or password were incorrect.");
      gnc_error_dialog (parent, fmt, newfile);
      break;

    case ERR_BACKEND_CONN_LOST:
      fmt = _("Can't connect to\n   %s\n"
              "Connection was lost, unable to send data.");
      gnc_error_dialog (parent, fmt, newfile);
      break;

    case ERR_BACKEND_TOO_NEW:
      fmt = _("This file/URL appears to be from a newer version\n"
              "of GnuCash. You must upgrade your version of GnuCash\n"
              "to work with this data.");
      gnc_error_dialog (parent, fmt);
      break;

    case ERR_BACKEND_NO_SUCH_DB:
      fmt = _("The database\n"
              "   %s\n"
              "doesn't seem to exist. Do you want to create it?\n");
      if (gnc_verify_dialog (parent, TRUE, fmt, newfile)) { uh_oh = FALSE; }
      break;

    case ERR_BACKEND_LOCKED:
      fmt = _("GnuCash could not obtain the lock for\n"
              "   %s.\n"
              "That database may be in use by another user,\n"
              "in which case you should not open the database.\n"
              "\nDo you want to proceed with opening the database?");
      if (gnc_verify_dialog (parent, TRUE, fmt, newfile)) { uh_oh = FALSE; }
      break;

    case ERR_BACKEND_READONLY:
      fmt = _("GnuCash could not write to\n"
              "   %s.\n"
              "That database may be on a read-only file system,\n"
              "or you may not have write permission for the directory.\n");
      gnc_error_dialog (parent, fmt, newfile);
      break;

    case ERR_BACKEND_DATA_CORRUPT:
      fmt = _("The file/URL \n    %s\n"
              "does not contain GnuCash data or the data is corrupt.");
      gnc_error_dialog (parent, fmt, newfile);
      break;

    case ERR_BACKEND_SERVER_ERR:
      fmt = _("The server at URL \n    %s\n"
              "experienced an error or encountered bad or corrupt data.");
      gnc_error_dialog (parent, fmt, newfile);
      break;

    case ERR_BACKEND_PERM:
      fmt = _("You do not have permission to access\n    %s\n");
      gnc_error_dialog (parent, fmt, newfile);
      break;

    case ERR_BACKEND_MISC:
      fmt = _("An error occurred while processing\n    %s\n");
      gnc_error_dialog (parent, fmt, newfile);
      break;

    case ERR_FILEIO_FILE_BAD_READ:
      fmt = _("There was an error reading the file.\n"
              "Do you want to continue?");
      if (gnc_verify_dialog (parent, TRUE, fmt)) { uh_oh = FALSE; }
      break;

    case ERR_FILEIO_PARSE_ERROR:
      fmt = _("There was an error parsing the file \n    %s\n");
      gnc_error_dialog (parent, fmt, newfile);
      break;

    case ERR_FILEIO_FILE_EMPTY:
      fmt = _("The file \n    %s\n is empty.");
      gnc_error_dialog (parent, fmt, newfile);
      break;

    case ERR_FILEIO_FILE_NOT_FOUND:
      fmt = _("The file \n    %s\n could not be found.");
      gnc_error_dialog (parent, fmt, newfile);
      break;

    case ERR_FILEIO_FILE_TOO_OLD:
      fmt = _("This file is from an older version of GnuCash.\n"
              "Do you want to continue?");
      if (gnc_verify_dialog (parent, TRUE, fmt)) { uh_oh = FALSE; }
      break;

    case ERR_FILEIO_UNKNOWN_FILE_TYPE:
      fmt = _("Unknown file type");
      gnc_error_dialog(parent, fmt, newfile);
      break;
      
    case ERR_SQL_DB_TOO_OLD:
      fmt = _("This database is from an older version of GnuCash.\n"
              "Do you want to want to upgrade the database "
              "to the current version?");
      if (gnc_verify_dialog (parent, TRUE, fmt)) { uh_oh = FALSE; }
      break;

    case ERR_SQL_DB_BUSY:
      fmt = _("The SQL database is in use by other users, "
              "and the upgrade cannot be performed until they logoff.\n"
              "If there are currently no other users, consult the \n"
              "documentation to learn how to clear out dangling login\n"
              "sessions.");
      gnc_error_dialog (parent, fmt);
      break;

    default:
      PERR("FIXME: Unhandled error %d", io_error);
      fmt = _("An unknown I/O error occurred.");
      gnc_error_dialog (parent, fmt);
      break;
  }

  return uh_oh;
}

static void
gnc_add_history (QofSession * session)
{
  char *url;
  char *file;

  if (!session) return;
  if (!history_add_file_func) return;

  url = xaccResolveURL (qof_session_get_url (session));
  if (!url)
    return;

  if (strncmp (url, "file:", 5) == 0)
    file = url + 5;
  else
    file = url;

  history_add_file_func (file);

  g_free (url);
}

static void
gnc_book_opened (void)
{
  QofSession *session = qof_session_get_current_session();
  scm_call_2 (scm_c_eval_string("gnc:hook-run-danglers"),
              scm_c_eval_string("gnc:*book-opened-hook*"),
              (session ? 
               gw_wcp_assimilate_ptr (session, scm_c_eval_string("<gnc:Session*>")) :
               SCM_BOOL_F));
}

void
gnc_file_new (void)
{
  QofSession *session;

  /* If user attempts to start a new session before saving results of
   * the last one, prompt them to clean up their act. */
  if (!gnc_file_query_save ())
    return;

  session = qof_session_get_current_session ();

  /* close any ongoing file sessions, and free the accounts.
   * disable events so we don't get spammed by redraws. */
  gnc_engine_suspend_events ();
  
  scm_call_2(scm_c_eval_string("gnc:hook-run-danglers"),
             scm_c_eval_string("gnc:*book-closed-hook*"),
             (session ?
              gw_wcp_assimilate_ptr (session, scm_c_eval_string("<gnc:Session*>")) :
              SCM_BOOL_F));

  gnc_close_gui_component_by_session (session);
  qof_session_destroy (session);

  /* start a new book */
  qof_session_get_current_session ();

  scm_call_1(scm_c_eval_string("gnc:hook-run-danglers"),
             scm_c_eval_string("gnc:*new-book-hook*"));

  gnc_book_opened ();

  gnc_engine_resume_events ();
  gnc_gui_refresh_all ();
}

gboolean
gnc_file_query_save (void)
{
  GtkWidget *parent = gnc_ui_get_toplevel();

  /* If user wants to mess around before finishing business with
   * the old file, give em a chance to figure out what's up.  
   * Pose the question as a "while" loop, so that if user screws
   * up the file-selection dialog, we don't blow em out of the water;
   * instead, give them another chance to say "no" to the verify box.
   */
  while (qof_book_not_saved(qof_session_get_book (qof_session_get_current_session ())))
  {
    GNCVerifyResult result;
    const char *message = _("Changes have been made since the last "
                            "Save. Save the data to file?");

    if (can_cancel_cb && can_cancel_cb ())
      result = gnc_verify_cancel_dialog (parent, GTK_RESPONSE_YES, message);
    else
    {
      gboolean do_save = gnc_verify_dialog (parent, TRUE, message);

      result = do_save ? GTK_RESPONSE_YES : GTK_RESPONSE_NO;
    }

    if (result == GTK_RESPONSE_CANCEL)
      return FALSE;

    if (result == GTK_RESPONSE_NO)
      return TRUE;

    gnc_file_save ();
  }

  return TRUE;
}

/* private utilities for file open; done in two stages */

static gboolean
gnc_post_file_open (const char * filename)
{
  QofSession *current_session, *new_session;
  gboolean uh_oh = FALSE;
  char * newfile;
  QofBackendError io_err = ERR_BACKEND_NO_ERR;

  if (!filename) return FALSE;

  newfile = xaccResolveURL (filename); 
  if (!newfile)
  {
    show_session_error (ERR_FILEIO_FILE_NOT_FOUND, filename);
    return FALSE;
  }

  /* disable events while moving over to the new set of accounts; 
   * the mass deletetion of accounts and transactions during
   * switchover would otherwise cause excessive redraws. */
  gnc_engine_suspend_events ();

  /* Change the mouse to a busy cursor */
  gnc_set_busy_cursor (NULL, TRUE);

  /* -------------- BEGIN CORE SESSION CODE ------------- */
  /* -- this code is almost identical in FileOpen and FileSaveAs -- */
  current_session  = qof_session_get_current_session();
  scm_call_2(scm_c_eval_string("gnc:hook-run-danglers"),
             scm_c_eval_string("gnc:*book-closed-hook*"),
             (current_session ?
              gw_wcp_assimilate_ptr (current_session,
                                     scm_c_eval_string("<gnc:Session*>")) :
              SCM_BOOL_F));
  qof_session_destroy (current_session);

  /* load the accounts from the users datafile */
  /* but first, check to make sure we've got a session going. */
  new_session = qof_session_new ();

  qof_session_begin (new_session, newfile, FALSE, FALSE);
  io_err = qof_session_get_error (new_session);

  /* if file appears to be locked, ask the user ... */
  if (ERR_BACKEND_LOCKED == io_err || ERR_BACKEND_READONLY == io_err)
  {
    const char *buttons[] = { N_("Quit"), N_("Open Anyway"),
                              N_("Create New File"), NULL };
    char *fmt = ((ERR_BACKEND_LOCKED == io_err) ?
                 _("GnuCash could not obtain the lock for\n"
                   "   %s.\n"
                   "That database may be in use by another user,\n"
                   "in which case you should not open the database.\n"
                   "\nWhat would you like to do?") :
                 _("WARNING!!!  GnuCash could not obtain the lock for\n"
                   "   %s.\n"
                   "That database may be on a read-only file system,\n"
                   "or you may not have write permission for the directory.\n"
                   "If you proceed you may not be able to save any changes.\n"
                   "\nWhat would you like to do?")
                 );
    int rc;

    if (shutdown_cb) {
      rc = gnc_generic_question_dialog (NULL, buttons, fmt, newfile);
    } else {
      rc = gnc_generic_question_dialog (NULL, buttons+1, fmt, newfile)+1;
    }

    if (rc == 0)
    {
      if (shutdown_cb)
        shutdown_cb(0);
      g_assert(1);
    }
    else if (rc == 1)
    {
      /* user told us to ignore locks. So ignore them. */
      qof_session_begin (new_session, newfile, TRUE, FALSE);
    }
    else
    {
      /* Can't use the given file, so just create a new
       * database so that the user will get a window that
       * they can click "Exit" on.
       */
      gnc_file_new ();
    }
  }

  /* if the database doesn't exist, ask the user ... */
  else if ((ERR_BACKEND_NO_SUCH_DB == io_err) ||
           (ERR_SQL_DB_TOO_OLD == io_err))
  {
    if (FALSE == show_session_error (io_err, newfile))
    {
      /* user told us to create a new database. Do it. */
      qof_session_begin (new_session, newfile, FALSE, TRUE);
    }
  }

  /* Check for errors again, since above may have cleared the lock.
   * If its still locked, still, doesn't exist, still too old, then
   * don't bother with the message, just die. */
  io_err = qof_session_get_error (new_session);
  if ((ERR_BACKEND_LOCKED == io_err) ||
      (ERR_BACKEND_READONLY == io_err) ||
      (ERR_BACKEND_NO_SUCH_DB == io_err) ||
      (ERR_SQL_DB_TOO_OLD == io_err))
  {
    uh_oh = TRUE;
  }
  else
  {
    uh_oh = show_session_error (io_err, newfile);
  }

  if (!uh_oh)
  {
    AccountGroup *new_group;

    if (file_percentage_func) {
      file_percentage_func(_("Reading file..."), 0.0);
      qof_session_load (new_session, file_percentage_func);
      file_percentage_func(NULL, -1.0);
    } else {
      qof_session_load (new_session, NULL);
    }

    /* check for i/o error, put up appropriate error dialog */
    io_err = qof_session_get_error (new_session);
    uh_oh = show_session_error (io_err, newfile);

    new_group = gnc_book_get_group (qof_session_get_book (new_session));
    if (uh_oh) new_group = NULL;

    /* Umm, came up empty-handed, but no error: 
     * The backend forgot to set an error. So make one up. */
    if (!uh_oh && !new_group) 
    {
      uh_oh = show_session_error (ERR_BACKEND_MISC, newfile);
    }
  }

  gnc_unset_busy_cursor (NULL);

  /* going down -- abandon ship */
  if (uh_oh) 
  {
    qof_session_destroy (new_session);

    /* well, no matter what, I think it's a good idea to have a
     * topgroup around.  For example, early in the gnucash startup
     * sequence, the user opens a file; if this open fails for any
     * reason, we don't want to leave them high & dry without a
     * topgroup, because if the user continues, then bad things will
     * happen. */
    qof_session_get_current_session ();

    g_free (newfile);

    gnc_engine_resume_events ();
    gnc_gui_refresh_all ();

    gnc_book_opened ();

    return FALSE;
  }

  /* if we got to here, then we've successfully gotten a new session */
  /* close up the old file session (if any) */
  qof_session_set_current_session(new_session);

  gnc_book_opened ();

  /* --------------- END CORE SESSION CODE -------------- */

  /* clean up old stuff, and then we're outta here. */
  gnc_add_history (new_session);

  g_free (newfile);

  gnc_engine_resume_events ();
  gnc_gui_refresh_all ();

  return TRUE;
}

gboolean
gnc_file_open (void)
{
  const char * newfile;
  const char * last;
  gboolean result;

  if (!gnc_file_query_save ())
    return FALSE;

  if (!file_dialog_func)
  {
    PWARN ("no file dialog function");
    return FALSE;
  }

  last = history_get_last_func ? history_get_last_func () : NULL;
  newfile = file_dialog_func (_("Open"), NULL, last);
  result = gnc_post_file_open (newfile);

  /* This dialogue can show up early in the startup process. If the
   * user fails to pick a file (by e.g. hitting the cancel button), we
   * might be left with a null topgroup, which leads to nastiness when
   * user goes to create their very first account. So create one. */
  qof_session_get_current_session ();

  return result;
}

gboolean
gnc_file_open_file (const char * newfile)
{
  if (!newfile) return FALSE;

  if (!gnc_file_query_save ())
    return FALSE;

  return gnc_post_file_open (newfile);
}

void
gnc_file_export_file(const char * newfile)
{
  QofSession *current_session, *new_session;
  gboolean ok;
  QofBackendError io_err = ERR_BACKEND_NO_ERR;
  char *default_dir;

  default_dir = gnc_lookup_string_option("__paths", "Export Accounts", NULL);
  if (default_dir == NULL)
    gnc_init_default_directory(&default_dir);

  if (!newfile) {
    if (!file_dialog_func) {
      PWARN ("no file dialog function");
      return;
    }

    newfile =  file_dialog_func (_("Export"), NULL, default_dir);
    if (!newfile)
      return;
  }

  /* Remember the directory as the default. */
  gnc_extract_directory(&default_dir, newfile);
  gnc_set_string_option("__paths", "Export Accounts", default_dir);
  g_free(default_dir);
  
  gnc_engine_suspend_events();

  /* -- this session code is NOT identical in FileOpen and FileSaveAs -- */

  new_session = qof_session_new ();
  qof_session_begin (new_session, newfile, FALSE, FALSE);

  io_err = qof_session_get_error (new_session);

  /* if file appears to be locked, ask the user ... */
  if (ERR_BACKEND_LOCKED == io_err || ERR_BACKEND_READONLY == io_err) 
  {
    if (FALSE == show_session_error (io_err, newfile))
    {
       /* user told us to ignore locks. So ignore them. */
      qof_session_begin (new_session, newfile, TRUE, FALSE);
    }
  }

  /* --------------- END CORE SESSION CODE -------------- */

  /* oops ... file already exists ... ask user what to do... */
  if (qof_session_save_may_clobber_data (new_session))
  {
    const char *format = _("The file \n    %s\n already exists.\n"
                           "Are you sure you want to overwrite it?");

    /* if user says cancel, we should break out */
    if (!gnc_verify_dialog (NULL, FALSE, format, newfile))
    {
      return;
    }

    /* Whoa-ok. Blow away the previous file. */
  }

  /* use the current session to save to file */
  gnc_set_busy_cursor (NULL, TRUE);
  current_session = qof_session_get_current_session();
  if (file_percentage_func) {
    file_percentage_func(_("Exporting file..."), 0.0);
    ok = qof_session_export (new_session, current_session,
                             file_percentage_func);
    file_percentage_func(NULL, -1.0);
  } else {
    ok = qof_session_export (new_session, current_session, NULL);
  }
  gnc_unset_busy_cursor (NULL);
  qof_session_destroy (new_session);
  gnc_engine_resume_events();

  if (!ok)
  {
    /* %s is the strerror(3) error string of the error that occurred. */
    const char *format = _("There was an error saving the file.\n\n%s");

    gnc_error_dialog (NULL, format, strerror(errno));
    return;
  }
}

static gboolean been_here_before = FALSE;

void
gnc_file_save (void)
{
  QofBackendError io_err;
  const char * newfile;
  QofSession *session;
  ENTER (" ");

  /* hack alert -- Somehow make sure all in-progress edits get committed! */

  /* If we don't have a filename/path to save to get one. */
  session = qof_session_get_current_session ();

  if (!qof_session_get_file_path (session))
  {
    gnc_file_save_as ();
    return;
  }

  /* use the current session to save to file */
  gnc_set_busy_cursor (NULL, TRUE);
  if (file_percentage_func) {
    file_percentage_func(_("Writing file..."), 0.0);
    qof_session_save (session, file_percentage_func);
    file_percentage_func(NULL, -1.0);
  } else {
    qof_session_save (session, NULL);
  }
  gnc_unset_busy_cursor (NULL);

  /* Make sure everything's OK - disk could be full, file could have
     become read-only etc. */
  newfile = qof_session_get_file_path (session);
  io_err = qof_session_get_error (session);
  if (ERR_BACKEND_NO_ERR != io_err)
  {
    show_session_error (io_err, newfile);

    if (been_here_before) return;
    been_here_before = TRUE;
    gnc_file_save_as ();   /* been_here prevents infinite recursion */
    been_here_before = FALSE;
    return;
  }

  gnc_add_history (session);

  /* save the main window state */
  scm_call_1 (scm_c_eval_string("gnc:main-window-save-state"),
              (session ?
               gw_wcp_assimilate_ptr (session, scm_c_eval_string("<gnc:Session*>")) :
               SCM_BOOL_F));

  LEAVE (" ");
}

void
gnc_file_save_as (void)
{
  QofSession *new_session;
  QofSession *session;
  const char *filename;
  char *default_dir = NULL;        /* Default to last open */
  const char *last;
  char *newfile;
  const char *oldfile;
  QofBackendError io_err = ERR_BACKEND_NO_ERR;

  ENTER(" ");

  if (!file_dialog_func)
  {
    PWARN ("no file dialog func");
    return;
  }

  last = history_get_last_func ? history_get_last_func() : NULL;
  if (last)
    gnc_extract_directory(&default_dir, last);
  else
    gnc_init_default_directory(&default_dir);
  filename = file_dialog_func (_("Save"), "*.gnc", default_dir);
  if (default_dir)
    free(default_dir);
  if (!filename) return;

  /* Check to see if the user specified the same file as the current
   * file. If so, then just do that, instead of the below, which
   * assumes a truly new name was given. */
  newfile = xaccResolveURL (filename);
  if (!newfile)
  {
     show_session_error (ERR_FILEIO_FILE_NOT_FOUND, filename);
     return;
  }

  session = qof_session_get_current_session ();
  oldfile = qof_session_get_file_path (session);
  if (oldfile && (strcmp(oldfile, newfile) == 0))
  {
    g_free (newfile);
    gnc_file_save ();
    return;
  }

  /* -- this session code is NOT identical in FileOpen and FileSaveAs -- */

  new_session = qof_session_new ();
  qof_session_begin (new_session, newfile, FALSE, FALSE);

  io_err = qof_session_get_error (new_session);

  /* if file appears to be locked, ask the user ... */
  if (ERR_BACKEND_LOCKED == io_err || ERR_BACKEND_READONLY == io_err) 
  {
    if (FALSE == show_session_error (io_err, newfile))
    {
       /* user told us to ignore locks. So ignore them. */
      qof_session_begin (new_session, newfile, TRUE, FALSE);
    }
  }

  /* if the database doesn't exist, ask the user ... */
  else if ((ERR_BACKEND_NO_SUCH_DB == io_err) ||
           (ERR_SQL_DB_TOO_OLD == io_err))
  {
    if (FALSE == show_session_error (io_err, newfile))
    {
      /* user told us to create a new database. Do it. */
      qof_session_begin (new_session, newfile, FALSE, TRUE);
    }
  }

  /* check again for session errors (since above dialog may have 
   * cleared a file lock & moved things forward some more) 
   * This time, errors will be fatal.
   */
  io_err = qof_session_get_error (new_session);
  if (ERR_BACKEND_NO_ERR != io_err) 
  {
    show_session_error (io_err, newfile);
    qof_session_destroy (new_session);
    g_free (newfile);
    return;
  }

  /* if we got to here, then we've successfully gotten a new session */
  /* close up the old file session (if any) */
  qof_session_swap_data (session, new_session);
  qof_session_destroy (session);
  session = NULL;

  /* XXX At this point, we should really mark the data in the new session
   * as being 'dirty', since we haven't saved it at all under the new
   * session. But I'm lazy...
   */
  qof_session_set_current_session(new_session);

  /* --------------- END CORE SESSION CODE -------------- */

  /* oops ... file already exists ... ask user what to do... */
  if (qof_session_save_may_clobber_data (new_session))
  {
    const char *format = _("The file \n    %s\n already exists.\n"
                           "Are you sure you want to overwrite it?");

    /* if user says cancel, we should break out */
    if (!gnc_verify_dialog (NULL, FALSE, format, newfile))
    {
      g_free (newfile);
      return;
    }

    /* Whoa-ok. Blow away the previous file. */
  }

  gnc_file_save ();

  g_free (newfile);
  LEAVE (" ");
}

void
gnc_file_quit (void)
{
  QofSession *session;

  session = qof_session_get_current_session ();

  /* disable events; otherwise the mass deletetion of accounts and
   * transactions during shutdown would cause massive redraws */
  gnc_engine_suspend_events ();

  scm_call_2(scm_c_eval_string("gnc:hook-run-danglers"),
             scm_c_eval_string("gnc:*book-closed-hook*"),
             (session ?
              gw_wcp_assimilate_ptr (session, scm_c_eval_string("<gnc:Session*>")) :
              SCM_BOOL_F));
  
  qof_session_destroy (session);

  qof_session_get_current_session ();

  gnc_engine_resume_events ();
}

void
gnc_file_set_can_cancel_callback (GNCCanCancelSaveCB cb)
{
  can_cancel_cb = cb;
}

void
gnc_file_set_shutdown_callback (GNCShutdownCB cb)
{
  shutdown_cb = cb;
}
