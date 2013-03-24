/*  Authors: Eric M. Ludlam <zappo@ultranet.com>
 *           Russ McManus <russell.mcmanus@gs.com>
 *           Dave Peticolas <dave@krondo.com>
 *
 *  gfec stands for 'guile fancy error catching'.
 *  This code is in the public domain.
 */

#include <assert.h>
#include <string.h>

#include "config.h"
#include "gfec.h"
#include "platform.h"
#if COMPILER(MSVC)
# define strdup _strdup
#endif




gboolean
gfec_try_load(gchar *fn)
{
    g_debug("looking for %s", fn);
    if (g_file_test(fn, G_FILE_TEST_EXISTS))
    {
        g_debug("trying to load %s", fn);
        return TRUE;
//        error_in_scm_eval = FALSE;
//        gfec_eval_file(fn, error_handler);
//        return !error_in_scm_eval;
    }
    return FALSE;
}

