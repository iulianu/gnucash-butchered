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

/*
 *  configure the cursor styles
 */

#include "config.h"

#include "gnucash-style.h"
#include "gnucash-grid.h"
#include "gnucash-color.h"
#include "messages.h"

#include "date.h"


#define DEFAULT_FONT "-adobe-helvetica-medium-r-normal--*-120-*-*-*-*-*-*"
#define HINT_FONT    "-adobe-helvetica-medium-o-normal--*-120-*-*-*-*-*-*"

#define DEFAULT_STYLE_WIDTH 680

GdkFont *gnucash_register_font = NULL;
GdkFont *gnucash_register_hint_font = NULL;

static char *register_font_name = NULL;
static char *register_hint_font_name = NULL;

static RegisterBorders reg_borders = 0;

static char *
style_get_key (SheetBlockStyle *style)
{
        switch (style->cursor_type) {
        case GNUCASH_CURSOR_HEADER:
        case GNUCASH_CURSOR_SINGLE:
        case GNUCASH_CURSOR_TRANS:
        case GNUCASH_CURSOR_SPLIT:
                return "singles";
                break;

        case GNUCASH_CURSOR_DOUBLE:
                return "doubles";
                break;
        }

        g_warning("style_get_key: bad cursor type\n");
        return NULL;
}


static void
cell_dimensions_construct (gpointer _cd, gpointer user_data)
{
        CellDimensions *cd = _cd;

        cd->pixel_width = -1;
        cd->can_span_over = TRUE;
}


static BlockDimensions *
style_dimensions_new (SheetBlockStyle *style)
{
        BlockDimensions *dimensions;

        dimensions = g_new0 (BlockDimensions, 1);

        dimensions->nrows = style->nrows;
        dimensions->ncols = style->ncols;

        dimensions->cell_dimensions = g_table_new (sizeof (CellDimensions),
                                                   cell_dimensions_construct,
                                                   NULL, NULL);

        g_table_resize (dimensions->cell_dimensions,
                        style->nrows, style->ncols);

        return dimensions;
}

static void
style_dimensions_destroy (BlockDimensions *dimensions)
{
        if (dimensions == NULL)
                return;

        g_table_destroy (dimensions->cell_dimensions);
        dimensions->cell_dimensions = NULL;

        g_free(dimensions);
}


static void
gnucash_style_dimensions_init (GnucashSheet *sheet, SheetBlockStyle *style)
{
        BlockDimensions *dimensions;

        dimensions = g_hash_table_lookup (sheet->dimensions_hash_table,
                                          style_get_key (style));

        if (!dimensions) {
                dimensions = style_dimensions_new (style);
                g_hash_table_insert (sheet->dimensions_hash_table,
                                     style_get_key (style), dimensions);
        }

        dimensions->refcount++;

        style->dimensions = dimensions;
}


CellDimensions *
gnucash_style_get_cell_dimensions (SheetBlockStyle *style, int row, int col)
{
        if (style == NULL)
                return NULL;
        if (style->dimensions == NULL)
                return NULL;
        if (style->dimensions->cell_dimensions == NULL)
                return NULL;

        return g_table_index (style->dimensions->cell_dimensions, row, col);
}

static int
compute_row_width (BlockDimensions *dimensions, int row, int col1, int col2)
{
        int j;
        int width = 0;

        col1 = MAX(0, col1);
        col2 = MIN(col2, dimensions->ncols-1);

        for (j = col1; j <= col2; j++) {
                CellDimensions *cd;
                cd = g_table_index (dimensions->cell_dimensions, row, j);
                width += cd->pixel_width;
        }

        return width;
}


/* This sets the initial sizes of the cells, based on the sample_text */
static void
set_dimensions_pass_one (GnucashSheet *sheet, CellBlock *cursor,
                         BlockDimensions *dimensions)
{
        GdkFont *font = GNUCASH_GRID(sheet->grid)->normal_font;
        CellDimensions *cd;
        int row, col;

        g_return_if_fail (font != NULL);

        for (row = 0; row < cursor->num_rows; row++)
        {
                for (col = 0; col < cursor->num_cols; col++)
                {
                        int width;
                        char *text;
                        CellBlockCell *cb_cell;

                        cd = g_table_index (dimensions->cell_dimensions,
                                            row, col);

                        cd->pixel_height = (font->ascent + font->descent +
                                            (2 * CELL_VPADDING));

                        cb_cell = gnc_cellblock_get_cell (cursor, row, col);

                        text = cb_cell->sample_text;
                        if (text != NULL)
                                cd->can_span_over = FALSE;

                        if (cd->pixel_width > 0)
                                continue;
                        else if (text)
                        {
                                width = gdk_string_width (font, text);
                                width += 2 * CELL_HPADDING;
                        }
                        else
                                width = 0;

                        cd->pixel_width = width;
                }

                cd = g_table_index (dimensions->cell_dimensions, row, 0);
                dimensions->height += cd->pixel_height;
        }
}


/* Now adjust things to make everything even. This code assumes that
 * all cursors have the same number of columns!!! */
static void
set_dimensions_pass_two (GnucashSheet *sheet, int default_width)
{
        SheetBlockStyle *style;
        BlockDimensions *dimensions;
        CellDimensions *cd;
        GTable *cd_table;
        CellBlock *cursor;

        int num_cols;
        int *widths;
        int width;
        int row, col;
        int i;

        dimensions = sheet->cursor_styles[GNUCASH_CURSOR_HEADER]->dimensions;
        cd_table = dimensions->cell_dimensions;
        cursor = sheet->cursors[GNUCASH_CURSOR_HEADER];

        width = 0;
        num_cols = cursor->num_cols;
        widths = g_new(int, num_cols);

        /* find header widths */
        for (col = 0; col < num_cols; col++)
        {
                cd = g_table_index (cd_table, 0, col);

                widths[col] = cd->pixel_width;
                width += cd->pixel_width;
        }

        if (width < default_width)
                for (col = 0; col < num_cols; col++)
                {
                        CellBlockCell *cb_cell;

                        cb_cell = gnc_cellblock_get_cell (cursor, 0, col);

                        if (!cb_cell->expandable)
                                continue;

                        cd = g_table_index (cd_table, 0, col);

                        cd->pixel_width += (default_width - width);
                        width += (default_width - width);
                        widths[col] = cd->pixel_width;

                        break;
                }

        /* adjust widths to be consistent */
        for (i = 0; i < GNUCASH_NUM_CURSORS; i++)
        {
                style = sheet->cursor_styles[i];
                dimensions = style->dimensions;
                cd_table = dimensions->cell_dimensions;
                cursor = sheet->cursors[i];

                for (row = 0; row < cursor->num_rows; row++)
                        for (col = 0; col < num_cols; col++)
                        {
                                cd = g_table_index (cd_table, row, col);

                                cd->pixel_width = widths[col];
                        }
        }

        /* now expand spanning cells */
        for (i = 0; i < GNUCASH_NUM_CURSORS; i++)
        {
                CellDimensions *cd_span;

                style = sheet->cursor_styles[i];
                dimensions = style->dimensions;
                cd_table = dimensions->cell_dimensions;
                cursor = sheet->cursors[i];

                for (row = 0; row < cursor->num_rows; row++)
                {
                        cd_span = NULL;

                        for (col = 0; col < num_cols; col++)
                        {
                                CellBlockCell *cb_cell;

                                cb_cell = gnc_cellblock_get_cell (cursor,
                                                                  row, col);

                                cd = g_table_index (cd_table, row, col);

                                if (cb_cell->span)
                                {
                                        cd_span = cd;
                                        continue;
                                }

                                if (!cd->can_span_over)
                                        continue;

                                if (cd_span == NULL)
                                        continue;

                                if (cb_cell->sample_text != NULL)
                                {
                                        cd_span = NULL;
                                        continue;
                                }

                                if (cd->pixel_width <= 0)
                                        continue;

                                cd_span->pixel_width += cd->pixel_width;
                                cd->pixel_width = 0;
                        }
                }
        }

        g_free (widths);
}

gint
gnucash_style_row_width(SheetBlockStyle *style, int row)
{
        BlockDimensions *dimensions;

        dimensions = style->dimensions;

        return compute_row_width(dimensions, row, 0, dimensions->ncols - 1);
}

static void
compute_cell_origins_x (BlockDimensions *dimensions)
{
        int x;
        int i, j;

        for (i = 0; i < dimensions->nrows; i++) {
                x = 0;

                for (j = 0; j < dimensions->ncols; j++) {
                        CellDimensions *cd;

                        cd = g_table_index (dimensions->cell_dimensions, i, j);

                        cd->origin_x = x;
                        x += cd->pixel_width;
                }
        }
}

static void
compute_cell_origins_y (BlockDimensions *dimensions)
{
        CellDimensions *cd;
        int y = 0;
        int i, j;

        for (i = 0; i < dimensions->nrows; i++) {
                for (j = 0; j < dimensions->ncols; j++) {
                        cd = g_table_index (dimensions->cell_dimensions, i, j);
                        cd->origin_y = y;
                }
                cd = g_table_index (dimensions->cell_dimensions, i, 0);
                y += cd->pixel_height;
        }
}

/* Calculate the widths and offsets */
static void
set_dimensions_pass_three (GnucashSheet *sheet)
{
        int i;

        for (i = 0; i < GNUCASH_NUM_CURSORS; i++)
        {
                BlockDimensions *dimensions;

                dimensions = sheet->cursor_styles[i]->dimensions;

                dimensions->width = compute_row_width (dimensions, 0, 0,
                                                       dimensions->ncols-1);

                compute_cell_origins_x (dimensions);
                compute_cell_origins_y (dimensions);
        }

}

static void
styles_recompute_layout_dimensions (GnucashSheet *sheet, int default_width)
{
        CellBlock *cursor;
        BlockDimensions *dimensions;
        int i;

        for (i = 0; i < GNUCASH_NUM_CURSORS; i++)
        {
                cursor = sheet->cursors[i];
                dimensions = sheet->cursor_styles[i]->dimensions;

                dimensions->height = 0;
                dimensions->width = default_width;

                set_dimensions_pass_one (sheet, cursor, dimensions);
        }

        set_dimensions_pass_two (sheet, default_width);
        set_dimensions_pass_three (sheet);
}

void
gnucash_sheet_styles_set_dimensions (GnucashSheet *sheet, int default_width)
{
        g_return_if_fail (sheet != NULL);
        g_return_if_fail (GNUCASH_IS_SHEET (sheet));

        styles_recompute_layout_dimensions (sheet, default_width);
}

gint
gnucash_style_col_is_resizable (SheetBlockStyle *style, int col)
{
        if (col < 0 || col >= style->ncols)
                return FALSE;

        return TRUE;
}

void
gnucash_sheet_set_col_width (GnucashSheet *sheet, int col, int width)
{
        CellDimensions *cd;
        SheetBlockStyle *style;
        int total;
        int diff;

        g_return_if_fail (sheet != NULL);
        g_return_if_fail (GNUCASH_IS_SHEET(sheet));
        g_return_if_fail (col >= 0);

        if (width < 0)
                return;

        style = sheet->cursor_styles[GNUCASH_CURSOR_HEADER];

        g_return_if_fail (col < style->ncols);

        cd = gnucash_style_get_cell_dimensions (style, 0, col);

        /* adjust the overall width of this style */
        diff = cd->pixel_width - width;
        cd->pixel_width = width;

        total = MAX (sheet->window_width, sheet->width - diff);

        set_dimensions_pass_two (sheet, total);
        set_dimensions_pass_three (sheet);
}


/* Recompiles the style information from the cellblock, without
 * recomputing the layout info or the dimensions. WARNING: this
 * function assumes that the space for the style info has been
 * allocated already. */
static void
gnucash_sheet_style_recompile(SheetBlockStyle *style, gint cursor_type)
{
        CellBlock *cursor;
        gint i, j, type;

        g_assert (style != NULL);
        g_assert (style->cursor != NULL);

        cursor = style->cursor;

        for (i = 0; i < style->nrows; i++) {
                for (j = 0; j < style->ncols; j++) {
                        CellBlockCell *cb_cell;
                        CellStyle *cs;

                        cb_cell = gnc_cellblock_get_cell (cursor, i, j);
                        cs = gnucash_style_get_cell_style (style, i, j);

                        type = cb_cell->cell_type;

                        style->header_font = gnucash_register_font;

                        gnucash_style_set_borders (style, reg_borders);

                        switch (cb_cell->alignment) {
                                case CELL_ALIGN_RIGHT:
                                        cs->alignment = GTK_JUSTIFY_RIGHT;
                                        break;
                                case CELL_ALIGN_CENTER:
                                        cs->alignment = GTK_JUSTIFY_CENTER;
                                        break;
                                default:
                                case CELL_ALIGN_LEFT:
                                        cs->alignment = GTK_JUSTIFY_LEFT;
                                        break;
                        }
                }
        }
}

void
gnucash_sheet_styles_recompile(GnucashSheet *sheet)
{
        int i;

        g_return_if_fail (sheet != NULL);
        g_return_if_fail (GNUCASH_IS_SHEET (sheet));

        for (i = 0; i < GNUCASH_NUM_CURSORS; i++)
                gnucash_sheet_style_recompile (sheet->cursor_styles[i], i);
}

void
gnucash_style_set_cell_borders (SheetBlockStyle *style,
                                int row, int col, int border)
{
        CellStyle *cs;

        if (style == NULL)
                return;

        cs = gnucash_style_get_cell_style (style, row, col);
        if (cs == NULL)
                return;

        cs->border = border;
}

void
gnucash_style_set_register_borders (int reg_borders_new)
{
        reg_borders = reg_borders_new;
}


void
gnucash_style_set_borders (SheetBlockStyle *style, int border)
{
        int row, col;

        g_return_if_fail (style != NULL);

        for (row  = 0; row < style->nrows; row++) {
                CellStyle *cs;

                for (col = 0; col < style->ncols; col++)
                        gnucash_style_set_cell_borders (style, row, col,
                                                        border);

                cs = gnucash_style_get_cell_style (style, row, 0);
                cs->border |= STYLE_BORDER_LEFT;

                cs = gnucash_style_get_cell_style (style, row,
                                                   style->ncols - 1);
                cs->border |= STYLE_BORDER_RIGHT;
        }
}

void
gnucash_sheet_set_borders (GnucashSheet *sheet, int border)
{
        int i;

        g_return_if_fail (GNUCASH_IS_SHEET (sheet));

        for (i = 0; i < GNUCASH_NUM_CURSORS; i++)
                gnucash_style_set_borders (sheet->cursor_styles[i], border);
}

CellStyle *
gnucash_style_get_cell_style (SheetBlockStyle *style, int row, int col)
{
        if (style == NULL)
                return NULL;

        return g_table_index (style->cell_styles, row, col);
}

static SheetBlockStyle *
gnucash_sheet_style_new (GnucashSheet *sheet, CellBlock *cursor,
                         GNCCursorType cursor_type)
{
        SheetBlockStyle *style;

        g_return_val_if_fail (sheet != NULL, NULL);
        g_return_val_if_fail (GNUCASH_IS_SHEET (sheet), NULL);
        g_return_val_if_fail (cursor != NULL, NULL);

        style = g_new0(SheetBlockStyle, 1);

        style->cursor = cursor;
        style->cursor_type = cursor_type;

        style->nrows = cursor->num_rows;
        style->ncols = cursor->num_cols;

        style->cell_styles = g_table_new (sizeof(CellStyle), NULL, NULL, NULL);
        g_table_resize (style->cell_styles, style->nrows, style->ncols);

        gnucash_sheet_style_recompile(style, cursor_type);

        gnucash_style_dimensions_init (sheet, style);

        return style;
}

void
gnucash_sheet_create_styles (GnucashSheet *sheet)
{
        int i;

        g_return_if_fail (sheet != NULL);
        g_return_if_fail (GNUCASH_IS_SHEET (sheet));

        for (i = 0; i < GNUCASH_NUM_CURSORS; i++)
                sheet->cursor_styles[i] = 
                        gnucash_sheet_style_new (sheet, sheet->cursors[i], i);
}

void
gnucash_sheet_compile_styles (GnucashSheet *sheet)
{
        g_return_if_fail (sheet != NULL);
        g_return_if_fail (GNUCASH_IS_SHEET (sheet));

        gnucash_sheet_styles_set_dimensions (sheet, DEFAULT_STYLE_WIDTH);
}

void
gnucash_sheet_style_destroy (GnucashSheet *sheet, SheetBlockStyle *style)
{
        if (sheet == NULL)
                return;
        if (style == NULL)
                return;

        g_table_destroy (style->cell_styles);
        style->cell_styles = NULL;

        style->dimensions->refcount--;

        if (style->dimensions->refcount == 0) {
                g_hash_table_remove (sheet->dimensions_hash_table,
                                     style_get_key (style));
                style_dimensions_destroy (style->dimensions);
        }

        g_free (style);
}


void
gnucash_sheet_style_get_cell_pixel_rel_coords (SheetBlockStyle *style,
                                               gint cell_row, gint cell_col,
                                               gint *x, gint *y,
                                               gint *w, gint *h)
{
        CellDimensions *cd;

        g_return_if_fail (style != NULL);
        g_return_if_fail (cell_row >= 0 && cell_row <= style->nrows);
        g_return_if_fail (cell_col >= 0 && cell_col <= style->ncols);

        cd = gnucash_style_get_cell_dimensions (style, cell_row, cell_col);

        *x = cd->origin_x;
        *y = cd->origin_y;
        *h = cd->pixel_height;
        *w = cd->pixel_width;
}


SheetBlockStyle *
gnucash_sheet_get_style (GnucashSheet *sheet, VirtualCellLocation vcell_loc)
{
        SheetBlock *block;

        g_return_val_if_fail (sheet != NULL, NULL);
        g_return_val_if_fail (GNUCASH_IS_SHEET(sheet), NULL);

        block = gnucash_sheet_get_block (sheet, vcell_loc);

        if (block)
                return block->style;
        else
                return NULL;
}


SheetBlockStyle *
gnucash_sheet_get_style_from_table (GnucashSheet *sheet,
                                    VirtualCellLocation vcell_loc)
{
        Table *table;
        VirtualCell *vcell;
        CellBlock *cursor;
        int i;

        g_return_val_if_fail (sheet != NULL, NULL);
        g_return_val_if_fail (GNUCASH_IS_SHEET(sheet), NULL);

        table = sheet->table;

        vcell = gnc_table_get_virtual_cell (table, vcell_loc);

        cursor = vcell->cellblock;

        for (i = 0; i < GNUCASH_NUM_CURSORS; i++)
                if (cursor == sheet->cursors[i])
                        return sheet->cursor_styles[i];

        return sheet->cursor_styles[GNUCASH_CURSOR_HEADER];
}


/*
 * For now, refcounting doesn't do much, but later we may want to
 * destroy styles
 */

void
gnucash_style_ref (SheetBlockStyle *style)
{
        g_return_if_fail (style != NULL);

        style->refcount++;
}


void
gnucash_style_unref (SheetBlockStyle *style)
{
        g_return_if_fail (style != NULL);

        style->refcount--;

        if (style->refcount < 0)
                g_warning ("Unbalanced Style ref/unref");
}

void
gnucash_style_set_register_font_name(const char *name)
{
        g_free(register_font_name);
        register_font_name = g_strdup(name);

        if (gnucash_register_font != NULL)
        {
                gdk_font_unref(gnucash_register_font);
                gnucash_register_font = NULL;
        }

        if (register_font_name != NULL)
                gnucash_register_font = gdk_font_load(register_font_name);

        if (gnucash_register_font == NULL)
        {
                g_free(register_font_name);
                register_font_name = NULL;
                gnucash_register_font = gdk_font_load(DEFAULT_FONT);
        }

        g_assert(gnucash_register_font != NULL);

        gdk_font_ref(gnucash_register_font);
}

void
gnucash_style_set_register_hint_font_name(const char *name)
{
        g_free(register_hint_font_name);
        register_hint_font_name = g_strdup(name);

        if (gnucash_register_hint_font != NULL)
        {
                gdk_font_unref(gnucash_register_hint_font);
                gnucash_register_hint_font = NULL;
        }

        if (register_hint_font_name != NULL)
                gnucash_register_hint_font =
                        gdk_font_load(register_hint_font_name);

        if (gnucash_register_hint_font == NULL)
        {
                g_free(register_hint_font_name);
                register_hint_font_name = NULL;
                gnucash_register_hint_font = gdk_font_load(HINT_FONT);
        }

        g_assert(gnucash_register_hint_font != NULL);

        gdk_font_ref(gnucash_register_hint_font);
}

void
gnucash_sheet_get_header_widths (GnucashSheet *sheet, int *header_widths)
{
        SheetBlockStyle *style;
        CellBlock *header;
        int row, col;

        g_return_if_fail(sheet != NULL);
        g_return_if_fail(GNUCASH_IS_SHEET(sheet));

        style = sheet->cursor_styles[GNUCASH_CURSOR_HEADER];
        header = sheet->cursors[GNUCASH_CURSOR_HEADER];

        g_return_if_fail(style != NULL);
        g_return_if_fail(header != NULL);

        for (row = 0; row < style->nrows; row++)
                for (col = 0; col < style->ncols; col++)
                {
                        CellDimensions *cd;
                        CellBlockCell *cb_cell;

                        cd = gnucash_style_get_cell_dimensions (style,
                                                                row, col);
                        if (cd == NULL)
                                continue;

                        cb_cell = gnc_cellblock_get_cell (header, row, col);
                        if (cb_cell == NULL)
                                continue;

                        if (cb_cell->cell_type < 0)
                                continue;

                        header_widths[cb_cell->cell_type] = cd->pixel_width;
                }
}

void
gnucash_sheet_set_header_widths (GnucashSheet *sheet, int *header_widths)
{
        SheetBlockStyle *style;
        CellBlock *header;
        int row, col;

        g_return_if_fail(sheet != NULL);
        g_return_if_fail(GNUCASH_IS_SHEET(sheet));

        style = sheet->cursor_styles[GNUCASH_CURSOR_HEADER];
        header = sheet->cursors[GNUCASH_CURSOR_HEADER];

        g_return_if_fail(style != NULL);
        g_return_if_fail(header != NULL);

        for (row = 0; row < style->nrows; row++)
                for (col = 0; col < style->ncols; col++)
                {
                        CellDimensions *cd;
                        CellBlockCell *cb_cell;

                        cd = gnucash_style_get_cell_dimensions (style,
                                                                row, col);

                        cb_cell = gnc_cellblock_get_cell (header, row, col);

                        if (cb_cell->cell_type < 0)
                                continue;

                        if (header_widths[cb_cell->cell_type] < 0)
                                continue;

                        cd->pixel_width = header_widths[cb_cell->cell_type];
                }
}

const char *
gnucash_style_get_default_register_font_name(void)
{
        return DEFAULT_FONT;
}

const char *
gnucash_style_get_default_register_hint_font_name(void)
{
        return HINT_FONT;
}

void
gnucash_style_init (void)
{
        if (gnucash_register_font == NULL)
                gnucash_style_set_register_font_name(NULL);

        if (gnucash_register_hint_font == NULL)
                gnucash_style_set_register_hint_font_name(NULL);
}


/*
  Local Variables:
  c-basic-offset: 8
  End:
*/
