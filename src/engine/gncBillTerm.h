/********************************************************************\
 * gncBillTerm.h -- the Gnucash Billing Term interface              *
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
/** @addtogroup BillTerm
    @{ */
/** @file gncBillTerm.h
    @brief Billing Term interface
    @author Copyright (C) 2002 Derek Atkins <warlord@MIT.EDU>
*/

#ifndef GNC_BILLTERM_H_
#define GNC_BILLTERM_H_

class GncBillTerm;

#include "qof.h"
#ifdef GNUCASH_MAJOR_VERSION
#include "gncBusiness.h"
#endif
#define GNC_ID_BILLTERM       "gncBillTerm"

/** @name BillTerm parameter names
 @{ */
#define GNC_BILLTERM_NAME 		"name"
#define GNC_BILLTERM_DESC 		"description"
#define GNC_BILLTERM_DUEDAYS 	"number of days due"
#define GNC_BILLTERM_DISCDAYS 	"number of discounted days"
#define GNC_BILLTERM_CUTOFF 	"cut off"
#define GNC_BILLTERM_TYPE 		"bill type"
#define GNC_BILLTERM_DISCOUNT	"amount of discount"
#define GNC_BILLTERM_REFCOUNT	"reference count"
/** @} */

/**
 * How to interpret the amount.
 * You can interpret it as a VALUE or a PERCENT.
 * ??? huh?
 * NOTE: This enum /depends/ on starting at value 1
 */
#define ENUM_TERMS_TYPE(_)  \
 _(GNC_TERM_TYPE_DAYS,=1) \
 _(GNC_TERM_TYPE_PROXIMO,)

DEFINE_ENUM(GncBillTermType, ENUM_TERMS_TYPE)

/** @name Create/Destroy Functions
 @{ */
GncBillTerm * gncBillTermCreate (QofBook *book);
void gncBillTermDestroy (GncBillTerm *term);
void gncBillTermIncRef (GncBillTerm *term);
void gncBillTermDecRef (GncBillTerm *term);

void gncBillTermChanged (GncBillTerm *term);
void gncBillTermBeginEdit (GncBillTerm *term);
void gncBillTermCommitEdit (GncBillTerm *term);
/** @} */

/** @name Set Functions
@{
*/
void gncBillTermSetName (GncBillTerm *term, const char *name);
void gncBillTermSetDescription (GncBillTerm *term, const char *name);
void gncBillTermSetType (GncBillTerm *term, GncBillTermType type);
void gncBillTermSetDueDays (GncBillTerm *term, gint days);
void gncBillTermSetDiscountDays (GncBillTerm *term, gint days);
void gncBillTermSetDiscount (GncBillTerm *term, gnc_numeric discount);
void gncBillTermSetCutoff (GncBillTerm *term, gint cutoff);

/** @} */

/** @name Get Functions
 @{ */
/** Return a pointer to the instance gncBillTerm that is identified
 *  by the guid, and is residing in the book. Returns NULL if the
 *  instance can't be found.
 *  Equivalent function prototype is
 *  GncBillTerm * gncBillTermLookup (QofBook *book, const GncGUID *guid);
 */
static inline GncBillTerm * gncBillTermLookup (const QofBook *book, const GncGUID *guid)
{
    QOF_BOOK_RETURN_ENTITY(book, guid, GNC_ID_BILLTERM, GncBillTerm);
}

GncBillTerm *gncBillTermLookupByName (QofBook *book, const char *name);
GList * gncBillTermGetTerms (QofBook *book);

const char *gncBillTermGetName (const GncBillTerm *term);
const char *gncBillTermGetDescription (const GncBillTerm *term);
GncBillTermType gncBillTermGetType (const GncBillTerm *term);
gint gncBillTermGetDueDays (const GncBillTerm *term);
gint gncBillTermGetDiscountDays (const GncBillTerm *term);
gnc_numeric gncBillTermGetDiscount (const GncBillTerm *term);
gint gncBillTermGetCutoff (const GncBillTerm *term);

bool gncBillTermIsDirty (const GncBillTerm *term);

GncBillTerm *gncBillTermGetParent (const GncBillTerm *term);
GncBillTerm *gncBillTermReturnChild (GncBillTerm *term, bool make_new);
#define gncBillTermGetChild(t) gncBillTermReturnChild((t),FALSE)
gint64 gncBillTermGetRefcount (const GncBillTerm *term);
/** @} */

/** @name Comparison Functions
 @{ */
/** Compare BillTerms on their name for sorting. */
int gncBillTermCompare (const GncBillTerm *a, const GncBillTerm *b);
/** Check if all internal fields of a and b match. */
bool gncBillTermEqual(const GncBillTerm *a, const GncBillTerm *b);
/** Check only if the bill terms are "family". This is the case if
 *  - a and b are the same bill term
 *  - a is b's parent or vice versa
 *  - a and be are children of the same parent
 *
 *  In practice, this check if performed by comparing the bill term's names.
 *  This is required to be unique per parent/children group.
 */
bool gncBillTermIsFamily (const GncBillTerm *a, const GncBillTerm *b);
/** @} */

/********************************************************/
/* functions to compute dates from Bill Terms           */

/* Compute the due date and discount dates from the post date */
Timespec gncBillTermComputeDueDate (const GncBillTerm *term, Timespec post_date);

/* deprecated */
#define gncBillTermGetGUID(x) qof_instance_get_guid (QOF_INSTANCE(x))

#endif /* GNC_BILLTERM_H_ */
/** @} */
/** @} */
