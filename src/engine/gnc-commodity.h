/********************************************************************
 * gnc-commodity.h -- API for tradable commodities (incl. currency) *
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

/** @addtogroup Engine
    @{ */
/** @addtogroup Commodity Commodities
    A commodity is something of value that is easily tradeable or
    sellable; for example, currencies, stocks, bonds, grain,
    copper, and oil are all commodities.  This file provides
    an API for defining a commodities, and for working with 
    collections of commodities.  All GnuCash financial transactions
    must identify the commodity that is being traded.

    @warning The system used here does not follow the object
    handling and identification system (GUID's, Entities, etc.)
    that the other parts of GnuCash use.  The API really should be
    ported over.  This would allow us to get rid of the 
    commodity table reoutines defined below. 

    @{ */
/** @file gnc-commodity.h
 *  @brief Commodity handling public routines   
 *  @author Copyright (C) 2000 Bill Gribble
 *  @author Copyright (C) 2001 Linas Vepstas <linas@linas.org>
 */

#ifndef GNC_COMMODITY_H
#define GNC_COMMODITY_H

#include <glib.h>
#include "gnc-engine.h"
#include "qofbook.h"

/** The commodity namespace definitions are used to tag a commodity by
 *  its type, or a stocks by the exchange where it is traded.
 *  
 *  The LEGACY name is only used by the file i/o routines, and is
 *  converted to another commodity namespace before it is seen by the
 *  rest of the system.  The ISO namespace represents currencies.
 *  With the exception of the NASDAQ namespace (which is used once in
 *  the binary importer) the rest of the namespace declarations are
 *  only used to populate an option menu in the commodity selection
 *  window.
 */
#define GNC_COMMODITY_NS_LEGACY "GNC_LEGACY_CURRENCIES"
#define GNC_COMMODITY_NS_ISO    "ISO4217"
#define GNC_COMMODITY_NS_NASDAQ "NASDAQ"
#define GNC_COMMODITY_NS_NYSE   "NYSE"
#define GNC_COMMODITY_NS_EUREX  "EUREX"
#define GNC_COMMODITY_NS_MUTUAL "FUND"
#define GNC_COMMODITY_NS_AMEX   "AMEX"
#define GNC_COMMODITY_NS_ASX    "ASX"


/** @name Commodity Quote Source functions */
/** @{ */

/** The quote source type enum account types are used to determine how
 *  the transaction data in the account is displayed.  These values
 *  can be safely changed from one release to the next.
 */
typedef enum
{
  SOURCE_SINGLE = 0,	/**< This quote source pulls from a single
			 *   specific web site.  For example, the
			 *   yahoo_australia source only pulls from
			 *   the yahoo web site. */
  SOURCE_MULTI,		/**< This quote source may pull from multiple
			 *   web sites.  For example, the australia
			 *   source may pull from ASX, yahoo, etc. */
  SOURCE_UNKNOWN,	/**< This is a locally installed quote source
			 *   that gnucash knows nothing about. May
			 *   pull from single or multiple
			 *   locations. */
  SOURCE_MAX,
  SOURCE_CURRENCY = SOURCE_MAX, /**< The special currency quote source. */
} QuoteSourceType;

/** This function indicates whether or not the Finance::Quote module
 *  is installed on a users computer.  This includes any other related
 *  modules that gnucash need to process F::Q information.
 *
 *  @return TRUE is F::Q is installed properly.
 */
gboolean gnc_quote_source_fq_installed (void);

/** Update gnucash internal tables based on what Finance::Quote
 *  sources are installed.  Sources that have been explicitly coded
 *  into gnucash are marked sensitive/insensitive based upon whether
 *  they are present. New sources that gnucash doesn't know about are
 *  added to its internal tables.
 *
 *  @param sources_list A list of strings containing the source names
 *  as they are known to F::Q.
 */
void gnc_quote_source_set_fq_installed (GList *sources_list);

/** Return the number of entries for a given type of quote source.
 *
 *  @param type The quote source type whose count should be returned.
 *
 *  @return The number of entries for this type of quote source.
 */
gint gnc_quote_source_num_entries(QuoteSourceType type);

/** Create a new quote source. This is called by the F::Q startup code
 *  or the XML parsing code to add new entries to the list of
 *  available quote sources.
 *
 *  @param name The internal name for this new quote source.
 *
 *  @param supported TRUE is this quote source is supported by F::Q.
 *  Should only be set by the F::Q startup routine.
 *
 *  @return A pointer to the newly created quote source.
 */
gnc_quote_source *gnc_quote_source_add_new(const char * name, gboolean supported);

/** Given the internal (gnucash or F::Q) name of a quote source, find
 *  the data structure identified by this name.
 *
 *  @param internal_name The name of this quote source.
 *
 *  @return A pointer to the price quote source that has the specified
 *  internal name.
 */
gnc_quote_source *gnc_quote_source_lookup_by_internal(const char * internal_name);

/** Given the type/index of a quote source, find the data structure
 *  identified by this pair.
 *
 *  @param type The type of this quote source.
 *
 *  @param index The index of this quote source within its type.
 *
 *  @return A pointer to the price quote source that has the specified
 *  type/index.
 */
gnc_quote_source *gnc_quote_source_lookup_by_ti(QuoteSourceType type, gint i);

/** Given a gnc_quote_source data structure, return the flag that
 *  indicates whether this particular quote source is supported by
 *  the user's F::Q installation.
 *
 *  @param source The quote source in question.
 *
 *  @return TRUE if the user's computer supports this quote source.
 */
gboolean gnc_quote_source_get_supported (gnc_quote_source *source);

/** Given a gnc_quote_source data structure, return the type of this
 *  particular quote source. (SINGLE, MULTI, UNKNOWN)
 *
 *  @param source The quote source in question.
 *
 *  @return The type of this quote source.
 */
QuoteSourceType gnc_quote_source_get_type (gnc_quote_source *source);

/** Given a gnc_quote_source data structure, return the index of this
 *  particular quote source within its type.
 *
 *  @param source The quote source in question.
 *
 *  @return The index of this quote source in its type.
 */
gint gnc_quote_source_get_index (gnc_quote_source *source);

/** Given a gnc_quote_source data structure, return the user friendly
 *  name of this quote source.  E.G. "Yahoo Australia" or "Australia
 *  (Yahoo, ASX, ...)"
 *
 *  @param source The quote source in question.
 *
 *  @return The user friendly name.
 */
const char *gnc_quote_source_get_user_name (gnc_quote_source *source);

/** Given a gnc_quote_source data structure, return the internal name
 *  of this quote source.  This is the name used by both gnucash and
 *  by Finance::Quote.  E.G. "yahoo_australia" or "australia"
 *
 *  @param source The quote source in question.
 *
 *  @return The internal name.
 */
const char *gnc_quote_source_get_internal_name (gnc_quote_source *source);

/** Given a gnc_quote_source data structure, return the internal name
 *  of this quote source.  This is the name used by both gnucash and
 *  by Finance::Quote.  E.G. "yahoo_australia" or "australia"
 *
 *  @note This routine should only be used for backward compatability
 *  with the existing XML files.  The rest of the code should use the
 *  gnc_quote_source_lookup_by_internal() routine.
 *
 *  @param source The quote source in question.
 *
 *  @return The internal name.
 */
const char *gnc_quote_source_get_old_internal_name (gnc_quote_source *source);
/** @} */


/** @name Commodity Creation */
/** @{ */

/** Create a new commodity. This function allocates a new commodity
 *  data structure, populates it with the data provided, and then
 *  generates the dynamic names that exist as part of a commodity.
 *
 *  @note This function does not check to see if the commodity exists
 *  before adding a new commodity.
 *
 *  @param fullname The complete name of this commodity. E.G. "Acme
 *  Systems, Inc."
 *
 *  @param namespace An aggregation of commodities. E.G. ISO4217,
 *  Nasdaq, Downbelow, etc.
 *
 *  @param mnemonic An abbreviation for this stock.  For publicly
 *  traced stocks, this field should contain the stock ticker
 *  symbol. This field is used to get online price quotes, so it must
 *  match the stock ticker symbol used by the exchange where you want
 *  to get automatic stock quote updates.  E.G. ACME, ACME.US, etc.
 *
 *  @param exchange_code A string containing the CUSIP code or similar
 *  UNIQUE code for this commodity. The stock ticker is NOT
 *  appropriate.
 *
 *  @param fraction The smallest division of this commodity
 *  allowed. I.E. If this is 1, then the commodity must be traded in
 *  whole units; if 100 then the commodity may be traded in 0.01
 *  units, etc.
 *
 *  @return A pointer to the new commodity.
 */
gnc_commodity * gnc_commodity_new(const char * fullname, 
                                  const char * namespace,
                                  const char * mnemonic,
                                  const char * exchange_code,
                                  int fraction);

/** Destroy a commodity.  Release all memory attached to this data structure.
 *  @note This function does not (can not) check to see if the
 *  commodity is referenced anywhere.
 *  @param cm The commodity to destroy.
 */
void  gnc_commodity_destroy(gnc_commodity * cm);

/** Copy src into dest */
void  gnc_commodity_copy(gnc_commodity * dest, gnc_commodity *src);

/** allocate and copy */
gnc_commodity * gnc_commodity_clone(gnc_commodity *src);
/** @} */



/** @name Commodity Accessor Routines - Get */
/** @{ */

/** Retrieve the mnemonic for the specified commodity.  This will be a
 *  pointer to a null terminated string of the form "ACME", "QWER",
 *  etc.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @return A pointer to the mnemonic for this commodity.  This string
 *  is owned by the engine and should not be freed by the caller.
 */
const char * gnc_commodity_get_mnemonic(const gnc_commodity * cm);

/** Retrieve the namespace for the specified commodity.  This will be
 *  a pointer to a null terminated string of the form "AMEX",
 *  "NASDAQ", etc.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @return A pointer to the namespace for this commodity.  This string
 *  is owned by the engine and should not be freed by the caller.
 */
const char * gnc_commodity_get_namespace(const gnc_commodity * cm);

/** Retrieve the full name for the specified commodity.  This will be
 *  a pointer to a null terminated string of the form "Acme Systems,
 *  Inc.", etc.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @return A pointer to the full name for this commodity.  This string
 *  is owned by the engine and should not be freed by the caller.
 */
const char * gnc_commodity_get_fullname(const gnc_commodity * cm);

/** Retrieve the 'print' name for the specified commodity.  This will
 *  be a pointer to a null terminated string of the form "Acme
 *  Systems, Inc. (ACME)", etc.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @return A pointer to the print name for this commodity.  This
 *  string is owned by the engine and should not be freed by the
 *  caller.
 */
const char * gnc_commodity_get_printname(const gnc_commodity * cm);

/** Retrieve the 'exchange code' for the specified commodity.  This
 *  will be a pointer to a null terminated string of the form
 *  "AXQ14728", etc.  This field is often used when presenting
 *  information to the user.
 *
 *  @note This is a unique code that specifies a particular item or
 *  set of shares of a commodity, not a code that specifies a stock
 *  exchange.  That is the namespace field.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @return A pointer to the exchange code for this commodity.  This
 *  string is owned by the engine and should not be freed by the
 *  caller.
 */
const char * gnc_commodity_get_exchange_code(const gnc_commodity * cm);

/** Retrieve the 'unique' name for the specified commodity.  This will
 *  be a pointer to a null terminated string of the form "AMEX::ACME",
 *  etc.  This field is often used when performing comparisons or
 *  other functions invisible to the user.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @return A pointer to the 'unique' name for this commodity.  This
 *  string is owned by the engine and should not be freed by the
 *  caller.
 */
const char * gnc_commodity_get_unique_name(const gnc_commodity * cm);

/** Retrieve the fraction for the specified commodity.  This will be
 *  an integer value specifying the number of fractional units that
 *  one of these commodities can be divided into.  Should always be a
 *  power of 10.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @return The number of fractional units that one of these
 *  commodities can be divided into.
 */
int     gnc_commodity_get_fraction(const gnc_commodity * cm);

/** Retrieve the 'mark' field for the specified commodity.
 *
 *  @note This is a private field used by the Postgres back end.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @return The value of the mark field.
 */
gint16  gnc_commodity_get_mark(const gnc_commodity * cm);

/** Retrieve the automatic price quote flag for the specified
 *  commodity.  This flag indicates whether stock quotes should be
 *  retrieved for the specified stock.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @return TRUE if quotes should be pulled for this commodity, FALSE
 *  otherwise.
 */
gboolean    gnc_commodity_get_quote_flag(const gnc_commodity *cm);

/** Retrieve the automatic price quote source for the specified
 *  commodity.  This will be a pointer to a null terminated string of
 *  the form "Yahoo (Asia)", etc.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @return A pointer to the price quote source for this commodity.
 *  This string is owned by the engine and should not be freed by the
 *  caller.
 */
gnc_quote_source* gnc_commodity_get_quote_source(const gnc_commodity *cm);
gnc_quote_source* gnc_commodity_get_default_quote_source(const gnc_commodity *cm);

/** Retrieve the automatic price quote timezone for the specified
 *  commodity.  This will be a pointer to a null terminated string of
 *  the form "America/New_York", etc.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @return A pointer to the price quote timezone for this commodity.
 *  This string is owned by the engine and should not be freed by the
 *  caller.
 */
const char* gnc_commodity_get_quote_tz(const gnc_commodity *cm);
/** @} */



/** @name Commodity Accessor Routines - Set */
/** @{ */

/** Set the mnemonic for the specified commodity.  This should be a
 *  pointer to a null terminated string of the form "ACME", "QWER",
 *  etc.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @param mnemonic A pointer to the mnemonic for this commodity.
 *  This string belongs to the caller and will be duplicated by the
 *  engine.
 */
void  gnc_commodity_set_mnemonic(gnc_commodity * cm, const char * mnemonic);

/** Set the namespace for the specified commodity.  This should be a
 *  pointer to a null terminated string of the form "AMEX", "NASDAQ",
 *  etc.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @param namespace A pointer to the namespace for this commodity.
 *  This string belongs to the caller and will be duplicated by the
 *  engine.
 */
void  gnc_commodity_set_namespace(gnc_commodity * cm, const char * namespace);

/** Set the full name for the specified commodity.  This should be
 *  a pointer to a null terminated string of the form "Acme Systems,
 *  Inc.", etc.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @param fullname A pointer to the full name for this commodity.
 *  This string belongs to the caller and will be duplicated by the
 *  engine.
 */
void  gnc_commodity_set_fullname(gnc_commodity * cm, const char * fullname);

/** Set the 'exchange code' for the specified commodity.  This should
 *  be a pointer to a null terminated string of the form "AXQ14728",
 *  etc.
 *
 *  @note This is a unique code that specifies a particular item or
 *  set of shares of a commodity, not a code that specifies a stock
 *  exchange.  That is the namespace field.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @param exchange_code A pointer to the full name for this commodity.  This
 *  string belongs to the caller and will be duplicated by the engine.
 */
void  gnc_commodity_set_exchange_code(gnc_commodity * cm, 
                                      const char * exchange_code);

/** Set the fraction for the specified commodity.  This should be
 *  an integer value specifying the number of fractional units that
 *  one of these commodities can be divided into.  Should always be a
 *  power of 10.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @param smallest_fraction The number of fractional units that one of
 *  these commodities can be divided into.
 */
void  gnc_commodity_set_fraction(gnc_commodity * cm, int smallest_fraction);

/** Set the automatic price quote flag for the specified commodity.
 *  This flag indicates whether stock quotes should be retrieved for
 *  the specified stock.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @param flag TRUE if quotes should be pulled for this commodity, FALSE
 *  otherwise.
 */
void  gnc_commodity_set_quote_flag(gnc_commodity *cm, const gboolean flag);

/** Set the automatic price quote source for the specified commodity.
 *  This should be a pointer to a null terminated string of the form
 *  "Yahoo (Asia)", etc.  Legal values can be found in the
 *  quote_sources array in the file gnc-ui-util.c.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @return A pointer to the price quote source for this commodity.
 *  This string belongs to the caller and will be duplicated by the
 *  engine.
 */
void  gnc_commodity_set_quote_source(gnc_commodity *cm, gnc_quote_source *src);

/** Set the automatic price quote timezone for the specified
 *  commodity.  This should be a pointer to a null terminated string
 *  of the form "America/New_York", etc.  Legal values can be found in
 *  the known_timezones array in the file dialog-util.c.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @return A pointer to the price quote timezone for this commodity.
 *  This string belongs to the caller and will be duplicated by the
 *  engine.
 */
void  gnc_commodity_set_quote_tz(gnc_commodity *cm, const char *tz);
/** @} */



/** @name Commodity Comparison */
/** @{ */

/** This routine returns TRUE if the two commodities are equivalent.
 *  Commodities are equivalent if they have the same namespace and
 *  mnemonic.  Equivalent commodities may belong to different
 *  exchanges, may have different fullnames, and may have different
 *  fractions.
 */
gboolean gnc_commodity_equiv(const gnc_commodity * a, const gnc_commodity * b);

/** This routine returns TRUE if the two commodities are equal.
 *  Commodities are equal if they have the same namespace, mnemonic,
 *  fullname, exchange private code and fraction.
 */
gboolean gnc_commodity_equal(const gnc_commodity * a, const gnc_commodity * b);
/** @} */


/** @name Currency Checks */
/** @{ */

/** Checks to see if the specified commodity namespace is the
 *  namespace for ISO 4217 currencies.
 *
 *  @param cm The string to check.
 *
 *  @return TRUE if the string indicates an ISO currency, FALSE otherwise. */
gboolean gnc_commodity_namespace_is_iso(const char *namespace);

/** Checks to see if the specified commodity is an ISO 4217 recognized currency.
 *
 *  @param cm The commodity to check.
 *
 *  @return TRUE if the commodity represents a currency, FALSE otherwise. */
gboolean gnc_commodity_is_iso(const gnc_commodity * cm);
/** @} */


/* =============================================================== */
/** @name Commodity Table */
/** @{ */

/** Returns the commodity table assoicated with a book.
 */
gnc_commodity_table * gnc_commodity_table_get_table(QofBook *book);

/* XXX backwards compat function; remove me someday */
#define gnc_book_get_commodity_table gnc_commodity_table_get_table

/** compare two tables for equality */
gboolean gnc_commodity_table_equal(gnc_commodity_table *t_1,
                                   gnc_commodity_table *t_2);

/** copy all commodities from src table to dest table */
void gnc_commodity_table_copy(gnc_commodity_table *dest,
                              gnc_commodity_table *src);
/** @} */
/* ---------------------------------------------------------- */
/** @name Commodity Table Lookup functions */
/** @{ */
gnc_commodity * gnc_commodity_table_lookup(const gnc_commodity_table * table, 
                                           const char * namespace, 
                                           const char * mnemonic);
gnc_commodity *
gnc_commodity_table_lookup_unique(const gnc_commodity_table *table,
                                  const char * unique_name);
gnc_commodity * gnc_commodity_table_find_full(const gnc_commodity_table * t,
                                              const char * namespace,
                                              const char * fullname);
/** @} */
/* ---------------------------------------------------------- */

/** @name Commodity Table Maintenance functions */
/** @{ */

/** Add a new commodity to the commodity table.  This routine handles
 *  the cases where the commodity already exists in the database (does
 *  nothing), or another entries has the same namespace and mnemonic
 *  (updates the existing entry).
 *
 *  @param table A pointer to the commodity table 
 *
 *  @param comm A pointer to the commodity to add.
 *
 *  @return The added commodity. Null on error.
 *
 *  @note The commodity pointer passed to this function should not be
 *  used after its return, as it may have been destroyed.  Use the
 *  return value which is guaranteed to be valid. */
gnc_commodity * gnc_commodity_table_insert(gnc_commodity_table * table,
                                           gnc_commodity * comm);

/** Remove a commodity from the commodity table. If the commodity to
 *  remove doesn't exist, nothing happens.
 *
 *  @param table A pointer to the commodity table 
 *
 *  @param comm A pointer to the commodity to remove. */
void gnc_commodity_table_remove(gnc_commodity_table * table,
				gnc_commodity * comm);

/** Add all the standard namespaces and currencies to the commodity
 *  table.  This routine creates the namespaces for the NYSE, NASDAQ,
 *  etc.  It also adds all of the ISO 4217 currencies to the commodity
 *  table.
 *
 *  @param table A pointer to the commodity table */
gboolean gnc_commodity_table_add_default_data(gnc_commodity_table *table);

/** @} */
/* ---------------------------------------------------------- */
/** @name Commodity Table Namespace functions */
/** @{ */

/** Return a count of the number of namespaces in the commodity table.
 *  This count includes both system and user defined namespaces.
 *
 *  @return The number of namespaces.  Zero if an invalid argument was
 *  supplied or there was an error. */
guint gnc_commodity_table_get_number_of_namespaces(gnc_commodity_table* tbl);

/** Test to see if the indicated namespace exits in the commodity table.
 *
 *  @param table A pointer to the commodity table 
 *
 *  @param namespace The new namespace to check.
 *
 *  @return 1 if the namespace exists. 0 if it doesn't exist, or the
 *  routine was passed a bad argument. */
int gnc_commodity_table_has_namespace(const gnc_commodity_table * t,
				      const char * namespace);

/** Return a list of all namespaces in the commodity table.  This
 *  returns both system and user defined namespaces.
 *
 *  @return A pointer to the list of names.  NULL if an invalid
 *  argument was supplied.
 *
 *  @note It is the callers responsibility to free the list. */
GList * gnc_commodity_table_get_namespaces(const gnc_commodity_table * t);

/** This function adds a new string to the list of commodity namespaces.
 *  If the new namespace already exists, nothing happens.
 *
 *  @param table A pointer to the commodity table 
 *
 *  @param namespace The new namespace to be added.*/
void      gnc_commodity_table_add_namespace(gnc_commodity_table * table,
                                            const char * namespace);

/** This function deletes a string from the list of commodity namespaces.
 *  If the namespace does not exist, nothing happens.
 *
 *  @param table A pointer to the commodity table 
 *
 *  @param namespace The namespace to be deleted.
 *
 *  @note This routine will destroy any commodities that exist as part
 *  of this namespace.  Use it carefully. */
void      gnc_commodity_table_delete_namespace(gnc_commodity_table * t,
                                               const char * namespace);
/** @} */
/* ---------------------------------------------------------- */
/** @name Commodity Table Accessor functions */
/** @{ */

/** Returns the number of commodities in the commodity table.
 *
 *  @param table A pointer to the commodity table 
 *
 *  @return The number of commodities in the table. 0 if there are no
 *  commodities, or the routine was passed a bad argument. */
guint gnc_commodity_table_get_size(gnc_commodity_table* tbl);

/** Return a list of all commodities in the commodity table that are
 *  in the given namespace.
 *
 *  @param namespace A string indicating which commodities should be
 *  returned. It is a required argument.
 *
 *  @return A pointer to the list of commodities.  NULL if an invalid
 *  argument was supplied, or the namespace could not be found.
 *
 *  @note It is the callers responsibility to free the list. */
GList * gnc_commodity_table_get_commodities(const gnc_commodity_table * t,
					    const char * namespace);

/** This function returns a list of commodities for which price quotes
 *  should be retrieved.  It will scan the entire commodity table (or
 *  a subset) and check each commodity to see if the price_quote_flag
 *  field has been set.  All matching commodities are queued onto a
 *  list, and the head of that list is returned.
 *
 *  @param table A pointer to the commodity table 
 *
 *  @param expression Use the given expression as a filter on the
 *  commodities to be returned. If non-null, only commodities in
 *  namespace that match the specified regular expression are checked.
 *  If null, all commodities are checked.
 *
 *  @return A pointer to a list of commodities.  NULL if invalid
 *  arguments were supplied or if there no commodities are flagged for
 *  quote retrieval.
 *
 *  @note It is the callers responsibility to free the list. */
GList * gnc_commodity_table_get_quotable_commodities(const gnc_commodity_table * table,
						     const char * expression);

/** Call a function once for each commodity in the commodity table.
 *  This table walk returns whenever the end of the table is reached,
 *  or the function returns FALSE.
 *
 *  @param table A pointer to the commodity table 
 *
 *  @param f The function to call for each commodity.
 *
 *  @param user_data A pointer that is passed into the function
 *  unchanged by the table walk routine. */
gboolean gnc_commodity_table_foreach_commodity(const gnc_commodity_table * table,
                                       gboolean (*f)(gnc_commodity *cm,
                                                     gpointer user_data),
                                       gpointer user_data);
/** @} */

/* ---------------------------------------------------------- */
/** @name Commodity Table Private/Internal-Use Only Routines */
/** @{ */

/** Set the 'mark' field for the specified commodity.
 *
 *  @note This is a private field used by the Postgres back end.
 *
 *  @param cm A pointer to a commodity data structure.
 *
 *  @param mark The new value of the mark field.
 */
void  gnc_commodity_set_mark(gnc_commodity * cm, gint16 mark);

/** You proably shouldn't be using gnc_commodity_table_new() directly,
 * its for internal use only. You should probably be using
 * gnc_commodity_table_get_table()
 */
gnc_commodity_table * gnc_commodity_table_new(void);
void          gnc_commodity_table_destroy(gnc_commodity_table * table);

/** You should probably not be using gnc_commodity_table_set_table()
 * directly.  Its for internal use only.
 */
void gnc_commodity_table_set_table(QofBook *book, gnc_commodity_table *ct);

/** Given the commodity 'from', this routine will find and return the
 *   equivalent commodity (commodity with the same 'unique name') in 
 *   the indicated book.  This routine is primarily useful for setting
 *   up clones of things across multiple books.
 */
gnc_commodity * gnc_commodity_obtain_twin (gnc_commodity *from, QofBook *book);

/** You should probably not be using gnc_commodity_table_register()
 * It is an internal routine for registering the gncObject for the
 * commodity table.
 */
gboolean gnc_commodity_table_register (void);
		  
/** @} */


#endif /* GNC_COMMODITY_H */
/** @} */
/** @} */
