/********************************************************************
 * gnc-html-guppi.h -- embed objects in the html stream          *
 * Copyright (C) 2000 Bill Gribble <grib@billgribble.com>           *
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
\********************************************************************/

#ifndef GNC_HTML_GUPPI_H
#define GNC_HTML_GUPPI_H

#include <gnome.h>
#include "gnc-html.h"

void        gnc_html_guppi_init(void);
void        gnc_html_guppi_shutdown(void);
GtkWidget * gnc_html_embedded_piechart(gnc_html * parent, 
                                       gint w, gint h, GHashTable * params);
GtkWidget * gnc_html_embedded_barchart(gnc_html * parent,
                                       gint w, gint h, GHashTable * params);
GtkWidget * gnc_html_embedded_scatter(gnc_html * parent,
                                      gint w, gint h, GHashTable * params);
GtkWidget * gnc_html_embedded_account_tree(gnc_html * parent,
                                           gint w, gint h, GHashTable * prms);

#endif
