/********************************************************************\
 * gnc-account-xml-v2.cpp -- account xml i/o implementation         *
 *                                                                  *
 * Copyright (C) 2001 James LewisMoss <dres@debian.org>             *
 * Copyright (C) 2002 Linas Vepstas <linas@linas.org>               *
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
#include <stdlib.h>
#include <string.h>

#include "gnc-xml-helper.h"
#include "sixtp.h"
#include "sixtp-utils.h"
#include "sixtp-parsers.h"
#include "sixtp-utils.h"
#include "sixtp-dom-parsers.h"
#include "sixtp-dom-generators.h"

#include "gnc-xml.h"
#include "io-gncxml-gen.h"
#include "io-gncxml-v2.h"

#include "sixtp-dom-parsers.h"
#include "AccountP.h"
#include "Account.h"
#include "gnc-lot.h"

static QofLogModule log_module = GNC_MOD_IO;

const gchar *account_version_string = "2.0.0";

/* ids */
#define gnc_account_string "gnc:account"
#define act_name_string "act:name"
#define act_id_string "act:id"
#define act_type_string "act:type"
#define act_commodity_string "act:commodity"
#define act_commodity_scu_string "act:commodity-scu"
#define act_non_standard_scu_string "act:non-standard-scu"
#define act_code_string "act:code"
#define act_description_string "act:description"
#define act_slots_string "act:slots"
#define act_parent_string "act:parent"
#define act_lots_string "act:lots"
/* The currency and security strings should not appear in newer
 * xml files (anything post-gnucash-1.6) */
#define act_currency_string "act:currency"
#define act_currency_scu_string "act:currency-scu"
#define act_security_string "act:security"
#define act_security_scu_string "act:security-scu"
#define act_hidden_string "act:hidden"
#define act_placeholder_string "act:placeholder"

xmlNodePtr
gnc_account_dom_tree_create(Account *act,
                            bool exporting,
                            bool allow_incompat)
{
    const char *str;
    kvp_frame *kf;
    xmlNodePtr ret;
    GList *n;
    Account *parent;
    gnc_commodity *acct_commodity;

    ENTER ("(account=%p)", act);

    ret = xmlNewNode(NULL, BAD_CAST gnc_account_string);
    xmlSetProp(ret, BAD_CAST "version", BAD_CAST account_version_string);

    xmlAddChild(ret, text_to_dom_tree(act_name_string,
                                      xaccAccountGetName(act)));

    xmlAddChild(ret, guid_to_dom_tree(act_id_string, xaccAccountGetGUID(act)));

    xmlAddChild(ret, text_to_dom_tree(
                    act_type_string,
                    xaccAccountTypeEnumAsString(xaccAccountGetType(act))));

    /* Don't write new XML tags in version 2.3.x and 2.4.x because it
       would mean 2.2.x cannot read those files again. But we can
       enable writing these tags in 2.5.x or late in 2.4.x. */
    /*
    xmlAddChild(ret, boolean_to_dom_tree(
    						act_hidden_string,
    						xaccAccountGetHidden(act)));
    xmlAddChild(ret, boolean_to_dom_tree(
    						act_placeholder_string,
    						xaccAccountGetPlaceholder(act)));
    */

    acct_commodity = xaccAccountGetCommodity(act);
    if (acct_commodity != NULL)
    {
        xmlAddChild(ret, commodity_ref_to_dom_tree(act_commodity_string,
                    acct_commodity));

        xmlAddChild(ret, int_to_dom_tree(act_commodity_scu_string,
                                         xaccAccountGetCommoditySCUi(act)));

        if (xaccAccountGetNonStdSCU(act))
            xmlNewChild(ret, NULL, BAD_CAST act_non_standard_scu_string, NULL);
    }

    str = xaccAccountGetCode(act);
    if (str && strlen(str) > 0)
    {
        xmlAddChild(ret, text_to_dom_tree(act_code_string, str));
    }

    str = xaccAccountGetDescription(act);
    if (str && strlen(str) > 0)
    {
        xmlAddChild(ret, text_to_dom_tree(act_description_string, str));
    }

    kf = xaccAccountGetSlots(act);
    if (kf)
    {
        xmlNodePtr kvpnode = kvp_frame_to_dom_tree(act_slots_string, kf);
        if (kvpnode)
        {
            xmlAddChild(ret, kvpnode);
        }
    }

    parent = gnc_account_get_parent(act);
    if (parent)
    {
        if (!gnc_account_is_root(parent) || allow_incompat)
            xmlAddChild(ret, guid_to_dom_tree(act_parent_string,
                                              xaccAccountGetGUID(parent)));
    }

    LotList_t lots = xaccAccountGetLotList (act);
    PINFO ("lot list count=%p", lots.size());
    if (!lots.empty() && !exporting)
    {
        xmlNodePtr toaddto = xmlNewChild(ret, NULL, BAD_CAST act_lots_string, NULL);

        lots.sort(lots_guid_weakorder);

        for (LotList_t::const_iterator it = lots.begin(); it != lots.end(); it++)
        {
            xmlAddChild(toaddto, gnc_lot_dom_tree_create(*it));
        }
    }

    LEAVE("");
    return ret;
}

/***********************************************************************/

struct account_pdata
{
    Account *account;
    QofBook *book;
};

static inline bool
set_string(xmlNodePtr node, Account* act,
           void (*func)(Account *act, const gchar *txt))
{
    gchar* txt = dom_tree_to_text(node);
//    g_return_val_if_fail(txt, FALSE);
    if(!txt) return false;

    func(act, txt);

    g_free(txt);

    return true;
}

static bool
account_name_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;

    return set_string(node, pdata->account, xaccAccountSetName);
}

static bool
account_id_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    GncGUID *guid;

    guid = dom_tree_to_guid(node);
//    g_return_val_if_fail(guid, FALSE);
    if(!guid) return false;

    xaccAccountSetGUID(pdata->account, guid);

    g_free(guid);

    return true;
}

static bool
account_type_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    GNCAccountType type = ACCT_TYPE_INVALID;
    char *string;

    string = (char*) xmlNodeGetContent (node->xmlChildrenNode);
    xaccAccountStringToType(string, &type);
    xmlFree (string);

    xaccAccountSetType(pdata->account, type);

    return true;
}

static bool
account_commodity_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    gnc_commodity *ref;

//    ref = dom_tree_to_commodity_ref_no_engine(node, pdata->book);
    ref = dom_tree_to_commodity_ref(node, pdata->book);
    xaccAccountSetCommodity(pdata->account, ref);

    return true;
}

static bool
account_commodity_scu_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    gint64 val;

    dom_tree_to_integer(node, &val);
    xaccAccountSetCommoditySCU(pdata->account, val);

    return true;
}

static bool
account_hidden_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    bool val;

    dom_tree_to_boolean(node, &val);
    xaccAccountSetHidden(pdata->account, val);

    return true;
}

static bool
account_placeholder_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    bool val;

    dom_tree_to_boolean(node, &val);
    xaccAccountSetPlaceholder(pdata->account, val);

    return true;
}

static bool
account_non_standard_scu_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;

    xaccAccountSetNonStdSCU(pdata->account, true);

    return true;
}

/* ============================================================== */
/* The following deprecated routines are here only to service
 * older XML files. */

static bool
deprecated_account_currency_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    gnc_commodity *ref;

    PWARN("Account %s: Obsolete xml tag 'act:currency' will not be preserved.",
          xaccAccountGetName( pdata->account ));
    ref = dom_tree_to_commodity_ref_no_engine(node, pdata->book);
    DxaccAccountSetCurrency(pdata->account, ref);

    return true;
}

static bool
deprecated_account_currency_scu_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    PWARN("Account %s: Obsolete xml tag 'act:currency-scu' will not be preserved.",
          xaccAccountGetName( pdata->account ));
    return true;
}

static bool
deprecated_account_security_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    gnc_commodity *ref, *orig = xaccAccountGetCommodity(pdata->account);

    PWARN("Account %s: Obsolete xml tag 'act:security' will not be preserved.",
          xaccAccountGetName( pdata->account ));
    /* If the account has both a commodity and a security elemet, and
       the commodity is a currecny, then the commodity is probably
       wrong. In that case we want to replace it with the
       security. jralls 2010-11-02 */
    if (!orig || gnc_commodity_is_currency( orig ) )
    {
        ref = dom_tree_to_commodity_ref_no_engine(node, pdata->book);
        xaccAccountSetCommodity(pdata->account, ref);
        /* If the SCU was set, it was probably wrong, so zero it out
           so that the SCU handler can fix it if there's a
           security-scu element. jralls 2010-11-02 */
        xaccAccountSetCommoditySCU(pdata->account, 0);
    }

    return true;
}

static bool
deprecated_account_security_scu_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    gint64 val;

    PWARN("Account %s: Obsolete xml tag 'act:security-scu' will not be preserved.",
          xaccAccountGetName( pdata->account ));
    if (!xaccAccountGetCommoditySCU(pdata->account))
    {
        dom_tree_to_integer(node, &val);
        xaccAccountSetCommoditySCU(pdata->account, val);
    }

    return true;
}

/* ============================================================== */

static bool
account_slots_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;

    return dom_tree_to_kvp_frame_given
           (node, xaccAccountGetSlots (pdata->account));
}

static bool
account_parent_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    Account *parent;
    GncGUID *gid;

    gid = dom_tree_to_guid(node);
//    g_return_val_if_fail(gid, FALSE);
    if(!gid) return false;

    parent = xaccAccountLookup(gid, pdata->book);
    if (!parent)
    {
        g_free (gid);
//        g_return_val_if_fail(parent, FALSE);
        if(!parent) return false;
    }

    gnc_account_append_child(parent, pdata->account);

    g_free (gid);

    return true;
}

static bool
account_code_handler(xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;

    return set_string(node, pdata->account, xaccAccountSetCode);
}

static bool
account_description_handler(xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;

    return set_string(node, pdata->account, xaccAccountSetDescription);
}

static bool
account_lots_handler(xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    xmlNodePtr mark;

    g_return_val_if_fail(node, FALSE);
    g_return_val_if_fail(node->xmlChildrenNode, false);

    for (mark = node->xmlChildrenNode; mark; mark = mark->next)
    {
        GNCLot *lot;

        if (g_strcmp0("text", (char*) mark->name) == 0)
            continue;

        lot = dom_tree_to_lot(mark, pdata->book);

        if (lot)
        {
            xaccAccountInsertLot (pdata->account, lot);
        }
        else
        {
            return false;
        }
    }
    return true;
}

static struct dom_tree_handler account_handlers_v2[] =
{
    { act_name_string, account_name_handler, 1, 0 },
    { act_id_string, account_id_handler, 1, 0 },
    { act_type_string, account_type_handler, 1, 0 },
    { act_commodity_string, account_commodity_handler, 0, 0 },
    { act_commodity_scu_string, account_commodity_scu_handler, 0, 0 },
    { act_non_standard_scu_string, account_non_standard_scu_handler, 0, 0 },
    { act_code_string, account_code_handler, 0, 0 },
    { act_description_string, account_description_handler, 0, 0},
    { act_slots_string, account_slots_handler, 0, 0 },
    { act_parent_string, account_parent_handler, 0, 0 },
    { act_lots_string, account_lots_handler, 0, 0 },
    { act_hidden_string, account_hidden_handler, 0, 0 },
    { act_placeholder_string, account_placeholder_handler, 0, 0 },

    /* These should not appear in  newer xml files; only in old
     * (circa gnucash-1.6) xml files. We maintain them for backward
     * compatibility. */
    { act_currency_string, deprecated_account_currency_handler, 0, 0 },
    { act_currency_scu_string, deprecated_account_currency_scu_handler, 0, 0 },
    { act_security_string, deprecated_account_security_handler, 0, 0 },
    { act_security_scu_string, deprecated_account_security_scu_handler, 0, 0 },
    { NULL, 0, 0, 0 }
};

static bool
gnc_account_end_handler(gpointer data_for_children,
                        GSList* data_from_children, GSList* sibling_data,
                        gpointer parent_data, gpointer global_data,
                        gpointer *result, const gchar *tag)
{
    Account *acc, *parent, *root;
    xmlNodePtr tree = (xmlNodePtr)data_for_children;
    gxpf_data *gdata = (gxpf_data*)global_data;
    QofBook *book = gdata->bookdata;
    int type;


    if (parent_data)
    {
        return true;
    }

    /* OK.  For some messed up reason this is getting called again with a
       NULL tag.  So we ignore those cases */
    if (!tag)
    {
        return true;
    }

    g_return_val_if_fail(tree, false);

    acc = dom_tree_to_account(tree, book);
    if (acc != NULL)
    {
        gdata->cb(tag, gdata->parsedata, acc);
        /*
         * Now return the account to the "edit" state.  At the end of reading
         * all the transactions, we will Commit.  This replaces #splits
         * rebalances with #accounts rebalances at the end.  A BIG win!
         */
        xaccAccountBeginEdit(acc);

        /* Backwards compatability.  If there's no parent, see if this
         * account is of type ROOT.  If not, find or create a ROOT
         * account and make that the parent. */
        parent = gnc_account_get_parent(acc);
        if (parent == NULL)
        {
            type = xaccAccountGetType(acc);
            if (type != ACCT_TYPE_ROOT)
            {
                root = gnc_book_get_root_account(book);
                if (root == NULL)
                {
                    root = gnc_account_create_root(book);
                }
                gnc_account_append_child(root, acc);
            }
        }
    }

    xmlFreeNode(tree);

    return acc != NULL;
}

Account*
dom_tree_to_account (xmlNodePtr node, QofBook *book)
{
    struct account_pdata act_pdata;
    Account *accToRet;
    bool successful;

    accToRet = xaccMallocAccount(book);
    xaccAccountBeginEdit(accToRet);

    act_pdata.account = accToRet;
    act_pdata.book = book;

    successful = dom_tree_generic_parse (node, account_handlers_v2,
                                         &act_pdata);
    if (successful)
    {
        xaccAccountCommitEdit (accToRet);
    }
    else
    {
        PERR ("failed to parse account tree");
        xaccAccountDestroy (accToRet);
        accToRet = NULL;
    }

    return accToRet;
}

sixtp*
gnc_account_sixtp_parser_create(void)
{
    return sixtp_dom_parser_new(gnc_account_end_handler, NULL, NULL);
}

/* ======================  END OF FILE ===================*/
