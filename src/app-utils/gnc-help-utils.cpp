/*
 * gnc-help-utils.cpp
 *
 * Copyright (C) 2007 Andreas Koehler <andi5.py@gmx.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact:
 *
 * Free Software Foundation           Voice:  +1-617-542-5942
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652
 * Boston, MA  02110-1301,  USA       gnu@gnu.org
 */

#include "config.h"
#include <glib.h>

#include "gnc-help-utils.h"

void
gnc_show_htmlhelp(const gchar *chmfile, const gchar *anchor)
{
    gchar *argv[3];

    g_return_if_fail(chmfile);

    argv[0] = "hh";
    argv[1] = g_strdup(chmfile);
    argv[2] = NULL;

    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                       NULL, NULL, NULL, NULL))
        if (g_file_test(chmfile, G_FILE_TEST_IS_REGULAR))
            g_warning("Found CHM help file, but could not spawn hh to open it");

    g_free(argv[1]);
}
