/*
 * gncOrderP.h -- the Core Busines Order Interface
 * Copyright (C) 2001 Derek Atkins
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#ifndef GNC_ORDERP_H_
#define GNC_ORDERP_H_

#include "gncOrder.h"

gboolean gncOrderRegister (void);
gint64 gncOrderNextID (QofBook *book);
void gncOrderSetGUID (GncOrder *order, const GUID *guid);
void gncOrderSetDirty (GncOrder *order, gboolean dirty);

#endif /* GNC_ORDERP_H_ */
