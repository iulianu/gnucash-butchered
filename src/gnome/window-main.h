/*******************************************************************\
 * window-main.h -- public GNOME main window functions              *
 * Copyright (C) 1998,1999 Linas Vepstas                            *
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

#ifndef __WINDOW_MAIN_H__
#define __WINDOW_MAIN_H__

#include "account-tree.h"


/** PROTOTYPES ******************************************************/

void mainWindow(void);

GNCAccountTree * gnc_get_current_account_tree();

Account * gnc_get_current_account();
GList *   gnc_get_current_accounts();

#endif
