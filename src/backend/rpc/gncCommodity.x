/*
 * FILE:
 * gncCommodity.x
 *
 * FUNCTION:
 * The RPC definition for a Gnucash Commodity
 *
 * HISTORY:
 * Created By:	Derek Atkins <warlord@MIT.EDU>
 * Copyright (c) 2001, Derek Atkins
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact:
 *
 * Free Software Foundation           Voice:  +1-617-542-5942
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652
 * Boston, MA  02111-1307,  USA       gnu@gnu.org
 */

#ifndef __GNC_COMMODITY_X
#define __GNC_COMMODITY_X

/* This is an actual commodity entry; unique_name is generated */
struct gncCommodity {
  string	fullname<>;
  string	namespace<>;
  string	mnemonic<>;
  string	printname<>;
  string	exchange_code<>;
  int		fraction;
};

/* List of commodities */
struct gnc_commoditylist {
  gncCommodity *	commodity;
  gnc_commoditylist *	next;
};

/* This is sufficient information to find a commodity in the commodity
 * table using gnc_commodity_table_lookup(), although while it does
 * save on network transfer space, it makes it harder on the sender
 * to actually package up an account.  So let's keep it here but not
 * use it.
 */
struct gncCommodityPtr {
  string	namespace<>;
  string	mnemonic<>;
};

#endif /* __GNC_COMMODITY_X */
