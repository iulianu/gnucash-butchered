/********************************************************************\
 * Transaction.h -- defines transaction for xacc (X-Accountant)     *
 * Copyright (C) 1997 Robin D. Clark                                *
 * Copyright (C) 1997, 1998 Linas Vepstas                           *
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

#ifndef __XACC_TRANSACTION_H__
#define __XACC_TRANSACTION_H__

#include "config.h"

#include <time.h>

/* Values for the reconciled field in Transaction: */
#define CREC 'c'              /* The transaction has been cleared        */
#define YREC 'y'              /* The transaction has been reconciled     */
#define FREC 'f'              /* frozen into accounting period           */
#define NREC 'n'              /* not reconciled or cleared               */

/** STRUCTS *********************************************************/
/* The debit & credit pointers are used to implement a double-entry 
 * accounting system.  Basically, the idea with double entry is that
 * there is always an account that is debited, and another that is
 * credited.  These two pointers identify the two accounts. 
 */

/* A split transaction is one which shows up as a credit (or debit) in
 * one account, and peices of it show up as debits (or credits) in other
 * accounts.  Thus, a single credit-card transaction might be split
 * between "dining", "tips" and "taxes" categories.
 */

typedef struct _account       Account;
typedef struct _account_group AccountGroup;
typedef struct _split         Split;
typedef struct _transaction   Transaction;


/** PROTOTYPES ******************************************************/
/*
 * The xaccMallocTransaction() will malloc memory and initilize it.
 *    Once created, it is usually unsafe to merely "free" this memory;
 *    the xaccTransDestroy() method should be called.
 *
 * The xaccInitTransaction() method will initialize the indicated memory 
 *    area.  Handy for on-stack trasnactions.
 */ 
Transaction * xaccMallocTransaction (void); 
void          xaccInitTransaction (Transaction *);

/* The xaccTransDestroy() method will remove all 
 *    of the splits from each of thier accounts, free the memory
 *    associated with them.  This routine must be followed by either
 *    an xaccTransCommitEdit(), in which case the transaction 
 *    memory will be freed, or by xaccTransRollbackEdit(), in which 
 *    case nothing at all is freed, and everything is put back into 
 *    original order.
 */
void          xaccTransDestroy (Transaction *);

/* The xaccTransBeginEdit() method must be called before any changes
 *    are made to a transaction or any of its componenet splits.  If 
 *    this is not done, errors will result.  If the defer flag is set, 
 *    then the automated re-balancing of all splits in this transaction
 *    is defered until the xaccTransCommitEdit() call. This allows 
 *    multiple splits to be edited, and prices fiddled with, and the whole
 *    system sent temporarily out of balance, up until the Commit
 *    call is made when double-entry is once again enforced.
 *
 * The xaccTransCommitEdit() method should be used toindicate that 
 *    all of the manipulations on teh transaction are complete, and
 *    that these should be made permanent.  Note that this routine
 *    may result in the deletion of the transaction, if the transaction 
 *    is "empty" (has no splits, or * has a single split in it whose 
 *    value is non-zero.)
 *
 * The xaccTransRollbackEdit() routine rejects all edits made, and 
 *    sets the transaction back to where it was before the editing 
 *    started.  This includes restoring any deleted splits, removing
 *    any added splits, and undoing the effects of xaccTransDestroy,
 *    as well as restoring prices, memo's descriptions, etc.
 */
void          xaccTransBeginEdit (Transaction *, int defer);
void          xaccTransCommitEdit (Transaction *);
void          xaccTransRollbackEdit (Transaction *);

/*
 * The xaccTransSetDateSecs() method will modify the posted date 
 *    of the transaction.  (Footnote: this shouldn't matter to a user,
 *    but anyone modifying the engine should understand that when
 *    xaccTransCommitEdit() is called, the date order of each of the 
 *    component splits will be checked, and will be resotred in 
 *    ascending date order.)
 *
 * The xaccTransSetDate() method does the same thing as 
 *    xaccTransSetDateSecs(), but takes a convenient day-month-year format.
 *
 * The xaccTransSetDateToday() method does the same thing as 
 *    xaccTransSetDateSecs(), but sets the date to the current system
 *    date/time.
 */
void          xaccTransSetDate (Transaction *, int day, int mon, int year);
void          xaccTransSetDateSecs (Transaction *, time_t);
void          xaccTransSetDateToday (Transaction *);

/* set the Num and Description fields ... */
void          xaccTransSetNum (Transaction *, const char *);
void          xaccTransSetDescription (Transaction *, const char *);

/* The xaccTransSetMemo() and xaccTransSetAction() methods are
 * convenience routines to keep the memo and action fields
 * of two-split trasnactions in sync.  If the transaction has
 * precisely two splits, then these routines will set the memo 
 * and action of both splits.   Otherwise, they will set the
 * memo and action of the first split (source split) only.
 */
void          xaccTransSetMemo (Transaction *, const char *);
void          xaccTransSetAction (Transaction *, const char *);

/*
 * The xaccTransAppendSplit() method will append the indicated 
 *    split to the collection of splits in this transaction.
 *    If the split is alredy a part of another transaction,
 *    it will be removed from that transaction first.
 */
void          xaccTransAppendSplit (Transaction *, Split *);

/* 
 * The xaccSplitDestroy() method will update its parent account and 
 *    transaction in a consistent maner, resulting in the complete 
 *    unlinking of the split, and the freeing of it's associated memory.
 *    The goal of this routine is to perform the removal and destruction
 *    of the split in an atomic fashion, with no chance of accidentally
 *    leaving the accounting structure out-of-balance or otherwise
 *    inconsistent.
 *
 *    If the deletion of the split leaves the transaction "empty",
 *    then the transaction will be marked for deletion.  (It will
 *    not be deleted until the xaccTransCommitEdit() routine is called.)
 *    The transaction is considered "empty" if it has no splits in it, 
 *    or it has only one split left, and that split is not a price split
 *    (i.e. has a non-zero value).  Transactions with only one split in 
 *    them are valid if and only if the value of that split is zero.
 */
void          xaccSplitDestroy (Split *);

/* ------------- gets --------------- */
/* The xaccTransGetSplit() method returns a pointer to each of the 
 *    splits in this transaction.  Valid values for i are zero to 
 *    (number_of__splits-1).  An invalid value of i will cause NULL to
 *    be returned.  A conenient way of cycling through all splits is
 *    to start at zero, and kep incrementing until a null value is returned.
 */
Split *       xaccTransGetSplit (Transaction *trans, int i);

/* These routines return the Num (or ID field), the description, 
 * and the date field.
 */
char *        xaccTransGetNum (Transaction *);
char *        xaccTransGetDescription (Transaction *);
time_t        xaccTransGetDate (Transaction *);

/* return the number of splits */
int           xaccTransCountSplits (Transaction *trans);

/* ------------- splits --------------- */
Split       * xaccMallocSplit (void);
void          xaccInitSplit   (Split *);    /* clears a split struct */

/* The memo is an arbitrary string associated with a split.
 *    Users typically type in free form text from the GUI.
 */
void          xaccSplitSetMemo (Split *, const char *);

/* The Action is essentially an arbitrary string, but is 
 * meant to be conveniently limited to a menu of selections 
 * such as  "Buy", "Sell", "Interest", etc.  However,
 * as far as the engine is concerned, its an arbitrary string.
 */
void          xaccSplitSetAction (Split *, const char *);

/* The Reconcile is a single byte, whose values are typically
 * are "no", "cleared" and "reconciled"
 */
void          xaccSplitSetReconcile (Split *, char);

/* 
 * The following four functions set the prices and amounts.
 * All of the routines always maintain balance: that is, 
 * invoking any of them will cause other splits in the transaction
 * to be modified so that the net value of the transaction is zero. 
 *
 * The xaccSplitSetShareAmount() method sets the number of shares
 *     that the split should have.  
 *
 * The xaccSplitSetSharePrice() method sets the price of the split.
 *
 * The xaccSplitSetValue() method adjusts the number of shares in 
 *     the split so that the number of shares times the share price
 *     equals the value passed in.
 *
 * The xaccSplitSetSharePriceAndAmount() method will simultaneously
 *     update the share price and the number of shares. This 
 *     is a utility routine that is equivalent to a xaccSplitSetSharePrice()
 *     followed by and xaccSplitSetAmount(), except that it incurs the
 *     processing overhead of balancing only once, instead of twise.
 */

void         xaccSplitSetSharePriceAndAmount (Split *, double price,
                                              double amount);
void         xaccSplitSetShareAmount (Split *, double);
void         xaccSplitSetSharePrice (Split *, double);
void         xaccSplitSetValue (Split *, double);
void         xaccSplitSetBaseValue (Split *s, double value, char * base_currency);


/* The following four subroutines return the running balance up
 * to & including the indicated split.
 * 
 * The balance is the currency-denominated balance.  For accounts
 * with non-unit share prices, it is correctly adjusted for
 * share prices.
 * 
 * The share-balance is the number of shares. 
 * Price fluctuations do not change the share balance.
 * 
 * The cleared-balance is the currency-denominated balance 
 * of all transactions that have been marked as cleared or reconciled.
 * It is correctly adjusted for price fluctuations.
 * 
 * The reconciled-balance is the currency-denominated balance
 * of all transactions that have been marked as reconciled.
 */

double xaccSplitGetBalance (Split *);
double xaccSplitGetClearedBalance (Split *);
double xaccSplitGetReconciledBalance (Split *);
double xaccSplitGetShareBalance (Split *);
double xaccSplitGetBaseValue (Split *s, char *base_currency);


/* return the parent transaction of the split */
Transaction * xaccSplitGetParent (Split *);

/* return the memo, action strings */
char *        xaccSplitGetMemo (Split *split);
char *        xaccSplitGetAction (Split *split);

/* return the value of the reconcile flag */
char          xaccSplitGetReconcile (Split *split);
double        xaccSplitGetShareAmount (Split * split);
double        xaccSplitGetSharePrice (Split * split);
double        xaccSplitGetValue (Split * split);

Account *     xaccSplitGetAccount (Split *);

/********************************************************************\
 * sorting comparison function
 *
 * The xaccTransOrder(ta,tb) method is useful for sorting.
 *    returns a negative value if transaction ta is dated earlier than tb, 
 *    returns a positive value if transaction ta is dated later than tb,
 *    then compares num, description and docref values, using the strcmp()
 *    c-library routine, returning  what strcmp would return.
 *    Finally, it returns zero if all of the above match.
 *    Note that it does *NOT* compare its member splits.
 *
 * The xaccSplitOrder(sa,sb) method is useful for sorting.
 *    returns a negative value if split sa has a smaller currency-value than sb,
 *    returns a positive value if split sa has a larger currency-value than sb,
 *    returns a negative value if split sa has a smaller share-price than sb,  
 *    returns a positive value if split sa has a larger share-price than sb,  
 *    then compares memo and action using the strcmp()
 *    c-library routine, returning  what strcmp would return.
 *    Then it compares the reconciled flags, then the reconciled dates,
 *    Then it strcmps() the docref fields.
 *    Finally, it returns zero if all of the above match.
 *    Note that it does *NOT* compare its parent transaction.
 *
 * The xaccSplitDateOrder(sa,sb) method is useful for sorting.
 *    It is just like the xaccSplitOrder() routine, except that it first
 *    calls xaccTransOrder(), and then does split comparisons only if
 *    xaccTransOrder returned zero (i.e. that everything matched).
 *
 * The xaccTransMatch() method is just like the xaccTransOrder method, 
 *    except that it also performs a comparison of each of its child splits,
 *    and returns a zero (match) condition only if they match as well.
 *    Note that split-matching includes matching thier parent accounts.
 */

int  xaccTransOrder     (Transaction **ta, Transaction **tb);
int  xaccTransMatch     (Transaction **ta, Transaction **tb);
int  xaccSplitOrder     (Split **sa, Split **sb);
int  xaccSplitDateOrder (Split **sa, Split **sb);

/********************************************************************\
 * Miscellaneous utility routines.
\********************************************************************/
/*
 * count the number of transactions in the null-terminated array
 */
int xaccCountTransactions (Transaction **tarray);

/* count the number of splits in the indicated array */
int xaccCountSplits (Split **sarray);

/* 
 * The xaccGetAccountByName() is a convenience routine that 
 *    is essentially identical to xaccGetPeerAccountFromName(),
 *    except that it accepts the handy transaction as root.
 */
Account * xaccGetAccountByName (Transaction *, const char *);

/* 
 * The xaccGetOtherSplit() is a convenience routine that returns
 *    the other of a pair of splits.  If there are more than two 
 *    splits, it returns NULL.
 */
Split * xaccGetOtherSplit (Split *);

/* The xaccIsPeerSplit() is a convenience routine that returns
 *    a non-zero value if the two splits share a common 
 *    parent transaction, else it returns zero.
 */
int xaccIsPeerSplit (Split *, Split *);

#endif /* __XACC_TRANSACTION_H__ */
