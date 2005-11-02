/***************************************************************************
 *            test-load-xml2.c
 *
 *  Fri Oct  7 20:51:46 2005
 *  Copyright  2005  Neil Williams
 *  linux@codehelp.co.uk
 ****************************************************************************/
/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
/* @file test-load-xml2.c
 * @brief test the loading of a version-2 gnucash XML file
 */

#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include "cashobjects.h"
#include "Group.h"
#include "TransLog.h"
#include "gnc-engine.h"
#include "gnc-backend-file.h"
#include "io-gncxml-v2.h"

#include "test-stuff.h"
#include "test-engine-stuff.h"
#include "test-file-stuff.h"

#define GNC_LIB_NAME "libgnc-backend-file.la"
#define GNC_LIB_INIT "gnc_provider_init"

static void
remove_files_pattern(const char *begining, const char *ending)
{
}

static void
remove_locks(const char *filename)
{
    struct stat buf;
    char *to_remove;
    
    {
        to_remove = g_strdup_printf("%s.LCK", filename);
        if(stat(to_remove, &buf) != -1)
        {
            unlink(to_remove);
        }
        g_free(to_remove);
    }
    
    remove_files_pattern(filename, ".LCK");
}

static void
test_load_file(const char *filename)
{
    QofSession *session;
    QofBook *book;
    AccountGroup *grp;
    gboolean ignore_lock;

    session = qof_session_new();

    remove_locks(filename);

    ignore_lock = (safe_strcmp(getenv("SRCDIR"), ".") != 0);
    qof_session_begin(session, filename, ignore_lock, FALSE);

    qof_session_load(session, NULL);
    book = qof_session_get_book (session);

    grp = xaccGetAccountGroup(book);
    do_test (xaccGroupGetBook (grp) == book,
             "book and group don't match");

    do_test_args(
        qof_session_get_error(session) == ERR_BACKEND_NO_ERR,
        "session load xml2", __FILE__, __LINE__, "%d for file %s",
        qof_session_get_error(session), filename);

    qof_session_end(session);
}

int
main (int argc, char ** argv)
{
    const char *location = getenv("GNC_TEST_FILES");
    DIR *xml2_dir;

	qof_init();
	cashobjects_register();
	do_test(
		qof_load_backend_library ("../", GNC_LIB_NAME, GNC_LIB_INIT),
		" loading gnc-backend-file GModule failed");

    if (!location)
    {
        location = "test-files/xml2";
    }


    xaccLogDisable();
    
    if((xml2_dir = opendir(location)) == NULL)
    {
        failure("unable to open xml2 directory");
    }
    else
    {
        struct dirent *entry;

        while((entry = readdir(xml2_dir)) != NULL)
        {
            if(strstr(entry->d_name, ".gml2") != NULL)
            {
                struct stat file_info;
                char *to_open = g_strdup_printf("%s/%s", location,
                                                entry->d_name);
                if(stat(to_open, &file_info) != 0)
                {
                    failure("unable to stat file");
                }
                else
                {
                    if(!S_ISDIR(file_info.st_mode))
                    {
                        test_load_file(to_open);
                    }
                }
                g_free(to_open);
            }
        }
    }

    closedir(xml2_dir);

    print_test_results();
    qof_close();
    return 0;
}
