/********************************************************************\
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

#ifndef GNUCASH_GRID_H
#define GNUCASH_GRID_H

#include <gnome.h>

#include "table-allgui.h"


#define GNUCASH_TYPE_GRID     (gnucash_grid_get_type ())
#define GNUCASH_GRID(obj)     (GTK_CHECK_CAST((obj), GNUCASH_TYPE_GRID, GnucashGrid))
#define GNUCASH_GRID_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), GNUCASH_TYPE_GRID))
#define GNUCASH_IS_GRID(o)    (GTK_CHECK_TYPE((o), GNUCASH_TYPE_GRID))


typedef struct {
        GnomeCanvasItem canvas_item;

        GnucashSheet *sheet;

        /* The first and last displayed block */
        int        top_block;
        int        bottom_block;

        /* Offset from spreadsheet origin in units */
        long       top_offset;
        long       left_offset;

        GdkGC      *grid_gc;	/* Draw grid gc */
        GdkGC      *fill_gc;	/* Default background fill gc */
        GdkGC      *gc;		/* Color used for the cell */

        GdkColor   background;
        GdkColor   grid_color;
        GdkColor   default_color;

        GdkFont *normal_font;
        GdkFont *italic_font;
} GnucashGrid;


typedef struct {
        GnomeCanvasItemClass parent_class;
} GnucashGridClass;


GtkType    gnucash_grid_get_type (void);
GtkWidget *gnucash_grid_new 	 (GnucashSheet *sheet);

gboolean
gnucash_grid_find_block_origin_by_pixel (GnucashGrid *grid,
                                         gint x, gint y,
                                         VirtualCellLocation *vcell_loc,
                                         gint *o_x, gint *o_y);

gboolean
gnucash_grid_find_cell_by_pixel (GnucashGrid *grid, gint x, gint y,
                                 VirtualLocation *vcell_loc);


#endif /* GNUCASH_GRID_H */


/*
  Local Variables:
  c-basic-offset: 8
  End:
*/
