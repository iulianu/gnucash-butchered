/********************************************************************\
 * RegWindow.h -- the register window for xacc (X-Accountant)       *
 * Copyright (C) 1997 Robin D. Clark                                *
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

#ifndef __REGWINDOW_H__
#define __REGWINDOW_H__

#include <gtk/gtk.h>

#include "config.h"

#include "Account.h"

/** GLOBALS *********************************************************/

/** STRUCTS *********************************************************/
typedef struct _RegWindow RegWindow;

/** PROTOTYPES ******************************************************/
void       accRefresh (Account *);
RegWindow *regWindowSimple( Account *account );
RegWindow *regWindowAccGroup( Account *account_group );
RegWindow *regWindowLedger( Account *lead, Account **account, int type);

/*
 * The xaccDestroyRegWindow() subroutine can be called from 
 * anywhere to shut down the Register window.  Used primarily when
 * destroying the underlying account.
 */
void       xaccDestroyRegWindow (Account *);

#endif
