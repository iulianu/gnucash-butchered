/*
 * gncJob.c -- the Core Job Interface
 * Copyright (C) 2001, 2002 Derek Atkins
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#include "config.h"

#include <glib.h>
#include <string.h>

#include "guid.h"
#include "messages.h"
#include "gnc-engine-util.h"
#include "gnc-numeric.h"
#include "gnc-book-p.h"
#include "GNCIdP.h"
#include "QueryObject.h"
#include "gnc-event-p.h"
#include "gnc-be-utils.h"

#include "gncBusiness.h"
#include "gncJob.h"
#include "gncJobP.h"

struct _gncJob {
  GNCBook *	book;
  GUID		guid;
  char *	id;
  char *	name;
  char *	desc;
  GncOwner	owner;
  gboolean	active;

  int		editlevel;
  gboolean	do_free;
  gboolean	dirty;
};

static short	module = MOD_BUSINESS;

#define _GNC_MOD_NAME	GNC_JOB_MODULE_NAME

#define CACHE_INSERT(str) g_cache_insert(gnc_engine_get_string_cache(), (gpointer)(str));
#define CACHE_REMOVE(str) g_cache_remove(gnc_engine_get_string_cache(), (str));

static void addObj (GncJob *job);
static void remObj (GncJob *job);

G_INLINE_FUNC void mark_job (GncJob *job);
G_INLINE_FUNC void
mark_job (GncJob *job)
{
  job->dirty = TRUE;
  gncBusinessSetDirtyFlag (job->book, _GNC_MOD_NAME, TRUE);

  gnc_engine_generate_event (&job->guid, GNC_EVENT_MODIFY);
}

/* Create/Destroy Functions */

GncJob *gncJobCreate (GNCBook *book)
{
  GncJob *job;

  if (!book) return NULL;

  job = g_new0 (GncJob, 1);
  job->book = book;
  job->dirty = FALSE;

  job->id = CACHE_INSERT ("");
  job->name = CACHE_INSERT ("");
  job->desc = CACHE_INSERT ("");
  job->active = TRUE;

  xaccGUIDNew (&job->guid, book);
  addObj (job);

  gnc_engine_generate_event (&job->guid, GNC_EVENT_CREATE);

  return job;
}

void gncJobDestroy (GncJob *job)
{
  if (!job) return;
  job->do_free = TRUE;
  gncJobCommitEdit (job);
}

static void gncJobFree (GncJob *job)
{
  if (!job) return;

  gnc_engine_generate_event (&job->guid, GNC_EVENT_DESTROY);

  CACHE_REMOVE (job->id);
  CACHE_REMOVE (job->name);
  CACHE_REMOVE (job->desc);

  switch (gncOwnerGetType (&(job->owner))) {
  case GNC_OWNER_CUSTOMER:
    gncCustomerRemoveJob (gncOwnerGetCustomer(&job->owner), job);
    break;
  case GNC_OWNER_VENDOR:
    gncVendorRemoveJob (gncOwnerGetVendor(&job->owner), job);
    break;
  default:
  }

  remObj (job);

  g_free (job);
}

/* Set Functions */

#define SET_STR(obj, member, str) { \
	char * tmp; \
	\
	if (!safe_strcmp (member, str)) return; \
	gncJobBeginEdit (obj); \
	tmp = CACHE_INSERT (str); \
	CACHE_REMOVE (member); \
	member = tmp; \
	}

void gncJobSetID (GncJob *job, const char *id)
{
  if (!job) return;
  if (!id) return;
  SET_STR(job, job->id, id);
  mark_job (job);
  gncJobCommitEdit (job);
}

void gncJobSetName (GncJob *job, const char *name)
{
  if (!job) return;
  if (!name) return;
  SET_STR(job, job->name, name);
  mark_job (job);
  gncJobCommitEdit (job);
}

void gncJobSetReference (GncJob *job, const char *desc)
{
  if (!job) return;
  if (!desc) return;
  SET_STR(job, job->desc, desc);
  mark_job (job);
  gncJobCommitEdit (job);
}

void gncJobSetGUID (GncJob *job, const GUID *guid)
{
  if (!job || !guid) return;
  if (guid_equal (guid, &job->guid)) return;

  gncJobBeginEdit (job);
  remObj (job);
  job->guid = *guid;
  addObj (job);
  gncJobCommitEdit (job);
}

void gncJobSetOwner (GncJob *job, GncOwner *owner)
{
  if (!job) return;
  if (!owner) return;
  if (gncOwnerEqual (owner, &(job->owner))) return;
  /* XXX: Fail if we have ANY orders or invoices */

  gncJobBeginEdit (job);

  switch (gncOwnerGetType (&(job->owner))) {
  case GNC_OWNER_CUSTOMER:
    gncCustomerRemoveJob (gncOwnerGetCustomer(&job->owner), job);
    break;
  case GNC_OWNER_VENDOR:
    gncVendorRemoveJob (gncOwnerGetVendor(&job->owner), job);
    break;
  default:
  }

  gncOwnerCopy (owner, &(job->owner));

  switch (gncOwnerGetType (&(job->owner))) {
  case GNC_OWNER_CUSTOMER:
    gncCustomerAddJob (gncOwnerGetCustomer(&job->owner), job);
    break;
  case GNC_OWNER_VENDOR:
    gncVendorAddJob (gncOwnerGetVendor(&job->owner), job);
    break;
  default:
  }

  mark_job (job);
  gncJobCommitEdit (job);
}

void gncJobSetActive (GncJob *job, gboolean active)
{
  if (!job) return;
  if (active == job->active) return;
  gncJobBeginEdit (job);
  job->active = active;
  mark_job (job);
  gncJobCommitEdit (job);
}

void gncJobBeginEdit (GncJob *job)
{
  GNC_BEGIN_EDIT (job, _GNC_MOD_NAME);
}

static void gncJobOnError (GncJob *job, GNCBackendError errcode)
{
  PERR("Job Backend Failure: %d", errcode);
}

static void gncJobOnDone (GncJob *job)
{
  job->dirty = FALSE;
}

void gncJobCommitEdit (GncJob *job)
{
  GNC_COMMIT_EDIT_PART1 (job);
  GNC_COMMIT_EDIT_PART2 (job, _GNC_MOD_NAME, gncJobOnError,
			 gncJobOnDone, gncJobFree);
}

/* Get Functions */

GNCBook * gncJobGetBook (GncJob *job)
{
  if (!job) return NULL;
  return job->book;
}

const char * gncJobGetID (GncJob *job)
{
  if (!job) return NULL;
  return job->id;
}

const char * gncJobGetName (GncJob *job)
{
  if (!job) return NULL;
  return job->name;
}

const char * gncJobGetReference (GncJob *job)
{
  if (!job) return NULL;
  return job->desc;
}

GncOwner * gncJobGetOwner (GncJob *job)
{
  if (!job) return NULL;
  return &(job->owner);
}

const GUID * gncJobGetGUID (GncJob *job)
{
  if (!job) return NULL;
  return &job->guid;
}

GUID gncJobRetGUID (GncJob *job)
{
  const GUID *guid = gncJobGetGUID (job);
  if (guid)
    return *guid;
  return *xaccGUIDNULL ();
}

gboolean gncJobGetActive (GncJob *job)
{
  if (!job) return FALSE;
  return job->active;
}

GncJob * gncJobLookup (GNCBook *book, const GUID *guid)
{
  if (!book || !guid) return NULL;
  return xaccLookupEntity (gnc_book_get_entity_table (book),
			   guid, _GNC_MOD_NAME);
}

GncJob * gncJobLookupDirect (GUID guid, GNCBook *book)
{
  if (!book) return NULL;
  return gncJobLookup (book, &guid);
}

gboolean gncJobIsDirty (GncJob *job)
{
  if (!job) return FALSE;
  return job->dirty;
}

/* Other functions */

int gncJobCompare (const GncJob * a, const GncJob *b) {
  if (!a && !b) return 0;
  if (!a && b) return 1;
  if (a && !b) return -1;

  return (safe_strcmp(a->id, b->id));
}


/* Package-Private functions */

static void addObj (GncJob *job)
{
  gncBusinessAddObject (job->book, _GNC_MOD_NAME, job, &job->guid);
}

static void remObj (GncJob *job)
{
  gncBusinessRemoveObject (job->book, _GNC_MOD_NAME, &job->guid);
}

static void _gncJobCreate (GNCBook *book)
{
  gncBusinessCreate (book, _GNC_MOD_NAME);
}

static void _gncJobDestroy (GNCBook *book)
{
  gncBusinessDestroy (book, _GNC_MOD_NAME);
}

static gboolean _gncJobIsDirty (GNCBook *book)
{
  return gncBusinessIsDirty (book, _GNC_MOD_NAME);
}

static void _gncJobMarkClean (GNCBook *book)
{
  gncBusinessSetDirtyFlag (book, _GNC_MOD_NAME, FALSE);
}

static void _gncJobForeach (GNCBook *book, foreachObjectCB cb,
			    gpointer user_data)
{
  gncBusinessForeach (book, _GNC_MOD_NAME, cb, user_data);
}

static const char * _gncJobPrintable (gpointer item)
{
  GncJob *c;

  if (!item) return NULL;

  c = item;
  return c->name;
}

static GncObject_t gncJobDesc = {
  GNC_OBJECT_VERSION,
  _GNC_MOD_NAME,
  "Job",
  _gncJobCreate,
  _gncJobDestroy,
  _gncJobIsDirty,
  _gncJobMarkClean,
  _gncJobForeach,
  _gncJobPrintable
};

gboolean gncJobRegister (void)
{
  static QueryObjectDef params[] = {
    { JOB_ID, QUERYCORE_STRING, (QueryAccess)gncJobGetID },
    { JOB_NAME, QUERYCORE_STRING, (QueryAccess)gncJobGetName },
    { JOB_REFERENCE, QUERYCORE_STRING, (QueryAccess)gncJobGetReference },
    { JOB_OWNER, GNC_OWNER_MODULE_NAME, (QueryAccess)gncJobGetOwner },
    { JOB_ACTIVE, QUERYCORE_BOOLEAN, (QueryAccess)gncJobGetActive },
    { QUERY_PARAM_BOOK, GNC_ID_BOOK, (QueryAccess)gncJobGetBook },
    { QUERY_PARAM_GUID, QUERYCORE_GUID, (QueryAccess)gncJobGetGUID },
    { NULL },
  };

  gncQueryObjectRegister (_GNC_MOD_NAME, (QuerySort)gncJobCompare, params);

  return gncObjectRegister (&gncJobDesc);
}

gint64 gncJobNextID (GNCBook *book)
{
  return gnc_book_get_counter (book, _GNC_MOD_NAME);
}
