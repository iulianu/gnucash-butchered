/* 
 * FILE:
 * putil.h
 *
 * FUNCTION:
 * Postgres backend utility macros
 *
 * HISTORY:
 * Copyright (c) 2000, 2001 Linas Vepstas
 * 
 */

#ifndef __P_UTIL_H__ 
#define __P_UTIL_H__ 

#include <glib.h>
#include <string.h>  
#include <sys/types.h>  

#include <libpq-fe.h>  

#include "Backend.h"
#include "BackendP.h"
#include "gnc-engine-util.h"
#include "guid.h"
#include "GNCId.h"

#include "PostgresBackend.h"


extern GUID nullguid;

/* The pgenGetResults() routine loops over all of the results returned
 *    by the database, and invokes the 'handler' routine on it.  The
 *    gpointer 'data' is passed to (*handler) the first time its invoked.
 *    Each subsequent time, the return value of (*handler) is passed
 *    as the 'data' gpointer.   This routine returns the gpointer that
 *    (handler) returned the last time it was invoked.
 */

gpointer pgendGetResults (PGBackend *be,
            gpointer (*handler) (PGBackend *, PGresult *, int, gpointer),
            gpointer data);


/* hack alert -- calling PQFinish() is quite harsh, since all 
 * subsequent sql queries will fail. On the other hand, killing
 * anything that follows *is* a way of minimizing data corruption 
 * due to subsequent mishaps ... so anyway, error handling in these 
 * routines needs to be rethought. 
 */

/* ============================================================= */
/* The SEND_QUERY macro sends the sql statement off to the server. 
 * It performs a minimal check to see that the send succeeded. 
 */

#define SEND_QUERY(be,buff,retval) 				\
{								\
   int rc;							\
   if (NULL == be->connection) return retval;			\
   PINFO ("sending query %s", buff);				\
   rc = PQsendQuery (be->connection, buff);			\
   if (!rc)							\
   {								\
      /* hack alert -- we need kinder, gentler error handling */\
      PERR("send query failed:\n"				\
           "\t%s", PQerrorMessage(be->connection));		\
      PQfinish (be->connection);				\
      be->connection = NULL;					\
      xaccBackendSetError (&be->be, ERR_BACKEND_CONN_LOST);	\
      return retval;						\
   }								\
}

/* --------------------------------------------------------------- */
/* The FINISH_QUERY macro makes sure that the previously sent
 * query complete with no errors.  It assumes that the query
 * is does not produce any results; if it did, those results are
 * discarded (only error conditions are checked for).
 */

#define FINISH_QUERY(conn) 					\
{								\
   int i=0;							\
   PGresult *result; 						\
   /* complete/commit the transaction, check the status */	\
   do {								\
      ExecStatusType status;					\
      result = PQgetResult((conn));				\
      if (!result) break;					\
      PINFO ("clearing result %d", i);				\
      status = PQresultStatus(result);  			\
      if (PGRES_COMMAND_OK != status) {				\
         PERR("finish query failed:\n"				\
              "\t%s", PQerrorMessage((conn)));			\
         PQclear(result);					\
         PQfinish ((conn));					\
         be->connection = NULL;					\
         xaccBackendSetError (&be->be, ERR_BACKEND_CONN_LOST);	\
	 break;							\
      }								\
      PQclear(result);						\
      i++;							\
   } while (result);						\
}

/* --------------------------------------------------------------- */
/* The GET_RESULTS macro grabs the result of an pgSQL query off the
 * wire, and makes sure that no errors occured. Results are left 
 * in the result buffer.
 */
#define GET_RESULTS(conn,result) 				\
{								\
   ExecStatusType status;  					\
   result = PQgetResult (conn);					\
   if (!result) break;						\
   status = PQresultStatus(result);				\
   if ((PGRES_COMMAND_OK != status) &&				\
       (PGRES_TUPLES_OK  != status))				\
   {								\
      PERR("failed to get result to query:\n"			\
           "\t%s", PQerrorMessage((conn)));			\
      PQclear (result);						\
      PQfinish (conn);						\
      be->connection = NULL;					\
      xaccBackendSetError (&be->be, ERR_BACKEND_SERVER_ERR);	\
      break;							\
   }								\
}

/* --------------------------------------------------------------- */
/* The IF_ONE_ROW macro counts the number of rows returned by 
 * a query, reports an error if there is more than one row, and
 * conditionally executes a block for the first row.
 */
 
#define IF_ONE_ROW(result,nrows,loopcounter)			\
   {								\
      int ncols = PQnfields (result);				\
      nrows += PQntuples (result);				\
      PINFO ("query result %d has %d rows and %d cols",		\
           loopcounter, nrows, ncols);				\
   }								\
   if (1 < nrows) {						\
      PERR ("unexpected duplicate records");			\
      xaccBackendSetError (&be->be, ERR_BACKEND_DATA_CORRUPT);	\
      break;							\
   } else if (1 == nrows) 

/* --------------------------------------------------------------- */
/* Some utility macros for comparing values returned from the
 * database to values in the engine structs.  These macros
 * all take three arguments:
 * -- sqlname -- input -- the name of the field in the sql table
 * -- fun -- input -- a subroutine returning a value
 * -- ndiffs -- input/output -- integer, incremented if the 
 *              value ofthe field and the value returned by
 *              the subroutine differ.
 *
 * The different macros compare different field types.
 */

#define DB_GET_VAL(str,n) (PQgetvalue (result, n, PQfnumber (result, str)))

/* Compare string types.  Null strings and empty strings are  
 * considered to be equal */
#define COMP_STR(sqlname,fun,ndiffs) { 				\
   if (null_strcmp (DB_GET_VAL(sqlname,0),fun)) {		\
      PINFO("mis-match: %s sql='%s', eng='%s'", sqlname, 	\
         DB_GET_VAL (sqlname,0), fun); 				\
      ndiffs++; 						\
   }								\
}

/* Compare commodities. This routine is almost identical to 
 * COMP_STR, except that a NULL currency from the engine
 * is allowed to match any currency in the sql DB.  This is
 * used to facilitate deletion, where the currency has been
 * nulled out .. */
#define COMP_COMMODITY(sqlname,fun,ndiffs) { 			\
   const char *com = fun;					\
   if (com) {							\
      if (null_strcmp (DB_GET_VAL(sqlname,0),com)) {		\
         PINFO("mis-match: %s sql='%s', eng='%s'", sqlname, 	\
            DB_GET_VAL (sqlname,0), fun); 			\
         ndiffs++; 						\
      }								\
   }								\
}

/* Compare guids. A NULL GUID from the engine is considered to 
 * match any value of a GUID in teh sql database.  This is 
 * equality is used to enable deletion, where the GUID may have
 * already been set to NULL in the engine, but not yet in the DB.
 */
#define COMP_GUID(sqlname,fun, ndiffs) { 			\
   char guid_str[GUID_ENCODING_LENGTH+1];			\
   const GUID *guid = fun;					\
   if (!guid_equal (guid, &nullguid)) {				\
      guid_to_string_buff(guid, guid_str); 			\
      if (null_strcmp (DB_GET_VAL(sqlname,0),guid_str)) { 	\
         PINFO("mis-match: %s sql='%s', eng='%s'", sqlname, 	\
            DB_GET_VAL(sqlname,0), guid_str); 			\
         ndiffs++; 						\
      }								\
   }								\
} 

/* Comapre one char only */
#define COMP_CHAR(sqlname,fun, ndiffs) { 			\
    if (tolower((DB_GET_VAL(sqlname,0))[0]) != tolower(fun)) {	\
       PINFO("mis-match: %s sql=%c eng=%c", sqlname, 		\
         tolower((DB_GET_VAL(sqlname,0))[0]), tolower(fun)); 	\
      ndiffs++; 						\
   }								\
}

/* Compare dates.
 * Assumes the datestring is in ISO-8601 format 
 * i.e. looks like 1998-07-17 11:00:00.68-05  
 * hack-alert doesn't compare nano-seconds ..  
 * this is intentional,  its because I suspect
 * the sql db round nanoseconds off ... 
 */
#define COMP_DATE(sqlname,fun,ndiffs) { 			\
    Timespec eng_time = fun;					\
    Timespec sql_time = gnc_iso8601_to_timespec_local(		\
                     DB_GET_VAL(sqlname,0)); 			\
    if (eng_time.tv_sec != sql_time.tv_sec) {			\
       char buff[80];						\
       gnc_timespec_to_iso8601_buff(eng_time, buff);		\
       PINFO("mis-match: %s sql='%s' eng=%s", sqlname, 		\
         DB_GET_VAL(sqlname,0), buff); 				\
      ndiffs++; 						\
   }								\
}

/* Compare the date of last modification. 
 * This is a special date comp to 
 * (1) make the m4 macros simpler, and
 * (2) avoid needless updates 
 */
#define COMP_NOW(sqlname,fun,ndiffs) { 	 			\
    Timespec eng_time = xaccTransRetDateEnteredTS(ptr);		\
    Timespec sql_time = gnc_iso8601_to_timespec_local(		\
                     DB_GET_VAL(sqlname,0)); 			\
    if (eng_time.tv_sec > sql_time.tv_sec) {			\
       char buff[80];						\
       gnc_timespec_to_iso8601_buff(eng_time, buff);		\
       PINFO("mis-match: %s sql='%s' eng=%s", sqlname, 		\
         DB_GET_VAL(sqlname,0), buff); 				\
      ndiffs++; 						\
   }								\
}


/* Compare long-long integers */
#define COMP_INT64(sqlname,fun,ndiffs) { 			\
   if (atoll (DB_GET_VAL(sqlname,0)) != fun) {			\
      PINFO("mis-match: %s sql='%s', eng='%lld'", sqlname, 	\
         DB_GET_VAL (sqlname,0), fun); 				\
      ndiffs++; 						\
   }								\
}

/* compare 32-bit ints */
#define COMP_INT32(sqlname,fun,ndiffs) { 			\
   if (atol (DB_GET_VAL(sqlname,0)) != fun) {			\
      PINFO("mis-match: %s sql='%s', eng='%d'", sqlname, 	\
         DB_GET_VAL (sqlname,0), fun); 				\
      ndiffs++; 						\
   }								\
}

#define DBL_RESOLUTION  2.0e-16

/* compare 64-bit floats */
#define COMP_DOUBLE(sqlname,fun,ndiffs) { 			\
   double sqlval = atof (DB_GET_VAL(sqlname,0));		\
   double engval = fun;						\
   if (((sqlval-engval) > DBL_RESOLUTION*engval) ||		\
       ((engval-sqlval) > DBL_RESOLUTION*engval)) {		\
      PINFO("mis-match: %s sql=%24.18g, eng=%24.18g", sqlname, 	\
         sqlval, engval); 					\
      ndiffs++; 						\
   }								\
}


#endif /* __P_UTIL_H__ */

/* ======================== END OF FILE ======================== */
