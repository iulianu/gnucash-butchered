/********************************************************************\
 * gnc-exp-parser.h -- Interface to expression parsing for GnuCash  *
 * Copyright (C) 2000 Dave Peticolas <dave@krondo.com>              *
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
 * along with this program; if not, write to the Free Software      *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.        *
\********************************************************************/

#ifndef __GNC_EXP_PARSER_H__
#define __GNC_EXP_PARSER_H__

#include "config.h"

#include <glib.h>


/* Initialize the expression parser. If this function is not
 * called before one of the other parsing routines (other than
 * gnc_exp_parser_shutdown), it will be called if needed. */
void gnc_exp_parser_init (void);

/* Shutdown the expression parser and free any associated memory. */
void gnc_exp_parser_shutdown (void);

/* Return a list of variable names which are currently defined
 * in the parser. The names should not be modified or freed. */
GList * gnc_exp_parser_get_variable_names (void);

/* Undefine the variable name if it is already defined. */
void gnc_exp_parser_remove_variable (const char *variable_name);

/* Undefine every variable name appearing in the list. Variables in
 * the list which are not defined are ignored. */
void gnc_exp_parser_remove_variable_names (GList * variable_names);

/* Return TRUE if the variable is defined, FALSE otherwise. If defined
 * and value_p != NULL, return the value in *value_p, otherwise, *value_p
 * is unchanged. */
gboolean gnc_exp_parser_get_value (const char * variable_name,
                                   double *value_p);

/* Set the value of the variable to the given value. If the variable is
 * not already defined, it will be after the call. */
void gnc_exp_parser_set_value (const char * variable_name, double value);

/* Parse the given expression using the current variable definitions.
 * If the parse was successful, return TRUE and, if value_p is
 * non-NULL, return the value of the resulting expression in *value_p.
 * Otherwise, return FALSE and *value_p is unchanged. If FALSE is
 * returned and error_loc_p is non-NULL, *error_loc_p is set to the
 * character in expression where parsing aborted. If TRUE is returned
 * and error_loc_p is non-NULL, *error_loc_p is set to NULL. */
gboolean gnc_exp_parser_parse (const char * expression, double *value_p,
                               char **error_loc_p);

/* If the last parse returned FALSE, return an error string describing
 * the problem. Otherwise, return NULL. */
const char * gnc_exp_parser_error_string (void);

#endif
