/*
 * FILE:
 * hello2.c 
 *
 * FUNCTION:
 * the second in a series of cgi-bin programming eamples.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gnc-book.h"
#include "gnc-engine.h"
#include "Group.h"
#include "io-gncxml.h"
#include "Query.h"
 
int
main (int argc, char *argv[]) 
{
   int fake_argc =1;
   char * fake_argv[] = {"hello2", 0};
   GNCBook *book;
   AccountGroup *grp;
   Query *q, *qq;
   GList *split_list, *sl2, *node;
   Split *s;
   char *bufp;
   int i, ii, rc, sz;
   

   /* intitialize the engine */
   gnc_engine_init (fake_argc, fake_argv);

   /* contact the database, which is a flat file for this demo */
   book = gnc_book_new ();

   rc = gnc_book_begin (book, "file:/tmp/demo.xac");
   if (!rc) {
      int err = gnc_book_get_error (book);
      printf ("HTTP/1.1 500 Server Error\n");
      printf ("\n");
      printf ("%d %s\n", err, strerror (err));
      goto bookerrexit;
   }

   rc = gnc_book_load (book);
   if (!rc) {
      int err = gnc_book_get_error (book);
      printf ("HTTP/1.1 500 Server Error\n");
      printf ("\n");
      printf ("%d %s\n", err, strerror (err));
      goto bookerrexit;
   }

   /* the grp pointer points to our local cache of the data */
   grp = gnc_book_get_group (book);
   
   /* build a query */
   q = xaccMallocQuery ();
   xaccQuerySetGroup (q, grp);
   xaccQuerySetMaxSplits (q, 30);
   
   /* Get everything between some random dates */
   /* In real life, we would use a query as specified by the user */
   xaccQueryAddDateMatch (q, TRUE, 28, 2, 1982,
		             FALSE, 16, 10, 2010,
			     QUERY_OR);

   split_list = xaccQueryGetSplits (q);

   /* count number of splits */
   i = 0;
   for (node = split_list; node; node = node->next)
   {
      s = node->data;
      i++;
   }
		       
   gncxml_write_query_to_buf(q, &bufp, &sz);
   qq = gncxml_read_query (bufp, sz);
xaccQuerySetMaxSplits (qq, 30);
   xaccQuerySetGroup (qq, grp);
   sl2 = xaccQueryGetSplits (qq);

   /* count number of splits */
   ii = 0;
   for (node = sl2; node; node = node->next)
   {
      s = node->data;
      ii++;
   }
		       
   /* print the HTTP header */
   printf ("HTTP/1.1 200 OK\n");
   printf ("Content-Type: text/xml\n");
   printf ("Content-Length: %d\n", sz);
   printf ("\n");

   printf ("%s", bufp);

   printf (" its %d and %d \n", i, ii);

   free (bufp);
   xaccFreeQuery (q);


bookerrexit:
   /* close the book */
   gnc_book_destroy (book);

   /* shut down the engine */
   gnc_engine_shutdown ();

   return 0;
}
