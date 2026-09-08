/*
 * render.c
 *
 * Copyright (c) 1995-1997, John Kilburg <john@cs.unlv.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "port_before.h"

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>

#include "port_after.h"

#include "ChimeraP.h"

struct ChimeraRenderP
{
  MemPool            mp;              /* memory pool */
  ChimeraRenderHooks rh;              /* renderer hooks */
  ChimeraRenderHooks orh;             /* override renderer hooks */
  void               *rc;             /* renderer context */
  void               *orc;            /* override renderer context */
  ChimeraContext     wc;
  ChimeraGUI         wd;
  ChimeraSink        wp;
  ChimeraRenderActionProc rap;        /* kludge */
  void               *rapc;           /* kludge context */
};

/*
 * RenderDestroy
 */
void
RenderDestroy(wn)
ChimeraRender wn;
{
  if (wn->rh.destroy != NULL) CMethod(wn->rh.destroy)(wn->rc);
  MPDestroy(wn->mp);
  return;
}

/*
 * RenderAdd
 */
void
RenderAdd(wn)
ChimeraRender wn;
{
  if (wn->rh.add != NULL) CMethod(wn->rh.add)(wn->rc);
  return;
}

/*
 * RenderEnd
 */
void
RenderEnd(wn)
ChimeraRender wn;
{
  if (wn->rh.end != NULL) CMethod(wn->rh.end)(wn->rc);
  return;
}

/*
 * RenderQuery
 */
char *
RenderQuery(wn, key)
ChimeraRender wn;
char *key;
{
  if (wn->rh.query != NULL)
  {
    return(CMethodBytePtr(wn->rh.query)(wn->rc, key));
  }
  return(NULL);
}

/*
 * RenderSearch
 */
int
RenderSearch(wn, key, mode)
ChimeraRender wn;
char *key;
int mode;
{
  if (wn->rh.search != NULL)
  {
    return(CMethodInt(wn->rh.search)(wn->rc, key, mode));
  }
  return(-1);
}

/*
 * RenderCancel
 */
void
RenderCancel(wn)
ChimeraRender wn;
{
  if (wn->rh.cancel != NULL)
  {
    CMethodVoid(wn->rh.cancel)(wn->rc);
  }
  return;
}

/*
 * RenderCreate
 */
ChimeraRender
RenderCreate(wc, wd, wp, rh, orh, orc, state, rap, rapc)
ChimeraContext wc;
ChimeraGUI wd;
ChimeraSink wp;
ChimeraRenderHooks *rh, *orh;
void *orc;
void *state;
ChimeraRenderActionProc rap;
void *rapc;
{
  ChimeraRender wn;
  MemPool mp;

  myassert(rh->init != NULL, "Callback is NULL.");
  myassert(rh->add != NULL, "Callback is NULL.");
  myassert(rh->end != NULL, "Callback is NULL.");
  myassert(rh->destroy != NULL, "Callback is NULL.");
  myassert(rh->expose != NULL, "Callback is NULL.");
  myassert(rh->cancel != NULL, "Callback is NULL.");

  mp = MPCreate();
  wn = (ChimeraRender)MPCGet(mp, sizeof(struct ChimeraRenderP));
  wn->mp = mp;
  wn->wc = wc;
  wn->wd = wd;
  wn->wp = wp;
  wn->orc = orc;

  wn->rap = rap;
  wn->rapc = rapc;

  memcpy(&(wn->rh), rh, sizeof(ChimeraRenderHooks));
  if (orh != NULL) memcpy(&(wn->orh), orh, sizeof(ChimeraRenderHooks));

  if ((wn->rc =
       CMethodVoidPtr(rh->init)(wn, rh->class_context, state)) == NULL)
  {
    MPDestroy(wn->mp);
    return(NULL);
  }

  GUIAddRender(wd, wn);

  return(wn);
}

void *
RenderGetState(wn)
ChimeraRender wn;
{
  if (wn->rh.getstate != NULL)
  {
    return(CMethodVoidPtr(wn->rh.getstate)(wn->rc));
  }
  return(NULL);
}

void
RenderAction(wn, wr, action)
ChimeraRender wn;
ChimeraRequest *wr;
char *action;
{
  if (wn->rap == NULL)
  {
    if (strcmp("download", action) == 0) DownloadOpen(wn->wc->cres, wr);
    else if (strcmp("external", action) == 0) ViewOpen(wn->wc->cres, wr);
    else fprintf (stderr, "Action cannot be taken.  Bug.\n");
  }
  else
  {
    CMethod(wn->rap)(wn->rapc, wn, wr, action);
  }

  return;
}

ChimeraGUI
RenderToGUI(wn)
ChimeraRender wn;
{
  return(wn->wd);
}

ChimeraSink
RenderToSink(wn)
ChimeraRender wn;
{
  return(wn->wp);
}

ChimeraContext
RenderToContext(wn)
ChimeraRender wn;
{
  return(wn->wc);
}

void
RenderSendMessage(wn, message)
ChimeraRender wn;
char *message;
{
  HeadPrintMessage(wn->wc, message);
  return;
}

void
RenderExpose(wn, x, y, width, height)
ChimeraRender wn;
int x, y;
unsigned int width, height;
{
  if (wn->rh.expose != NULL)
  {
    CMethodBool(wn->rh.expose)(wn->rc, x, y, width, height);
  }
  return;
}

void
RenderSelect(wn, x, y, action)
ChimeraRender wn;
int x, y;
char *action;
{
  if (wn->orc != NULL && wn->orh.select != NULL)
  {
    CMethodBool(wn->orh.select)(wn->orc, x, y, action);
  }
  else if (wn->rh.select != NULL)
  {
    CMethodBool(wn->rh.select)(wn->rc, x, y, action);
  }

  return;
}

void
RenderMotion(wn, x, y)
ChimeraRender wn;
int x, y;
{
  if (wn->orc != NULL && wn->orh.motion != NULL)
  {
    CMethodBool(wn->orh.motion)(wn->orc, x, y);
  }
  if (wn->rh.motion != NULL)
  {
    CMethodBool(wn->rh.motion)(wn->rc, x, y);
  }
  return;
}

/*
 * RenderToResources
 */
ChimeraResources
RenderToResources(wn)
ChimeraRender wn;
{
  return(wn->wc->cres);
}
