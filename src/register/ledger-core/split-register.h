/********************************************************************\
 * split-register.h -- split register api                           *
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
/** @addtogroup GUI
@{ */
/** @addtogroup Ledger Checkbook Register Display Area

    @file split-register.h
    @brief Checkbook Register Display Area
    @author Copyright (C) 1998-2000 Linas Vepstas <linas@linas.org>

    The register is the spread-sheet-like area that looks like a 
    checkbook register.  It displays transactions, and allows the 
    user to edit transactions in-place.  The register does *not*
    contain any of the other window decorations that one might want 
    to have for a free standing window (e.g. menubars, toolbars, etc.)

    All user input to the register is handled by the 'cursor', which
    is mapped onto one of the displayed rows in the register.

    The layout of the register is configurable.  There's a broad
    variety of cell types to choose from:  date cells, which know 
    how to parse dates; price cells, which know how to parse prices,
    etc.  These cells can be laid out in any column; even a multi-row
    layout is supported.  The name 'split-register' is derived from
    the fact that this register can display multiple rows of 
    transaction splits underneath a transaction title/summary row.

\par Design Notes.
@{
Some notes about the "blank split":\n
Q: What is the "blank split"?\n
A: A new, empty split appended to the bottom of the ledger
 window.  The blank split provides an area where the user
 can type in new split/transaction info.  
 The "blank split" is treated in a special way for a number
 of reasons:
-  it must always appear as the bottom-most split
  in the Ledger window,
-  it must be committed if the user edits it, and 
  a new blank split must be created.
-   it must be deleted when the ledger window is closed.
To implement the above, the register "user_data" is used
to store an SRInfo structure containing the blank split.

 @}

\par Some notes on Commit/Rollback:
 @{
 * 
 * There's an engine component and a gui component to the commit/rollback
 * scheme.  On the engine side, one must always call BeginEdit()
 * before starting to edit a transaction.  When you think you're done,
 * you can call CommitEdit() to commit the changes, or RollbackEdit() to
 * go back to how things were before you started the edit. Think of it as
 * a one-shot mega-undo for that transaction.
 * 
 * Note that the query engine uses the original values, not the currently
 * edited values, when performing a sort.  This allows your to e.g. edit
 * the date without having the transaction hop around in the gui while you
 * do it.
 * 
 * On the gui side, commits are now performed on a per-transaction basis,
 * rather than a per-split (per-journal-entry) basis.  This means that
 * if you have a transaction with a lot of splits in it, you can edit them
 * all you want without having to commit one before moving to the next.
 * 
 * Similarly, the "cancel" button will now undo the changes to all of the
 * lines in the transaction display, not just to one line (one split) at a
 * time.
 * 

 @}
 \par Some notes on Reloads & Redraws:
 @{
 * Reloads and redraws tend to be heavyweight. We try to use change flags
 * as much as possible in this code, but imagine the following scenario:
 *
 * Create two bank accounts.  Transfer money from one to the other.
 * Open two registers, showing each account. Change the amount in one window.
 * Note that the other window also redraws, to show the new correct amount.
 * 
 * Since you changed an amount value, potentially *all* displayed
 * balances change in *both* register windows (as well as the ledger
 * balance in the main window).  Three or more windows may be involved
 * if you have e.g. windows open for bank, employer, taxes and your
 * entering a paycheck (or correcting a typo in an old paycheck).
 * Changing a date might even cause all entries in all three windows
 * to be re-ordered.
 *
 * The only thing I can think of is a bit stored with every table
 * entry, stating 'this entry has changed since lst time, redraw it'.
 * But that still doesn't avoid the overhead of reloading the table
 * from the engine.
 @}

    The Register itself is independent of GnuCash, and is designed 
    so that it can be used with other applications.
    The Ledger is an adaptation of the Register for use by GnuCash.
    The Ledger sets up an explicit visual layout, putting certain 
    types of cells in specific locations (e.g. date on left, summary 
    in middle, value at right), and hooks up these cells to 
    the various GnuCash financial objects.

    This code is also theoretically independent of the actual GUI
    toolkit/widget-set (it once worked with both Motif and Gnome).
    The actual GUI-toolkit specific code is supposed to be in a 
    GUI portability layer.  Over the years, some gnome-isms may 
    have snuck in; these should also be cleaned up.
*/
/** @{

*/

#ifndef SPLIT_REGISTER_H
#define SPLIT_REGISTER_H

#include <glib.h>

#include "Group.h"
#include "Transaction.h"
#include "table-allgui.h"

/** \brief Register types.
 *
 * "registers" are single-account display windows.
 * "ledgers" are multiple-account display windows */
typedef enum
{
  BANK_REGISTER,
  CASH_REGISTER,
  ASSET_REGISTER,
  CREDIT_REGISTER,
  LIABILITY_REGISTER,
  INCOME_REGISTER,
  EXPENSE_REGISTER,
  EQUITY_REGISTER,
  STOCK_REGISTER,
  CURRENCY_REGISTER,
  RECEIVABLE_REGISTER,
  PAYABLE_REGISTER,
  NUM_SINGLE_REGISTER_TYPES,

  GENERAL_LEDGER = NUM_SINGLE_REGISTER_TYPES,
  INCOME_LEDGER,
  PORTFOLIO_LEDGER,
  SEARCH_LEDGER,

  NUM_REGISTER_TYPES
} SplitRegisterType;

typedef enum
{
  REG_STYLE_LEDGER,
  REG_STYLE_AUTO_LEDGER,
  REG_STYLE_JOURNAL
} SplitRegisterStyle;

/** Cell Names. T* cells are transaction summary cells  */
#define ACTN_CELL  "action"
#define BALN_CELL  "balance"
#define CRED_CELL  "credit"
#define DATE_CELL  "date"
#define DDUE_CELL  "date-due"
#define DEBT_CELL  "debit"
#define DESC_CELL  "description"
#define FCRED_CELL "credit-formula"
#define FDEBT_CELL "debit-formula"
#define MEMO_CELL  "memo"
#define MXFRM_CELL "transfer"
#define NOTES_CELL "notes"
#define NUM_CELL   "num"
#define PRIC_CELL  "price"
#define RATE_CELL  "exchrate"
#define RECN_CELL  "reconcile"
#define SHRS_CELL  "shares"
#define TBALN_CELL "trans-balance"
#define TCRED_CELL "trans-credit"
#define TDEBT_CELL "trans-debit"
#define TSHRS_CELL "trans-shares"
#define TYPE_CELL  "split-type"
#define XFRM_CELL  "account"
#define VNOTES_CELL "void-notes"

/** Cursor Names  */
#define CURSOR_SINGLE_LEDGER  "cursor-single-ledger"
#define CURSOR_DOUBLE_LEDGER  "cursor-double-ledger"
#define CURSOR_SINGLE_JOURNAL "cursor-single-journal"
#define CURSOR_DOUBLE_JOURNAL "cursor-double-journal"
#define CURSOR_SPLIT          "cursor-split"


/** Types of cursors */
typedef enum
{
  CURSOR_CLASS_NONE = -1,
  CURSOR_CLASS_SPLIT,
  CURSOR_CLASS_TRANS,
  NUM_CURSOR_CLASSES
} CursorClass;

typedef struct split_register_colors
{
  guint32 header_bg_color;

  guint32 primary_bg_color;
  guint32 secondary_bg_color;

  guint32 primary_active_bg_color;
  guint32 secondary_active_bg_color;

  guint32 split_bg_color;
  guint32 split_active_bg_color;
} SplitRegisterColors;


typedef struct split_register SplitRegister;
typedef struct sr_info SRInfo;

/** \brief The type, style and table for the register. */
struct split_register
{
  Table * table;   /**< The table itself that implements the underlying GUI. */

  SplitRegisterType type;
  SplitRegisterStyle style;

  gboolean use_double_line;
  gboolean is_template;
  gboolean do_auto_complete;

  SRInfo * sr_info;   /**< \internal private data; outsiders should not access this */
};

/** Callback function type */
typedef gncUIWidget (*SRGetParentCallback) (gpointer user_data);


/* Prototypes ******************************************************/

/** Create and return a new split register. */
SplitRegister * gnc_split_register_new (SplitRegisterType type,
                                        SplitRegisterStyle style,
                                        gboolean use_double_line,
                                        gboolean is_template);

/** Configure the split register. */
void gnc_split_register_config (SplitRegister *reg,
                                SplitRegisterType type,
                                SplitRegisterStyle style,
                                gboolean use_double_line);

/** Change the auto-completion behavior. **/
void gnc_split_register_set_auto_complete(SplitRegister *reg,
                                          gboolean do_auto_complete);

/** Destroy the split register. */
void gnc_split_register_destroy (SplitRegister *reg);

/** Make a register window read-only. */
void gnc_split_register_set_read_only (SplitRegister *reg, gboolean read_only);

/** Set the template account used by template registers */
void gnc_split_register_set_template_account (SplitRegister *reg,
                                              Account *template_account);

/** Returns the class of the current cursor */
CursorClass gnc_split_register_get_current_cursor_class (SplitRegister *reg);

/** Returns the class of the cursor at the given virtual cell location. */
CursorClass gnc_split_register_get_cursor_class
                                              (SplitRegister *reg,
                                               VirtualCellLocation vcell_loc);

/** Sets the user data and callback hooks for the register. */
void gnc_split_register_set_data (SplitRegister *reg, gpointer user_data,
                                  SRGetParentCallback get_parent);

/** Returns the transaction which is the parent of the current split. */
Transaction * gnc_split_register_get_current_trans (SplitRegister *reg);

/** Returns the split at which the cursor is currently located. */
Split * gnc_split_register_get_current_split (SplitRegister *reg);

/** Returns the blank split or NULL if there is none. */
Split * gnc_split_register_get_blank_split (SplitRegister *reg);

/**  Searches the split register for the given split. If found, it
 *    returns TRUE and vcell_loc is set to the location of the
 *    split. Otherwise, returns FALSE. */
gboolean
gnc_split_register_get_split_virt_loc (SplitRegister *reg, Split *split,
                                       VirtualCellLocation *vcell_loc);

/** Searches the split register for the given split. If found, it
 *    returns TRUE and virt_loc is set to the location of either the
 *    debit or credit column in the split, whichever one is
 *    non-blank. Otherwise, returns FALSE. */
gboolean
gnc_split_register_get_split_amount_virt_loc (SplitRegister *reg, Split *split,
                                              VirtualLocation *virt_loc);

/** Given the current virtual location, find the split that anchors
 * this transaction to the current register. Otherwise, returns NULL.
 */
Split *
gnc_split_register_get_current_trans_split (SplitRegister *reg,
                                            VirtualCellLocation *vcell_loc);

/** Duplicates either the current transaction or the current split
 *    depending on the register mode and cursor position. Returns the
 *    split just created, or the 'main' split of the transaction just
 *    created, or NULL if nothing happened. */
Split * gnc_split_register_duplicate_current (SplitRegister *reg);

/** Makes a copy of the current entity, either a split or a
 *    transaction, so that it can be pasted later. */
void gnc_split_register_copy_current (SplitRegister *reg);

/** Equivalent to copying the current entity and the deleting it with
 *    the approriate delete method. */
void gnc_split_register_cut_current (SplitRegister *reg);

/** Pastes a previous copied entity onto the current entity, but only
 *    if the copied and current entity have the same type. */
void gnc_split_register_paste_current (SplitRegister *reg);

/** Deletes the split associated with the current cursor, if both are
 *    non-NULL. Deleting the blank split just clears cursor values. */
void gnc_split_register_delete_current_split (SplitRegister *reg);

/** Deletes the transaction associated with the current cursor, if both
 *    are non-NULL. */
void gnc_split_register_delete_current_trans (SplitRegister *reg);

/** Voids the transaction associated with the current cursor, if
 *    non-NULL. */
void gnc_split_register_void_current_trans (SplitRegister *reg,
					    const char *reason);

/** Unvoids the transaction associated with the current cursor, if
 *    non-NULL. */
void gnc_split_register_unvoid_current_trans (SplitRegister *reg);

/** Deletes the non-transaction splits associated wih the current
 *    cursor, if both are non-NULL. */
void gnc_split_register_empty_current_trans_except_split  (SplitRegister *reg, Split *split);
void gnc_split_register_empty_current_trans  (SplitRegister *reg);

/** Cancels any changes made to the current cursor, reloads the cursor
 *    from the engine, reloads the table from the cursor, and updates
 *    the GUI. The change flags are cleared. */
void gnc_split_register_cancel_cursor_split_changes (SplitRegister *reg);

/** Cancels any changes made to the current pending transaction,
 *    reloads the table from the engine, and updates the GUI. The
 *    change flags are cleared. */
void gnc_split_register_cancel_cursor_trans_changes (SplitRegister *reg);

/** Copy transaction information from a list of splits to the rows of
 *    the register GUI.  The third argument, default_source_acc, will
 *    be used to initialize the source account of a new, blank split
 *    appended to the tail end of the register.  This "blank split" is
 *    the place where the user can edit info to create new
 *    transactions. */
void gnc_split_register_load (SplitRegister *reg, GList * split_list,
                              Account *default_source_acc);


/** Copy the contents of the current cursor to a split. The split and
 *    transaction that are updated are the ones associated with the
 *    current cursor (register entry) position. If the do_commit flag
 *    is set, the transaction will also be committed. If it is the
 *    blank transaction, and the do_commit flag is set, a refresh will
 *    result in a new blank transaction.  The method returns TRUE if
 *    something was changed. */
gboolean gnc_split_register_save (SplitRegister *reg, gboolean do_commit);

/** Causes a redraw of the register window associated with reg. */
void gnc_split_register_redraw (SplitRegister *reg);

/** Returns TRUE if the register has changed cells. */
gboolean gnc_split_register_changed (SplitRegister *reg);

/** If TRUE, visually indicate the demarcation between splits with post
 * dates prior to the present, and after. This will only make sense if
 * the splits are ordered primarily by post date. */
void gnc_split_register_show_present_divider (SplitRegister *reg,
                                              gboolean show_present);

/** Expand the current transaction if it is collapsed. */
void gnc_split_register_expand_current_trans (SplitRegister *reg,
                                              gboolean expand);

/** Return TRUE if current trans is expanded and style is REG_STYLE_LEDGER. */
gboolean gnc_split_register_current_trans_expanded (SplitRegister *reg);

/** Return the debit string used in the register. */
const char * gnc_split_register_get_debit_string (SplitRegister *reg);

/** Return the credit string used in the register. */
const char * gnc_split_register_get_credit_string (SplitRegister *reg);


/** Pop up the exchange-rate dialog, maybe, for the current split.
 * If force_dialog is TRUE, the forces the dialog to to be called.
 * If the dialog does not complete successfully, then return TRUE.
 * Return FALSE in all other cases (meaning "move on")
 */
gboolean
gnc_split_register_handle_exchange (SplitRegister *reg, gboolean force_dialog);

/** @} */
/** @} */
/* -------------------------------------------------------------- */

/** Private function -- outsiders must not use this */
gboolean gnc_split_register_full_refresh_ok (SplitRegister *reg);

/** Private function -- outsiders must not use this */
void     gnc_split_register_load_xfer_cells (SplitRegister *reg,
                                             Account *base_account);

/** Private function -- outsiders must not use this */
void gnc_copy_trans_onto_trans (Transaction *from, Transaction *to,
                                gboolean use_cut_semantics,
                                gboolean do_commit);

#endif
