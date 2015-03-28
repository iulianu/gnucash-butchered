/********************************************************************
 * gnc-commodity.cpp  api for tradable commodities (incl. currency) *
 * Copyright (C) 2000 Bill Gribble                                  *
 * Copyright (C) 2001,2003 Linas Vepstas <linas@linas.org>          *
 * Copyright (c) 2006 David Hampton <hampton@employees.org>         *
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
 *******************************************************************/

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

#include "gnc-commodity.h"
#include "gnc-main.h"

static QofLogModule log_module = GNC_MOD_COMMODITY;

/* Parts per unit is nominal, i.e. number of 'partname' units in
 * a 'unitname' unit.  fraction is transactional, i.e. how many
 * of the smallest-transactional-units of the currency are there
 * in a 'unitname' unit. */


struct CommodityPrivate
{
    gnc_commodity_namespace *comm_namespace;

    char    * fullname;
    char    * mnemonic;
    char    * printname;
    char    * cusip;          /* CUSIP or other identifying code */
    int       fraction;
    char    * unique_name;

    bool  quote_flag;	    /* user wants price quotes */
    gnc_quote_source * quote_source;   /* current/old source of quotes */
    char    * quote_tz;

    /* the number of accounts using this commodity - this field is not
     * persisted */
    int       usage_count;
    
    CommodityPrivate()
    {
        comm_namespace = NULL;
        fullname = NULL;
        mnemonic = NULL;
        printname = NULL;
        cusip = NULL;
        fraction = 0;
        unique_name = NULL;
        quote_flag = false;
        quote_source = NULL;
        quote_tz = NULL;
        usage_count = 0;
    }
};


#define GET_PRIVATE(o) \
    (o->priv)

static void commodity_free(gnc_commodity * cm);

class gnc_commodity_table
{
public:
    GHashTable * ns_table;
    std::list<gnc_commodity_namespace*> ns_list;
};

struct gnc_new_iso_code
{
    const char *old_code;
    const char *new_code;
} gnc_new_iso_codes[] =
{
    {"RUR", "RUB"}, /* Russian Ruble: RUR through 1997-12, RUB from 1998-01 onwards; see bug #393185 */
    {"PLZ", "PLN"}, /* Polish Zloty */
    {"UAG", "UAH"}, /* Ukraine Hryvnia */
    {"NIS", "ILS"}, /* New Israeli Shekel: The informal abbreviation may be "NIS", but
		     its iso-4217 is clearly ILS and only this! Incorrectly changed
		     due to bug#152755 (Nov 2004) and changed back again by bug#492417
		     (Oct 2008). */
    {"MXP", "MXN"}, /* Mexican (Nuevo) Peso */
    {"TRL", "TRY"}, /* New Turkish Lira: changed 2005 */

    /* Only add currencies to this table when the old currency no longer
     * exists in the file iso-4217-currencies.scm */
};
#define GNC_NEW_ISO_CODES \
        (sizeof(gnc_new_iso_codes) / sizeof(struct gnc_new_iso_code))

static bool fq_is_installed = FALSE;

struct gnc_quote_source_s
{
    bool supported;
    QuoteSourceType type;
    gint index;
    char *user_name;		/* User friendly name */
    char *old_internal_name;	/* Name used internally (deprecated) */
    char *internal_name;		/* Name used internally and by finance::quote. */
};

static gnc_quote_source currency_quote_source =
{ TRUE, 0, 0, "Currency", "CURRENCY", "currency" };

static gnc_quote_source single_quote_sources[] =
{
    { FALSE, 0, 0, "Amsterdam Euronext eXchange, NL", "AEX", "aex" },
    { FALSE, 0, 0, "AEX Futures (now in AEX)", "AEX_FUTURES", "aex_futures" },
    { FALSE, 0, 0, "AEX Options (now in AEX)", "AEX_OPTIONS", "aex_options" },
    { FALSE, 0, 0, "American International Assurance, HK", "AIAHK", "aiahk" },
    { FALSE, 0, 0, "Association  of  Mutual  Funds  in  India", "AMFIINDIA", "amfiindia" },
    { FALSE, 0, 0, "Athens Stock Exchange, GR", "ASEGR", "asegr" },
    { FALSE, 0, 0, "Australian Stock Exchange, AU", "ASX", "asx" },
    { FALSE, 0, 0, "BMO NesbittBurns, CA", "BMONESBITTBURNS", "bmonesbittburns" },
    { FALSE, 0, 0, "BUX/Magyar Tökepiac, HU", "BUX", "bux" },
    { FALSE, 0, 0, "Cominvest, ex-Adig, DE", "COMINVEST", "cominvest" },
    { FALSE, 0, 0, "Deka Investments, DE", "DEKA", "deka" },
    { FALSE, 0, 0, "DWS, DE", "DWS", "dwsfunds" },
    { FALSE, 0, 0, "Fidelity Direct", "FIDELITY_DIRECT", "fidelity_direct" },
    { FALSE, 0, 0, "Finance Canada", "FINANCECANADA", "financecanada" },
    { FALSE, 0, 0, "Finanzpartner, DE", "FINANZPARTNER", "finanzpartner" },
    { FALSE, 0, 0, "First Trust Portfolios, US", "FTPORTFOLIOS_DIRECT", "ftportfolios_direct" },
    { FALSE, 0, 0, "Fund Library, CA", "FUNDLIBRARY", "fundlibrary" },
    { FALSE, 0, 0, "GoldMoney spot rates, JE", "GOLDMONEY", "goldmoney" },
    { FALSE, 0, 0, "HElsinki stock eXchange, FI", "HEX", "hex" },
    { FALSE, 0, 0, "Man Investments, AU", "maninv", "maninv" },
    { FALSE, 0, 0, "Morningstar, SE", "MORNINGSTAR", "morningstar" },
    { FALSE, 0, 0, "Motley Fool, US", "FOOL", "fool" },
    { FALSE, 0, 0, "New Zealand stock eXchange, NZ", "NZX", "nzx" },
    { FALSE, 0, 0, "Paris Stock Exchange/Boursorama, FR", "BOURSO", "bourso" },
    { FALSE, 0, 0, "Paris Stock Exchange/LeRevenu, FR", "LEREVENU", "lerevenu" },
    { FALSE, 0, 0, "Platinum Asset Management, AU", "PLATINUM", "platinum" },
    { FALSE, 0, 0, "Skandinaviska Enskilda Banken, SE", "SEB_FUNDS", "seb_funds" },
    { FALSE, 0, 0, "Sharenet, ZA", "ZA", "za" },
    { FALSE, 0, 0, "StockHouse Canada", "STOCKHOUSE_FUND", "stockhousecanada_fund" },
    { FALSE, 0, 0, "TD Waterhouse Canada", "TDWATERHOUSE", "tdwaterhouse" },
    { FALSE, 0, 0, "TD Efunds, CA", "TDEFUNDS", "tdefunds" },
    { FALSE, 0, 0, "TIAA-CREF, US", "TIAACREF", "tiaacref" },
    { FALSE, 0, 0, "Toronto Stock eXchange, CA", "TSX", "tsx" },
    { FALSE, 0, 0, "T. Rowe Price, US", "TRPRICE_DIRECT", "troweprice_direct" },
    { FALSE, 0, 0, "Trustnet, GB", "TRUSTNET", "trustnet" },
    { FALSE, 0, 0, "Union Investment, DE", "UNIONFUNDS", "unionfunds" },
    { FALSE, 0, 0, "US Treasury Bonds", "usfedbonds", "usfedbonds" },
    { FALSE, 0, 0, "US Govt. Thrift Savings Plan", "TSP", "tsp" },
    { FALSE, 0, 0, "Vanguard", "VANGUARD", "vanguard" }, /* No module seen in F::Q 1.17. */
    { FALSE, 0, 0, "VWD, DE (unmaintained)", "VWD", "vwd" },
    { FALSE, 0, 0, "Yahoo USA", "YAHOO", "yahoo" },
    { FALSE, 0, 0, "Yahoo Asia", "YAHOO_ASIA", "yahoo_asia" },
    { FALSE, 0, 0, "Yahoo Australia", "YAHOO_AUSTRALIA", "yahoo_australia" },
    { FALSE, 0, 0, "Yahoo Brasil", "YAHOO_BRASIL", "yahoo_brasil" },
    { FALSE, 0, 0, "Yahoo Europe", "YAHOO_EUROPE", "yahoo_europe" },
    { FALSE, 0, 0, "Yahoo New Zealand", "YAHOO_NZ", "yahoo_nz" },
    { FALSE, 0, 0, "Zuerich Investments (outdated)", "ZIFUNDS", "zifunds" }, /* Removed from F::Q 1.11. */
};
static gnc_quote_source multiple_quote_sources[] =
{
    { FALSE, 0, 0, "Asia (Yahoo, ...)", "ASIA", "asia" },
    { FALSE, 0, 0, "Australia (ASX, Yahoo, ...)", "AUSTRALIA", "australia" },
    { FALSE, 0, 0, "Brasil (Yahoo, ...)", "BRASIL", "brasil" },
    { FALSE, 0, 0, "Canada (Yahoo, ...)", "CANADA", "canada" },
    { FALSE, 0, 0, "Canada Mutual (Fund Library, ...)", "CANADAMUTUAL", "canadamutual" },
    { FALSE, 0, 0, "Dutch (AEX, ...)", "DUTCH", "dutch" },
    { FALSE, 0, 0, "Europe (Yahoo, ...)", "EUROPE", "europe" },
    { FALSE, 0, 0, "Greece (ASE, ...)", "GREECE", "greece" },
    { FALSE, 0, 0, "India Mutual (AMFI, ...)", "INDIAMUTUAL", "indiamutual" },
    { FALSE, 0, 0, "Fidelity (Fidelity, ...)", "FIDELITY", "fidelity" },
    { FALSE, 0, 0, "Finland (HEX, ...)", "FINLAND", "finland" },
    { FALSE, 0, 0, "First Trust (First Trust, ...)", "FTPORTFOLIOS", "ftportfolios" },
    { FALSE, 0, 0, "France (Boursorama, ...)", "FRANCE", "france" },
    { FALSE, 0, 0, "Nasdaq (Yahoo, ...)", "NASDAQ", "nasdaq" },
    { FALSE, 0, 0, "New Zealand (Yahoo, ...)", "NZ", "nz" },
    { FALSE, 0, 0, "NYSE (Yahoo, ...)", "NYSE", "nyse" },
    /*    { FALSE, 0, 0, "South Africa (Sharenet, ...)", "ZA", "za" }, */
    { FALSE, 0, 0, "T. Rowe Price", "TRPRICE", "troweprice" },
    { FALSE, 0, 0, "U.K. Unit Trusts", "UKUNITTRUSTS", "uk_unit_trusts" },
    { FALSE, 0, 0, "USA (Yahoo, Fool ...)", "USA", "usa" },
};

static const int num_single_quote_sources =
    sizeof(single_quote_sources) / sizeof(gnc_quote_source);
static const int num_multiple_quote_sources =
    sizeof(multiple_quote_sources) / sizeof(gnc_quote_source);
static GList *new_quote_sources = NULL;


/********************************************************************
 * gnc_quote_source_fq_installed
 *
 * This function indicates whether or not the Finance::Quote module
 * is installed on a users computer.
 ********************************************************************/
bool
gnc_quote_source_fq_installed (void)
{
    return fq_is_installed;
}

/********************************************************************
 * gnc_quote_source_num_entries
 *
 * Return the number of entries for a given type of price source.
 ********************************************************************/
gint gnc_quote_source_num_entries(QuoteSourceType type)
{
    if  (type == SOURCE_CURRENCY)
        return 1;

    if  (type == SOURCE_SINGLE)
        return num_single_quote_sources;

    if (type == SOURCE_MULTI)
        return num_multiple_quote_sources;

    return g_list_length(new_quote_sources);
}

/********************************************************************
 * gnc_quote_source_init_tables
 *
 * Update the type/index values for prices sources.
 ********************************************************************/
static void
gnc_quote_source_init_tables (void)
{
    gint i;

    for (i = 0; i < num_single_quote_sources; i++)
    {
        single_quote_sources[i].type = SOURCE_SINGLE;
        single_quote_sources[i].index = i;
    }

    for (i = 0; i < num_multiple_quote_sources; i++)
    {
        multiple_quote_sources[i].type = SOURCE_MULTI;
        multiple_quote_sources[i].index = i;
    }

    currency_quote_source.type = SOURCE_CURRENCY;
    currency_quote_source.index = 0;
}


/********************************************************************
 * gnc_quote_source_add_new
 *
 * Add a new price source. Called when unknown source names are found
 * either in the F::Q installation (a newly available source) or in
 * the user's data file (a source that has vanished but needs to be
 * tracked.)
 ********************************************************************/
gnc_quote_source *
gnc_quote_source_add_new (const char *source_name, bool supported)
{
    gnc_quote_source *new_source;

    DEBUG("Creating new source %s", (source_name == NULL ? "(null)" : source_name));
    new_source = malloc(sizeof(gnc_quote_source));
    new_source->supported = supported;
    new_source->type = SOURCE_UNKNOWN;
    new_source->index = g_list_length(new_quote_sources);

    /* This name can be changed if/when support for this price source is
     * integrated into gnucash. */
    new_source->user_name = g_strdup(source_name);

    /* This name is permanent and must be kept the same if/when support
     * for this price source is integrated into gnucash (i.e. for a
     * nice user name). */
    new_source->old_internal_name = g_strdup(source_name);
    new_source->internal_name = g_strdup(source_name);
    new_quote_sources = g_list_append(new_quote_sources, new_source);
    return new_source;
}

/********************************************************************
 * gnc_quote_source_lookup_by_xxx
 *
 * Lookup a price source data structure based upon various criteria.
 ********************************************************************/
gnc_quote_source *
gnc_quote_source_lookup_by_ti (QuoteSourceType type, gint index)
{
    gnc_quote_source *source;
    GList *node;

    ENTER("type/index is %d/%d", type, index);
    switch (type)
    {
    case SOURCE_CURRENCY:
        LEAVE("found %s", currency_quote_source.user_name);
        return &currency_quote_source;
        break;

    case SOURCE_SINGLE:
        if (index < num_single_quote_sources)
        {
            LEAVE("found %s", single_quote_sources[index].user_name);
            return &single_quote_sources[index];
        }
        break;

    case SOURCE_MULTI:
        if (index < num_multiple_quote_sources)
        {
            LEAVE("found %s", multiple_quote_sources[index].user_name);
            return &multiple_quote_sources[index];
        }
        break;

    case SOURCE_UNKNOWN:
    default:
        node = g_list_nth(new_quote_sources, index);
        if (node)
        {
            source = node->data;
            LEAVE("found %s", source->user_name);
            return source;
        }
        break;
    }

    LEAVE("not found");
    return NULL;
}

gnc_quote_source *
gnc_quote_source_lookup_by_internal(const char * name)
{
    gnc_quote_source *source;
    GList *node;
    gint i;

    if ((name == NULL) || (g_strcmp0(name, "") == 0))
    {
        return NULL;
    }

    if (g_strcmp0(name, currency_quote_source.internal_name) == 0)
        return &currency_quote_source;
    if (g_strcmp0(name, currency_quote_source.old_internal_name) == 0)
        return &currency_quote_source;

    for (i = 0; i < num_single_quote_sources; i++)
    {
        if (g_strcmp0(name, single_quote_sources[i].internal_name) == 0)
            return &single_quote_sources[i];
        if (g_strcmp0(name, single_quote_sources[i].old_internal_name) == 0)
            return &single_quote_sources[i];
    }

    for (i = 0; i < num_multiple_quote_sources; i++)
    {
        if (g_strcmp0(name, multiple_quote_sources[i].internal_name) == 0)
            return &multiple_quote_sources[i];
        if (g_strcmp0(name, multiple_quote_sources[i].old_internal_name) == 0)
            return &multiple_quote_sources[i];
    }

    for (i = 0, node = new_quote_sources; node; node = node->next, i++)
    {
        source = node->data;
        if (g_strcmp0(name, source->internal_name) == 0)
            return source;
        if (g_strcmp0(name, source->old_internal_name) == 0)
            return source;
    }

    DEBUG("gnc_quote_source_lookup_by_internal: Unknown source %s", name);
    return NULL;
}

/********************************************************************
 * gnc_quote_source_get_xxx
 *
 * Accessor functions - get functions only. There are no set functions.
 ********************************************************************/
QuoteSourceType
gnc_quote_source_get_type (const gnc_quote_source *source)
{
    ENTER("%p", source);
    if (!source)
    {
        LEAVE("bad source");
        return SOURCE_SINGLE;
    }

    LEAVE("type is %d", source->type);
    return source->type;
}

gint
gnc_quote_source_get_index (const gnc_quote_source *source)
{
    ENTER("%p", source);
    if (!source)
    {
        LEAVE("bad source");
        return 0;
    }

    LEAVE("index is %d", source->index);
    return source->index;
}

bool
gnc_quote_source_get_supported (const gnc_quote_source *source)
{
    ENTER("%p", source);
    if (!source)
    {
        LEAVE("bad source");
        return FALSE;
    }

    LEAVE("%ssupported", source && source->supported ? "" : "not ");
    return source->supported;
}

const char *
gnc_quote_source_get_user_name (const gnc_quote_source *source)
{
    ENTER("%p", source);
    if (!source)
    {
        LEAVE("bad source");
        return NULL;
    }
    LEAVE("user name %s", source->user_name);
    return source->user_name;
}

const char *
gnc_quote_source_get_internal_name (const gnc_quote_source *source)
{
    ENTER("%p", source);
    if (!source)
    {
        LEAVE("bad source");
        return NULL;
    }
    LEAVE("internal name %s", source->internal_name);
    return source->internal_name;
}

/********************************************************************
 * gnc_quote_source_set_fq_installed
 *
 * Update gnucash internal tables on what Finance::Quote sources are
 * installed.
 ********************************************************************/
void
gnc_quote_source_set_fq_installed (const GList *sources_list)
{
    gnc_quote_source *source;
    char *source_name;
    const GList *node;

    ENTER(" ");
    fq_is_installed = TRUE;

    if (!sources_list)
        return;

    for (node = sources_list; node; node = node->next)
    {
        source_name = node->data;

        source = gnc_quote_source_lookup_by_internal(source_name);
        if (source != NULL)
        {
            DEBUG("Found source %s: %s", source_name, source->user_name);
            source->supported = TRUE;
            continue;
        }

        gnc_quote_source_add_new(source_name, TRUE);
    }
    LEAVE(" ");
}

/********************************************************************
 * QoF Helpers
 ********************************************************************/

void
gnc_commodity_begin_edit (gnc_commodity *cm)
{
    qof_begin_edit(cm);
}

static void commit_err (QofInstance *inst, QofBackendError errcode)
{
    PERR ("Failed to commit: %d", errcode);
    gnc_engine_signal_commit_error( errcode );
}

static void noop (QofInstance *inst) {}

static void
comm_free(QofInstance* inst)
{
    commodity_free( (gnc_commodity*)(inst) );
}

void
gnc_commodity_commit_edit (gnc_commodity *cm)
{
    if (!qof_commit_edit (QOF_INSTANCE(cm))) return;
    qof_commit_edit_part2 (cm, commit_err, noop, comm_free);
}

/********************************************************************
 * gnc_commodity_new
 ********************************************************************/

static void
mark_commodity_dirty (gnc_commodity *cm)
{
    qof_instance_set_dirty(cm);
    qof_event_gen (cm, QOF_EVENT_MODIFY, NULL);
}

static void
reset_printname(CommodityPrivate *priv)
{
    g_free(priv->printname);
    priv->printname = g_strdup_printf("%s (%s)",
                                      priv->mnemonic ? priv->mnemonic : "",
                                      priv->fullname ? priv->fullname : "");
}

static void
reset_unique_name(CommodityPrivate *priv)
{
    gnc_commodity_namespace *ns;

    g_free(priv->unique_name);
    ns = priv->comm_namespace;
    priv->unique_name = g_strdup_printf("%s::%s",
                                        ns ? ns->name : "",
                                        priv->mnemonic ? priv->mnemonic : "");
}

gnc_commodity::gnc_commodity()
{
    priv = new CommodityPrivate;
    priv->comm_namespace = NULL;
    priv->fullname = CACHE_INSERT("");
    priv->mnemonic = CACHE_INSERT("");
    priv->cusip = CACHE_INSERT("");
    priv->fraction = 10000;
    priv->quote_flag = 0;
    priv->quote_source = NULL;
    priv->quote_tz = CACHE_INSERT("");

    reset_printname(priv);
    reset_unique_name(priv);
}

gnc_commodity::~gnc_commodity()
{
    delete priv;
}

gnc_commodity *
gnc_commodity_new(QofBook *book, const char * fullname,
                  const char * comm_namespace, const char * mnemonic,
                  const char * cusip, int fraction)
{
    gnc_commodity * retval = new gnc_commodity; //g_object_new(GNC_TYPE_COMMODITY, NULL);

    qof_instance_init_data (retval, GNC_ID_COMMODITY, book);
    gnc_commodity_begin_edit(retval);

    if ( comm_namespace != NULL )
    {
        gnc_commodity_set_namespace(retval, comm_namespace);
        if (gnc_commodity_namespace_is_iso(comm_namespace))
        {
            gnc_commodity_set_quote_source(retval,
                                           gnc_quote_source_lookup_by_internal("currency") );
        }
    }
    gnc_commodity_set_fullname(retval, fullname);
    gnc_commodity_set_mnemonic(retval, mnemonic);
    gnc_commodity_set_cusip(retval, cusip);
    gnc_commodity_set_fraction(retval, fraction);
    gnc_commodity_commit_edit(retval);

    qof_event_gen (retval, QOF_EVENT_CREATE, NULL);

    return retval;
}


/********************************************************************
 * gnc_commodity_destroy
 ********************************************************************/

static void
commodity_free(gnc_commodity * cm)
{
    QofBook *book;
    gnc_commodity_table *table;
    CommodityPrivate* priv;

    if (!cm) return;

    book = qof_instance_get_book(cm);
    table = gnc_commodity_table_get_table(book);
    gnc_commodity_table_remove(table, cm);
    priv = GET_PRIVATE(cm);

    qof_event_gen (cm, QOF_EVENT_DESTROY, NULL);

    /* Set at creation */
    CACHE_REMOVE (priv->fullname);
    CACHE_REMOVE (priv->cusip);
    CACHE_REMOVE (priv->mnemonic);
    CACHE_REMOVE (priv->quote_tz);
    priv->comm_namespace = NULL;

    /* Set through accessor functions */
    priv->quote_source = NULL;

    /* Automatically generated */
    g_free(priv->printname);
    priv->printname = NULL;

    g_free(priv->unique_name);
    priv->unique_name = NULL;

#ifdef ACCOUNTS_CLEANED_UP
    /* Account objects are not actually cleaned up when a book is closed (in fact
     * a memory leak), but commodities are, so in currently this warning gets hit
     * quite frequently.  Disable the check until cleaning up of accounts objects
     * on close is implemented.  */
    if (priv->usage_count != 0)
    {
        PWARN("Destroying commodity (%p) with non-zero usage_count (%d).", cm,
              priv->usage_count);
    }
#endif

    /* qof_instance_release (&cm->inst); */
//    g_object_unref(cm);
    delete cm;
}

void
gnc_commodity_destroy(gnc_commodity * cm)
{
    gnc_commodity_begin_edit(cm);
    qof_instance_set_destroying(cm, TRUE);
    gnc_commodity_commit_edit(cm);
}

void
gnc_commodity_copy(gnc_commodity * dest, const gnc_commodity *src)
{
    CommodityPrivate* src_priv = GET_PRIVATE(src);
    CommodityPrivate* dest_priv = GET_PRIVATE(dest);

    gnc_commodity_set_fullname (dest, src_priv->fullname);
    gnc_commodity_set_mnemonic (dest, src_priv->mnemonic);
    dest_priv->comm_namespace = src_priv->comm_namespace;
    gnc_commodity_set_fraction (dest, src_priv->fraction);
    gnc_commodity_set_cusip (dest, src_priv->cusip);
    gnc_commodity_set_quote_flag (dest, src_priv->quote_flag);
    gnc_commodity_set_quote_source (dest, gnc_commodity_get_quote_source (src));
    gnc_commodity_set_quote_tz (dest, src_priv->quote_tz);
    kvp_frame_delete (dest->kvp_data);
    dest->kvp_data = kvp_frame_copy (src->kvp_data);
    kvp_frame_delete (dest->kvp_data);
    dest->kvp_data = kvp_frame_copy (src->kvp_data);
}

gnc_commodity *
gnc_commodity_clone(const gnc_commodity *src, QofBook *dest_book)
{
    CommodityPrivate* src_priv;
    CommodityPrivate* dest_priv;

    gnc_commodity * dest = new gnc_commodity;// g_object_new(GNC_TYPE_COMMODITY, NULL);
    qof_instance_init_data (dest, GNC_ID_COMMODITY, dest_book);
    src_priv = GET_PRIVATE(src);
    dest_priv = GET_PRIVATE(dest);

    dest_priv->fullname = CACHE_INSERT(src_priv->fullname);
    dest_priv->mnemonic = CACHE_INSERT(src_priv->mnemonic);
    dest_priv->cusip = CACHE_INSERT(src_priv->cusip);
    dest_priv->quote_tz = CACHE_INSERT(src_priv->quote_tz);

    dest_priv->comm_namespace = src_priv->comm_namespace;

    dest_priv->fraction = src_priv->fraction;
    dest_priv->quote_flag = src_priv->quote_flag;

    gnc_commodity_set_quote_source (dest, gnc_commodity_get_quote_source (src));

    kvp_frame_delete (dest->kvp_data);
    dest->kvp_data = kvp_frame_copy (src->kvp_data);

    reset_printname(dest_priv);
    reset_unique_name(dest_priv);

    return dest;
}

/********************************************************************
 * gnc_commodity_get_mnemonic
 ********************************************************************/

const char *
gnc_commodity_get_mnemonic(const gnc_commodity * cm)
{
    if (!cm) return NULL;
    return GET_PRIVATE(cm)->mnemonic;
}

/********************************************************************
 * gnc_commodity_get_printname
 ********************************************************************/

const char *
gnc_commodity_get_printname(const gnc_commodity * cm)
{
    if (!cm) return NULL;
    return GET_PRIVATE(cm)->printname;
}


/********************************************************************
 * gnc_commodity_get_namespace
 ********************************************************************/

const char *
gnc_commodity_get_namespace(const gnc_commodity * cm)
{
    if (!cm) return NULL;
    return gnc_commodity_namespace_get_name(GET_PRIVATE(cm)->comm_namespace);
}

const char *
gnc_commodity_get_namespace_compat(const gnc_commodity * cm)
{
    CommodityPrivate* priv;

    if (!cm) return NULL;
    priv = GET_PRIVATE(cm);
    if (!priv->comm_namespace) return NULL;
    if (priv->comm_namespace->iso4217)
    {
        /* Data files are still written with ISO4217. */
        return GNC_COMMODITY_NS_ISO;
    }
    return gnc_commodity_namespace_get_name(priv->comm_namespace);
}

gnc_commodity_namespace *
gnc_commodity_get_namespace_ds(const gnc_commodity * cm)
{
    if (!cm) return NULL;
    return GET_PRIVATE(cm)->comm_namespace;
}

/********************************************************************
 * gnc_commodity_get_fullname
 ********************************************************************/

const char *
gnc_commodity_get_fullname(const gnc_commodity * cm)
{
    if (!cm) return NULL;
    return GET_PRIVATE(cm)->fullname;
}


/********************************************************************
 * gnc_commodity_get_unique_name
 ********************************************************************/

const char *
gnc_commodity_get_unique_name(const gnc_commodity * cm)
{
    if (!cm) return NULL;
    return GET_PRIVATE(cm)->unique_name;
}


/********************************************************************
 * gnc_commodity_get_cusip
 ********************************************************************/

const char *
gnc_commodity_get_cusip(const gnc_commodity * cm)
{
    if (!cm) return NULL;
    return GET_PRIVATE(cm)->cusip;
}

/********************************************************************
 * gnc_commodity_get_fraction
 ********************************************************************/

int
gnc_commodity_get_fraction(const gnc_commodity * cm)
{
    if (!cm) return 0;
    return GET_PRIVATE(cm)->fraction;
}

/********************************************************************
 * gnc_commodity_get_auto_quote_control_flag
 ********************************************************************/

static bool
gnc_commodity_get_auto_quote_control_flag(const gnc_commodity *cm)
{
    const char *str;

    if (!cm) return FALSE;

    str = kvp_frame_get_string(cm->kvp_data, "auto_quote_control");
    return !str || (strcmp(str, "false") != 0);
}

/********************************************************************
 * gnc_commodity_get_quote_flag
 ********************************************************************/

bool
gnc_commodity_get_quote_flag(const gnc_commodity *cm)
{
    if (!cm) return FALSE;
    return (GET_PRIVATE(cm)->quote_flag);
}

/********************************************************************
 * gnc_commodity_get_quote_source
 ********************************************************************/

gnc_quote_source*
gnc_commodity_get_quote_source(const gnc_commodity *cm)
{
    CommodityPrivate* priv;

    if (!cm) return NULL;
    priv = GET_PRIVATE(cm);
    if (!priv->quote_source && gnc_commodity_is_iso(cm))
        return &currency_quote_source;
    return priv->quote_source;
}

gnc_quote_source*
gnc_commodity_get_default_quote_source(const gnc_commodity *cm)
{
    if (cm && gnc_commodity_is_iso(cm))
        return &currency_quote_source;
    /* Should make this a user option at some point. */
    return gnc_quote_source_lookup_by_internal("yahoo");
}

/********************************************************************
 * gnc_commodity_get_quote_tz
 ********************************************************************/

const char*
gnc_commodity_get_quote_tz(const gnc_commodity *cm)
{
    if (!cm) return NULL;
    return GET_PRIVATE(cm)->quote_tz;
}

/********************************************************************
 * gnc_commodity_set_mnemonic
 ********************************************************************/

void
gnc_commodity_set_mnemonic(gnc_commodity * cm, const char * mnemonic)
{
    CommodityPrivate* priv;

    if (!cm) return;
    priv = GET_PRIVATE(cm);
    if (priv->mnemonic == mnemonic) return;

    gnc_commodity_begin_edit(cm);
    CACHE_REMOVE (priv->mnemonic);
    priv->mnemonic = CACHE_INSERT(mnemonic);

    mark_commodity_dirty (cm);
    reset_printname(priv);
    reset_unique_name(priv);
    gnc_commodity_commit_edit(cm);
}

/********************************************************************
 * gnc_commodity_set_namespace
 ********************************************************************/

void
gnc_commodity_set_namespace(gnc_commodity * cm, const char * comm_namespace)
{
    QofBook *book;
    gnc_commodity_table *table;
    gnc_commodity_namespace *nsp;
    CommodityPrivate* priv;

    if (!cm) return;
    priv = GET_PRIVATE(cm);
    book = qof_instance_get_book (cm);
    table = gnc_commodity_table_get_table(book);
    nsp = gnc_commodity_table_add_namespace(table, comm_namespace, book);
    if (priv->comm_namespace == nsp)
        return;

    gnc_commodity_begin_edit(cm);
    priv->comm_namespace = nsp;
    if (nsp->iso4217)
        priv->quote_source = gnc_quote_source_lookup_by_internal("currency");
    mark_commodity_dirty(cm);
    reset_printname(priv);
    reset_unique_name(priv);
    gnc_commodity_commit_edit(cm);
}

/********************************************************************
 * gnc_commodity_set_fullname
 ********************************************************************/

void
gnc_commodity_set_fullname(gnc_commodity * cm, const char * fullname)
{
    CommodityPrivate* priv;

    if (!cm) return;
    priv = GET_PRIVATE(cm);
    if (priv->fullname == fullname) return;

    CACHE_REMOVE (priv->fullname);
    priv->fullname = CACHE_INSERT (fullname);

    gnc_commodity_begin_edit(cm);
    mark_commodity_dirty(cm);
    reset_printname(priv);
    gnc_commodity_commit_edit(cm);
}

/********************************************************************
 * gnc_commodity_set_cusip
 ********************************************************************/

void
gnc_commodity_set_cusip(gnc_commodity * cm,
                        const char * cusip)
{
    CommodityPrivate* priv;

    if (!cm) return;

    priv = GET_PRIVATE(cm);
    if (priv->cusip == cusip) return;

    gnc_commodity_begin_edit(cm);
    CACHE_REMOVE (priv->cusip);
    priv->cusip = CACHE_INSERT (cusip);
    mark_commodity_dirty(cm);
    gnc_commodity_commit_edit(cm);
}

/********************************************************************
 * gnc_commodity_set_fraction
 ********************************************************************/

void
gnc_commodity_set_fraction(gnc_commodity * cm, int fraction)
{
    if (!cm) return;
    gnc_commodity_begin_edit(cm);
    GET_PRIVATE(cm)->fraction = fraction;
    mark_commodity_dirty(cm);
    gnc_commodity_commit_edit(cm);
}

/********************************************************************
 * gnc_commodity_set_auto_quote_control_flag
 ********************************************************************/

static void
gnc_commodity_set_auto_quote_control_flag(gnc_commodity *cm,
        const bool flag)
{
    ENTER ("(cm=%p, flag=%d)", cm, flag);

    if (!cm)
    {
        LEAVE("");
        return;
    }

    gnc_commodity_begin_edit(cm);
    kvp_frame_set_string(cm->kvp_data,
                         "auto_quote_control", flag ? NULL : "false");
    mark_commodity_dirty(cm);
    gnc_commodity_commit_edit(cm);
    LEAVE("");
}

/********************************************************************
 * gnc_commodity_user_set_quote_flag
 ********************************************************************/

void
gnc_commodity_user_set_quote_flag(gnc_commodity *cm, const bool flag)
{
    CommodityPrivate* priv;

    ENTER ("(cm=%p, flag=%d)", cm, flag);

    if (!cm)
    {
        LEAVE("");
        return;
    }

    priv = GET_PRIVATE(cm);
    gnc_commodity_begin_edit(cm);
    gnc_commodity_set_quote_flag(cm, flag);
    if (gnc_commodity_is_iso(cm))
    {
        /* For currencies, disable auto quote control if the quote flag is being
         * changed from its default value and enable it if the quote flag is being
         * reset to its default value.  The defaults for the quote flag are
         * disabled if no accounts are using the currency, and true otherwise.
         * Thus enable auto quote control if flag is FALSE and there are not any
         * accounts using this currency OR flag is TRUE and there are accounts
         * using this currency; otherwise disable auto quote control */
        gnc_commodity_set_auto_quote_control_flag(cm,
                (!flag && (priv->usage_count == 0)) || (flag && (priv->usage_count != 0)));
    }
    gnc_commodity_commit_edit(cm);
    LEAVE("");
}

/********************************************************************
 * gnc_commodity_set_quote_flag
 ********************************************************************/

void
gnc_commodity_set_quote_flag(gnc_commodity *cm, const bool flag)
{
    ENTER ("(cm=%p, flag=%d)", cm, flag);

    if (!cm) return;
    gnc_commodity_begin_edit(cm);
    GET_PRIVATE(cm)->quote_flag = flag;
    mark_commodity_dirty(cm);
    gnc_commodity_commit_edit(cm);
    LEAVE(" ");
}

/********************************************************************
 * gnc_commodity_set_quote_source
 ********************************************************************/

void
gnc_commodity_set_quote_source(gnc_commodity *cm, gnc_quote_source *src)
{
    ENTER ("(cm=%p, src=%p(%s))", cm, src, src ? src->internal_name : "unknown");

    if (!cm) return;
    gnc_commodity_begin_edit(cm);
    GET_PRIVATE(cm)->quote_source = src;
    mark_commodity_dirty(cm);
    gnc_commodity_commit_edit(cm);
    LEAVE(" ");
}

/********************************************************************
 * gnc_commodity_set_quote_tz
 ********************************************************************/

void
gnc_commodity_set_quote_tz(gnc_commodity *cm, const char *tz)
{
    CommodityPrivate* priv;

    if (!cm) return;

    ENTER ("(cm=%p, tz=%s)", cm, tz ? tz : "(null)");

    priv = GET_PRIVATE(cm);

    if (tz == priv->quote_tz)
    {
        LEAVE("Already correct TZ");
        return;
    }

    gnc_commodity_begin_edit(cm);
    CACHE_REMOVE (priv->quote_tz);
    priv->quote_tz = CACHE_INSERT (tz);
    mark_commodity_dirty(cm);
    gnc_commodity_commit_edit(cm);
    LEAVE(" ");
}

/********************************************************************
 * gnc_commodity_increment_usage_count
 ********************************************************************/

void
gnc_commodity_increment_usage_count(gnc_commodity *cm)
{
    CommodityPrivate* priv;

    ENTER("(cm=%p)", cm);

    if (!cm)
    {
        LEAVE("");
        return;
    }

    priv = GET_PRIVATE(cm);

    if ((priv->usage_count == 0) && !priv->quote_flag
            && gnc_commodity_get_auto_quote_control_flag(cm)
            && gnc_commodity_is_iso(cm))
    {
        /* compatability hack - Gnucash 1.8 gets currency quotes when a
           non-default currency is assigned to an account.  */
        gnc_commodity_begin_edit(cm);
        gnc_commodity_set_quote_flag(cm, TRUE);
        gnc_commodity_set_quote_source(cm,
                                       gnc_commodity_get_default_quote_source(cm));
        gnc_commodity_commit_edit(cm);
    }
    priv->usage_count++;
    LEAVE("(usage_count=%d)", priv->usage_count);
}

/********************************************************************
 * gnc_commodity_decrement_usage_count
 ********************************************************************/

void
gnc_commodity_decrement_usage_count(gnc_commodity *cm)
{
    CommodityPrivate* priv;

    ENTER("(cm=%p)", cm);

    if (!cm)
    {
        LEAVE("");
        return;
    }

    priv = GET_PRIVATE(cm);

    if (priv->usage_count == 0)
    {
        PWARN("usage_count already zero");
        LEAVE("");
        return;
    }

    priv->usage_count--;
    if ((priv->usage_count == 0) && priv->quote_flag
            && gnc_commodity_get_auto_quote_control_flag(cm)
            && gnc_commodity_is_iso(cm))
    {
        /* if this is a currency with auto quote control enabled and no more
         * accounts reference this currency, disable quote retrieval */
        gnc_commodity_set_quote_flag(cm, FALSE);
    }
    LEAVE("(usage_count=%d)", priv->usage_count);
}

/********************************************************************\
\********************************************************************/


/********************************************************************
 * gnc_commodity_equiv
 * are two commodities the same?
 ********************************************************************/

bool
gnc_commodity_equiv(const gnc_commodity * a, const gnc_commodity * b)
{
    CommodityPrivate* priv_a;
    CommodityPrivate* priv_b;

    if (a == b) return TRUE;
    if (!a || !b) return FALSE;

    priv_a = GET_PRIVATE(a);
    priv_b = GET_PRIVATE(b);
    if (priv_a->comm_namespace != priv_b->comm_namespace) return FALSE;
    if (g_strcmp0(priv_a->mnemonic, priv_b->mnemonic) != 0) return FALSE;
    return TRUE;
}

bool
gnc_commodity_equal(const gnc_commodity * a, const gnc_commodity * b)
{
    CommodityPrivate* priv_a;
    CommodityPrivate* priv_b;
    bool same_book;

    if (a == b) return TRUE;

    if (!a || !b)
    {
        DEBUG ("one is NULL");
        return FALSE;
    }

    priv_a = GET_PRIVATE(a);
    priv_b = GET_PRIVATE(b);
    same_book = qof_instance_get_book(QOF_INSTANCE(a)) == qof_instance_get_book(QOF_INSTANCE(b));

    if ((same_book && priv_a->comm_namespace != priv_b->comm_namespace)
            || (!same_book && g_strcmp0( gnc_commodity_namespace_get_name(priv_a->comm_namespace),
                                           gnc_commodity_namespace_get_name(priv_b->comm_namespace)) != 0))
    {
        DEBUG ("namespaces differ: %p(%s) vs %p(%s)",
               priv_a->comm_namespace, gnc_commodity_namespace_get_name(priv_a->comm_namespace),
               priv_b->comm_namespace, gnc_commodity_namespace_get_name(priv_b->comm_namespace));
        return FALSE;
    }

    if (g_strcmp0(priv_a->mnemonic, priv_b->mnemonic) != 0)
    {
        DEBUG ("mnemonics differ: %s vs %s", priv_a->mnemonic, priv_b->mnemonic);
        return FALSE;
    }

    if (g_strcmp0(priv_a->fullname, priv_b->fullname) != 0)
    {
        DEBUG ("fullnames differ: %s vs %s", priv_a->fullname, priv_b->fullname);
        return FALSE;
    }

    if (g_strcmp0(priv_a->cusip, priv_b->cusip) != 0)
    {
        DEBUG ("cusips differ: %s vs %s", priv_a->cusip, priv_b->cusip);
        return FALSE;
    }

    if (priv_a->fraction != priv_b->fraction)
    {
        DEBUG ("fractions differ: %d vs %d", priv_a->fraction, priv_b->fraction);
        return FALSE;
    }

    return TRUE;
}

int gnc_commodity_compare(const gnc_commodity * a, const gnc_commodity * b)
{
    if (gnc_commodity_equal(a, b))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

int gnc_commodity_compare_void(const void * a, const void * b)
{
    return gnc_commodity_compare(a, b);
}

/************************************************************
 *                   Namespace functions                    *
 ************************************************************/

gnc_commodity_namespace::gnc_commodity_namespace()
{
    name = NULL;
    iso4217 = false;
    cm_table = NULL;
}
    
gnc_commodity_namespace::~gnc_commodity_namespace()
{
    // TODO
}


const char *
gnc_commodity_namespace_get_name (const gnc_commodity_namespace *ns)
{
    if (ns == NULL)
        return NULL;
    return ns->name;
}

CommodityList_t 
gnc_commodity_namespace_get_commodity_list(const gnc_commodity_namespace *comm_namespace)
{
    if (!comm_namespace)
        return CommodityList_t();

    return comm_namespace->cm_list;
}

bool
gnc_commodity_namespace_is_iso(const char *comm_namespace)
{
    return ((g_strcmp0(comm_namespace, GNC_COMMODITY_NS_ISO) == 0) ||
            (g_strcmp0(comm_namespace, GNC_COMMODITY_NS_CURRENCY) == 0));
}

static const gchar *
gnc_commodity_table_map_namespace(const char * comm_namespace)
{
    if (g_strcmp0(comm_namespace, GNC_COMMODITY_NS_ISO) == 0)
        return GNC_COMMODITY_NS_CURRENCY;
    return comm_namespace;
}

/********************************************************************
 * gnc_commodity_table_new
 * make a new commodity table
 ********************************************************************/

gnc_commodity_table *
gnc_commodity_table_new(void)
{
    gnc_commodity_table * retval = new gnc_commodity_table;//g_new0(gnc_commodity_table, 1);
    retval->ns_table = g_hash_table_new(&g_str_hash, &g_str_equal);
//    retval->ns_list = NULL;
    return retval;
}

/********************************************************************
 * book anchor functons
 ********************************************************************/

gnc_commodity_table *
gnc_commodity_table_get_table(QofBook *book)
{
    if (!book) return NULL;
    return qof_book_get_data (book, GNC_COMMODITY_TABLE);
}

gnc_commodity *
gnc_commodity_obtain_twin (const gnc_commodity *from, QofBook *book)
{
    gnc_commodity *twin;
    const char * ucom;
    gnc_commodity_table * comtbl;

    if (!from) return NULL;
    comtbl = gnc_commodity_table_get_table (book);
    if (!comtbl) return NULL;

    ucom = gnc_commodity_get_unique_name (from);
    twin = gnc_commodity_table_lookup_unique (comtbl, ucom);
    if (!twin)
    {
        twin = gnc_commodity_clone (from, book);
        twin = gnc_commodity_table_insert (comtbl, twin);
    }
    return twin;
}

/********************************************************************
 * gnc_commodity_table_get_size
 * get the size of the commodity table
 ********************************************************************/

static void
count_coms(gpointer key, gpointer value, gpointer user_data)
{
    GHashTable *tbl = ((gnc_commodity_namespace*)value)->cm_table;
    guint *count = (guint*)user_data;

    if (g_strcmp0((char*)key, GNC_COMMODITY_NS_CURRENCY) == 0)
    {
        /* don't count default commodities */
        return;
    }

    if (!value) return;

    *count += g_hash_table_size(tbl);
}

guint
gnc_commodity_table_get_size(const gnc_commodity_table* tbl)
{
    guint count = 0;
    g_return_val_if_fail(tbl, 0);
    g_return_val_if_fail(tbl->ns_table, 0);

    g_hash_table_foreach(tbl->ns_table, count_coms, (gpointer)&count);

    return count;
}

/********************************************************************
 * gnc_commodity_table_lookup
 * locate a commodity by namespace and mnemonic.
 ********************************************************************/

gnc_commodity *
gnc_commodity_table_lookup(const gnc_commodity_table * table,
                           const char * comm_namespace, const char * mnemonic)
{
    gnc_commodity_namespace * nsp = NULL;
    unsigned int i;

    if (!table || !comm_namespace || !mnemonic) return NULL;

    nsp = gnc_commodity_table_find_namespace(table, comm_namespace);

    if (nsp)
    {
        /*
         * Backward compatability support for currencies that have
         * recently changed.
         */
        if (nsp->iso4217)
        {
            for (i = 0; i < GNC_NEW_ISO_CODES; i++)
            {
                if (strcmp(mnemonic, gnc_new_iso_codes[i].old_code) == 0)
                {
                    mnemonic = gnc_new_iso_codes[i].new_code;
                    break;
                }
            }
        }
        return g_hash_table_lookup(nsp->cm_table, (gpointer)mnemonic);
    }
    else
    {
        return NULL;
    }
}

/********************************************************************
 * gnc_commodity_table_lookup
 * locate a commodity by unique name.
 ********************************************************************/

gnc_commodity *
gnc_commodity_table_lookup_unique(const gnc_commodity_table *table,
                                  const char * unique_name)
{
    char *comm_namespace;
    char *mnemonic;
    gnc_commodity *commodity;

    if (!table || !unique_name) return NULL;

    comm_namespace = g_strdup (unique_name);
    mnemonic = strstr (comm_namespace, "::");
    if (!mnemonic)
    {
        g_free (comm_namespace);
        return NULL;
    }

    *mnemonic = '\0';
    mnemonic += 2;

    commodity = gnc_commodity_table_lookup (table, comm_namespace, mnemonic);

    g_free (comm_namespace);

    return commodity;
}

/********************************************************************
 * gnc_commodity_table_find_full
 * locate a commodity by namespace and printable name
 ********************************************************************/

gnc_commodity *
gnc_commodity_table_find_full(const gnc_commodity_table * table,
                              const char * comm_namespace,
                              const char * fullname)
{
    gnc_commodity * retval = NULL;

    if (!fullname || (fullname[0] == '\0'))
        return NULL;

    CommodityList_t all = gnc_commodity_table_get_commodities(table, comm_namespace);

    for (CommodityList_t::iterator it = all.begin(); it != all.end(); it++)
    {
        if (!strcmp(fullname,
                    gnc_commodity_get_printname(*it)))
        {
            retval = *it;
            break;
        }
    }

    return retval;
}


/********************************************************************
 * gnc_commodity_table_insert
 * add a commodity to the table.
 ********************************************************************/

gnc_commodity *
gnc_commodity_table_insert(gnc_commodity_table * table,
                           gnc_commodity * comm)
{
    gnc_commodity_namespace * nsp = NULL;
    gnc_commodity *c;
    const char *ns_name;
    CommodityPrivate* priv;
    QofBook *book;

    if (!table) return NULL;
    if (!comm) return NULL;

    priv = GET_PRIVATE(comm);

    ENTER ("(table=%p, comm=%p) %s %s", table, comm,
           (priv->mnemonic == NULL ? "(null)" : priv->mnemonic),
           (priv->fullname == NULL ? "(null)" : priv->fullname));
    ns_name = gnc_commodity_namespace_get_name(priv->comm_namespace);
    c = gnc_commodity_table_lookup (table, ns_name, priv->mnemonic);

    if (c)
    {
        if (c == comm)
        {
            LEAVE("already in table");
            return c;
        }

        /* Backward compatability support for currencies that have
         * recently changed. */
        if (priv->comm_namespace->iso4217)
        {
            guint i;
            for (i = 0; i < GNC_NEW_ISO_CODES; i++)
            {
                if (!priv->mnemonic
                        || !strcmp(priv->mnemonic, gnc_new_iso_codes[i].old_code))
                {
                    gnc_commodity_set_mnemonic(comm, gnc_new_iso_codes[i].new_code);
                    break;
                }
            }
        }

        gnc_commodity_copy (c, comm);
        gnc_commodity_destroy (comm);
        LEAVE("found at %p", c);
        return c;
    }

    book = qof_instance_get_book (comm);
    nsp = gnc_commodity_table_add_namespace(table, ns_name, book);

    PINFO ("insert %p %s into nsp=%p %s", priv->mnemonic, priv->mnemonic,
           nsp->cm_table, nsp->name);
    g_hash_table_insert(nsp->cm_table,
                        CACHE_INSERT(priv->mnemonic),
                        (gpointer)comm);
    nsp->cm_list.push_back(comm);

    qof_event_gen (comm, QOF_EVENT_ADD, NULL);
    LEAVE ("(table=%p, comm=%p)", table, comm);
    return comm;
}

/********************************************************************
 * gnc_commodity_table_remove
 * remove a commodity from the table.
 ********************************************************************/

void
gnc_commodity_table_remove(gnc_commodity_table * table,
                           gnc_commodity * comm)
{
    gnc_commodity_namespace * nsp;
    gnc_commodity *c;
    CommodityPrivate* priv;
    const char *ns_name;

    if (!table) return;
    if (!comm) return;

    priv = GET_PRIVATE(comm);
    ns_name = gnc_commodity_namespace_get_name(priv->comm_namespace);
    c = gnc_commodity_table_lookup (table, ns_name, priv->mnemonic);
    if (c != comm) return;

    qof_event_gen (comm, QOF_EVENT_REMOVE, NULL);

    nsp = gnc_commodity_table_find_namespace(table, ns_name);
    if (!nsp) return;

    nsp->cm_list.remove(comm);
    g_hash_table_remove (nsp->cm_table, priv->mnemonic);
    /* XXX minor mem leak, should remove the key as well */
}

/********************************************************************
 * gnc_commodity_table_has_namespace
 * see if the commodities namespace exists. May have zero commodities.
 ********************************************************************/

int
gnc_commodity_table_has_namespace(const gnc_commodity_table * table,
                                  const char * comm_namespace)
{
    gnc_commodity_namespace * nsp = NULL;

    if (!table || !comm_namespace)
    {
        return 0;
    }

    nsp = gnc_commodity_table_find_namespace(table, comm_namespace);
    if (nsp)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static void
hash_keys_helper(gpointer key, gpointer value, gpointer data)
{
    GList ** l = data;
    *l = g_list_prepend(*l, key);
}

static GList *
g_hash_table_keys(GHashTable * table)
{
    GList * l = NULL;
    g_hash_table_foreach(table, &hash_keys_helper, (gpointer) &l);
    return l;
}

static void
hash_values_helper(gpointer key, gpointer value, gpointer data)
{
    GList ** l = data;
    *l = g_list_prepend(*l, value);
}

static GList *
g_hash_table_values(GHashTable * table)
{
    GList * l = NULL;
    g_hash_table_foreach(table, &hash_values_helper, (gpointer) &l);
    return l;
}

/********************************************************************
 * gnc_commodity_table_get_namespaces
 * see if any commodities in the namespace exist
 ********************************************************************/

std::list<gnc_commodity_namespace*>
gnc_commodity_table_get_namespaces(const gnc_commodity_table * table)
{
    if (!table)
        return std::list<gnc_commodity_namespace*>();

    return table->ns_list;//g_hash_table_keys(table->ns_table);
}

//GList *
//gnc_commodity_table_get_namespaces_list(const gnc_commodity_table * table)
//{
//    if (!table)
//        return NULL;
//
//    return table->ns_list;
//}

/* Because gnc_commodity_table_add_namespace maps GNC_COMMODITY_NS_ISO to
   GNC_COMMODITY_NS_CURRENCY and then sets iso4217 if the namespace is
   either of these, the net result is that the iso4217 bit is set only
   for GNC_COMMODITY_NS_CURRENCY.  This means that gnc_commodity_is_iso is
   a subset of gnc_commodity_is_currency.  Most callers seem to use
   gnc_commodity_is_iso. */
bool
gnc_commodity_is_iso(const gnc_commodity * cm)
{
    CommodityPrivate* priv;

    if (!cm) return FALSE;

    priv = GET_PRIVATE(cm);
    if ( !priv->comm_namespace) return FALSE;
    return priv->comm_namespace->iso4217;
}

bool
gnc_commodity_is_currency(const gnc_commodity *cm)
{
    const char *ns_name;
    if (!cm) return FALSE;

    ns_name = gnc_commodity_namespace_get_name(GET_PRIVATE(cm)->comm_namespace);
    return (!g_strcmp0(ns_name, GNC_COMMODITY_NS_LEGACY) ||
            !g_strcmp0(ns_name, GNC_COMMODITY_NS_CURRENCY));
}

/********************************************************************
 * gnc_commodity_table_get_commodities
 * list commodities in a give namespace
 ********************************************************************/

CommodityList_t
gnc_commodity_table_get_commodities(const gnc_commodity_table * table,
                                    const char * comm_namespace)
{
    gnc_commodity_namespace * ns = NULL;

    if (!table)
        return CommodityList_t();

    ns = gnc_commodity_table_find_namespace(table, comm_namespace);
    if (!ns)
        return CommodityList_t();

    GList * legacy_values = g_hash_table_values(ns->cm_table);
    CommodityList_t list;
    for(GList * it = legacy_values; it; it = it->next)
    {
        list.push_back(reinterpret_cast<gnc_commodity*>(it->data));
    }
    return list;
}

/********************************************************************
 * gnc_commodity_table_get_quotable_commodities
 * list commodities in a given namespace that get price quotes
 ********************************************************************/

static void
get_quotables_helper1(gpointer key, gpointer value, gpointer data)
{
    gnc_commodity *comm = value;
    CommodityPrivate* priv = GET_PRIVATE(comm);
    CommodityList_t * l = reinterpret_cast<CommodityList_t*>(data);

    if (!priv->quote_flag ||
            !priv->quote_source || !priv->quote_source->supported)
        return;
    l->push_front(value);
}

static bool
get_quotables_helper2 (gnc_commodity *comm, gpointer data)
{
    CommodityList_t * l = reinterpret_cast<CommodityList_t*>(data);
    CommodityPrivate* priv = GET_PRIVATE(comm);

    if (!priv->quote_flag ||
            !priv->quote_source || !priv->quote_source->supported)
        return TRUE;
    l->push_front(comm);
    return TRUE;
}

CommodityList_t
gnc_commodity_table_get_quotable_commodities(const gnc_commodity_table * table)
{
    gnc_commodity_namespace * ns = NULL;
    const char *comm_namespace;
    CommodityList_t l;
    regex_t pattern;
    const char *expression = gnc_main_get_namespace_regexp();

    ENTER("table=%p, expression=%s", table, expression);
    if (!table)
        return l;

    if (expression && *expression)
    {
        if (regcomp(&pattern, expression, REG_EXTENDED | REG_ICASE) != 0)
        {
            LEAVE("Cannot compile regex");
            return l;
        }

        std::list<gnc_commodity_namespace*> nslist = gnc_commodity_table_get_namespaces(table);
        for (std::list<gnc_commodity_namespace*>::iterator it = nslist.begin(); it != nslist.end(); it++)
        {
            comm_namespace = (*it)->name;
            if (regexec(&pattern, comm_namespace, 0, NULL, 0) == 0)
            {
                DEBUG("Running list of %s commodities", comm_namespace);
                ns = gnc_commodity_table_find_namespace(table, comm_namespace);
                if (ns)
                {
                    g_hash_table_foreach(ns->cm_table, &get_quotables_helper1, (gpointer) &l);
                }
            }
        }
        regfree(&pattern);
    }
    else
    {
        gnc_commodity_table_foreach_commodity(table, get_quotables_helper2,
                                              (gpointer) &l);
    }
    LEAVE("list size %d", l.size());
    return l;
}

/********************************************************************
 * gnc_commodity_table_add_namespace
 * add an empty namespace if it does not exist
 ********************************************************************/

gnc_commodity_namespace *
gnc_commodity_table_add_namespace(gnc_commodity_table * table,
                                  const char * comm_namespace,
                                  QofBook *book)
{
    gnc_commodity_namespace * ns = NULL;

    if (!table) return NULL;

    comm_namespace = gnc_commodity_table_map_namespace(comm_namespace);
    ns = gnc_commodity_table_find_namespace(table, comm_namespace);
    if (!ns)
    {
        ns = new gnc_commodity_namespace; //g_object_new(GNC_TYPE_COMMODITY_NAMESPACE, NULL);
        ns->cm_table = g_hash_table_new(g_str_hash, g_str_equal);
        ns->name = CACHE_INSERT((gpointer)comm_namespace);
        ns->iso4217 = gnc_commodity_namespace_is_iso(comm_namespace);
        qof_instance_init_data (ns, GNC_ID_COMMODITY_NAMESPACE, book);
        qof_event_gen (ns, QOF_EVENT_CREATE, NULL);

        g_hash_table_insert(table->ns_table,
                            (gpointer) ns->name,
                            (gpointer) ns);
        table->ns_list.push_back(ns);
        qof_event_gen (ns, QOF_EVENT_ADD, NULL);
    }
    return ns;
}


gnc_commodity_namespace *
gnc_commodity_table_find_namespace(const gnc_commodity_table * table,
                                   const char * comm_namespace)
{
    if (!table || !comm_namespace)
        return NULL;

    comm_namespace = gnc_commodity_table_map_namespace(comm_namespace);
    return g_hash_table_lookup(table->ns_table, (gpointer)comm_namespace);
}


gnc_commodity *
gnc_commodity_find_commodity_by_guid(const GncGUID *guid, QofBook *book)
{
    QofCollection *col;
    if (!guid || !book) return NULL;
    col = qof_book_get_collection (book, GNC_ID_COMMODITY);
    return (gnc_commodity *) qof_collection_lookup_entity (col, guid);
}

/********************************************************************
 * gnc_commodity_table_delete_namespace
 * delete a namespace
 ********************************************************************/

static int
ns_helper(gpointer key, gpointer value, gpointer user_data)
{
    gnc_commodity * c = value;
    gnc_commodity_destroy(c);
    CACHE_REMOVE(key);  /* key is commodity mnemonic */
    return TRUE;
}

void
gnc_commodity_table_delete_namespace(gnc_commodity_table * table,
                                     const char * comm_namespace)
{
    gnc_commodity_namespace * ns;

    if (!table) return;

    ns = gnc_commodity_table_find_namespace(table, comm_namespace);
    if (!ns)
        return;

    qof_event_gen (ns, QOF_EVENT_REMOVE, NULL);
    g_hash_table_remove(table->ns_table, comm_namespace);
    table->ns_list.remove(ns);

    g_hash_table_foreach_remove(ns->cm_table, ns_helper, NULL);
    g_hash_table_destroy(ns->cm_table);
    CACHE_REMOVE(ns->name);

    qof_event_gen (ns, QOF_EVENT_DESTROY, NULL);
    /* qof_instance_release(&ns->inst); */
//    g_object_unref(ns);
    delete ns;
}

/********************************************************************
 * gnc_commodity_table_foreach_commodity
 * call user-defined function once for every commodity in every
 * namespace
 ********************************************************************/

typedef struct
{
    bool ok;
    bool (*func)(gnc_commodity *, gpointer);
    gpointer user_data;
} IterData;

static void
iter_commodity (gpointer key, gpointer value, gpointer user_data)
{
    IterData *iter_data = (IterData *) user_data;
    gnc_commodity *cm = (gnc_commodity *) value;

    if (iter_data->ok)
    {
        iter_data->ok = (iter_data->func)(cm, iter_data->user_data);
    }
}

static void
iter_namespace (gpointer key, gpointer value, gpointer user_data)
{
    GHashTable *namespace_hash = ((gnc_commodity_namespace *) value)->cm_table;
    g_hash_table_foreach (namespace_hash, iter_commodity, user_data);
}

bool
gnc_commodity_table_foreach_commodity (const gnc_commodity_table * tbl,
                                       bool (*f)(gnc_commodity *, gpointer),
                                       gpointer user_data)
{
    IterData iter_data;

    if (!tbl || !f) return FALSE;

    iter_data.ok = TRUE;
    iter_data.func = f;
    iter_data.user_data = user_data;

    g_hash_table_foreach(tbl->ns_table, iter_namespace, (gpointer)&iter_data);

    return iter_data.ok;
}

/********************************************************************
 * gnc_commodity_table_destroy
 * cleanup and free.
 ********************************************************************/

void
gnc_commodity_table_destroy(gnc_commodity_table * t)
{
    gnc_commodity_namespace * ns;

    if (!t) return;
    ENTER ("table=%p", t);

    std::list<gnc_commodity_namespace*>::iterator item = t->ns_list.begin();
    std::list<gnc_commodity_namespace*>::iterator next;
    for (; item != t->ns_list.end(); item = next)
    {
        next = item;
        next++;
        ns = *item;
        gnc_commodity_table_delete_namespace(t, ns->name);
    }

    g_hash_table_destroy(t->ns_table);
    t->ns_table = NULL;
//    g_free(t);
    delete t;
    LEAVE ("table=%p", t);
}

/* =========================================================== */

/********************************************************************
 * gnc_commodity_table_add_default_data
 ********************************************************************/

#define CUR_I18N(String) dgettext ("iso_4217", String)

bool
gnc_commodity_table_add_default_data(gnc_commodity_table *table, QofBook *book)
{
    QofCollection *col;
    gnc_commodity* c;

    ENTER ("table=%p", table);
    gnc_commodity_table_add_namespace(table, GNC_COMMODITY_NS_AMEX, book);
    gnc_commodity_table_add_namespace(table, GNC_COMMODITY_NS_NYSE, book);
    gnc_commodity_table_add_namespace(table, GNC_COMMODITY_NS_NASDAQ, book);
    gnc_commodity_table_add_namespace(table, GNC_COMMODITY_NS_EUREX, book);
    gnc_commodity_table_add_namespace(table, GNC_COMMODITY_NS_MUTUAL, book);
    gnc_commodity_table_add_namespace(table, "template", book);
    c = gnc_commodity_new(book, "template", "template", "template", "template", 1);
    gnc_commodity_table_insert(table, c);

#include "iso-4217-currencies.cpp"

    /* We've just created the default namespaces and currencies.  Mark
     * these collections as clean because there is no USER entered data
     * in these collections as of yet. */
    col = qof_book_get_collection(book, GNC_ID_COMMODITY);
    qof_collection_mark_clean(col);
    col = qof_book_get_collection(book, GNC_ID_COMMODITY_NAMESPACE);
    qof_collection_mark_clean(col);

    LEAVE ("table=%p", table);
    return TRUE;
}

/********************************************************************
 ********************************************************************/
/* QofObject function implementation and registration */

/* A C++ compiler doesn't have C99 "designated initializers"
 * so we wrap them in a macro that is empty. */
# define DI(x) /* */
static QofObject commodity_object_def =
{
    DI(.interface_version = ) QOF_OBJECT_VERSION,
    DI(.e_type            = ) GNC_ID_COMMODITY,
    DI(.type_label        = ) "Commodity",
    DI(.create            = ) NULL,
    DI(.book_begin        = ) NULL,
    DI(.book_end          = ) NULL,
    DI(.is_dirty          = ) qof_collection_is_dirty,
    DI(.mark_clean        = ) qof_collection_mark_clean,
    DI(.foreach           = ) qof_collection_foreach,
    DI(.printable         = ) (const char * (*)(gpointer)) gnc_commodity_get_fullname,
};

static QofObject namespace_object_def =
{
    DI(.interface_version = ) QOF_OBJECT_VERSION,
    DI(.e_type            = ) GNC_ID_COMMODITY_NAMESPACE,
    DI(.type_label        = ) "Namespace",
    DI(.create            = ) NULL,
    DI(.book_begin        = ) NULL,
    DI(.book_end          = ) NULL,
    DI(.is_dirty          = ) NULL,
    DI(.mark_clean        = ) NULL,
    DI(.foreach           = ) NULL,
    DI(.printable         = ) NULL,
};

static void
commodity_table_book_begin (QofBook *book)
{
    gnc_commodity_table *ct;
    ENTER ("book=%p", book);

    if (gnc_commodity_table_get_table(book))
        return;

    ct = gnc_commodity_table_new ();
    qof_book_set_data (book, GNC_COMMODITY_TABLE, ct);

    if (!gnc_commodity_table_add_default_data(ct, book))
    {
        PWARN("unable to initialize book's commodity_table");
    }

    LEAVE ("book=%p", book);
}

static void
commodity_table_book_end (QofBook *book)
{
    gnc_commodity_table *ct;

    ct = gnc_commodity_table_get_table (book);
    qof_book_set_data (book, GNC_COMMODITY_TABLE, NULL);
    gnc_commodity_table_destroy (ct);
}

static QofObject commodity_table_object_def =
{
    DI(.interface_version = ) QOF_OBJECT_VERSION,
    DI(.e_type            = ) GNC_ID_COMMODITY_TABLE,
    DI(.type_label        = ) "CommodityTable",
    DI(.create            = ) NULL,
    DI(.book_begin        = ) commodity_table_book_begin,
    DI(.book_end          = ) commodity_table_book_end,
    DI(.is_dirty          = ) qof_collection_is_dirty,
    DI(.mark_clean        = ) qof_collection_mark_clean,
    DI(.foreach           = ) NULL,
    DI(.printable         = ) NULL,
    DI(.version_cmp       = ) NULL,
};

bool
gnc_commodity_table_register (void)
{
    gnc_quote_source_init_tables();

    if (!qof_object_register (&commodity_object_def))
        return FALSE;
    if (!qof_object_register (&namespace_object_def))
        return FALSE;
    return qof_object_register (&commodity_table_object_def);
}

/* *******************************************************************
*  gnc_monetary methods
********************************************************************/

/** Add a gnc_monetary to the list */
MonetaryList_t
gnc_monetary_list_add_monetary(MonetaryList_t list, gnc_monetary add_mon)
{
    MonetaryList_t::iterator it = list.begin();
    for (; it != list.end(); it++)
    {
        gnc_monetary *list_mon = *it;
        if (gnc_commodity_equiv(list_mon->commodity, add_mon.commodity))
        {
            list_mon->value = gnc_numeric_add(list_mon->value, add_mon.value,
                                              GNC_DENOM_AUTO, GNC_HOW_DENOM_EXACT);
            break;
        }
    }

    /* See if we found an entry, and add one if not */
    if (it == list.end())
    {
        gnc_monetary *new_mon = new gnc_monetary;//g_new0(gnc_monetary, 1);
        *new_mon = add_mon;
        list.push_front(new_mon);
    }

    return list;
}

/** Delete all entries in the list that have zero value.  Return list
    pointer will be a null pointer if there are no non-zero entries **/
MonetaryList_t
gnc_monetary_list_delete_zeros(MonetaryList_t list)
{
    MonetaryList_t::iterator it = list.begin();
    while(it != list.end())
    {
        gnc_monetary *mon = *it;
        if (gnc_numeric_zero_p(mon->value))
        {
//            g_free(mon);
            delete mon;
            list.erase(it++);
        }
        else
        {
            it++;
        }
    }
    return list;
}

/** Free a MonetaryList and all the monetaries it points to */
void
gnc_monetary_list_free(MonetaryList_t list)
{
    for(MonetaryList_t::iterator it = list.begin(); it != list.end(); it++)
    {
//        g_free(tmp->data);
        delete (gnc_monetary *)(*it);
    }
}

/* ========================= END OF FILE ============================== */
