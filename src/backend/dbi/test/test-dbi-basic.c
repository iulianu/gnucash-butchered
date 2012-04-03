/***************************************************************************
 *            test-dbi.c
 *
 *  Tests saving and loading to a dbi/sqlite3 db
 *
 *  Copyright (C) 2009  Phil Longstaff <plongstaff@rogers.com>
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "config.h"
#include "qof.h"
#include "cashobjects.h"
#include "test-engine-stuff.h"
#include "test-stuff.h"
#include "test-dbi-stuff.h"
#include <unittest-support.h>

#include "TransLog.h"
#include "Account.h"
#include "Transaction.h"
#include "Split.h"
#include "gnc-commodity.h"

#define FILE_NAME "sqlite3:///tmp/test-sqlite3-file"
#define GNC_LIB_NAME "gncmod-backend-dbi"

static QofSession*
create_session(void)
{
    QofSession* session;
    QofBook* book;
    Account *root, *acct1, *acct2;
    KvpFrame* frame;
    Transaction* tx;
    Split *spl1, *spl2;
    Timespec ts;
    struct timeval tv;
    gnc_commodity_table* table;
    gnc_commodity* currency;
    gchar *msg = "[gnc_dbi_unlock()] There was no lock entry in the Lock table";
    gchar *log_domain = "gnc.backend.dbi";
    guint loglevel = G_LOG_LEVEL_WARNING, hdlr;
    TestErrorStruct check = { loglevel, log_domain, msg };
    hdlr = g_log_set_handler (log_domain, loglevel,
                              (GLogFunc)test_checked_handler, &check);

    session = qof_session_new();
    book = qof_session_get_book( session );
    root = gnc_book_get_root_account( book );
    g_log_remove_handler (log_domain, hdlr);

    table = gnc_commodity_table_get_table( book );
    currency = gnc_commodity_table_lookup( table, GNC_COMMODITY_NS_CURRENCY, "CAD" );

    acct1 = xaccMallocAccount( book );
    xaccAccountSetType( acct1, ACCT_TYPE_BANK );
    xaccAccountSetName( acct1, "Bank 1" );
    xaccAccountSetCommodity( acct1, currency );

    frame = qof_instance_get_slots( QOF_INSTANCE(acct1) );
    kvp_frame_set_gint64( frame, "int64-val", 100 );
    kvp_frame_set_double( frame, "double-val", 3.14159 );
    kvp_frame_set_numeric( frame, "numeric-val", gnc_numeric_zero() );

    time( &(tv.tv_sec) );
    tv.tv_usec = 0;
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = 1000 * tv.tv_usec;
    kvp_frame_set_timespec( frame, "timespec-val", ts );

    kvp_frame_set_string( frame, "string-val", "abcdefghijklmnop" );
    kvp_frame_set_guid( frame, "guid-val", qof_instance_get_guid( QOF_INSTANCE(acct1) ) );

    gnc_account_append_child( root, acct1 );

    acct2 = xaccMallocAccount( book );
    xaccAccountSetType( acct2, ACCT_TYPE_BANK );
    xaccAccountSetName( acct2, "Bank 1" );

    tx = xaccMallocTransaction( book );
    xaccTransBeginEdit( tx );
    xaccTransSetCurrency( tx, currency );
    spl1 = xaccMallocSplit( book );
    xaccTransAppendSplit( tx, spl1 );
    spl2 = xaccMallocSplit( book );
    xaccTransAppendSplit( tx, spl2 );
    xaccTransCommitEdit( tx );


    return session;
}

int main (int argc, char ** argv)
{
    gchar* filename;
    QofSession* session_1;

    qof_init();
    cashobjects_register();
    xaccLogDisable();
    qof_load_backend_library ("../.libs/", GNC_LIB_NAME);

    // Create a session with data
    session_1 = create_session();
    filename = tempnam( "/tmp", "test-sqlite3-" );
    g_test_message ( "Using filename: %s\n", filename );
    test_dbi_store_and_reload( "sqlite3", session_1, filename );
    session_1 = create_session();
    test_dbi_safe_save( "sqlite3", filename );
    test_dbi_version_control( "sqlite3", filename );
#ifdef TEST_MYSQL_URL
    g_test_message ( "TEST_MYSQL_URL='%s'\n", TEST_MYSQL_URL );
    if ( strlen( TEST_MYSQL_URL ) > 0 )
    {
        session_1 = create_session();
        test_dbi_store_and_reload( "mysql", session_1, TEST_MYSQL_URL );
        session_1 = create_session();
        test_dbi_safe_save( "mysql", filename );
        test_dbi_version_control( "mysql", filename );
    }
#endif
#ifdef TEST_PGSQL_URL
    g_test_message ( "TEST_PGSQL_URL='%s'\n", TEST_PGSQL_URL );
    if ( strlen( TEST_PGSQL_URL ) > 0 )
    {
        session_1 = create_session();
        test_dbi_store_and_reload( "pgsql", session_1, TEST_PGSQL_URL );
        session_1 = create_session();
        test_dbi_safe_save( "pgsql", filename );
        test_dbi_version_control( "pgsql", filename );
    }
#endif
    print_test_results();
    qof_close();
    exit(get_rv());
}

