#include "config.h"

#include <glib.h>
#include <string.h>

#include "gnc-xml-helper.h"

#include "sixtp.h"
#include "sixtp-utils.h"
#include "sixtp-parsers.h"
#include "sixtp-utils.h"
#include "sixtp-dom-parsers.h"
#include "sixtp-dom-generators.h"

#include "gnc-xml.h"
#include "gnc-engine-util.h"
#include "io-gncxml-gen.h"

#include "sixtp-dom-parsers.h"
#include "AccountP.h"
#include "Account.h"
#include "Group.h"

const gchar *commodity_version_string = "2.0.0";

xmlNodePtr
gnc_commodity_dom_tree_create(const gnc_commodity *com)
{
    xmlNodePtr ret;

    ret = xmlNewNode(NULL, "gnc:commodity");

    xmlSetProp(ret, "version", commodity_version_string);
    
    xmlAddChild(ret, text_to_dom_tree("cmdty:space",
                                      gnc_commodity_get_namespace(com)));
    xmlAddChild(ret, text_to_dom_tree("cmdty:id",
                                      gnc_commodity_get_mnemonic(com)));

    if(gnc_commodity_get_fullname(com))
    {
        xmlAddChild(ret, text_to_dom_tree("cmdty:name",
                                          gnc_commodity_get_fullname(com)));
    }

    if(gnc_commodity_get_exchange_code(com) &&
       strlen(gnc_commodity_get_exchange_code(com)) > 0)
    {
        xmlAddChild(ret, text_to_dom_tree(
                        "cmdty:xcode",
                        gnc_commodity_get_exchange_code(com)));
    }
    
    {
        gchar *text;
        text = g_strdup_printf("%d", gnc_commodity_get_fraction(com));
        xmlAddChild(ret, text_to_dom_tree("cmdty:fraction", text));
        g_free(text);
    }

    return ret;
}

/***********************************************************************/

struct com_char_handler
{
    gchar *tag;
    void(*func)(gnc_commodity *com, const char*val);
};

struct com_char_handler com_handlers[] = {
    { "cmdty:space", gnc_commodity_set_namespace },
    { "cmdty:id", gnc_commodity_set_mnemonic },
    { "cmdty:name", gnc_commodity_set_fullname },
    { "cmdty:xcode", gnc_commodity_set_exchange_code },
    { 0, 0 }
};

static void
set_commodity_value(xmlNodePtr node, gnc_commodity* com)
{
    if(safe_strcmp(node->name, "cmdty:fraction") == 0)
    {
        gint64 val;
        if(string_to_integer(node->xmlChildrenNode->content, &val))
        {
            gnc_commodity_set_fraction(com, val);
        }
    }
    else 
    {
        struct com_char_handler *mark;

        for(mark = com_handlers; mark->tag; mark++)
        {
            if(safe_strcmp(mark->tag, node->name) == 0)
            {
                gchar* val = dom_tree_to_text(node);
                (mark->func)(com, val);
                g_free(val);
                break;
            }
        }
    }
}

static gboolean
valid_commodity(gnc_commodity *com)
{
    if(gnc_commodity_get_namespace(com) == NULL)
    {
        g_warning("Invalid commodity: no namespace");
        return FALSE;
    }
    if(gnc_commodity_get_mnemonic(com) == NULL)
    {
        g_warning("Invalid commodity: no mnemonic");
        return FALSE;
    }
    if(gnc_commodity_get_fraction(com) == 0)
    {
        g_warning("Invalid commodity: 0 fraction");
        return FALSE;
    }
    return TRUE;
}

static gboolean
gnc_commodity_end_handler(gpointer data_for_children,
                          GSList* data_from_children, GSList* sibling_data,
                          gpointer parent_data, gpointer global_data,
                          gpointer *result, const gchar *tag)
{
    gnc_commodity *com;
    xmlNodePtr achild;
    xmlNodePtr tree = (xmlNodePtr)data_for_children;
    gxpf_data *gdata = (gxpf_data*)global_data;

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
    
    com = gnc_commodity_new(NULL, NULL, NULL, NULL, 0);

    for(achild = tree->xmlChildrenNode; achild; achild = achild->next)
    {
        set_commodity_value(achild, com);
    }

    if(!valid_commodity(com))
    {
        g_warning("Invalid commodity parsed");
        xmlElemDump(stdout, NULL, tree);
        printf("\n");
        fflush(stdout);
        gnc_commodity_destroy(com);
        return FALSE;
    }

    gdata->cb(tag, gdata->data, com);

    xmlFreeNode(tree);
    
    return TRUE;
}


sixtp*
gnc_commodity_sixtp_parser_create(void)
{
    return sixtp_dom_parser_new(gnc_commodity_end_handler, NULL, NULL);
}
