/********************************************************************\
 * druid-utils.h -- utility functions for creating druids           *
 * Copyright (C) 2001 Jeremy Collins                                *
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

#ifndef DRUID_UTILS_H
#define DRUID_UTILS_H

#include <gnome.h>

void gnc_druid_set_title_image (GnomeDruid *druid, char *image_path);
void gnc_druid_set_logo_image (GnomeDruid *druid, char *image_path);
void gnc_druid_set_watermark_image (GnomeDruid *druid, char *image_path);

void gnc_druid_set_colors (GnomeDruid *druid);

#endif
