/********************************************************************\
 * io-utils.c -- implementation for gnucash file i/o utils          *
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
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/

#include "config.h"

#include <stdio.h>

#include <glib.h>

#include "Group.h"
#include "gnc-book.h"
#include "gnc-xml.h"
#include "gnc-xml.h"
#include "io-utils.h"

/*
  <!-- Local variables: -->
  <!-- mode: C          -->
  <!-- End:             -->
*/

static const gchar *emacs_trailer =
"<!-- Local variables: -->\n"
"<!-- mode: xml        -->\n"
"<!-- End:             -->\n";


void
write_emacs_trailer(FILE *out)
{
    fprintf(out, emacs_trailer);
}

void
write_account_group(FILE *out, AccountGroup *grp, sixtp_gdv2 *gd)
{
    GList *list;
    GList *node;

    list = xaccGroupGetAccountList(grp);

    for (node = list; node; node = node->next) 
    {
        xmlNodePtr accnode;
        AccountGroup *newgrp;
        
        accnode = gnc_account_dom_tree_create((Account*)(node->data),
					      gd && gd->exporting);

        xmlElemDump(out, NULL, accnode);
        fprintf(out, "\n");

        xmlFreeNode(accnode);
	gd->counter.accounts_loaded++;
	run_callback(gd, "account");

        newgrp = xaccAccountGetChildren((Account*)(node->data));

        if (newgrp)
        {
            write_account_group(out, newgrp, gd);
        }
    }
}

void
write_accounts(FILE *out, GNCBook *book, sixtp_gdv2 *gd)
{
    write_account_group(out, gnc_book_get_group(book), gd);
}
