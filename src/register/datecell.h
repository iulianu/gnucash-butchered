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
 * datecell.h
 *
 * FUNCTION:
 * The DateCell object implements a date handling cell.  It
 * is able to display dates in several formats, it is also 
 * able to accept date input, and also supports a number of accelerator
 * keys.
 *
 * On output, this cell will display a date as mm/dd/yy
 * The actual date formate is compile-time configurable.
 *
 * hack alert -- make the display format run-time configurable, and 
 * appropriately internationalized.
 *
 * One input, this cell is able to parse dates that are entered.
 * It rejects any input that is not in the form of a date.  Input
 * also includes a number of accelerators:  
 *
 *     '+':
 *     '=':    increment day 
 *
 *     '_':
 *     '-':    decrement day 
 *
 *     '}':
 *     ']':    increment month 
 *
 *     '{':
 *     '[':    decrment month 
 *
 *     'M':
 *     'm':    begining of month 
 *
 *     'H':
 *     'h':    end of month 
 *
 *     'Y':
 *     'y':    begining of year 
 *
 *     'R':
 *     'r':    end of year 
 *
 *     'T':
 *     't':    today 
 *
 * 
 * METHODS:
 * The xaccSetDateCellValue() method accepts a numeric date and
 *    sets the contents of the date cell to that value.  The
 *    value day, month, year should follow the regular written
 *    conventions, i.e. day==(1..31), mon==(1..12), year==4 digits
 *
 * The xaccCommitDateCell() method commits any pending changes to the 
 *    value of the cell.  That is, it will take the cells current string 
 *    value, and parse it into a month-day-year value.
 *
 *    Why is this needed? Basically, while the user is typing into the
 *    cell, we do not even try to parse the date, lest we confuse things
 *    royally.  Normally, when the user leaves this cell, and moves to  
 *    another, we parse the date and print it nicely and cleanly into 
 *    the cell.  But it can happen that we want to get the accurate contents 
 *    of the date cell before we've left it, e.g. if the user has clicked
 *    on the "commit" button.  This is the routine to call for that.
 *
 * HISTORY:
 * Copyright (c) 1998, 1999, 2000 Linas Vepstas
 * Copyright (c) 2000 Dave Peticolas
 */
 
#ifndef __DATE_CELL_C__
#define __DATE_CELL_C__

#include <time.h>

#include "basiccell.h"
#include "date.h"


typedef struct _DateCell {
   BasicCell cell;
   struct tm date;
} DateCell;

/* installs a callback to handle date recording */
DateCell * xaccMallocDateCell (void);
void       xaccInitDateCell (DateCell *);
void       xaccDestroyDateCell (DateCell *);

void       xaccSetDateCellValue (DateCell *, int day, int mon, int year);  
void       xaccSetDateCellValueSecs (DateCell *, time_t secs);
void       xaccSetDateCellValueSecsL (DateCell *, long long secs);

void       xaccCommitDateCell (DateCell *);

void       xaccDateCellGetDate (DateCell *cell, Timespec *ts);

#endif /* __DATE_CELL_C__ */

/* --------------- end of file ---------------------- */
