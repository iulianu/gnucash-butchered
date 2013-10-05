/********************************************************************\
 * gncBillTermP.h -- the Gnucash Billing Term private interface     *
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

#ifndef GNC_BILLTERMP_H_
#define GNC_BILLTERMP_H_

#include "gncBillTerm.h"

class GncBillTerm : public QofInstance
{
public:
    /* 'visible' data fields directly manipulated by user */
    char *          name;
    char *          desc;
    GncBillTermType type;
    gint            due_days;
    gint            disc_days;
    gnc_numeric     discount;
    gint            cutoff;

    /* Internal management fields */
    /* See src/doc/business.txt for an explanation of the following */
    /* Code that handles this is *identical* to that in gncTaxTable */
    gint64          refcount;
    GncBillTerm *   parent;      /* if non-null, we are an immutable child */
    GncBillTerm *   child;       /* if non-null, we have not changed */
    gboolean        invisible;
    GList *         children;    /* list of children for disconnection */
    
    GncBillTerm();
    virtual ~GncBillTerm();
};

gboolean gncBillTermRegister (void);

void gncBillTermSetParent (GncBillTerm *term, GncBillTerm *parent);
void gncBillTermSetChild (GncBillTerm *term, GncBillTerm *child);
void gncBillTermSetRefcount (GncBillTerm *term, gint64 refcount);
void gncBillTermMakeInvisible (GncBillTerm *term);

gboolean gncBillTermGetInvisible (const GncBillTerm *term);

#define gncBillTermSetGUID(E,G) qof_instance_set_guid(QOF_INSTANCE(E),(G))


#endif /* GNC_BILLTERMP_H_ */
