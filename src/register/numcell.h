/********************************************************************\
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

/*
 * FILE:
 * numcell.h
 *
 * FUNCTION:
 * The NumCell object implements a number handling cell. It
 * supports a number of accelerator keys for number entry.
 *
 * HISTORY:
 * Copyright (c) 2000 Dave Peticolas <peticola@cs.ucdavis.edu>
 */
 
#ifndef __NUM_CELL_H__
#define __NUM_CELL_H__

#include "basiccell.h"

typedef struct _NumCell
{
  BasicCell cell;
  long int next_num;
  gboolean next_num_set;
} NumCell;

NumCell * xaccMallocNumCell (void);
void      xaccInitNumCell (NumCell *);
void      xaccDestroyNumCell (NumCell *);

void      xaccSetNumCellValue (NumCell *cell, const char *str);
void      xaccSetNumCellLastNum (NumCell *cell, const char *str);

#endif /* __NUM_CELL_H__ */
