/********************************************************************
 * gnc-backend-file.c: load and save data to files                  *
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
\********************************************************************/
/** @file gnc-backend-file.c
 *  @brief load and save data to files 
 *  @author Copyright (c) 2000 Gnumatic Inc.
 *  @author Copyright (c) 2002 Derek Atkins <warlord@MIT.EDU>
 *  @author Copyright (c) 2003 Linas Vepstas <linas@linas.org>
 *
 * This file implements the top-level QofBackend API for saving/
 * restoring data to/from an ordinary Unix filesystem file.
 */

#define _GNU_SOURCE

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <time.h>

#include "TransLog.h"
#include "gnc-engine.h"

#include "gnc-filepath-utils.h"

#include "io-gncxml.h"
#include "io-gncbin.h"
#include "io-gncxml-v2.h"
#include "gnc-backend-api.h"
#include "gnc-backend-file.h"

#define GNC_BE_DAYS "file_retention_days"
#define GNC_BE_ZIP  "file_compression"

static QofLogModule log_module = GNC_MOD_BACKEND;

static gint file_retention_days = 0;
static gboolean file_compression = FALSE;

static void option_cb (QofBackendOption *option, gpointer data)
{
	if(0 == safe_strcmp(GNC_BE_DAYS, option->option_name)) {
		file_retention_days = *(gint*)option->value;
	}
	if(0 == safe_strcmp(GNC_BE_ZIP, option->option_name)) {
		file_compression = (gboolean)qof_util_bool_to_int(option->value);
	}
}

/* lookup the options and modify the frame */
static void
gnc_file_be_set_config(QofBackend *be, KvpFrame *config)
{
	qof_backend_option_foreach(config, option_cb, NULL);
}

static KvpFrame*
gnc_file_be_get_config(QofBackend *be)
{
	QofBackendOption *option;

	qof_backend_prepare_frame(be);
	option = g_new0(QofBackendOption, 1);
	option->option_name = GNC_BE_DAYS;
	option->description = _("Number of days to retain old files");
	option->tooltip = _("GnuCash keeps backups of old files. "
		"This setting specifies how long each is kept.");
	option->type = KVP_TYPE_GINT64;
	option->value = (gpointer)&file_retention_days;
	qof_backend_prepare_option(be, option);
	g_free(option);
	option = g_new0(QofBackendOption, 1);
	option->option_name = GNC_BE_ZIP;
	option->description = _("Compress output files?");
	option->tooltip = _("GnuCash can save data files with compression."
		" Enable this option to compress your data file. ");
	option->type = KVP_TYPE_STRING;
	if(file_compression) { option->value = (gpointer)"TRUE"; }
	else { option->value = (gpointer)"FALSE"; }
	qof_backend_prepare_option(be, option);
	g_free(option);
	return qof_backend_complete_frame(be);
}

/* ================================================================= */

static gboolean
gnc_file_be_get_file_lock (FileBackend *be)
{
    struct stat statbuf;
    char pathbuf[PATH_MAX];
    char *path = NULL;
    int rc;
    QofBackendError be_err;

    rc = stat (be->lockfile, &statbuf);
    if (!rc)
    {
        /* oops .. file is locked by another user  .. */
        qof_backend_set_error ((QofBackend*)be, ERR_BACKEND_LOCKED);
        return FALSE;
    }

    be->lockfd = open (be->lockfile, O_RDWR | O_CREAT | O_EXCL , 0);
    if (be->lockfd < 0)
    {
        /* oops .. we can't create the lockfile .. */
        switch (errno) {
        case EACCES:
        case EROFS:
        case ENOSPC:
          be_err = ERR_BACKEND_READONLY;
          break;
        default:
          be_err = ERR_BACKEND_LOCKED;
          break;
        }
        qof_backend_set_error ((QofBackend*)be, be_err);
        return FALSE;
    }

    /* OK, now work around some NFS atomic lock race condition 
     * mumbo-jumbo.  We do this by linking a unique file, and 
     * then examing the link count.  At least that's what the 
     * NFS programmers guide suggests. 
     * Note: the "unique filename" must be unique for the
     * triplet filename-host-process, otherwise accidental 
     * aliases can occur.
     */

    /* apparently, even this code may not work for some NFS
     * implementations. In the long run, I am told that 
     * ftp.debian.org
     *  /pub/debian/dists/unstable/main/source/libs/liblockfile_0.1-6.tar.gz
     * provides a better long-term solution.
     */

    strcpy (pathbuf, be->lockfile);
    path = strrchr (pathbuf, '.');
    sprintf (path, ".%lx.%d.LNK", gethostid(), getpid());

    rc = link (be->lockfile, pathbuf);
    if (rc)
    {
        /* If hard links aren't supported, just allow the lock. */
        if (errno == EOPNOTSUPP || errno == EPERM)
        {
            be->linkfile = NULL;
            return TRUE;
        }

        /* Otherwise, something else is wrong. */
        qof_backend_set_error ((QofBackend*)be, ERR_BACKEND_LOCKED);
        unlink (pathbuf);
        close (be->lockfd);
        unlink (be->lockfile);
        return FALSE;
    }

    rc = stat (be->lockfile, &statbuf);
    if (rc)
    {
        /* oops .. stat failed!  This can't happen! */
        qof_backend_set_error ((QofBackend*)be, ERR_BACKEND_LOCKED);
        unlink (pathbuf);
        close (be->lockfd);
        unlink (be->lockfile);
        return FALSE;
    }

    if (statbuf.st_nlink != 2)
    {
        qof_backend_set_error ((QofBackend*)be, ERR_BACKEND_LOCKED);
        unlink (pathbuf);
        close (be->lockfd);
        unlink (be->lockfile);
        return FALSE;
    }

    be->linkfile = g_strdup (pathbuf);

    return TRUE;
}

/* ================================================================= */

static void
file_session_begin(QofBackend *be_start, QofSession *session, 
                   const char *book_id,
                   gboolean ignore_lock, gboolean create_if_nonexistent)
{
    FileBackend *be = (FileBackend*) be_start;
    char *p;

    ENTER (" ");

    /* Make sure the directory is there */
    be->dirname = xaccResolveFilePath(book_id);
    if (NULL == be->dirname)
    {
        qof_backend_set_error (be_start, ERR_FILEIO_FILE_NOT_FOUND);
        return;
    }
    be->fullpath = g_strdup (be->dirname);
    be->be.fullpath = be->fullpath;
    p = strrchr (be->dirname, '/');
    if (p && p != be->dirname)
    {
        struct stat statbuf;
        int rc;

        *p = '\0';
        rc = stat (be->dirname, &statbuf);
        if (rc != 0 || !S_ISDIR(statbuf.st_mode))
        {
            qof_backend_set_error (be_start, ERR_FILEIO_FILE_NOT_FOUND);
            g_free (be->fullpath); be->fullpath = NULL;
            g_free (be->dirname); be->dirname = NULL;
            return;
        }
        rc = stat (be->fullpath, &statbuf);
        if (rc == 0 && S_ISDIR(statbuf.st_mode))
       {
            qof_backend_set_error (be_start, ERR_FILEIO_UNKNOWN_FILE_TYPE);
            g_free (be->fullpath); be->fullpath = NULL;
            g_free (be->dirname); be->dirname = NULL;
            return;
        }
    }

    /* ---------------------------------------------------- */
    /* We should now have a fully resolved path name.
     * Lets see if we can get a lock on it. */

    be->lockfile = g_strconcat(be->fullpath, ".LCK", NULL);

    if (!ignore_lock && !gnc_file_be_get_file_lock (be))
    {
        g_free (be->lockfile); be->lockfile = NULL;
        return;
    }

    LEAVE (" ");
    return;
}

/* ================================================================= */

static void
file_session_end(QofBackend *be_start)
{
    FileBackend *be = (FileBackend*)be_start;
    ENTER (" ");

    if (be->linkfile)
        unlink (be->linkfile);

    if (be->lockfd > 0)
        close (be->lockfd);

    if (be->lockfile)
        unlink (be->lockfile);

    g_free (be->dirname);
    be->dirname = NULL;

    g_free (be->fullpath);
    be->fullpath = NULL;

    g_free (be->lockfile);
    be->lockfile = NULL;

    g_free (be->linkfile);
    be->linkfile = NULL;
    LEAVE (" ");
}

static void
file_destroy_backend(QofBackend *be)
{
    g_free(be);
}

/* ================================================================= */
/* Write the financial data in a book to a file, returning FALSE on
   error and setting the error_result to indicate what went wrong if
   it's not NULL.  This function does not manage file locks in any
   way.

   If make_backup is true, write out a time-stamped copy of the file
   into the same directory as the indicated file, with a filename of
   "file.YYYYMMDDHHMMSS.xac" where YYYYMMDDHHMMSS is replaced with the
   current year/month/day/hour/minute/second. */

static gboolean
copy_file(const char *orig, const char *bkup)
{
    static int buf_size = 1024;
    char buf[buf_size];
    int orig_fd;
    int bkup_fd;
    ssize_t count_write;
    ssize_t count_read;

    orig_fd = open(orig, O_RDONLY);
    if(orig_fd == -1)
    {
        return FALSE;
    }
    bkup_fd = creat(bkup, 0600);
    if(bkup_fd == -1)
    {
        close(orig_fd);
        return FALSE;
    }

    do
    {
        count_read = read(orig_fd, buf, buf_size);
        if(count_read == -1 && errno != EINTR)
        {
            close(orig_fd);
            close(bkup_fd);
            return FALSE;
        }

        if(count_read > 0)
        {
            count_write = write(bkup_fd, buf, count_read);
            if(count_write == -1)
            {
                close(orig_fd);
                close(bkup_fd);
                return FALSE;
            }
        }
    } while(count_read > 0);

    close(orig_fd);
    close(bkup_fd);
    
    return TRUE;
}
        
/* ================================================================= */

static gboolean
gnc_int_link_or_make_backup(FileBackend *be, const char *orig, const char *bkup)
{
    int err_ret = link(orig, bkup);
    if(err_ret != 0)
    {
        if(errno == EPERM || errno == EOPNOTSUPP)
        {
            err_ret = copy_file(orig, bkup);
        }

        if(!err_ret)
        {
            qof_backend_set_error((QofBackend*)be, ERR_FILEIO_BACKUP_ERROR);
            PWARN ("unable to make file backup from %s to %s: %s", 
                    orig, bkup, strerror(errno) ? strerror(errno) : ""); 
            return FALSE;
        }
    }

    return TRUE;
}

/* ================================================================= */

static gboolean
is_gzipped_file(const gchar *name)
{
    unsigned char buf[2];
    int fd = open(name, O_RDONLY);

    if(fd == 0)
    {
        return FALSE;
    }

    if(read(fd, buf, 2) != 2)
    {
        return FALSE;
    }

    if(buf[0] == 037 && buf[1] == 0213)
    {
        return TRUE;
    }
    
    return FALSE;
}
    
static QofBookFileType
gnc_file_be_determine_file_type(const char *path)
{
	if(gnc_is_xml_data_file_v2(path)) {
        return GNC_BOOK_XML2_FILE;
    } else if(gnc_is_xml_data_file(path)) {
        return GNC_BOOK_XML1_FILE;
    } else if(is_gzipped_file(path)) {
        return GNC_BOOK_XML2_FILE;
	}
	else if(gnc_is_bin_file(path)) { return GNC_BOOK_BIN_FILE; }
	return GNC_BOOK_NOT_OURS;
}

static gboolean
gnc_determine_file_type (const char *path)
{
	struct stat sbuf;
	int rc;
	FILE *t;

	if (!path) { return FALSE; }
	if (0 == safe_strcmp(path, QOF_STDOUT)) { return FALSE; }
	t = fopen(path, "r");
	if(!t) { PINFO (" new file"); return TRUE; }
	fclose(t);
	rc = stat(path, &sbuf);
	if(rc < 0) { return FALSE; }
	if (sbuf.st_size == 0)    { PINFO (" empty file"); return TRUE; }
	if(gnc_is_xml_data_file_v2(path))   { return TRUE; } 
	else if(gnc_is_xml_data_file(path)) { return TRUE; } 
	else if(is_gzipped_file(path))      { return TRUE; }
	else if(gnc_is_bin_file(path))      { return TRUE; }
	PINFO (" %s is not a gnc file", path);
	return FALSE;
}	

static gboolean
gnc_file_be_backup_file(FileBackend *be)
{
    gboolean bkup_ret;
    char *timestamp;
    char *backup;
    const char *datafile;
    struct stat statbuf;
    int rc;

    datafile = be->fullpath;
    
    rc = stat (datafile, &statbuf);
    if (rc)
      return (errno == ENOENT);

    if(gnc_file_be_determine_file_type(datafile) == GNC_BOOK_BIN_FILE)
    {
        /* make a more permament safer backup */
        const char *back = "-binfmt.bkup";
        char *bin_bkup = g_new(char, strlen(datafile) + strlen(back) + 1);
        strcpy(bin_bkup, datafile);
        strcat(bin_bkup, back);
        bkup_ret = gnc_int_link_or_make_backup(be, datafile, bin_bkup);
        g_free(bin_bkup);
        if(!bkup_ret)
        {
            return FALSE;
        }
    }

    timestamp = xaccDateUtilGetStampNow ();
    backup = g_new (char, strlen (datafile) + strlen (timestamp) + 6);
    strcpy (backup, datafile);
    strcat (backup, ".");
    strcat (backup, timestamp);
    strcat (backup, ".xac");
    g_free (timestamp);

    bkup_ret = gnc_int_link_or_make_backup(be, datafile, backup);
    g_free(backup);

    return bkup_ret;
}

/* ================================================================= */
 
static gboolean
gnc_file_be_write_to_file(FileBackend *fbe, 
                          QofBook *book, 
                          const gchar *datafile,
                          gboolean make_backup)
{
    QofBackend *be = &fbe->be;
    char *tmp_name;
    struct stat statbuf;
    int rc;
    QofBackendError be_err;

    ENTER (" book=%p file=%s", book, datafile);

    /* If the book is 'clean', recently saved, then don't save again. */
    /* XXX this is currently broken due to faulty 'Save As' logic. */
    /* if (FALSE == qof_book_not_saved (book)) return FALSE; */

    tmp_name = g_new(char, strlen(datafile) + 12);
    strcpy(tmp_name, datafile);
    strcat(tmp_name, ".tmp-XXXXXX");

    if(!mktemp(tmp_name))
    {
        qof_backend_set_error(be, ERR_BACKEND_MISC);
        return FALSE;
    }
  
    if(make_backup)
    {
        if(!gnc_file_be_backup_file(fbe))
        {
            return FALSE;
        }
    }
  
    if(gnc_book_write_to_xml_file_v2(book, tmp_name, file_compression)) 
    {
        /* Record the file's permissions before unlinking it */
        rc = stat(datafile, &statbuf);
        if(rc == 0)
        {
            /* Use the permissions from the original data file */
            if(chmod(tmp_name, statbuf.st_mode) != 0)
            {
                qof_backend_set_error(be, ERR_BACKEND_PERM);
		/* FIXME: Even if the chmod did fail, the save
		   nevertheless completed successfully. It is
		   therefore wrong to signal the ERR_BACKEND_PERM
		   error here which implies that the saving itself
		   failed! What should we do? */
                PWARN("unable to chmod filename %s: %s",
                        tmp_name ? tmp_name : "(null)", 
                        strerror(errno) ? strerror(errno) : ""); 
#if VFAT_DOESNT_SUCK  /* chmod always fails on vfat fs */
                g_free(tmp_name);
                return FALSE;
#endif
            }
	    /* Don't try to change the owner. Only root can do
	       that. */
            if(chown(tmp_name, -1, statbuf.st_gid) != 0)
            {
	        /* qof_backend_set_error(be, ERR_BACKEND_PERM); */
	        /* A failed chown doesn't mean that the saving itself
		   failed. So don't abort with an error here! */
                PWARN("unable to chown filename %s: %s",
                        tmp_name ? tmp_name : "(null)", 
                        strerror(errno) ? strerror(errno) : ""); 
#if VFAT_DOESNT_SUCK /* chown always fails on vfat fs */
                /* g_free(tmp_name);
		   return FALSE; */
#endif
            }
        }
        if(unlink(datafile) != 0 && errno != ENOENT)
        {
            qof_backend_set_error(be, ERR_FILEIO_BACKUP_ERROR);
            PWARN("unable to unlink filename %s: %s",
                  datafile ? datafile : "(null)", 
                  strerror(errno) ? strerror(errno) : ""); 
            g_free(tmp_name);
            return FALSE;
        }
        if(!gnc_int_link_or_make_backup(fbe, tmp_name, datafile))
        {
            qof_backend_set_error(be, ERR_FILEIO_BACKUP_ERROR);
            g_free(tmp_name);
            return FALSE;
        }
        if(unlink(tmp_name) != 0)
        {
            qof_backend_set_error(be, ERR_BACKEND_PERM);
            PWARN("unable to unlink temp filename %s: %s", 
                   tmp_name ? tmp_name : "(null)", 
                   strerror(errno) ? strerror(errno) : ""); 
            g_free(tmp_name);
            return FALSE;
        }
        g_free(tmp_name);

        /* Since we successfully saved the book, 
         * we should mark it clean. */
        qof_book_mark_saved (book);
        LEAVE (" sucessful save of book=%p to file=%s", book, datafile);
        return TRUE;
    }
    else
    {
        if(unlink(tmp_name) != 0)
        {
            switch (errno) {
            case ENOENT:     /* tmp_name doesn't exist?  Assume "RO" error */
            case EACCES:
            case EPERM:
            case EROFS:
              be_err = ERR_BACKEND_READONLY;
              break;
            default:
              be_err = ERR_BACKEND_MISC;
            }
            qof_backend_set_error(be, be_err);
            PWARN("unable to unlink temp_filename %s: %s", 
                   tmp_name ? tmp_name : "(null)", 
                   strerror(errno) ? strerror(errno) : ""); 
            /* already in an error just flow on through */
        }
        g_free(tmp_name);
        return FALSE;
    }
    return TRUE;
}

/* ================================================================= */

static int
gnc_file_be_select_files (const struct dirent *d)
{
    int len = strlen(d->d_name) - 4;

    if (len <= 0)
        return(0);
  
    return((strcmp(d->d_name + len, ".LNK") == 0) ||
           (strcmp(d->d_name + len, ".xac") == 0) ||
           (strcmp(d->d_name + len, ".log") == 0));
}

static void
gnc_file_be_remove_old_files(FileBackend *be)
{
    struct dirent *dent;
    DIR *dir;
    struct stat lockstatbuf, statbuf;
    int pathlen;
    time_t now;

    if (stat (be->lockfile, &lockstatbuf) != 0)
        return;
    pathlen = strlen(be->fullpath);

    /*
     * Clean up any lockfiles from prior crashes, and clean up old
     * data and log files.  Scandir will do a fist pass on the
     * filenames and cull the directory down to just files with the
     * appropriate extensions.  Pity you can't pass user data into
     * scandir...
     */

    /*
     * Unfortunately scandir() is not portable, so re-write this
     * function without it.  Note that this version will be even a bit
     * faster because it does not have to sort, malloc, or anything
     * else that scandir did, and it only performs a single pass
     * through the directory rather than one pass through the
     * directory and then one pass over the 'matching' files. --
     * warlord@MIT.EDU 2002-05-06
     */
    
    dir = opendir (be->dirname);
    if (!dir)
        return;

    now = time(NULL);
    while((dent = readdir(dir)) != NULL) {
        char *name;
        int len;

        if (gnc_file_be_select_files (dent) == 0)
             continue;

        name = g_strconcat(be->dirname, "/", dent->d_name, NULL);
        len = strlen(name) - 4;

        /* Is this file associated with the current data file */
        if (strncmp(name, be->fullpath, pathlen) == 0) 
        {
            if ((safe_strcmp(name + len, ".LNK") == 0) &&
                /* Is a lock file. Skip the active lock file */
                (safe_strcmp(name, be->linkfile) != 0) &&
                /* Only delete lock files older than the active one */
                (stat(name, &statbuf) == 0) &&
                (statbuf.st_mtime <lockstatbuf.st_mtime)) 
            {
                PINFO ("unlink lock file: %s", name);
                unlink(name);
            } 
            else if (file_retention_days > 0) 
            {
                time_t file_time;
                struct tm file_tm;
                int days;
                const char* res;

                PINFO ("file retention = %d days", file_retention_days);

                /* Is the backup file old enough to delete */
                memset(&file_tm, 0, sizeof(file_tm));
                res = strptime(name+pathlen+1, "%Y%m%d%H%M%S", &file_tm);
                file_time = mktime(&file_tm);
                days = (int)(difftime(now, file_time) / 86400);

                /* Make sure this file actually has a date before unlinking */
                if (res && res != name+pathlen+1 &&
                    /* We consumed some but not all of the filename */
		    !strcmp(res, ".xac") &&
                    file_time > 0 &&
                    /* we actually have a reasonable time and it is old enough */
                    days > file_retention_days) 
                {
                    PINFO ("unlink stale (%d days old) file: %s", days, name);
                    unlink(name);
                }
            }
        }
        g_free(name);
    }
    closedir (dir);
}

static void
file_sync_all(QofBackend* be, QofBook *book)
{
    FileBackend *fbe = (FileBackend *) be;
    ENTER ("book=%p, primary=%p", book, fbe->primary_book);

    /* We make an important assumption here, that we might want to change
     * in the future: when the user says 'save', we really save the one,
     * the only, the current open book, and nothing else.  We do this
     * because we assume that any other books that we are dealing with
     * are 'read-only', non-editable, because they are closed books.
     * If we ever want to have more than one book open read-write,
     * this will have to change.
     */
    if (NULL == fbe->primary_book) fbe->primary_book = book;
    if (book != fbe->primary_book) return;

    gnc_file_be_write_to_file (fbe, book, fbe->fullpath, TRUE);
    gnc_file_be_remove_old_files (fbe);
    LEAVE ("book=%p", book);
}

/* ================================================================= */
/* Routines to deal with the creation of multiple books.
 * The core design assumption here is that the book
 * begin-edit/commit-edit routines are used solely to write out
 * closed accounting periods to files.  They're not currently
 * designed to do anything other than this. (Although they could be).
 */

static char *
build_period_filepath (FileBackend *fbe, QofBook *book)
{
    int len;
    char *str, *p, *q;

    len = strlen (fbe->fullpath) + GUID_ENCODING_LENGTH + 14;
    str = g_new (char, len);
    strcpy (str, fbe->fullpath);

    /* XXX it would be nice for the user if we made the book 
     * closing date and/or title part of the file-name. */
    p = strrchr (str, '/');
    p++;
    p = stpcpy (p, "book-");
    p = guid_to_string_buff (qof_book_get_guid(book), p);
    p = stpcpy (p, "-");
    q = strrchr (fbe->fullpath, '/');
    q++;
    p = stpcpy (p, q);
    p = stpcpy (p, ".gml");

    return str;
}

static void
file_begin_edit (QofBackend *be, QofInstance *inst)
{
    if (0) build_period_filepath(0, 0);
#if BORKEN_FOR_NOW
    FileBackend *fbe = (FileBackend *) be;
    QofBook *book = gp;
    const char * filepath;

    QofIdTypeConst typ = QOF_ENTITY(inst)->e_type;
    if (strcmp (GNC_ID_PERIOD, typ)) return;
    filepath = build_period_filepath(fbe, book);
    PINFO (" ====================== book=%p filepath=%s\n", book, filepath);

    if (NULL == fbe->primary_book)
    {
        PERR ("You should have saved the data "
              "at least once before closing the books!\n");
    }
    /* XXX To be anal about it, we should really be checking to see
     * if there already is a file with this book GUID, and disallowing
     * further progress.  This is because we are not allowed to 
     * modify books that are closed (They should be treated as 
     * 'read-only').
     */
#endif
}

static void
file_rollback_edit (QofBackend *be, QofInstance *inst)
{
#if BORKEN_FOR_NOW
    QofBook *book = gp;

    if (strcmp (GNC_ID_PERIOD, typ)) return;
    PINFO ("book=%p", book);
#endif
}

static void
file_commit_edit (QofBackend *be, QofInstance *inst)
{
#if BORKEN_FOR_NOW
    FileBackend *fbe = (FileBackend *) be;
    QofBook *book = gp;
    const char * filepath;

    if (strcmp (GNC_ID_PERIOD, typ)) return;
    filepath = build_period_filepath(fbe, book);
    PINFO (" ====================== book=%p filepath=%s\n", book, filepath);
    gnc_file_be_write_to_file(fbe, book, filepath, FALSE);

    /* We want to force a save of the current book at this point,
     * because if we don't, and the user forgets to do so, then
     * there'll be the same transactions in the closed book,
     * and also in the current book. */
    gnc_file_be_write_to_file (fbe, fbe->primary_book, fbe->fullpath, TRUE);
#endif
}

/* ---------------------------------------------------------------------- */


/* Load financial data from a file into the book, automatically
   detecting the format of the file, if possible.  Return FALSE on
   error, and set the error parameter to indicate what went wrong if
   it's not NULL.  This function does not manage file locks in any
   way. */

static void
gnc_file_be_load_from_file (QofBackend *bend, QofBook *book)
{
    QofBackendError error;
    gboolean rc;
    FileBackend *be = (FileBackend *) bend;

	error = ERR_BACKEND_NO_ERR;
    be->primary_book = book;

    switch (gnc_file_be_determine_file_type(be->fullpath))
    {
    case GNC_BOOK_XML2_FILE:
        rc = qof_session_load_from_xml_file_v2 (be, book);
        if (FALSE == rc) error = ERR_FILEIO_PARSE_ERROR;
        break;

    case GNC_BOOK_XML1_FILE:
        rc = qof_session_load_from_xml_file (book, be->fullpath);
        if (FALSE == rc) error = ERR_FILEIO_PARSE_ERROR;
        break;
    case GNC_BOOK_BIN_FILE:
        qof_session_load_from_binfile(book, be->fullpath);
        error = gnc_get_binfile_io_error();
        break;
    default:
        PWARN("File not any known type");
        error = ERR_FILEIO_UNKNOWN_FILE_TYPE;
        break;
    }

    if(error != ERR_BACKEND_NO_ERR) 
    {
        qof_backend_set_error(bend, error);
    }

    /* We just got done loading, it can't possibly be dirty !! */
    qof_book_mark_saved (book);
}

/* ---------------------------------------------------------------------- */

static gboolean
gnc_file_be_save_may_clobber_data (QofBackend *bend)
{
  struct stat statbuf;
  if (!bend->fullpath) return FALSE;

  /* FIXME: Make sure this doesn't need more sophisticated semantics
   * in the face of special file, devices, pipes, symlinks, etc. */
  if (stat(bend->fullpath, &statbuf) == 0) return TRUE;
  return FALSE;
}


static void
gnc_file_be_write_accounts_to_file(QofBackend *be, QofBook *book)
{
    const gchar *datafile;

    datafile = ((FileBackend *)be)->fullpath;
    gnc_book_write_accounts_to_xml_file_v2(be, book, datafile);
}

/* ================================================================= */
#if 0 //def GNUCASH_MAJOR_VERSION
QofBackend *
libgncmod_backend_file_LTX_gnc_backend_new(void)
{

    fbe->dirname = NULL;
    fbe->fullpath = NULL;
    fbe->lockfile = NULL;
    fbe->linkfile = NULL;
    fbe->price_lookup = NULL;
    fbe->lockfd = -1;

    fbe->primary_book = NULL;

    return be;
}
#endif
QofBackend*
gnc_backend_new(void)
{
	FileBackend *gnc_be;
	QofBackend *be;

	gnc_be = g_new0(FileBackend, 1);
	be = (QofBackend*) gnc_be;
	qof_backend_init(be);

	be->session_begin = file_session_begin;
	be->session_end = file_session_end;
	be->destroy_backend = file_destroy_backend;

	be->load = gnc_file_be_load_from_file;
	be->save_may_clobber_data = gnc_file_be_save_may_clobber_data;

	/* The file backend treats accounting periods transactionally. */
	be->begin = file_begin_edit;
	be->commit = file_commit_edit;
	be->rollback = file_rollback_edit;

	/* The file backend always loads all data ... */
	be->compile_query = NULL;
	be->free_query = NULL;
	be->run_query = NULL;

	be->counter = NULL;

	/* The file backend will never be multi-user... */
	be->events_pending = NULL;
	be->process_events = NULL;

	be->sync = file_sync_all;
	be->load_config = gnc_file_be_set_config;
	be->get_config = gnc_file_be_get_config;

	gnc_be->export = gnc_file_be_write_accounts_to_file;
	gnc_be->dirname = NULL;
	gnc_be->fullpath = NULL;
	gnc_be->lockfile = NULL;
	gnc_be->linkfile = NULL;
	gnc_be->lockfd = -1;

	gnc_be->primary_book = NULL;
	return be;
}

static void
gnc_provider_free (QofBackendProvider *prov)
{
        prov->provider_name = NULL;
        prov->access_method = NULL;
        g_free (prov);
}

void
gnc_provider_init(void)
{
	QofBackendProvider *prov;
#ifdef ENABLE_NLS
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif
        prov = g_new0 (QofBackendProvider, 1);
        prov->provider_name = "GnuCash File Backend Version 2";
        prov->access_method = "file";
        prov->partial_book_supported = FALSE;
        prov->backend_new = gnc_backend_new;
        prov->provider_free = gnc_provider_free;
	prov->check_data_type = gnc_determine_file_type;
        qof_backend_register_provider (prov);
}

/* ========================== END OF FILE ===================== */
