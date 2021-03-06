/********************************************************************\
 * io-utils.cpp -- implementation for gnucash file i/o utils        *
 *                                                                  *
 * Copyright (C) 2001 James LewisMoss <dres@debian.org>             *
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

#include "config.h"

#include <stdio.h>

#include <glib.h>

#include "gnc-xml.h"
#include "gnc-xml.h"
#include "io-utils.h"
#include "gnc-gconf-utils.h"

/*
  <!-- Local variables: -->
  <!-- mode: C          -->
  <!-- End:             -->
*/

static const gchar *emacs_trailer =
    "<!-- Local variables: -->\n"
    "<!-- mode: xml        -->\n"
    "<!-- End:             -->\n";


bool
write_emacs_trailer(FILE *out)
{
    return fprintf(out, "%s", emacs_trailer) >= 0;
}

static bool
write_one_account(FILE *out,
                  Account *account,
                  sixtp_gdv2 *gd,
                  bool allow_incompat)
{
    xmlNodePtr accnode;

    accnode =
        gnc_account_dom_tree_create(account, gd && gd->exporting, allow_incompat);

    xmlElemDump(out, NULL, accnode);
    xmlFreeNode(accnode);

    if (ferror(out) || fprintf(out, "\n") < 0)
        return FALSE;

    gd->counter.accounts_loaded++;
    run_callback(gd, "account");
    return TRUE;
}

bool
write_account_tree(FILE *out, Account *root, sixtp_gdv2 *gd)
{
    bool allow_incompat = true;
    bool success = true;

    if (allow_incompat)
        if (!write_one_account(out, root, gd, allow_incompat))
            return false;

    AccountList_t descendants = gnc_account_get_descendants(root);
    for (AccountList_t::iterator node = descendants.begin(); node != descendants.end(); node++)
    {
        if (!write_one_account(out, *node, gd, allow_incompat))
        {
            success = false;
            break;
        }
    }

    return success;
}

bool
write_accounts(FILE *out, QofBook *book, sixtp_gdv2 *gd)
{
    return write_account_tree(out, gnc_book_get_root_account(book), gd);
}
