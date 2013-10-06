/********************************************************************\
 * gncTaxTableP.h -- the Gnucash Tax Table private interface        *
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

/*
 * Copyright (C) 2002 Derek Atkins
 * Copyright (C) 2003 Linas Vepstas <linas@linas.org>
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#ifndef GNC_TAXTABLEP_H_
#define GNC_TAXTABLEP_H_

#include "gncTaxTable.h"


class GncTaxTable : public QofInstance
{
public:
    char *          name;
    GncTaxTableEntryList*  entries;
    Timespec        modtime;      /* internal date of last modtime */

    /* See src/doc/business.txt for an explanation of the following */
    /* Code that handles this is *identical* to that in gncBillTerm */
    gint64          refcount;
    GncTaxTable *   parent;       /* if non-null, we are an immutable child */
    GncTaxTable *   child;        /* if non-null, we have not changed */
    bool        invisible;
    GList *         children;     /* list of children for disconnection */
    
    GncTaxTable();
    virtual ~GncTaxTable() {}
};

bool gncTaxTableRegister (void);

void gncTaxTableSetParent (GncTaxTable *table, GncTaxTable *parent);
void gncTaxTableSetChild (GncTaxTable *table, GncTaxTable *child);
void gncTaxTableSetRefcount (GncTaxTable *table, gint64 refcount);
void gncTaxTableMakeInvisible (GncTaxTable *table);

bool gncTaxTableGetInvisible (const GncTaxTable *table);

GncTaxTable* gncTaxTableEntryGetTable( const GncTaxTableEntry* entry );

#define gncTaxTableSetGUID(E,G) qof_instance_set_guid(QOF_INSTANCE(E),(G))

#endif /* GNC_TAXTABLEP_H_ */
