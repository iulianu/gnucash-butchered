/********************************************************************\
 * quickfillcell-gnome.c --  implement gnome part of quickfill cell *
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

/* quickfillcell-gnome.c
 *
 * Implements gnome dependent quickfill cell functions.
 *
 * Copyright (C) 2000 Dave Peticolas <peticola@cs.ucdavis.edu>
 */

#include "config.h"

#include <gnome.h>

#include "quickfillcell.h"


static gboolean
QuickFillDirect (BasicCell *bcell,
                 int *cursor_position,
                 int *start_selection,
                 int *end_selection,
                 void *gui_data)
{
        QuickFillCell *cell = (QuickFillCell *) bcell;
        GdkEventKey *event = gui_data;
        const char *match_str;
        QuickFill *match;
        int prefix_len;

        if (event->type != GDK_KEY_PRESS)
                return FALSE;

        switch (event->keyval) {
                case GDK_slash:
                        if (!(event->state & GDK_MOD1_MASK))
                                return FALSE;
                        break;
                case GDK_Tab:
                case GDK_ISO_Left_Tab:
                        if (!(event->state & GDK_CONTROL_MASK))
                                return FALSE;
                        break;
                default:
                        return FALSE;
        }

        if ((*start_selection <= *cursor_position) &&
            (*end_selection >= *cursor_position))
                *cursor_position = *start_selection;
        else if ((*end_selection <= *cursor_position) &&
                 (*start_selection >= *cursor_position))
                *cursor_position = *end_selection;

        match = gnc_quickfill_get_string_len_match (cell->qf, bcell->value,
                                                    *cursor_position);

        if (match == NULL)
                return TRUE;

        match = gnc_quickfill_get_unique_len_match (match, &prefix_len);
        if (match == NULL)
                return TRUE;

        match_str = gnc_quickfill_string (match);

        if ((match_str != NULL) &&
            (strncmp(match_str, bcell->value, strlen(bcell->value)) == 0) && 
            (strcmp(match_str, bcell->value) != 0))
                xaccSetBasicCellValue(bcell, match_str);

        *cursor_position += prefix_len;
        *start_selection = *cursor_position;
        *end_selection = -1;

        return TRUE;
}


void
xaccQuickFillGUIInit (QuickFillCell *cell)
{
        if (cell == NULL)
                return;

        cell->cell.direct_update = QuickFillDirect;
}

/* =============== end of file =================== */

/*
  Local Variables:
  c-basic-offset: 8
  End:
*/
