/********************************************************************\
 * io-gncxml-v2.h -- api for gnucash xml i/o                        *
 *                                                                  *
 * Copyright (c) 2001 Gnumatic Incorporated                         *
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

/*
 * io-gncxml.h -- api for new XML-based file format
 *
 * Initial code by James LewisMoss
 */

#ifndef IO_GNCXML_V2_H
#define IO_GNCXML_V2_H

#include <glib.h>

#include "Account.h"
#include "Backend.h"
#include "Transaction.h"
#include "gnc-book.h"
#include "gnc-commodity.h"
#include "gnc-engine.h"
#include "gnc-pricedb.h"
#include "SchedXaction.h"

#include "sixtp.h"

typedef struct
{
    int accounts_total;
    int accounts_loaded;

    int books_total;
    int books_loaded;

    int commodities_total;
    int commodities_loaded;

    int transactions_total;
    int transactions_loaded;

    int prices_total;
    int prices_loaded;

    int schedXactions_total;
    int schedXactions_loaded;
} load_counter;

typedef struct sixtp_gdv2 sixtp_gdv2;
typedef void (*countCallbackFn)(sixtp_gdv2 *gd, const char *type);
struct sixtp_gdv2
{
    GNCBook *book;
    load_counter counter;
    countCallbackFn countCallback;
    GNCBePercentageFunc gui_display_fn;
};

/**
 * Struct used to pass in a new data type for XML storage.  This contains
 * the set of callbacks to read and write XML for new data objects..  New
 * types should register an instance of this object with the engine.
 *
 * The create_parser() method will create a new sixtp parser for this
 *   data type.
 *
 * The add_item() method takes a local state and a new object of this type
 *   and the method implementation should do whatever is necessary to cleanup
 *   the object and (maybe) add it into the book stored in the local-state.
 *
 * The get_count() method returns the number of items of this type.
 *
 * The write() method writes out all the objects of this particular type
 *   in the book and stores the XML in the FILE.
 */
#define GNC_FILE_BACKEND	"gnc:file:2"
#define GNC_FILE_BACKEND_VERS	2
typedef struct
{
  int		version;	/* backend version number */
  const char *	type_name;	/* The XML tag for this type */

  sixtp *	(*create_parser) (void);
  gboolean	(*add_item)(sixtp_gdv2 *, gpointer obj);
  int		(*get_count) (GNCBook *);
  void		(*write) (FILE*, GNCBook*);
} GncXmlDataType_t;

/**
 * Struct used to pass the account group/accounts and trasnactions in
 * the <gnc:template-transactions> section between the parser in
 * gnc-schedxactions-xml-v2.c and the add-to-book callback in
 * io-gncxml-v2.c.
 **/
typedef struct
{
	AccountList	*accts;
	TransList	*transactions;
        GNCBook *book;
} gnc_template_xaction_data;

/* Call after loading each record */
void run_callback(sixtp_gdv2 *data, const char *type);

/* read in an account group from a file */
gboolean gnc_session_load_from_xml_file_v2(GNCSession *session);

/* write all book info to a file */
gboolean gnc_book_write_to_xml_filehandle_v2(GNCBook *book, FILE *fh);
gboolean gnc_book_write_to_xml_file_v2(GNCBook *book, const char *filename);

/* write just the commodities and accounts to a file */
gboolean gnc_book_write_accounts_to_xml_filehandle_v2(Backend *be, GNCBook *book, FILE *fh);
gboolean gnc_book_write_accounts_to_xml_file_v2(Backend * be, GNCBook *book,
						const char *filename);

/* The is_gncxml_file() routine checks to see if the first few 
 * chars of the file look like gnc-xml data.
 */
gboolean gnc_is_xml_data_file_v2(const gchar *name);

#endif /* __IO_GNCXML_V2_H__ */
