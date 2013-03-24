/*  Authors: Eric M. Ludlam <zappo@ultranet.com>
 *           Russ McManus <russell.mcmanus@gs.com>
 *           Dave Peticolas <dave@krondo.com>
 *
 *  gfec stands for 'guile fancy error catching'.
 *  This code is in the public domain.
 */

#ifndef GFEC_H
#define GFEC_H

#include <glib.h>

gboolean gfec_try_load(gchar *fn);

#endif
