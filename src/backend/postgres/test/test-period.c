
/* 
 * Test file created by Linas Vepstas <linas@linas.org>
 * Minimal test to see if a book can be split into two periods
 * without crashing.  Book is written to the database.
 * December 2001
 * License: GPL
 */

#include <ctype.h>
#include <glib.h>
#include <libguile.h>
#include <time.h>

#include "Account.h"
#include "Group.h"
#include "Period.h"
#include "qofbook.h"
#include "qofbook-p.h"
#include "gnc-engine-util.h"
#include "gnc-module.h"
#include "test-stuff.h"
#include "test-engine-stuff.h"
#include "Transaction.h"



static void
run_test (void)
{
  QofBackendError io_err;
  QofSession *session;
  QofBook *openbook, *closedbook;
  AccountGroup *grp;
  AccountList *acclist, *anode;
  Account * acc = NULL;
  SplitList *splist;
  Split *sfirst, *slast;
  Transaction *tfirst, *tlast;
  Timespec tsfirst, tslast, tsmiddle;
  char * test_url;
  


  if(!gnc_module_load("gnucash/engine", 0))
  {
    failure("couldn't load gnucash/engine");
    exit(get_rv());
  }

  session = get_random_session ();

  test_url = "postgres://localhost/qqq?mode=single-update";
  qof_session_begin (session, test_url, FALSE, TRUE);

  io_err = qof_session_get_error (session);
  g_return_if_fail (io_err == ERR_BACKEND_NO_ERR);

  openbook = qof_session_get_book (session);
  if (!openbook)
  {
    failure("book not created");
    exit(get_rv());
  }

  add_random_transactions_to_book (openbook, 12);

  grp = gnc_book_get_group (openbook);

  acclist = xaccGroupGetSubAccounts (grp);
  for (anode=acclist; anode; anode=anode->next)
  {
    int ns;
    acc = anode->data;
    ns = g_list_length (xaccAccountGetSplitList (acc));
    if (2 <= ns) break;
    acc = NULL;
  }

  if(!acc)
  {
    failure("group didn't have accounts with enough splits");
    exit(get_rv());
  }

  splist = xaccAccountGetSplitList(acc);
  if(!splist)
  {
    failure("account has no transactions");
    exit(get_rv());
  }

  sfirst = splist->data;
  slast = g_list_last(splist) ->data;
  if (sfirst == slast)
  {
    failure("account doesn't have enough transactions");
    exit(get_rv());
  }

  tfirst = xaccSplitGetParent (sfirst);
  tlast = xaccSplitGetParent (slast);
  
  if (!tfirst || !tlast)
  {
    failure("malformed transactions in account");
    exit(get_rv());
  }

  tsfirst = xaccTransRetDatePostedTS (tfirst);
  tslast = xaccTransRetDatePostedTS (tlast);

  if (tsfirst.tv_sec == tslast.tv_sec)
  {
    failure("transactions not time separated");
    exit(get_rv());
  }

  tsmiddle = tsfirst;
  tsmiddle.tv_sec = (tsfirst.tv_sec + tslast.tv_sec)/2;

  qof_session_save (session, NULL);
  io_err = qof_session_get_error (session);
  g_return_if_fail (io_err == ERR_BACKEND_NO_ERR);

  // stdout is broken with guile for some reason
  // gnc_set_logfile (stdout);
  // gnc_set_log_level_global (GNC_LOG_INFO);
  closedbook = gnc_book_close_period (openbook, tsmiddle, 
                  NULL, "this is opening balance dude");

  if (!closedbook)
  {
    failure("closed book not created");
    exit(get_rv());
  }

  qof_session_save (session, NULL);
  io_err = qof_session_get_error (session);
  g_return_if_fail (io_err == ERR_BACKEND_NO_ERR);

  qof_session_end (session);
  io_err = qof_session_get_error (session);
  g_return_if_fail (io_err == ERR_BACKEND_NO_ERR);

  success ("periods lightly tested and seem to work");
}

static void
main_helper (void *closure, int argc, char **argv)
{
  run_test ();

  print_test_results();
  exit(get_rv());
}

int
main (int argc, char **argv)
{
  scm_boot_guile(argc, argv, main_helper, NULL);
  return 0;
}
