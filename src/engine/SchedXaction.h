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
#include "Recurrence.h"
#include "gnc-engine.h"
#include <list>

/** Just the variable temporal bits from the SX structure. */
struct SXTmpStateData
{
    GDate last_date;
    int num_occur_rem;
    int num_inst;
} ;

/**
 * A single scheduled transaction.
 *
 * Scheduled transactions have a list of transactions, and a frequency
 * [and associated date anchors] with which they are scheduled.
 *
 * Things that make sense to have in a template transaction:
 *   [not] Date [though eventually some/multiple template transactions
 *               might have relative dates].
 *   Memo
 *   Account
 *   Funds In/Out... or an expr involving 'amt' [A, x, y, a?] for
 *     variable expenses.
 *
 * Template transactions are instantiated by:
 *  . copying the fields of the template
 *  . setting the date to the calculated "due" date.
 *
 * We should be able to use the GeneralLedger [or, yet-another-subtype
 * of the internal ledger] for this editing.
 **/
class SchedXaction : public QofInstance
{
public:
    char           *name;

    RecurrenceList_t schedule;

    GDate           last_date;

    GDate           start_date;
    /* if end_date is invalid, then no end. */
    GDate           end_date;

    /* if num_occurances_total == 0, then no limit */
    int            num_occurances_total;
    /* remaining occurrences are as-of the 'last_date'. */
    int            num_occurances_remain;

    /* the current instance-count of the SX. */
    int            instance_num;

    bool        enabled;
    bool        autoCreateOption;
    bool        autoCreateNotify;
    int            advanceCreateDays;
    int            advanceRemindDays;

    Account        *template_acct;

    /** The list of deferred SX instances.  This list is of SXTmpStateData
     * instances.  */
    std::list<SXTmpStateData*> deferredList;
    
    SchedXaction();
};

#define xaccSchedXactionSetGUID(X,G) qof_instance_set_guid(QOF_INSTANCE(X),(G))

/**
 * Creates and initializes a scheduled transaction.
*/
SchedXaction *xaccSchedXactionMalloc(QofBook *book);

void sx_set_template_account (SchedXaction *sx, Account *account);

/**
 * Cleans up and frees a SchedXaction and its associated data.
*/
void xaccSchedXactionDestroy( SchedXaction *sx );

void gnc_sx_begin_edit (SchedXaction *sx);
void gnc_sx_commit_edit (SchedXaction *sx);

/*@ dependent @*/
RecurrenceList_t gnc_sx_get_schedule(const SchedXaction *sx);
/** @param[in] schedule A GList<Recurrence*> **/
void gnc_sx_set_schedule(SchedXaction *sx, const RecurrenceList_t &schedule);

char *xaccSchedXactionGetName( const SchedXaction *sx );
/**
 * A copy of the name is made.
*/
void xaccSchedXactionSetName( SchedXaction *sx, const char *newName );

const GDate* xaccSchedXactionGetStartDate(const SchedXaction *sx );
void xaccSchedXactionSetStartDate( SchedXaction *sx, const GDate* newStart );

bool xaccSchedXactionHasEndDate( const SchedXaction *sx );
/**
 * Returns invalid date when there is no end-date specified.
*/
const GDate* xaccSchedXactionGetEndDate(const SchedXaction *sx );
/**
 * Set to an invalid GDate to turn off 'end-date' definition.
*/
void xaccSchedXactionSetEndDate( SchedXaction *sx, const GDate* newEnd );

const GDate* xaccSchedXactionGetLastOccurDate(const SchedXaction *sx );
void xaccSchedXactionSetLastOccurDate( SchedXaction *sx, const GDate* newLastOccur );

/**
 * Returns true if the scheduled transaction has a defined number of
 * occurrences, false if not.
*/
bool xaccSchedXactionHasOccurDef( const SchedXaction *sx );
int xaccSchedXactionGetNumOccur( const SchedXaction *sx );
/**
 * Set to '0' to turn off number-of-occurrences definition.
*/
void xaccSchedXactionSetNumOccur( SchedXaction *sx, int numNum );
int xaccSchedXactionGetRemOccur( const SchedXaction *sx );
void xaccSchedXactionSetRemOccur( SchedXaction *sx, int numRemain );

/** Calculates and returns the number of occurrences of the given SX
 * in the given date range (inclusive). */
int gnc_sx_get_num_occur_daterange(const SchedXaction *sx, const GDate* start_date, const GDate* end_date);

/** \brief Get the instance count.
 *
 *   This is incremented by one for every created
 * instance of the SX.  Returns the instance num of the SX unless stateData
 * is non-null, in which case it returns the instance num from the state
 * data.
 * @param sx The instance whose state should be retrieved.
 * @param stateData may be NULL.
*/
int gnc_sx_get_instance_count( const SchedXaction *sx, SXTmpStateData *stateData );
/**
 * Sets the instance count to something other than the default.  As the
 * default is the incorrect value '0', callers should DTRT here.
*/
void gnc_sx_set_instance_count( SchedXaction *sx, int instanceNum );

SplitList_t xaccSchedXactionGetSplits( const SchedXaction *sx );

bool xaccSchedXactionGetEnabled( const SchedXaction *sx );
void xaccSchedXactionSetEnabled( SchedXaction *sx, bool newEnabled );

void xaccSchedXactionGetAutoCreate( const SchedXaction *sx,
                                    /*@ out @*/ bool *outAutoCreate,
                                    /*@ out @*/ bool *outNotify );
void xaccSchedXactionSetAutoCreate( SchedXaction *sx,
                                    bool newAutoCreate,
                                    bool newNotify );

int xaccSchedXactionGetAdvanceCreation( const SchedXaction *sx );
void xaccSchedXactionSetAdvanceCreation( SchedXaction *sx, int createDays );

int xaccSchedXactionGetAdvanceReminder( const SchedXaction *sx );
void xaccSchedXactionSetAdvanceReminder( SchedXaction *sx, int reminderDays );

/** \name Temporal state data.
 *
 * These functions allow us to opaquely save the entire temporal state of
 * ScheduledTransactions.  This is used by the "since-last-run" dialog to
 * store the initial state of SXes before modification ... if it later
 * becomes necessary to revert an entire set of changes, we can 'revert' the
 * SX without having to rollback all the individual state changes.
@{
*/
/** Allocates a new SXTmpStateData object and fills it with the
 * current state of the given sx.
 */
SXTmpStateData *gnc_sx_create_temporal_state(const SchedXaction *sx );

/** Calculates the next occurrence of the given SX and stores that
 * occurence in the remporalStateDate. The SX is unchanged. */
void gnc_sx_incr_temporal_state(const SchedXaction *sx, SXTmpStateData *stateData );

/** Frees the given stateDate object. */
void gnc_sx_destroy_temporal_state( SXTmpStateData *stateData );

/** \brief Allocates and returns a one-by-one copy of the given
 * temporal state.
 *
 * The caller must destroy the returned object with
 * gnc_sx_destroy_temporal_state() after usage.
*/
SXTmpStateData *gnc_sx_clone_temporal_state( SXTmpStateData *stateData );
/** @} */

/** \brief Returns the next occurrence of a scheduled transaction.
 *
 *   If the transaction hasn't occurred, then it's based off the start date.
 * Otherwise, it's based off the last-occurrence date.
 *
 * If state data is NULL, the current value of the SX is used for
 * computation.  Otherwise, the values in the state data are used.  This
 * allows the caller to correctly create a set of instances into the future
 * for possible action without modifying the SX state until action is
 * actually taken.
*/
GDate xaccSchedXactionGetNextInstance(const SchedXaction *sx, SXTmpStateData *stateData );
GDate xaccSchedXactionGetInstanceAfter(const SchedXaction *sx,
                                       GDate *date,
                                       SXTmpStateData *stateData );

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
void gnc_sx_add_defer_instance( SchedXaction *sx, SXTmpStateData *deferStateData );

/** \brief Removes an instance from the deferred list.

If the instance is no longer useful; gnc_sx_destroy_temporal_state() it.
*/
void gnc_sx_remove_defer_instance( SchedXaction *sx, SXTmpStateData *deferStateData );

/** \brief Returns the defer list from the SX.

 This is a date-sorted state-data instance list.
 The list should not be modified by the caller; use the
 gnc_sx_{add,remove}_defer_instance() functions to modify the list.
*/
std::list<SXTmpStateData*>
gnc_sx_get_defer_instances( SchedXaction *sx );

/* #defines for KvpFrame strings and QOF */
#define GNC_SX_ID                    "sched-xaction"
#define GNC_SX_ACCOUNT               "account"
#define GNC_SX_CREDIT_FORMULA        "credit-formula"
#define GNC_SX_DEBIT_FORMULA         "debit-formula"
#define GNC_SX_CREDIT_NUMERIC        "credit-numeric"
#define GNC_SX_DEBIT_NUMERIC         "debit-numeric"
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
bool SXRegister (void);

/** \deprecated */
#define xaccSchedXactionIsDirty(X) qof_instance_is_dirty (QOF_INSTANCE(X))
/** \deprecated */
#define xaccSchedXactionGetGUID(X) qof_entity_get_guid(QOF_INSTANCE(X))
/** \deprecated */
#define xaccSchedXactionGetSlots(X) qof_instance_get_slots(QOF_INSTANCE(X))

#endif /* XACC_SCHEDXACTION_H */

/** @} */
/** @} */
