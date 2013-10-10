/********************************************************************\
 * Account.cpp -- Account data structure implementation             *
 * Copyright (C) 1997 Robin D. Clark                                *
 * Copyright (C) 1997-2003 Linas Vepstas <linas@linas.org>          *
 * Copyright (C) 2007 David Hampton <hampton@employees.org>         *
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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>
#include <typeinfo>
#include <algorithm>

#include "AccountP.h"
#include "Split.h"
#include "Transaction.h"
#include "TransactionP.h"
#include "gnc-event.h"
#include "gnc-glib-utils.h"
#include "gnc-lot.h"
#include "gnc-pricedb.h"

static QofLogModule log_module = GNC_MOD_ACCOUNT;

/* The Canonical Account Separator.  Pre-Initialized. */
static char account_separator[8] = ".";
static wchar_t account_uc_separator = ':';

#define GET_PRIVATE(o)  \
   (o->priv)

/********************************************************************\
 * Because I can't use C++ for this project, doesn't mean that I    *
 * can't pretend to!  These functions perform actions on the        *
 * account data structure, in order to encapsulate the knowledge    *
 * of the internals of the Account in one file.                     *
\********************************************************************/

static void xaccAccountBringUpToDate (Account *acc);


/********************************************************************\
 * gnc_get_account_separator                                        *
 *   returns the current account separator character                *
 *                                                                  *
 * Args: none                                                       *
 * Returns: account separator character                             *
 \*******************************************************************/
const char *
gnc_get_account_separator_string (void)
{
    return account_separator;
}

wchar_t
gnc_get_account_separator (void)
{
    return account_uc_separator;
}

void
gnc_set_account_separator (const char *separator)
{
    wchar_t uc;
    int count;

    uc = g_utf8_get_char_validated(separator, -1);
    if ((uc == (wchar_t) - 2) || (uc == (wchar_t) - 1) || g_unichar_isalnum(uc))
    {
        account_uc_separator = ':';
        strcpy(account_separator, ":");
        return;
    }

    account_uc_separator = uc;
    count = g_unichar_to_utf8(uc, account_separator);
    account_separator[count] = '\0';
}

char *gnc_account_name_violations_errmsg (const char *separator, const std::list<std::string> & invalid_account_names)
{
    gchar *message = NULL;
    gchar *account_list = NULL;

    for ( std::list<std::string>::const_iterator node = invalid_account_names.begin(); node != invalid_account_names.end(); node++ )
    {
        if ( !account_list )
            account_list = node->c_str();
        else
        {
            gchar *tmp_list = NULL;

            tmp_list = g_strconcat (account_list, "\n", node->c_str(), NULL );
            g_free ( account_list );
            account_list = tmp_list;
        }
    }

    /* Translators: The first %s will be the account separator character,
       the second %s is a list of account names.
       The resulting string will be displayed to the user if there are
       account names containing the separator character. */
    message = g_strdup_printf(
                  _("The separator character \"%s\" is used in one or more account names.\n\n"
                    "This will result in unexpected behaviour. "
                    "Either change the account names or choose another separator character.\n\n"
                    "Below you will find the list of invalid account names:\n"
                    "%s"), separator, account_list );
    g_free ( account_list );
    return message;
}

std::list<std::string>
gnc_account_list_name_violations (QofBook *book, const char *separator)
{
    Account *root_account = gnc_book_get_root_account(book);
    
    std::list<std::string> invalid_list;

//    g_return_val_if_fail (separator != NULL, NULL);
    if(!separator) return invalid_list; // empty list

    if (root_account == NULL)
        return invalid_list; //empty list

    AccountList_t accounts = gnc_account_get_descendants (root_account);
    for (AccountList_t::const_iterator node = accounts.begin(); node != accounts.end(); node++)
    {
        Account *acct      = *node;
        gchar   *acct_name = g_strdup ( xaccAccountGetName ( acct ) );

        if ( g_strstr_len ( acct_name, -1, separator ) )
            invalid_list.push_front(acct_name);
        else
            g_free ( acct_name );
    }
    return invalid_list;
}

/********************************************************************\
\********************************************************************/

G_INLINE_FUNC void mark_account (Account *acc);
void
mark_account (Account *acc)
{
    qof_instance_set_dirty(acc);
}

/********************************************************************\
\********************************************************************/

/* Temporary */
GType gnc_account_get_type(void)
{
    return 0x43A014F2;
}

Account::Account()
{
    priv = new AccountPrivate;
    priv->parent   = NULL;

    priv->accountName = CACHE_INSERT("");
    priv->accountCode = CACHE_INSERT("");
    priv->description = CACHE_INSERT("");

    priv->type = ACCT_TYPE_NONE;

    priv->mark = 0;

    priv->policy = xaccGetFIFOPolicy();
    priv->lots = NULL;

    priv->commodity = NULL;
    priv->commodity_scu = 0;
    priv->non_standard_scu = FALSE;

    priv->balance = gnc_numeric_zero();
    priv->cleared_balance = gnc_numeric_zero();
    priv->reconciled_balance = gnc_numeric_zero();
    priv->starting_balance = gnc_numeric_zero();
    priv->starting_cleared_balance = gnc_numeric_zero();
    priv->starting_reconciled_balance = gnc_numeric_zero();
    priv->balance_dirty = FALSE;

    priv->splits = NULL;
    priv->sort_dirty = FALSE;
}

Account::~Account()
{
    // TODO
    delete priv;
}

static void
xaccInitAccount (Account * acc, QofBook *book)
{
    ENTER ("book=%p\n", book);
    qof_instance_init_data (acc, GNC_ID_ACCOUNT, book);

    LEAVE ("account=%p\n", acc);
}

/********************************************************************\
\********************************************************************/

QofBook *
gnc_account_get_book(const Account *account)
{
    return qof_instance_get_book(QOF_INSTANCE(account));
}

/********************************************************************\
\********************************************************************/

static Account *
gnc_coll_get_root_account (QofCollection *col)
{
    if (!col) return NULL;
    return qof_collection_get_data (col);
}

static void
gnc_coll_set_root_account (QofCollection *col, Account *root)
{
    AccountPrivate *rpriv;
    Account *old_root;
    if (!col) return;

    old_root = gnc_coll_get_root_account (col);
    if (old_root == root) return;

    /* If the new root is already linked into the tree somewhere, then
     * remove it from its current position before adding it at the
     * top. */
    rpriv = GET_PRIVATE(root);
    if (rpriv->parent)
    {
        xaccAccountBeginEdit(root);
        gnc_account_remove_child(rpriv->parent, root);
        xaccAccountCommitEdit(root);
    }

    qof_collection_set_data (col, root);

    if (old_root)
    {
        xaccAccountBeginEdit (old_root);
        xaccAccountDestroy (old_root);
    }
}

Account *
gnc_book_get_root_account (QofBook *book)
{
    QofCollection *col;
    Account *root;

    if (!book) return NULL;
    col = qof_book_get_collection (book, GNC_ID_ROOT_ACCOUNT);
    root = gnc_coll_get_root_account (col);
    if (root == NULL)
        root = gnc_account_create_root(book);
    return root;
}

void
gnc_book_set_root_account (QofBook *book, Account *root)
{
    QofCollection *col;
    if (!book) return;

    if (root && gnc_account_get_book(root) != book)
    {
        PERR ("cannot mix and match books freely!");
        return;
    }

    col = qof_book_get_collection (book, GNC_ID_ROOT_ACCOUNT);
    gnc_coll_set_root_account (col, root);
}

/********************************************************************\
\********************************************************************/

Account *
xaccMallocAccount (QofBook *book)
{
    Account *acc;

    g_return_val_if_fail (book, NULL);

    acc = new Account; //g_object_new (GNC_TYPE_ACCOUNT, NULL);
    xaccInitAccount (acc, book);
    qof_event_gen (acc, QOF_EVENT_CREATE, NULL);

    return acc;
}

Account *
gnc_account_create_root (QofBook *book)
{
    Account *root;
    AccountPrivate *rpriv;

    root = xaccMallocAccount(book);
    rpriv = GET_PRIVATE(root);
    xaccAccountBeginEdit(root);
    rpriv->type = ACCT_TYPE_ROOT;
    CACHE_REPLACE(rpriv->accountName, "Root Account");
    xaccAccountCommitEdit(root);
    gnc_book_set_root_account(book, root);
    return root;
}

Account *
xaccCloneAccount(const Account *from, QofBook *book)
{
    Account *ret;
    AccountPrivate *from_priv, *priv;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(from), NULL);
//    g_return_val_if_fail(QOF_IS_BOOK(book), NULL);
    if(!from) return NULL;
    if(!book) return NULL;
    
    ENTER (" ");
    ret = new Account; //g_object_new (GNC_TYPE_ACCOUNT, NULL);
    g_return_val_if_fail (ret, NULL);

    from_priv = GET_PRIVATE(from);
    priv = GET_PRIVATE(ret);
    xaccInitAccount (ret, book);

    /* Do not Begin/CommitEdit() here; give the caller
     * a chance to fix things up, and let them do it.
     * Also let caller issue the generate_event (EVENT_CREATE) */
    priv->type = from_priv->type;

    priv->accountName = CACHE_INSERT(from_priv->accountName);
    priv->accountCode = CACHE_INSERT(from_priv->accountCode);
    priv->description = CACHE_INSERT(from_priv->description);

    kvp_frame_delete(ret->kvp_data);
    ret->kvp_data = kvp_frame_copy(from->kvp_data);

    /* The new book should contain a commodity that matches
     * the one in the old book. Find it, use it. */
    priv->commodity = gnc_commodity_obtain_twin(from_priv->commodity, book);
    gnc_commodity_increment_usage_count(priv->commodity);

    priv->commodity_scu = from_priv->commodity_scu;
    priv->non_standard_scu = from_priv->non_standard_scu;

    qof_instance_set_dirty(ret);
    LEAVE (" ");
    return ret;
}

/********************************************************************\
\********************************************************************/

static void
xaccFreeOneChildAccount (Account *acc)
{
    /* FIXME: this code is kind of hacky.  actually, all this code
     * seems to assume that the account edit levels are all 1. */
    if (qof_instance_get_editlevel(acc) == 0)
        xaccAccountBeginEdit(acc);
    xaccAccountDestroy(acc);
}

static void
xaccFreeAccountChildren (Account *acc)
{
    AccountPrivate *priv;

    /* Copy the list since it will be modified */
    priv = GET_PRIVATE(acc);
    AccountList_t children = priv->children;
    std::for_each(children.begin(), children.end(), xaccFreeOneChildAccount);
}

/* The xaccFreeAccount() routine releases memory associated with the
 * account.  It should never be called directly from user code;
 * instead, the xaccAccountDestroy() routine should be used (because
 * xaccAccountDestroy() has the correct commit semantics). */
static void
xaccFreeAccount (Account *acc)
{
    AccountPrivate *priv;
    GList *lp;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return NULL;

    priv = GET_PRIVATE(acc);
    qof_event_gen (acc, QOF_EVENT_DESTROY, NULL);

    if (! priv->children.empty())
    {
        PERR (" instead of calling xaccFreeAccount(), please call \n"
              " xaccAccountBeginEdit(); xaccAccountDestroy(); \n");

        /* First, recursively free children */
        xaccFreeAccountChildren(acc);
    }

    /* remove lots -- although these should be gone by now. */
    if (priv->lots)
    {
        PERR (" instead of calling xaccFreeAccount(), please call \n"
              " xaccAccountBeginEdit(); xaccAccountDestroy(); \n");

        for (lp = priv->lots; lp; lp = lp->next)
        {
            GNCLot *lot = lp->data;
            gnc_lot_destroy (lot);
        }
        g_list_free (priv->lots);
        priv->lots = NULL;
    }

    /* Next, clean up the splits */
    /* NB there shouldn't be any splits by now ... they should
     * have been all been freed by CommitEdit().  We can remove this
     * check once we know the warning isn't occurring any more. */
    if (priv->splits)
    {
        GList *slist;
        PERR (" instead of calling xaccFreeAccount(), please call \n"
              " xaccAccountBeginEdit(); xaccAccountDestroy(); \n");

        qof_instance_reset_editlevel(acc);

        slist = g_list_copy(priv->splits);
        for (lp = slist; lp; lp = lp->next)
        {
            Split *s = (Split *) lp->data;
            g_assert(xaccSplitGetAccount(s) == acc);
            xaccSplitDestroy (s);
        }
        g_list_free(slist);
/* Nothing here (or in xaccAccountCommitEdit) NULLs priv->splits, so this asserts every time.
        g_assert(priv->splits == NULL);
*/
    }

    CACHE_REPLACE(priv->accountName, NULL);
    CACHE_REPLACE(priv->accountCode, NULL);
    CACHE_REPLACE(priv->description, NULL);

    /* zero out values, just in case stray
     * pointers are pointing here. */

    priv->parent = NULL;

    priv->balance  = gnc_numeric_zero();
    priv->cleared_balance = gnc_numeric_zero();
    priv->reconciled_balance = gnc_numeric_zero();

    priv->type = ACCT_TYPE_NONE;
    gnc_commodity_decrement_usage_count(priv->commodity);
    priv->commodity = NULL;

    priv->balance_dirty = FALSE;
    priv->sort_dirty = FALSE;

    /* qof_instance_release (&acc->inst); */
//    g_object_unref(acc);
    delete acc;
}

/********************************************************************\
 * transactional routines
\********************************************************************/

void
xaccAccountBeginEdit (Account *acc)
{
    g_return_if_fail(acc);
    qof_begin_edit(acc);
}

static void on_done(QofInstance *inst)
{
    /* old event style */
    qof_event_gen (inst, QOF_EVENT_MODIFY, NULL);
}

static void on_err (QofInstance *inst, QofBackendError errcode)
{
    PERR("commit error: %d", errcode);
    gnc_engine_signal_commit_error( errcode );
}

static void acc_free (QofInstance *inst)
{
    AccountPrivate *priv;
    Account *acc = (Account *) inst;

    priv = GET_PRIVATE(acc);
    if (priv->parent)
        gnc_account_remove_child(priv->parent, acc);
    xaccFreeAccount(acc);
}

static void
destroy_pending_splits_for_account(QofInstance *ent, gpointer acc)
{
    Transaction *trans = (Transaction *) ent;
    Split *split;

    if (xaccTransIsOpen(trans))
        while ((split = xaccTransFindSplitByAccount(trans, acc)))
            xaccSplitDestroy(split);
}

void
xaccAccountCommitEdit (Account *acc)
{
    AccountPrivate *priv;
    QofBook *book;

    g_return_if_fail(acc);
    if (!qof_commit_edit(acc)) return;

    /* If marked for deletion, get rid of subaccounts first,
     * and then the splits ... */
    priv = GET_PRIVATE(acc);
    if (qof_instance_get_destroying(acc))
    {
        GList *lp, *slist;
        QofCollection *col;

        qof_instance_increase_editlevel(acc);

        /* First, recursively free children */
        xaccFreeAccountChildren(acc);

        PINFO ("freeing splits for account %p (%s)",
               acc, priv->accountName ? priv->accountName : "(null)");

        book = qof_instance_get_book(acc);

        /* If book is shutting down, just clear the split list.  The splits
           themselves will be destroyed by the transaction code */
        if (!qof_book_shutting_down(book))
        {
            slist = g_list_copy(priv->splits);
            for (lp = slist; lp; lp = lp->next)
            {
                Split *s = lp->data;
                xaccSplitDestroy (s);
            }
            g_list_free(slist);
        }
        else
        {
            g_list_free(priv->splits);
            priv->splits = NULL;
        }

        /* It turns out there's a case where this assertion does not hold:
           When the user tries to delete an Imbalance account, while also
           deleting all the splits in it.  The splits will just get
           recreated and put right back into the same account!

           g_assert(priv->splits == NULL || qof_book_shutting_down(acc->inst.book));
        */

        if (!qof_book_shutting_down(book))
        {
            col = qof_book_get_collection(book, GNC_ID_TRANS);
            qof_collection_foreach(col, destroy_pending_splits_for_account, acc);

            /* the lots should be empty by now */
            for (lp = priv->lots; lp; lp = lp->next)
            {
                GNCLot *lot = lp->data;
                gnc_lot_destroy (lot);
            }
        }
        g_list_free(priv->lots);
        priv->lots = NULL;

        qof_instance_set_dirty(acc);
        qof_instance_decrease_editlevel(acc);
    }
    else
    {
        xaccAccountBringUpToDate(acc);
    }

    qof_commit_edit_part2(acc, on_err, on_done, acc_free);
}

void
xaccAccountDestroy (Account *acc)
{
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    qof_instance_set_destroying(acc, TRUE);

    xaccAccountCommitEdit (acc);
}

/********************************************************************\
\********************************************************************/

static bool
xaccAcctChildrenEqual(const AccountList_t & children_a,
                      const AccountList_t & children_b,
                      bool check_guids)
{
    AccountList_t::const_iterator na, nb;
    
    for(na = children_a.begin(),
        nb = children_b.begin();
            na != children_a.end() && nb != children_b.end();
            na++, nb++)
    {
        Account *aa = *na;
        Account *ab = *nb;

        if (!xaccAccountEqual(aa, ab, check_guids))
        {
            char sa[GUID_ENCODING_LENGTH + 1];
            char sb[GUID_ENCODING_LENGTH + 1];

            guid_to_string_buff (xaccAccountGetGUID (aa), sa);
            guid_to_string_buff (xaccAccountGetGUID (ab), sb);

            PWARN ("accounts %s and %s differ", sa, sb);

            return(FALSE);
        }
    }

    if (na != children_a.end() || nb != children_b.end())
    {
        PWARN ("different numbers of accounts");
        return(FALSE);
    }

    return(TRUE);
}

bool
xaccAccountEqual(const Account *aa, const Account *ab, bool check_guids)
{
    AccountPrivate *priv_aa, *priv_ab;

    if (!aa && !ab) return TRUE;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(aa), FALSE);
//    g_return_val_if_fail(GNC_IS_ACCOUNT(ab), FALSE);
    if(!aa) return false;
    if(!ab) return false;

    priv_aa = GET_PRIVATE(aa);
    priv_ab = GET_PRIVATE(ab);
    if (priv_aa->type != priv_ab->type)
    {
        PWARN ("types differ: %d vs %d", priv_aa->type, priv_ab->type);
        return FALSE;
    }

    if (g_strcmp0(priv_aa->accountName, priv_ab->accountName) != 0)
    {
        PWARN ("names differ: %s vs %s", priv_aa->accountName, priv_ab->accountName);
        return FALSE;
    }

    if (g_strcmp0(priv_aa->accountCode, priv_ab->accountCode) != 0)
    {
        PWARN ("codes differ: %s vs %s", priv_aa->accountCode, priv_ab->accountCode);
        return FALSE;
    }

    if (g_strcmp0(priv_aa->description, priv_ab->description) != 0)
    {
        PWARN ("descriptions differ: %s vs %s", priv_aa->description, priv_ab->description);
        return FALSE;
    }

    if (!gnc_commodity_equal(priv_aa->commodity, priv_ab->commodity))
    {
        PWARN ("commodities differ");
        return FALSE;
    }

    if (check_guids)
    {
        if (qof_instance_guid_compare(aa, ab) != 0)
        {
            PWARN ("GUIDs differ");
            return FALSE;
        }
    }

    if (kvp_frame_compare(aa->kvp_data, ab->kvp_data) != 0)
    {
        char *frame_a;
        char *frame_b;

        frame_a = kvp_frame_to_string (aa->kvp_data);
        frame_b = kvp_frame_to_string (ab->kvp_data);

        PWARN ("kvp frames differ:\n%s\n\nvs\n\n%s", frame_a, frame_b);

        g_free (frame_a);
        g_free (frame_b);

        return FALSE;
    }

    if (!gnc_numeric_equal(priv_aa->starting_balance, priv_ab->starting_balance))
    {
        char *str_a;
        char *str_b;

        str_a = gnc_numeric_to_string(priv_aa->starting_balance);
        str_b = gnc_numeric_to_string(priv_ab->starting_balance);

        PWARN ("starting balances differ: %s vs %s", str_a, str_b);

        g_free (str_a);
        g_free (str_b);

        return FALSE;
    }

    if (!gnc_numeric_equal(priv_aa->starting_cleared_balance,
                           priv_ab->starting_cleared_balance))
    {
        char *str_a;
        char *str_b;

        str_a = gnc_numeric_to_string(priv_aa->starting_cleared_balance);
        str_b = gnc_numeric_to_string(priv_ab->starting_cleared_balance);

        PWARN ("starting cleared balances differ: %s vs %s", str_a, str_b);

        g_free (str_a);
        g_free (str_b);

        return FALSE;
    }

    if (!gnc_numeric_equal(priv_aa->starting_reconciled_balance,
                           priv_ab->starting_reconciled_balance))
    {
        char *str_a;
        char *str_b;

        str_a = gnc_numeric_to_string(priv_aa->starting_reconciled_balance);
        str_b = gnc_numeric_to_string(priv_ab->starting_reconciled_balance);

        PWARN ("starting reconciled balances differ: %s vs %s", str_a, str_b);

        g_free (str_a);
        g_free (str_b);

        return FALSE;
    }

    if (!gnc_numeric_equal(priv_aa->balance, priv_ab->balance))
    {
        char *str_a;
        char *str_b;

        str_a = gnc_numeric_to_string(priv_aa->balance);
        str_b = gnc_numeric_to_string(priv_ab->balance);

        PWARN ("balances differ: %s vs %s", str_a, str_b);

        g_free (str_a);
        g_free (str_b);

        return FALSE;
    }

    if (!gnc_numeric_equal(priv_aa->cleared_balance, priv_ab->cleared_balance))
    {
        char *str_a;
        char *str_b;

        str_a = gnc_numeric_to_string(priv_aa->cleared_balance);
        str_b = gnc_numeric_to_string(priv_ab->cleared_balance);

        PWARN ("cleared balances differ: %s vs %s", str_a, str_b);

        g_free (str_a);
        g_free (str_b);

        return FALSE;
    }

    if (!gnc_numeric_equal(priv_aa->reconciled_balance, priv_ab->reconciled_balance))
    {
        char *str_a;
        char *str_b;

        str_a = gnc_numeric_to_string(priv_aa->reconciled_balance);
        str_b = gnc_numeric_to_string(priv_ab->reconciled_balance);

        PWARN ("reconciled balances differ: %s vs %s", str_a, str_b);

        g_free (str_a);
        g_free (str_b);

        return FALSE;
    }

    /* no parent; always compare downwards. */

    {
        GList *la = priv_aa->splits;
        GList *lb = priv_ab->splits;

        if ((la && !lb) || (!la && lb))
        {
            PWARN ("only one has splits");
            return FALSE;
        }

        if (la && lb)
        {
            /* presume that the splits are in the same order */
            while (la && lb)
            {
                Split *sa = (Split *) la->data;
                Split *sb = (Split *) lb->data;

                if (!xaccSplitEqual(sa, sb, check_guids, TRUE, FALSE))
                {
                    PWARN ("splits differ");
                    return(FALSE);
                }

                la = la->next;
                lb = lb->next;
            }

            if ((la != NULL) || (lb != NULL))
            {
                PWARN ("number of splits differs");
                return(FALSE);
            }
        }
    }

    if (!xaccAcctChildrenEqual(priv_aa->children, priv_ab->children, check_guids))
    {
        PWARN ("children differ");
        return FALSE;
    }

    return(TRUE);
}

/********************************************************************\
\********************************************************************/
void
gnc_account_set_sort_dirty (Account *acc)
{
    AccountPrivate *priv;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    if (qof_instance_get_destroying(acc))
        return;

    priv = GET_PRIVATE(acc);
    priv->sort_dirty = TRUE;
}

void
gnc_account_set_balance_dirty (Account *acc)
{
    AccountPrivate *priv;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    if (qof_instance_get_destroying(acc))
        return;

    priv = GET_PRIVATE(acc);
    priv->balance_dirty = TRUE;
}

/********************************************************************\
\********************************************************************/

bool
gnc_account_insert_split (Account *acc, Split *s)
{
    AccountPrivate *priv;
    GList *node;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), FALSE);
//    g_return_val_if_fail(GNC_IS_SPLIT(s), FALSE);
    if(!acc) return false;
    if(!s) return false;

    priv = GET_PRIVATE(acc);
    node = g_list_find(priv->splits, s);
    if (node)
        return FALSE;

    if (qof_instance_get_editlevel(acc) == 0)
    {
        priv->splits = g_list_insert_sorted(priv->splits, s,
                                            (GCompareFunc)xaccSplitOrder);
    }
    else
    {
        priv->splits = g_list_prepend(priv->splits, s);
        priv->sort_dirty = TRUE;
    }

    //FIXME: find better event
    qof_event_gen (acc, QOF_EVENT_MODIFY, NULL);
    /* Also send an event based on the account */
    qof_event_gen(acc, GNC_EVENT_ITEM_ADDED, s);

    priv->balance_dirty = TRUE;
//  DRH: Should the below be added? It is present in the delete path.
//  xaccAccountRecomputeBalance(acc);
    return TRUE;
}

bool
gnc_account_remove_split (Account *acc, Split *s)
{
    AccountPrivate *priv;
    GList *node;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), FALSE);
//    g_return_val_if_fail(GNC_IS_SPLIT(s), FALSE);
    if(!acc) return false;
    if(!s) return false;

    priv = GET_PRIVATE(acc);
    node = g_list_find(priv->splits, s);
    if (NULL == node)
        return FALSE;

    priv->splits = g_list_delete_link(priv->splits, node);
    //FIXME: find better event type
    qof_event_gen(acc, QOF_EVENT_MODIFY, NULL);
    // And send the account-based event, too
    qof_event_gen(acc, GNC_EVENT_ITEM_REMOVED, s);

    priv->balance_dirty = TRUE;
    xaccAccountRecomputeBalance(acc);
    return TRUE;
}

void
xaccAccountSortSplits (Account *acc, bool force)
{
    AccountPrivate *priv;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    priv = GET_PRIVATE(acc);
    if (!priv->sort_dirty || (!force && qof_instance_get_editlevel(acc) > 0))
        return;
    priv->splits = g_list_sort(priv->splits, (GCompareFunc)xaccSplitOrder);
    priv->sort_dirty = FALSE;
    priv->balance_dirty = TRUE;
}

static void
xaccAccountBringUpToDate(Account *acc)
{
    if (!acc) return;

    /* if a re-sort happens here, then everything will update, so the
       cost basis and balance calls are no-ops */
    xaccAccountSortSplits(acc, FALSE);
    xaccAccountRecomputeBalance(acc);
}

/********************************************************************\
\********************************************************************/

void
xaccAccountSetGUID (Account *acc, const GncGUID *guid)
{
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
//    g_return_if_fail(guid);
    if(!acc) return;
    if(!guid) return;

    /* XXX this looks fishy and weird to me ... */
    PINFO("acct=%p", acc);
    xaccAccountBeginEdit (acc);
    qof_instance_set_guid (acc, guid);
    qof_instance_set_dirty(acc);
    xaccAccountCommitEdit (acc);
}

/********************************************************************\
\********************************************************************/

Account *
xaccAccountLookup (const GncGUID *guid, QofBook *book)
{
    QofCollection *col;
    if (!guid || !book) return NULL;
    col = qof_book_get_collection (book, GNC_ID_ACCOUNT);
    return (Account *) qof_collection_lookup_entity (col, guid);
}

/********************************************************************\
\********************************************************************/

void
xaccAccountSetMark (Account *acc, short m)
{
    AccountPrivate *priv;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    priv = GET_PRIVATE(acc);
    priv->mark = m;
}

void
xaccClearMark (Account *acc, short val)
{
    Account *root;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    root = gnc_account_get_root(acc);
    xaccClearMarkDown(root ? root : acc, val);
}

void
xaccClearMarkDown (Account *acc, short val)
{
    AccountPrivate *priv;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    priv = GET_PRIVATE(acc);
    priv->mark = val;
    for (AccountList_t::iterator node = priv->children.begin();
            node != priv->children.end(); node++)
    {
        xaccClearMarkDown(*node, val);
    }
}

/********************************************************************\
\********************************************************************/

GNCPolicy *
gnc_account_get_policy (Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
    if(!acc) return NULL;

    return GET_PRIVATE(acc)->policy;
}

void
gnc_account_set_policy (Account *acc, GNCPolicy *policy)
{
    AccountPrivate *priv;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    priv = GET_PRIVATE(acc);
    priv->policy = policy ? policy : xaccGetFIFOPolicy();
}

/********************************************************************\
\********************************************************************/

void
xaccAccountRemoveLot (Account *acc, GNCLot *lot)
{
    AccountPrivate *priv;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
//    g_return_if_fail(GNC_IS_LOT(lot));
    if(!acc) return;
    if(!lot) return;

    priv = GET_PRIVATE(acc);
    g_return_if_fail(priv->lots);

    ENTER ("(acc=%p, lot=%p)", acc, lot);
    priv->lots = g_list_remove(priv->lots, lot);
    qof_event_gen (lot, QOF_EVENT_REMOVE, NULL);
    qof_event_gen (acc, QOF_EVENT_MODIFY, NULL);
    LEAVE ("(acc=%p, lot=%p)", acc, lot);
}

void
xaccAccountInsertLot (Account *acc, GNCLot *lot)
{
    AccountPrivate *priv, *opriv;
    Account * old_acc = NULL;
    Account* lot_account;

    /* errors */
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
//    g_return_if_fail(GNC_IS_LOT(lot));
    if(!acc) return;
    if(!lot) return;

    /* optimizations */
    lot_account = gnc_lot_get_account(lot);
    if (lot_account == acc)
        return;

    ENTER ("(acc=%p, lot=%p)", acc, lot);

    /* pull it out of the old account */
    if (lot_account)
    {
        old_acc = lot_account;
        opriv = GET_PRIVATE(old_acc);
        opriv->lots = g_list_remove(opriv->lots, lot);
    }

    priv = GET_PRIVATE(acc);
    priv->lots = g_list_prepend(priv->lots, lot);
    gnc_lot_set_account(lot, acc);

    /* Don't move the splits to the new account.  The caller will do this
     * if appropriate, and doing it here will not work if we are being
     * called from gnc_book_close_period since xaccAccountInsertSplit
     * will try to balance capital gains and things aren't ready for that. */

    qof_event_gen (lot, QOF_EVENT_ADD, NULL);
    qof_event_gen (acc, QOF_EVENT_MODIFY, NULL);

    LEAVE ("(acc=%p, lot=%p)", acc, lot);
}

/********************************************************************\
\********************************************************************/
static void
xaccPreSplitMove (Split *split, gpointer dummy)
{
    xaccTransBeginEdit (xaccSplitGetParent (split));
}

static void
xaccPostSplitMove (Split *split, Account *accto)
{
    Transaction *trans;

    xaccSplitSetAccount(split, accto);
    xaccSplitSetAmount(split, split->amount);
    trans = xaccSplitGetParent (split);
    xaccTransCommitEdit (trans);
}

void
xaccAccountMoveAllSplits (Account *accfrom, Account *accto)
{
    AccountPrivate *from_priv;

    /* errors */
//    g_return_if_fail(GNC_IS_ACCOUNT(accfrom));
//    g_return_if_fail(GNC_IS_ACCOUNT(accto));
    if(!accfrom) return;
    if(!accto) return;

    /* optimizations */
    from_priv = GET_PRIVATE(accfrom);
    if (!from_priv->splits || accfrom == accto)
        return;

    /* check for book mix-up */
    g_return_if_fail (qof_instance_books_equal(accfrom, accto));
    ENTER ("(accfrom=%p, accto=%p)", accfrom, accto);

    xaccAccountBeginEdit(accfrom);
    xaccAccountBeginEdit(accto);
    /* Begin editing both accounts and all transactions in accfrom. */
    g_list_foreach(from_priv->splits, (GFunc)xaccPreSplitMove, NULL);

    /* Concatenate accfrom's lists of splits and lots to accto's lists. */
    //to_priv->splits = g_list_concat(to_priv->splits, from_priv->splits);
    //to_priv->lots = g_list_concat(to_priv->lots, from_priv->lots);

    /* Set appropriate flags. */
    //from_priv->balance_dirty = TRUE;
    //from_priv->sort_dirty = FALSE;
    //to_priv->balance_dirty = TRUE;
    //to_priv->sort_dirty = TRUE;

    /*
     * Change each split's account back pointer to accto.
     * Convert each split's amount to accto's commodity.
     * Commit to editing each transaction.
     */
    g_list_foreach(from_priv->splits, (GFunc)xaccPostSplitMove, (gpointer)accto);

    /* Finally empty accfrom. */
    g_assert(from_priv->splits == NULL);
    g_assert(from_priv->lots == NULL);
    xaccAccountCommitEdit(accfrom);
    xaccAccountCommitEdit(accto);

    LEAVE ("(accfrom=%p, accto=%p)", accfrom, accto);
}


/********************************************************************\
 * xaccAccountRecomputeBalance                                      *
 *   recomputes the partial balances and the current balance for    *
 *   this account.                                                  *
 *                                                                  *
 * The way the computation is done depends on whether the partial   *
 * balances are for a monetary account (bank, cash, etc.) or a      *
 * certificate account (stock portfolio, mutual fund).  For bank    *
 * accounts, the invariant amount is the dollar amount. For share   *
 * accounts, the invariant amount is the number of shares. For      *
 * share accounts, the share price fluctuates, and the current      *
 * value of such an account is the number of shares times the       *
 * current share price.                                             *
 *                                                                  *
 * Part of the complexity of this computation stems from the fact   *
 * xacc uses a double-entry system, meaning that one transaction    *
 * appears in two accounts: one account is debited, and the other   *
 * is credited.  When the transaction represents a sale of shares,  *
 * or a purchase of shares, some care must be taken to compute      *
 * balances correctly.  For a sale of shares, the stock account must*
 * be debited in shares, but the bank account must be credited      *
 * in dollars.  Thus, two different mechanisms must be used to      *
 * compute balances, depending on account type.                     *
 *                                                                  *
 * Args:   account -- the account for which to recompute balances   *
 * Return: void                                                     *
\********************************************************************/

void
xaccAccountRecomputeBalance (Account * acc)
{
    AccountPrivate *priv;
    gnc_numeric  balance;
    gnc_numeric  cleared_balance;
    gnc_numeric  reconciled_balance;
    GList *lp;

    if (NULL == acc) return;

    priv = GET_PRIVATE(acc);
    if (qof_instance_get_editlevel(acc) > 0) return;
    if (!priv->balance_dirty) return;
    if (qof_instance_get_destroying(acc)) return;
    if (qof_book_shutting_down(qof_instance_get_book(acc))) return;

    balance            = priv->starting_balance;
    cleared_balance    = priv->starting_cleared_balance;
    reconciled_balance = priv->starting_reconciled_balance;

    PINFO ("acct=%s starting baln=%" G_GINT64_FORMAT "/%" G_GINT64_FORMAT,
           priv->accountName, balance.num, balance.denom);
    for (lp = priv->splits; lp; lp = lp->next)
    {
        Split *split = (Split *) lp->data;
        gnc_numeric amt = xaccSplitGetAmount (split);

        balance = gnc_numeric_add_fixed(balance, amt);

        if (NREC != split->reconciled)
        {
            cleared_balance = gnc_numeric_add_fixed(cleared_balance, amt);
        }

        if (YREC == split->reconciled ||
                FREC == split->reconciled)
        {
            reconciled_balance =
                gnc_numeric_add_fixed(reconciled_balance, amt);
        }

        split->balance = balance;
        split->cleared_balance = cleared_balance;
        split->reconciled_balance = reconciled_balance;

    }

    priv->balance = balance;
    priv->cleared_balance = cleared_balance;
    priv->reconciled_balance = reconciled_balance;
    priv->balance_dirty = FALSE;
}

/********************************************************************\
\********************************************************************/

/* The sort order is used to implicitly define an
 * order for report generation */

static int typeorder[NUM_ACCOUNT_TYPES] =
{
    ACCT_TYPE_BANK, ACCT_TYPE_STOCK, ACCT_TYPE_MUTUAL, ACCT_TYPE_CURRENCY,
    ACCT_TYPE_CASH, ACCT_TYPE_ASSET, ACCT_TYPE_RECEIVABLE,
    ACCT_TYPE_CREDIT, ACCT_TYPE_LIABILITY, ACCT_TYPE_PAYABLE,
    ACCT_TYPE_INCOME, ACCT_TYPE_EXPENSE, ACCT_TYPE_EQUITY, ACCT_TYPE_TRADING
};

static int revorder[NUM_ACCOUNT_TYPES] =
{
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};


int
xaccAccountOrder (const Account *aa, const Account *ab)
{
    AccountPrivate *priv_aa, *priv_ab;
    char *da, *db;
    char *endptr = NULL;
    int ta, tb, result;
    long la, lb;

    if ( aa && !ab ) return -1;
    if ( !aa && ab ) return +1;
    if ( !aa && !ab ) return 0;

    priv_aa = GET_PRIVATE(aa);
    priv_ab = GET_PRIVATE(ab);

    /* sort on accountCode strings */
    da = priv_aa->accountCode;
    db = priv_ab->accountCode;

    /* If accountCodes are both base 36 integers do an integer sort */
    la = strtoul (da, &endptr, 36);
    if ((*da != '\0') && (*endptr == '\0'))
    {
        lb = strtoul (db, &endptr, 36);
        if ((*db != '\0') && (*endptr == '\0'))
        {
            if (la < lb) return -1;
            if (la > lb) return +1;
        }
    }

    /* Otherwise do a string sort */
    result = g_strcmp0 (da, db);
    if (result)
        return result;

    /* if account-type-order array not initialized, initialize it */
    /* this will happen at most once during program invocation */
    if (-1 == revorder[0])
    {
        int i;
        for (i = 0; i < NUM_ACCOUNT_TYPES; i++)
        {
            revorder [typeorder[i]] = i;
        }
    }

    /* otherwise, sort on account type */
    ta = priv_aa->type;
    tb = priv_ab->type;
    ta = revorder[ta];
    tb = revorder[tb];
    if (ta < tb) return -1;
    if (ta > tb) return +1;

    /* otherwise, sort on accountName strings */
    da = priv_aa->accountName;
    db = priv_ab->accountName;
    result = safe_utf8_collate(da, db);
    if (result)
        return result;

    /* guarantee a stable sort */
    return qof_instance_guid_compare(aa, ab);
}

static bool xaccAccountOrderStrictWeak(const Account *aa, const Account *ab)
{
    return xaccAccountOrder(aa, ab) < 0;
}

static int
qof_xaccAccountOrder (const Account **aa, const Account **ab)
{
    return xaccAccountOrder(*aa, *ab);
}

/********************************************************************\
\********************************************************************/

void
xaccAccountSetType (Account *acc, GNCAccountType tip)
{
    AccountPrivate *priv;

    /* errors */
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;
    if(tip >= NUM_ACCOUNT_TYPES) return;

    /* optimizations */
    priv = GET_PRIVATE(acc);
    if (priv->type == tip)
        return;

    xaccAccountBeginEdit(acc);
    priv->type = tip;
    priv->balance_dirty = TRUE; /* new type may affect balance computation */
    mark_account(acc);
    xaccAccountCommitEdit(acc);
}

void
xaccAccountSetName (Account *acc, const char *str)
{
    AccountPrivate *priv;

    /* errors */
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
//    g_return_if_fail(str);
    if(!acc) return;
    if(!str) return;

    /* optimizations */
    priv = GET_PRIVATE(acc);
    if (g_strcmp0(str, priv->accountName) == 0)
        return;

    xaccAccountBeginEdit(acc);
    CACHE_REPLACE(priv->accountName, str);
    mark_account (acc);
    xaccAccountCommitEdit(acc);
}

void
xaccAccountSetCode (Account *acc, const char *str)
{
    AccountPrivate *priv;

    /* errors */
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    /* optimizations */
    priv = GET_PRIVATE(acc);
    if (g_strcmp0(str, priv->accountCode) == 0)
        return;

    xaccAccountBeginEdit(acc);
    CACHE_REPLACE(priv->accountCode, str ? str : "");
    mark_account (acc);
    xaccAccountCommitEdit(acc);
}

void
xaccAccountSetDescription (Account *acc, const char *str)
{
    AccountPrivate *priv;

    /* errors */
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    /* optimizations */
    priv = GET_PRIVATE(acc);
    if (g_strcmp0(str, priv->description) == 0)
        return;

    xaccAccountBeginEdit(acc);
    CACHE_REPLACE(priv->description, str ? str : "");
    mark_account (acc);
    xaccAccountCommitEdit(acc);
}

void
xaccAccountSetColor (Account *acc, const char *str)
{
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    xaccAccountBeginEdit(acc);
    if (str)
    {
        gchar *tmp = g_strstrip(g_strdup(str));
        kvp_frame_set_slot_nc(acc->kvp_data, "color",
                              strlen(tmp) ? kvp_value_new_string(tmp) : NULL);
        g_free(tmp);
    }
    else
    {
        kvp_frame_set_slot_nc(acc->kvp_data, "color", NULL);
    }
    mark_account (acc);
    xaccAccountCommitEdit(acc);
}

void
xaccAccountSetFilter (Account *acc, const char *str)
{
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    xaccAccountBeginEdit(acc);
    if (str)
    {
        gchar *tmp = g_strstrip(g_strdup(str));
        kvp_frame_set_slot_nc(acc->kvp_data, "filter",
                              strlen(tmp) ? kvp_value_new_string(tmp) : NULL);
        g_free(tmp);
    }
    else
    {
        kvp_frame_set_slot_nc(acc->kvp_data, "filter", NULL);
    }
    mark_account (acc);
    xaccAccountCommitEdit(acc);
}

void
xaccAccountSetSortOrder (Account *acc, const char *str)
{
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    xaccAccountBeginEdit(acc);
    if (str)
    {
        gchar *tmp = g_strstrip(g_strdup(str));
        kvp_frame_set_slot_nc(acc->kvp_data, "sort-order",
                              strlen(tmp) ? kvp_value_new_string(tmp) : NULL);
        g_free(tmp);
    }
    else
    {
        kvp_frame_set_slot_nc(acc->kvp_data, "sort-order", NULL);
    }
    mark_account (acc);
    xaccAccountCommitEdit(acc);
}

static void
qofAccountSetParent (Account *acc, QofInstance *parent)
{
    Account *parent_acc;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
//    g_return_if_fail(GNC_IS_ACCOUNT(parent));
    if(!acc) return;
    if(!parent) return;
    if(typeid(*parent) != typeid(Account))
        return;
    
    parent_acc = dynamic_cast<Account*>(parent);
    xaccAccountBeginEdit(acc);
    xaccAccountBeginEdit(parent_acc);
    gnc_account_append_child(parent_acc, acc);
    mark_account (parent_acc);
    mark_account (acc);
    xaccAccountCommitEdit(acc);
    xaccAccountCommitEdit(parent_acc);
}

void
xaccAccountSetNotes (Account *acc, const char *str)
{
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    xaccAccountBeginEdit(acc);
    if (str)
    {
        gchar *tmp = g_strstrip(g_strdup(str));
        kvp_frame_set_slot_nc(acc->kvp_data, "notes",
                              strlen(tmp) ? kvp_value_new_string(tmp) : NULL);
        g_free(tmp);
    }
    else
    {
        kvp_frame_set_slot_nc(acc->kvp_data, "notes", NULL);
    }
    mark_account(acc);
    xaccAccountCommitEdit(acc);
}

void
xaccAccountSetCommodity (Account * acc, gnc_commodity * com)
{
    AccountPrivate *priv;
    GList *lp;

    /* errors */
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
//    g_return_if_fail(GNC_IS_COMMODITY(com));
    if(!acc) return;
    if(!com) return;

    /* optimizations */
    priv = GET_PRIVATE(acc);
    if (com == priv->commodity)
        return;

    xaccAccountBeginEdit(acc);
    gnc_commodity_decrement_usage_count(priv->commodity);
    priv->commodity = com;
    gnc_commodity_increment_usage_count(com);
    priv->commodity_scu = gnc_commodity_get_fraction(com);
    priv->non_standard_scu = FALSE;

    /* iterate over splits */
    for (lp = priv->splits; lp; lp = lp->next)
    {
        Split *s = (Split *) lp->data;
        Transaction *trans = xaccSplitGetParent (s);

        xaccTransBeginEdit (trans);
        xaccSplitSetAmount (s, xaccSplitGetAmount(s));
        xaccTransCommitEdit (trans);
    }

    priv->sort_dirty = TRUE;  /* Not needed. */
    priv->balance_dirty = TRUE;
    mark_account (acc);

    xaccAccountCommitEdit(acc);
}

/*
 * Set the account scu and then check to see if it is the same as the
 * commodity scu.  This function is called when parsing the data file
 * and is designed to catch cases where the two were accidentally set
 * to mismatched values in the past.
 */
void
xaccAccountSetCommoditySCU (Account *acc, int scu)
{
    AccountPrivate *priv;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    priv = GET_PRIVATE(acc);
    xaccAccountBeginEdit(acc);
    priv->commodity_scu = scu;
    if (scu != gnc_commodity_get_fraction(priv->commodity))
        priv->non_standard_scu = TRUE;
    mark_account(acc);
    xaccAccountCommitEdit(acc);
}

int
xaccAccountGetCommoditySCUi (const Account * acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), 0);
    if(!acc) return;
    return GET_PRIVATE(acc)->commodity_scu;
}

int
xaccAccountGetCommoditySCU (const Account * acc)
{
    AccountPrivate *priv;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), 0);
    if(!acc) return;

    priv = GET_PRIVATE(acc);
    if (priv->non_standard_scu || !priv->commodity)
        return priv->commodity_scu;
    return gnc_commodity_get_fraction(priv->commodity);
}

void
xaccAccountSetNonStdSCU (Account *acc, bool flag)
{
    AccountPrivate *priv;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    priv = GET_PRIVATE(acc);
    if (priv->non_standard_scu == flag)
        return;
    xaccAccountBeginEdit(acc);
    priv->non_standard_scu = flag;
    mark_account (acc);
    xaccAccountCommitEdit(acc);
}

bool
xaccAccountGetNonStdSCU (const Account * acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), 0);
    if(!acc) return;
    return GET_PRIVATE(acc)->non_standard_scu;
}

/********************************************************************\
\********************************************************************/
/* below follow the old, deprecated currency/security routines. */

void
DxaccAccountSetCurrency (Account * acc, gnc_commodity * currency)
{
    QofBook *book;
    const char *string;
    gnc_commodity *commodity;

    if ((!acc) || (!currency)) return;

    xaccAccountBeginEdit(acc);
    string = gnc_commodity_get_unique_name (currency);
    kvp_frame_set_slot_nc(acc->kvp_data, "old-currency",
                          kvp_value_new_string(string));
    mark_account (acc);
    xaccAccountCommitEdit(acc);

    commodity = DxaccAccountGetCurrency (acc);
    if (!commodity)
    {
        book = qof_instance_get_book(acc);
        gnc_commodity_table_insert (gnc_commodity_table_get_table (book), currency);
    }
}

/********************************************************************\
\********************************************************************/

void
gnc_account_append_child (Account *new_parent, Account *child)
{
    AccountPrivate *ppriv, *cpriv;
    Account *old_parent;
    QofCollection *col;

    /* errors */
//    g_assert(GNC_IS_ACCOUNT(new_parent));
//    g_assert(GNC_IS_ACCOUNT(child));

    /* optimizations */
    ppriv = GET_PRIVATE(new_parent);
    cpriv = GET_PRIVATE(child);
    old_parent = cpriv->parent;
    if (old_parent == new_parent)
        return;

    //  xaccAccountBeginEdit(new_parent);
    xaccAccountBeginEdit(child);
    if (old_parent)
    {
        gnc_account_remove_child(old_parent, child);

        if (!qof_instance_books_equal(old_parent, new_parent))
        {
            /* hack alert -- this implementation is not exactly correct.
             * If the entity tables are not identical, then the 'from' book
             * may have a different backend than the 'to' book.  This means
             * that we should get the 'from' backend to destroy this account,
             * and the 'to' backend to save it.  Right now, this is broken.
             *
             * A 'correct' implementation similar to this is in Period.c
             * except its for transactions ...
             *
             * Note also, we need to reparent the children to the new book as well.
             */
            PWARN ("reparenting accounts across books is not correctly supported\n");

            qof_event_gen (child, QOF_EVENT_DESTROY, NULL);
            col = qof_book_get_collection (qof_instance_get_book(new_parent),
                                           GNC_ID_ACCOUNT);
            qof_collection_insert_entity (col, child);
            qof_event_gen (child, QOF_EVENT_CREATE, NULL);
        }
    }
    cpriv->parent = new_parent;
    ppriv->children.push_back(child);
    qof_instance_set_dirty(new_parent);
    qof_instance_set_dirty(child);

    /* Send events data. Warning: The call to commit_edit is also going
     * to send a MODIFY event. If the gtktreemodelfilter code gets the
     * MODIFY before it gets the ADD, it gets very confused and thinks
     * that two nodes have been added. */
    qof_event_gen (child, QOF_EVENT_ADD, NULL);
    // qof_event_gen (&new_parent->inst, QOF_EVENT_MODIFY, NULL);

    xaccAccountCommitEdit (child);
    //  xaccAccountCommitEdit(new_parent);
}

void
gnc_account_remove_child (Account *parent, Account *child)
{
    AccountPrivate *ppriv, *cpriv;
    GncEventData ed;

    if (!child) return;

    /* Note this routine might be called on accounts which
     * are not yet parented. */
    if (!parent) return;

    ppriv = GET_PRIVATE(parent);
    cpriv = GET_PRIVATE(child);

    if (cpriv->parent != parent)
    {
        PERR ("account not a child of parent");
        return;
    }

    /* Gather event data */
    ed.node = parent;
    ed.idx = list_index_of(ppriv->children, child);

    ppriv->children.remove(child);

    /* Now send the event. */
    qof_event_gen(child, QOF_EVENT_REMOVE, &ed);

    /* clear the account's parent pointer after REMOVE event generation. */
    cpriv->parent = NULL;

    qof_event_gen (parent, QOF_EVENT_MODIFY, NULL);
}

Account *
gnc_account_get_parent (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
    if(!acc) return NULL;
    return GET_PRIVATE(acc)->parent;
}

Account *
gnc_account_get_root (Account *acc)
{
    AccountPrivate *priv;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
    if(!acc) return NULL;

    priv = GET_PRIVATE(acc);
    while (priv->parent)
    {
        acc = priv->parent;
        priv = GET_PRIVATE(acc);
    }

    return acc;
}

bool
gnc_account_is_root (const Account *account)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(account), FALSE);
    if(!account) return false;
    return (GET_PRIVATE(account)->parent == NULL);
}

AccountList_t
gnc_account_get_children (const Account *account)
{
    AccountList_t children;
//    g_return_val_if_fail(GNC_IS_ACCOUNT(account), NULL);
    if(!account) return children; // empty list
    children = GET_PRIVATE(account)->children;
    return children;
}

AccountList_t
gnc_account_get_children_sorted (const Account *account)
{
    AccountList_t children;

    /* errors */
//    g_return_val_if_fail(GNC_IS_ACCOUNT(account), NULL);
    if(!account) return children; // empty list

    AccountPrivate *priv = GET_PRIVATE(account);
    children = priv->children;
    children.sort(xaccAccountOrderStrictWeak);
    return children;
}

int
gnc_account_n_children (const Account *account)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(account), 0);
    if(!account) return 0;
    return GET_PRIVATE(account)->children.size();
}

int
gnc_account_child_index (const Account *parent, const Account *child)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(parent), -1);
//    g_return_val_if_fail(GNC_IS_ACCOUNT(child), -1);
    if(!parent) return -1;
    if(!child) return -1;
    return list_index_of(GET_PRIVATE(parent)->children, const_cast<Account*>(child));
}

Account *
gnc_account_nth_child (const Account *parent, int num)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(parent), NULL);
    if(!parent) return NULL;
    AccountList_t::iterator pos = GET_PRIVATE(parent)->children.begin();
    std::advance(pos, num);
    return *pos;
}

int
gnc_account_n_descendants (const Account *account)
{
    AccountPrivate *priv;
    gint count = 0;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(account), 0);
    if(!account) return 0;

    priv = GET_PRIVATE(account);
    for (AccountList_t::const_iterator node = priv->children.begin(); 
            node != priv->children.end(); node++)
    {
        count += gnc_account_n_descendants(*node) + 1;
    }
    return count;
}

gint
gnc_account_get_current_depth (const Account *account)
{
    AccountPrivate *priv;
    int depth = 0;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(account), 0);
    if(!account) return 0;

    priv = GET_PRIVATE(account);
    while (priv->parent && (priv->type != ACCT_TYPE_ROOT))
    {
        account = priv->parent;
        priv = GET_PRIVATE(account);
        depth++;
    }

    return depth;
}

int
gnc_account_get_tree_depth (const Account *account)
{
    AccountPrivate *priv;
    gint depth = 0, child_depth;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(account), 0);
    if(!account) return 0;

    priv = GET_PRIVATE(account);
    if (priv->children.empty())
        return 1;

    for (AccountList_t::const_iterator node = priv->children.begin(); 
            node != priv->children.end(); node++)
    {
        child_depth = gnc_account_get_tree_depth(*node);
        depth = MAX(depth, child_depth);
    }
    return depth + 1;
}

AccountList_t
gnc_account_get_descendants (const Account *account)
{
    AccountPrivate *priv;
    AccountList_t descendants;
    
//    g_return_val_if_fail(GNC_IS_ACCOUNT(account), NULL);
    if(!account) return descendants; // empty list

    priv = GET_PRIVATE(account);
    for (AccountList_t::const_iterator child = priv->children.begin();
            child != priv->children.end(); child++)
    {
        descendants.push_back(*child);
        AccountList_t subdescs = gnc_account_get_descendants(*child);
        descendants.insert( descendants.end(), subdescs.begin(), subdescs.end() );
    }
    return descendants;
}

AccountList_t
gnc_account_get_descendants_sorted (const Account *account)
{
    AccountPrivate *priv;
    AccountList_t descendants;

    /* errors */
//    g_return_val_if_fail(GNC_IS_ACCOUNT(account), NULL);
    if(!account) return descendants; // empty list

    priv = GET_PRIVATE(account);

    AccountList_t children = priv->children;
    children.sort(xaccAccountOrderStrictWeak);
    for (AccountList_t::const_iterator child = children.begin();
            child != children.end(); child++)
    {
        descendants.push_back(*child);
        AccountList_t subdescs = gnc_account_get_descendants_sorted(*child);
        descendants.insert(descendants.end(), subdescs.begin(), subdescs.end());
    }
    return descendants;
}

Account *
gnc_account_lookup_by_name (const Account *parent, const char * name)
{
    AccountPrivate *cpriv, *ppriv;
    Account *child, *result;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(parent), NULL);
//    g_return_val_if_fail(name, NULL);
    if(!parent) return NULL;
    if(!name) return NULL;

    /* first, look for accounts hanging off the current node */
    ppriv = GET_PRIVATE(parent);
    for (AccountList_t::iterator node = ppriv->children.begin();
            node != ppriv->children.end(); node++)
    {
        child = *node;
        cpriv = GET_PRIVATE(child);
        if (g_strcmp0(cpriv->accountName, name) == 0)
            return child;
    }

    /* if we are still here, then we haven't found the account yet.
     * Recursively search each of the child accounts next */
    for (node = ppriv->children.begin();
            node != ppriv->children.end(); node++)
    {
        child = *node;
        result = gnc_account_lookup_by_name (child, name);
        if (result)
            return result;
    }

    return NULL;
}

Account *
gnc_account_lookup_by_code (const Account *parent, const char * code)
{
    AccountPrivate *cpriv, *ppriv;
    Account *child, *result;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(parent), NULL);
//    g_return_val_if_fail(code, NULL);
    if(!parent) return NULL;
    if(!code) return NULL;

    /* first, look for accounts hanging off the current node */
    ppriv = GET_PRIVATE(parent);
    for (AccountList_t::iterator node = ppriv->children.begin();
            node != ppriv->children.end(); node++)
    {
        child = *node;
        cpriv = GET_PRIVATE(child);
        if (g_strcmp0(cpriv->accountCode, code) == 0)
            return child;
    }

    /* if we are still here, then we haven't found the account yet.
     * Recursively search each of the child accounts next */
    for (node = ppriv->children.begin();
            node != ppriv->children.end(); node++)
    {
        child = *node;
        result = gnc_account_lookup_by_code (child, code);
        if (result)
            return result;
    }

    return NULL;
}

/********************************************************************\
 * Fetch an account, given its full name                            *
\********************************************************************/

static Account *
gnc_account_lookup_by_full_name_helper (const Account *parent,
                                        char **names)
{
    const AccountPrivate *priv, *ppriv;
    Account *found;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(parent), NULL);
//    g_return_val_if_fail(names, NULL);
    if(!parent) return NULL;
    if(!names) return NULL;

    /* Look for the first name in the children. */
    ppriv = GET_PRIVATE(parent);
    for (AccountList_t::const_iterator node = ppriv->children.begin();
            node != ppriv->children.end(); node++)
    {
        Account *account = *node;

        priv = GET_PRIVATE(account);
        if (g_strcmp0(priv->accountName, names[0]) == 0)
        {
            /* We found an account.  If the next entry is NULL, there is
             * nothing left in the name, so just return the account. */
            if (names[1] == NULL)
                return account;

            /* No children?  We're done. */
            if (priv->children.empty())
                return NULL;

            /* There's stuff left to search for.  Search recursively. */
            found = gnc_account_lookup_by_full_name_helper(account, &names[1]);
            if (found != NULL)
            {
                return found;
            }
        }
    }

    return NULL;
}


Account *
gnc_account_lookup_by_full_name (const Account *any_acc,
                                 const char *name)
{
    const AccountPrivate *rpriv;
    const Account *root;
    Account *found;
    char **names;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(any_acc), NULL);
//    g_return_val_if_fail(name, NULL);
    if(!any_acc) return NULL;
    if(!name) return NULL;

    root = any_acc;
    rpriv = GET_PRIVATE(root);
    while (rpriv->parent)
    {
        root = rpriv->parent;
        rpriv = GET_PRIVATE(root);
    }
    names = g_strsplit(name, gnc_get_account_separator_string(), -1);
    found = gnc_account_lookup_by_full_name_helper(root, names);
    g_strfreev(names);
    return found;
}

void
gnc_account_foreach_child (const Account *acc,
                           AccountCb thunk,
                           gpointer user_data)
{
    const AccountPrivate *priv;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
//    g_return_if_fail(thunk);
    if(!acc) return;
    if(!thunk) return;

    priv = GET_PRIVATE(acc);
    for (AccountList_t::const_iterator node = priv->children.begin();
            node != priv->children.end(); node++)
    {
        thunk (*node, user_data);
    }
}

void
gnc_account_foreach_descendant (const Account *acc,
                                AccountCb thunk,
                                gpointer user_data)
{
    const AccountPrivate *priv;
    Account *child;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
//    g_return_if_fail(thunk);
    if(!acc) return;
    if(!thunk) return;

    priv = GET_PRIVATE(acc);
    for (AccountList_t::const_iterator node = priv->children.begin();
            node != priv->children.end(); node++)
    {
        child = *node;
        thunk(child, user_data);
        gnc_account_foreach_descendant(child, thunk, user_data);
    }
}

gpointer
gnc_account_foreach_descendant_until (const Account *acc,
                                      AccountCb2 thunk,
                                      gpointer user_data)
{
    const AccountPrivate *priv;
    Account *child;
    gpointer result;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
//    g_return_val_if_fail(thunk, NULL);
    if(!acc) return;
    if(!thunk) return;

    priv = GET_PRIVATE(acc);
    for (AccountList_t::const_iterator node = priv->children.begin();
            node != priv->children.end(); node++)
    {
        child = *node;
        result = thunk(child, user_data);
        if (result)
            return(result);

        result = gnc_account_foreach_descendant_until(child, thunk, user_data);
        if (result)
            return(result);
    }

    return NULL;
}


GNCAccountType
xaccAccountGetType (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), ACCT_TYPE_NONE);
    if(!acc) return ACCT_TYPE_NONE;
    return GET_PRIVATE(acc)->type;
}

static const char*
qofAccountGetTypeString (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
    if(!acc) return NULL;
    return xaccAccountTypeEnumAsString(GET_PRIVATE(acc)->type);
}

static void
qofAccountSetType (Account *acc, const char *type_string)
{
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
//    g_return_if_fail(type_string);
    if(!acc) return;
    if(!type_string) return;
    xaccAccountSetType(acc, xaccAccountStringToEnum(type_string));
}

const char *
xaccAccountGetName (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
    if(!acc) return NULL;
    return GET_PRIVATE(acc)->accountName;
}

gchar *
gnc_account_get_full_name(const Account *account)
{
    AccountPrivate *priv;
    const Account *a;
    char *fullname;
    gchar **names;
    int level;

    /* So much for hardening the API. Too many callers to this function don't
     * bother to check if they have a non-NULL pointer before calling. */
    if (NULL == account)
        return g_strdup("");

    /* errors */
//    g_return_val_if_fail(GNC_IS_ACCOUNT(account), g_strdup(""));
    if(!account) return g_strdup("");

    /* optimizations */
    priv = GET_PRIVATE(account);
    if (!priv->parent)
        return g_strdup("");

    /* Figure out how much space is needed by counting the nodes up to
     * the root. */
    level = 0;
    for (a = account; a; a = priv->parent)
    {
        priv = GET_PRIVATE(a);
        level++;
    }

    /* Get all the pointers in the right order. The root node "entry"
     * becomes the terminating NULL pointer for the array of strings. */
    names = g_malloc(level * sizeof(gchar *));
    names[--level] = NULL;
    for (a = account; level > 0; a = priv->parent)
    {
        priv = GET_PRIVATE(a);
        names[--level] = priv->accountName;
    }

    /* Build the full name */
    fullname =  g_strjoinv(account_separator, names);
    g_free(names);

    return fullname;
}

const char *
xaccAccountGetCode (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
    if(!acc) return NULL;
    return GET_PRIVATE(acc)->accountCode;
}

const char *
xaccAccountGetDescription (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
    if(!acc) return NULL;
    return GET_PRIVATE(acc)->description;
}

const char *
xaccAccountGetColor (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
    if(!acc) return NULL;
    return acc ? kvp_frame_get_string(acc->kvp_data, "color") : NULL;
}

const char *
xaccAccountGetFilter (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), 0);
    if(!acc) return NULL;
    return acc ? kvp_frame_get_string(acc->kvp_data, "filter") : NULL;
}

const char *
xaccAccountGetSortOrder (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), 0);
    if(!acc) return NULL;
    return acc ? kvp_frame_get_string(acc->kvp_data, "sort-order") : NULL;
}

const char *
xaccAccountGetNotes (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
    if(!acc) return NULL;
    return acc ? kvp_frame_get_string(acc->kvp_data, "notes") : NULL;
}

gnc_commodity *
DxaccAccountGetCurrency (const Account *acc)
{
    KvpValue *v;
    const char *s;
    gnc_commodity_table *table;

    if (!acc) return NULL;

    v = kvp_frame_get_slot(acc->kvp_data, "old-currency");
    if (!v) return NULL;

    s = kvp_value_get_string (v);
    if (!s) return NULL;

    table = gnc_commodity_table_get_table (qof_instance_get_book(acc));

    return gnc_commodity_table_lookup_unique (table, s);
}

gnc_commodity *
xaccAccountGetCommodity (const Account *acc)
{
//    if (!GNC_IS_ACCOUNT(acc))
//        return NULL;
    if(!acc) return NULL;
    return GET_PRIVATE(acc)->commodity;
}

/********************************************************************\
\********************************************************************/
gnc_numeric gnc_account_get_start_balance(const Account * acc)
{
    return GET_PRIVATE(acc)->starting_balance;
}

void
gnc_account_set_start_balance (Account *acc, const gnc_numeric start_baln)
{
    AccountPrivate *priv;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    priv = GET_PRIVATE(acc);
    priv->starting_balance = start_baln;
    priv->balance_dirty = TRUE;
}

void
gnc_account_set_start_cleared_balance (Account *acc,
                                       const gnc_numeric start_baln)
{
    AccountPrivate *priv;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    priv = GET_PRIVATE(acc);
    priv->starting_cleared_balance = start_baln;
    priv->balance_dirty = TRUE;
}

void
gnc_account_set_start_reconciled_balance (Account *acc,
        const gnc_numeric start_baln)
{
    AccountPrivate *priv;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    priv = GET_PRIVATE(acc);
    priv->starting_reconciled_balance = start_baln;
    priv->balance_dirty = TRUE;
}

gnc_numeric
xaccAccountGetBalance (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), gnc_numeric_zero());
    if(!acc) return gnc_numeric_zero();
    return GET_PRIVATE(acc)->balance;
}

gnc_numeric
xaccAccountGetClearedBalance (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), gnc_numeric_zero());
    if(!acc) return gnc_numeric_zero();
    return GET_PRIVATE(acc)->cleared_balance;
}

gnc_numeric
xaccAccountGetReconciledBalance (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), gnc_numeric_zero());
    if(!acc) return gnc_numeric_zero();
    return GET_PRIVATE(acc)->reconciled_balance;
}

gnc_numeric
xaccAccountGetProjectedMinimumBalance (const Account *acc)
{
    AccountPrivate *priv;
    GList *node;
    time64 today;
    gnc_numeric lowest = gnc_numeric_zero ();
    int seen_a_transaction = 0;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), gnc_numeric_zero());
    if(!acc) return gnc_numeric_zero();

    priv = GET_PRIVATE(acc);
    today = gnc_time64_get_today_end();
    for (node = g_list_last(priv->splits); node; node = node->prev)
    {
        Split *split = node->data;

        if (!seen_a_transaction)
        {
            lowest = xaccSplitGetBalance (split);
            seen_a_transaction = 1;
        }
        else if (gnc_numeric_compare(xaccSplitGetBalance (split), lowest) < 0)
        {
            lowest = xaccSplitGetBalance (split);
        }

        if (xaccTransGetDate (xaccSplitGetParent (split)) <= today)
            return lowest;
    }

    return lowest;
}


/********************************************************************\
\********************************************************************/

gnc_numeric
xaccAccountGetBalanceAsOfDate (Account *acc, time64 date)
{
    /* Ideally this could use xaccAccountForEachSplit, but
     * it doesn't exist yet and I'm uncertain of exactly how
     * it would work at this time, since it differs from
     * xaccAccountForEachTransaction by using gpointer return
     * values rather than gints.
     */
    AccountPrivate *priv;
    GList   *lp;
    Timespec ts, trans_ts;
    bool found = FALSE;
    gnc_numeric balance;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), gnc_numeric_zero());
    if(!acc) return gnc_numeric_zero();

    xaccAccountSortSplits (acc, TRUE); /* just in case, normally a noop */
    xaccAccountRecomputeBalance (acc); /* just in case, normally a noop */

    priv = GET_PRIVATE(acc);
    balance = priv->balance;

    /* Since transaction post times are stored as a Timespec,
     * convert date into a Timespec as well rather than converting
     * each transaction's Timespec into a time64.
     *
     * FIXME: CAS: I think this comment is a bogus justification for
     * using xaccTransGetDatePostedTS.  There's no benefit to using
     * Timespec when the input argument is time64, and it's hard to
     * imagine that casting long long to long and comparing two longs is
     * worse than comparing two long longs every time.  IMO,
     * xaccAccountGetPresentBalance gets this right, and its algorithm
     * should be used here.
     */
    ts.tv_sec = date;
    ts.tv_nsec = 0;

    lp = priv->splits;
    while ( lp && !found )
    {
        xaccTransGetDatePostedTS( xaccSplitGetParent( (Split *)lp->data ),
                                  &trans_ts );
        if ( timespec_cmp( &trans_ts, &ts ) >= 0 )
            found = TRUE;
        else
            lp = lp->next;
    }

    if ( lp )
    {
        if ( lp->prev )
        {
            /* Since lp is now pointing to a split which was past the reconcile
             * date, get the running balance of the previous split.
             */
            balance = xaccSplitGetBalance( (Split *)lp->prev->data );
        }
        else
        {
            /* AsOf date must be before any entries, return zero. */
            balance = gnc_numeric_zero();
        }
    }

    /* Otherwise there were no splits posted after the given date,
     * so the latest account balance should be good enough.
     */

    return( balance );
}

/*
 * Originally gsr_account_present_balance in gnc-split-reg.c
 *
 * How does this routine compare to xaccAccountGetBalanceAsOfDate just
 * above?  These two routines should eventually be collapsed into one.
 * Perhaps the startup logic from that one, and the logic from this
 * one that walks from the tail of the split list.
 */
gnc_numeric
xaccAccountGetPresentBalance (const Account *acc)
{
    AccountPrivate *priv;
    GList *node;
    time64 today;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), gnc_numeric_zero());
    if(!acc) return gnc_numeric_zero();

    priv = GET_PRIVATE(acc);
    today = gnc_time64_get_today_end();
    for (node = g_list_last(priv->splits); node; node = node->prev)
    {
        Split *split = node->data;

        if (xaccTransGetDate (xaccSplitGetParent (split)) <= today)
            return xaccSplitGetBalance (split);
    }

    return gnc_numeric_zero ();
}


/********************************************************************\
\********************************************************************/
/* XXX TODO: These 'GetBal' routines should be moved to some
 * utility area outside of the core account engine area.
 */

/*
 * Convert a balance from one currency to another.
 */
gnc_numeric
xaccAccountConvertBalanceToCurrency(const Account *acc, /* for book */
                                    gnc_numeric balance,
                                    const gnc_commodity *balance_currency,
                                    const gnc_commodity *new_currency)
{
    QofBook *book;
    GNCPriceDB *pdb;

    if (gnc_numeric_zero_p (balance) ||
            gnc_commodity_equiv (balance_currency, new_currency))
        return balance;

    book = gnc_account_get_book (acc);
    pdb = gnc_pricedb_get_db (book);

    balance = gnc_pricedb_convert_balance_latest_price(
                  pdb, balance, balance_currency, new_currency);

    return balance;
}

/*
 * Convert a balance from one currency to another with price of
 * a given date.
 */
gnc_numeric
xaccAccountConvertBalanceToCurrencyAsOfDate(const Account *acc, /* for book */
        gnc_numeric balance,
        gnc_commodity *balance_currency,
        gnc_commodity *new_currency,
        time64 date)
{
    QofBook *book;
    GNCPriceDB *pdb;
    Timespec ts;

    if (gnc_numeric_zero_p (balance) ||
            gnc_commodity_equiv (balance_currency, new_currency))
        return balance;

    book = gnc_account_get_book (acc);
    pdb = gnc_pricedb_get_db (book);

    ts.tv_sec = date;
    ts.tv_nsec = 0;

    balance = gnc_pricedb_convert_balance_nearest_price(
                  pdb, balance, balance_currency, new_currency, ts);

    return balance;
}

/*
 * Given an account and a GetBalanceFn pointer, extract the requested
 * balance from the account and then convert it to the desired
 * currency.
 */
static gnc_numeric
xaccAccountGetXxxBalanceInCurrency (const Account *acc,
                                    xaccGetBalanceFn fn,
                                    const gnc_commodity *report_currency)
{
    AccountPrivate *priv;
    gnc_numeric balance;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), gnc_numeric_zero());
//    g_return_val_if_fail(fn, gnc_numeric_zero());
//    g_return_val_if_fail(GNC_IS_COMMODITY(report_currency), gnc_numeric_zero());
    if(!acc) return gnc_numeric_zero();
    if(!fn) return gnc_numeric_zero();
    if(!report_currency) return gnc_numeric_zero();

    priv = GET_PRIVATE(acc);
    balance = fn(acc);
    balance = xaccAccountConvertBalanceToCurrency(acc, balance,
              priv->commodity,
              report_currency);
    return balance;
}

static gnc_numeric
xaccAccountGetXxxBalanceAsOfDateInCurrency(Account *acc, time64 date,
        xaccGetBalanceAsOfDateFn fn,
        const gnc_commodity *report_commodity)
{
    AccountPrivate *priv;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), gnc_numeric_zero());
//    g_return_val_if_fail(fn, gnc_numeric_zero());
//    g_return_val_if_fail(GNC_IS_COMMODITY(report_commodity), gnc_numeric_zero());
    if(!acc) return gnc_numeric_zero();
    if(!fn) return gnc_numeric_zero();
    if(!report_commodity) return gnc_numeric_zero();
    
    priv = GET_PRIVATE(acc);
    return xaccAccountConvertBalanceToCurrency(
               acc, fn(acc, date), priv->commodity, report_commodity);
}

/*
 * Data structure used to pass various arguments into the following fn.
 */
typedef struct
{
    const gnc_commodity *currency;
    gnc_numeric balance;
    xaccGetBalanceFn fn;
    xaccGetBalanceAsOfDateFn asOfDateFn;
    time64 date;
} CurrencyBalance;


/*
 * A helper function for iterating over all the accounts in a list or
 * tree.  This function is called once per account, and sums up the
 * values of all these accounts.
 */
static void
xaccAccountBalanceHelper (Account *acc, gpointer data)
{
    CurrencyBalance *cb = data;
    gnc_numeric balance;

    if (!cb->fn || !cb->currency)
        return;
    balance = xaccAccountGetXxxBalanceInCurrency (acc, cb->fn, cb->currency);
    cb->balance = gnc_numeric_add (cb->balance, balance,
                                   gnc_commodity_get_fraction (cb->currency),
                                   GNC_HOW_RND_ROUND_HALF_UP);
}

static void
xaccAccountBalanceAsOfDateHelper (Account *acc, gpointer data)
{
    CurrencyBalance *cb = data;
    gnc_numeric balance;

    g_return_if_fail (cb->asOfDateFn && cb->currency);

    balance = xaccAccountGetXxxBalanceAsOfDateInCurrency (
                  acc, cb->date, cb->asOfDateFn, cb->currency);
    cb->balance = gnc_numeric_add (cb->balance, balance,
                                   gnc_commodity_get_fraction (cb->currency),
                                   GNC_HOW_RND_ROUND_HALF_UP);
}



/*
 * Common function that iterates recursively over all accounts below
 * the specified account.  It uses xaccAccountBalanceHelper to sum up
 * the balances of all its children, and uses the specified function
 * 'fn' for extracting the balance.  This function may extract the
 * current value, the reconciled value, etc.
 *
 * If 'report_commodity' is NULL, just use the account's commodity.
 * If 'include_children' is FALSE, this function doesn't recurse at all.
 */
static gnc_numeric
xaccAccountGetXxxBalanceInCurrencyRecursive (const Account *acc,
        xaccGetBalanceFn fn,
        const gnc_commodity *report_commodity,
        bool include_children)
{
    gnc_numeric balance;

    if (!acc) return gnc_numeric_zero ();
    if (!report_commodity)
        report_commodity = xaccAccountGetCommodity (acc);
    if (!report_commodity)
        return gnc_numeric_zero();

    balance = xaccAccountGetXxxBalanceInCurrency (acc, fn, report_commodity);

    /* If needed, sum up the children converting to the *requested*
       commodity. */
    if (include_children)
    {
#ifdef _MSC_VER
        /* MSVC compiler: Somehow, the struct initialization containing a
           gnc_numeric doesn't work. As an exception, we hand-initialize
           that member afterwards. */
        CurrencyBalance cb = { report_commodity, { 0 }, fn, NULL, 0 };
        cb.balance = balance;
#else
        CurrencyBalance cb = { report_commodity, balance, fn, NULL, 0 };
#endif

        gnc_account_foreach_descendant (acc, xaccAccountBalanceHelper, &cb);
        balance = cb.balance;
    }

    return balance;
}

static gnc_numeric
xaccAccountGetXxxBalanceAsOfDateInCurrencyRecursive (
    Account *acc, time64 date, xaccGetBalanceAsOfDateFn fn,
    gnc_commodity *report_commodity, bool include_children)
{
    gnc_numeric balance;

    g_return_val_if_fail(acc, gnc_numeric_zero());
    if (!report_commodity)
        report_commodity = xaccAccountGetCommodity (acc);
    if (!report_commodity)
        return gnc_numeric_zero();

    balance = xaccAccountGetXxxBalanceAsOfDateInCurrency(
                  acc, date, fn, report_commodity);

    /* If needed, sum up the children converting to the *requested*
       commodity. */
    if (include_children)
    {
#ifdef _MSC_VER
        /* MSVC compiler: Somehow, the struct initialization containing a
           gnc_numeric doesn't work. As an exception, we hand-initialize
           that member afterwards. */
        CurrencyBalance cb = { report_commodity, 0, NULL, fn, date };
        cb.balance = balance;
#else
        CurrencyBalance cb = { report_commodity, balance, NULL, fn, date };
#endif

        gnc_account_foreach_descendant (acc, xaccAccountBalanceAsOfDateHelper, &cb);
        balance = cb.balance;
    }

    return balance;
}

gnc_numeric
xaccAccountGetBalanceInCurrency (const Account *acc,
                                 const gnc_commodity *report_commodity,
                                 bool include_children)
{
    gnc_numeric rc;
    rc = xaccAccountGetXxxBalanceInCurrencyRecursive (
             acc, xaccAccountGetBalance, report_commodity, include_children);
    PINFO(" baln=%" G_GINT64_FORMAT "/%" G_GINT64_FORMAT, rc.num, rc.denom);
    return rc;
}


gnc_numeric
xaccAccountGetClearedBalanceInCurrency (const Account *acc,
                                        const gnc_commodity *report_commodity,
                                        bool include_children)
{
    return xaccAccountGetXxxBalanceInCurrencyRecursive (
               acc, xaccAccountGetClearedBalance, report_commodity,
               include_children);
}

gnc_numeric
xaccAccountGetReconciledBalanceInCurrency (const Account *acc,
        const gnc_commodity *report_commodity,
        bool include_children)
{
    return xaccAccountGetXxxBalanceInCurrencyRecursive (
               acc, xaccAccountGetReconciledBalance, report_commodity,
               include_children);
}

gnc_numeric
xaccAccountGetPresentBalanceInCurrency (const Account *acc,
                                        const gnc_commodity *report_commodity,
                                        bool include_children)
{
    return xaccAccountGetXxxBalanceInCurrencyRecursive (
               acc, xaccAccountGetPresentBalance, report_commodity,
               include_children);
}

gnc_numeric
xaccAccountGetProjectedMinimumBalanceInCurrency (
    const Account *acc,
    const gnc_commodity *report_commodity,
    bool include_children)
{
    return xaccAccountGetXxxBalanceInCurrencyRecursive (
               acc, xaccAccountGetProjectedMinimumBalance, report_commodity,
               include_children);
}

gnc_numeric
xaccAccountGetBalanceAsOfDateInCurrency(
    Account *acc, time64 date, gnc_commodity *report_commodity,
    bool include_children)
{
    return xaccAccountGetXxxBalanceAsOfDateInCurrencyRecursive (
               acc, date, xaccAccountGetBalanceAsOfDate, report_commodity,
               include_children);
}

gnc_numeric
xaccAccountGetBalanceChangeForPeriod (Account *acc, time64 t1, time64 t2,
				      bool recurse)
{
    gnc_numeric b1, b2;

    b1 = xaccAccountGetBalanceAsOfDateInCurrency(acc, t1, NULL, recurse);
    b2 = xaccAccountGetBalanceAsOfDateInCurrency(acc, t2, NULL, recurse);
    return gnc_numeric_sub(b2, b1, GNC_DENOM_AUTO, GNC_HOW_DENOM_FIXED);
}


/********************************************************************\
\********************************************************************/

/* THIS API NEEDS TO CHANGE.
 *
 * This code exposes the internal structure of the account object to
 * external callers by returning the actual list used by the object.
 * It should instead return a copy of the split list that the caller
 * is required to free.  That change would provide the freedom of
 * allowing the internal organization to change data structures if
 * necessary for whatever reason, while leaving the external API
 * unchanged. */
/* XXX: violates the const'ness by forcing a sort before returning
 * the splitlist */
SplitList *
xaccAccountGetSplitList (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
    if(!acc) return NULL;
    xaccAccountSortSplits((Account*)acc, FALSE);  // normally a noop
    return GET_PRIVATE(acc)->splits;
}

LotList *
xaccAccountGetLotList (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
    if(!acc) return NULL;
    return g_list_copy(GET_PRIVATE(acc)->lots);
}

LotList *
xaccAccountFindOpenLots (const Account *acc,
                         bool (*match_func)(GNCLot *lot,
                                 gpointer user_data),
                         gpointer user_data, GCompareFunc sort_func)
{
    AccountPrivate *priv;
    GList *lot_list;
    GList *retval = NULL;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
    if(!acc) return NULL;

    priv = GET_PRIVATE(acc);
    for (lot_list = priv->lots; lot_list; lot_list = lot_list->next)
    {
        GNCLot *lot = lot_list->data;

        /* If this lot is closed, then ignore it */
        if (gnc_lot_is_closed (lot))
            continue;

        if (match_func && !(match_func)(lot, user_data))
            continue;

        /* Ok, this is a valid lot.  Add it to our list of lots */
        if (sort_func)
            retval = g_list_insert_sorted (retval, lot, sort_func);
        else
            retval = g_list_prepend (retval, lot);
    }

    return retval;
}

gpointer
xaccAccountForEachLot(const Account *acc,
                      gpointer (*proc)(GNCLot *lot, void *data), void *data)
{
    AccountPrivate *priv;
    LotList *node;
    gpointer result = NULL;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
//    g_return_val_if_fail(proc, NULL);
    if(!acc) return NULL;
    if(!proc) return NULL;

    priv = GET_PRIVATE(acc);
    for (node = priv->lots; node; node = node->next)
        if ((result = proc((GNCLot *)node->data, data)))
            break;

    return result;
}

/********************************************************************\
\********************************************************************/

/* These functions use interchange gint64 and bool.  Is that right? */
bool
xaccAccountGetTaxRelated (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), FALSE);
    if(!acc) return NULL;
    return kvp_frame_get_gint64(acc->kvp_data, "tax-related");
}

void
xaccAccountSetTaxRelated (Account *acc, bool tax_related)
{
    KvpValue *new_value;

//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return NULL;

    if (tax_related)
        new_value = kvp_value_new_gint64 (tax_related);
    else
        new_value = NULL;

    xaccAccountBeginEdit (acc);
    kvp_frame_set_slot_nc(acc->kvp_data, "tax-related", new_value);
    mark_account (acc);
    xaccAccountCommitEdit (acc);
}

const char *
xaccAccountGetTaxUSCode (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
    if(!acc) return NULL;
    return kvp_frame_get_string(acc->kvp_data, "tax-US/code");
}

void
xaccAccountSetTaxUSCode (Account *acc, const char *code)
{
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return NULL;

    xaccAccountBeginEdit (acc);
    kvp_frame_set_string (acc->kvp_data, "/tax-US/code", code);
    if (!code)
    {
        KvpFrame  *frame = NULL;
        kvp_frame_set_frame (acc->kvp_data, "/tax-US", frame);
    }
    mark_account (acc);
    xaccAccountCommitEdit (acc);
}

const char *
xaccAccountGetTaxUSPayerNameSource (const Account *acc)
{
//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), NULL);
    if(!acc) return NULL;
    return kvp_frame_get_string(acc->kvp_data,
                                "tax-US/payer-name-source");
}

void
xaccAccountSetTaxUSPayerNameSource (Account *acc, const char *source)
{
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    xaccAccountBeginEdit (acc);
    kvp_frame_set_string (acc->kvp_data,
                          "/tax-US/payer-name-source", source);
    mark_account (acc);
    xaccAccountCommitEdit (acc);
}

gint64
xaccAccountGetTaxUSCopyNumber (const Account *acc)
{
    gint64 copy_number;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), 1);
    if(!acc) return 1;
    
    copy_number = kvp_frame_get_gint64(acc->kvp_data,
                                       "tax-US/copy-number");
    return (copy_number == 0) ? 1 : copy_number;
}

void
xaccAccountSetTaxUSCopyNumber (Account *acc, gint64 copy_number)
{
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    xaccAccountBeginEdit (acc);
    if (copy_number != 0)
        kvp_frame_set_gint64 (acc->kvp_data, "/tax-US/copy-number", copy_number);
    else
    {
        KvpFrame * frame;
        KvpValue *value;

        value = NULL;
        frame = kvp_frame_set_value_nc (acc->kvp_data,
                                        "/tax-US/copy-number", value);
        if (!frame) kvp_value_delete (value);
    }
    mark_account (acc);
    xaccAccountCommitEdit (acc);
}

/********************************************************************\
\********************************************************************/

bool
xaccAccountGetPlaceholder (const Account *acc)
{
    const char *str;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), FALSE);
    if(!acc) return false;

    str = kvp_frame_get_string(acc->kvp_data, "placeholder");
    return (str && !strcmp(str, "true"));
}

void
xaccAccountSetPlaceholder (Account *acc, bool val)
{
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    xaccAccountBeginEdit (acc);
    kvp_frame_set_string (acc->kvp_data,
                          "placeholder", val ? "true" : NULL);
    mark_account (acc);
    xaccAccountCommitEdit (acc);
}

GNCPlaceholderType
xaccAccountGetDescendantPlaceholder (const Account *acc)
{
    GNCPlaceholderType ret = PLACEHOLDER_NONE;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), PLACEHOLDER_NONE);
    if(!acc) return PLACEHOLDER_NONE;
    
    if (xaccAccountGetPlaceholder(acc)) return PLACEHOLDER_THIS;

    AccountList_t descendants = gnc_account_get_descendants(acc);
    for (AccountList_t::const_iterator node = descendants.begin();
            node != descendants.end(); node++)
        if (xaccAccountGetPlaceholder(*node))
        {
            ret = PLACEHOLDER_CHILD;
            break;
        }

    return ret;
}

/********************************************************************\
\********************************************************************/

bool
xaccAccountGetHidden (const Account *acc)
{
    const char *str;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), FALSE);
    if(!acc) return false;

    str = kvp_frame_get_string(acc->kvp_data, "hidden");
    return (str && !strcmp(str, "true"));
}

void
xaccAccountSetHidden (Account *acc, bool val)
{
//    g_return_if_fail(GNC_IS_ACCOUNT(acc));
    if(!acc) return;

    xaccAccountBeginEdit (acc);
    kvp_frame_set_string (acc->kvp_data, "hidden",
                          val ? "true" : NULL);
    mark_account (acc);
    xaccAccountCommitEdit (acc);
}

bool
xaccAccountIsHidden (const Account *acc)
{
    AccountPrivate *priv;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), FALSE);
    if(!acc) return false;

    if (xaccAccountGetHidden(acc))
        return TRUE;
    priv = GET_PRIVATE(acc);
    while ((acc = priv->parent) != NULL)
    {
        priv = GET_PRIVATE(acc);
        if (xaccAccountGetHidden(acc))
            return TRUE;
    }
    return FALSE;
}

/********************************************************************\
\********************************************************************/

bool
xaccAccountHasAncestor (const Account *acc, const Account * ancestor)
{
    const Account *parent;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), FALSE);
//    g_return_val_if_fail(GNC_IS_ACCOUNT(ancestor), FALSE);
    if(!acc) return false;
    if(!ancestor) return false;

    parent = acc;
    while (parent && parent != ancestor)
        parent = GET_PRIVATE(parent)->parent;

    return (parent == ancestor);
}

/********************************************************************\
\********************************************************************/

/* You must edit the functions in this block in tandem.  KEEP THEM IN
   SYNC! */

#define GNC_RETURN_ENUM_AS_STRING(x) case (ACCT_TYPE_ ## x): return #x;

const char *
xaccAccountTypeEnumAsString(GNCAccountType type)
{
    switch (type)
    {
        GNC_RETURN_ENUM_AS_STRING(NONE);
        GNC_RETURN_ENUM_AS_STRING(BANK);
        GNC_RETURN_ENUM_AS_STRING(CASH);
        GNC_RETURN_ENUM_AS_STRING(CREDIT);
        GNC_RETURN_ENUM_AS_STRING(ASSET);
        GNC_RETURN_ENUM_AS_STRING(LIABILITY);
        GNC_RETURN_ENUM_AS_STRING(STOCK);
        GNC_RETURN_ENUM_AS_STRING(MUTUAL);
        GNC_RETURN_ENUM_AS_STRING(CURRENCY);
        GNC_RETURN_ENUM_AS_STRING(INCOME);
        GNC_RETURN_ENUM_AS_STRING(EXPENSE);
        GNC_RETURN_ENUM_AS_STRING(EQUITY);
        GNC_RETURN_ENUM_AS_STRING(RECEIVABLE);
        GNC_RETURN_ENUM_AS_STRING(PAYABLE);
        GNC_RETURN_ENUM_AS_STRING(ROOT);
        GNC_RETURN_ENUM_AS_STRING(TRADING);
        GNC_RETURN_ENUM_AS_STRING(CHECKING);
        GNC_RETURN_ENUM_AS_STRING(SAVINGS);
        GNC_RETURN_ENUM_AS_STRING(MONEYMRKT);
        GNC_RETURN_ENUM_AS_STRING(CREDITLINE);
    default:
        PERR ("asked to translate unknown account type %d.\n", type);
        break;
    }
    return(NULL);
}

#undef GNC_RETURN_ENUM_AS_STRING

#define GNC_RETURN_ON_MATCH(x) \
  if(g_strcmp0(#x, (str)) == 0) { *type = ACCT_TYPE_ ## x; return(TRUE); }

bool
xaccAccountStringToType(const char* str, GNCAccountType *type)
{

    GNC_RETURN_ON_MATCH(NONE);
    GNC_RETURN_ON_MATCH(BANK);
    GNC_RETURN_ON_MATCH(CASH);
    GNC_RETURN_ON_MATCH(CREDIT);
    GNC_RETURN_ON_MATCH(ASSET);
    GNC_RETURN_ON_MATCH(LIABILITY);
    GNC_RETURN_ON_MATCH(STOCK);
    GNC_RETURN_ON_MATCH(MUTUAL);
    GNC_RETURN_ON_MATCH(CURRENCY);
    GNC_RETURN_ON_MATCH(INCOME);
    GNC_RETURN_ON_MATCH(EXPENSE);
    GNC_RETURN_ON_MATCH(EQUITY);
    GNC_RETURN_ON_MATCH(RECEIVABLE);
    GNC_RETURN_ON_MATCH(PAYABLE);
    GNC_RETURN_ON_MATCH(ROOT);
    GNC_RETURN_ON_MATCH(TRADING);
    GNC_RETURN_ON_MATCH(CHECKING);
    GNC_RETURN_ON_MATCH(SAVINGS);
    GNC_RETURN_ON_MATCH(MONEYMRKT);
    GNC_RETURN_ON_MATCH(CREDITLINE);

    PERR("asked to translate unknown account type string %s.\n",
         str ? str : "(null)");

    return(FALSE);
}

#undef GNC_RETURN_ON_MATCH

/* impedance mismatch is a source of loss */
GNCAccountType
xaccAccountStringToEnum(const char* str)
{
    GNCAccountType type;
    bool rc;
    rc = xaccAccountStringToType(str, &type);
    if (FALSE == rc) return ACCT_TYPE_INVALID;
    return type;
}

/********************************************************************\
\********************************************************************/

static char *
account_type_name[NUM_ACCOUNT_TYPES] =
{
    N_("Bank"),
    N_("Cash"),
    N_("Asset"),
    N_("Credit Card"),
    N_("Liability"),
    N_("Stock"),
    N_("Mutual Fund"),
    N_("Currency"),
    N_("Income"),
    N_("Expense"),
    N_("Equity"),
    N_("A/Receivable"),
    N_("A/Payable"),
    N_("Root"),
    N_("Trading")
    /*
      N_("Checking"),
      N_("Savings"),
      N_("Money Market"),
      N_("Credit Line")
    */
};

const char *
xaccAccountGetTypeStr(GNCAccountType type)
{
    if (type < 0 || NUM_ACCOUNT_TYPES <= type ) return "";
    return _(account_type_name [type]);
}

/********************************************************************\
\********************************************************************/

guint32
xaccParentAccountTypesCompatibleWith (GNCAccountType type)
{
    switch (type)
    {
    case ACCT_TYPE_BANK:
    case ACCT_TYPE_CASH:
    case ACCT_TYPE_ASSET:
    case ACCT_TYPE_STOCK:
    case ACCT_TYPE_MUTUAL:
    case ACCT_TYPE_CURRENCY:
    case ACCT_TYPE_CREDIT:
    case ACCT_TYPE_LIABILITY:
    case ACCT_TYPE_RECEIVABLE:
    case ACCT_TYPE_PAYABLE:
        return
            (1 << ACCT_TYPE_BANK)       |
            (1 << ACCT_TYPE_CASH)       |
            (1 << ACCT_TYPE_ASSET)      |
            (1 << ACCT_TYPE_STOCK)      |
            (1 << ACCT_TYPE_MUTUAL)     |
            (1 << ACCT_TYPE_CURRENCY)   |
            (1 << ACCT_TYPE_CREDIT)     |
            (1 << ACCT_TYPE_LIABILITY)  |
            (1 << ACCT_TYPE_RECEIVABLE) |
            (1 << ACCT_TYPE_PAYABLE)    |
            (1 << ACCT_TYPE_ROOT);
    case ACCT_TYPE_INCOME:
    case ACCT_TYPE_EXPENSE:
        return
            (1 << ACCT_TYPE_INCOME)     |
            (1 << ACCT_TYPE_EXPENSE)    |
            (1 << ACCT_TYPE_ROOT);
    case ACCT_TYPE_EQUITY:
        return
            (1 << ACCT_TYPE_EQUITY)     |
            (1 << ACCT_TYPE_ROOT);
    case ACCT_TYPE_TRADING:
        return
            (1 << ACCT_TYPE_TRADING)    |
            (1 << ACCT_TYPE_ROOT);
    default:
        PERR("bad account type: %d", type);
        return 0;
    }
}

bool
xaccAccountTypesCompatible (GNCAccountType parent_type,
                            GNCAccountType child_type)
{
    return ((xaccParentAccountTypesCompatibleWith (parent_type) &
             (1 << child_type))
            != 0);
}

guint32
xaccAccountTypesValid(void)
{
    guint32 mask = (1 << NUM_ACCOUNT_TYPES) - 1;
    mask &= ~((1 << ACCT_TYPE_CURRENCY) |  /* DEPRECATED */
              (1 << ACCT_TYPE_ROOT));      /* ROOT */

    return mask;
}

bool
xaccAccountIsPriced(const Account *acc)
{
    AccountPrivate *priv;

//    g_return_val_if_fail(GNC_IS_ACCOUNT(acc), FALSE);
    if(!acc) return false;

    priv = GET_PRIVATE(acc);
    return (priv->type == ACCT_TYPE_STOCK || priv->type == ACCT_TYPE_MUTUAL ||
            priv->type == ACCT_TYPE_CURRENCY);
}

/********************************************************************\
\********************************************************************/

bool
xaccAccountGetReconcileLastDate (const Account *acc, time64 *last_date)
{
    KvpValue *v;

    if (!acc) return FALSE;

    v = kvp_frame_get_value(acc->kvp_data, "reconcile-info/last-date");

    if (!v || kvp_value_get_type(v) != KVP_TYPE_GINT64)
        return FALSE;

    if (last_date)
        *last_date = kvp_value_get_gint64(v);

    return TRUE;
}

/********************************************************************\
\********************************************************************/

void
xaccAccountSetReconcileLastDate (Account *acc, time64 last_date)
{
    if (!acc) return;

    xaccAccountBeginEdit (acc);
    kvp_frame_set_gint64 (acc->kvp_data,
                          "/reconcile-info/last-date", last_date);
    mark_account (acc);
    xaccAccountCommitEdit (acc);
}

/********************************************************************\
\********************************************************************/

bool
xaccAccountGetReconcileLastInterval (const Account *acc,
                                     int *months, int *days)
{
    KvpValue *v1, *v2;

    if (!acc) return FALSE;

    v1 = kvp_frame_get_value(acc->kvp_data,
                             "reconcile-info/last-interval/months");
    v2 = kvp_frame_get_value(acc->kvp_data,
                             "reconcile-info/last-interval/days");
    if (!v1 || (kvp_value_get_type (v1) != KVP_TYPE_GINT64) ||
            !v2 || (kvp_value_get_type (v2) != KVP_TYPE_GINT64))
        return FALSE;

    if (months)
        *months = kvp_value_get_gint64 (v1);
    if (days)
        *days = kvp_value_get_gint64 (v2);
    return TRUE;
}

/********************************************************************\
\********************************************************************/

void
xaccAccountSetReconcileLastInterval (Account *acc, int months, int days)
{
    KvpFrame *frame;
    if (!acc) return;

    xaccAccountBeginEdit (acc);

    frame = kvp_frame_get_frame_slash (acc->kvp_data,
                                       "/reconcile-info/last-interval");
    g_assert(frame);

    kvp_frame_set_gint64 (frame, "months", months);
    kvp_frame_set_gint64 (frame, "days", days);

    mark_account (acc);
    xaccAccountCommitEdit (acc);
}

/********************************************************************\
\********************************************************************/

bool
xaccAccountGetReconcilePostponeDate (const Account *acc, time64 *postpone_date)
{
    KvpValue *v;

    if (!acc) return FALSE;

    v = kvp_frame_get_value(acc->kvp_data, "reconcile-info/postpone/date");
    if (!v || kvp_value_get_type (v) != KVP_TYPE_GINT64)
        return FALSE;

    if (postpone_date)
        *postpone_date = kvp_value_get_gint64 (v);

    return TRUE;
}

/********************************************************************\
\********************************************************************/

void
xaccAccountSetReconcilePostponeDate (Account *acc, time64 postpone_date)
{
    if (!acc) return;

    xaccAccountBeginEdit (acc);

    /* XXX this should be using timespecs, not gints !! */
    kvp_frame_set_gint64 (acc->kvp_data,
                          "reconcile-info/postpone/date", postpone_date);
    mark_account (acc);
    xaccAccountCommitEdit (acc);
}

/********************************************************************\
\********************************************************************/

bool
xaccAccountGetReconcilePostponeBalance (const Account *acc,
                                        gnc_numeric *balance)
{
    KvpValue *v;

    if (!acc) return FALSE;

    v = kvp_frame_get_value(acc->kvp_data,
                            "reconcile-info/postpone/balance");
    if (!v || kvp_value_get_type (v) != KVP_TYPE_NUMERIC)
        return FALSE;

    if (balance)
        *balance = kvp_value_get_numeric (v);

    return TRUE;
}

/********************************************************************\
\********************************************************************/

void
xaccAccountSetReconcilePostponeBalance (Account *acc, gnc_numeric balance)
{
    if (!acc) return;

    xaccAccountBeginEdit (acc);
    kvp_frame_set_gnc_numeric (acc->kvp_data,
                               "/reconcile-info/postpone/balance", balance);
    mark_account (acc);
    xaccAccountCommitEdit (acc);
}

/********************************************************************\

\********************************************************************/

void
xaccAccountClearReconcilePostpone (Account *acc)
{
    if (!acc) return;

    xaccAccountBeginEdit (acc);
    kvp_frame_set_value (acc->kvp_data, "reconcile-info/postpone", NULL);
    mark_account (acc);
    xaccAccountCommitEdit (acc);
}

/********************************************************************\
\********************************************************************/

/* xaccAccountGetAutoInterestXfer: determine whether the auto interest
 * xfer option is enabled for this account, and return that value.
 * If it is not defined for the account, return the default value.
 */
bool
xaccAccountGetAutoInterestXfer (const Account *acc, bool default_value)
{
    const char *str = NULL;
    if (!acc) return default_value;

    str = kvp_frame_get_string(acc->kvp_data,
                               "reconcile-info/auto-interest-transfer");
    return str ? !strcmp(str, "true") : default_value;
}

/********************************************************************\
\********************************************************************/

void
xaccAccountSetAutoInterestXfer (Account *acc, bool option)
{
    if (!acc) return;

    xaccAccountBeginEdit (acc);
    /* FIXME: need KVP_TYPE_BOOLEAN for this someday */
    kvp_frame_set_string (acc->kvp_data,
                          "/reconcile-info/auto-interest-transfer",
                          (option ? "true" : "false"));
    mark_account (acc);
    xaccAccountCommitEdit (acc);
}

/********************************************************************\
\********************************************************************/

const char *
xaccAccountGetLastNum (const Account *acc)
{
    return acc ? kvp_frame_get_string(acc->kvp_data, "last-num") : NULL;
}

/********************************************************************\
\********************************************************************/

void
xaccAccountSetLastNum (Account *acc, const char *num)
{
    if (!acc) return;

    xaccAccountBeginEdit (acc);
    kvp_frame_set_string(acc->kvp_data, "last-num", num);
    mark_account (acc);
    xaccAccountCommitEdit (acc);
}

/********************************************************************\
\********************************************************************/

void
dxaccAccountSetPriceSrc(Account *acc, const char *src)
{
    if (!acc) return;

    xaccAccountBeginEdit(acc);
    if (xaccAccountIsPriced(acc))
    {
        kvp_frame_set_slot_nc(acc->kvp_data,
                              "old-price-source",
                              src ? kvp_value_new_string(src) : NULL);
        mark_account (acc);
    }

    qof_instance_set_dirty(acc);
    xaccAccountCommitEdit(acc);
}

/********************************************************************\
\********************************************************************/

const char*
dxaccAccountGetPriceSrc(const Account *acc)
{
    if (!acc) return NULL;

    if (xaccAccountIsPriced(acc))
    {
        KvpValue *value = kvp_frame_get_slot(acc->kvp_data,
                                             "old-price-source");
        if (value) return (kvp_value_get_string(value));
    }
    return NULL;
}

/********************************************************************\
\********************************************************************/

void
dxaccAccountSetQuoteTZ(Account *acc, const char *tz)
{
    if (!acc) return;

    xaccAccountBeginEdit(acc);
    if (xaccAccountIsPriced(acc))
    {
        kvp_frame_set_slot_nc(acc->kvp_data,
                              "old-quote-tz",
                              tz ? kvp_value_new_string(tz) : NULL);
        mark_account (acc);
    }
    qof_instance_set_dirty(acc);
    xaccAccountCommitEdit(acc);
}

/********************************************************************\
\********************************************************************/

const char*
dxaccAccountGetQuoteTZ(const Account *acc)
{
    if (!acc) return NULL;

    if (xaccAccountIsPriced(acc))
    {
        KvpValue *value = kvp_frame_get_slot(acc->kvp_data, "old-quote-tz");
        if (value) return (kvp_value_get_string(value));
    }
    return NULL;
}

/********************************************************************\
\********************************************************************/

void
xaccAccountSetReconcileChildrenStatus(Account *acc, bool status)
{
    if (!acc) return;

    xaccAccountBeginEdit (acc);

    /* XXX FIXME: someday this should use KVP_TYPE_BOOLEAN */
    kvp_frame_set_gint64 (acc->kvp_data,
                          "/reconcile-info/include-children", status);
    mark_account(acc);
    xaccAccountCommitEdit (acc);
}

/********************************************************************\
\********************************************************************/

bool
xaccAccountGetReconcileChildrenStatus(const Account *acc)
{
    /* access the account's kvp-data for status and return that, if no value
     * is found then we can assume not to include the children, that being
     * the default behaviour
     */
    return acc ? kvp_frame_get_gint64(acc->kvp_data,
                                      "reconcile-info/include-children") : FALSE;
}

/********************************************************************\
\********************************************************************/

/* The caller of this function can get back one or both of the
 * matching split and transaction pointers, depending on whether
 * a valid pointer to the location to store those pointers is
 * passed.
 */
static void
finder_help_function(const Account *acc, const char *description,
                     Split **split, Transaction **trans )
{
    AccountPrivate *priv;
    GList *slp;

    /* First, make sure we set the data to NULL BEFORE we start */
    if (split) *split = NULL;
    if (trans) *trans = NULL;

    /* Then see if we have any work to do */
    if (acc == NULL) return;

    /* Why is this loop iterated backwards ?? Presumably because the split
     * list is in date order, and the most recent matches should be
     * returned!?  */
    priv = GET_PRIVATE(acc);
    for (slp = g_list_last(priv->splits); slp; slp = slp->prev)
    {
        Split *lsplit = slp->data;
        Transaction *ltrans = xaccSplitGetParent(lsplit);

        if (g_strcmp0 (description, xaccTransGetDescription (ltrans)) == 0)
        {
            if (split) *split = lsplit;
            if (trans) *trans = ltrans;
            return;
        }
    }
}

Split *
xaccAccountFindSplitByDesc(const Account *acc, const char *description)
{
    Split *split;

    /* Get the split which has a transaction matching the description. */
    finder_help_function(acc, description, &split, NULL);
    return split;
}

/* This routine is for finding a matching transaction in an account by
 * matching on the description field. [CAS: The rest of this comment
 * seems to belong somewhere else.] This routine is used for
 * auto-filling in registers with a default leading account. The
 * dest_trans is a transaction used for currency checking. */
Transaction *
xaccAccountFindTransByDesc(const Account *acc, const char *description)
{
    Transaction *trans;

    /* Get the translation matching the description. */
    finder_help_function(acc, description, NULL, &trans);
    return trans;
}

/* ================================================================ */
/* Concatenation, Merging functions                                */

void
gnc_account_join_children (Account *to_parent, Account *from_parent)
{
    AccountPrivate *from_priv;

    /* errors */
//    g_return_if_fail(GNC_IS_ACCOUNT(to_parent));
//    g_return_if_fail(GNC_IS_ACCOUNT(from_parent));
    if(!to_parent) return;
    if(!from_parent) return;

    /* optimizations */
    from_priv = GET_PRIVATE(from_parent);
    if (from_priv->children.empty())
        return;

    ENTER (" ");
    for (AccountList_t::const_iterator node = from_priv->children.begin();
            node != from_priv->children.end(); node++)
        gnc_account_append_child(to_parent, *node);
    LEAVE (" ");
}
/********************************************************************\
\********************************************************************/

void
gnc_account_merge_children (Account *parent)
{
    AccountPrivate *ppriv, *priv_a, *priv_b;

//    g_return_if_fail(GNC_IS_ACCOUNT(parent));
    if(!parent) return;

    ppriv = GET_PRIVATE(parent);
    for (AccountList_t::iterator node_a = ppriv->children.begin();
            node_a != ppriv->children.end(); node_a++)
    {
        Account *acc_a = *node_a;

        priv_a = GET_PRIVATE(acc_a);
        AccountList_t::iterator node_b = node_a;
        node_b++;
        for (; node_b != ppriv->children.end(); node_b++)
        {
            Account *acc_b = *node_b;

            priv_b = GET_PRIVATE(acc_b);
            if (0 != null_strcmp(priv_a->accountName, priv_b->accountName))
                continue;
            if (0 != null_strcmp(priv_a->accountCode, priv_b->accountCode))
                continue;
            if (0 != null_strcmp(priv_a->description, priv_b->description))
                continue;
            if (0 != null_strcmp(xaccAccountGetColor(acc_a),
                                 xaccAccountGetColor(acc_b)))
                continue;
            if (!gnc_commodity_equiv(priv_a->commodity, priv_b->commodity))
                continue;
            if (0 != null_strcmp(xaccAccountGetNotes(acc_a),
                                 xaccAccountGetNotes(acc_b)))
                continue;
            if (priv_a->type != priv_b->type)
                continue;

            /* consolidate children */
            if (! priv_b->children.empty())
            {
                for (AccountList_t::iterator worker = priv_b->children.begin();
                        worker != priv_b->children.end(); worker++)
                    gnc_account_append_child (acc_a, *worker);

                qof_event_gen (acc_a, QOF_EVENT_MODIFY, NULL);
                qof_event_gen (acc_b, QOF_EVENT_MODIFY, NULL);
            }

            /* recurse to do the children's children */
            gnc_account_merge_children (acc_a);

            /* consolidate transactions */
            while (priv_b->splits)
                xaccSplitSetAccount (priv_b->splits->data, acc_a);

            /* move back one before removal. next iteration around the loop
             * will get the node after node_b */
            node_b--;

            /* The destroy function will remove from list -- node_a is ok,
             * it's before node_b */
            xaccAccountBeginEdit (acc_b);
            xaccAccountDestroy (acc_b);
        }
    }
}

/* ================================================================ */
/* Transaction Traversal functions                                  */


void
xaccSplitsBeginStagedTransactionTraversals (GList *splits)
{
    GList *lp;

    for (lp = splits; lp; lp = lp->next)
    {
        Split *s = lp->data;
        Transaction *trans = s->parent;

        if (trans)
            trans->marker = 0;
    }
}

/* original function */
void
xaccAccountBeginStagedTransactionTraversals (const Account *account)
{
    AccountPrivate *priv;

    if (!account)
        return;
    priv = GET_PRIVATE(account);
    xaccSplitsBeginStagedTransactionTraversals(priv->splits);
}

bool
xaccTransactionTraverse (Transaction *trans, int stage)
{
    if (trans == NULL) return FALSE;

    if (trans->marker < stage)
    {
        trans->marker = stage;
        return TRUE;
    }

    return FALSE;
}

static void do_one_split (Split *s, gpointer data)
{
    Transaction *trans = s->parent;
    trans->marker = 0;
}

static void do_one_account (Account *account)
{
    AccountPrivate *priv = GET_PRIVATE(account);
    g_list_foreach(priv->splits, (GFunc)do_one_split, NULL);
}

/* Replacement for xaccGroupBeginStagedTransactionTraversals */
void
gnc_account_tree_begin_staged_transaction_traversals (Account *account)
{
    AccountList_t descendants = gnc_account_get_descendants(account);
    std::for_each(descendants.begin(), descendants.end(), do_one_account);
}

int
xaccAccountStagedTransactionTraversal (const Account *acc,
                                       unsigned int stage,
                                       TransactionCallback thunk,
                                       void *cb_data)
{
    AccountPrivate *priv;
    GList *split_p;
    GList *next;
    Transaction *trans;
    Split *s;
    int retval;

    if (!acc) return 0;

    priv = GET_PRIVATE(acc);
    for (split_p = priv->splits; split_p; split_p = next)
    {
        /* Get the next element in the split list now, just in case some
         * naughty thunk destroys the one we're using. This reduces, but
         * does not eliminate, the possibility of undefined results if
         * a thunk removes splits from this account. */
        next = g_list_next(split_p);

        s = split_p->data;
        trans = s->parent;
        if (trans && (trans->marker < stage))
        {
            trans->marker = stage;
            if (thunk)
            {
                retval = thunk(trans, cb_data);
                if (retval) return retval;
            }
        }
    }

    return 0;
}

int
gnc_account_tree_staged_transaction_traversal (const Account *acc,
        unsigned int stage,
        TransactionCallback thunk,
        void *cb_data)
{
    const AccountPrivate *priv;
    GList *split_p;
    Transaction *trans;
    Split *s;
    int retval;

    if (!acc) return 0;

    /* depth first traversal */
    priv = GET_PRIVATE(acc);
    for (AccountList_t::const_iterator acc_p = priv->children.begin();
            acc_p != priv->children.end(); acc_p++)
    {
        retval = gnc_account_tree_staged_transaction_traversal(*acc_p, stage,
                 thunk, cb_data);
        if (retval) return retval;
    }

    /* Now this account */
    for (split_p = priv->splits; split_p; split_p = g_list_next(split_p))
    {
        s = split_p->data;
        trans = s->parent;
        if (trans && (trans->marker < stage))
        {
            trans->marker = stage;
            if (thunk)
            {
                retval = thunk(trans, cb_data);
                if (retval) return retval;
            }
        }
    }

    return 0;
}

/********************************************************************\
\********************************************************************/

int
xaccAccountTreeForEachTransaction (Account *acc,
                                   int (*proc)(Transaction *t, void *data),
                                   void *data)
{
    if (!acc || !proc) return 0;

    gnc_account_tree_begin_staged_transaction_traversals (acc);
    return gnc_account_tree_staged_transaction_traversal (acc, 42, proc, data);
}


gint
xaccAccountForEachTransaction(const Account *acc, TransactionCallback proc,
                              void *data)
{
    if (!acc || !proc) return 0;
    xaccAccountBeginStagedTransactionTraversals (acc);
    return xaccAccountStagedTransactionTraversal(acc, 42, proc, data);
}

/* ================================================================ */
/* QofObject function implementation and registration */

static void
gnc_account_book_end(QofBook* book)
{
    Account *root_account = gnc_book_get_root_account(book);

    xaccAccountBeginEdit(root_account);
    xaccAccountDestroy(root_account);
}

/* C++ compilers don't have C99 "designated initializers"
 * so we wrap them in a macro that is empty */
# define DI(x) /* */

static QofObject account_object_def =
{
    DI(.interface_version = ) QOF_OBJECT_VERSION,
    DI(.e_type            = ) GNC_ID_ACCOUNT,
    DI(.type_label        = ) "Account",
    DI(.create            = ) (gpointer)xaccMallocAccount,
    DI(.book_begin        = ) NULL,
    DI(.book_end          = ) gnc_account_book_end,
    DI(.is_dirty          = ) qof_collection_is_dirty,
    DI(.mark_clean        = ) qof_collection_mark_clean,
    DI(.foreach           = ) qof_collection_foreach,
    DI(.printable         = ) (const char * (*)(gpointer)) xaccAccountGetName,
    DI(.version_cmp       = ) (int (*)(gpointer, gpointer)) qof_instance_version_cmp,
};

bool xaccAccountRegister (void)
{
    static QofParam params[] =
    {
        {
            ACCOUNT_NAME_, QOF_TYPE_STRING,
            (QofAccessFunc) xaccAccountGetName,
            (QofSetterFunc) xaccAccountSetName
        },
        {
            ACCOUNT_CODE_, QOF_TYPE_STRING,
            (QofAccessFunc) xaccAccountGetCode,
            (QofSetterFunc) xaccAccountSetCode
        },
        {
            ACCOUNT_DESCRIPTION_, QOF_TYPE_STRING,
            (QofAccessFunc) xaccAccountGetDescription,
            (QofSetterFunc) xaccAccountSetDescription
        },
        {
            ACCOUNT_COLOR_, QOF_TYPE_STRING,
            (QofAccessFunc) xaccAccountGetColor,
            (QofSetterFunc) xaccAccountSetColor
        },
        {
            ACCOUNT_FILTER_, QOF_TYPE_STRING,
            (QofAccessFunc) xaccAccountGetFilter,
            (QofSetterFunc) xaccAccountSetFilter
        },
        {
            ACCOUNT_SORT_ORDER_, QOF_TYPE_STRING,
            (QofAccessFunc) xaccAccountGetSortOrder,
            (QofSetterFunc) xaccAccountSetSortOrder
        },
        {
            ACCOUNT_NOTES_, QOF_TYPE_STRING,
            (QofAccessFunc) xaccAccountGetNotes,
            (QofSetterFunc) xaccAccountSetNotes
        },
        {
            ACCOUNT_PRESENT_, QOF_TYPE_NUMERIC,
            (QofAccessFunc) xaccAccountGetPresentBalance, NULL
        },
        {
            ACCOUNT_BALANCE_, QOF_TYPE_NUMERIC,
            (QofAccessFunc) xaccAccountGetBalance, NULL
        },
        {
            ACCOUNT_CLEARED_, QOF_TYPE_NUMERIC,
            (QofAccessFunc) xaccAccountGetClearedBalance, NULL
        },
        {
            ACCOUNT_RECONCILED_, QOF_TYPE_NUMERIC,
            (QofAccessFunc) xaccAccountGetReconciledBalance, NULL
        },
        {
            ACCOUNT_TYPE_, QOF_TYPE_STRING,
            (QofAccessFunc) qofAccountGetTypeString,
            (QofSetterFunc) qofAccountSetType
        },
        {
            ACCOUNT_FUTURE_MINIMUM_, QOF_TYPE_NUMERIC,
            (QofAccessFunc) xaccAccountGetProjectedMinimumBalance, NULL
        },
        {
            ACCOUNT_TAX_RELATED, QOF_TYPE_BOOLEAN,
            (QofAccessFunc) xaccAccountGetTaxRelated,
            (QofSetterFunc) xaccAccountSetTaxRelated
        },
        {
            ACCOUNT_SCU, QOF_TYPE_INT32,
            (QofAccessFunc) xaccAccountGetCommoditySCU,
            (QofSetterFunc) xaccAccountSetCommoditySCU
        },
        {
            ACCOUNT_NSCU, QOF_TYPE_BOOLEAN,
            (QofAccessFunc) xaccAccountGetNonStdSCU,
            (QofSetterFunc) xaccAccountSetNonStdSCU
        },
        {
            ACCOUNT_PARENT, GNC_ID_ACCOUNT,
            (QofAccessFunc) gnc_account_get_parent,
            (QofSetterFunc) qofAccountSetParent
        },
        {
            QOF_PARAM_BOOK, QOF_ID_BOOK,
            (QofAccessFunc) qof_instance_get_book, NULL
        },
        {
            QOF_PARAM_GUID, QOF_TYPE_GUID,
            (QofAccessFunc) qof_instance_get_guid, NULL
        },
        {
            ACCOUNT_KVP, QOF_TYPE_KVP,
            (QofAccessFunc) qof_instance_get_slots, NULL
        },
        { NULL },
    };

    qof_class_register (GNC_ID_ACCOUNT, (QofSortFunc) qof_xaccAccountOrder, params);

    return qof_object_register (&account_object_def);
}

/* ======================= UNIT TESTING ACCESS =======================
 * The following functions are for unit testing use only.
 */
static AccountPrivate*
utest_account_get_private (Account *acc)
{
    return GET_PRIVATE (acc);
}

AccountTestFunctions*
_utest_account_fill_functions(void)
{
    AccountTestFunctions* func = new AccountTestFunctions;//g_new(AccountTestFunctions, 1);

    func->get_private = utest_account_get_private;
    func->coll_get_root_account = gnc_coll_get_root_account;
    func->xaccFreeAccountChildren = xaccFreeAccountChildren;
    func->xaccFreeAccount = xaccFreeAccount;
    func->qofAccountSetParent = qofAccountSetParent;
    func->gnc_account_lookup_by_full_name_helper =
        gnc_account_lookup_by_full_name_helper;

    return func;
}
/* ======================= END OF FILE =========================== */
