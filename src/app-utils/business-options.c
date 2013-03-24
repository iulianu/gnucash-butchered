/*
 * business-options.c -- Non-GUI Option Utilities for GNC Business Objects
 *
 * Written By: Derek Atkins <warlord@MIT.EDU>
 * Copyright (C) 2003  Derek Atkins
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
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652
 * Boston, MA  02110-1301,  USA       gnu@gnu.org
 */

#include "config.h"

#include "business-options.h"

#define FUNC_NAME G_STRFUNC

#define LOOKUP_OPTION(fcn) \
  GNCOption *option; \
  SCM getter; \
  SCM value; \
  \
  option = gnc_option_db_get_option_by_name (odb, section, name); \
  \
  if (option == NULL) \
    return default_value; \
  \
  getter = gnc_option_getter (option); \
  if (getter == SCM_UNDEFINED) \
    return default_value; \
  \
  value = scm_call_0 (getter); \
  if (value == SCM_BOOL_F) \
    return NULL; \
  SWIG_GetModule(NULL); /* Work-around for SWIG bug. */       \
  if (!SWIG_IsPointer(value))             \
    scm_misc_error(fcn, "SCM is not a wrapped pointer.", value)
