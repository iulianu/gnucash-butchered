#include <glib.h>
#include <libguile.h>

#include "guid.h"
#include "gnc-module.h"
#include "gnc-engine-util.h"
#include "qofobject.h"

#include "gncJob.h"
#include "gncJobP.h"
#include "test-stuff.h"

static int count = 0;

static void
test_string_fcn (QofBook *book, const char *message,
		 void (*set) (GncJob *, const char *str),
		 const char * (*get)(GncJob *));

#if 0
static void
test_numeric_fcn (QofBook *book, const char *message,
		  void (*set) (GncJob *, gnc_numeric),
		  gnc_numeric (*get)(GncJob *));
#endif

static void
test_bool_fcn (QofBook *book, const char *message,
		  void (*set) (GncJob *, gboolean),
		  gboolean (*get) (GncJob *));

#if 0
static void
test_gint_fcn (QofBook *book, const char *message,
	       void (*set) (GncJob *, gint),
	       gint (*get) (GncJob *));
#endif

static void
test_job (void)
{
  QofBook *book;
  GncJob *job;

  book = qof_book_new ();

  /* Test creation/destruction */
  {
    do_test (gncJobCreate (NULL) == NULL, "job create NULL");
    job = gncJobCreate (book);
    do_test (job != NULL, "job create");
    do_test (qof_instance_get_book(QOF_INSTANCE(job)) == book,
	     "getbook");

    gncJobBeginEdit (job);
    gncJobDestroy (job);
    success ("create/destroy");
  }

  /* Test setting/getting routines; does the active flag get set right? */
  {
    GUID guid;

    test_string_fcn (book, "Id", gncJobSetID, gncJobGetID);
    test_string_fcn (book, "Name", gncJobSetName, gncJobGetName);
    test_string_fcn (book, "Reference", gncJobSetReference, gncJobGetReference);

    test_bool_fcn (book, "Active", gncJobSetActive, gncJobGetActive);

    guid_new (&guid);
    job = gncJobCreate (book); count++;
    gncJobSetGUID (job, &guid);
    do_test (guid_equal (&guid, qof_instance_get_guid(QOF_INSTANCE(job))), "guid compare");
  }
#if 0
  {
    GList *list;

    list = gncBusinessGetList (book, GNC_JOB_MODULE_NAME, TRUE);
    do_test (list != NULL, "getList all");
    do_test (g_list_length (list) == count, "correct length: all");
    g_list_free (list);

    list = gncBusinessGetList (book, GNC_JOB_MODULE_NAME, FALSE);
    do_test (list != NULL, "getList active");
    do_test (g_list_length (list) == 1, "correct length: active");
    g_list_free (list);
  }
#endif
  {
    const char *str = get_random_string();
    const char *res;

    gncJobSetName (job, str);
    res = qof_object_printable (GNC_JOB_MODULE_NAME, job);
    do_test (res != NULL, "Printable NULL?");
    do_test (safe_strcmp (str, res) == 0, "Printable equals");
  }    
  {
    GList *list;
    GncOwner owner;
    GncCustomer *cust = gncCustomerCreate (book);

    gncOwnerInitCustomer (&owner, cust);

    do_test (gncCustomerGetJoblist (cust, TRUE) == NULL, "empty list at start");
    gncJobSetOwner (job, &owner);
    list = gncCustomerGetJoblist (cust, FALSE);
    do_test (list != NULL, "added to cust");
    do_test (g_list_length (list) == 1, "correct joblist length");
    do_test (list->data == job, "verify job in list");
    gncJobSetActive (job, FALSE);
    list = gncCustomerGetJoblist (cust, FALSE);
    do_test (list == NULL, "no active jobs");
    list = gncCustomerGetJoblist (cust, TRUE);
    do_test (list != NULL, "all jobs");
    gncJobBeginEdit (job);
    gncJobDestroy (job);
    list = gncCustomerGetJoblist (cust, TRUE);
    do_test (list == NULL, "no more jobs");
  }
}

static void
test_string_fcn (QofBook *book, const char *message,
		 void (*set) (GncJob *, const char *str),
		 const char * (*get)(GncJob *))
{
  GncJob *job = gncJobCreate (book);
  char const *str = get_random_string ();

  do_test (!qof_instance_is_dirty (QOF_INSTANCE(job)), "test if start dirty");
  gncJobBeginEdit (job);
  set (job, str);
  do_test (qof_instance_is_dirty (QOF_INSTANCE(job)), "test dirty later");
  gncJobCommitEdit (job);
  do_test (!qof_instance_is_dirty (QOF_INSTANCE(job)), "test dirty after commit");
  do_test (safe_strcmp (get (job), str) == 0, message);
  gncJobSetActive (job, FALSE); count++;
}

#if 0
static void
test_numeric_fcn (QofBook *book, const char *message,
		  void (*set) (GncJob *, gnc_numeric),
		  gnc_numeric (*get)(GncJob *))
{
  GncJob *job = gncJobCreate (book);
  gnc_numeric num = gnc_numeric_create (17, 1);

  do_test (!qof_instance_is_dirty (QOF_INSTANCE(job)), "test if start dirty");
  gncJobBeginEdit (job);
  set (job, num);
  do_test (qof_instance_is_dirty (QOF_INSTANCE(job)), "test dirty later");
  gncJobCommitEdit (job);
  do_test (!qof_instance_is_dirty (QOF_INSTANCE(job)), "test dirty after commit");
  do_test (gnc_numeric_equal (get (job), num), message);
  gncJobSetActive (job, FALSE); count++;
}
#endif

static void
test_bool_fcn (QofBook *book, const char *message,
	       void (*set) (GncJob *, gboolean),
	       gboolean (*get) (GncJob *))
{
  GncJob *job = gncJobCreate (book);
  gboolean num = get_random_boolean ();

  do_test (!qof_instance_is_dirty (QOF_INSTANCE(job)), "test if start dirty");
  gncJobBeginEdit (job);
  set (job, FALSE);
  set (job, TRUE);
  set (job, num);
  do_test (qof_instance_is_dirty (QOF_INSTANCE(job)), "test dirty later");
  gncJobCommitEdit (job);
  do_test (!qof_instance_is_dirty (QOF_INSTANCE(job)), "test dirty after commit");
  do_test (get (job) == num, message);
  gncJobSetActive (job, FALSE); count++;
}

#if 0
static void
test_gint_fcn (QofBook *book, const char *message,
	       void (*set) (GncJob *, gint),
	       gint (*get) (GncJob *))
{
  GncJob *job = gncJobCreate (book);
  gint num = 17;

  do_test (!qof_instance_is_dirty (QOF_INSTANCE(job)), "test if start dirty");
  gncJobBeginEdit (job);
  set (job, num);
  do_test (qof_instance_is_dirty (QOF_INSTANCE(job)), "test dirty later");
  gncJobCommitEdit (job);
  do_test (!qof_instance_is_dirty (QOF_INSTANCE(job)), "test dirty after commit");
  do_test (get (job) == num, message);
  gncJobSetActive (job, FALSE); count++;
}
#endif

static void
main_helper (void *closure, int argc, char **argv)
{
  gnc_module_load("gnucash/business-core", 0);
  test_job();
  print_test_results();
  exit(get_rv());
}

int
main (int argc, char **argv)
{
  scm_boot_guile (argc, argv, main_helper, NULL);
  return 0;
}
