/********************************************************************\
 * gncCustomerP.h -- the Core Customer Interface                    *
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
 * Copyright (C) 2001 Derek Atkins
 * Copyright (C) 2003 Linas Vepstas <linas@linas.org>
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#ifndef GNC_CUSTOMERP_H_
#define GNC_CUSTOMERP_H_

#include "gncCustomer.h"
#include "gncTaxTableP.h"

class GncCustomer : public QofInstance
{
public:
    /* The following fields are identical to 'vendor' */
    char *          id;
    char *          name;
    char *          notes;
    GncBillTerm *   terms;
    GncAddress *    addr;
    gnc_commodity * currency;
    GncTaxTable*    taxtable;
    bool        taxtable_override;
    GncTaxIncluded  taxincluded;
    bool            active;
    GList *         jobs;

    /* The following fields are unique to 'customer' */
    gnc_numeric     credit;
    gnc_numeric     discount;
    GncAddress *    shipaddr;
    
    GncCustomer();
    virtual ~GncCustomer();
};


bool gncCustomerRegister (void);
gchar *gncCustomerNextID (QofBook *book);

#define gncCustomerSetGUID(E,G) qof_instance_set_guid(QOF_INSTANCE(E),(G))

#endif /* GNC_CUSTOMERP_H_ */
