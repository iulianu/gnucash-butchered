/********************************************************************\
 * messages.h -- national-language messages for GnuCash             *
 * Copyright (C) 1997 Robin D. Clark                                *
 * Copyright (C) 1997, 1998 Linas Vepstas                           *
 * Copyright (C) 2000 Dave Peticolas                                *
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
 * along with this program; if not, write to the Free Software      *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.        *
\********************************************************************/

#ifndef __XACC_MESSAGES_H__
#define __XACC_MESSAGES_H__

#include "config.h"

#if defined(HAVE_GETTEXT)             /* HAVE_GETTEXT */

#include <libintl.h>
#include <locale.h>

#undef _

#ifdef DISABLE_GETTEXT_UNDERSCORE
#define _(String) (String)
#else                                 /* ENABLE_GETTEXT_UNDERSCORE */
#define _(String) gettext(String)
#endif		                      /* End ENABLE_GETTEXT_UNDERSCORE */

#else                                 /* Not HAVE_GETTEXT */

#define _(String)       (String)
#define gettext(String) (String)

#endif                                /* End Not HAVE_GETTEXT */

#undef  N_
#define N_(String) (String)

#endif /* __XACC_MESSAGES_H__ */
