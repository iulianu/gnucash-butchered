/********************************************************************\
 * gncJob.h -- the Core Job Interface                               *
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
/** @addtogroup Business
    @{ */
/** @addtogroup Job
    @{ */
/** @file gncJob.h
    @brief  Job Interface
    @author Copyright (C) 2001, 2002 Derek Atkins <warlord@MIT.EDU>
*/

#ifndef GNC_JOB_H_
#define GNC_JOB_H_

typedef struct _gncJob GncJob;

#include "gncAddress.h"
#include "gncOwner.h"

#define GNC_ID_JOB "gncJob"
#define GNC_IS_JOB(obj)  (QOF_CHECK_TYPE((obj), GNC_ID_JOB))
#define GNC_JOB(obj)     (QOF_CHECK_CAST((obj), GNC_ID_JOB, GncJob))

/* Create/Destroy Functions */

GncJob *gncJobCreate (QofBook *book);
void gncJobDestroy (GncJob *job);

/** \name Set Functions
@{
*/

void gncJobSetID (GncJob *job, const char *id);
void gncJobSetName (GncJob *job, const char *jobname);
void gncJobSetReference (GncJob *job, const char *owner_reference);
void gncJobSetOwner (GncJob *job, GncOwner *owner);
void gncJobSetActive (GncJob *job, gboolean active);

/** @} */
void gncJobBeginEdit (GncJob *job);
void gncJobCommitEdit (GncJob *job);

/** \name Get Functions
@{
*/

const char * gncJobGetID (GncJob *job);
const char * gncJobGetName (GncJob *job);
const char * gncJobGetReference (GncJob *job);
GncOwner * gncJobGetOwner (GncJob *job);

/** @} */
gboolean gncJobGetActive (GncJob *job);

/** Return a pointer to the instance gncJob that is identified
 *  by the guid, and is residing in the book. Returns NULL if the
 *  instance can't be found.
 *  Equivalent function prototype is
 *  GncJob * gncJobLookup (QofBook *book, const GUID *guid);
 */
#define gncJobLookup(book,guid)    \
       QOF_BOOK_LOOKUP_ENTITY((book),(guid),GNC_ID_JOB, GncJob)

/* Other functions */

int gncJobCompare (const GncJob *a, const GncJob *b);

#define JOB_ID		"id"
#define JOB_NAME	"name"
#define JOB_REFERENCE	"reference"
#define JOB_OWNER	"owner"
#define Q_JOB_OWNER "owner_collection"
#define JOB_ACTIVE	"active"

/** deprecated functions */
#define gncJobGetBook(x) qof_instance_get_book(QOF_INSTANCE(x))
#define gncJobGetGUID(x) qof_instance_get_guid(QOF_INSTANCE(x))
#define gncJobRetGUID(x) (x ? *(qof_instance_get_guid(QOF_INSTANCE(x))) : *(guid_null()))
#define gncJobLookupDirect(G,B) gncJobLookup((B),&(G))

#endif /* GNC_JOB_H_ */
/** @} */
/** @} */
