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
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/
/**
 * @addtogroup Engine
 * @{ */
/**
   @addtogroup SchedXaction Scheduled/Periodic/Recurring Transactions

   Scheduled Transactions provide a framework for remembering
   information about a transactions that are set to occur in the 
   future, either once or periodically.
 @{ */
/**
 * @file SchedXaction.h
 * @brief Scheduled Transactions public handling routines.
 * @author Copyright (C) 2001 Joshua Sled <jsled@asynchronous.org>
*/

#ifndef XACC_SCHEDXACTION_H
#define XACC_SCHEDXACTION_H

#include <time.h>
#include <glib.h>
#include "qof.h"
#include "FreqSpec.h"
#include "gnc-engine.h"

/**
 * The SchedXaction data.
*/
typedef struct gncp_SchedXaction SchedXaction;

/**
 * Creates and initializes a scheduled transaction.
*/
SchedXaction *xaccSchedXactionMalloc(QofBook *book);

/**
 * Cleans up and frees a SchedXaction and it's associated data.
*/
void xaccSchedXactionFree( SchedXaction *sx );

FreqSpec *xaccSchedXactionGetFreqSpec( SchedXaction *sx );
/**
 * The FreqSpec is given to the SchedXaction for mem mgmt; it should
 * not be freed by the external code.
*/
void xaccSchedXactionSetFreqSpec( SchedXaction *sx, FreqSpec *fs );

gchar *xaccSchedXactionGetName( SchedXaction *sx );
/**
 * A copy of the name is made.
*/
void xaccSchedXactionSetName( SchedXaction *sx, const gchar *newName );

GDate* xaccSchedXactionGetStartDate( SchedXaction *sx );
void xaccSchedXactionSetStartDate( SchedXaction *sx, GDate* newStart );

int xaccSchedXactionHasEndDate( SchedXaction *sx );
/**
 * Returns invalid date when there is no end-date specified.
*/
GDate* xaccSchedXactionGetEndDate( SchedXaction *sx );
/**
 * Set to an invalid GDate to turn off 'end-date' definition.
*/
void xaccSchedXactionSetEndDate( SchedXaction *sx, GDate* newEnd );

GDate* xaccSchedXactionGetLastOccurDate( SchedXaction *sx );
void xaccSchedXactionSetLastOccurDate( SchedXaction *sx, GDate* newLastOccur );

/**
 * Returns true if the scheduled transaction has a defined number of
 * occurances, false if not.
*/
gboolean xaccSchedXactionHasOccurDef( SchedXaction *sx );
gint xaccSchedXactionGetNumOccur( SchedXaction *sx );
/**
 * Set to '0' to turn off number-of-occurances definition.
*/
void xaccSchedXactionSetNumOccur( SchedXaction *sx, gint numNum );
gint xaccSchedXactionGetRemOccur( SchedXaction *sx );
void xaccSchedXactionSetRemOccur( SchedXaction *sx, gint numRemain );

/** \brief Set the instance count.
 *
 *   This is incremented by one for every created
 * instance of the SX.  Returns the instance num of the SX unless stateData
 * is non-null, in which case it returns the instance num from the state
 * data.
 * @param sx The instance whose state should be retrieved.
 * @param stateData may be NULL.
*/
gint gnc_sx_get_instance_count( SchedXaction *sx, void *stateData );
/**
 * Sets the instance count to something other than the default.  As the
 * default is the incorrect value '0', callers should DTRT here.
*/
void gnc_sx_set_instance_count( SchedXaction *sx, gint instanceNum );

GList *xaccSchedXactionGetSplits( SchedXaction *sx );
void xaccSchedXactionSetSplits( SchedXaction *sx, GList *newSplits );

void xaccSchedXactionGetAutoCreate( SchedXaction *sx,
                                    gboolean *outAutoCreate,
                                    gboolean *outNotify );
void xaccSchedXactionSetAutoCreate( SchedXaction *sx,
                                    gboolean newAutoCreate,
                                    gboolean newNotify );

gint xaccSchedXactionGetAdvanceCreation( SchedXaction *sx );
void xaccSchedXactionSetAdvanceCreation( SchedXaction *sx, gint createDays );

gint xaccSchedXactionGetAdvanceReminder( SchedXaction *sx );
void xaccSchedXactionSetAdvanceReminder( SchedXaction *sx, gint reminderDays );

/** \name Temporal state data.
 *
 * These functions allow us to opaquely save the entire temporal state of
 * ScheduledTransactions.  This is used by the "since-last-run" dialog to
 * store the initial state of SXes before modification ... if it later
 * becomes necessary to revert an entire set of changes, we can 'revert' the
 * SX without having to rollback all the individual state changes.
@{
*/
void *gnc_sx_create_temporal_state( SchedXaction *sx );
void gnc_sx_incr_temporal_state( SchedXaction *sx, void *stateData );
void gnc_sx_revert_to_temporal_state( SchedXaction *sx,
                                      void *stateData );
void gnc_sx_destroy_temporal_state( void *stateData );
/** \brief Allocates and returns a copy of the given temporal state.

 *   Destroy with gnc_sx_destroy_temporal_state(), as you'd expect.
*/
void *gnc_sx_clone_temporal_state( void *stateData );
/** @} */

/** \brief Returns the next occurance of a scheduled transaction.

 *   If the transaction hasn't occured, then it's based off the start date.
 * Otherwise, it's based off the last-occurance date.
 *
 * If state data is NULL, the current value of the SX is used for
 * computation.  Otherwise, the values in the state data are used.  This
 * allows the caller to correctly create a set of instances into the future
 * for possible action without modifying the SX state until action is
 * actually taken.
*/
GDate xaccSchedXactionGetNextInstance( SchedXaction *sx, void *stateData );
GDate xaccSchedXactionGetInstanceAfter( SchedXaction *sx,
                                        GDate *date,
                                        void *stateData );

/** \brief Set the schedxaction's template transaction.

t_t_list is a glist of TTInfo's as defined in SX-ttinfo.h.
The edit dialog doesn't use this mechanism; maybe it should.
*/
void xaccSchedXactionSetTemplateTrans( SchedXaction *sx,
                                       GList *t_t_list,
                                       QofBook *book );

/** \brief Adds an instance to the deferred list of the SX.

Added instances are added in date-sorted order.
*/
void gnc_sx_add_defer_instance( SchedXaction *sx, void *deferStateData );

/** \brief Removes an instance from the deferred list.

If the instance is no longer useful; gnc_sx_destroy_temporal_state() it.
*/
void gnc_sx_remove_defer_instance( SchedXaction *sx, void *deferStateData );

/** \brief Returns the defer list from the SX.

 This is a date-sorted state-data instance list.
 The list should not be modified by the caller; use the
 gnc_sx_{add,remove}_defer_instance() functions to modifiy the list.
*/
GList *gnc_sx_get_defer_instances( SchedXaction *sx );

/* #defines for KvpFrame strings and QOF */
#define GNC_SX_ID                    "sched-xaction"
#define GNC_SX_ACCOUNT               "account"
#define GNC_SX_CREDIT_FORMULA        "credit-formula"
#define GNC_SX_DEBIT_FORMULA         "debit-formula"
#define GNC_SX_SHARES                "shares"
#define GNC_SX_AMOUNT                "amnt"
#define GNC_SX_FROM_SCHED_XACTION    "from-sched-xaction"
#define GNC_SX_FREQ_SPEC             "scheduled-frequency"
#define GNC_SX_NAME                  "sched-xname"
#define GNC_SX_START_DATE            "sched-start-date"
#define GNC_SX_LAST_DATE             "sched-last-date"
#define GNC_SX_NUM_OCCUR             "sx-total-number"
#define GNC_SX_REM_OCCUR             "sx-remaining-num"

/** \brief QOF registration. */
gboolean SXRegister (void);

/** \deprecated */
#define xaccSchedXactionIsDirty(X) qof_instance_is_dirty (QOF_INSTANCE(X))
/** \deprecated */
#define xaccSchedXactionGetGUID(X) qof_entity_get_guid(QOF_ENTITY(X))
/** \deprecated */
#define xaccSchedXactionGetSlots(X) qof_instance_get_slots(QOF_INSTANCE(X))

/** \deprecated to be replaced with 'dirty' kvp's */
KvpValue *xaccSchedXactionGetSlot( SchedXaction *sx, 
				    const char *slot );
/** \deprecated to be replaced with 'dirty' kvp's */
void xaccSchedXactionSetSlot( SchedXaction *sx, 
			      const char *slot,
			      const KvpValue *value );


#endif /* XACC_SCHEDXACTION_H */

/** @} */
/** @} */
