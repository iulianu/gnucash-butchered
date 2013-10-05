/********************************************************************\
 * gncEntryP.h -- the Core Busines Entry Interface                  *
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

#ifndef GNC_ENTRYP_H_
#define GNC_ENTRYP_H_

#include "gncEntry.h"

class GncEntry : public QofInstance
{
public:
    Timespec	date;
    Timespec	date_entered;
    char *	desc;
    char *	action;
    char *	notes;
    gnc_numeric 	quantity;

    /* customer invoice data */
    Account *	i_account;
    gnc_numeric 	i_price;
    gboolean	i_taxable;
    gboolean	i_taxincluded;
    GncTaxTable *	i_tax_table;
    gnc_numeric 	i_discount;
    GncAmountType	i_disc_type;
    GncDiscountHow i_disc_how;

    /* vendor bill data */
    Account *	b_account;
    gnc_numeric 	b_price;
    gboolean	b_taxable;
    gboolean	b_taxincluded;
    GncTaxTable *	b_tax_table;
    gboolean	billable;
    GncOwner	billto;

    /* employee bill data */
    GncEntryPaymentType b_payment;

    /* my parent(s) */
    GncOrder *	order;
    GncInvoice *	invoice;
    GncInvoice *	bill;

    /* CACHED VALUES */
    gboolean	values_dirty;

    /* customer invoice */
    gnc_numeric	i_value;
    gnc_numeric	i_value_rounded;
    GList *	i_tax_values;
    gnc_numeric	i_tax_value;
    gnc_numeric	i_tax_value_rounded;
    gnc_numeric	i_disc_value;
    gnc_numeric	i_disc_value_rounded;
    Timespec	i_taxtable_modtime;

    /* vendor bill */
    gnc_numeric	b_value;
    gnc_numeric	b_value_rounded;
    GList *	b_tax_values;
    gnc_numeric	b_tax_value;
    gnc_numeric	b_tax_value_rounded;
    Timespec	b_taxtable_modtime;

    GncEntry();
    virtual ~GncEntry() {}
};

gboolean gncEntryRegister (void);
void gncEntrySetGUID (GncEntry *entry, const GncGUID *guid);
void gncEntrySetOrder (GncEntry *entry, GncOrder *order);
void gncEntrySetInvoice (GncEntry *entry, GncInvoice *invoice);
void gncEntrySetBill (GncEntry *entry, GncInvoice *bill);
void gncEntrySetDirty (GncEntry *entry, gboolean dirty);

#define gncEntrySetGUID(E,G) qof_instance_set_guid(QOF_INSTANCE(E),(G))

#endif /* GNC_ENTRYP_H_ */
