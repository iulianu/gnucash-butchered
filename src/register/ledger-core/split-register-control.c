/********************************************************************\
 * split-register-control.c -- split register control object        *
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
 *                                                                  *
\********************************************************************/

#include "config.h"

#include "AccWindow.h"
#include "Group.h"
#include "Scrub.h"
#include "combocell.h"
#include "global-options.h"
#include "gnc-component-manager.h"
#include "gnc-engine-util.h"
#include "gnc-ui.h"
#include "gnc-ui-util.h"
#include "messages.h"
#include "pricecell.h"
#include "datecell.h"
#include "split-register-control.h"
#include "split-register-model-save.h"
#include "split-register-p.h"
#include "table-allgui.h"
#include "dialog-transfer.h"


/* This static indicates the debugging module that this .o belongs to. */
static short module = MOD_LEDGER;


static gboolean
gnc_split_register_balance_trans (SplitRegister *reg, Transaction *trans)
{
  gnc_numeric imbalance;

  imbalance = xaccTransGetImbalance (trans);
  if (!gnc_numeric_zero_p (imbalance))
  {
    int choice;
    int default_value;
    Account *default_account;
    Account *other_account;
    GList *radio_list = NULL;
    const char *title   = _("Rebalance Transaction");
    const char *message = _("The current transaction is not balanced.");
    Split *split;
    Split *other_split;
    gboolean two_accounts;

    split = xaccTransGetSplit (trans, 0);
    other_split = xaccSplitGetOtherSplit (split);
  
    if (other_split == NULL)
    {
      two_accounts = FALSE;
      other_account = NULL;
    }
    else
    {
      two_accounts = TRUE;
      other_account = xaccSplitGetAccount (other_split);
    }

    default_account = gnc_split_register_get_default_account (reg);
    
    /* If the two pointers are the same, the account from other_split
     * is actually the default account. We must make other_account
     * the account from split instead.   */
     
    if (default_account == other_account)
      other_account = xaccSplitGetAccount (split);

    /*  If the two pointers are still the same, we have two splits, but
     *  they both refer to the same account. While non-sensical, we don't
     *  object.   */

    if (default_account == other_account)
      two_accounts = FALSE;

    radio_list = g_list_append (radio_list,
                                _("Balance it manually"));
    radio_list = g_list_append (radio_list,
                                _("Let GnuCash add an adjusting split"));

    if (reg->type < NUM_SINGLE_REGISTER_TYPES)
    {
      radio_list = g_list_append (radio_list,
                                  _("Adjust current account split total"));

      if (two_accounts)
        radio_list = g_list_append (radio_list,
                                    _("Adjust other account split total"));

      default_value = 2;
    }
    else
      default_value = 0;

    choice = gnc_choose_radio_option_dialog_parented
      (gnc_split_register_get_parent (reg),
       title,
       message,
       default_value,
       radio_list);

    g_list_free (radio_list);

    switch (choice)
    {
      default:
      case 0:
        break;

      case 1:
        xaccTransScrubImbalance (trans, gnc_get_current_group (), NULL,
                                 gnc_get_current_book ());
        break;

      case 2:
        xaccTransScrubImbalance (trans, gnc_get_current_group (),
                                 default_account, gnc_get_current_book ());
        break;

      case 3:
        xaccTransScrubImbalance (trans, gnc_get_current_group (),
                                 other_account, gnc_get_current_book ());
        break;
    }

    return TRUE;
  }

  return FALSE;
}

static gboolean
gnc_trans_has_reconciled_splits (Transaction *trans)
{
  GList *node;

  for (node = xaccTransGetSplitList (trans); node; node = node->next)
  {
    Split *split = node->data;

    switch (xaccSplitGetReconcile (split))
    {
      case YREC:
      case FREC:
        return TRUE;

      default:
        break;
    }
  }

  return FALSE;
}

static gboolean
gnc_split_register_old_split_empty_p (SplitRegister *reg, Split *split)
{
  BasicCell *cell;
  gnc_numeric amount;
  const char *string;

  string = gnc_table_layout_get_cell_value (reg->table->layout, MEMO_CELL);
  if ((string != NULL) && (*string != '\0'))
    return FALSE;

  string = gnc_table_layout_get_cell_value (reg->table->layout, XFRM_CELL);
  if ((string != NULL) && (*string != '\0'))
    return FALSE;

  cell = gnc_table_layout_get_cell (reg->table->layout, CRED_CELL);
  if (cell) {
    amount = gnc_price_cell_get_value ((PriceCell *) cell);
    if (!gnc_numeric_zero_p (amount))
      return FALSE;
  }

  cell = gnc_table_layout_get_cell (reg->table->layout, DEBT_CELL);
  if (cell) {
    amount = gnc_price_cell_get_value ((PriceCell *) cell);
    if (!gnc_numeric_zero_p (amount))
      return FALSE;
  }

  return TRUE;
}

static void
gnc_split_register_move_cursor (VirtualLocation *p_new_virt_loc,
                                gpointer user_data)
{
  VirtualLocation new_virt_loc = *p_new_virt_loc;
  VirtualCellLocation old_trans_split_loc;
  SplitRegister *reg = user_data;
  Transaction *pending_trans;
  Transaction *new_trans;
  Transaction *old_trans;
  Split *old_trans_split;
  Split *new_trans_split;
  Split *new_split;
  Split *old_split;
  CursorClass new_class;
  CursorClass old_class;
  gboolean exact_traversal;
  gboolean do_refresh;
  gboolean saved;
  SRInfo *info;

  if (!reg)
    return;

  info = gnc_split_register_get_info (reg);
  pending_trans = xaccTransLookup (&info->pending_trans_guid,
                                   gnc_get_current_book ());

  PINFO ("start callback %d %d \n",
         new_virt_loc.vcell_loc.virt_row,
         new_virt_loc.vcell_loc.virt_col);

  /* The transaction we are coming from */
  old_split = gnc_split_register_get_current_split (reg);
  old_trans = gnc_split_register_get_current_trans (reg);
  old_trans_split =
    gnc_split_register_get_current_trans_split (reg, &old_trans_split_loc);
  old_class = gnc_split_register_get_current_cursor_class (reg);

  exact_traversal = info->exact_traversal;

  if (info->traverse_to_new)
  {
    if (old_class == CURSOR_CLASS_SPLIT)
      new_trans = old_trans;
    else
      new_trans = NULL;

    new_split = NULL;
    new_trans_split = NULL;
    new_class = CURSOR_CLASS_NONE;
  }
  else if (!info->hint_set_by_traverse)
  {
    /* The transaction where we are moving to */
    new_trans = gnc_split_register_get_trans (reg, new_virt_loc.vcell_loc);

    /* The split we are moving to */
    new_split = gnc_split_register_get_split (reg, new_virt_loc.vcell_loc);

    /* The split at the transaction line we are moving to */
    new_trans_split = gnc_split_register_get_trans_split
      (reg, new_virt_loc.vcell_loc, NULL);

    new_class = gnc_split_register_get_cursor_class (reg,
                                                     new_virt_loc.vcell_loc);
  }
  else
  {
    new_trans = info->cursor_hint_trans;
    new_split = info->cursor_hint_split;
    new_trans_split = info->cursor_hint_trans_split;
    new_class = info->cursor_hint_cursor_class;
  }

  info->hint_set_by_traverse = FALSE;
  info->reg_loaded = FALSE;

  gnc_suspend_gui_refresh ();

  /* commit the contents of the cursor into the database */
  saved = gnc_split_register_save (reg, old_trans != new_trans);
  if ((old_class == CURSOR_CLASS_SPLIT) &&
      old_split &&
      (old_split != new_split) &&
      gnc_split_register_old_split_empty_p(reg, old_split)) {
    int current_row;

    xaccSplitDestroy(old_split);
    old_split = NULL;

    /*
     * If the user is moving down a row, we've just thrown off the
     * numbers by deleting a split. Correct for that.
     */
    current_row = reg->table->current_cursor_loc.vcell_loc.virt_row;
    if (new_virt_loc.vcell_loc.virt_row > current_row)
      new_virt_loc.vcell_loc.virt_row--;
  }
  else if ((pending_trans != NULL)      &&
	   (pending_trans == old_trans) &&
	   (old_trans != new_trans))
  {
    if (gnc_split_register_balance_trans (reg, old_trans))
    {
      /* Trans was unbalanced. */
      new_trans = old_trans;
      new_split = old_split;
      new_trans_split = old_trans_split;
      new_class = old_class;
      new_virt_loc = reg->table->current_cursor_loc;
    }
    else
    {
      /* Trans was balanced. Let it go. */
      if (xaccTransIsOpen (old_trans))
	xaccTransCommitEdit (old_trans);

      info->pending_trans_guid = *xaccGUIDNULL ();
      pending_trans = NULL;
      saved = TRUE;
    }
  }
  else if (old_trans &&
           (old_trans != new_trans) &&
           !gnc_trans_has_reconciled_splits (old_trans) &&
           !info->first_pass &&
           gnc_split_register_balance_trans (reg, old_trans))
  {
    /* no matter what, stay there so the user can see what happened */
    new_trans = old_trans;
    new_split = old_split;
    new_trans_split = old_trans_split;
    new_class = old_class;
    new_virt_loc = reg->table->current_cursor_loc;
  }

  if (saved)
  {
    info->cursor_hint_trans = new_trans;
    info->cursor_hint_split = new_split;
    info->cursor_hint_trans_split = new_trans_split;
    info->cursor_hint_cursor_class = new_class;
  }

  if (old_split != new_split)
    info->change_confirmed = FALSE;

  gnc_resume_gui_refresh ();

  /* redrawing the register can muck everything up */
  if (saved)
  {
    VirtualCellLocation vcell_loc;

    if (!info->reg_loaded)
      gnc_split_register_redraw (reg);

    /* if the split we were going to is still in the register,
     * then it may have moved. Find out where it is now. */
    if (gnc_split_register_find_split (reg, new_trans, new_trans_split,
                                       new_split, new_class, &vcell_loc))
    {
      VirtualCell *vcell;

      vcell = gnc_table_get_virtual_cell (reg->table, vcell_loc);

      new_virt_loc.vcell_loc = vcell_loc;
    }
    else
      new_virt_loc.vcell_loc = reg->table->current_cursor_loc.vcell_loc;

    new_trans = gnc_split_register_get_trans (reg, new_virt_loc.vcell_loc);
    new_split = gnc_split_register_get_split (reg, new_virt_loc.vcell_loc);
    new_trans_split = gnc_split_register_get_trans_split
      (reg, new_virt_loc.vcell_loc, NULL);
    new_class = gnc_split_register_get_cursor_class (reg,
                                                     new_virt_loc.vcell_loc);
  }
  else if (info->traverse_to_new)
  {
    new_trans = info->cursor_hint_trans;
    new_split = info->cursor_hint_split;
    new_trans_split = info->cursor_hint_trans_split;
    new_class = info->cursor_hint_cursor_class;
  }

  gnc_table_find_close_valid_cell (reg->table, &new_virt_loc, exact_traversal);

  *p_new_virt_loc = new_virt_loc;

  PINFO ("after move %d %d \n",
         new_virt_loc.vcell_loc.virt_row,
         new_virt_loc.vcell_loc.virt_col);

  /* if the register was reloaded, then everything should be fine :)
   * otherwise, we may need to change some visibility settings. */
  if (saved)
  {
    gnc_split_register_set_cell_fractions (reg, new_split);
    return;
  }

  /* in the mult-line and dynamic modes, we need to hide the old
   * and show the new. */
  if (((REG_STYLE_AUTO_LEDGER == reg->style) ||
       (REG_STYLE_JOURNAL     == reg->style) ||
       info->trans_expanded) &&
      (old_trans_split != new_trans_split))
  {
    VirtualCellLocation vc_loc;

    vc_loc = old_trans_split_loc;
    gnc_table_set_virt_cell_cursor
      (reg->table, vc_loc, gnc_split_register_get_passive_cursor (reg));
    gnc_split_register_set_trans_visible (reg, vc_loc, FALSE,
                                          reg->style == REG_STYLE_JOURNAL);

    if ((REG_STYLE_AUTO_LEDGER == reg->style) ||
        (REG_STYLE_JOURNAL     == reg->style))
    {
      gnc_split_register_get_trans_split (reg, new_virt_loc.vcell_loc,
                                          &vc_loc);
      gnc_table_set_virt_cell_cursor
        (reg->table, vc_loc, gnc_split_register_get_active_cursor (reg));
      gnc_split_register_set_trans_visible (reg, vc_loc, TRUE,
                                            reg->style == REG_STYLE_JOURNAL);
    }

    info->trans_expanded = FALSE;

    do_refresh = TRUE;
  }
  else
    do_refresh = FALSE;

  info->cursor_hint_trans = new_trans;
  info->cursor_hint_split = new_split;
  info->cursor_hint_trans_split = new_trans_split;
  info->cursor_hint_cursor_class = new_class;

  gnc_split_register_set_cell_fractions (reg, new_split);

  gnc_table_find_close_valid_cell (reg->table, p_new_virt_loc,
                                   exact_traversal);

  if (do_refresh)
  {
    VirtualCellLocation vc_loc;

    gnc_table_refresh_gui (reg->table, FALSE);
    gnc_table_leave_update (reg->table, reg->table->current_cursor_loc);

    gnc_split_register_get_trans_split (reg, p_new_virt_loc->vcell_loc,
                                        &vc_loc);
    gnc_split_register_show_trans (reg, vc_loc);
  }
}

static Split *
gnc_find_split_in_trans_by_memo (Transaction *trans, const char *memo,
                                 gboolean unit_price)
{
  GList *node;

  for (node = xaccTransGetSplitList (trans); node; node = node->next)
  {
    Split *split = node->data;

    if (unit_price)
    {
      gnc_numeric price = xaccSplitGetSharePrice (split);
      if (!gnc_numeric_equal (price, gnc_numeric_create (1, 1)))
        continue;
    }

    if (safe_strcmp (memo, xaccSplitGetMemo (split)) == 0)
      return split;
  }

  return NULL;
}

static Split *
gnc_find_split_in_account_by_memo (Account *account, const char *memo,
                                   gboolean unit_price)
{
  GList *slp;

  if (account == NULL) return NULL;

  for (slp = g_list_last (xaccAccountGetSplitList (account));
       slp;
       slp = slp->prev)
  {
    Split *split = slp->data;
    Transaction *trans = xaccSplitGetParent (split);

    split = gnc_find_split_in_trans_by_memo (trans, memo, unit_price);

    if (split != NULL) return split;
  }

  return NULL;
}

static Split *
gnc_find_split_in_reg_by_memo (SplitRegister *reg, const char *memo,
                               gboolean unit_price)
{
  int virt_row, virt_col;
  int num_rows, num_cols;
  Transaction *last_trans;

  if (reg == NULL)
    return NULL;

  if (reg->table == NULL)
    return NULL;

  num_rows = reg->table->num_virt_rows;
  num_cols = reg->table->num_virt_cols;

  last_trans = NULL;

  for (virt_row = num_rows - 1; virt_row >= 0; virt_row--)
    for (virt_col = num_cols - 1; virt_col >= 0; virt_col--)
    {
      Split *split;
      Transaction *trans;
      VirtualCellLocation vcell_loc = { virt_row, virt_col };

      split = gnc_split_register_get_split (reg, vcell_loc);
      trans = xaccSplitGetParent (split);

      if (trans == last_trans)
        continue;

      split = gnc_find_split_in_trans_by_memo (trans, memo, unit_price);
      if (split != NULL)
        return split;

      last_trans = trans;
    }

  return NULL;
}

static Transaction *
gnc_find_trans_in_reg_by_desc (SplitRegister *reg, const char *description)
{
  int virt_row, virt_col;
  int num_rows, num_cols;
  Transaction *last_trans;

  if (reg == NULL)
    return NULL;

  if (reg->table == NULL)
    return NULL;

  num_rows = reg->table->num_virt_rows;
  num_cols = reg->table->num_virt_cols;

  last_trans = NULL;

  for (virt_row = num_rows - 1; virt_row >= 0; virt_row--)
    for (virt_col = num_cols - 1; virt_col >= 0; virt_col--)
    {
      Split *split;
      Transaction *trans;
      VirtualCellLocation vcell_loc = { virt_row, virt_col };

      split = gnc_split_register_get_split (reg, vcell_loc);
      trans = xaccSplitGetParent(split);

      if (trans == last_trans)
        continue;

      if (safe_strcmp (description, xaccTransGetDescription (trans)) == 0)
        return trans;

      last_trans = trans;
    }

  return NULL;
}

/* This function determines if auto-completion is appropriate and,
 * if so, performs it. This should only be called by LedgerTraverse. */
static gboolean
gnc_split_register_auto_completion (SplitRegister *reg,
                                    gncTableTraversalDir dir,
                                    VirtualLocation *p_new_virt_loc)
{
  SRInfo *info = gnc_split_register_get_info (reg);
  VirtualLocation new_virt_loc;
  CursorClass cursor_class;
  Transaction *pending_trans;
  Transaction *blank_trans;
  const char *cell_name;
  Transaction *trans;
  Split *blank_split;
  gnc_numeric amount;
  BasicCell *cell;
  Split *split;

  blank_split = xaccSplitLookup (&info->blank_split_guid,
                                 gnc_get_current_book ());
  blank_trans = xaccSplitGetParent (blank_split);

  pending_trans = xaccTransLookup (&info->pending_trans_guid,
                                   gnc_get_current_book ());

  /* auto-completion is only triggered by a tab out */
  if (dir != GNC_TABLE_TRAVERSE_RIGHT)
    return FALSE;

  split = gnc_split_register_get_current_split (reg);
  trans = gnc_split_register_get_current_trans (reg);
  if (trans == NULL)
    return FALSE;

  cursor_class = gnc_split_register_get_current_cursor_class (reg);
  cell_name = gnc_table_get_current_cell_name (reg->table);

  switch (cursor_class)
  {
    case CURSOR_CLASS_TRANS:
      {
        Transaction *auto_trans;
        const char *desc;

        /* there must be a blank transaction * */
        if (blank_trans == NULL)
          return FALSE;

        /* we must be on the blank split */
        if (trans != blank_trans)
          return FALSE;

        /* and leaving the description cell */
        if (!gnc_cell_name_equal (cell_name, DESC_CELL))
          return FALSE;

        /* nothing but the date, num, and description should be changed */
        /* FIXME, this should be refactored. */
        if (gnc_table_layout_get_cell_changed (reg->table->layout,
                                               XFRM_CELL, TRUE)  ||
            gnc_table_layout_get_cell_changed (reg->table->layout,
                                               MXFRM_CELL, TRUE) ||
            gnc_table_layout_get_cell_changed (reg->table->layout,
                                               PRIC_CELL, TRUE)  ||
            gnc_table_layout_get_cell_changed (reg->table->layout,
                                               SHRS_CELL, TRUE)  ||
            gnc_table_layout_get_cell_changed (reg->table->layout,
                                               DEBT_CELL, TRUE)  ||
            gnc_table_layout_get_cell_changed (reg->table->layout,
                                               CRED_CELL, TRUE)  ||
            gnc_table_layout_get_cell_changed (reg->table->layout,
                                               NOTES_CELL, TRUE) ||
            gnc_table_layout_get_cell_changed (reg->table->layout,
                                               RECN_CELL, TRUE))
          return FALSE;

        /* and the description should be changed */
        if (!gnc_table_layout_get_cell_changed (reg->table->layout,
                                                DESC_CELL, TRUE))
          return FALSE;

        /* to a non-empty value */
        desc = gnc_table_layout_get_cell_value (reg->table->layout, DESC_CELL);
        if ((desc == NULL) || (*desc == '\0'))
          return FALSE;

        /* find a transaction to auto-complete on */
        if (gnc_split_register_get_default_account (reg) != NULL)
        {
          Account *account = gnc_split_register_get_default_account (reg);

          auto_trans = xaccAccountFindTransByDesc(account, desc);
        }
        else
          auto_trans = gnc_find_trans_in_reg_by_desc(reg, desc);

        if (auto_trans == NULL)
          return FALSE;

        /* now perform the completion */

        gnc_suspend_gui_refresh ();

        xaccTransBeginEdit (trans);
        gnc_copy_trans_onto_trans (auto_trans, trans, FALSE, FALSE);

        if (gnc_split_register_get_default_account (reg) != NULL)
        {
          Account *default_account;
          GList *node;

          default_account = gnc_split_register_get_default_account (reg);
          blank_split = NULL;

          for (node = xaccTransGetSplitList (trans); node; node = node->next)
          {
            Split *s = node->data;

            if (default_account == xaccSplitGetAccount(s))
            {
              blank_split = s;
              info->blank_split_guid = *xaccSplitGetGUID(blank_split);
              break;
            }
          }

          if (blank_split == NULL)
          {
            blank_split = xaccTransGetSplit(trans, 0);
            info->blank_split_guid = *xaccSplitGetGUID(blank_split);
          }
        }
        else
        {
          blank_split = xaccTransGetSplit(trans, 0);
          info->blank_split_guid = *xaccSplitGetGUID(blank_split);
        }

        if ((pending_trans != NULL) && (pending_trans != trans))
          if (xaccTransIsOpen (pending_trans))
            xaccTransCommitEdit (pending_trans);

        pending_trans = trans;
        info->pending_trans_guid = *xaccTransGetGUID (pending_trans);

        info->blank_split_edited = TRUE;

        {
          SRSaveData *sd;

          sd = gnc_split_register_save_data_new (trans, blank_split,
					 (info->trans_expanded ||
					  reg->style == REG_STYLE_AUTO_LEDGER ||
					  reg->style == REG_STYLE_JOURNAL));
          gnc_table_save_cells (reg->table, sd);
          gnc_split_register_save_data_destroy (sd);
        }

        gnc_resume_gui_refresh ();

        /* now move to the non-empty amount column */
        amount = xaccSplitGetAmount (blank_split);
        cell_name = (gnc_numeric_negative_p (amount)) ? CRED_CELL : DEBT_CELL;

        if (gnc_table_get_current_cell_location (reg->table, cell_name,
                                                 &new_virt_loc))
          *p_new_virt_loc = new_virt_loc;
      }

    break;

    case CURSOR_CLASS_SPLIT:
      {
        char *fullname;
        const char *memo;
        gboolean unit_price;
        Split *auto_split;

        /* we must be on a blank split of a transaction */
        if (split != NULL)
          return FALSE;

        /* and leaving the memo cell */
        if (!gnc_cell_name_equal (cell_name, MEMO_CELL))
          return FALSE;

        /* nothing but the action, memo, and amounts should be changed */
        /* FIXME. This should be refactored. */
        if (gnc_table_layout_get_cell_changed (reg->table->layout,
                                               XFRM_CELL, TRUE)  ||
            gnc_table_layout_get_cell_changed (reg->table->layout,
                                               MXFRM_CELL, TRUE) ||
            gnc_table_layout_get_cell_changed (reg->table->layout,
                                               PRIC_CELL, TRUE)  ||
            gnc_table_layout_get_cell_changed (reg->table->layout,
                                               SHRS_CELL, TRUE)  ||
            gnc_table_layout_get_cell_changed (reg->table->layout,
                                               RECN_CELL, TRUE))
          return FALSE;

        /* and the memo should be changed */
        if (!gnc_table_layout_get_cell_changed (reg->table->layout,
                                                MEMO_CELL, TRUE))
          return FALSE;

        /* to a non-empty value */
        memo = gnc_table_layout_get_cell_value (reg->table->layout, MEMO_CELL);
        if ((memo == NULL) || (*memo == '\0'))
          return FALSE;

        /* if there is no price field, only auto-complete from splits with
         * a unit share price. */
        unit_price = !gnc_table_get_current_cell_location (reg->table,
                                                           PRIC_CELL, NULL);

        /* find a split to auto-complete on */
        if (gnc_split_register_get_default_account (reg) != NULL)
        {
          Account *account = gnc_split_register_get_default_account (reg);

          auto_split = gnc_find_split_in_account_by_memo (account, memo,
                                                          unit_price);
        }
        else
          auto_split = gnc_find_split_in_reg_by_memo (reg, memo, unit_price);

        if (auto_split == NULL)
          return FALSE;

        /* the auto-complete code below is taken from xaccSRGetEntryHandler */

        /* auto-complete the action field if it wasn't changed */
        if (!gnc_table_layout_get_cell_changed (reg->table->layout,
                                                ACTN_CELL, TRUE))
        {
          cell = gnc_table_layout_get_cell (reg->table->layout, ACTN_CELL);
          gnc_combo_cell_set_value ((ComboCell *) cell,
                                    xaccSplitGetAction (auto_split));
        }

        /* auto-complete the account name */
        cell = gnc_table_layout_get_cell (reg->table->layout, XFRM_CELL);

        fullname = xaccAccountGetFullName (xaccSplitGetAccount (auto_split),
                                           gnc_get_account_separator ());
        gnc_combo_cell_set_value ((ComboCell *) cell, fullname);
        g_free(fullname);

        gnc_basic_cell_set_changed (cell, TRUE);

        if (!gnc_table_layout_get_cell_changed (reg->table->layout,
                                                DEBT_CELL, TRUE) &&
            !gnc_table_layout_get_cell_changed (reg->table->layout,
                                                CRED_CELL, TRUE))
        {
          BasicCell *debit_cell;
          BasicCell *credit_cell;

          amount = xaccSplitGetValue (auto_split);

          debit_cell = gnc_table_layout_get_cell (reg->table->layout,
                                                  DEBT_CELL);
          credit_cell = gnc_table_layout_get_cell (reg->table->layout,
                                                   CRED_CELL);

          gnc_price_cell_set_debt_credit_value ((PriceCell *) debit_cell,
                                                (PriceCell *) credit_cell,
                                                amount);

          gnc_basic_cell_set_changed (debit_cell, TRUE);
          gnc_basic_cell_set_changed (credit_cell, TRUE);
        }

        /* and refresh the gui */
        gnc_table_refresh_gui (reg->table, TRUE);

        /* now move to the non-empty amount column */
        amount = xaccSplitGetAmount (auto_split);
        cell_name = (gnc_numeric_negative_p (amount)) ? CRED_CELL : DEBT_CELL;

        if (gnc_table_get_current_cell_location (reg->table, cell_name,
                                                 &new_virt_loc))
          *p_new_virt_loc = new_virt_loc;
      }

    break;

    default:
      break;
  }

  return TRUE;
}

static void
gnc_split_register_traverse_check_stock_action (SplitRegister *reg, const char *cell_name)
{
  BasicCell *cell;
  gnc_numeric shares;
  gboolean buy, sell;
  const char *name;

  if (!gnc_cell_name_equal (cell_name, ACTN_CELL) ||
      !gnc_table_layout_get_cell_changed (reg->table->layout,
					  ACTN_CELL, FALSE))
    return;

  cell = gnc_table_layout_get_cell (reg->table->layout, ACTN_CELL);
  if (!cell)
    return;
  name = ((ComboCell *)cell)->cell.value;
  if ((name == NULL) || (*name == '\0'))
    return;

  buy  = safe_strcmp (name, ACTION_BUY_STR)  == 0;
  sell = safe_strcmp (name, ACTION_SELL_STR) == 0;
  if (!buy && !sell)
    return;

  cell = gnc_table_layout_get_cell (reg->table->layout, SHRS_CELL);
  if (!cell)
    return;
  shares = gnc_price_cell_get_value ((PriceCell *) cell);

  if ((buy  && !gnc_numeric_positive_p (shares)) ||
      (sell &&  gnc_numeric_positive_p (shares))) {
    gnc_price_cell_set_value ((PriceCell *)cell, gnc_numeric_neg (shares));
    gnc_basic_cell_set_changed (cell, TRUE);
  }
}

static void
gnc_split_register_traverse_check_stock_shares (SplitRegister *reg, const char *cell_name)
{
  BasicCell *cell;
  gnc_numeric shares;
  gboolean buy;
  const char *name;

  if (!gnc_cell_name_equal (cell_name, SHRS_CELL) ||
      !gnc_table_layout_get_cell_changed (reg->table->layout,
					  SHRS_CELL, FALSE))
    return;

  cell = gnc_table_layout_get_cell (reg->table->layout, SHRS_CELL);
  if (!cell)
    return;
  shares = gnc_price_cell_get_value ((PriceCell *) cell);
  if (gnc_numeric_zero_p (shares))
    return;
  buy  = gnc_numeric_positive_p (shares);

  cell = gnc_table_layout_get_cell (reg->table->layout, ACTN_CELL);
  if (!cell)
    return;
  name = ((ComboCell *)cell)->cell.value;

  if (buy && (safe_strcmp (name, ACTION_BUY_STR) != 0)) {
    gnc_combo_cell_set_value((ComboCell *)cell, ACTION_BUY_STR);
    gnc_basic_cell_set_changed (cell, TRUE);
  } else if (safe_strcmp (name, ACTION_SELL_STR) != 0) {
    gnc_combo_cell_set_value((ComboCell *)cell, ACTION_SELL_STR);
    gnc_basic_cell_set_changed (cell, TRUE);
  }
}

static Account *
gnc_split_register_get_account_always (SplitRegister *reg, const char * cell_name,
				       gboolean force)
{
  BasicCell *cell;
  const char *name;
  gboolean dummy;

  cell = gnc_table_layout_get_cell (reg->table->layout, cell_name);
  if (!cell)
    return NULL;
  name = gnc_basic_cell_get_value (cell);

  /* If 'name' is "-- Split Transaction --" then return NULL or the register acct */
  if (!safe_strcmp (name, _("-- Split Transaction --"))) {
    if (force)
      return gnc_split_register_get_default_account (reg);
    else
      return NULL;
  }

  return gnc_split_register_get_account_by_name (reg, cell, name, &dummy);
}

static const char *
gnc_split_register_get_cell_string (SplitRegister *reg, const char *cell_name)
{
  BasicCell *cell;

  cell = gnc_table_layout_get_cell (reg->table->layout, cell_name);
  if (!cell)
    return "";

  return gnc_basic_cell_get_value (cell);
}

static Timespec
gnc_split_register_get_cell_date (SplitRegister *reg, const char *cell_name)
{
  DateCell *cell;
  Timespec ts;

  cell = (DateCell*) gnc_table_layout_get_cell (reg->table->layout, cell_name);

  if (cell)
    gnc_date_cell_get_date (cell, &ts);
  else
    timespecFromTime_t (&ts, time (NULL));

  return ts;
}

/* This function checks to see if we need to determine an exchange rate.
 * If we need to determine an exchange rate, then pop up the dialog.
 * If the dialog does not complete successfully, then return TRUE.
 * Return FALSE in all other cases (meaning "move on")
 */
gboolean
gnc_split_register_handle_exchange (SplitRegister *reg, gboolean force_dialog)
{
  Transaction *txn;
  Account *xfer_acc;
  gnc_commodity *txn_cur, *xfer_com;
  gnc_numeric amount, exch_rate;
  XferDialog *xfer;
  gboolean used_mxfrm = FALSE;
  gboolean swap_amounts = FALSE;
  PriceCell *rate_cell;
  
  /* Make sure we NEED this for this type of register */
  rate_cell = (PriceCell*) gnc_table_layout_get_cell (reg->table->layout, RATE_CELL);
  if (!rate_cell)
    return FALSE;

  /* See if we already have an exchange rate... */
  exch_rate = gnc_price_cell_get_value (rate_cell);
  if (! gnc_numeric_zero_p (exch_rate) && !force_dialog)
    return FALSE;

  /* Grab the xfer account */
  xfer_acc = gnc_split_register_get_account_always (reg, XFRM_CELL, FALSE);
  if (!xfer_acc) {
    used_mxfrm = TRUE;
    xfer_acc = gnc_split_register_get_account_always (reg, MXFRM_CELL, TRUE);
  }

  if (!xfer_acc)
    return FALSE;

  /* Grab the txn currency */
  txn = gnc_split_register_get_current_trans (reg);

  /* Check if the commodities are the same */
  txn_cur = xaccTransGetCurrency (txn);
  xfer_com = xaccAccountGetCommodity (xfer_acc);

  if (gnc_commodity_equal (txn_cur, xfer_com)) {
    /* If we're not forcing the dialog, then there is no reason to
     * go on.  We're using the correct accounts.
     */
    if (!force_dialog)
      return FALSE;

    /* If we're forcing, then compare the current account
     * commodity to the transaction commodity.
     * XXX: we only need to do this for two-split, basic, non-expanded registers
     */

    xfer_acc = gnc_split_register_get_default_account (reg);
    xfer_com = xaccAccountGetCommodity (xfer_acc);
    if (gnc_commodity_equal (txn_cur, xfer_com))
      return FALSE;

    swap_amounts = TRUE;
  }

  /* Ok, we need to grab the exchange rate */
  amount = gnc_split_register_debcred_cell_value (reg);

  /* create the exchange-rate dialog */
  xfer = gnc_xfer_dialog (NULL, NULL); /* XXX */
  gnc_xfer_dialog_is_exchange_dialog (xfer, &exch_rate);

  /* enter the accounts */
  gnc_xfer_dialog_select_to_account (xfer, xfer_acc);
  gnc_xfer_dialog_select_from_currency (xfer, txn_cur);
  gnc_xfer_dialog_hide_to_account_tree (xfer);
  gnc_xfer_dialog_hide_from_account_tree (xfer);

  /* fill in the dialog entries */
  gnc_xfer_dialog_set_amount (xfer, amount);
  gnc_xfer_dialog_set_description (xfer,
				   gnc_split_register_get_cell_string (reg, DESC_CELL));
  gnc_xfer_dialog_set_memo (xfer, 
			    gnc_split_register_get_cell_string (reg, MEMO_CELL));
  gnc_xfer_dialog_set_num (xfer, 
			   gnc_split_register_get_cell_string (reg, NUM_CELL));
  gnc_xfer_dialog_set_date (xfer,
			    timespecToTime_t (
			    gnc_split_register_get_cell_date (reg, DATE_CELL)));

  /* and run it... */
  if (gnc_xfer_dialog_run_until_done (xfer) == FALSE)
    return TRUE;

  /* Set the RATE_CELL on this cursor and mark it changed */
  gnc_price_cell_set_value (rate_cell, exch_rate);
  gnc_basic_cell_set_changed (&rate_cell->cell, TRUE);
  return FALSE;
}

static gboolean
gnc_split_register_traverse (VirtualLocation *p_new_virt_loc,
                             gncTableTraversalDir dir,
                             gpointer user_data)
{
  SplitRegister *reg = user_data;
  Transaction *pending_trans;
  VirtualLocation virt_loc;
  Transaction *trans, *new_trans;
  GNCVerifyResult result;
  gboolean changed;
  SRInfo *info;
  Split *split;
  const char *cell_name;

  if (!reg)
    return FALSE;

  info = gnc_split_register_get_info (reg);

  if (info->first_pass)
    return FALSE;

  pending_trans = xaccTransLookup (&info->pending_trans_guid,
                                   gnc_get_current_book ());
  virt_loc = *p_new_virt_loc;

  info->exact_traversal = (dir == GNC_TABLE_TRAVERSE_POINTER);

  split = gnc_split_register_get_current_split (reg);
  trans = gnc_split_register_get_current_trans (reg);
  if (trans == NULL)
    return FALSE;

  /* no changes, make sure we aren't going off the end */
  changed = gnc_table_current_cursor_changed (reg->table, FALSE);
  if (!changed && (pending_trans != trans))
  {
    gnc_table_find_close_valid_cell (reg->table, &virt_loc,
                                     info->exact_traversal);

    *p_new_virt_loc = virt_loc;

    return FALSE;
  }

  /* Get the current cell-name to check it.. */
  cell_name = gnc_table_get_current_cell_name (reg->table);

  /* See if we are leaving an account field */
  do
  {
    ComboCell *cell;
    char *name;


    if (!gnc_cell_name_equal (cell_name, XFRM_CELL) &&
        !gnc_cell_name_equal (cell_name, MXFRM_CELL))
      break;

    cell = NULL;

    if (gnc_cell_name_equal (cell_name, XFRM_CELL))
    {
      if (gnc_table_layout_get_cell_changed (reg->table->layout,
                                             XFRM_CELL, FALSE))
        cell = (ComboCell *) gnc_table_layout_get_cell (reg->table->layout,
                                                        XFRM_CELL);
    }

    if (gnc_cell_name_equal (cell_name, MXFRM_CELL))
    {
      if (gnc_table_layout_get_cell_changed (reg->table->layout,
                                             MXFRM_CELL, FALSE))
        cell = (ComboCell *) gnc_table_layout_get_cell (reg->table->layout,
                                                        MXFRM_CELL);
    }

    if (!cell)
      break;

    /* The account changed -- reset the rate
     * XXX: perhaps we should only do this is the currency changed?
     */
    {
      PriceCell *rate_cell = (PriceCell*)
	gnc_table_layout_get_cell (reg->table->layout, RATE_CELL);

      if (rate_cell)
	gnc_price_cell_set_value (rate_cell, gnc_numeric_zero());
    }

    name = cell->cell.value;
    if (!name || *name == '\0' ||
        safe_strcmp (name, SPLIT_TRANS_STR) == 0 ||
        safe_strcmp (name, STOCK_SPLIT_STR) == 0)
      break;

    /* Create the account if necessary. Also checks for a placeholder */
    (void) gnc_split_register_get_account_by_name (reg, (BasicCell *)cell, cell->cell.value,
						   &info->full_refresh);
  } while (FALSE);

  /* See if we are leaving an action field */
  if ((reg->type == STOCK_REGISTER) ||
      (reg->type == PORTFOLIO_LEDGER) ||
      (reg->type == CURRENCY_REGISTER)) {
    gnc_split_register_traverse_check_stock_action (reg, cell_name);
    gnc_split_register_traverse_check_stock_shares (reg, cell_name);
  }
 
  /* See if we are tabbing off the end of the very last line */
  do
  {
    VirtualLocation virt_loc;

    if (!changed && !info->blank_split_edited)
      break;

    if (dir != GNC_TABLE_TRAVERSE_RIGHT)
      break;

    virt_loc = reg->table->current_cursor_loc;
    if (gnc_table_move_vertical_position (reg->table, &virt_loc, 1))
      break;

    virt_loc = reg->table->current_cursor_loc;
    if (gnc_table_move_tab (reg->table, &virt_loc, TRUE))
      break;

    /* Deal with the exchange-rate */
    if (gnc_split_register_handle_exchange (reg, FALSE))
      return TRUE;

    *p_new_virt_loc = reg->table->current_cursor_loc;
    (p_new_virt_loc->vcell_loc.virt_row)++;
    p_new_virt_loc->phys_row_offset = 0;
    p_new_virt_loc->phys_col_offset = 0;

    info->traverse_to_new = TRUE;

    return FALSE;

  } while (FALSE);

  /* Now see if we are changing cursors. If not, we may be able to
   * auto-complete. */
  if (!gnc_table_virtual_cell_out_of_bounds (reg->table, virt_loc.vcell_loc))
  {
    if (gnc_split_register_auto_completion (reg, dir, p_new_virt_loc))
      return FALSE;
  }

  /* See if we are tabbing off the end of a blank split */
  do
  {
    VirtualLocation virt_loc;
    int old_virt_row;

    if (!changed)
      break;

    if (split)
      break;

    if (dir != GNC_TABLE_TRAVERSE_RIGHT)
      break;

    virt_loc = reg->table->current_cursor_loc;
    old_virt_row = virt_loc.vcell_loc.virt_row;

    if (gnc_table_move_tab (reg->table, &virt_loc, TRUE) &&
        old_virt_row == virt_loc.vcell_loc.virt_row)
      break;

    /* If we are here, then: (a) the current cursor has been
     * edited, and (b) we are on the blank split of a multi-line
     * transaction, and (c) we are tabbing out of the last cell
     * on the line. Thus, we want to go ahead and add the new
     * split and end up on the new blank split of the current
     * transaction. */

    /* Deal with the exchange-rate */
    if (gnc_split_register_handle_exchange (reg, FALSE))
      return TRUE;

    info->cursor_hint_trans = trans;
    info->cursor_hint_split = split;
    info->cursor_hint_trans_split =
      gnc_split_register_get_current_trans_split (reg, NULL);
    info->cursor_hint_cursor_class = CURSOR_CLASS_SPLIT;
    info->hint_set_by_traverse = TRUE;

    return FALSE;

  } while (FALSE);


  {
    int old_virt_row;

    old_virt_row = reg->table->current_cursor_loc.vcell_loc.virt_row;
    
    /* Check for going off the end */
    gnc_table_find_close_valid_cell (reg->table, &virt_loc,
				     info->exact_traversal);


    /* Did we change vertical position? */
    if (virt_loc.vcell_loc.virt_row != old_virt_row)
      /* Deal with the exchange-rate */
      if (gnc_split_register_handle_exchange (reg, FALSE))
	return TRUE;
  }


  /* Same transaction, no problem */
  new_trans = gnc_split_register_get_trans (reg, virt_loc.vcell_loc);
  if (trans == new_trans)
  {
    *p_new_virt_loc = virt_loc;

    return FALSE;
  }

  /* Ok, we are changing transactions and the current transaction has
   * changed. See what the user wants to do. */
  {
    const char *message;

    message = _("The current transaction has been changed.\n"
                "Would you like to record it?");

    result = gnc_verify_cancel_dialog_parented
      (gnc_split_register_get_parent (reg),
       GNC_VERIFY_YES, message);
  }

  switch (result)
  {
    case GNC_VERIFY_YES:
      break;

    case GNC_VERIFY_NO:
      {
        VirtualCellLocation vcell_loc;
        Split *new_split;
        Split *trans_split;
        CursorClass new_class;

        new_split = gnc_split_register_get_split (reg, virt_loc.vcell_loc);
        trans_split = gnc_split_register_get_trans_split (reg,
                                                          virt_loc.vcell_loc,
                                                          NULL);
        new_class = gnc_split_register_get_cursor_class (reg,
                                                         virt_loc.vcell_loc);

        gnc_split_register_cancel_cursor_trans_changes (reg);

        if (gnc_split_register_find_split (reg, new_trans, trans_split,
                                           new_split, new_class, &vcell_loc))
          virt_loc.vcell_loc = vcell_loc;

        gnc_table_find_close_valid_cell (reg->table, &virt_loc,
                                         info->exact_traversal);

        *p_new_virt_loc = virt_loc;
      }

      break;

    case GNC_VERIFY_CANCEL:
      return TRUE;

    default:
      break;
  }

  return FALSE;
}

TableControl *
gnc_split_register_control_new (void)
{
  TableControl *control;

  control = gnc_table_control_new ();

  control->move_cursor = gnc_split_register_move_cursor;
  control->traverse = gnc_split_register_traverse;

  return control;
}

gboolean
gnc_split_register_recn_cell_confirm (char old_flag, gpointer data)
{
  SplitRegister *reg = data;

  if (old_flag == YREC)
  {
    const char *message = _("Do you really want to mark this transaction "
                            "not reconciled?\nDoing so might make future "
                            "reconciliation difficult!");
    gboolean confirm;

    confirm = gnc_lookup_boolean_option ("Register",
                                         "Confirm before changing reconciled",
                                         TRUE);
    if (!confirm)
      return TRUE;

    return gnc_verify_dialog_parented (gnc_split_register_get_parent (reg),
                                       TRUE, message);
  }

  return TRUE;
}
