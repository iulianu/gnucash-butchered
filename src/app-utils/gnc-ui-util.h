/********************************************************************\
 * gnc-ui-util.h -- utility functions for the GnuCash UI            *
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

#ifndef GNC_UI_UTIL_H
#define GNC_UI_UTIL_H

#include "config.h"

#include <glib.h>
#include <locale.h>

#include "Account.h"
#include "gnc-engine.h"
#include "Group.h"
#include "gnc-session.h"


typedef GNCSession * (*GNCSessionCB) (void);


/* User Settings ****************************************************/
gboolean gnc_color_deficits (void);

char gnc_get_account_separator (void);

gboolean gnc_reverse_balance(Account *account);
gboolean gnc_reverse_balance_type(GNCAccountType type);


/* Default directories **********************************************/

void gnc_init_default_directory (char **dirname);
void gnc_extract_directory (char **dirname, const char *filename);


/* Engine enhancements & i18n ***************************************/
GNCBook * gnc_get_current_book (void);
AccountGroup * gnc_get_current_group (void);
gnc_commodity_table * gnc_get_current_commodities (void);

typedef enum
{
  ACCOUNT_TYPE = 0,
  ACCOUNT_NAME,
  ACCOUNT_CODE,
  ACCOUNT_DESCRIPTION,
  ACCOUNT_NOTES,
  ACCOUNT_COMMODITY,
  ACCOUNT_BALANCE,        /* with sign reversal */
  ACCOUNT_BALANCE_REPORT, /* ACCOUNT_BALANCE in default report currency */
  ACCOUNT_TOTAL,          /* balance + children's balance with sign reversal */
  ACCOUNT_TOTAL_REPORT,   /* ACCOUNT_TOTAL in default report currency */
  ACCOUNT_TAX_INFO,
  NUM_ACCOUNT_FIELDS
} AccountFieldCode;

const char * gnc_ui_account_get_field_name (AccountFieldCode field);

/* Must g_free string when done */
char * gnc_ui_account_get_field_value_string (Account *account,
                                              AccountFieldCode field);

gnc_numeric gnc_ui_convert_balance_to_currency(gnc_numeric balance,
                                               gnc_commodity *balance_currency,
                                               gnc_commodity *currency);

gnc_numeric gnc_ui_account_get_balance (Account *account,
                                        gboolean include_children);

gnc_numeric gnc_ui_account_get_reconciled_balance(Account *account,
                                                  gboolean include_children);
gnc_numeric gnc_ui_account_get_balance_as_of_date (Account *account,
                                                   time_t date,
                                                   gboolean include_children);

const char * gnc_get_reconcile_str (char reconciled_flag);
const char * gnc_get_reconcile_valid_flags (void);
const char * gnc_get_reconcile_flag_order (void);

typedef enum
{
  EQUITY_OPENING_BALANCE,
  EQUITY_RETAINED_EARNINGS,
  NUM_EQUITY_TYPES
} GNCEquityType;

Account * gnc_find_or_create_equity_account (AccountGroup *group,
                                             GNCEquityType equity_type,
                                             gnc_commodity *currency,
                                             GNCBook *book);
gboolean gnc_account_create_opening_balance (Account *account,
                                             gnc_numeric balance,
                                             time_t date,
                                             GNCBook *book);

char * gnc_account_get_full_name (Account *account);


/* Price source functions *******************************************/

/* NOTE: If you modify PriceSourceCode, please update price-quotes.scm
   as well. */
typedef enum
{
  SOURCE_NONE = 0,
  SPECIFIC_SOURCES,
  SOURCE_AEX,
  SOURCE_ASX,
  SOURCE_DWS,
  SOURCE_FIDELITY_DIRECT,
  SOURCE_FOOL,
  SOURCE_FUNDLIBRARY,
  SOURCE_TDWATERHOUSE,
  SOURCE_TIAA_CREF,
  SOURCE_TROWEPRICE_DIRECT,
  SOURCE_TRUSTNET,
  SOURCE_UNION,
  SOURCE_VANGUARD,
  SOURCE_VWD,
  SOURCE_YAHOO,
  SOURCE_YAHOO_ASIA,
  SOURCE_YAHOO_AUSTRALIA,
  SOURCE_YAHOO_EUROPE,
  SOURCE_ZUERICH,
  GENERAL_SOURCES,
  SOURCE_ASIA,
  SOURCE_AUSTRALIA,
  SOURCE_CANADA,
  SOURCE_CANADAMUTUAL,
  SOURCE_DUTCH,
  SOURCE_EUROPE,
  SOURCE_FIDELITY,
  SOURCE_TROWEPRICE,
  SOURCE_UKUNITTRUSTS,
  SOURCE_USA,
  NUM_SOURCES
} PriceSourceCode;
/* NOTE: If you modify PriceSourceCode, please update price-quotes.scm
   as well. */

const char * gnc_price_source_enum2name (PriceSourceCode source);
const char * gnc_price_source_enum2internal (PriceSourceCode source);
const char * gnc_price_source_internal2fq (const char * codename);
PriceSourceCode gnc_price_source_internal2enum (const char * internal_name);
PriceSourceCode gnc_price_source_fq2enum (const char * fq_name);
gboolean gnc_price_source_sensitive (PriceSourceCode source);
void gnc_price_source_set_fq_installed (GList *sources_list);
gboolean gnc_price_source_have_fq (void);


/* Locale functions *************************************************/

/* The gnc_localeconv() subroutine returns an lconv structure
 * containing locale information. If no locale is set, the structure
 * is given default (en_US) values.  */
struct lconv * gnc_localeconv (void);

/* Returns the default currency of the current locale, or NULL if no
 * sensible currency could be identified from the locale. */
gnc_commodity * gnc_locale_default_currency_nodefault (void);

/* Returns the default currency of the current locale. WATCH OUT: If
 * no currency could be identified from the locale, this one returns
 * "USD", but this will have nothing to do with the actual locale. */
gnc_commodity * gnc_locale_default_currency (void);

/* Returns the default ISO currency string of the current locale. */
const char * gnc_locale_default_iso_currency_code (void);

/* Returns the number of decimal place to print in the current locale */
int gnc_locale_decimal_places (void);

/* Push and pop locales. Currently, this has no effect on gnc_localeconv.
 * i.e., after the first call to gnc_localeconv, subsequent calls will
 * return the same information. */
void gnc_push_locale (const char *locale);
void gnc_pop_locale (void);

/* Amount printing and parsing **************************************/

/*
 * The xaccPrintAmount() and xaccSPrintAmount() routines provide
 *    i18n'ed convenience routines for printing gnc_numerics.
 *    amounts. Both routines take a gnc_numeric argument and
 *    a printing information object.
 *
 * The xaccPrintAmount() routine returns a pointer to a statically
 *    allocated buffer, and is therefore not thread-safe.
 *
 * The xaccSPrintAmount() routine accepts a pointer to the buffer to be
 *    printed to.  It returns the length of the printed string.
 */

typedef struct _GNCPrintAmountInfo
{
  const gnc_commodity *commodity;  /* may be NULL */

  guint8 max_decimal_places;
  guint8 min_decimal_places;

  unsigned int use_separators : 1; /* Print thousands separators */
  unsigned int use_symbol : 1;     /* Print currency symbol */
  unsigned int use_locale : 1;     /* Use locale for some positioning */
  unsigned int monetary : 1;       /* Is a monetary quantity */
  unsigned int force_fit : 1;      /* Don't print more than max_dp places */
  unsigned int round : 1;          /* Round at max_dp instead of truncating */
} GNCPrintAmountInfo;


GNCPrintAmountInfo gnc_default_print_info (gboolean use_symbol);

GNCPrintAmountInfo gnc_commodity_print_info (const gnc_commodity *commodity,
                                             gboolean use_symbol);

GNCPrintAmountInfo gnc_account_print_info (Account *account,
                                           gboolean use_symbol);

GNCPrintAmountInfo gnc_split_amount_print_info (Split *split,
                                                gboolean use_symbol);
GNCPrintAmountInfo gnc_split_value_print_info (Split *split,
                                               gboolean use_symbol);

GNCPrintAmountInfo gnc_share_print_info_places (int decplaces);
GNCPrintAmountInfo gnc_default_share_print_info (void);
GNCPrintAmountInfo gnc_default_price_print_info (void);

GNCPrintAmountInfo gnc_integral_print_info (void);

const char * xaccPrintAmount (gnc_numeric val, GNCPrintAmountInfo info);
int xaccSPrintAmount (char *buf, gnc_numeric val, GNCPrintAmountInfo info);


/* xaccParseAmount parses in_str to obtain a numeric result. The
 *   routine will parse as much of in_str as it can to obtain a single
 *   number. The number is parsed using the current locale information
 *   and the 'monetary' flag. The routine will return TRUE if it
 *   successfully parsed a number and FALSE otherwise. If TRUE is
 *   returned and result is non-NULL, the value of the parsed number
 *   is stored in *result. If FALSE is returned, *result is
 *   unchanged. If TRUE is returned and endstr is non-NULL, the
 *   location of the first character in in_str not used by the parser
 *   will be returned in *endstr. If FALSE is returned and endstr is
 *   non-NULL, *endstr will point to in_str. */
gboolean xaccParseAmount (const char * in_str, gboolean monetary,
                          gnc_numeric *result, char **endstr);


/* Automatic decimal place conversion *******************************/

/* enable/disable the auto decimal option */
void gnc_set_auto_decimal_enabled(gboolean enabled);

/* set how many auto decimal places to use */
void gnc_set_auto_decimal_places(int places);


/* Missing functions ************************************************/

#ifndef HAVE_TOWUPPER
gint32 towupper (gint32 wc);
int iswlower (gint32 wc);
#endif

#endif
