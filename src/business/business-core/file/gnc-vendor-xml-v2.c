/********************************************************************\
 * gnc-vendor-xml-v2.c -- vendor xml i/o implementation         *
 *                                                                  *
 * Copyright (C) 2002 Derek Atkins <warlord@MIT.EDU>                *
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

#include "config.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include "gnc-xml-helper.h"

#include "sixtp.h"
#include "sixtp-utils.h"
#include "sixtp-parsers.h"
#include "sixtp-utils.h"
#include "sixtp-dom-parsers.h"
#include "sixtp-dom-generators.h"

#include "gnc-xml.h"
#include "io-gncxml-gen.h"
#include "io-gncxml-v2.h"

#include "gncBillTermP.h"
#include "gncVendorP.h"
#include "gncTaxTableP.h"
#include "gnc-vendor-xml-v2.h"
#include "gnc-address-xml-v2.h"
#include "gnc-engine-util.h"

#include "gncObject.h"

#define _GNC_MOD_NAME	GNC_VENDOR_MODULE_NAME

static short module = MOD_IO;

const gchar *vendor_version_string = "2.0.0";

/* ids */
#define gnc_vendor_string "gnc:GncVendor"
#define vendor_name_string "vendor:name"
#define vendor_guid_string "vendor:guid"
#define vendor_id_string "vendor:id"
#define vendor_addr_string "vendor:addr"
#define vendor_notes_string "vendor:notes"
#define vendor_terms_string "vendor:terms"
#define vendor_taxincluded_string "vendor:taxincluded"
#define vendor_active_string "vendor:active"
#define vendor_commodity_string "vendor:commodity"
#define vendor_taxtable_string "vendor:taxtable"
#define vendor_taxtableoverride_string "vendor:use-tt"

static void
maybe_add_string (xmlNodePtr ptr, const char *tag, const char *str)
{
  if (str && strlen(str) > 0)
    xmlAddChild (ptr, text_to_dom_tree (tag, str));
}

static xmlNodePtr
vendor_dom_tree_create (GncVendor *vendor)
{
    xmlNodePtr ret;
    GncBillTerm *term;
    GncTaxTable *taxtable;

    ret = xmlNewNode(NULL, gnc_vendor_string);
    xmlSetProp(ret, "version", vendor_version_string);

    xmlAddChild(ret, guid_to_dom_tree(vendor_guid_string,
				      gncVendorGetGUID (vendor)));

    xmlAddChild(ret, text_to_dom_tree(vendor_name_string,
                                      gncVendorGetName (vendor)));
    
    xmlAddChild(ret, text_to_dom_tree(vendor_id_string,
                                      gncVendorGetID (vendor)));

    xmlAddChild(ret, gnc_address_to_dom_tree(vendor_addr_string,
					     gncVendorGetAddr (vendor)));
    
    maybe_add_string (ret, vendor_notes_string, gncVendorGetNotes (vendor));

    term = gncVendorGetTerms (vendor);
    if (term)
      xmlAddChild(ret, guid_to_dom_tree(vendor_terms_string,
					gncBillTermGetGUID (term)));

    xmlAddChild(ret, text_to_dom_tree(vendor_taxincluded_string,
				      gncTaxIncludedTypeToString (
				     gncVendorGetTaxIncluded (vendor))));

    xmlAddChild(ret, int_to_dom_tree(vendor_active_string,
				     gncVendorGetActive (vendor)));

    xmlAddChild
      (ret,
       commodity_ref_to_dom_tree(vendor_commodity_string,
				 gncVendorGetCommodity (vendor)));

    xmlAddChild (ret, int_to_dom_tree (vendor_taxtableoverride_string,
				       gncVendorGetTaxTableOverride (vendor)));
    taxtable = gncVendorGetTaxTable (vendor);
    if (taxtable)
      xmlAddChild (ret, guid_to_dom_tree (vendor_taxtable_string,
					  gncTaxTableGetGUID (taxtable)));

    return ret;
}

/***********************************************************************/

struct vendor_pdata
{
  GncVendor *vendor;
  GNCBook *book;
};

static gboolean
set_string(xmlNodePtr node, GncVendor* vendor,
           void (*func)(GncVendor *vendor, const char *txt))
{
  char* txt = dom_tree_to_text(node);
  g_return_val_if_fail(txt, FALSE);
    
  func(vendor, txt);
  
  g_free(txt);
    
  return TRUE;
}

static gboolean
set_boolean(xmlNodePtr node, GncVendor* vendor,
	    void (*func)(GncVendor* vendor, gboolean b))
{
    gint64 val;
    gboolean ret;

    ret = dom_tree_to_integer(node, &val);
    if (ret)
      func(vendor, (gboolean)val);

    return ret;
}

static gboolean
vendor_name_handler (xmlNodePtr node, gpointer vendor_pdata)
{
    struct vendor_pdata *pdata = vendor_pdata;

    return set_string(node, pdata->vendor, gncVendorSetName);
}

static gboolean
vendor_guid_handler (xmlNodePtr node, gpointer vendor_pdata)
{
    struct vendor_pdata *pdata = vendor_pdata;
    GUID *guid;
    GncVendor *vendor;

    guid = dom_tree_to_guid(node);
    g_return_val_if_fail(guid, FALSE);
    vendor = gncVendorLookup (pdata->book, guid);
    if (vendor) {
      gncVendorDestroy (pdata->vendor);
      pdata->vendor = vendor;
      gncVendorBeginEdit (vendor);
    } else {
      gncVendorSetGUID(pdata->vendor, guid);
    }

    g_free(guid);
    
    return TRUE;
}

static gboolean
vendor_id_handler (xmlNodePtr node, gpointer vendor_pdata)
{
    struct vendor_pdata *pdata = vendor_pdata;

    return set_string(node, pdata->vendor, gncVendorSetID);
}

static gboolean
vendor_notes_handler (xmlNodePtr node, gpointer vendor_pdata)
{
    struct vendor_pdata *pdata = vendor_pdata;

    return set_string(node, pdata->vendor, gncVendorSetNotes);
}

static gboolean
vendor_terms_handler (xmlNodePtr node, gpointer vendor_pdata)
{
    struct vendor_pdata *pdata = vendor_pdata;
    GUID *guid;
    GncBillTerm *term;

    guid = dom_tree_to_guid(node);
    g_return_val_if_fail (guid, FALSE);
    term = gncBillTermLookup (pdata->book, guid);
    if (!term) {
      term = gncBillTermCreate (pdata->book);
      gncBillTermBeginEdit (term);
      gncBillTermSetGUID (term, guid);
      gncBillTermCommitEdit (term);
    } else
      gncBillTermDecRef (term);

    g_free (guid);
    gncVendorSetTerms (pdata->vendor, term);
    
    return TRUE;
}

static gboolean
vendor_addr_handler (xmlNodePtr node, gpointer vendor_pdata)
{
    struct vendor_pdata *pdata = vendor_pdata;

    return gnc_dom_tree_to_address (node, gncVendorGetAddr(pdata->vendor));
}

static gboolean
vendor_taxincluded_handler (xmlNodePtr node, gpointer vendor_pdata)
{
    struct vendor_pdata *pdata = vendor_pdata;
    GncTaxIncluded type;
    char *str;
    gboolean ret;

    str = dom_tree_to_text (node);
    g_return_val_if_fail (str, FALSE);

    ret = gncTaxIncludedStringToType (str, &type);
    g_free (str);

    if (ret)
      gncVendorSetTaxIncluded(pdata->vendor, type);

    return ret;
}

static gboolean
vendor_active_handler (xmlNodePtr node, gpointer vendor_pdata)
{
    struct vendor_pdata *pdata = vendor_pdata;
    return set_boolean (node, pdata->vendor, gncVendorSetActive);
}

static gboolean
vendor_commodity_handler (xmlNodePtr node, gpointer vendor_pdata)
{
    struct vendor_pdata *pdata = vendor_pdata;
    gnc_commodity *com;

    com = dom_tree_to_commodity_ref(node, pdata->book);
    g_return_val_if_fail (com, FALSE);

    gncVendorSetCommodity (pdata->vendor, com);

    return TRUE;
}

static gboolean
vendor_taxtable_handler (xmlNodePtr node, gpointer vendor_pdata)
{
    struct vendor_pdata *pdata = vendor_pdata;
    GUID *guid;
    GncTaxTable *taxtable;

    guid = dom_tree_to_guid (node);
    g_return_val_if_fail (guid, FALSE);
    taxtable = gncTaxTableLookup (pdata->book, guid);
    if (!taxtable) {
      taxtable = gncTaxTableCreate (pdata->book);
      gncTaxTableBeginEdit (taxtable);
      gncTaxTableSetGUID (taxtable, guid);
      gncTaxTableCommitEdit (taxtable);
    } else
      gncTaxTableDecRef (taxtable);

    gncVendorSetTaxTable (pdata->vendor, taxtable);
    g_free(guid);
    return TRUE;
}

static gboolean
vendor_taxtableoverride_handler (xmlNodePtr node, gpointer vendor_pdata)
{
    struct vendor_pdata *pdata = vendor_pdata;
    return set_boolean (node, pdata->vendor, gncVendorSetTaxTableOverride);
}

static struct dom_tree_handler vendor_handlers_v2[] = {
    { vendor_name_string, vendor_name_handler, 1, 0 },
    { vendor_guid_string, vendor_guid_handler, 1, 0 },
    { vendor_id_string, vendor_id_handler, 1, 0 },
    { vendor_addr_string, vendor_addr_handler, 1, 0 },
    { vendor_notes_string, vendor_notes_handler, 0, 0 },
    { vendor_terms_string, vendor_terms_handler, 0, 0 },
    { vendor_taxincluded_string, vendor_taxincluded_handler, 1, 0 },
    { vendor_active_string, vendor_active_handler, 1, 0 },
    { vendor_commodity_string, vendor_commodity_handler, 1, 0 },
    { vendor_taxtable_string, vendor_taxtable_handler, 0, 0 },
    { vendor_taxtableoverride_string, vendor_taxtableoverride_handler, 0, 0 },
    { NULL, 0, 0, 0 }
};

static GncVendor*
dom_tree_to_vendor (xmlNodePtr node, GNCBook *book)
{
    struct vendor_pdata vendor_pdata;
    gboolean successful;

    vendor_pdata.vendor = gncVendorCreate(book);
    vendor_pdata.book = book;
    gncVendorBeginEdit (vendor_pdata.vendor);

    successful = dom_tree_generic_parse (node, vendor_handlers_v2,
                                         &vendor_pdata);

    if (successful)
      gncVendorCommitEdit (vendor_pdata.vendor);
    else
    {
        PERR ("failed to parse vendor tree");
        gncVendorDestroy (vendor_pdata.vendor);
        vendor_pdata.vendor = NULL;
    }

    return vendor_pdata.vendor;
}

static gboolean
gnc_vendor_end_handler(gpointer data_for_children,
			 GSList* data_from_children, GSList* sibling_data,
			 gpointer parent_data, gpointer global_data,
			 gpointer *result, const gchar *tag)
{
    int successful;
    GncVendor *vendor;
    xmlNodePtr tree = (xmlNodePtr)data_for_children;
    gxpf_data *gdata = (gxpf_data*)global_data;
    GNCBook *book = gdata->bookdata;

    successful = TRUE;

    if(parent_data)
    {
        return TRUE;
    }

    /* OK.  For some messed up reason this is getting called again with a
       NULL tag.  So we ignore those cases */
    if(!tag)
    {
        return TRUE;
    }

    g_return_val_if_fail(tree, FALSE);

    vendor = dom_tree_to_vendor(tree, book);
    if(vendor != NULL)
    {
        gdata->cb(tag, gdata->parsedata, vendor);
    }

    xmlFreeNode(tree);

    return vendor != NULL;
}

static sixtp *
vendor_sixtp_parser_create(void)
{
  return sixtp_dom_parser_new(gnc_vendor_end_handler, NULL, NULL);
}

static void
do_count (gpointer vendor_p, gpointer count_p)
{
  int *count = count_p;
  (*count)++;
}

static int
vendor_get_count (GNCBook *book)
{
  int count = 0;
  gncObjectForeach (_GNC_MOD_NAME, book, do_count, (gpointer) &count);
  return count;
}

static void
xml_add_vendor (gpointer vendor_p, gpointer out_p)
{
  xmlNodePtr node;
  GncVendor *vendor = vendor_p;
  FILE *out = out_p;

  node = vendor_dom_tree_create (vendor);
  xmlElemDump(out, NULL, node);
  fprintf(out, "\n");
  xmlFreeNode (node);
}

static void
vendor_write (FILE *out, GNCBook *book)
{
  gncObjectForeach (_GNC_MOD_NAME, book, xml_add_vendor, (gpointer) out);
}

void
gnc_vendor_xml_initialize (void)
{
  static GncXmlDataType_t be_data = {
    GNC_FILE_BACKEND_VERS,
    gnc_vendor_string,
    vendor_sixtp_parser_create,
    NULL,			/* add_item */
    vendor_get_count,
    vendor_write,
  };

  gncObjectRegisterBackend (_GNC_MOD_NAME,
			    GNC_FILE_BACKEND,
			    &be_data);
}
