/********************************************************************
 * gnc-schedxactions-xml-v2.c -- xml routines for transactions      *
 * Copyright (C) 2001 Joshua Sled <jsled@asynchronous.org>          *
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
 *******************************************************************/

#include "config.h"

#include <glib.h>
#include <string.h>

#include "Group.h"
#include "SchedXactionP.h"

#include "gnc-xml-helper.h"
#include "gnc-engine-util.h"

#include "sixtp.h"
#include "sixtp-utils.h"
#include "sixtp-parsers.h"
#include "sixtp-utils.h"
#include "sixtp-dom-parsers.h"
#include "sixtp-dom-generators.h"

#include "gnc-xml.h"

#include "io-gncxml-v2.h"
#include "io-gncxml-gen.h"

#include "sixtp-dom-parsers.h"

static short module = MOD_SX;

/**
 * The XML output should look something like:
 * <gnc:count-data cd:type="schedXaction">XXX</gnc:count-data>
 * ...
 * <gnc:schedxaction version="1.0.0">
 *   <sx:id type="guid">...</sx:id>
 *   <sx:name>Rent</sx:name>
 *   <sx:autoCreate>y</sx:autoCreate>
 *   <sx:autoCreateNotify>n</sx:autoCreateNotify>
 *   <sx:advanceCreateDays>0</sx:advanceCreateDays>
 *   <sx:advanceRemindDays>5</sx:advanceRemindDays>
 *   <sx:instanceCount>100</sx:instanceCount>
 *   <sx:lastOccur>
 *     <gdate>2001-02-28</gdate>
 *   </sx:lastOccur>
 *   <sx:start>
 *     <gdate>2000-12-31</gdate>
 *   </sx:start>
 *   <!-- no end -->
 *   <sx:freq>
 *     <!-- freq spec tree -->
 *   </sx:freq>
 *   <sx:deferredInstance>
 *     <sx:last>2001-10-02</sx:last>
 *     [...]
 *   </sx:deferredInstance>
 *   [...]
 * </gnc:schedxaction>
 * <gnc:schedxaction version="1.0.0">
 *   <sx:id type="guid">...</sx:id>
 *   <sx:name>Loan 1</sx:name>
 *   <sx:manual-conf>f</sx:manual-conf>
 *   <sx:start>
 *      <gdate>2000-12-31</gdate>
 *   </sx:start>
 *   <sx:end type="date">
 *     <gdate>2004-03-20</gdate>
 *   </sx:end>
 *   <sx:instanceCount>100</sx:instanceCount>
 *   <sx:freq>
 *     <!-- freqspec tree -->
 *   </sx:freq>
 * </gnc:schedxaction>
 * <gnc:schedxaction version="1.0.0">
 *   <sx:id type="guid">...</sx:id>
 *   <sx:name>Loan 2</sx:name>
 *   <sx:manual-conf>f</sx:manual-conf>
 *   <sx:instanceCount>100</sx:instanceCount>
 *   <sx:start>
 *      <gdate>2000-12-31</gdate>
 *   </sx:start>
 *   <sx:end type="num_occur">
 *     <sx:num>42</sx:num>
 *   </sx:end>
 *   <sx:freq>
 *     <!-- freqspec tree -->
 *   </sx:freq>
 * </gnc:schedxaction>
 * 
 * et-cetera...
 * bleh.
 **/

/* 
 * All tags should be #defined here 
 */

#define SX_ID                   "sx:id"
#define SX_NAME                 "sx:name"
#define SX_AUTOCREATE           "sx:autoCreate"
#define SX_AUTOCREATE_NOTIFY    "sx:autoCreateNotify"
#define SX_ADVANCE_CREATE_DAYS  "sx:advanceCreateDays"
#define SX_ADVANCE_REMIND_DAYS  "sx:advanceRemindDays"
#define SX_INSTANCE_COUNT       "sx:instanceCount"
#define SX_START                "sx:start"
#define SX_LAST                 "sx:last"
#define SX_NUM_OCCUR            "sx:num-occur"
#define SX_REM_OCCUR            "sx:rem-occur"
#define SX_END                  "sx:end"
#define SX_TEMPL_ACCT           "sx:templ-acct" 
#define SX_FREQSPEC             "sx:freqspec"
#define SX_SLOTS                "sx:slots"
#define SX_DEFER_INSTANCE       "sx:deferredInstance"

/*
 * FIXME: These should be defined in a header somewhere
 */

#define GNC_ACCOUNT_TAG         "gnc:account"
#define GNC_TRANSACTION_TAG     "gnc:transaction"
#define GNC_SCHEDXACTION_TAG    "gnc:schedxaction"

const gchar *schedxaction_version_string = "1.0.0";

xmlNodePtr
gnc_schedXaction_dom_tree_create(SchedXaction *sx)
{
    xmlNodePtr	ret;
    xmlNodePtr	fsNode;
    GDate	*date;
    gint        instCount;
    const GUID        *templ_acc_guid;

    templ_acc_guid = xaccAccountGetGUID(sx->template_acct);

    /* FIXME: this should be the same as the def in io-gncxml-v2.c */
    ret = xmlNewNode( NULL, GNC_SCHEDXACTION_TAG );

    xmlSetProp( ret, "version", schedxaction_version_string );

    xmlAddChild( ret,
                 guid_to_dom_tree(SX_ID,
                                  xaccSchedXactionGetGUID(sx)) );

    xmlNewTextChild( ret, NULL, SX_NAME, xaccSchedXactionGetName(sx) );

    xmlNewTextChild( ret, NULL, SX_AUTOCREATE,
                     ( sx->autoCreateOption ? "y" : "n" ) );
    xmlNewTextChild( ret, NULL, SX_AUTOCREATE_NOTIFY,
                     ( sx->autoCreateNotify ? "y" : "n" ) );
    xmlAddChild(ret, int_to_dom_tree(SX_ADVANCE_CREATE_DAYS,
                                     sx->advanceCreateDays));
    xmlAddChild(ret, int_to_dom_tree(SX_ADVANCE_REMIND_DAYS,
                                     sx->advanceRemindDays));

    instCount = gnc_sx_get_instance_count( sx, NULL );
    xmlAddChild( ret, int_to_dom_tree( SX_INSTANCE_COUNT,
                                       instCount ) );

    xmlAddChild( ret,
                 gdate_to_dom_tree( SX_START,
                                    xaccSchedXactionGetStartDate(sx) ) );

    date = xaccSchedXactionGetLastOccurDate(sx);
    if ( g_date_valid( date ) ) {
            xmlAddChild( ret, gdate_to_dom_tree( SX_LAST, date ) );
    }

    if ( xaccSchedXactionHasOccurDef(sx) ) {

        xmlAddChild(ret, int_to_dom_tree( SX_NUM_OCCUR,
                                          xaccSchedXactionGetNumOccur(sx)));
        xmlAddChild(ret, int_to_dom_tree( SX_REM_OCCUR,
                                          xaccSchedXactionGetRemOccur(sx)));

    } else if ( xaccSchedXactionHasEndDate(sx) ) {
            xmlAddChild( ret,
                         gdate_to_dom_tree( SX_END,
                                            xaccSchedXactionGetEndDate(sx) ) );
    }

    /* output template account GUID */
    xmlAddChild( ret, 
		 guid_to_dom_tree(SX_TEMPL_ACCT,
				  templ_acc_guid));
				  
    /* output freq spec */
    fsNode = xmlNewNode(NULL, SX_FREQSPEC);
    xmlAddChild( fsNode,
                 gnc_freqSpec_dom_tree_create(
                         xaccSchedXactionGetFreqSpec(sx)) );
    xmlAddChild( ret, fsNode );

    /* Output deferred-instance list. */
    {
            xmlNodePtr instNode;
            temporalStateData *tsd;
            GList *l;

            for ( l = gnc_sx_get_defer_instances( sx ); l; l = l->next ) {
                    tsd = (temporalStateData*)l->data;

                    instNode = xmlNewNode( NULL, SX_DEFER_INSTANCE );
                    if ( g_date_valid( &tsd->last_date ) )
                    {
                      xmlAddChild( instNode, gdate_to_dom_tree( SX_LAST,
                                                                &tsd->last_date ) );
                    }
                    xmlAddChild( instNode, int_to_dom_tree( SX_REM_OCCUR,
                                                            tsd->num_occur_rem ) );
                    xmlAddChild( instNode, int_to_dom_tree( SX_INSTANCE_COUNT,
                                                            tsd->num_inst ) );
                    xmlAddChild( ret, instNode );
            }
    }
    
    /* output kvp_frame */
    {
        xmlNodePtr kvpnode =
                kvp_frame_to_dom_tree( SX_SLOTS,
                                       xaccSchedXactionGetSlots(sx) );
        if ( kvpnode )
        {
            xmlAddChild(ret, kvpnode);
        }
    }

    return ret;
}

struct sx_pdata
{
  SchedXaction *sx;
  GNCBook *book;
};

static
gboolean
sx_id_handler( xmlNodePtr node, gpointer sx_pdata )
{
    struct sx_pdata *pdata = sx_pdata;
    SchedXaction *sx = pdata->sx;
    GUID        *tmp = dom_tree_to_guid( node );

    g_return_val_if_fail( tmp, FALSE );
    xaccSchedXactionSetGUID(sx, *tmp);
    g_free( tmp );

    return TRUE;
}

static
gboolean
sx_name_handler( xmlNodePtr node, gpointer sx_pdata )
{
    struct sx_pdata *pdata = sx_pdata;
    SchedXaction *sx = pdata->sx;
    gchar *tmp = dom_tree_to_text( node );

    g_return_val_if_fail( tmp, FALSE );
    xaccSchedXactionSetName( sx, tmp );
    g_free( tmp );

    return TRUE;
}

static gboolean
sx_autoCreate_handler( xmlNodePtr node, gpointer sx_pdata )
{
    struct sx_pdata *pdata = sx_pdata;
    SchedXaction *sx = pdata->sx;
    gchar *tmp = dom_tree_to_text( node );

    sx->autoCreateOption = (safe_strcmp( tmp, "y" ) == 0 ? TRUE : FALSE );

    return TRUE;
}

static gboolean
sx_notify_handler( xmlNodePtr node, gpointer sx_pdata )
{
    struct sx_pdata *pdata = sx_pdata;
    SchedXaction *sx = pdata->sx;
    gchar *tmp = dom_tree_to_text( node );

    sx->autoCreateNotify = (safe_strcmp( tmp, "y" ) == 0 ? TRUE : FALSE );

    return TRUE;
}

static gboolean
sx_advCreate_handler( xmlNodePtr node, gpointer sx_pdata )
{
    struct sx_pdata *pdata = sx_pdata;
    SchedXaction *sx = pdata->sx;
    gint64 advCreate;

    if ( ! dom_tree_to_integer( node, &advCreate ) ) {
      return FALSE;
    }

    xaccSchedXactionSetAdvanceCreation( sx, advCreate );
    return TRUE;
}

static gboolean
sx_advRemind_handler( xmlNodePtr node, gpointer sx_pdata )
{
    struct sx_pdata *pdata = sx_pdata;
    SchedXaction *sx = pdata->sx;
    gint64 advRemind;

    if ( ! dom_tree_to_integer( node, &advRemind ) ) {
      return FALSE;
    }

    xaccSchedXactionSetAdvanceReminder( sx, advRemind );
    return TRUE;
}

static
gboolean
sx_set_date( xmlNodePtr node, SchedXaction *sx,
             void (*settor)( SchedXaction *sx, GDate *d ) )
{
    GDate *date;
    date = dom_tree_to_gdate( node );
    g_return_val_if_fail( date, FALSE );
    (*settor)( sx, date );
    g_date_free( date );

    return TRUE;
}

static
gboolean
sx_instcount_handler( xmlNodePtr node, gpointer sx_pdata )
{
  struct sx_pdata *pdata = sx_pdata;
  SchedXaction *sx = pdata->sx;
  gint64 instanceNum;

  if ( ! dom_tree_to_integer( node, &instanceNum ) ) {
    return FALSE;
  }

  gnc_sx_set_instance_count( sx, instanceNum );
  return TRUE;
}

static
gboolean
sx_start_handler( xmlNodePtr node, gpointer sx_pdata )
{
    struct sx_pdata *pdata = sx_pdata;
    SchedXaction *sx = pdata->sx;

    return sx_set_date( node, sx, xaccSchedXactionSetStartDate );
}

static
gboolean
sx_last_handler( xmlNodePtr node, gpointer sx_pdata )
{
    struct sx_pdata *pdata = sx_pdata;
    SchedXaction *sx = pdata->sx;

    return sx_set_date( node, sx, xaccSchedXactionSetLastOccurDate );
}

static
gboolean
sx_end_handler( xmlNodePtr node, gpointer sx_pdata )
{
    struct sx_pdata *pdata = sx_pdata;
    SchedXaction *sx = pdata->sx;

    return sx_set_date( node, sx, xaccSchedXactionSetEndDate );
}

static
gboolean
sx_freqspec_handler( xmlNodePtr node, gpointer sx_pdata )
{
    struct sx_pdata *pdata = sx_pdata;
    SchedXaction *sx = pdata->sx;
    FreqSpec *fs;

    g_return_val_if_fail( node, FALSE );

    fs = dom_tree_to_freqSpec( node, pdata->book );
    xaccSchedXactionSetFreqSpec( sx, fs );

    return TRUE;
}

static
gboolean
sx_defer_last_handler( xmlNodePtr node, gpointer gpTSD )
{
        GDate *gd;
        temporalStateData *tsd = (temporalStateData*)gpTSD;

        g_return_val_if_fail( node, FALSE );
        gd = dom_tree_to_gdate( node );
        g_return_val_if_fail( gd, FALSE );
        tsd->last_date = *gd;
        g_date_free( gd );
        return TRUE;
}

static
gboolean
sx_defer_rem_occur_handler( xmlNodePtr node, gpointer gpTSD )
{
        gint64 remOccur;
        temporalStateData *tsd = (temporalStateData*)gpTSD;
        g_return_val_if_fail( node, FALSE );

        if ( ! dom_tree_to_integer( node, &remOccur ) ) {
                return FALSE;
        }
        tsd->num_occur_rem = remOccur;
        return TRUE;
}

static
gboolean
sx_defer_inst_count_handler( xmlNodePtr node, gpointer gpTSD )
{
        gint64 instCount;
        temporalStateData *tsd = (temporalStateData*)gpTSD;
        g_return_val_if_fail( node, FALSE );

        if ( ! dom_tree_to_integer( node, &instCount ) ) {
                return FALSE;
        }
        tsd->num_inst = instCount;
        return TRUE;
}

struct dom_tree_handler sx_defer_dom_handlers[] = {
       /* tag name, handler, opt, ? */
        { SX_LAST,           sx_defer_last_handler,       1, 0 },
        { SX_REM_OCCUR,      sx_defer_rem_occur_handler,  1, 0 },
        { SX_INSTANCE_COUNT, sx_defer_inst_count_handler, 1, 0 },
        { NULL, NULL, 0, 0 }
};

static
gboolean
sx_defer_inst_handler( xmlNodePtr node, gpointer sx_pdata )
{
        struct sx_pdata *pdata = sx_pdata;
        SchedXaction *sx = pdata->sx;
        temporalStateData *tsd;

        g_return_val_if_fail( node, FALSE );

        tsd = g_new0( temporalStateData, 1 );
        g_assert( sx_defer_dom_handlers != NULL );
        if ( !dom_tree_generic_parse( node,
                                      sx_defer_dom_handlers,
                                      tsd ) ) {
                xmlElemDump(stdout, NULL, node);
                g_free( tsd );
                tsd = NULL;
                return FALSE;
        }

        /* We assume they were serialized in sorted order, here. */
        sx->deferredList = g_list_append( sx->deferredList, tsd );
        return TRUE;
}

static
gboolean
sx_numOccur_handler( xmlNodePtr node, gpointer sx_pdata )
{
    struct sx_pdata *pdata = sx_pdata;
    SchedXaction *sx = pdata->sx;
    gint64 numOccur;

    if ( ! dom_tree_to_integer( node, &numOccur ) ) {
        return FALSE;
    }

    xaccSchedXactionSetNumOccur( sx, numOccur );

    return TRUE;
}


static 
gboolean
sx_templ_acct_handler( xmlNodePtr node, gpointer sx_pdata)
{
  struct sx_pdata *pdata = sx_pdata;
  SchedXaction *sx = pdata->sx;
  GUID *templ_acct_guid = dom_tree_to_guid(node);

  if (!templ_acct_guid)
  {
    return FALSE;
  }

  sx->template_acct = xaccAccountLookup(templ_acct_guid, pdata->book);
  g_free(templ_acct_guid);

  return TRUE;
}

			
static
gboolean
sx_remOccur_handler( xmlNodePtr node, gpointer sx_pdata )
{
  struct sx_pdata *pdata = sx_pdata;
  SchedXaction *sx = pdata->sx;
  gint64        remOccur;

  if ( ! dom_tree_to_integer( node, &remOccur ) ) {
    return FALSE;
  }

  xaccSchedXactionSetRemOccur( sx, remOccur );

  return TRUE;
}

static
gboolean
sx_slots_handler( xmlNodePtr node, gpointer sx_pdata )
{
  struct sx_pdata *pdata = sx_pdata;
  SchedXaction *sx = pdata->sx;

  return dom_tree_to_kvp_frame_given( node, xaccSchedXactionGetSlots (sx) );
}

struct dom_tree_handler sx_dom_handlers[] = {
    { SX_ID,                  sx_id_handler,         1, 0 },
    { SX_NAME,                sx_name_handler,       1, 0 },
    { SX_AUTOCREATE,          sx_autoCreate_handler, 1, 0 },
    { SX_AUTOCREATE_NOTIFY,   sx_notify_handler,     1, 0 },
    { SX_ADVANCE_CREATE_DAYS, sx_advCreate_handler,  1, 0 },
    { SX_ADVANCE_REMIND_DAYS, sx_advRemind_handler,  1, 0 },
    { SX_INSTANCE_COUNT,      sx_instcount_handler,  0, 0 },
    { SX_START,               sx_start_handler,      1, 0 },
    { SX_LAST,                sx_last_handler,       0, 0 },
    { SX_NUM_OCCUR,           sx_numOccur_handler,   0, 0 },
    { SX_REM_OCCUR,           sx_remOccur_handler,   0, 0 },
    { SX_END,                 sx_end_handler,        0, 0 },
    { SX_TEMPL_ACCT,          sx_templ_acct_handler, 0, 0 },
    { SX_FREQSPEC,            sx_freqspec_handler,   1, 0 },
    { SX_DEFER_INSTANCE,      sx_defer_inst_handler, 0, 0 },
    { SX_SLOTS,               sx_slots_handler,      0, 0 },
    { NULL,                   NULL, 0, 0 }
};

static gboolean
gnc_schedXaction_end_handler(gpointer data_for_children,
                             GSList* data_from_children, GSList* sibling_data,
                             gpointer parent_data, gpointer global_data,
                             gpointer *result, const gchar *tag)
{
    SchedXaction *sx;
    gboolean     successful = FALSE;
    xmlNodePtr   tree = (xmlNodePtr)data_for_children;
    gxpf_data    *gdata = (gxpf_data*)global_data;
    struct sx_pdata sx_pdata;

    if ( parent_data ) {
        return TRUE;
    }

    if ( !tag ) {
        return TRUE;
    }

    g_return_val_if_fail( tree, FALSE );

    sx = xaccSchedXactionMalloc( gdata->bookdata );

    /* FIXME: this should be removed somewhere near 1.8 release time. */
    {
            /* This is the just-created template acct.  It can safely be
               removed, as we either will find or don't have a relevent
               template_acct. */
            xaccAccountBeginEdit (sx->template_acct);
            xaccAccountDestroy( sx->template_acct );
            sx->template_acct = NULL;
    }

    sx_pdata.sx = sx;
    sx_pdata.book = gdata->bookdata;

    g_assert( sx_dom_handlers != NULL );

    successful = dom_tree_generic_parse( tree, sx_dom_handlers, &sx_pdata );

    if ( successful ) 
    {
            gdata->cb( tag, gdata->parsedata, sx );
    } 
    else 
    {
            PERR ("failed to parse scheduled xaction");
            xmlElemDump( stdout, NULL, tree );
            xaccSchedXactionFree( sx );
    }

    /* FIXME: this should be removed somewhere near 1.8 release time. */
    if ( sx->template_acct == NULL )
    {
            AccountGroup *ag = NULL;
            char *id = NULL;
            Account *acct = NULL;
            sixtp_gdv2 *sixdata = gdata->parsedata;
            GNCBook *book;

            book = sixdata->book;

            /* We're dealing with a pre-200107<near-end-of-month> rgmerk
               change re: storing template accounts. */
            /* Fix: get account with name of our GUID from the template
               accounts group.  Make that our template_acct pointer. */
            id = guid_to_string( xaccSchedXactionGetGUID( sx ) );
            ag = gnc_book_get_template_group(book);
            if ( ag == NULL )
            {
                    PERR( "Error getting template account group "
                          "from being-parsed Book." );
                    xmlFreeNode( tree );
                    return FALSE;
            }
            acct = xaccGetAccountFromName( ag, id );
            if ( acct == NULL )
            {
                    PERR( "Error getting template account "
                          "with name \"%s\"", id );
                    xmlFreeNode( tree );
                    return FALSE;
            }
            DEBUG( "Got template account with name \"%s\" for "
                   "SX with GUID \"%s\"",
                   xaccAccountGetName( acct ), id );

            /* FIXME: free existing template account. 
	     *  HUH????? We only execute this if there isn't
	     * currently an existing template account, don't we?
	     * <rgmerk>
	     */

            sx->template_acct = acct;
    }

    xaccSchedXactionSetDirtyness(sx, FALSE);
    xmlFreeNode( tree );

    return successful;
}

sixtp*
gnc_schedXaction_sixtp_parser_create(void)
{
    return sixtp_dom_parser_new( gnc_schedXaction_end_handler, NULL, NULL );
}

static
gboolean
tt_act_handler( xmlNodePtr node, gpointer data )
{
        gnc_template_xaction_data *txd = data;
        Account *acc;
        gnc_commodity *com;

        acc = dom_tree_to_account(node, txd->book);

        if ( acc == NULL ) {
                return FALSE;
        } else {
                xaccAccountBeginEdit (acc);

                /* Check for the lack of a commodity [signifying that the
                   pre-7/11/2001-CIT-change SX template Account was parsed [but
                   incorrectly]. */
                if ( xaccAccountGetCommodity( acc ) == NULL ) {
                        /* FIXME: This should first look in the table of the
                           book, maybe? The right thing happens [WRT file
                           load/save] if we just _new all the time, but it
                           doesn't seem right. This whole block should go
                           away at some point, but the same concern still
                           applies for
                           SchedXaction.c:xaccSchedXactionInit... */
                        com = gnc_commodity_new( "template", "template",
                                                 "template", "template",
                                                 1 );
                        xaccAccountSetCommodity( acc, com );
                }

                txd->accts = g_list_append( txd->accts, acc );
        }

        return TRUE;
}

static
gboolean
tt_trn_handler( xmlNodePtr node, gpointer data )
{
        gnc_template_xaction_data *txd = data;
        Transaction        *trn;

        trn = dom_tree_to_transaction( node, txd->book );

        if ( trn == NULL ) {
                return FALSE;
        } else {
                txd->transactions = g_list_append( txd->transactions, trn );
        }

        return TRUE;
}

struct dom_tree_handler tt_dom_handlers[] = {
        { GNC_ACCOUNT_TAG,     tt_act_handler, 0, 0 },
        { GNC_TRANSACTION_TAG, tt_trn_handler, 0, 0 },
        { NULL, NULL, 0, 0 },
};

static gboolean
gnc_template_transaction_end_handler(gpointer data_for_children,
                                     GSList* data_from_children,
                                     GSList* sibling_data,
                                     gpointer parent_data,
                                     gpointer global_data,
                                     gpointer *result,
                                     const gchar *tag)
{
        gboolean   successful = FALSE;
        xmlNodePtr tree = data_for_children;
        gxpf_data  *gdata = global_data;
        GNCBook    *book = gdata->bookdata;
        GList      *n;
        gnc_template_xaction_data txd;

        txd.book = book;
        txd.accts = NULL;
        txd.transactions = NULL;

        /* the DOM tree will have an account tree [the template group
           and account] and a list of transactions [which will be
           members of the template account].

           we want to parse through the dom trees for each, placing
           the null-parent account in the book's template-group slot,
           the others under it, and the transactions as normal. */

        if ( parent_data ) {
                return TRUE;
        }

        if ( !tag ) {
                return TRUE;
        }

        g_return_val_if_fail( tree, FALSE );
        
        successful = dom_tree_generic_parse( tree, tt_dom_handlers, &txd );

        if ( successful ) 
        {
                gdata->cb( tag, gdata->parsedata, &txd );
        } 
        else 
        {
                PERR ("failed to parse template transaction");
                xmlElemDump( stdout, NULL, tree );
        }

        /* cleanup */
        for ( n = txd.accts; n; n = n->next ) {
                n->data = NULL;
        }
        for ( n = txd.transactions; n; n = n->next ) {
                n->data = NULL;
        }
        g_list_free( txd.accts );
        g_list_free( txd.transactions );

        xmlFreeNode( tree );

        return successful;
}

sixtp*
gnc_template_transaction_sixtp_parser_create( void )
{
        return sixtp_dom_parser_new( gnc_template_transaction_end_handler,
                                     NULL, NULL );
}
