/********************************************************************\
 * basiccell.c -- base class for editable cell in a table           *
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
\********************************************************************/

/*
 * FILE:
 * basiccell.c
 *
 * FUNCTION: 
 * Implements the base class for the cell handler object.
 * See the header file for additional documentation.
 *
 * HISTORY:
 * Copyright (c) 1998 Linas Vepstas
 * Copyright (c) 2000 Dave Peticolas <dave@krondo.com>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "basiccell.h"
#include "dialog-utils.h"
#include "gnc-engine-util.h"


gboolean
gnc_cell_name_equal (const char * cell_name_1,
                     const char * cell_name_2)
{
  return (safe_strcmp (cell_name_1, cell_name_2) == 0);
}

BasicCell *
gnc_basic_cell_new (void)
{
  BasicCell * cell;

  cell = g_new0 (BasicCell, 1);

  gnc_basic_cell_init (cell);

  return cell;
}

static void
gnc_basic_cell_clear (BasicCell *cell)
{
  g_free (cell->cell_name);
  cell->cell_name = NULL;

  cell->changed = FALSE;
  cell->conditionally_changed = FALSE;

  cell->value = NULL;
  cell->value_w = NULL;
  cell->value_len = 0;

  cell->set_value = NULL;
  cell->enter_cell = NULL;
  cell->modify_verify = NULL;
  cell->direct_update = NULL;
  cell->leave_cell = NULL;
  cell->gui_realize = NULL;
  cell->gui_move = NULL;
  cell->gui_destroy = NULL;

  cell->is_popup = FALSE;

  cell->gui_private = NULL;

  g_free (cell->sample_text);
  cell->sample_text = NULL;
}

void
gnc_basic_cell_init (BasicCell *cell)
{
  gnc_basic_cell_clear (cell);

  cell->value = g_strdup ("");

  cell->value_len = gnc_mbstowcs (&cell->value_w, cell->value);
}

void
gnc_basic_cell_destroy (BasicCell *cell)
{
  if (cell->destroy)
    cell->destroy (cell);

  /* give any gui elements a chance to clean up */
  if (cell->gui_destroy)
    (*(cell->gui_destroy)) (cell);

  /* free up data strings */
  g_free (cell->value);
  cell->value = NULL;

  g_free (cell->value_w);
  cell->value_w = NULL;

  /* help prevent access to freed memory */
  gnc_basic_cell_clear (cell);

  /* free the object itself */
  g_free (cell);
}

void
gnc_basic_cell_set_name (BasicCell *cell, const char *name)
{
  if (!cell) return;
  if (cell->cell_name == name) return;

  g_free (cell->cell_name);
  cell->cell_name = g_strdup (name);
}

gboolean
gnc_basic_cell_has_name (BasicCell *cell, const char *name)
{
  if (!cell) return FALSE;
  if (!name) return FALSE;
  if (!cell->cell_name) return FALSE;

  return (strcmp (name, cell->cell_name) == 0);
}

void
gnc_basic_cell_set_sample_text (BasicCell *cell,
                                const char *sample_text)
{
  if (!cell) return;
  if (cell->sample_text == sample_text) return;

  g_free (cell->sample_text);
  cell->sample_text = g_strdup (sample_text);
}

void
gnc_basic_cell_set_alignment (BasicCell *cell,
                              CellAlignment alignment)
{
  if (!cell) return;
  cell->alignment = alignment;
}

void
gnc_basic_cell_set_expandable (BasicCell *cell, gboolean expandable)
{
  if (!cell) return;
  cell->expandable = expandable;
}

void
gnc_basic_cell_set_span (BasicCell *cell, gboolean span)
{
  if (!cell) return;
  cell->span = span;
}

const char *
gnc_basic_cell_get_value (BasicCell *cell)
{
  g_return_val_if_fail (cell != NULL, NULL);

  return cell->value;
}

void
gnc_basic_cell_set_value (BasicCell *cell, const char *val)
{
  CellSetValueFunc cb;

  cb = cell->set_value;
  if (cb)
  {
    /* avoid recursion by disabling the  
     * callback while it's being called. */
    cell->set_value = NULL;
    cb (cell, val);
    cell->set_value = cb;
  }
  else
    gnc_basic_cell_set_value_internal (cell, val);
}

gboolean
gnc_basic_cell_get_changed (BasicCell *cell)
{
  if (!cell) return FALSE;

  return cell->changed;
}

gboolean
gnc_basic_cell_get_conditionally_changed (BasicCell *cell)
{
  if (!cell) return FALSE;

  return cell->conditionally_changed;
}

void
gnc_basic_cell_set_changed (BasicCell *cell, gboolean changed)
{
  if (!cell) return;

  cell->changed = changed;
}

void
gnc_basic_cell_set_conditionally_changed (BasicCell *cell, gboolean changed)
{
  if (!cell) return;

  cell->conditionally_changed = changed;
}

void
gnc_basic_cell_set_value_internal (BasicCell *cell, const char *value)
{
  if (value == NULL)
    value = "";

  g_free (cell->value);
  cell->value = g_strdup (value);

  g_free (cell->value_w);
  cell->value_len = gnc_mbstowcs (&cell->value_w, cell->value);
}

void
gnc_basic_cell_set_wcvalue_internal (BasicCell *cell, const GdkWChar *value)
{
  if (!value)
  {
    gnc_basic_cell_set_value_internal (cell, "");
    return;
  }

  g_free (cell->value);
  cell->value = gnc_wcstombs (value);

  g_free (cell->value_w);
  cell->value_len = gnc_mbstowcs (&cell->value_w, cell->value);
}
