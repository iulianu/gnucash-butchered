/********************************************************************\
 * Backend.c -- utility routines for dealing with the data backend  *
 * Copyright (C) 2000 Linas Vepstas <linas@linas.org>               *
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

#include "Account.h"
#include "AccountP.h"
#include "BackendP.h"
#include "Group.h"
#include "GroupP.h"
#include "gnc-engine-util.h"
#include "TransactionP.h"

/* static short module = MOD_ENGINE; */


/********************************************************************\
 * Fetch the backend                                                *
\********************************************************************/

Backend *
xaccAccountGetBackend (Account * acc)
{
  Account *parent_acc;
  AccountGroup * grp;

  if (!acc) return NULL;

  /* find the first account group that has a backend */
  grp = (AccountGroup *) acc->parent;
  while (grp) {
    if (grp->backend) return (grp->backend);
    parent_acc = grp -> parent;
    grp = NULL;
    if (parent_acc) {
       grp = (AccountGroup *) parent_acc->parent;
    }
  }
  return NULL;
}

/********************************************************************\
 * Fetch the backend                                                *
\********************************************************************/

Backend *
xaccTransactionGetBackend (Transaction *trans)
{
  GList *node;
  Split *s;

  if (!trans) return NULL;

  /* find an account */
  s = xaccTransGetSplit (trans, 0);
  if (!s) return NULL;

  /* I suppose it would be more 'technically correct' to make sure that
   * all splits share the same backend, and flag an error if they
   * don't.  However, at this point, it seems quite unlikely, so we'll
   * just use the first backend we find.
   */
  return xaccAccountGetBackend (s->acc);
}

/********************************************************************\
 * Set the backend                                                  *
\********************************************************************/

void 
xaccGroupSetBackend (AccountGroup *grp, Backend *be)
{
   if (!grp) return;
   grp->backend = be;
}

Backend *
xaccGroupGetBackend (AccountGroup *grp)
{
   if (!grp) return NULL;
   return (grp->backend);
}

/************************* END OF FILE ********************************/
