/*
 * FILE:
 * Query.h
 *
 * DESCRIPTION:
 * Provide a simple query engine interface.
 *
 * HISTORY:
 * created by Linas Vepstas Sept 1998
 * Copyright (c) 1998 Linas Vepstas
 */

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
 * along with this program; if not, write to the Free Software      *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.        *
\********************************************************************/

#ifndef __GNUCASH_QUERY_H__
#define __GNUCASH_QUERY_H__

#include <time.h>

#include "gnc-common.h"
#include "Account.h"
#include "Transaction.h"

typedef struct _Query Query;

/* sorting orders */
enum {
  BY_STANDARD,
  BY_DATE,
  BY_NUM,
  BY_AMOUNT,
  BY_MEMO,
  BY_DESC,
  BY_NONE
};

Query * xaccMallocQuery (void);
void    xaccInitQuery (Query *);
void    xaccFreeQuery (Query *);

/* The xaccSetAccountList() method is used to define the set
 *    of accounts the should be queried.
 */
void xaccQuerySetAccounts (Query *, Account **list);
void xaccQueryAddAccount (Query *, Account *acc);

/* The xaccQuerySetMaxSplits() method sets the maximum number
 *    of splits to return as a result of a query.
 */
void  xaccQuerySetMaxSplits (Query *, int);

/* The xaccQuerySetDateRange() method sets the date range
 *    for the query.  The query will return only those splits
 *    that are within this date range. The arguments "earliest"
 *    and "latest" are seconds before or since 00:00:00 Jan 1 1970.
 */
void xaccQuerySetDateRange (Query *, time_t earliest, time_t latest);
#ifndef SWIG  /* swig chokes on long long */
void xaccQuerySetDateRangeL (Query *, long long earliest, long long latest);
#endif

void xaccQuerySetEarliest (Query *, time_t earliest);
void xaccQuerySetLatest   (Query *, time_t latest);

time_t xaccQueryGetEarliest (Query *);
time_t xaccQueryGetLatest   (Query *);

/* The xaccQueryShowEarliestDateFound function and its partner tell
 * the Query to use the earliest (resp. latest) splits found. */
void xaccQueryShowEarliestDateFound (Query *);
void xaccQueryShowLatestDateFound   (Query *);

/* The xaccQuerySetSortOrder() method sets the sort order that
 *    should be used on the splits.  The three arguments should 
 *    be chosen from the enums above.  The first argument has the
 *    sort priority, the next the next, etc.
 */
void xaccQuerySetSortOrder (Query *, int, int, int); 

/* The xaccQueryGetSplits() method returns a list of splits
 *    matching the query and sorting criteria previously set up.
 */
Split ** xaccQueryGetSplits (Query *);

/* The xaccQueryGetEaliestDateFound() routine will return the 
 *    earliest date that appears in the list of returned splits.
 */
time_t xaccQueryGetEarliestDateFound (Query *);
time_t xaccQueryGetLatestDateFound (Query *);

#endif /* __GNUCASH_QUERY_H__ */
