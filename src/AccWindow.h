/********************************************************************\
 * AccWindow.h -- window for creating new accounts for GnuCash      *
 * Copyright (C) 1997 Robin D. Clark                                *
 * Copyright (C) 2000 Dave Peticolas                                *
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
 *                                                                  *
 *   Author: Rob Clark                                              *
 * Internet: rclark@cs.hmc.edu                                      *
 *  Address: 609 8th Street                                         *
 *           Huntington Beach, CA 92648-4632                        *
\********************************************************************/

#ifndef __ACC_WINDOW_H__
#define __ACC_WINDOW_H__

#include "config.h"

#include "Account.h"
#include "Group.h"

/** PROTOTYPES ******************************************************/
typedef struct _accwindow       AccWindow;
typedef struct _editaccwindow   EditAccWindow;

AccWindow       * accWindow( AccountGroup *grp);
EditAccWindow   * editAccWindow( Account *account );


/*
 * The xaccDestroyEditAccWindow() subroutine can be called from 
 * anywhere to shut down the Register window.  Used primarily when
 * destroying the underlying account.
 */
void xaccDestroyEditAccWindow (Account *);

void editAccountRefresh(Account *);

void xaccSetDefaultNewaccountCurrency(char *new_default_currency);

#endif /* __ACC_WINDOW_H__ */
