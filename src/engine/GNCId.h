/********************************************************************\
 * GNCId.h -- Gnucash entity identifier API                         *
 * Copyright (C) 2000 Dave Peticolas <peticola@cs.ucdavis.edu>      *
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
 *                                                                  *
\********************************************************************/

#ifndef __GNC_ID__
#define __GNC_ID__ 1

/* This file defines an API for using gnucash entity identifiers.
 *
 * Identifiers can be used to reference Accounts, Transactions, and
 * Splits. These four Gnucash types are referred to as Gnucash
 * entities. Identifiers are globally-unique and permanent, i.e., once
 * an entity has been assigned an identifier, it retains that same
 * identifier for its lifetime.
 *
 * Identifiers can be encoded as hex strings. */

#include <config.h>

#include "guid.h"
#include "util.h"


/* Identifiers are 'typed' with integers. The ids used in gnucash are
 * defined below. An id with type GNC_ID_NONE does not refer to any
 * entity, although that may change as new ids are created. An id with
 * type GNC_ID_NULL does not refer to any entity, and will never refer
 * to any entity. An identifier with any other type may refer to an
 * actual entity, but that is not guaranteed. If an id does refer to
 * an entity, the type of the entity will match the type of the
 * identifier. */
typedef enum
{
  GNC_ID_NONE = 0,
  GNC_ID_NULL,
  GNC_ID_ACCOUNT,
  GNC_ID_TRANS,
  GNC_ID_SPLIT,
  LAST_GNC_ID = GNC_ID_SPLIT
} GNCIdType;


/* Return the type of an identifier. */
GNCIdType xaccGUIDType(const GUID * guid);

/* Returns a GUID which is guaranteed to never reference any entity. */
const GUID * xaccGUIDNULL(void);

#endif
