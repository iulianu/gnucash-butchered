/********************************************************************\
 * QuickFill.h -- the quickfill tree data structure                 *
 * Copyright (C) 1997 Robin D. Clark                                *
 * Copyright (C) 1998 Linas Vepstas                                 *
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
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/

#include "config.h"

#include <string.h>

#include "QuickFill.h"
#include "gnc-engine.h"
#include "gnc-engine.h"
#include "gnc-ui-util.h"


struct _QuickFill
{
  char *text;          /* the first matching text string     */
  int len;             /* number of chars in text string     */
  GHashTable *matches; /* array of children in the tree      */
};


/** PROTOTYPES ******************************************************/
static void quickfill_insert_recursive (QuickFill *qf, const char *text,
                                        int depth, QuickFillSort sort);

/* This static indicates the debugging module that this .o belongs to.  */
static QofLogModule log_module = GNC_MOD_REGISTER;

/********************************************************************\
\********************************************************************/

static guint
quickfill_hash (gconstpointer key)
{
  return GPOINTER_TO_UINT (key);
}

static gint
quickfill_compare (gconstpointer key1, gconstpointer key2)
{
  guint k1 = GPOINTER_TO_UINT (key1);
  guint k2 = GPOINTER_TO_UINT (key2);

  return (k1 == k2);
}

/********************************************************************\
\********************************************************************/

QuickFill *
gnc_quickfill_new (void)
{
  QuickFill *qf;

  if (sizeof (guint) < sizeof (gunichar))
  {
    PWARN ("Can't use quickfill");
    return NULL;
  }

  qf = g_new (QuickFill, 1);

  qf->text = NULL;
  qf->len = 0;

  qf->matches = g_hash_table_new (quickfill_hash, quickfill_compare);

  return qf;
}

/********************************************************************\
\********************************************************************/

static gboolean
destroy_helper (gpointer key, gpointer value, gpointer data)
{
  gnc_quickfill_destroy (value);
  return TRUE;
}

void
gnc_quickfill_destroy (QuickFill *qf)
{
  if (qf == NULL)
    return;

  g_hash_table_foreach (qf->matches, (GHFunc)destroy_helper, NULL);
  g_hash_table_destroy (qf->matches);
  qf->matches = NULL;

  if (qf->text)
    gnc_string_cache_remove(qf->text);
  qf->text = NULL;
  qf->len = 0;

  g_free (qf);
}

void
gnc_quickfill_purge (QuickFill *qf)
{
  if (qf == NULL)
    return;

  g_hash_table_foreach_remove (qf->matches, destroy_helper, NULL);

  if (qf->text)
    gnc_string_cache_remove (qf->text);
  qf->text = NULL;
  qf->len = 0;
}

/********************************************************************\
\********************************************************************/

const char *
gnc_quickfill_string (QuickFill *qf)
{
  if (qf == NULL)
    return NULL;

  return qf->text;
}

/********************************************************************\
\********************************************************************/

QuickFill *
gnc_quickfill_get_char_match (QuickFill *qf, gunichar uc)
{
  guint key = g_unichar_toupper (uc);

  if (NULL == qf) return NULL;

  DEBUG ("xaccGetQuickFill(): index = %u\n", key);

  return g_hash_table_lookup (qf->matches, GUINT_TO_POINTER (key));
}

/********************************************************************\
\********************************************************************/

QuickFill *
gnc_quickfill_get_string_len_match (QuickFill *qf,
                                    const char *str, int len)
{
  const char *c;
  gunichar uc;

  if (NULL == qf) return NULL;
  if (NULL == str) return NULL;

  c = str;
  while (*c && (len > 0))
  {
    if (qf == NULL)
      return NULL;

    uc = g_utf8_get_char (c);
    qf = gnc_quickfill_get_char_match (qf, uc);

    c = g_utf8_next_char (c);
    len--;
  }

  return qf;
}

/********************************************************************\
\********************************************************************/

QuickFill *
gnc_quickfill_get_string_match (QuickFill *qf, const char *str)
{
  if (NULL == qf) return NULL;
  if (NULL == str) return NULL;

  return gnc_quickfill_get_string_len_match (qf, str, g_utf8_strlen (str, -1));
}

/********************************************************************\
\********************************************************************/

static void
unique_len_helper (gpointer key, gpointer value, gpointer data)
{
  QuickFill **qf_p = data;

  *qf_p = value;
}

QuickFill *
gnc_quickfill_get_unique_len_match (QuickFill *qf, int *length)
{
  if (length != NULL)
    *length = 0;

  if (qf == NULL)
    return NULL;

  while (1)
  {
    guint count;

    count = g_hash_table_size (qf->matches);

    if (count != 1)
    {
      return qf;
    }

    g_hash_table_foreach (qf->matches, unique_len_helper, &qf);

    if (length != NULL)
      (*length)++;
  }
}

/********************************************************************\
\********************************************************************/

void
gnc_quickfill_insert (QuickFill *qf, const char *text, QuickFillSort sort)
{
  gchar *normalized_str;

  if (NULL == qf) return;
  if (NULL == text) return;


  normalized_str = g_utf8_normalize (text, -1, G_NORMALIZE_NFC);
  quickfill_insert_recursive (qf, normalized_str, 0, sort);
  g_free (normalized_str);
}

/********************************************************************\
\********************************************************************/

static void
quickfill_insert_recursive (QuickFill *qf, const char *text, int depth,
                            QuickFillSort sort)
{
  guint key;
  char *old_text;
  QuickFill *match_qf;
  int len;
  char *key_char;
  gunichar key_char_uc;

  if (qf == NULL)
    return;
  
  if ((text == NULL) || (g_utf8_strlen (text, -1) <= depth))
    return;

  key_char = g_utf8_offset_to_pointer (text, depth);

  key_char_uc = g_utf8_get_char (key_char);
  key = g_unichar_toupper (key_char_uc);

  match_qf = g_hash_table_lookup (qf->matches, GUINT_TO_POINTER (key));
  if (match_qf == NULL)
  {
    match_qf = gnc_quickfill_new ();
    g_hash_table_insert (qf->matches, GUINT_TO_POINTER (key), match_qf);
  }

  old_text = match_qf->text;

  switch (sort)
  {
    case QUICKFILL_ALPHA:
      if (old_text && (g_utf8_collate (text, old_text) >= 0))
        break;

    case QUICKFILL_LIFO:
    default:
      len = g_utf8_strlen (text, -1);

      /* If there's no string there already, just put the new one in. */
      if (old_text == NULL)
      {
        match_qf->text = gnc_string_cache_insert((gpointer) text);
        match_qf->len = len;
        break;
      }

      /* Leave prefixes in place */
      if ((len > match_qf->len) &&
          (strncmp(text, old_text, strlen(old_text)) == 0))
        break;

      gnc_string_cache_remove(old_text);
      match_qf->text = gnc_string_cache_insert((gpointer) text);
      match_qf->len = len;
      break;
  }

  quickfill_insert_recursive (match_qf, text, ++depth, sort);
}

/********************** END OF FILE *********************************\
\********************************************************************/
