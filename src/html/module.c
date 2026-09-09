/*
 * module.c
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
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>


#include "html.h"

static bool InAnchor _ArgProto((int, int, HTMLBox));
static void HTMLAdd _ArgProto((void *));
static void HTMLEnd _ArgProto((void *));
static bool HTMLExpose _ArgProto((void *, int, int,
				  unsigned int, unsigned int));
static bool HTMLMotion _ArgProto((void *, int, int));
static bool HTMLMouse _ArgProto((void *, int, int, char *));
static byte *HTMLQuery _ArgProto((void *, char *));
static int HTMLSearch _ArgProto((void *, char *, int));
static void HTMLDestroy _ArgProto((void *));
static void HTMLCancel _ArgProto((void *));
static void *HTMLInit _ArgProto((ChimeraRender, void *, void *));
static void *HTMLGetState _ArgProto((void *));
static void HTMLClassDestroy _ArgProto((void *));
int InitModule_HTML _ArgProto((ChimeraResources));
static void DeselectTimeOut _ArgProto((ChimeraTimeOut, void *));

static void
DeselectTimeOut(wt, closure)
ChimeraTimeOut wt;
void *closure;
{
  HTMLAnchor a;
  HTMLInfo li = (HTMLInfo)closure;

  for (a = (HTMLAnchor)GListGetHead(li->alist); a != NULL;
       a = (HTMLAnchor)GListGetNext(li->alist))
  {
    if (a->p == li->sa->p)
    {
      HTMLClearB(a->box, BOX_SELECT);
      HTMLRenderBox(li, NULL, a->box);
    }
  }

  return;
}

static void
HTMLAdd(closure)
void *closure;
{
  HTMLInfo li = (HTMLInfo)closure;
  byte *data;
  size_t len;
  MIMEHeader mh;

  if (li->cancel) return;

  SinkGetData(li->wp, &data, &len, &mh);

  MLAddData(li->hs, (char *)data, len);

  return;
}

static void
HTMLEnd(closure)
void *closure;
{
  HTMLInfo li = (HTMLInfo)closure;
  byte *data;
  size_t len;
  MIMEHeader mh;

  if (li->cancel) return;

  SinkGetData(li->wp, &data, &len, &mh);

  MLEndData(li->hs, (char *)data, len);

  return;
}

static bool
HTMLExpose(closure, x, y, width, height)
void *closure;
int x, y;
unsigned int width, height;
{
  HTMLInfo li = (HTMLInfo)closure;
  Region r;
  XRectangle rec;

  r = XCreateRegion();
  rec.x = (short)x;
  rec.y = (short)y;
  rec.width = (unsigned short)width;
  rec.height = (unsigned short)height;
  XUnionRectWithRegion(&rec, r, r);

  HTMLRenderBox(li, r, li->firstbox);

  XDestroyRegion(r);

  return(true);
}

/*
 * InAnchor
 */
static bool
InAnchor(x, y, box)
int x, y;
HTMLBox box;
{
  if (x > box->x &&
      x < box->x + box->width &&
      y > box->y &&
      y < box->y + box->height)
  {
    return(true);
  }

  return(false);
}

static bool
HTMLMouse(closure, x, y, action)
void *closure;
int x, y;
char *action;
{
  HTMLInfo li = (HTMLInfo)closure;
  HTMLAnchor a;
  GList list;

  list = li->alist;
  for (a = (HTMLAnchor)GListGetHead(list); a != NULL;
       a = (HTMLAnchor)GListGetNext(list))
  {
    if (InAnchor(x, y, a->box))
    {
      if (a->p != NULL)
      {
	li->sa = a;
	for (a = (HTMLAnchor)GListGetHead(list); a != NULL;
	     a = (HTMLAnchor)GListGetNext(list))
	{
	  if (a->p == li->sa->p)
	  {
	    HTMLSetB(a->box, BOX_SELECT);
	    HTMLRenderBox(li, NULL, a->box);
	  }
	}
	XFlush(li->dpy);
	HTMLLoadAnchor(li, li->sa, x, y, action, false);

	if (li->sto != NULL) TimeOutDestroy(li->sto);
	li->sto = TimeOutCreate(li->cres, li->selectTimeOut,
				DeselectTimeOut, li);
      }
      break;
    }
  }

  return(true);
}

static bool
HTMLMotion(closure, x, y)
void *closure;
int x, y;
{
  HTMLInfo li = (HTMLInfo)closure;
  HTMLAnchor a;
  GList list;

  list = li->alist;
  for (a = (HTMLAnchor)GListGetHead(list); a != NULL;
       a = (HTMLAnchor)GListGetNext(list))
  {
    if (InAnchor(x, y, a->box)) break;
  }

  if (a == NULL || a->p == NULL) RenderSendMessage(li->wn, NULL);
  else HTMLPrintAnchor(li, a, x, y, false);
  li->over = a;

  return(true);
}

/*
 * HTMLQuery
 */
static byte *
HTMLQuery(closure, key)
void *closure;
char *key;
{
  HTMLInfo li = (HTMLInfo)closure;

  if (strcmp(key, "title") == 0)
  {
    if (li->title != NULL) return(li->title);
  }
  return(NULL);
}

/*
 * HTMLSearch
 *
 * And here's an interesting development...there is no way to do a
 * search for text unless another field is added to HTMLBox.  That
 * is unsavory.  Work on this later.
 */
static int
HTMLSearch(closure, key, mode)
void *closure;
char *key;
int mode;
{
  return(-1);
}

/*
 * HTMLCancel
 */
static void
HTMLCancel(closure)
void *closure;
{
  HTMLInfo li = (HTMLInfo)closure;
  ChimeraSink wp;

  myassert(li != NULL, "HTMLInfo is NULL!");

  for (wp = (ChimeraSink)GListGetHead(li->sinks); wp != NULL;
       wp = (ChimeraSink)GListGetNext(li->sinks))
  {
    SinkCancel(wp);
  }

  li->cancel = true;

  if (li->wt != NULL)
  {
    TaskRemove(li->cres, li->wt);
    li->wt = NULL;
  }

  return;
}

/*
 * HTMLDestroy
 */
static void
HTMLDestroy(closure)
void *closure;
{
  HTMLInfo li = (HTMLInfo)closure;
  ChimeraSink wp;

  GListRemoveItem(li->lc->contexts, li);

  HTMLDestroyBox(li, li->firstbox);

  while ((wp = (ChimeraSink)GListPop(li->sinks)) != NULL)
  {
    SinkDestroy(wp);
  }

  HTMLDestroyFrameSets(li);

  if (li->wt != NULL) TaskRemove(li->cres, li->wt);
  if (li->sto != NULL) TimeOutDestroy(li->sto);
  if (li->hs != NULL) MLDestroy(li->hs);

  MPDestroy(li->mp);

  return;
}

/*
 * HTMLInit
 */
static void *
HTMLInit(wn, closure, state)
ChimeraRender wn;
void *closure;
void *state;
{
  HTMLInfo li;
  HTMLClass lc = (HTMLClass)closure;
  MemPool mp;
  char *url;
  ChimeraSink wp;
  ChimeraGUI wd;

  wp = RenderToSink(wn);
  wd = RenderToGUI(wn);

  url = SinkGetInfo(wp, "x-url");

  mp = MPCreate();
  li = (HTMLInfo)MPCGet(mp, sizeof(struct HTMLInfoP));
  li->mp = mp;
  li->wc = RenderToContext(wn);
  li->cres = RenderToResources(wn);
  li->ps = (HTMLState)state;
  li->url = url;
  li->burl = url;
  li->wd = wd;
  li->wp = wp;
  li->wn = wn;
  li->widget = GUIToWidget(li->wd);
  li->hs = MLInit(HTMLHandler, li);
  li->lc = lc;
  li->css = lc->css;
  li->envstack = GListCreateX(mp);
  li->selectors = GListCreateX(mp);
  li->oldselectors = GListCreateX(mp);
  li->endstack = GListCreateX(mp);
  li->alist = GListCreateX(mp);
  li->maps = GListCreateX(mp);
  li->sinks = GListCreateX(mp);
  li->loads = GListCreateX(mp);
  li->framesets = GListCreateX(mp);

  if (SinkWasReloaded(wp)) li->reload = true;
  else li->reload = false;

  ResourceGetInt(li->cres, "html.leftMargin", &(li->lmargin));
  if (li->lmargin <= 0) li->lmargin = 20;

  ResourceGetInt(li->cres, "html.rightMargin", &(li->rmargin));
  if (li->rmargin <= 0) li->rmargin = 20;

  ResourceGetInt(li->cres, "html.bottomMargin", &(li->bmargin));
  if (li->bmargin <= 0) li->bmargin = 20;

  ResourceGetInt(li->cres, "html.topMargin", &(li->tmargin));
  if (li->tmargin <= 0) li->tmargin = 20;

  ResourceGetBool(li->cres, "html.printHTMLErrors", &(li->printHTMLErrors));

  li->textLineSpace = -1;
  ResourceGetInt(li->cres, "html.textLineSpace", &(li->textLineSpace));
  if (li->textLineSpace < 0) li->textLineSpace = 3;

  li->tableCellInfinity = 0;
  ResourceGetUInt(li->cres,
		  "html.tableCellInfinity", &(li->tableCellInfinity));

  ResourceGetUInt(li->cres, "html.inlineTimeOut", &(li->inlineTimeOut));
  if (li->inlineTimeOut < 5) li->inlineTimeOut = 5000;

  ResourceGetUInt(li->cres, "html.selectTimeOut", &(li->selectTimeOut));
  if (li->selectTimeOut < 1000) li->selectTimeOut = 1000;

  ResourceGetBool(li->cres, "html.flowDebug", &(li->flowDebug));

  ResourceGetBool(li->cres, "html.constraintDebug", &(li->constraintDebug));

  ResourceGetBool(li->cres, "html.printTags", &(li->printTags));

  /*
   * Graphics setup
   */
  li->dpy = GUIToDisplay(li->wd);
  li->win = GUIToWindow(li->wd);
  li->gc = DefaultGC(li->dpy, DefaultScreen(li->dpy));

  GUIGetNamedColor(wd, "blue", &(li->anchor_color));
  GUIGetNamedColor(wd, "red", &(li->anchor_select_color));
  GUIGetNamedColor(wd, "black", &(li->fg));

  XSetForeground(li->dpy, li->gc, li->fg);

  HTMLSetupFonts(li);

  GUISetScrollBar(wd, true);

  if (GUIGetDimensions(wd, &(li->maxwidth), &(li->maxheight)) == -1)
  {
    /* Dimensions of display not set so we'll set them. */
    li->maxwidth = 200;
    li->maxheight = 200;
    GUISetDimensions(wd, li->maxwidth, li->maxheight);
    /* Now grab the internal dimensions again */
    if (GUIGetDimensions(wd, &(li->maxwidth), &(li->maxheight)) == -1)
    {
      return(NULL);
    }
  }

  GListAddHead(lc->contexts, li);

  HTMLStart(li);

  return(li);
}

/*
 * HTMLClassDestroy
 */
static void
HTMLClassDestroy(closure)
void *closure;
{
  HTMLClass lc = (HTMLClass)closure;

  HTMLFreeFonts(lc);
  MPDestroy(lc->mp);

  return;
}

/*
 * HTMLGetState
 */
void *
HTMLGetState(closure)
void *closure;
{
  HTMLInfo li = (HTMLInfo)closure;
  HTMLState ps;

  if ((ps = (HTMLState)GListPop(li->lc->oldstates)) == NULL)
  {
    ps = (HTMLState)MPCGet(li->lc->mp, sizeof(struct HTMLStateP));
  }

  GUIGetScrollPosition(li->wd, &(ps->x), &(ps->y));

  return(ps);
}

/*
 * InitModule_HTML
 */
int
InitModule_HTML(cres)
ChimeraResources cres;
{
  ChimeraRenderHooks rh;
  HTMLClass lc;
  MemPool mp;
  FILE *fp;
  char *cssfile;
  char *b;
  struct stat st;
  size_t rval, blen;

  mp = MPCreate();
  lc = (HTMLClass)MPCGet(mp, sizeof(struct HTMLClassP));
  lc->mp = mp;
  lc->oldstates = GListCreateX(mp);
  lc->contexts = GListCreateX(mp);

  /*
   * Snarf up the default style sheet.
   */
  if ((cssfile = ResourceGetFilename(cres, mp, "html.cssFile")) != NULL)
  {
    if (stat(cssfile, &st) == 0 && (fp = fopen(cssfile, "r")) != NULL)
    {
      if ((b = (char *)malloc(st.st_size)) == NULL)
      {
	while ((rval = fread(b + blen, 1, st.st_size - blen, fp)) > 0 &&
	       !feof(fp))
	{
	  blen += rval;
	}
	fclose(fp);

	if (blen == st.st_size)
	{
	  lc->css = CSSParseBuffer(NULL, b, blen, NULL, NULL);
	}

	free(b);
      }
    }
  }

  memset(&rh, 0, sizeof(rh));
  rh.class_context = lc;
  rh.class_destroy = HTMLClassDestroy;
  rh.content = "text/html";
  rh.init = HTMLInit;
  rh.add = HTMLAdd;
  rh.end = HTMLEnd;
  rh.search = HTMLSearch;
  rh.destroy = HTMLDestroy;
  rh.getstate = HTMLGetState;
  rh.query = HTMLQuery;
  rh.cancel = HTMLCancel;
  rh.expose = HTMLExpose;
  rh.select = HTMLMouse;
  rh.motion = HTMLMotion;
  RenderAddHooks(cres, &rh);

  return(0);
}

/*
 * HTMLSetFinalPosition
 */
void
HTMLSetFinalPosition(li)
HTMLInfo li;
{
  char *cp;

  if (li->ps != NULL)
  {
    GUISetScrollPosition(li->wd, li->ps->x, li->ps->y);
    GListAddHead(li->lc->oldstates, li->ps);
    li->ps = NULL;
  }
  else
  {
    for (cp = li->url; *cp != '\0'; cp++)
    {
      if (*cp == '#')
      {
	HTMLFindName(li, cp + 1);
	return;
      }
    }
  }

  return;
}
