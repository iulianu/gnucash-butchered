/********************************************************************\
 * gnc-account-xml-v2.c -- account xml i/o implementation           *
 *                                                                  *
 * Copyright (C) 2001 James LewisMoss <dres@debian.org>             *
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
#include "Group.h"

const gchar *account_version_string = "2.0.0";

/* ids */
#define gnc_account_string "gnc:account"
#define act_name_string "act:name"
#define act_id_string "act:id"
#define act_type_string "act:type"
#define act_commodity_string "act:commodity"
#define act_commodity_scu_string "act:commodity-scu"
#define act_currency_string "act:currency"
#define act_currency_scu_string "act:currency-scu"
#define act_code_string "act:code"
#define act_description_string "act:description"
#define act_security_string "act:security"
#define act_security_scu_string "act:security-scu"
#define act_slots_string "act:slots"
#define act_parent_string "act:parent"

xmlNodePtr
gnc_account_dom_tree_create(Account *act)
{
    xmlNodePtr ret;

    ret = xmlNewNode(NULL, gnc_account_string);
    xmlSetProp(ret, "version", account_version_string);

    xmlAddChild(ret, text_to_dom_tree(act_name_string,
                                      xaccAccountGetName(act)));
    
    xmlAddChild(ret, guid_to_dom_tree(act_id_string, xaccAccountGetGUID(act)));

    xmlAddChild(ret, text_to_dom_tree(
                    act_type_string,
                    xaccAccountTypeEnumAsString(xaccAccountGetType(act))));

    xmlAddChild(ret, commodity_ref_to_dom_tree(act_commodity_string,
                                               xaccAccountGetCommodity(act)));

    xmlAddChild(ret, int_to_dom_tree(act_commodity_scu_string,
                                     xaccAccountGetCommoditySCU(act)));
    
    if(xaccAccountGetCode(act) &&
        strlen(xaccAccountGetCode(act)) > 0)
    {
        xmlAddChild(ret, text_to_dom_tree(act_code_string,
                                          xaccAccountGetCode(act)));
    }

    if(xaccAccountGetDescription(act) &&
       strlen(xaccAccountGetDescription(act)) > 0)
    {
        xmlAddChild(ret, text_to_dom_tree(act_description_string,
                                          xaccAccountGetDescription(act)));
    }
       
    if(xaccAccountGetSlots(act))
    {
        xmlNodePtr kvpnode = kvp_frame_to_dom_tree(act_slots_string,
                                                   xaccAccountGetSlots(act));
        if(kvpnode)
        {
            xmlAddChild(ret, kvpnode);
        }
    }

    if(xaccAccountGetParentAccount(act))
    {
        xmlAddChild(ret, guid_to_dom_tree(
                     act_parent_string,
                     xaccAccountGetGUID(xaccAccountGetParentAccount(act))));
    }
    
    return ret;
}

/***********************************************************************/

struct account_pdata
{
  Account *account;
  GNCSession *session;
};

static gboolean
set_string(xmlNodePtr node, Account* act,
           void (*func)(Account *act, const gchar *txt))
{
    gchar* txt = dom_tree_to_text(node);
    g_return_val_if_fail(txt, FALSE);
    
    func(act, txt);

    g_free(txt);
    
    return TRUE;
}

static gboolean
account_name_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;

    return set_string(node, pdata->account, xaccAccountSetName);
}

static gboolean
account_id_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    GUID *guid;

    guid = dom_tree_to_guid(node);
    xaccAccountSetGUID(pdata->account, guid);

    g_free(guid);
    
    return TRUE;
}

static gboolean
account_type_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    int type;
    char *string;

    string = xmlNodeGetContent (node->xmlChildrenNode);
    xaccAccountStringToType(string, &type);
    xmlFree (string);

    xaccAccountSetType(pdata->account, type);

    return TRUE;
}

static gboolean
account_commodity_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    gnc_commodity *ref;

    ref = dom_tree_to_commodity_ref_no_engine(node);
    xaccAccountSetCommodity(pdata->account, ref);

    return TRUE;
}

static gboolean
account_commodity_scu_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    gint64 val;

    dom_tree_to_integer(node, &val);
    xaccAccountSetCommoditySCU(pdata->account, val);

    return TRUE;
}

static gboolean
account_currency_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    gnc_commodity *ref;

    ref = dom_tree_to_commodity_ref_no_engine(node);
    DxaccAccountSetCurrency(pdata->account, ref, pdata->session);

    return TRUE;
}

static gboolean
account_currency_scu_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    gint64 val;

    dom_tree_to_integer(node, &val);
    DxaccAccountSetCurrencySCU(pdata->account, val);

    return TRUE;
}

static gboolean
account_security_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    gnc_commodity *ref;

    ref = dom_tree_to_commodity_ref_no_engine(node);
    DxaccAccountSetSecurity(pdata->account, ref, pdata->session);

    return TRUE;
}

static gboolean
account_security_scu_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    gint64 val;

    dom_tree_to_integer(node, &val);
    xaccAccountSetCommoditySCU(pdata->account, val);

    return TRUE;
}

static gboolean
account_slots_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    gboolean success;

    success = dom_tree_to_kvp_frame_given
      (node, xaccAccountGetSlots (pdata->account));

    g_return_val_if_fail(success, FALSE);
    
    return TRUE;
}

static gboolean
account_parent_handler (xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;
    Account *parent;
    GUID *gid;

    gid = dom_tree_to_guid(node);
    g_return_val_if_fail(gid, FALSE);

    parent = xaccAccountLookup(gid);
    if (!parent)
    {
      g_free (gid);
      g_return_val_if_fail(parent, FALSE);
    }

    xaccAccountInsertSubAccount(parent, pdata->account);

    g_free (gid);

    return TRUE;
}

static gboolean
account_code_handler(xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;

    return set_string(node, pdata->account, xaccAccountSetCode);
}

static gboolean
account_description_handler(xmlNodePtr node, gpointer act_pdata)
{
    struct account_pdata *pdata = act_pdata;

    return set_string(node, pdata->account, xaccAccountSetDescription);
}

static struct dom_tree_handler account_handlers_v2[] = {
    { act_name_string, account_name_handler, 1, 0 },
    { act_id_string, account_id_handler, 1, 0 },
    { act_type_string, account_type_handler, 1, 0 },
    { act_commodity_string, account_commodity_handler, 0, 0 },
    { act_commodity_scu_string, account_commodity_scu_handler, 0, 0 },
    { act_currency_string, account_currency_handler, 0, 0 },
    { act_currency_scu_string, account_currency_scu_handler, 0, 0 },
    { act_code_string, account_code_handler, 0, 0 },
    { act_description_string, account_description_handler, 0, 0},
    { act_security_string, account_security_handler, 0, 0 },
    { act_security_scu_string, account_security_scu_handler, 0, 0 },
    { act_slots_string, account_slots_handler, 0, 0 },
    { act_parent_string, account_parent_handler, 0, 0 },
    { NULL, 0, 0, 0 }
};

static gboolean
gnc_account_end_handler(gpointer data_for_children,
                        GSList* data_from_children, GSList* sibling_data,
                        gpointer parent_data, gpointer global_data,
                        gpointer *result, const gchar *tag)
{
    int successful;
    Account *acc;
    xmlNodePtr achild;
    xmlNodePtr tree = (xmlNodePtr)data_for_children;
    gxpf_data *gdata = (gxpf_data*)global_data;
    GNCSession *session = gdata->sessiondata;

    successful = TRUE;

    if(parent_data)
    {
        return TRUE;
    }

    /* OK.  For some messed up reason this is getting called again with a
       NULL tag.  So we ignore those cases */
    if(!tag)
    {
        return TRUE;
    }

    g_return_val_if_fail(tree, FALSE);

    acc = dom_tree_to_account(tree, session);
    if(acc != NULL)
    {
        gdata->cb(tag, gdata->parsedata, acc);
        /*
         * Now return the account to the "edit" state.  At the end of reading
         * all the transactions, we will Commit.  This replaces #splits
         * rebalances with #accounts rebalances at the end.  A BIG win!
         */
        xaccAccountBeginEdit(acc);
    }

    xmlFreeNode(tree);

    return acc != NULL;
}

Account*
dom_tree_to_account (xmlNodePtr node, GNCSession * session)
{
    struct account_pdata act_pdata;
    Account *accToRet;
    gboolean successful;

    accToRet = xaccMallocAccount();
    xaccAccountBeginEdit(accToRet);

    act_pdata.account = accToRet;
    act_pdata.session = session;

    successful = dom_tree_generic_parse (node, account_handlers_v2,
                                         &act_pdata);
    xaccAccountCommitEdit (accToRet);

    if (!successful)
    {
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
