/********************************************************************\
 * option-util.h -- GNOME<->guile option interface                  *
 * Copyright (C) 1998,1999 Linas Vepstas                            *
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
\********************************************************************/

#ifndef OPTION_UTIL_H
#define OPTION_UTIL_H

#include <glib.h>

#include "gnc-commodity.h"
#include "qof.h"
#include "GNCId.h"

typedef struct gnc_option GNCOption;
typedef struct gnc_option_section GNCOptionSection;
typedef struct gnc_option_db GNCOptionDB;

typedef int GNCOptionDBHandle;

typedef void (*GNCOptionSetUIValue) (GNCOption *option,
                                     gboolean use_default);
typedef void (*GNCOptionSetSelectable) (GNCOption *option,
                                        gboolean selectable);
typedef void (*GNCOptionChangeCallback) (gpointer user_data);

/***** Prototypes ********************************************************/

void gnc_option_set_changed (GNCOption *option, gboolean changed);

/** Returns an opaque pointer to the widget of this option. The actual
 * GUI implementation in dialog-options.c will store a GtkWidget* in
 * here. */
gpointer gnc_option_get_widget (GNCOption *option);

/** Store an opaque pointer to the widget of this option. The actual
 * GUI implementation in dialog-options.c will store a GtkWidget* in
 * here. */
void gnc_option_set_widget (GNCOption *option, gpointer widget);

void gnc_option_set_selectable (GNCOption *option, gboolean selectable);

void          gnc_option_db_destroy(GNCOptionDB *odb);

void gnc_option_db_load_from_kvp(GNCOptionDB* odb, kvp_frame *slots);

char * gnc_option_section(GNCOption *option);

char * gnc_option_sort_tag(GNCOption *option);

gdouble  gnc_option_color_range(GNCOption *option);

guint32  gnc_option_get_color_argb(GNCOption *option);
gboolean gnc_option_get_color_info(GNCOption *option,
                                   gboolean use_default,
                                   gdouble *red,
                                   gdouble *green,
                                   gdouble *blue,
                                   gdouble *alpha);

gboolean gnc_option_db_dirty(GNCOptionDB *odb);
void     gnc_option_db_clean(GNCOptionDB *odb);

gboolean gnc_option_db_get_changed(GNCOptionDB *odb);

gboolean gnc_option_db_lookup_boolean_option(GNCOptionDB *odb,
        const char *section,
        const char *name,
        gboolean default_value);

char * gnc_option_db_lookup_string_option(GNCOptionDB *odb,
        const char *section,
        const char *name,
        const char *default_value);

char * gnc_option_db_lookup_font_option(GNCOptionDB *odb,
                                        const char *section,
                                        const char *name,
                                        const char *default_value);

char * gnc_option_db_lookup_multichoice_option(GNCOptionDB *odb,
        const char *section,
        const char *name,
        const char *default_value);

time64 gnc_option_db_lookup_date_option(GNCOptionDB *odb,
                                        const char *section,
                                        const char *name,
                                        gboolean *is_relative,
                                        Timespec *set_ab_value,
                                        char **set_rel_value,
                                        Timespec *default_value);

gdouble gnc_option_db_lookup_number_option(GNCOptionDB *odb,
        const char *section,
        const char *name,
        gdouble default_value);

gboolean gnc_option_db_lookup_color_option(GNCOptionDB *odb,
        const char *section,
        const char *name,
        gdouble *red,
        gdouble *green,
        gdouble *blue,
        gdouble *alpha);

guint32 gnc_option_db_lookup_color_option_argb(GNCOptionDB *odb,
        const char *section,
        const char *name,
        guint32 default_value);

GSList * gnc_option_db_lookup_list_option(GNCOptionDB *odb,
        const char *section,
        const char *name,
        GSList *default_value);

void gnc_free_list_option_value(GSList *list);

gnc_commodity *
gnc_option_db_lookup_currency_option(GNCOptionDB *odb,
                                     const char *section,
                                     const char *name,
                                     gnc_commodity *default_value);

void gnc_option_db_set_option_default(GNCOptionDB *odb,
                                      const char *section,
                                      const char *name);

gboolean gnc_option_db_set_number_option(GNCOptionDB *odb,
        const char *section,
        const char *name,
        gdouble value);

gboolean gnc_option_db_set_boolean_option(GNCOptionDB *odb,
        const char *section,
        const char *name,
        gboolean value);

gboolean gnc_option_db_set_string_option(GNCOptionDB *odb,
        const char *section,
        const char *name,
        const char *value);

/* private */
void gncp_option_invoke_callback(GNCOptionChangeCallback callback,
                                 gpointer data);

/* Reset all the widgets in one section to their default values */
void gnc_option_db_section_reset_widgets (GNCOptionSection *section);

/* Reset all the widgets to their default values */
void gnc_option_db_reset_widgets (GNCOptionDB *odb);

#endif /* OPTION_UTIL_H */
