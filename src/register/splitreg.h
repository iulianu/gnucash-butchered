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
 * splitreg.h
 *
 * FUNCTION:
 * Implements a basic display register/ledger.
 * This object makes specific cells have specific properties
 * (price, text, date, etc.) and specific names that correspond.
 * It also determines the actual physical layout, arrangement
 * of columns, etc.
 *
 * See src/doc/design/gnucash-design.info for more information.
 *
 * DESIGN HOPES:
 * Should probably move at least some of the layout to a config 
 * file.  Might make good sense to use scheme/guile for the layout.
 *
 * HISTORY:
 * Copyright (c) 1998, 1999, 2000 Linas Vepstas
 */

#ifndef __XACC_SPLITREG_H__
#define __XACC_SPLITREG_H__

#include "basiccell.h"
#include "cellblock.h"
#include "combocell.h"
#include "datecell.h"
#include "quickfillcell.h"
#include "pricecell.h"
#include "numcell.h"
#include "recncell.h"
#include "table-allgui.h"

/* defined register types.
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

  GENERAL_LEDGER,
  INCOME_LEDGER,
  PORTFOLIO_LEDGER,
  SEARCH_LEDGER
} SplitRegisterType;

/* These values are used to identify the cells in the register. */
/* Keep these in sync with the cell_names array in splitreg.c.  */
typedef enum
{
  NO_CELL    = -1,
  DATE_CELL  =  0,
  NUM_CELL,
  DESC_CELL,
  RECN_CELL,
  SHRBALN_CELL,
  BALN_CELL,
  ACTN_CELL,
  XFRM_CELL,
  XTO_CELL,
  MEMO_CELL,
  CRED_CELL,
  DEBT_CELL,
  PRIC_CELL,
  SHRS_CELL,
  MXFRM_CELL, /* MXFRM is the "mirrored" transfer-from account */
  TCRED_CELL,
  TDEBT_CELL,
  CELL_TYPE_COUNT
} CellType;

/*
 * enumerated display styles 
 * REG_SINGLE_LINE    -- show one line per transaction
 * REG_DOUBLE_LINE    -- show two lines per transaction
 * REG_MULTI_LINE     -- show multiple lines per transaction
 * REG_SINGLE_DYNAMIC -- dynamically expand edited transaction,
 *                       all other transactions on one line
 * REG_DOUBLE_DYNAMIC -- dynamically expand edited transaction,
 *                       all other transactions on two lines
 */
typedef enum
{
  REG_STYLE_LEDGER,
  REG_STYLE_AUTO_LEDGER,
  REG_STYLE_JOURNAL
} SplitRegisterStyle;

/* modified flags -- indicate which cell values have been modified by user */
#define MOD_NONE   0x0000
#define MOD_DATE   0x0001
#define MOD_NUM    0x0002
#define MOD_DESC   0x0004
#define MOD_RECN   0x0008
#define MOD_ACTN   0x0010
#define MOD_XFRM   0x0020
#define MOD_MXFRM  0x0040
#define MOD_XTO    0x0080
#define MOD_MEMO   0x0100
#define MOD_AMNT   0x0200
#define MOD_PRIC   0x0400
#define MOD_SHRS   0x0800
#define MOD_ALL    0xffff

/* Types of cursors */
typedef enum
{
  CURSOR_CLASS_NONE = -1,
  CURSOR_CLASS_SPLIT,
  CURSOR_CLASS_TRANS,
  NUM_CURSOR_CLASSES
} CursorClass;

typedef enum
{
  CURSOR_TYPE_NONE = -1,
  CURSOR_TYPE_HEADER,
  CURSOR_TYPE_SINGLE_LEDGER,
  CURSOR_TYPE_DOUBLE_LEDGER,
  CURSOR_TYPE_SINGLE_JOURNAL,
  CURSOR_TYPE_DOUBLE_JOURNAL,
  CURSOR_TYPE_SPLIT,
  NUM_CURSOR_TYPES
} CursorType;

typedef struct _SplitRegisterBuffer SplitRegisterBuffer;
typedef struct _SplitRegister SplitRegister;

typedef void (*SplitRegisterDestroyCB) (SplitRegister *reg);

struct _SplitRegister
{
  /* the table itself that implements the underlying GUI. */
  Table         * table;

  /* the cursors that define the register structure */
  CellBlock     * cursor_header;
  CellBlock     * cursor_ledger_single;
  CellBlock     * cursor_ledger_double;
  CellBlock     * cursor_journal_single;
  CellBlock     * cursor_journal_double;
  CellBlock     * cursor_split;

  BasicCell     * nullCell;

  DateCell      * dateCell;
  NumCell       * numCell;
  QuickFillCell * descCell;
  RecnCell      * recnCell;   /* main transaction line reconcile */
  PriceCell     * shrbalnCell;
  PriceCell     * balanceCell;
  ComboCell     * actionCell;
  ComboCell     * xfrmCell;
  ComboCell     * xtoCell;
  QuickFillCell * memoCell;
  PriceCell     * creditCell;
  PriceCell     * debitCell;
  PriceCell     * priceCell;
  PriceCell     * sharesCell;
  ComboCell     * mxfrmCell;
  PriceCell     * tcreditCell;
  PriceCell     * tdebitCell;

  SplitRegisterType type;
  SplitRegisterStyle style;
  gboolean use_double_line;

  /* some private data; outsiders should not access this */

  BasicCell *header_cells[CELL_TYPE_COUNT];
  BasicCell *cells[CELL_TYPE_COUNT];

  /* user_data allows users of this object to hang
   * private data onto it */
  void *user_data;

  /* The destroy callback gives user's a chance 
   * to free up any associated user_hook data */
  SplitRegisterDestroyCB destroy;
};


typedef char* (*SRStringGetter) (SplitRegisterType);

void            xaccSplitRegisterSetDebitStringGetter(SRStringGetter getter);
void            xaccSplitRegisterSetCreditStringGetter(SRStringGetter getter);

SplitRegister *
xaccMallocSplitRegister (SplitRegisterType type,
                         SplitRegisterStyle style,
                         gboolean use_double_line,
                         TableGetEntryHandler entry_handler,
                         TableGetFGColorHandler fg_color_handler,
                         TableGetBGColorHandler bg_color_handler,
                         VirtCellDataAllocator allocator,
                         VirtCellDataDeallocator deallocator,
                         VirtCellDataCopy copy);

void            xaccConfigSplitRegister (SplitRegister *reg,
                                         SplitRegisterType type,
                                         SplitRegisterStyle style,
                                         gboolean use_double_line);

void            xaccDestroySplitRegister (SplitRegister *reg);

/* returns non-zero value if updates have been made to data */
guint32         xaccSplitRegisterGetChangeFlag (SplitRegister *reg);

/* Clears all change flags in the register. Does not alter values */
void            xaccSplitRegisterClearChangeFlag (SplitRegister *reg);

/* Returns the type of the current cursor */
CursorClass     xaccSplitRegisterGetCurrentCursorClass (SplitRegister *reg);

/* Returns the type of the cursor at the given virtual row and column. */
CursorClass    xaccSplitRegisterGetCursorClass (SplitRegister *reg,
                                                VirtualCellLocation vcell_loc);

/* Returns the type of the current cell */
CellType        xaccSplitRegisterGetCurrentCellType (SplitRegister *reg);

/* Returns the type of the cell at the given virtual row and column. */
CellType        xaccSplitRegisterGetCellType (SplitRegister *reg,
                                              VirtualLocation virt_loc);

/* Returns the virtual location in the current cursor of the
 * given cell using the pointer values. The function returns true if
 * the given cell type is in the current cursor, false otherwise. */
gboolean   xaccSplitRegisterGetCellLoc (SplitRegister *reg,
                                        CellType cell_type,
                                        VirtualCellLocation vcell_loc,
                                        VirtualLocation *virt_loc);

gboolean   xaccSplitRegisterGetCurrentCellLoc (SplitRegister *reg,
                                               CellType cell_type,
                                               VirtualLocation *virt_loc);

/* Functions for working with split register buffers */
SplitRegisterBuffer * xaccMallocSplitRegisterBuffer (void);
void xaccDestroySplitRegisterBuffer (SplitRegisterBuffer *srb);

void xaccSplitRegisterSaveCursor(SplitRegister *sr, SplitRegisterBuffer *srb);
void xaccSplitRegisterRestoreCursorChanged(SplitRegister *sr,
                                           SplitRegisterBuffer *srb);

const char * xaccSplitRegisterGetCellTypeName (CellType type);
CellType     xaccSplitRegisterGetCellTypeFromName (const char *name);


#endif /* __XACC_SPLITREG_H__ */

/* ============ END OF FILE ===================== */
