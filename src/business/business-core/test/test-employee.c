#include <glib.h>
#include <libguile.h>

#include "guid.h"
#include "gnc-module.h"
#include "gnc-engine-util.h"
#include "qofinstance.h"
#include "qofobject.h"

#include "gncEmployee.h"
#include "gncEmployeeP.h"
#include "test-stuff.h"

static int count = 0;

static void
test_string_fcn (QofBook *book, const char *message,
		 void (*set) (GncEmployee *, const char *str),
		 const char * (*get)(GncEmployee *));

static void
test_numeric_fcn (QofBook *book, const char *message,
		  void (*set) (GncEmployee *, gnc_numeric),
		  gnc_numeric (*get)(GncEmployee *));

static void
test_bool_fcn (QofBook *book, const char *message,
		  void (*set) (GncEmployee *, gboolean),
		  gboolean (*get) (GncEmployee *));

#if 0
static void
test_gint_fcn (QofBook *book, const char *message,
	       void (*set) (GncEmployee *, gint),
	       gint (*get) (GncEmployee *));
#endif

static void
test_employee (void)
{
  QofBook *book;
  GncEmployee *employee;

  book = qof_book_new ();

  /* Test creation/destruction */
  {
    do_test (gncEmployeeCreate (NULL) == NULL, "employee create NULL");
    employee = gncEmployeeCreate (book);
    do_test (employee != NULL, "employee create");
    do_test (qof_instance_get_book(QOF_INSTANCE(employee)) == book,
	     "getbook");

    gncEmployeeBeginEdit (employee);
    gncEmployeeDestroy (employee);
    success ("create/destroy");
  }

  /* Test setting/getting routines; does the active flag get set right? */
  {
    GUID guid;

    test_string_fcn (book, "Id", gncEmployeeSetID, gncEmployeeGetID);
    test_string_fcn (book, "Username", gncEmployeeSetUsername, gncEmployeeGetUsername);
    test_string_fcn (book, "Language", gncEmployeeSetLanguage, gncEmployeeGetLanguage);
    test_string_fcn (book, "Acl", gncEmployeeSetAcl, gncEmployeeGetAcl);

    test_numeric_fcn (book, "Workday", gncEmployeeSetWorkday, gncEmployeeGetWorkday);
    test_numeric_fcn (book, "Rate", gncEmployeeSetRate, gncEmployeeGetRate);

    test_bool_fcn (book, "Active", gncEmployeeSetActive, gncEmployeeGetActive);

    do_test (gncEmployeeGetAddr (employee) != NULL, "Addr");

    guid_new (&guid);
    employee = gncEmployeeCreate (book); count++;
    gncEmployeeSetGUID (employee, &guid);
    do_test (guid_equal (&guid, qof_instance_get_guid(QOF_INSTANCE(employee))), "guid compare");
  }
#if 0
  {
    GList *list;

    list = gncBusinessGetList (book, GNC_EMPLOYEE_MODULE_NAME, TRUE);
    do_test (list != NULL, "getList all");
    do_test (g_list_length (list) == count, "correct length: all");
    g_list_free (list);

    list = gncBusinessGetList (book, GNC_EMPLOYEE_MODULE_NAME, FALSE);
    do_test (list != NULL, "getList active");
    do_test (g_list_length (list) == 1, "correct length: active");
    g_list_free (list);
  }
#endif
  {
    const char *str = get_random_string();
    const char *res;
    GncAddress *addr;

    addr = gncEmployeeGetAddr (employee);
    gncAddressSetName (addr, str);
    res = qof_object_printable (GNC_EMPLOYEE_MODULE_NAME, employee);
    do_test (res != NULL, "Printable NULL?");
    do_test (safe_strcmp (str, res) == 0, "Printable equals");
  }    
}

static void
test_string_fcn (QofBook *book, const char *message,
		 void (*set) (GncEmployee *, const char *str),
		 const char * (*get)(GncEmployee *))
{
  GncEmployee *employee = gncEmployeeCreate (book);
  char const *str = get_random_string ();

  do_test (!gncEmployeeIsDirty (employee), "test if start dirty");
  gncEmployeeBeginEdit (employee);
  set (employee, str);
  do_test (gncEmployeeIsDirty (employee), "test dirty later");
  gncEmployeeCommitEdit (employee);
  do_test (!gncEmployeeIsDirty (employee), "test dirty after commit");
  do_test (safe_strcmp (get (employee), str) == 0, message);
  gncEmployeeSetActive (employee, FALSE);
  count++;
}

static void
test_numeric_fcn (QofBook *book, const char *message,
		  void (*set) (GncEmployee *, gnc_numeric),
		  gnc_numeric (*get)(GncEmployee *))
{
  GncEmployee *employee = gncEmployeeCreate (book);
  gnc_numeric num = gnc_numeric_create (17, 1);

  do_test (!gncEmployeeIsDirty (employee), "test if start dirty");
  gncEmployeeBeginEdit (employee);
  set (employee, num);
  do_test (gncEmployeeIsDirty (employee), "test dirty later");
  gncEmployeeCommitEdit (employee);
  do_test (!gncEmployeeIsDirty (employee), "test dirty after commit");
  do_test (gnc_numeric_equal (get (employee), num), message);
  gncEmployeeSetActive (employee, FALSE);
  count++;
}

static void
test_bool_fcn (QofBook *book, const char *message,
	       void (*set) (GncEmployee *, gboolean),
	       gboolean (*get) (GncEmployee *))
{
  GncEmployee *employee = gncEmployeeCreate (book);
  gboolean num = get_random_boolean ();

  do_test (!gncEmployeeIsDirty (employee), "test if start dirty");
  gncEmployeeBeginEdit (employee);
  set (employee, FALSE);
  set (employee, TRUE);
  set (employee, num);
  do_test (gncEmployeeIsDirty (employee), "test dirty later");
  gncEmployeeCommitEdit (employee);
  do_test (!gncEmployeeIsDirty (employee), "test dirty after commit");
  do_test (get (employee) == num, message);
  gncEmployeeSetActive (employee, FALSE);
  count++;
}

#if 0
static void
test_gint_fcn (QofBook *book, const char *message,
	       void (*set) (GncEmployee *, gint),
	       gint (*get) (GncEmployee *))
{
  GncEmployee *employee = gncEmployeeCreate (book);
  gint num = 17;

  do_test (!gncEmployeeIsDirty (employee), "test if start dirty");
  gncEmployeeBeginEdit (employee);
  set (employee, num);
  do_test (gncEmployeeIsDirty (employee), "test dirty later");
  gncEmployeeCommitEdit (employee);
  do_test (!gncEmployeeIsDirty (employee), "test dirty after commit");
  do_test (get (employee) == num, message);
  gncEmployeeSetActive (employee, FALSE);
  count++;
}
#endif

static void
main_helper (void *closure, int argc, char **argv)
{
  gnc_module_load("gnucash/business-core", 0);
  test_employee();
  print_test_results();
  exit(get_rv());
}

int
main (int argc, char **argv)
{
  scm_boot_guile (argc, argv, main_helper, NULL);
  return 0;
}
