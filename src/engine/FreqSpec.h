/********************************************************************\
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
/** @addtogroup Engine_SchedXaction
    @{ */
/** @file FreqSpec.h
    @brief Frequency Specification
    @author Copyright (C) 2001 Joshua Sled <jsled@asynchronous.org>
    @author Copyright (C) 2001 Ben Stanley <bds02@uow.edu.au>  
*/

#ifndef XACC_FREQSPEC_H
#define XACC_FREQSPEC_H

#include "config.h"

#include <glib.h>

#include "gnc-engine.h"
#include "guid.h"
#include "qofbook.h"

/**
 * Frequency specification.
 *
 **/
typedef enum gncp_FreqType {
        INVALID,
        ONCE,
        DAILY,
        WEEKLY, /* Hmmm... This is sort of DAILY[7]... */
        /* BI_WEEKLY: weekly[2] */
        /* SEMI_MONTHLY: use composite */
        MONTHLY,
        MONTH_RELATIVE,
        /* YEARLY: monthly[12] */
        COMPOSITE,
} FreqType;

/**
 * The user's conception of the frequency.  It is expected that this
 * list will grow, while the former [FreqType] will not.
 *
 * Ideally this is not here, but what can you do?
 **/
typedef enum gncp_UIFreqType {
        UIFREQ_NONE,
        UIFREQ_ONCE,
        UIFREQ_DAILY,
        UIFREQ_DAILY_MF,
        UIFREQ_WEEKLY,
        UIFREQ_BI_WEEKLY,
        UIFREQ_SEMI_MONTHLY,
        UIFREQ_MONTHLY,
        UIFREQ_QUARTERLY,
        UIFREQ_TRI_ANUALLY,
        UIFREQ_SEMI_YEARLY,
        UIFREQ_YEARLY,
        UIFREQ_NUM_UI_FREQSPECS
} UIFreqType;

/**
 * Forward declaration of FreqSpec type for storing
 * date repetition information. This is an opaque type.
 */

struct gncp_freq_spec;
typedef struct gncp_freq_spec FreqSpec;


/** PROTOTYPES ******************************************************/

/**
 * Allocates memory for a FreqSpec and initializes it.
 **/
FreqSpec* xaccFreqSpecMalloc(QofBook *book);

/**
 * destroys any private data belonging to the FreqSpec.
 * Use this for a stack object.
 */
void xaccFreqSpecCleanUp( FreqSpec *fs );

/**
 * Frees a heap allocated FreqSpec.
 * This is the opposite of xaccFreqSpecMalloc().
 **/
void xaccFreqSpecFree( FreqSpec *fs );

/**
 * Gets the type of a FreqSpec.
 **/
FreqType xaccFreqSpecGetType( FreqSpec *fs );

/**
 * Sets the type of a FreqSpec.
 * Setting the type re-initializes any spec-data; this means
 * destroying any sub-types in the case of COMPOSITE.
 *
 * \warning THESE FUNCTIONS HAVE NOT BEEN MAINTAINED THROUGH BEN'S CHANGES.
 * They need to be checked.
 **/
/* void xaccFreqSpecSetType( FreqSpec *fs, FreqType newType ); */
void xaccFreqSpecSetUIType( FreqSpec *fs, UIFreqType newUIFreqType );
/** DOCUMENT ME! */
UIFreqType xaccFreqSpecGetUIType( FreqSpec *fs );


/**
 * Sets the type to once-off, and initialises the
 * date it occurs on.
 * Disposes of any previous data.
 */
void xaccFreqSpecSetOnceDate( FreqSpec *fs,
                              const GDate* when );

/**
 * Sets the type to DAILY. Disposes of any previous data.
 * Uses the start date to figure
 * out how many days after epoch (1/1/1900) this repeat would
 * have first occurred on if it had been running back then.
 * This is used later to figure out absolute repeat dates.
 */
void xaccFreqSpecSetDaily( FreqSpec *fs,
                           const GDate* initial_date,
                           guint interval_days );

/**
 * Sets the type to WEEKLY. Disposes of any previous data.
 * Uses the inital date to figure out the day of the week to use.
 */
void xaccFreqSpecSetWeekly( FreqSpec *fs,
                            const GDate* inital_date,
                            guint interval_weeks );

/**
 * Sets the type to MONTHLY. Disposes of any previous data.
 * Uses the inital date to figure out the day of the month to use.
 */
void xaccFreqSpecSetMonthly( FreqSpec *fs,
                             const GDate* inital_date,
                             guint interval_months );

/**
 * Sets the type to MONTH_RELATIVE. Disposes of any previous data.
 * Uses the inital date to figure out the day of the month to use.
 */
void xaccFreqSpecSetMonthRelative( FreqSpec *fs,
                                   const GDate* inital_date,
                                   guint interval_months );

/**
 * Sets the type to COMPOSITE. Disposes of any previous data.
 * You must Add some repeats to the composite before using
 * it for repeating.
 */
void xaccFreqSpecSetComposite( FreqSpec *fs );

/**
 * Returns a human-readable string of the Frequency.  This uses
 * UIFreqType to unroll the internal structure.  It is an assertion
 * failure if the FreqSpec data doesn't match the UIFreqType.
 * Caller allocates the GString [natch].
 **/
void xaccFreqSpecGetFreqStr( FreqSpec *fs, GString *str );

/** DOCUMENT ME! */
int xaccFreqSpecGetOnce( FreqSpec *fs, GDate *outGD );
/** DOCUMENT ME! */
int xaccFreqSpecGetDaily( FreqSpec *fs, int *outRepeat );
/** DOCUMENT ME! */
int xaccFreqSpecGetWeekly( FreqSpec *fs, int *outRepeat, int *outDayOfWeek );
/** DOCUMENT ME! */
int xaccFreqSpecGetMonthly( FreqSpec *fs, int *outRepeat,
                            int *outDayOfMonth, int *outMonthOffset );
/* FIXME: add month-relative */

/**
 * Returns the list of FreqSpecs in a COMPOSITE FreqSpec.
 * It is an error to use this if the type is not COMPOSITE.
 * The caller should not modify this list;
 * only add/remove composites and use this fn to get
 * the perhaps-changed list head.
 **/
GList* xaccFreqSpecCompositeGet( FreqSpec *fs );

/**
 * Adds a FreqSpec to the list in a COMPOSITE FreqSpec; if the
 * FreqSpec is not COMPOSITE, this is an assertion failure.
 **/
void xaccFreqSpecCompositeAdd( FreqSpec *fs, FreqSpec *fsToAdd );

/**
 * Returns the next instance of the FreqSpec after a given input date.
 * Note that if the given date happens to be a repeat date,
 * then the next repeat date will be returned.
 **/
void xaccFreqSpecGetNextInstance( FreqSpec *fs,
                                  const GDate* in_date,
                                  GDate* out_date );

/**
 * qsort-style comparison of FreqSpecs.
 * More frequently-occuring FreqSpecs are sorted before less-frequent FreqSpecs.
 * FIXME: What to do for composites?
 **/
int gnc_freq_spec_compare( FreqSpec *a, FreqSpec *b );

#endif /* XACC_FREQSPEC_H */
/**@}*/
