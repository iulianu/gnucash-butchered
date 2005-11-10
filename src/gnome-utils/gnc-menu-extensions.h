/********************************************************************\
 * gnc-menu-extensions.h -- functions to build dynamic menus        *
 * Copyright (C) 1999 Rob Browning         	                    *
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
\********************************************************************/

#ifndef GNC_MENU_EXTENSIONS_H
#define GNC_MENU_EXTENSIONS_H

#include <gtk/gtk.h>
#include <libguile.h>

typedef struct _ExtensionInfo
{
  SCM extension;

  GtkActionEntry ae;
  gchar *path;
  gchar *sort_key;
  const gchar *typeStr;
  GtkUIManagerItemType type;
} ExtensionInfo;


#define ADDITIONAL_MENUS_PLACEHOLDER "AdditionalMenusPlaceholder"

GSList *gnc_extensions_get_menu_list (void);
void gnc_extension_invoke_cb (SCM extension, SCM window);

/** This function stores a menu item/callback for later insertion into
 *  the application menus,
 *
 *  @param extension A scheme object descrubing the menu to be
 *  inserted.  Functions written in C should use the gnc-plugin code.
 */
void gnc_add_scm_extension(SCM extension);

/** This function inserts all of the menu items stored by
 *  gnc_add_scm_extension() into the application menus.
 *
 *  @param uiMerge The GtkUIManager [GtkUIManager] object to use for
 *  merging.
 */
void gnc_extensions_menu_setup( GtkUIManager *uiMerge );

/** This function releases any memory being held by the 'extensions'
 *  code.  It is called from the window shutdown code.
 */
void gnc_extensions_shutdown(void);

#endif
