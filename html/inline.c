/*
 * inline.c
 *
 * libhtml - HTML->X renderer
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

#include "port_after.h"

#include "html.h"

struct HTMLInlineP
{
  HTMLInfo li;

  ChimeraGUI wd;
  ChimeraSink wp;
  ChimeraRender wn;
  HTMLInlineInfo ii;

  bool setupcalled;

  HTMLBox box;

  unsigned int width, height, border;
  unsigned int vspace, hspace;
  char *mapurl;
  bool ismap;
  HTMLEnv env;
  bool delayed;

  ChimeraRenderHooks orh;
  void *rc;
};

static int ImgInit _ArgProto((ChimeraSink, void *));
static void ImgAdd _ArgProto((void *));
static void ImgEnd _ArgProto((void *));
static void ImgMessage _ArgProto((void *, char *));

static void DestroyInline _ArgProto((HTMLInfo, HTMLBox));
static bool InlineSelectCallback _ArgProto((void *, int, int, char *));
static bool InlineMotionCallback _ArgProto((void *, int, int));

void InlineResizeCallback _ArgProto((ChimeraGUI, void *,
					    unsigned int, unsigned int));

static void InlineContinue _ArgProto((HTMLInline));

static void SetupInline _ArgProto((HTMLInfo, HTMLBox));
static void AddInline _ArgProto((HTMLInline));

static void HandleInlineReload _ArgProto((HTMLInfo, ChimeraRequest *));

/*
 * InlineContinue
 */
static void
InlineContinue(isp)
HTMLInline isp;
{
  if (isp->delayed)
  {
    isp->delayed = false;
    HTMLContinueLayout(isp->li);
  }
  return;
}

/*
 * SetupInline
 */
static void
SetupInline(li, box)
HTMLInfo li;
HTMLBox box;
{
  HTMLInline isp = (HTMLInline)box->closure;

  isp->setupcalled = true;
  if (isp->wd != NULL)
  {
    GUIMap(isp->wd, box->x + isp->hspace, box->y + isp->vspace);
  }

  return;
}

/*
 * DestroyInline
 */
static void
DestroyInline(li, box)
HTMLInfo li;
HTMLBox box;
{
  HTMLInline isp = (HTMLInline)box->closure;

  if (isp->wn != NULL)
  {
    RenderDestroy(isp->wn);
    isp->wn = NULL;
  }
  if (isp->wp != NULL)
  {
    SinkSetHooks(isp->wp, NULL, NULL);
    isp->wp = NULL;
  }
  if (isp->wd != NULL)
  {
    GUIDestroy(isp->wd);
    isp->wd = NULL;
  }

  return;
}

/*
 * AddInline
 */
static void
AddInline(isp)
HTMLInline isp;
{
  HTMLBox box;
  HTMLAttribID aid;
  int border;

  isp->box = box = HTMLCreateBox(isp->li, isp->env);
  
  if ((border = MLAttributeToInt(isp->ii.p, "border")) < 0) isp->border = 0;
  else isp->border = border;

  aid = HTMLAttributeToID(isp->ii.p, "align");
  if (aid == ATTRIB_MIDDLE) box->baseline = isp->height / 2;
  else if (aid == ATTRIB_TOP) box->baseline = 0;
  else if (aid == ATTRIB_LEFT) HTMLSetB(box, BOX_FLOAT_LEFT);
  else if (aid == ATTRIB_RIGHT) HTMLSetB(box, BOX_FLOAT_RIGHT);
  else box->baseline = isp->height;

  box->setup = SetupInline;
  box->destroy = DestroyInline;
  box->width = isp->width + isp->hspace * 2;
  box->height = isp->height + isp->vspace * 2;
  box->closure = isp;
  
  HTMLEnvAddBox(isp->li, isp->env, box);

  return;
}

/*
 * InlineResizeCallback
 */
void
InlineResizeCallback(wd, closure, width, height)
ChimeraGUI wd;
void *closure;
unsigned int width, height;
{
  HTMLInline isp = (HTMLInline)closure;

  if (isp->box == NULL)
  {
    isp->width = width;
    isp->height = height;
    AddInline(isp);
    InlineContinue(isp);
  }

  return;
}

/*
 * InlineSelectCallback
 */
static bool
InlineSelectCallback(closure, x, y, action)
void *closure;
int x, y;
char *action;
{
  HTMLInline isp = (HTMLInline)closure;
  HTMLAnchor a;
  GList list;
  char *url;

  myassert(isp->box != NULL, "MouseCallback: box == NULL");

  if (isp->mapurl != NULL)
  {
    if ((url = HTMLFindMapURL(isp->li, isp->mapurl, x, y)) != NULL)
    {
      HTMLLoadURL(isp->li, NULL, url, action);
      return(true);
    }
  }

  list = isp->li->alist;
  for (a = (HTMLAnchor)GListGetHead(list); a != NULL;
       a = (HTMLAnchor)GListGetNext(list))
  {
    if (a->box == isp->box)
    {
      HTMLLoadAnchor(isp->li, a, x, y, action, isp->ismap);
      return(true);
    }
  }

  return(true);
}

static bool
InlineMotionCallback(closure, x, y)
void *closure;
int x, y;
{
  HTMLInline isp = (HTMLInline)closure;
  HTMLAnchor a;
  GList list;
  char *url;

  myassert(isp->box != NULL, "MotionCallback: box == NULL");

  if (isp->mapurl != NULL)
  {
    if ((url = HTMLFindMapURL(isp->li, isp->mapurl, x, y)) != NULL)
    {
      HTMLPrintURL(isp->li, url);
      return(true);
    }
  }

  isp->li->over = NULL;
  list = isp->li->alist;
  for (a = (HTMLAnchor)GListGetHead(list); a != NULL;
       a = (HTMLAnchor)GListGetNext(list))
  {
    if (a->box == isp->box)
    {
      HTMLPrintAnchor(isp->li, a, x, y, isp->ismap);
      isp->li->over = a;
      return(true);
    }
  }

  return(true);
}

/*
 * ImgMessage
 */
static void
ImgMessage(closure, message)
void *closure;
char *message;
{
  HTMLInline isp = (HTMLInline)closure;

  if (isp->li->wn != NULL) RenderSendMessage(isp->li->wn, message);
  return;
}

/*
 * ImgEnd
 */
static void
ImgEnd(closure)
void *closure;
{
  HTMLInline isp = (HTMLInline)closure;
  RenderEnd(isp->wn);
  return;
}

/*
 * ImgAdd
 */
static void
ImgAdd(closure)
void *closure;
{
  HTMLInline isp = (HTMLInline)closure;
  RenderAdd(isp->wn);
  return;
}

/*
 * ImgInit
 */
static int
ImgInit(wp, closure)
ChimeraSink wp;
void *closure;
{
  HTMLInline isp = (HTMLInline)closure;
  ChimeraRenderHooks *rh;
  char *ctype;

  ctype = SinkGetInfo(wp, "content-type");
  if ((rh = RenderGetHooks(SinkToResources(wp), ctype)) == NULL)
  {
    SinkCancel(wp);
    InlineContinue(isp);
    return(-1);
  }

  isp->wd = GUICreate(isp->li->wc, isp->li->wd, InlineResizeCallback, isp);
  if (isp->width > 0 && isp->height > 0)
  {
    GUISetInitialDimensions(isp->wd, isp->width, isp->height);
  }
  if (isp->setupcalled)
  {
    GUIMap(isp->wd, isp->box->x + isp->hspace, isp->box->y + isp->vspace);
  }

  isp->wn = RenderCreate(isp->li->wc, isp->wd, isp->wp,
			 rh, &(isp->orh), isp->rc, NULL,
			 NULL, NULL);
  if (isp->wn == NULL)
  {
    SinkCancel(wp);
    return(-1);
  }

  return(0);
}

/*
 *
 * Public functions
 *
 */
HTMLInline
HTMLCreateInline(li, env, url, ii, orh, rc)
HTMLInfo li;
HTMLEnv env;
char *url;
HTMLInlineInfo *ii;
ChimeraRenderHooks *orh;
void *rc;
{
  ChimeraSinkHooks hooks;
  HTMLInline isp;
  ChimeraRequest *wr;
  int width, height, vspace, hspace;

  if ((wr = RequestCreate(li->cres, url, li->burl)) == NULL)
  {
    return(NULL);
  }

  if (li->reload) HandleInlineReload(li, wr);

  /* Select only image content that we can deal with directly */
  if (RequestAddRegexContent(li->cres, wr, "image/*") <= 0)
  {
    RequestDestroy(wr);
    return(NULL);
  }

  isp = (HTMLInline)MPCGet(li->mp, sizeof(struct HTMLInlineP));
  memcpy(&(isp->ii), ii, sizeof(HTMLInlineInfo));
  isp->li = li;
  isp->env = env;
  if (orh == NULL)
  {
    memset(&(isp->orh), 0, sizeof(isp->orh));
    isp->orh.select = InlineSelectCallback;
    isp->orh.motion = InlineMotionCallback;
    isp->rc = isp;
  }
  else
  {
    memcpy(&(isp->orh), orh, sizeof(ChimeraRenderHooks));
    isp->rc = rc;
  }

  if ((width = MLAttributeToInt(ii->p, "width")) < 0) isp->width = 0;
  else isp->width = width;
  if ((height = MLAttributeToInt(ii->p, "height")) < 0) isp->height = 0;
  else isp->height = height;
  if ((vspace = MLAttributeToInt(ii->p, "vspace")) < 0) isp->vspace = 0;
  else isp->vspace = vspace;
  if ((hspace = MLAttributeToInt(ii->p, "hspace")) < 0) isp->hspace = 0;
  else isp->hspace = hspace;
  isp->ismap = MLFindAttribute(ii->p, "ismap") != NULL ? true:false;
  isp->mapurl = MLFindAttribute(ii->p, "usemap");
 
  if (isp->width == 0 || isp->height == 0)
  {
    isp->delayed = true;
    HTMLDelayLayout(li);
  }
  else AddInline(isp);

  memset(&hooks, 0, sizeof(hooks));
  hooks.init = ImgInit;
  hooks.add = ImgAdd;
  hooks.end = ImgEnd;
  hooks.message = ImgMessage;

  if ((isp->wp = SinkCreate(li->cres, wr)) == NULL)
  {
    InlineContinue(isp);
  }
  else
  {
    SinkSetHooks(isp->wp, &hooks, isp);
    GListAddHead(li->sinks, isp->wp);
  }

  return(isp);
}

/*
 * HTMLInlineToBox
 */
HTMLBox
HTMLInlineToBox(is)
HTMLInline is;
{
  return(is->box);
}

/*
 * HTMLInlineToInfo
 */
HTMLInfo
HTMLInlineToInfo(is)
HTMLInline is;
{
  return(is->li);
}

/*
 * HTMLInlineDestroy
 */
void
HTMLInlineDestroy(is)
HTMLInline is;
{
  return;
}

/*
 * HTMLImg
 */
void
HTMLImg(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  char *url;
  HTMLInlineInfo ii;

  if ((url = MLFindAttribute(p, "src")) == NULL) return;

  memset(&ii, 0, sizeof(HTMLInlineInfo));
  ii.p = p;

  HTMLCreateInline(li, env, url, &ii, NULL, NULL);

  return;
}

/*
 * HTMLImgInsert
 *
 * Always OK image insertion for now.  I suppose this is where one
 * could check for conditional image loading.  The idea here is to
 * start the download as soon as possible.
 */
HTMLInsertStatus
HTMLImgInsert(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  char *url;
  ChimeraRequest *wr;
  ChimeraSink wp;

  if ((url = MLFindAttribute(p, "src")) == NULL) return(HTMLInsertOK);

  if ((wr = RequestCreate(li->cres, url, li->burl)) == NULL)
  {
    return(HTMLInsertOK);
  }

  if (li->reload) HandleInlineReload(li, wr);

  /* Select only image content that we can deal with directly */
  if (RequestAddRegexContent(li->cres, wr, "image/*") <= 0)
  {
    RequestDestroy(wr);
    return(HTMLInsertOK);
  }

  if ((wp = SinkCreate(li->cres, wr)) != NULL)
  {
    SinkSetHooks(wp, NULL, NULL);
    GListAddHead(li->sinks, wp);
  }

  return(HTMLInsertOK);
}

/*
 * HandleInlineReload
 */
static void
HandleInlineReload(li, wr)
HTMLInfo li;
ChimeraRequest *wr;
{
  char *xurl;

  for (xurl = (char *)GListGetHead(li->loads); xurl != NULL;
       xurl = (char *)GListGetNext(li->loads))
  {
    if (strlen(wr->url) == strlen(xurl) && strcmp(wr->url, xurl) == 0)
    {
      break;
    }
  }
  if (xurl == NULL)
  {
    RequestReload(wr, true);
    GListAddHead(li->loads, MPStrDup(li->mp, wr->url));
  }

  return;
}
