/*
 * gncEmployee.h -- the Core Employee Interface
 * Copyright (C) 2001 Derek Atkins
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#ifndef GNC_EMPLOYEE_H_
#define GNC_EMPLOYEE_H_

typedef struct _gncEmployee GncEmployee;

#include "gnc-book.h"
#include "gncAddress.h"
#include "Account.h"

#define GNC_EMPLOYEE_MODULE_NAME "gncEmployee"

/* Create/Destroy Functions */

GncEmployee *gncEmployeeCreate (GNCBook *book);
void gncEmployeeDestroy (GncEmployee *employee);

/* Set Functions */

void gncEmployeeSetID (GncEmployee *employee, const char *id);
void gncEmployeeSetUsername (GncEmployee *employee, const char *username);
void gncEmployeeSetLanguage (GncEmployee *employee, const char *language);
void gncEmployeeSetAcl (GncEmployee *employee, const char *acl);
void gncEmployeeSetWorkday (GncEmployee *employee, gnc_numeric workday);
void gncEmployeeSetRate (GncEmployee *employee, gnc_numeric rate);
void gncEmployeeSetCurrency (GncEmployee *employee, gnc_commodity * currency);
void gncEmployeeSetActive (GncEmployee *employee, gboolean active);
void gncEmployeeSetCCard (GncEmployee *employee, Account* ccard_acc);

/* Get Functions */

GNCBook * gncEmployeeGetBook (GncEmployee *employee);
const GUID * gncEmployeeGetGUID (GncEmployee *employee);
const char * gncEmployeeGetID (GncEmployee *employee);
const char * gncEmployeeGetUsername (GncEmployee *employee);
GncAddress * gncEmployeeGetAddr (GncEmployee *employee);
const char * gncEmployeeGetLanguage (GncEmployee *employee);
const char * gncEmployeeGetAcl (GncEmployee *employee);
gnc_numeric gncEmployeeGetWorkday (GncEmployee *employee);
gnc_numeric gncEmployeeGetRate (GncEmployee *employee);
gnc_commodity * gncEmployeeGetCurrency (GncEmployee *employee);
gboolean gncEmployeeGetActive (GncEmployee *employee);
Account * gncEmployeeGetCCard (GncEmployee *employee);

GncEmployee * gncEmployeeLookup (GNCBook *book, const GUID *guid);
gboolean gncEmployeeIsDirty (GncEmployee *employee);

GUID gncEmployeeRetGUID (GncEmployee *employee);
GncEmployee * gncEmployeeLookupDirect (GUID guid, GNCBook *book);

void gncEmployeeBeginEdit (GncEmployee *employee);
void gncEmployeeCommitEdit (GncEmployee *employee);
int gncEmployeeCompare (GncEmployee *a, GncEmployee *b);

#define EMPLOYEE_ID	"id"
#define EMPLOYEE_USERNAME	"username"
#define EMPLOYEE_ADDR	"addr"

#endif /* GNC_EMPLOYEE_H_ */
