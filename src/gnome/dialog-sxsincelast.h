/********************************************************************\
 * dialog-sxsincelast.h - SchedXaction "Since-Last-Run" dialog      *
 * Copyright (c) 2001 Joshua Sled <jsled@asynchronous.org>          *
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
\********************************************************************/

#ifndef DIALOG_SXSINCELAST_H
#define DIALOG_SXSINCELAST_H

/**
 * @return The magnitude of the return value is the number of auto-created,
 * no-notification scheduled transactions created.  This value is positive if
 * there are additionally other SXes which need user interaction and the
 * Druid has been displayed, or negative if there are not, and no Druid
 * window was realized.  In the case where there the dialog has been
 * displayed but no auto-create-no-notify transactions have been created,
 * INT_MAX [limits.h] is returned.  0 is treated as negative, with no
 * transactions created and no dialog displayed.  The caller can use this
 * value as appropriate to inform the user.
 *
 * [e.g., for book-open-hook: do nothing; for menu-selection: display an info
 *  dialog stating there's nothing to do.]
 **/
gint gnc_ui_sxsincelast_dialog_create( void );
void gnc_ui_sxsincelast_guile_wrapper( char* );

/**
 * Returns the varaibles from the Splits of the given SchedXaction as the
 * keys of the given GHashTable.
 **/
void sxsl_get_sx_vars( SchedXaction *sx, GHashTable *varHash );

/**
 * Returns the variables from the given formula [free-form non-numeric
 * character strings] as the keys of the given GHashTable.
 * @param result can be NULL if you're not interested in the result
 **/
int parse_vars_from_formula( const char *formula,
                             GHashTable *varHash,
                             gnc_numeric *result );

void print_vars_helper( gpointer key, gpointer value, gpointer user_data );

#endif // !defined(DIALOG_SXSINCELAST_H)
