/*
 * FILE:
 * RpcServer.c
 *
 * FUNCTION:
 * Implements the Gnucash RPC server.
 *
 * HISTORY:
 * Created By:	Derek Atkins <warlord@MIT.EDU>
 * Copyright (c) 2001, Derek Atkins
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact:
 *
 * Free Software Foundation           Voice:  +1-617-542-5942
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652
 * Boston, MA  02110-1301,  USA       gnu@gnu.org
 */

#define _GNU_SOURCE

#include "config.h"

#include "qof"

#include "RpcServer.h"
#include "RpcServerP.h"
#include "RpcSock.h"
#include "gncRpc.h"
#include "xprt_thrd.h"

extern void gncrpc_prog_1(struct svc_req *, register SVCXPRT *);

static short module = MOD_BACKEND;

static void myClose (void *arg)
{
  GncRpcSvc *cl = (GncRpcSvc *)arg;

  if (!cl)
    return;

  TXPRT_DESTROY (cl->xprt);
  if (cl->clnt)
    CLNT_DESTROY (cl->clnt);
  if (cl->book)
    gnc_book_destroy (cl->book);

  PINFO ("Client Disconnected: %p", cl);

  /* Remove myself from the Client List */
  cl->clist->clist = g_list_remove (cl->clist->clist, cl);

  g_free (cl);
}

int rpc_server_run (unsigned short port)
{
  RPCSock *master;
  int ret;
  int run = 1;
  GncRPCClist clist;

  ENTER ("port=%d", port);

  if (!port)
    port = RPCEND_PORT;

  if ((ret = RpcCreateListener (htons(port), &master)) != 0) {
    LEAVE ("listener failed");
    return ret;
  }

  fprintf (stderr, "RPC Server Running...\n");

  while (run) {
    GncRpcSvc *new = g_malloc (sizeof (*new));
    if (!new) {
      LEAVE ("g_malloc failed");
      return -1;
    }

    memset (new, 0, sizeof (*new));
    new->book = gnc_book_new ();
    if (!new->book) {
      g_free (new);
      LEAVE ("gnc_book_new() failed");
      return -2;
    }

    new->clist = &clist;

    /* Setup to listen */
    RpcListen (master, 3);

    /* Grab the next client */
    if ((ret = RpcAccept (master, &(new->sock))) != 0) {
      run = 0;
      LEAVE ("Accept failed (%d)", ret);
      break;
    }

    /* XXX Authenticate? */

    /* Build the RPC Transport */
    if ((ret = RpcTransport (new->sock, myClose, (void *)new, &(new->xprt)))
	!= 0) {
      RpcClose (new->sock);
      run = 0;
      LEAVE ("Transport failed (%d)", ret);
      break;
    }

    /* Add this client to the clist */
    clist.clist = g_list_prepend (clist.clist, new);

    /* Register the service */
    TXPRT_REG_CALLOUT (new->xprt, GNCRPC_PROG, GNCRPC_VERS, gncrpc_prog_1);
    PINFO ("New Client connected: %p", new);
  }
  return ret;
}
