/********************************************************************\
 * query-user.h -- functions for creating dialogs for GnuCash       * 
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
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
\********************************************************************/

#ifndef __QUERY_USER_H__
#define __QUERY_USER_H__

#include <guile/gh.h>

enum
{
  GNC_QUERY_YES = -1,
  GNC_QUERY_NO = -2,
  GNC_QUERY_CANCEL = -3
};


gboolean gnc_verify_dialog_parented(GtkWindow *parent, const char *message,
                                    gboolean yes_is_default);

void gnc_info_dialog(const char *message);
void gnc_info_dialog_parented(GtkWindow *parent, const char *message);

void gnc_warning_dialog(const char *message);

void gnc_error_dialog_parented(GtkWindow *parent, const char *message);

SCM gnc_choose_item_from_list_dialog(const char *title, SCM list_items);

#endif
