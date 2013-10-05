/********************************************************************
 * gnc-pricedb-p.h -- a simple price database for gnucash.          *
 * Copyright (C) 2001 Rob Browning                                  *
 * Copyright (C) 2003 Linas Vepstas <linas@linas.org>               *
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

#ifndef GNC_PRICEDB_P_H
#define GNC_PRICEDB_P_H

#include <glib.h>
#include "qof.h"
#include "gnc-engine.h"
#include "gnc-pricedb.h"

class GNCPrice : public QofInstance
{
public:
    /* 'public' data fields */
    GNCPriceDB *db;
    gnc_commodity *commodity;
    gnc_commodity *currency;
    Timespec tmspec;
    char *source;
    char *type;
    gnc_numeric value;

    /* 'private' object management fields */
    guint32  refcount;             /* garbage collection reference count */
    
    GNCPrice();
    virtual ~GNCPrice();
};


class GNCPriceDB : public QofInstance
{
public:
    GHashTable *commodity_hash;
    gboolean bulk_update;		 /* TRUE while reading XML file, etc. */
    
    GNCPriceDB();
    virtual ~GNCPriceDB();
};

/* These structs define the kind of price lookup being done
 * so that it can be passed to the backend.  This is a rather
 * cheesy, low-brow interface.  It could stand improvement.
 */
enum PriceLookupType
{
    LOOKUP_LATEST = 1,
    LOOKUP_ALL,
    LOOKUP_AT_TIME,
    LOOKUP_NEAREST_IN_TIME,
    LOOKUP_LATEST_BEFORE,
    LOOKUP_EARLIEST_AFTER
};


struct GNCPriceLookup
{
    PriceLookupType type;
    GNCPriceDB     *prdb;
    const gnc_commodity  *commodity;
    const gnc_commodity  *currency;
    Timespec        date;
};


typedef struct gnc_price_lookup_helper_s
{
    GList    **return_list;
    Timespec time;
} GNCPriceLookupHelper;

#define  gnc_price_set_guid(P,G)  qof_instance_set_guid(QOF_INSTANCE(P),(G))
void     gnc_pricedb_substitute_commodity(GNCPriceDB *db,
        gnc_commodity *old_c,
        gnc_commodity *new_c);

/** register the pricedb object with the gncObject system */
gboolean gnc_pricedb_register (void);

QofBackend * xaccPriceDBGetBackend (GNCPriceDB *prdb);

#endif
