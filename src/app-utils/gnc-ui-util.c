/********************************************************************\
 * gnc-ui-util.c -- utility functions for the GnuCash UI            *
 * Copyright (C) 2000 Dave Peticolas <dave@krondo.com>              *
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
\********************************************************************/

#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <glib.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "Transaction.h"
#include "global-options.h"

#include "Account.h"
#include "gnc-book.h"
#include "gnc-component-manager.h"
#include "gnc-engine-util.h"
#include "gnc-engine.h"
#include "gnc-euro.h"
#include "gnc-module.h"
#include "gnc-session.h"
#include "gnc-ui-util.h"
#include "Group.h"
#include "messages.h"
#include "Transaction.h"
#include "guile-mappings.h"


static short module = MOD_GUI;

static gboolean auto_decimal_enabled = FALSE;
static int auto_decimal_places = 2;    /* default, can be changed */

static gboolean reverse_balance_inited = FALSE;
static SCM reverse_balance_callback_id = SCM_UNDEFINED;
static gboolean reverse_type[NUM_ACCOUNT_TYPES];
static gboolean fq_is_installed = FALSE;

typedef struct quote_source_t {
  gboolean supported;
  gboolean translate;
  char *user_name;	/* User friendly name */
  char *internal_name;	/* Name used internally. */
  char *fq_name;	/* Name used by finance::quote. */
} quote_source;

static quote_source quote_sources[NUM_SOURCES] = {
  { TRUE,  TRUE,  N_("(none)"), NULL, NULL },
  { FALSE, TRUE,  N_("-- Single Sources --"), NULL, NULL },
  { FALSE, FALSE, "AEX", "AEX", "aex" },
  { FALSE, FALSE, "ASX", "ASX", "asx" },
  { FALSE, FALSE, "DWS", "DWS", "dwsfunds" },
  { FALSE, FALSE, "Fidelity Direct", "FIDELITY_DIRECT", "fidelity_direct" },
  { FALSE, FALSE, "Motley Fool", "FOOL", "fool" },
  { FALSE, FALSE, "Fund Library", "FUNDLIBRARY", "fundlibrary" },
  { FALSE, FALSE, "TD Waterhouse Canada", "TDWATERHOUSE", "tdwaterhouse" },
  { FALSE, FALSE, "TIAA-CREF", "TIAACREF", "tiaacref" },
  { FALSE, FALSE, "T. Rowe Price", "TRPRICE_DIRECT", "troweprice_direct" }, /* Not Implemented */
  { FALSE, FALSE, "Trustnet", "TRUSTNET", "trustnet" },
  { FALSE, FALSE, "Union Investments", "UNIONFUNDS", "unionfunds" },
  { FALSE, FALSE, "Vanguard", "VANGUARD", "vanguard" },
  { FALSE, FALSE, "VWD", "VWD", "vwd" },
  { FALSE, FALSE, "Yahoo", "YAHOO", "yahoo" },
  { FALSE, FALSE, "Yahoo Asia", "YAHOO_ASIA", "yahoo_asia" },
  { FALSE, FALSE, "Yahoo Australia", "YAHOO_AUSTRALIA", "yahoo_australia" },
  { FALSE, FALSE, "Yahoo Europe", "YAHOO_EUROPE", "yahoo_europe" },
  { FALSE, FALSE, "Zuerich Investments", "ZIFUNDS", "zifunds" },
  { FALSE, TRUE,  N_("-- Multiple Sources --"), NULL, NULL },
  { FALSE, FALSE, "Asia (Yahoo, ...)", "ASIA", "asia" },
  { FALSE, FALSE, "Australia (ASX, Yahoo, ...)", "AUSTRALIA", "australia" },
  { FALSE, FALSE, "Canada (Yahoo, ...)", "CANADA", "canada" },
  { FALSE, FALSE, "Canada Mutual (Fund Library, ...)", "CANADAMUTUAL", "canadamutual" },
  { FALSE, FALSE, "Dutch (AEX, ...)", "DUTCH", "dutch" },
  { FALSE, FALSE, "Europe (Yahoo, ...)", "EUROPE", "europe" },
  { FALSE, FALSE, "Fidelity (Fidelity, ...)", "FIDELITY", "fidelity" },
  { FALSE, FALSE, "T. Rowe Price", "TRPRICE", "troweprice" },
  { FALSE, FALSE, "U.K. Unit Trusts", "UKUNITTRUSTS", "uk_unit_trusts" },
  { FALSE, FALSE, "USA (Yahoo, Fool ...)", "USA", "usa" },
};

/********************************************************************\
 * gnc_color_deficits                                               *
 *   return a boolean value indicating whether deficit quantities   *
 *   should be displayed using the gnc_get_deficit_color().         *
 *                                                                  *
 * Args: none                                                       *
 * Returns: boolean deficit color indicator                         *
 \*******************************************************************/
gboolean
gnc_color_deficits (void)
{
  return gnc_lookup_boolean_option ("General",
                                    "Display negative amounts in red",
                                    TRUE);
}


/********************************************************************\
 * gnc_get_account_separator                                        *
 *   returns the current account separator character                *
 *                                                                  *
 * Args: none                                                       *
 * Returns: account separator character                             *
 \*******************************************************************/
char
gnc_get_account_separator (void)
{
  char separator = ':';
  char *string;

  string = gnc_lookup_multichoice_option("General",
                                         "Account Separator",
                                         "colon");

  if (safe_strcmp(string, "colon") == 0)
    separator = ':';
  else if (safe_strcmp(string, "slash") == 0)
    separator = '/';
  else if (safe_strcmp(string, "backslash") == 0)
    separator = '\\';
  else if (safe_strcmp(string, "dash") == 0)
    separator = '-';
  else if (safe_strcmp(string, "period") == 0)
    separator = '.';

  if (string != NULL)
    free(string);

  return separator;
}


static void
gnc_configure_reverse_balance (void)
{
  gchar *choice;
  gint i;

  for (i = 0; i < NUM_ACCOUNT_TYPES; i++)
    reverse_type[i] = FALSE;

  choice = gnc_lookup_multichoice_option ("Accounts",
                                          "Reversed-balance account types",
                                          "credit");

  if (safe_strcmp (choice, "income-expense") == 0)
  {
    reverse_type[INCOME]  = TRUE;
    reverse_type[EXPENSE] = TRUE;
  }
  else if (safe_strcmp (choice, "credit") == 0)
  {
    reverse_type[LIABILITY] = TRUE;
    reverse_type[PAYABLE]   = TRUE;
    reverse_type[EQUITY]    = TRUE;
    reverse_type[INCOME]    = TRUE;
    reverse_type[CREDIT]    = TRUE;
  }
  else if (safe_strcmp (choice, "none") == 0)
  {
  }
  else
  {
    PERR("bad value\n");

    reverse_type[INCOME]  = TRUE;
    reverse_type[EXPENSE] = TRUE;
  }

  if (choice != NULL)
    free (choice);
}

static void
gnc_configure_reverse_balance_cb (gpointer not_used)
{
  gnc_configure_reverse_balance ();
  gnc_gui_refresh_all ();
}

static void
gnc_reverse_balance_init (void)
{
  gnc_configure_reverse_balance ();

  reverse_balance_callback_id = 
    gnc_register_option_change_callback (gnc_configure_reverse_balance_cb,
                                         NULL, "Accounts",
                                         "Reversed-balance account types");

  reverse_balance_inited = (reverse_balance_callback_id != SCM_UNDEFINED);
}

gboolean
gnc_reverse_balance_type (GNCAccountType type)
{
  if ((type < 0) || (type >= NUM_ACCOUNT_TYPES))
    return FALSE;

  if (!reverse_balance_inited)
    gnc_reverse_balance_init ();

  return reverse_type[type];
}

gboolean
gnc_reverse_balance (Account *account)
{
  int type;

  if (account == NULL)
    return FALSE;

  type = xaccAccountGetType (account);
  if ((type < 0) || (type >= NUM_ACCOUNT_TYPES))
    return FALSE;

  if (!reverse_balance_inited)
    gnc_reverse_balance_init ();

  return reverse_type[type];
}


void
gnc_init_default_directory (char **dirname)
{
  if (*dirname == NULL)
    *dirname = g_strdup_printf("%s/", getenv("HOME"));
}

void
gnc_extract_directory (char **dirname, const char *filename)
{
  char *tmp;

  if (*dirname)
    free(*dirname);

  /* Parse out the directory. */
  if ((filename == NULL) || (rindex(filename, '/') == NULL)) {
    *dirname = NULL;
    return;
  }

  *dirname = g_strdup(filename);
  tmp = rindex(*dirname, '/');
  *(tmp+1) = '\0';
}

GNCBook *
gnc_get_current_book (void)
{
  return gnc_session_get_book (gnc_get_current_session ());
}

AccountGroup *
gnc_get_current_group (void)
{
  return gnc_book_get_group (gnc_get_current_book ());
}

gnc_commodity_table *
gnc_get_current_commodities (void)
{
  return gnc_book_get_commodity_table (gnc_get_current_book ());
}

const char *
gnc_ui_account_get_field_name (AccountFieldCode field)
{
  g_return_val_if_fail ((field >= 0) && (field < NUM_ACCOUNT_FIELDS), NULL);

  switch (field)
  {
    case ACCOUNT_TYPE :
      return _("Type");
      break;
    case ACCOUNT_NAME :
      return _("Account Name");
      break;
    case ACCOUNT_CODE :
      return _("Account Code");
      break;
    case ACCOUNT_DESCRIPTION :
      return _("Description");
      break;
    case ACCOUNT_NOTES :
      return _("Notes");
      break;
    case ACCOUNT_COMMODITY :
      return _("Commodity");
      break;
    case ACCOUNT_BALANCE :
      return _("Balance");
      break;
    case ACCOUNT_BALANCE_REPORT :
      return _("Balance");
      break;
    case ACCOUNT_TOTAL :
      return _("Total");
      break;
    case ACCOUNT_TOTAL_REPORT :
      return _("Total");
      break;
    case ACCOUNT_TAX_INFO :
      return _("Tax Info");
    default:
      break;
  }

  return NULL;
}


static gnc_numeric
gnc_account_get_balance_in_currency (Account *account,
                                     gnc_commodity *currency)
{
  GNCBook *book;
  GNCPriceDB *pdb;
  GNCPrice *price;
  gnc_numeric balance;
  gnc_commodity *commodity;

  if (!account || !currency)
    return gnc_numeric_zero ();

  balance = xaccAccountGetBalance (account);
  commodity = xaccAccountGetCommodity (account);

  if (gnc_numeric_zero_p (balance) ||
      gnc_commodity_equiv (currency, commodity))
    return balance;

  book = xaccGroupGetBook (xaccAccountGetRoot (account));
  g_return_val_if_fail (book != NULL, gnc_numeric_zero ());

  pdb = gnc_book_get_pricedb (book);

  price = gnc_pricedb_lookup_latest (pdb, commodity, currency);
  if (!price)
    return gnc_numeric_zero ();

  balance = gnc_numeric_mul (balance, gnc_price_get_value (price),
                             gnc_commodity_get_fraction (currency),
                             GNC_RND_ROUND);

  gnc_price_unref (price);

  return balance;
}


gnc_numeric
gnc_ui_convert_balance_to_currency(gnc_numeric balance, gnc_commodity *balance_currency, gnc_commodity *currency)
{
  GNCBook *book;
  GNCPriceDB *pdb;
  GNCPrice *price;

  if (gnc_numeric_zero_p (balance) ||
      gnc_commodity_equiv (currency, balance_currency))
    return balance;

  book = gnc_get_current_book ();
  pdb = gnc_book_get_pricedb (book);

  price = gnc_pricedb_lookup_latest (pdb, balance_currency, currency);
  if (!price)
    return gnc_numeric_zero ();

  balance = gnc_numeric_mul (balance, gnc_price_get_value (price),
                             gnc_commodity_get_fraction (currency),
                             GNC_RND_ROUND);

  gnc_price_unref (price);

  return balance;
}

static gnc_numeric
gnc_account_get_reconciled_balance_in_currency (Account *account,
                                                gnc_commodity *currency)
{
  gnc_numeric balance;
  gnc_commodity *balance_currency;

  if (!account || !currency)
    return gnc_numeric_zero ();

  balance = xaccAccountGetReconciledBalance (account);
  balance_currency = xaccAccountGetCommodity (account);

  return gnc_ui_convert_balance_to_currency (balance, balance_currency,
                                             currency);
}

typedef struct
{
  gnc_commodity *currency;
  gnc_numeric balance;
} CurrencyBalance;


static gpointer
balance_helper (Account *account, gpointer data)
{
  CurrencyBalance *cb = data;
  gnc_numeric balance;

  if (!cb->currency)
    return NULL;

  balance = gnc_account_get_balance_in_currency (account, cb->currency);

  cb->balance = gnc_numeric_add (cb->balance, balance,
                                 gnc_commodity_get_fraction (cb->currency),
                                 GNC_RND_ROUND);

  return NULL;
}

static gpointer
reconciled_balance_helper (Account *account, gpointer data)
{
  CurrencyBalance *cb = data;
  gnc_numeric balance;

  balance = gnc_account_get_reconciled_balance_in_currency (account, cb->currency);

  cb->balance = gnc_numeric_add (cb->balance, balance,
                                 gnc_commodity_get_fraction (cb->currency),
                                 GNC_RND_ROUND);

  return NULL;
}

gnc_numeric
gnc_ui_account_get_balance (Account *account, gboolean include_children)
{
  gnc_numeric balance;
  gnc_commodity *commodity;

  if (account == NULL)
    return gnc_numeric_zero ();

  commodity = xaccAccountGetCommodity (account);

  balance = gnc_account_get_balance_in_currency (account, commodity);

  if (include_children)
  {
    AccountGroup *children;
    CurrencyBalance cb = { commodity, balance };

    children = xaccAccountGetChildren (account);

    xaccGroupForEachAccount (children, balance_helper, &cb, TRUE);

    balance = cb.balance;
  }

  /* reverse sign if needed */
  if (gnc_reverse_balance (account))
    balance = gnc_numeric_neg (balance);

  return balance;
}


static char *
gnc_ui_account_get_tax_info_string (Account *account)
{
  static SCM get_form = SCM_UNDEFINED;
  static SCM get_desc = SCM_UNDEFINED;

  GNCAccountType atype;
  const char *code;
  SCM category;
  SCM code_scm;
  char *result;
  char *form;
  char *desc;
  SCM scm;

  if (get_form == SCM_UNDEFINED)
  {
    GNCModule module;

    module = gnc_module_load ("gnucash/tax/us", 0);

    g_return_val_if_fail (module, NULL);

    get_form = scm_c_eval_string ("(false-if-exception gnc:txf-get-form)");
    get_desc = scm_c_eval_string ("(false-if-exception gnc:txf-get-description)");
  }

  g_return_val_if_fail (SCM_PROCEDUREP (get_form), NULL);
  g_return_val_if_fail (SCM_PROCEDUREP (get_desc), NULL);

  if (!account)
    return NULL;

  if (!xaccAccountGetTaxRelated (account))
    return NULL;

  atype = xaccAccountGetType (account);
  if (atype != INCOME && atype != EXPENSE)
    return NULL;

  code = xaccAccountGetTaxUSCode (account);
  if (!code)
    return NULL;

  category = scm_c_eval_string (atype == INCOME ?
				"txf-income-categories" :
				"txf-expense-categories");

  code_scm = scm_str2symbol (code);

  scm = scm_call_2 (get_form, category, code_scm);
  if (!SCM_STRINGP (scm))
    return NULL;

  form = gh_scm2newstr (scm, NULL);
  if (!form)
    return NULL;

  scm = scm_call_2 (get_desc, category, code_scm);
  if (!SCM_STRINGP (scm))
  {
    free (form);
    return NULL;
  }

  desc = gh_scm2newstr (scm, NULL);
  if (!desc)
  {
    free (form);
    return NULL;
  }

  result = g_strdup_printf ("%s %s", form, desc);

  free (form);
  free (desc);

  return result;
}


gnc_numeric
gnc_ui_account_get_reconciled_balance (Account *account,
                                       gboolean include_children)
{
  gnc_numeric balance;
  gnc_commodity *currency;

  if (account == NULL)
    return gnc_numeric_zero ();

  currency = xaccAccountGetCommodity (account);

  balance = gnc_account_get_reconciled_balance_in_currency (account, currency);

  if (include_children)
  {
    AccountGroup *children;
    CurrencyBalance cb = { currency, balance };

    children = xaccAccountGetChildren (account);

    xaccGroupForEachAccount (children, reconciled_balance_helper, &cb, TRUE);

    balance = cb.balance;
  }

  /* reverse sign if needed */
  if (gnc_reverse_balance (account))
    balance = gnc_numeric_neg (balance);

  return balance;
}

gnc_numeric
gnc_ui_account_get_balance_as_of_date (Account *account, time_t date,
                                       gboolean include_children)
{
  gnc_numeric balance;
  gnc_commodity *currency;

  if (account == NULL)
    return gnc_numeric_zero ();

  currency = xaccAccountGetCommodity (account);
  balance = xaccAccountGetBalanceAsOfDate (account, date);

  if (include_children)
  {
    AccountGroup *children_group;
    GList *children, *node;

    children_group = xaccAccountGetChildren (account);
    children = xaccGroupGetSubAccounts (children_group);

    for (node = children; node; node = node->next)
    {
      Account *child;
      gnc_commodity *child_currency;
      gnc_numeric child_balance;

      child = node->data;
      child_currency = xaccAccountGetCommodity (child);
      child_balance = xaccAccountGetBalanceAsOfDate (child, date);
      child_balance = gnc_ui_convert_balance_to_currency
        (child_balance, child_currency, currency);
      balance = gnc_numeric_add_fixed (balance, child_balance);
    }
  }

  /* reverse sign if needed */
  if (gnc_reverse_balance (account))
    balance = gnc_numeric_neg (balance);

  return balance;
}

char *
gnc_ui_account_get_field_value_string (Account *account,
                                       AccountFieldCode field)
{
  g_return_val_if_fail ((field >= 0) && (field < NUM_ACCOUNT_FIELDS), NULL);

  if (account == NULL)
    return NULL;

  switch (field)
  {
    case ACCOUNT_TYPE :
      return g_strdup (xaccAccountGetTypeStr(xaccAccountGetType(account)));

    case ACCOUNT_NAME :
      return g_strdup (xaccAccountGetName(account));

    case ACCOUNT_CODE :
      return g_strdup (xaccAccountGetCode(account));

    case ACCOUNT_DESCRIPTION :
      return g_strdup (xaccAccountGetDescription(account));

    case ACCOUNT_NOTES :
      return g_strdup (xaccAccountGetNotes(account));

    case ACCOUNT_COMMODITY :
      return
        g_strdup
        (gnc_commodity_get_printname(xaccAccountGetCommodity(account)));

    case ACCOUNT_BALANCE :
      {
        gnc_numeric balance = gnc_ui_account_get_balance(account, FALSE);

        return g_strdup
          (xaccPrintAmount (balance, gnc_account_print_info (account, TRUE)));
      }

    case ACCOUNT_BALANCE_REPORT :
      {
	gnc_commodity * commodity = xaccAccountGetCommodity(account);
        gnc_commodity * report_commodity = gnc_default_report_currency();
        gnc_numeric balance = gnc_ui_account_get_balance(account, FALSE);

	gnc_numeric report_balance = gnc_ui_convert_balance_to_currency(balance, commodity, 
                                                                        report_commodity);

        return g_strdup
          (xaccPrintAmount(report_balance,
                           gnc_commodity_print_info (report_commodity, TRUE)));
      }

    case ACCOUNT_TOTAL :
      {
	gnc_numeric balance = gnc_ui_account_get_balance(account, TRUE);

        return g_strdup
          (xaccPrintAmount(balance, gnc_account_print_info (account, TRUE)));
      }

    case ACCOUNT_TOTAL_REPORT :
      {
	gnc_commodity * commodity = xaccAccountGetCommodity(account);
        gnc_commodity * report_commodity = gnc_default_report_currency();
	gnc_numeric balance = gnc_ui_account_get_balance(account, TRUE);

	gnc_numeric report_balance = gnc_ui_convert_balance_to_currency(balance, commodity, 
                                                                        report_commodity);

	return g_strdup
          (xaccPrintAmount(report_balance,
                           gnc_commodity_print_info (report_commodity, TRUE)));
      }

    case ACCOUNT_TAX_INFO:
      return gnc_ui_account_get_tax_info_string (account);

    default:
      break;
  }

  return NULL;
}


/********************************************************************\
 * gnc_get_reconcile_str                                            *
 *   return the i18n'd string for the given reconciled flag         *
 *                                                                  *
 * Args: reconciled_flag - the flag to stringize                    *
 * Returns: the i18n'd reconciled string                            *
\********************************************************************/
const char *
gnc_get_reconcile_str (char reconciled_flag)
{
  switch (reconciled_flag)
  {
    /* Translators: For the following strings, the single letters
       after the colon are abbreviations of the word before the
       colon. Please only translate the letter *after* the colon. */
    case NREC: return _("not cleared:n") + 12;
      /* Please only translate the letter *after* the colon. */
    case CREC: return _("cleared:c") + 8;
      /* Please only translate the letter *after* the colon. */
    case YREC: return _("reconciled:y") + 11;
      /* Please only translate the letter *after* the colon. */
    case FREC: return _("frozen:f") + 7;
      /* Please only translate the letter *after* the colon. */
    case VREC: return _("void:v") + 5;
    default:
      PERR("Bad reconciled flag\n");
      return NULL;
  }
}

/********************************************************************\
 * gnc_get_reconcile_valid_flags                                    *
 *   return a string containing the list of reconciled flags        *
 *                                                                  *
 * Returns: the i18n'd reconciled flags string                      *
\********************************************************************/
const char *
gnc_get_reconcile_valid_flags (void)
{
  static const char flags[] = { NREC, CREC, YREC, FREC, VREC, 0 };
  return flags;
}

/********************************************************************\
 * gnc_get_reconcile_flag_order                                     *
 *   return a string containing the reconciled-flag change order    *
 *                                                                  *
 * Args: reconciled_flag - the flag to stringize                    *
 * Returns: the i18n'd reconciled string                            *
\********************************************************************/
const char *
gnc_get_reconcile_flag_order (void)
{
  static const char flags[] = { NREC, CREC, 0 };
  return flags;
}


/* Get the full name of a quote source */
const char *
gnc_price_source_enum2name (PriceSourceCode source)
{
  if (source >= NUM_SOURCES) {
    PWARN("Unknown source %d", source);
    return NULL;
  }
  return quote_sources[source].user_name;
}

/* Get the codename string of a quote source */
const char *
gnc_price_source_enum2internal (PriceSourceCode source)
{
  if (source >= NUM_SOURCES) {
    PWARN("Unknown source %d", source);
    return NULL;
  }
  return quote_sources[source].internal_name;
}

/* Get the codename string of a source */
PriceSourceCode
gnc_price_source_internal2enum (const char * internal_name)
{
  gint i;

  if (internal_name == NULL)
    return SOURCE_NONE;

  if (safe_strcmp(internal_name, "") == 0)
    return SOURCE_NONE;

  for (i = 1; i < NUM_SOURCES; i++)
    if (safe_strcmp(internal_name, quote_sources[i].internal_name) == 0)
      return i;

  PWARN("Unknown source %s", internal_name);
  return SOURCE_NONE;
}

/* Get the codename string of a source */
PriceSourceCode
gnc_price_source_fq2enum (const char * fq_name)
{
  gint i;

  if (fq_name == NULL)
    return SOURCE_NONE;

  if (safe_strcmp(fq_name, "") == 0)
    return SOURCE_NONE;

  for (i = 1; i < NUM_SOURCES; i++)
    if (safe_strcmp(fq_name, quote_sources[i].fq_name) == 0)
      return i;

//  NYSE and Nasdaq have deliberately been left out. Don't always print them.
//  PWARN("Unknown source %s", fq_name);
  return SOURCE_NONE;
}

/* Get the codename string of a source */
const char *
gnc_price_source_internal2fq (const char * codename)
{
  gint i;

  if (codename == NULL)
    return NULL;

  if (safe_strcmp(codename, "") == 0)
    return NULL;

  if (safe_strcmp(codename, "CURRENCY") == 0)
    return "currency";

  for (i = 1; i < NUM_SOURCES; i++)
    if (safe_strcmp(codename, quote_sources[i].internal_name) == 0)
      return quote_sources[i].fq_name;

  PWARN("Unknown source %s", codename);
  return NULL;
}

/* Get whether or not a quote source should be sensitive in the menu */
void
gnc_price_source_set_fq_installed (GList *sources_list)
{
  GList *node;
  PriceSourceCode code;
  char *source;

  if (!sources_list)
    return;

  fq_is_installed = TRUE;
  for (node = sources_list; node; node = node->next) {
    source = node->data;
    code = gnc_price_source_fq2enum (source);
    if ((code != SOURCE_NONE) && (code < NUM_SOURCES)) {
      quote_sources[code].supported = TRUE;
    }
  }
}

gboolean
gnc_price_source_have_fq (void)
{
  return fq_is_installed;
}

/* Get whether or not a quote source should be sensitive in the menu */
gboolean
gnc_price_source_sensitive (PriceSourceCode source)
{
  if (source >= NUM_SOURCES) {
    PWARN("Unknown source");
    return FALSE;
  }

  return quote_sources[source].supported;
}

static const char *
equity_base_name (GNCEquityType equity_type)
{
  switch (equity_type)
  {
    case EQUITY_OPENING_BALANCE:
      return N_("Opening Balances");

    case EQUITY_RETAINED_EARNINGS:
      return N_("Retained Earnings");

    default:
      return NULL;
  }
}

Account *
gnc_find_or_create_equity_account (AccountGroup *group,
                                   GNCEquityType equity_type,
                                   gnc_commodity *currency,
                                   GNCBook *book)
{
  Account *parent;
  Account *account;
  gboolean name_exists;
  gboolean base_name_exists;
  const char *base_name;
  char *name;

  g_return_val_if_fail (equity_type >= 0, NULL);
  g_return_val_if_fail (equity_type < NUM_EQUITY_TYPES, NULL);
  g_return_val_if_fail (currency != NULL, NULL);
  g_return_val_if_fail (group != NULL, NULL);

  base_name = equity_base_name (equity_type);

  account = xaccGetAccountFromName (group, base_name);
  if (account && xaccAccountGetType (account) != EQUITY)
    account = NULL;

  if (!account)
  {
    base_name = _(base_name);

    account = xaccGetAccountFromName (group, base_name);
    if (account && xaccAccountGetType (account) != EQUITY)
      account = NULL;
  }

  base_name_exists = (account != NULL);

  if (account &&
      gnc_commodity_equiv (currency, xaccAccountGetCommodity (account)))
    return account;

  name = g_strconcat (base_name, " - ",
                      gnc_commodity_get_mnemonic (currency), NULL);
  account = xaccGetAccountFromName (group, name);
  if (account && xaccAccountGetType (account) != EQUITY)
    account = NULL;

  name_exists = (account != NULL);

  if (account &&
      gnc_commodity_equiv (currency, xaccAccountGetCommodity (account)))
    return account;

  /* Couldn't find one, so create it */
  if (name_exists && base_name_exists)
  {
    PWARN ("equity account with unexpected currency");
    g_free (name);
    return NULL;
  }

  if (!base_name_exists &&
      gnc_commodity_equiv (currency, gnc_default_currency ()))
  {
    g_free (name);
    name = g_strdup (base_name);
  }

  parent = xaccGetAccountFromName (group, _("Equity"));
  if (parent && xaccAccountGetType (parent) != EQUITY)
    parent = NULL;

  account = xaccMallocAccount (book);

  xaccAccountBeginEdit (account);

  xaccAccountSetName (account, name);
  xaccAccountSetType (account, EQUITY);
  xaccAccountSetCommodity (account, currency);

  if (parent)
  {
    xaccAccountBeginEdit (parent);
    xaccAccountInsertSubAccount (parent, account);
    xaccAccountCommitEdit (parent);
  }
  else
    xaccGroupInsertAccount (group, account);

  xaccAccountCommitEdit (account);

  g_free (name);

  return account;
}

gboolean
gnc_account_create_opening_balance (Account *account,
                                    gnc_numeric balance,
                                    time_t date,
                                    GNCBook *book)
{
  Account *equity_account;
  Transaction *trans;
  Split *split;

  if (gnc_numeric_zero_p (balance))
    return TRUE;

  g_return_val_if_fail (account != NULL, FALSE);

  equity_account =
    gnc_find_or_create_equity_account (xaccAccountGetRoot (account),
                                       EQUITY_OPENING_BALANCE,
                                       xaccAccountGetCommodity (account),
                                       book);
  if (!equity_account)
    return FALSE;

  xaccAccountBeginEdit (account);
  xaccAccountBeginEdit (equity_account);

  trans = xaccMallocTransaction (book);

  xaccTransBeginEdit (trans);

  xaccTransSetCurrency (trans, xaccAccountGetCommodity (account));
  xaccTransSetDateSecs (trans, date);
  xaccTransSetDescription (trans, _("Opening Balance"));

  split = xaccMallocSplit (book);

  xaccTransAppendSplit (trans, split);
  xaccAccountInsertSplit (account, split);

  xaccSplitSetAmount (split, balance);
  xaccSplitSetValue (split, balance);

  balance = gnc_numeric_neg (balance);

  split = xaccMallocSplit (book);

  xaccTransAppendSplit (trans, split);
  xaccAccountInsertSplit (equity_account, split);

  xaccSplitSetAmount (split, balance);
  xaccSplitSetValue (split, balance);

  xaccTransCommitEdit (trans);
  xaccAccountCommitEdit (equity_account);
  xaccAccountCommitEdit (account);

  return TRUE;
}

char *
gnc_account_get_full_name (Account *account)
{
  if (!account) return NULL;

  return xaccAccountGetFullName (account, gnc_get_account_separator ());
}

static void
gnc_lconv_set (char **p_value, char *default_value)
{
  char *value = *p_value;

  if ((value == NULL) || (value[0] == 0))
    *p_value = default_value;

  *p_value = g_strdup (*p_value);
}

static void
gnc_lconv_set_char (char *p_value, char default_value)
{
  if ((p_value != NULL) && (*p_value == CHAR_MAX))
    *p_value = default_value;
}

struct lconv *
gnc_localeconv (void)
{
  static struct lconv lc;
  static gboolean lc_set = FALSE;

  if (lc_set)
    return &lc;

  lc = *localeconv();

  gnc_lconv_set(&lc.decimal_point, ".");
  gnc_lconv_set(&lc.thousands_sep, ",");
  gnc_lconv_set(&lc.grouping, "\003");
  gnc_lconv_set(&lc.int_curr_symbol, "USD ");
  gnc_lconv_set(&lc.currency_symbol, "$");
  gnc_lconv_set(&lc.mon_decimal_point, ".");
  gnc_lconv_set(&lc.mon_thousands_sep, ",");
  gnc_lconv_set(&lc.mon_grouping, "\003");
  gnc_lconv_set(&lc.negative_sign, "-");

  gnc_lconv_set_char(&lc.frac_digits, 2);
  gnc_lconv_set_char(&lc.int_frac_digits, 2);
  gnc_lconv_set_char(&lc.p_cs_precedes, 1);
  gnc_lconv_set_char(&lc.p_sep_by_space, 0);
  gnc_lconv_set_char(&lc.n_cs_precedes, 1);
  gnc_lconv_set_char(&lc.n_sep_by_space, 0);
  gnc_lconv_set_char(&lc.p_sign_posn, 1);
  gnc_lconv_set_char(&lc.n_sign_posn, 1);

  lc_set = TRUE;

  return &lc;
}

const char *
gnc_locale_default_iso_currency_code (void)
{
  static char *code = NULL;
  struct lconv *lc;

  if (code)
    return code;

  lc = gnc_localeconv ();

  code = g_strdup (lc->int_curr_symbol);

  /* The int_curr_symbol includes a space at the end! Note: you
   * can't just change "USD " to "USD" in gnc_localeconv, because
   * that is only used if int_curr_symbol was not defined in the
   * current locale. If it was, it will have the space! */
  g_strstrip (code);

  return code;
}

gnc_commodity *
gnc_locale_default_currency_nodefault (void)
{
  gnc_commodity * currency;
  gnc_commodity_table *table;
  const char *code;

  table = gnc_get_current_commodities ();
  code = gnc_locale_default_iso_currency_code ();

  currency = gnc_commodity_table_lookup (table, GNC_COMMODITY_NS_ISO, code);

  return (currency ? currency : NULL);
}

gnc_commodity *
gnc_locale_default_currency (void)
{
  gnc_commodity * currency = gnc_locale_default_currency_nodefault ();

  return (currency ? currency :
	  gnc_commodity_table_lookup (gnc_get_current_commodities (), 
				      GNC_COMMODITY_NS_ISO, "USD"));
}


/* Return the number of decimal places for this locale. */
int 
gnc_locale_decimal_places (void)
{
  static gboolean got_it = FALSE;
  static int places;
  struct lconv *lc;

  if (got_it)
    return places;

  lc = gnc_localeconv();
  places = lc->frac_digits;

  /* frac_digits is already initialized by gnc_localeconv, hopefully
   * to a reasonable default. */
  got_it = TRUE;

  return places;
}


static GList *locale_stack = NULL;

void
gnc_push_locale (const char *locale)
{
  char *saved_locale;

  g_return_if_fail (locale != NULL);

  saved_locale = g_strdup (setlocale (LC_ALL, NULL));
  locale_stack = g_list_prepend (locale_stack, saved_locale);
  setlocale (LC_ALL, locale);
}

void
gnc_pop_locale (void)
{
  char *saved_locale;
  GList *node;

  g_return_if_fail (locale_stack != NULL);

  node = locale_stack;
  saved_locale = node->data;

  setlocale (LC_ALL, saved_locale);

  locale_stack = g_list_remove_link (locale_stack, node);
  g_list_free_1 (node);
  g_free (saved_locale);
}

GNCPrintAmountInfo
gnc_default_print_info (gboolean use_symbol)
{
  static GNCPrintAmountInfo info;
  static gboolean got_it = FALSE;
  struct lconv *lc;

  /* These must be updated each time. */
  info.use_symbol = use_symbol ? 1 : 0;
  info.commodity = gnc_default_currency ();

  if (got_it)
    return info;

  lc = gnc_localeconv ();

  info.max_decimal_places = lc->frac_digits;
  info.min_decimal_places = lc->frac_digits;

  info.use_separators = 1;
  info.use_locale = 1;
  info.monetary = 1;
  info.force_fit = 0;
  info.round = 0;

  got_it = TRUE;

  return info;
}

static gboolean
is_decimal_fraction (int fraction, guint8 *max_decimal_places_p)
{
  guint8 max_decimal_places = 0;

  if (fraction <= 0)
    return FALSE;

  while (fraction != 1)
  {
    if (fraction % 10 != 0)
      return FALSE;

    fraction = fraction / 10;
    max_decimal_places += 1;
  }

  if (max_decimal_places_p)
    *max_decimal_places_p = max_decimal_places;

  return TRUE;
}

GNCPrintAmountInfo
gnc_commodity_print_info (const gnc_commodity *commodity,
                          gboolean use_symbol)
{
  GNCPrintAmountInfo info;
  gboolean is_iso;

  if (commodity == NULL)
    return gnc_default_print_info (use_symbol);

  info.commodity = commodity;

  is_iso = (safe_strcmp (gnc_commodity_get_namespace (commodity),
                         GNC_COMMODITY_NS_ISO) == 0);

  if (is_decimal_fraction (gnc_commodity_get_fraction (commodity),
                           &info.max_decimal_places))
  {
    if (is_iso)
      info.min_decimal_places = info.max_decimal_places;
    else
      info.min_decimal_places = 0;
  }
  else
    info.max_decimal_places = info.min_decimal_places = 0;

  info.use_separators = 1;
  info.use_symbol = use_symbol ? 1 : 0;
  info.use_locale = is_iso ? 1 : 0;
  info.monetary = 1;
  info.force_fit = 0;
  info.round = 0;

  return info;
}

static GNCPrintAmountInfo
gnc_account_print_info_helper(Account *account, gboolean use_symbol,
                              gnc_commodity * (*efffunc)(Account *),
                              int (*scufunc)(Account*))
{
  GNCPrintAmountInfo info;
  gboolean is_iso;
  int scu;

  if (account == NULL)
    return gnc_default_print_info (use_symbol);

  info.commodity = efffunc (account);

  is_iso = (safe_strcmp (gnc_commodity_get_namespace (info.commodity),
                         GNC_COMMODITY_NS_ISO) == 0);

  scu = scufunc (account);

  if (is_decimal_fraction (scu, &info.max_decimal_places))
  {
    if (is_iso)
      info.min_decimal_places = info.max_decimal_places;
    else
      info.min_decimal_places = 0;
  }
  else
    info.max_decimal_places = info.min_decimal_places = 0;

  info.use_separators = 1;
  info.use_symbol = use_symbol ? 1 : 0;
  info.use_locale = is_iso ? 1 : 0;
  info.monetary = 1;
  info.force_fit = 0;
  info.round = 0;

  return info;
}

GNCPrintAmountInfo
gnc_account_print_info (Account *account, gboolean use_symbol)
{
    return gnc_account_print_info_helper(account, use_symbol,
                                         xaccAccountGetCommodity,
                                         xaccAccountGetCommoditySCU);
}

GNCPrintAmountInfo
gnc_split_amount_print_info (Split *split, gboolean use_symbol)
{
  if (!split)
  {
    GNCPrintAmountInfo info = gnc_default_share_print_info ();
    info.use_symbol = use_symbol;
    return info;
  }

  return gnc_account_print_info (xaccSplitGetAccount (split), use_symbol);
}

GNCPrintAmountInfo
gnc_split_value_print_info (Split *split, gboolean use_symbol)
{
  Transaction *trans;

  if (!split) return gnc_default_print_info (use_symbol);

  trans = xaccSplitGetParent (split);

  return gnc_commodity_print_info (xaccTransGetCurrency (trans), use_symbol);
}

static GNCPrintAmountInfo
gnc_default_print_info_helper (int decplaces)
{
    GNCPrintAmountInfo info;

    info.commodity = NULL;

    info.max_decimal_places = decplaces;
    info.min_decimal_places = 0;

    info.use_separators = 1;
    info.use_symbol = 0;
    info.use_locale = 1;
    info.monetary = 1;
    info.force_fit = 0;
    info.round = 0;

    return info;
}

GNCPrintAmountInfo
gnc_default_share_print_info (void)
{
  static GNCPrintAmountInfo info;
  static gboolean got_it = FALSE;

  if (!got_it)
  {
      info = gnc_default_print_info_helper (5);
      got_it = TRUE;
  }

  return info;
}

GNCPrintAmountInfo
gnc_share_print_info_places (int decplaces)
{
  GNCPrintAmountInfo info;

  info = gnc_default_share_print_info ();
  info.max_decimal_places = decplaces;
  info.min_decimal_places = decplaces;
  info.force_fit = 1;
  info.round = 1;
  return info;
}

GNCPrintAmountInfo
gnc_default_price_print_info (void)
{
  static GNCPrintAmountInfo info;
  static gboolean got_it = FALSE;

  if (!got_it)
  {
    info = gnc_default_print_info_helper (6);
    got_it = TRUE;
  }

  return info;
}

GNCPrintAmountInfo
gnc_integral_print_info (void)
{
  static GNCPrintAmountInfo info;
  static gboolean got_it = FALSE;

  if (!got_it)
  {
    info = gnc_default_print_info_helper (0);
    got_it = TRUE;
  }

  return info;
}

/* Utility function for printing non-negative amounts */
static int
PrintAmountInternal(char *buf, gnc_numeric val, const GNCPrintAmountInfo *info)
{
  struct lconv *lc = gnc_localeconv();
  int num_whole_digits;
  char temp_buf[64];
  gnc_numeric whole, rounding;
  int min_dp, max_dp;

  g_return_val_if_fail (info != NULL, 0);

  if (gnc_numeric_check (val))
  {
    PWARN ("Bad numeric.");
    *buf = '\0';
    return 0;
  }

  /* print the absolute value */
  val = gnc_numeric_abs (val);

  /* Force at least auto_decimal_places zeros */
  if (auto_decimal_enabled) {
    min_dp = MAX(auto_decimal_places, info->min_decimal_places);
    max_dp = MAX(auto_decimal_places, info->max_decimal_places);
  } else {
    min_dp = info->min_decimal_places;
    max_dp = info->max_decimal_places;
  }

  /* Don to limit the number of decimal places _UNLESS_ force_fit is
   * true. */
  if (!info->force_fit)
    max_dp = 99;

  /* rounding? -- can only ROUND if force_fit is also true */
  if (info->round && info->force_fit) {
    rounding.num = 5; /* Limit the denom to 10^13 ~= 2^44, leaving max at ~524288 */
    rounding.denom = pow(10, max_dp + 1);
    val = gnc_numeric_add(val, rounding, GNC_DENOM_AUTO, GNC_DENOM_LCD);
  }

  /* calculate the integer part and the remainder */
  whole = gnc_numeric_create (val.num / val.denom, 1);
  val = gnc_numeric_sub (val, whole, val.denom, GNC_RND_NEVER);
  if (gnc_numeric_check (val))
  {
    PWARN ("Problem with remainder.");
    *buf = '\0';
    return 0;
  }

  /* print the integer part without separators */
  sprintf(temp_buf, "%lld", (long long int) whole.num);
  num_whole_digits = strlen (temp_buf);

  if (!info->use_separators)
    strcpy (buf, temp_buf);
  else
  {
    int group_count;
    char separator;
    char *temp_ptr;
    char *buf_ptr;
    char *group;

    if (info->monetary)
    {
      separator = lc->mon_thousands_sep[0];
      group = lc->mon_grouping;
    }
    else
    {
      separator = lc->thousands_sep[0];
      group = lc->grouping;
    }

    buf_ptr = buf;
    temp_ptr = &temp_buf[num_whole_digits - 1];
    group_count = 0;

    while (temp_ptr != temp_buf)
    {
      *buf_ptr++ = *temp_ptr--;

      if (*group != CHAR_MAX)
      {
        group_count++;

        if (group_count == *group)
        {
          *buf_ptr++ = separator;
          group_count = 0;

          /* Peek ahead at the next group code */
          switch (group[1])
          {
            /* A null char means repeat the last group indefinitely */
            case '\0':
              break;
            /* CHAR_MAX means no more grouping allowed */
            case CHAR_MAX:
              /* fall through */
            /* Anything else means another group size */
            default:
              group++;
              break;
          }
        }
      }
    }

    /* We built the string backwards, now reverse */
    *buf_ptr++ = *temp_ptr;
    *buf_ptr = '\0';
    g_strreverse(buf);
  } /* endif */

  /* at this point, buf contains the whole part of the number */

  /* If it's not decimal, print the fraction as an expression */
  if (!is_decimal_fraction (val.denom, NULL))
  {
    if (!gnc_numeric_zero_p (val))
    {
      val = gnc_numeric_reduce (val);

      sprintf (temp_buf, " + %lld / %lld",
               (long long int) val.num,
               (long long int) val.denom);

      strcat (buf, temp_buf);
    }
  }
  else
  {
    guint8 num_decimal_places = 0;
    char *temp_ptr = temp_buf;

    *temp_ptr++ = info->monetary ?
      lc->mon_decimal_point[0] : lc->decimal_point[0];

    while (!gnc_numeric_zero_p (val) && (val.denom != 1) &&
	   (num_decimal_places < max_dp))
    {
      gint64 digit;

      val.denom = val.denom / 10;

      digit = val.num / val.denom;

      *temp_ptr++ = digit + '0';
      num_decimal_places++;

      val.num = val.num - (digit * val.denom);
    }

    while (num_decimal_places < min_dp)
    {
      *temp_ptr++ = '0';
      num_decimal_places++;
    }

    /* cap the end and move to the last character */
    *temp_ptr-- = '\0';

    /* Here we strip off trailing decimal zeros per the argument. */
    while (*temp_ptr == '0' && num_decimal_places > min_dp)
    {
      *temp_ptr-- = '\0';
      num_decimal_places--;
    }

    if (num_decimal_places > max_dp)
    {
      PWARN ("max_decimal_places too small; limit %d, value %s%s",
	     info->max_decimal_places, buf, temp_buf);
    }

    if (num_decimal_places > 0)
      strcat (buf, temp_buf);
  }

  return strlen(buf);
}

/**
 * @param bufp Should be at least 64 chars.
 **/
int
xaccSPrintAmount (char * bufp, gnc_numeric val, GNCPrintAmountInfo info)
{
   struct lconv *lc;

   char *orig_bufp = bufp;
   const char *currency_symbol;
   const char *sign;

   char cs_precedes;
   char sep_by_space;
   char sign_posn;

   gboolean print_sign = TRUE;
   gboolean is_shares = FALSE;

   if (!bufp)
     return 0;

   lc = gnc_localeconv();

   if (info.use_symbol)
   {
     /* There was a bug here: don't use gnc_locale_default_currency */
     if (gnc_commodity_equiv (info.commodity, 
			      gnc_locale_default_currency_nodefault ()))
     {
       currency_symbol = lc->currency_symbol;
     }
     else
     {
       if (info.commodity &&
           safe_strcmp (GNC_COMMODITY_NS_ISO,
                        gnc_commodity_get_namespace (info.commodity)) != 0)
         is_shares = TRUE;

       currency_symbol = gnc_commodity_get_mnemonic (info.commodity);
       info.use_locale = 0;
     }

     if (currency_symbol == NULL)
       currency_symbol = "";
   }
   else
     currency_symbol = NULL;

   if (!info.use_locale)
   {
     cs_precedes = is_shares ? 0 : 1;
     sep_by_space = 1;
   }
   else
   {
     if (gnc_numeric_negative_p (val))
     {
       cs_precedes  = lc->n_cs_precedes;
       sep_by_space = lc->n_sep_by_space;
     }
     else
     {
       cs_precedes  = lc->p_cs_precedes;
       sep_by_space = lc->p_sep_by_space;
     }
   }

   if (gnc_numeric_negative_p (val))
   {
     sign = lc->negative_sign;
     sign_posn = lc->n_sign_posn;
   }
   else
   {
     sign = lc->positive_sign;
     sign_posn = lc->p_sign_posn;
   }

   if (gnc_numeric_zero_p (val) || (sign == NULL) || (sign[0] == 0))
     print_sign = FALSE;

   /* See if we print sign now */
   if (print_sign && (sign_posn == 1))
     bufp = gnc_stpcpy(bufp, sign);

   /* Now see if we print currency */
   if (cs_precedes)
   {
     /* See if we print sign now */
     if (print_sign && (sign_posn == 3))
       bufp = gnc_stpcpy(bufp, sign);

     if (info.use_symbol)
     {
       bufp = gnc_stpcpy(bufp, currency_symbol);
       if (sep_by_space)
         bufp = gnc_stpcpy(bufp, " ");
     }

     /* See if we print sign now */
     if (print_sign && (sign_posn == 4))
       bufp = gnc_stpcpy(bufp, sign);
   }

   /* Now see if we print parentheses */
   if (print_sign && (sign_posn == 0))
     bufp = gnc_stpcpy(bufp, "(");

   /* Now print the value */
   bufp += PrintAmountInternal(bufp, val, &info);

   /* Now see if we print parentheses */
   if (print_sign && (sign_posn == 0))
     bufp = gnc_stpcpy(bufp, ")");

   /* Now see if we print currency */
   if (!cs_precedes)
   {
     /* See if we print sign now */
     if (print_sign && (sign_posn == 3))
       bufp = gnc_stpcpy(bufp, sign);

     if (info.use_symbol)
     {
       if (sep_by_space)
         bufp = gnc_stpcpy(bufp, " ");
       bufp = gnc_stpcpy(bufp, currency_symbol);
     }

     /* See if we print sign now */
     if (print_sign && (sign_posn == 4))
       bufp = gnc_stpcpy(bufp, sign);
   }

   /* See if we print sign now */
   if (print_sign && (sign_posn == 2))
     bufp = gnc_stpcpy(bufp, sign);

   /* return length of printed string */
   return (bufp - orig_bufp);
}

const char *
xaccPrintAmount (gnc_numeric val, GNCPrintAmountInfo info)
{
  /* hack alert -- this is not thread safe ... */
  static char buf[1024];

  xaccSPrintAmount (buf, val, info);

  /* its OK to return buf, since we declared it static */
  return buf;
}


/********************************************************************\
 * xaccParseAmount                                                  *
 *   parses amount strings using locale data                        *
 *                                                                  *
 * Args: in_str   -- pointer to string rep of num                   *
 *       monetary -- boolean indicating whether value is monetary   *
 *       result   -- pointer to result location, may be NULL        *
 *       endstr   -- used to store first digit not used in parsing  *
 * Return: gboolean -- TRUE if a number found and parsed            *
 *                     If FALSE, result is not changed              *
\********************************************************************/

/* Parsing state machine states */
typedef enum
{
  START_ST,       /* Parsing initial whitespace */
  NEG_ST,         /* Parsed a negative sign or a left paren */
  PRE_GROUP_ST,   /* Parsing digits before grouping and decimal characters */
  START_GROUP_ST, /* Start of a digit group encountered (possibly) */
  IN_GROUP_ST,    /* Within a digit group */
  FRAC_ST,        /* Parsing the fractional portion of a number */
  DONE_ST,        /* Finished, number is correct module grouping constraints */
  NO_NUM_ST       /* Finished, number was malformed */
} ParseState;

#define done_state(state) (((state) == DONE_ST) || ((state) == NO_NUM_ST))

G_INLINE_FUNC long long int multiplier (int num_decimals);

G_INLINE_FUNC long long int
multiplier (int num_decimals)
{
  switch (num_decimals)
  {
    case 8:
      return 100000000;
    case 7:
      return 10000000;
    case 6:
      return 1000000;
    case 5:
      return 100000;
    case 4:
      return 10000;
    case 3:
      return 1000;
    case 2:
      return 100;
    case 1:
      return 10;
    default:
      PERR("bad fraction length");
      g_assert_not_reached();
      break;
  }

  return 1;
}

gboolean
xaccParseAmount (const char * in_str, gboolean monetary, gnc_numeric *result,
                 char **endstr)
{
  struct lconv *lc = gnc_localeconv();
  gboolean is_negative;
  gboolean got_decimal;
  gboolean need_paren;
  GList * group_data;
  long long int numer;
  long long int denom;
  int group_count;

  ParseState state;

  char negative_sign;
  char decimal_point;
  char group_separator;

  const char *in;
  char *out_str;
  char *out;

  /* Initialize *endstr to in_str */
  if (endstr != NULL)
    *endstr = (char *) in_str;

  if (in_str == NULL)
    return FALSE;

  negative_sign = lc->negative_sign[0];
  if (monetary)
  {
    group_separator = lc->mon_thousands_sep[0];
    decimal_point = lc->mon_decimal_point[0];
  }
  else
  {
    group_separator = lc->thousands_sep[0];
    decimal_point = lc->decimal_point[0];
  }

  /* 'out_str' will be used to store digits for numeric conversion.
   * 'out' will be used to traverse out_str. */
  out = out_str = g_new(char, strlen(in_str) + 1);

  /* 'in' is used to traverse 'in_str'. */
  in = in_str;

  is_negative = FALSE;
  got_decimal = FALSE;
  need_paren = FALSE;
  group_data = NULL;
  group_count = 0;
  numer = 0;
  denom = 1;

  /* Initialize the state machine */
  state = START_ST;

  /* This while loop implements a state machine for parsing numbers. */
  while (TRUE)
  {
    ParseState next_state = state;

    /* Note we never need to check for then end of 'in_str' explicitly.
     * The 'else' clauses on all the state transitions will handle that. */
    switch (state)
    {
      /* START_ST means we have parsed 0 or more whitespace characters */
      case START_ST:
        if (isdigit(*in))
        {
          *out++ = *in; /* we record the digits themselves in out_str
                         * for later conversion by libc routines */
          next_state = PRE_GROUP_ST;
        }
        else if (*in == decimal_point)
        {
          next_state = FRAC_ST;
        }
        else if (isspace(*in))
        {
        }
        else if (*in == negative_sign)
        {
          is_negative = TRUE;
          next_state = NEG_ST;
        }
        else if (*in == '(')
        {
          is_negative = TRUE;
          need_paren = TRUE;
          next_state = NEG_ST;
        }
        else
        {
          next_state = NO_NUM_ST;
        }

        break;

      /* NEG_ST means we have just parsed a negative sign. For now,
       * we only recognize formats where the negative sign comes first. */
      case NEG_ST:
        if (isdigit(*in))
        {
          *out++ = *in;
          next_state = PRE_GROUP_ST;
        }
        else if (*in == decimal_point)
        {
          next_state = FRAC_ST;
        }
        else if (isspace(*in))
        {
        }
        else
        {
          next_state = NO_NUM_ST;
        }

        break;

      /* PRE_GROUP_ST means we have started parsing the number, but
       * have not encountered a decimal point or a grouping character. */
      case PRE_GROUP_ST:
        if (isdigit(*in))
        {
          *out++ = *in;
        }
        else if (*in == decimal_point)
        {
          next_state = FRAC_ST;
        }
        else if (*in == group_separator)
        {
          next_state = START_GROUP_ST;
        }
        else if (*in == ')' && need_paren)
        {
          next_state = DONE_ST;
          need_paren = FALSE;
        }
        else
        {
          next_state = DONE_ST;
        }

        break;

      /* START_GROUP_ST means we have just parsed a group character.
       * Note that group characters might be whitespace!!! In general,
       * if a decimal point or a group character is whitespace, we
       * try to interpret it in the fashion that will allow parsing
       * of the current number to continue. */
      case START_GROUP_ST:
        if (isdigit(*in))
        {
          *out++ = *in;
          group_count++; /* We record the number of digits
                          * in the group for later checking. */
          next_state = IN_GROUP_ST;
        }
        else if (*in == decimal_point)
        {
          /* If we now get a decimal point, and both the decimal
           * and the group separator are also whitespace, assume
           * the last group separator was actually whitespace and
           * stop parsing. Otherwise, there's a problem. */
          if (isspace(group_separator) && isspace(decimal_point))
            next_state = DONE_ST;
          else
            next_state = NO_NUM_ST;
        }
        else if (*in == ')' && need_paren)
        {
          if (isspace(group_separator))
          {
            next_state = DONE_ST;
            need_paren = FALSE;
          }
          else
            next_state = NO_NUM_ST;
        }
        else
        {
          /* If the last group separator is also whitespace,
           * assume it was intended as such and stop parsing.
           * Otherwise, there is a problem. */
          if (isspace(group_separator))
            next_state = DONE_ST;
          else
            next_state = NO_NUM_ST;
        }
        break;

      /* IN_GROUP_ST means we are in the middle of parsing
       * a group of digits. */
      case IN_GROUP_ST:
        if (isdigit(*in))
        {
          *out++ = *in;
          group_count++; /* We record the number of digits
                          * in the group for later checking. */
        }
        else if (*in == decimal_point)
        {
          next_state = FRAC_ST;
        }
        else if (*in == group_separator)
        {
          next_state = START_GROUP_ST;
        }
        else if (*in == ')' && need_paren)
        {
          next_state = DONE_ST;
          need_paren = FALSE;
        }
        else
        {
          next_state = DONE_ST;
        }

        break;

      /* FRAC_ST means we are now parsing fractional digits. */
      case FRAC_ST:
        if (isdigit(*in))
        {
          *out++ = *in;
        }
        else if (*in == decimal_point)
        {
          /* If a subsequent decimal point is also whitespace,
           * assume it was intended as such and stop parsing.
           * Otherwise, there is a problem. */
          if (isspace(decimal_point))
            next_state = DONE_ST;
          else
            next_state = NO_NUM_ST;
        }
        else if (*in == group_separator)
        {
          /* If a subsequent group separator is also whitespace,
           * assume it was intended as such and stop parsing.
           * Otherwise, there is a problem. */
          if (isspace(group_separator))
            next_state = DONE_ST;
          else
            next_state = NO_NUM_ST;
        }
        else if (*in == ')' && need_paren)
        {
          next_state = DONE_ST;
          need_paren = FALSE;
        }
        else
        {
          next_state = DONE_ST;
        }

        break;

      default:
        PERR("bad state");
        g_assert_not_reached();
        break;
    }

    /* If we're moving out of the IN_GROUP_ST, record data for the group */
    if ((state == IN_GROUP_ST) && (next_state != IN_GROUP_ST))
    {
      group_data = g_list_prepend(group_data, GINT_TO_POINTER(group_count));
      group_count = 0;
    }

    /* If we're moving into the FRAC_ST or out of the machine
     * without going through FRAC_ST, record the integral value. */
    if (((next_state == FRAC_ST) && (state != FRAC_ST)) ||
        ((next_state == DONE_ST) && !got_decimal))
    {
      *out = '\0';

      if (*out_str != '\0' && sscanf(out_str, GNC_SCANF_LLD, &numer) < 1)
      {
        next_state = NO_NUM_ST;
      }
      else if (next_state == FRAC_ST)
      {
        /* reset the out pointer to record the fraction */
        out = out_str;
        *out = '\0';

        got_decimal = TRUE;
      }
    }

    state = next_state;
    if (done_state (state))
      break;

    in++;
  }

  /* If there was an error, just quit */
  if (need_paren || (state == NO_NUM_ST))
  {
    g_free(out_str);
    g_list_free(group_data);
    return FALSE;
  }

  /* If there were groups, validate them */
  if (group_data != NULL)
  {
    gboolean good_grouping = TRUE;
    GList *node;
    char *group;

    group = monetary ? lc->mon_grouping : lc->grouping;

    /* The groups were built in reverse order. This
     * is the easiest order to verify them in. */
    for (node = group_data; node; node = node->next)
    {
      /* Verify group size */
      if (*group != GPOINTER_TO_INT(node->data))
      {
        good_grouping = FALSE;
        break;
      }

      /* Peek ahead at the next group code */
      switch (group[1])
      {
        /* A null char means repeat the last group indefinitely */
        case '\0':
          break;
        /* CHAR_MAX means no more grouping allowed */
        case CHAR_MAX:
          if (node->next != NULL)
            good_grouping = FALSE;
          break;
        /* Anything else means another group size */
        default:
          group++;
          break;
      }

      if (!good_grouping)
        break;
    }

    g_list_free(group_data);

    if (!good_grouping)
    {
      g_free(out_str);
      return FALSE;
    }
  }

  /* Cap the end of the fraction string, if any */
  *out = '\0';

  /* Add in fractional value */
  if (got_decimal && (*out_str != '\0'))
  {
    size_t len;
    long long int fraction;

    len = strlen(out_str);

    if (len > 8)
    {
      out_str[8] = '\0';
      len = 8;
    }

    if (sscanf (out_str, GNC_SCANF_LLD, &fraction) < 1)
    {
      g_free(out_str);
      return FALSE;
    }

    denom = multiplier(len);
    numer *= denom;
    numer += fraction;
  }
  else if (monetary && auto_decimal_enabled && !got_decimal)
  {
    if ((auto_decimal_places > 0) && (auto_decimal_places < 9))
    {
      denom = multiplier(auto_decimal_places);

      /* No need to multiply numer by denom at this point,
       * since by specifying the auto decimal places the
       * user has effectively determined the scaling factor
       * for the numerator they entered.
       */
    }
  }

  if (result != NULL)
  {
    *result = gnc_numeric_create (numer, denom);
    if (is_negative)
      *result = gnc_numeric_neg (*result);
  }

  if (endstr != NULL)
    *endstr = (char *) in;

  g_free (out_str);

  return TRUE;
}

/* enable/disable the auto_decimal_enabled option */
void
gnc_set_auto_decimal_enabled(gboolean enabled)
{
  auto_decimal_enabled = enabled;
}

/* set the number of auto decimal places to use */
void
gnc_set_auto_decimal_places( int places )
{
  auto_decimal_places = places;
}

/* These implementations are rather lame. */
#ifndef HAVE_TOWUPPER
gint32
towupper (gint32 wc)
{
  if (wc > 127)
    return wc;

  return toupper ((int) wc);
}

int
iswlower (gint32 wc)
{
  if (wc > 127)
    return 1;

  return islower ((int) wc);
}
#endif
