/* 
 * gnc-plugin-page-register.h -- 
 *
 * Copyright (C) 2003 Jan Arne Petersen <jpetersen@uni-bonn.de>
 * Copyright (C) 2003 David Hampton <hampton@employees.org>
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

/** @addtogroup ContentPlugins
    @{ */
/** @addtogroup RegisterPlugin Register Page
    @{ */
/** @file gnc-plugin-page-register.h 
    @brief  utility functions for the GnuCash UI
    @author Copyright (C) 2003 Jan Arne Petersen <jpetersen@uni-bonn.de>
    @author Copyright (C) 2003 David Hampton <hampton@employees.org>
*/

#ifndef __GNC_PLUGIN_PAGE_REGISTER_H
#define __GNC_PLUGIN_PAGE_REGISTER_H

#include <gtk/gtkwindow.h>

#include "Account.h"
#include "gnc-ledger-display.h"
#include "gnc-plugin-page.h"
#include "gnc-split-reg.h"

G_BEGIN_DECLS

/* type macros */
#define GNC_TYPE_PLUGIN_PAGE_REGISTER            (gnc_plugin_page_register_get_type ())
#define GNC_PLUGIN_PAGE_REGISTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNC_TYPE_PLUGIN_PAGE_REGISTER, GncPluginPageRegister))
#define GNC_PLUGIN_PAGE_REGISTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GNC_TYPE_PLUGIN_PAGE_REGISTER, GncPluginPageRegisterClass))
#define GNC_IS_PLUGIN_PAGE_REGISTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNC_TYPE_PLUGIN_PAGE_REGISTER))
#define GNC_IS_PLUGIN_PAGE_REGISTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNC_TYPE_PLUGIN_PAGE_REGISTER))
#define GNC_PLUGIN_PAGE_REGISTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GNC_TYPE_PLUGIN_PAGE_REGISTER, GncPluginPageRegisterClass))
#define GNC_PLUGIN_PAGE_REGISTER_NAME "GncPluginPageRegister"

/* typedefs & structures */
typedef struct GncPluginPageRegisterPrivate GncPluginPageRegisterPrivate;

typedef struct {
	GncPluginPage parent;

	GncPluginPageRegisterPrivate *priv;
} GncPluginPageRegister;

typedef struct {
	GncPluginPageClass parent;
} GncPluginPageRegisterClass;

/* function prototypes */

/** Retrieve the type number for an "account tree" plugin page.
 *
 *  @return The type number.
 */
GType gnc_plugin_page_register_get_type (void);


/** Create a new "register" plugin page, given a pointer to an
 *  account.
 *
 *  @param account The pointer to the account to embed in the
 *  register.
 *
 *  @param subaccounts TRUE if all the sub-accounts of the specified
 *  account should be included in the register.
 *
 *  @return The newly created plugin page.
 */
GncPluginPage *gnc_plugin_page_register_new (Account *account, gboolean subaccounts);


/** Create a new "register" plugin page, given a pointer to an already
 *  created ledger.  This function should be used when the ledger
 *  already exists.  (E.G.  From the "find transaction" code, or from
 *  the scheduled transaction code.)
 *
 *  @param ledger The pointer to the ledger to embed into the
 *  register.
 *
 *  @return The newly created plugin page.
 */
GncPluginPage *gnc_plugin_page_register_new_ledger (GNCLedgerDisplay *ledger);


/** Create a new "register" plugin page containing a general ledger.
 *
 *  @return The newly created plugin page.
 */
GncPluginPage *gnc_plugin_page_register_new_gl (void);


/** Set various register options on a newly created "register" plugin page.
 *
 *  @param plugin_page The "register" page to modify.
 *
 *  @param lines_opt_page The name of the options page containing the
 *  number of lines per register.
 *
 *  @param lines_opt_name The name of the options (within the page
 *  specified above) containing the number of lines per register.
 *
 *  @param lines_default The number of lines to use if the specified
 *  options cannot be found or has never been set.
 *
 *  @param disallowCaps Capabilities to disable on this register.
 *  These include "read only", "jump", "schedule", et. al.
 */
void
gnc_plugin_page_register_set_options (GncPluginPage *plugin_page,
				      const char *lines_opt_page,
				      const char *lines_opt_name,
				      gint lines_default,
				      gint disallowCaps);


/** Get the GNCSplitReg data structure associated with this register page.
 *
 *  @param plugin_page A "register" page.
 */
GNCSplitReg * gnc_plugin_page_register_get_gsr (GncPluginPage *plugin_page);


/** Get the Account associated with this register page.
 *
 *  @param page A "register" page.
 *
 *  @return The account if the register contsins only a single
 *  account, or an account and its sub-accounts.  NULL otherwise.
 */
Account *gnc_plugin_page_register_get_account (GncPluginPageRegister *page);

G_END_DECLS
/** @} */
/** @} */

#endif /* __GNC_PLUGIN_PAGE_REGISTER_H */

