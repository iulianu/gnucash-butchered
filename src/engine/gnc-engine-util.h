/********************************************************************\
 * gnc-engine-util.h -- GnuCash engine utility functions            *
 * Copyright (C) 1997 Robin D. Clark                                *
 * Copyright (C) 1998-2000 Linas Vepstas <linas@linas.org>          *
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
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
 *                                                                  *
 *   Author: Rob Clark (rclark@cs.hmc.edu)                          *
 *   Author: Linas Vepstas (linas@linas.org)                        *
\********************************************************************/

#ifndef __GNC_ENGINE_UTIL_H__
#define __GNC_ENGINE_UTIL_H__

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>

#include "gnc-common.h"
#include "gnc-commodity.h"
#include "gnc-numeric.h"

#define BUFSIZE   1024


/** DEBUGGING MACROS ************************************************/
/* The debuging macros enable the setting of trace messages */

typedef enum
{
  MOD_DUMMY   =  0,
  MOD_ENGINE  =  1,
  MOD_IO      =  2,
  MOD_REGISTER=  3,
  MOD_LEDGER  =  4,
  MOD_HTML    =  5,
  MOD_GUI     =  6,
  MOD_SCRUB   =  7,
  MOD_GTK_REG =  8,
  MOD_GUILE   =  9,
  MOD_BACKEND = 10,
  MOD_QUERY   = 11,
  MOD_LAST    = 11
} gncModuleType;

typedef enum
{
  GNC_LOG_FATAL   = 0,
  GNC_LOG_ERROR   = 1,
  GNC_LOG_WARNING = 2,
  GNC_LOG_INFO    = 3,
  GNC_LOG_DEBUG   = 4,
  GNC_LOG_DETAIL  = 5
} gncLogLevel;

extern gncLogLevel loglevel[MOD_LAST + 1];

void gnc_log (gncModuleType module, gncLogLevel log_level,
              const char *prefix, const char *function_name,
              const char *format, ...) G_GNUC_PRINTF(5,6);

#define FATAL(format, args...) \
  gnc_log(module, GNC_LOG_FATAL, "Fatal Error", __FUNCTION__, format, ## args);

#define PERR(format, args...) \
  gnc_log(module, GNC_LOG_ERROR, "Error", __FUNCTION__, format, ##args);

#define PWARN(format, args...) \
  gnc_log(module, GNC_LOG_WARNING, "Warning", __FUNCTION__, format, ## args);

#define PINFO(format, args...) \
  gnc_log(module, GNC_LOG_INFO, "Info", __FUNCTION__, format, ## args);

#define DEBUG(format, args...) \
  gnc_log(module, GNC_LOG_DEBUG, "Debug", __FUNCTION__, format, ## args);

#define ENTER(format, args...) \
  gnc_log(module, GNC_LOG_DEBUG, "Enter", __FUNCTION__, format, ## args);

#define LEAVE(format, args...) \
  gnc_log(module, GNC_LOG_DEBUG, "Leave", __FUNCTION__, format, ## args);

#define DETAIL(format, args...) \
  gnc_log(module, GNC_LOG_DETAIL, "Detail", __FUNCTION__, format, ## args);


#define DEBUGCMD(x) { if (loglevel[module] >= GNC_LOG_DEBUG) { x; }}

#define ERROR()     fprintf(stderr,"%s: Line %d, error = %s\n", \
			    __FILE__, __LINE__, strerror(errno));


/* Set the logging level of the given module. */
void gnc_set_log_level(gncModuleType module, gncLogLevel level);

/* Set the logging level for all modules. */
void gnc_set_log_level_global(gncLogLevel level);


/** Macros *****************************************************/
#define EPS  (1.0e-6)
#define DEQEPS(x,y,eps) (((((x)+(eps))>(y)) ? 1 : 0) && ((((x)-(eps))<(y)) ? 1 : 0))
#define DEQ(x,y) DEQEPS(x,y,EPS)


#define SAFE_STRCMP(da,db) {		\
  if ((da) && (db)) {			\
    int retval = strcmp ((da), (db));	\
    /* if strings differ, return */	\
    if (retval) return retval;		\
  } else 				\
  if ((!(da)) && (db)) {		\
    return -1;				\
  } else 				\
  if ((da) && (!(db))) {		\
    return +1;				\
  }					\
}


/** Prototypes *************************************************/

int safe_strcmp (const char * da, const char * db);

/* Search for str2 in first nchar chars of str1, ignore case. Return
 * pointer to first match, or null. These are just like that strnstr
 * and the strstr functions, except that they ignore the case. */
extern char *strncasestr(const char *str1, const char *str2, size_t len);
extern char *strcasestr(const char *str1, const char *str2);

/* The ultostr() subroutine is the inverse of strtoul(). It accepts a
 * number and prints it in the indicated base.  The returned string
 * should be g_freed when done.  */
char * ultostr (unsigned long val, int base);

/* Returns true if string s is a number, possibly surrounded by
 * whitespace. */
gboolean gnc_strisnum(const char *s);

#endif
