/*
 * plain.c
 *
 * libplain - a bad plain text renderer
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

/*
 * This sucks.
 */

#include "port_before.h"

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <X11/Intrinsic.h>

#include "port_after.h"

#include "Chimera.h"
#include "ChimeraRender.h"

#define PLAIN_MARGIN 20

typedef struct
{
  char *s;
  size_t len;
  int y;
  unsigned int width;
} LineInfoP, *LineInfo;

typedef struct
{
  MemPool mp;
  GList lines;
  size_t off;
  size_t len;
  byte *data;

  XFontStruct *font;
  Window win;
  Widget w;
  Display *dpy;
  GC gc;
  Pixel fg, bg;

  ChimeraSink wp;
  ChimeraResources cres;
  ChimeraGUI wd;

  /* Dimensions stuff */
  unsigned int lineSpace;
  unsigned int baseline;
  unsigned int lineheight;
  unsigned int width, height;

  LineInfo searchline;
  char *searchchar;
  LineInfo searchcur;
  char *searchstr;
  size_t searchlen;
} PlainInfoP, *PlainInfo;

static LineInfo MakeLine _ArgProto((PlainInfo, char *, size_t));
static void ParsePlainData _ArgProto((PlainInfo));
static int PlainSearch _ArgProto((void *, char *, int));

static LineInfo
MakeLine(pi, s, len)
PlainInfo pi;
char *s;
size_t len;
{
  LineInfo li;

  li = (LineInfo)MPCGet(pi->mp, sizeof(LineInfoP));
  li->s = s;
  li->len = len;
  li->y = pi->height;
  pi->height += pi->lineheight;
  
  li->width = XTextWidth(pi->font, s, len);
  if (li->width > pi->width) pi->width = li->width + PLAIN_MARGIN * 2;

  GUISetDimensions(pi->wd, pi->width, pi->height);

  XSetFont(pi->dpy, pi->gc, pi->font->fid);
  XSetForeground(pi->dpy, pi->gc, pi->fg);

  XDrawString(pi->dpy, pi->win, pi->gc, PLAIN_MARGIN, li->y + pi->baseline,
	      li->s,
	      (int)li->len); /* what to do about this ? */

  GListAddTail(pi->lines, li);

  return(li);
}

static void
ParsePlainData(pi)
PlainInfo pi;
{
  char *fcp, *cp, *lcp;
  MIMEHeader mh;

  SinkGetData(pi->wp, &(pi->data), &(pi->len), &mh);

  fcp = (char *)(pi->data + pi->off);
  lcp = (char *)(pi->data + pi->len);
  for (cp = fcp; cp < lcp; cp++)
  {
    if (*cp == '\r')
    {
      MakeLine(pi, fcp, cp - fcp);
      pi->off = (byte *)cp - pi->data + 1;
      fcp = cp + 1;
      if (fcp < lcp && *fcp == '\n')
      {
	fcp++;
	pi->off++;
      }
    }
    else if (*cp == '\n')
    {
      MakeLine(pi, fcp, cp - fcp);
      pi->off = (byte *)cp - pi->data + 1;
      fcp = cp + 1;
    }
  }

  return;
}

static void
PlainAdd(closure)
void *closure;
{
  ParsePlainData((PlainInfo)closure);
  return;
}

static void
PlainEnd(closure)
void *closure;
{
  PlainInfo pi = (PlainInfo)closure;

  ParsePlainData(pi);

  if (pi->off < pi->len)
  {
    MakeLine(pi, (char *)(pi->data + pi->off), pi->len - pi->off);
  }

  pi->height += pi->lineheight;
  GUISetDimensions(pi->wd, pi->width, pi->height);

  return;
}

static bool
PlainExpose(closure, x, y, width, height)
void *closure;
int x, y;
unsigned int width, height;
{
  LineInfo li;
  PlainInfo pi = (PlainInfo)closure;
  Dimension swidth;

  XSetFont(pi->dpy, pi->gc, pi->font->fid);
  XSetForeground(pi->dpy, pi->gc, pi->fg);

  for (li = (LineInfo)GListGetHead(pi->lines); li != NULL;
       li = (LineInfo)GListGetNext(pi->lines))
  {
    if (li == pi->searchcur)
    {
      XDrawString(pi->dpy, pi->win, pi->gc, PLAIN_MARGIN, li->y + pi->baseline,
		  li->s, pi->searchstr - li->s);
      swidth = XTextWidth(pi->font, li->s, pi->searchstr - li->s);

      XSetForeground(pi->dpy, pi->gc, pi->bg);
      XSetBackground(pi->dpy, pi->gc, pi->fg);

      XDrawString(pi->dpy, pi->win, pi->gc,
		  PLAIN_MARGIN + swidth, li->y + pi->baseline,
		  pi->searchstr, pi->searchlen);

      XSetForeground(pi->dpy, pi->gc, pi->fg);
      XSetBackground(pi->dpy, pi->gc, pi->bg);

      swidth = XTextWidth(pi->font, pi->searchstr, pi->searchlen);

      XDrawString(pi->dpy, pi->win, pi->gc,
		  PLAIN_MARGIN + swidth, li->y + pi->baseline,
		  pi->searchstr + pi->searchlen,
		  li->len - (pi->searchstr - li->s) - pi->searchlen);

    }
    else
    {
      XDrawString(pi->dpy, pi->win, pi->gc,
		  PLAIN_MARGIN, li->y + pi->baseline,
		  li->s, (int)li->len ); /* what to do about this ? */
    }
  }

  return(true);
}

static bool
PlainMouse(closure, x, y, action)
void *closure;
int x, y;
char *action;
{
  return(true);
}

static bool
PlainMotion(closure, x, y)
void *closure;
int x, y;
{
  return(true);
}

/*
 * PlainQuery
 */
static byte *
PlainQuery(closure, key)
void *closure;
char *key;
{
  return(NULL);
}

/*
 * PlainSearch
 */
static int
PlainSearch(closure, data, mode)
void *closure;
char *data;
int mode;
{
  LineInfo li;
  PlainInfo pi = (PlainInfo)closure;
  char *cp, *lcp, *xcp, *dcp, *ldcp;
  size_t dlen;
  bool found;

  dlen = strlen(data);
  for (li = (LineInfo)GListGetHead(pi->lines); li != NULL;
       li = (LineInfo)GListGetNext(pi->lines))
  {
    lcp = li->s + li->len;
    for (cp = li->s; cp < lcp; cp++)
    {
      if (*cp == data[0])
      {
	dcp = data;
	ldcp = data + dlen;
	found = true;
	for (xcp = cp; dcp < ldcp && xcp < lcp; dcp++, xcp++)
	{
	  if (*xcp != *dcp)
	  {
	    found = false;
	    break;
	  }
	}
	if (found && dcp == ldcp)
	{
	  GUISetScrollPosition(pi->wd, 0, -(li->y));
	  pi->searchcur = li;
	  pi->searchstr = cp;
	  pi->searchlen = dlen;
	  pi->searchline = li;
	  pi->searchchar = xcp;
	  return(0);
	}
      }
    }
  }

  return(-1);
}

/*
 * PlainDestroy
 */
static void
PlainDestroy(closure)
void *closure;
{
  PlainInfo pi = (PlainInfo)closure;

  MPDestroy(pi->mp);

  return;
}

/*
 * PlainCancel
 */
static void
PlainCancel(closure)
void *closure;
{
  return;
}

/*
 * PlainInit
 */
static void *
PlainInit(wn, closure, state)
ChimeraRender wn;
void *closure;
void *state;
{
  PlainInfo pi;
  MemPool mp;
  char *name;
  unsigned int width, height;
  ChimeraSink wp;
  ChimeraGUI wd;

  wd = RenderToGUI(wn);
  wp = RenderToSink(wn);

  mp = MPCreate();
  pi = (PlainInfo)MPCGet(mp, sizeof(PlainInfoP));
  pi->mp = mp;
  pi->wp = wp;
  pi->lineheight = 40;
  pi->lines = GListCreateX(mp);
  pi->wd = wd;
  pi->cres = RenderToResources(wn);

  pi->dpy = GUIToDisplay(wd);
  pi->win = GUIToWindow(wd);
  pi->gc = DefaultGC(pi->dpy, DefaultScreen(pi->dpy));

  pi->lineSpace = 2;

  name = ResourceGetString(pi->cres, "plain.font");
  if (name == NULL) name = "fixed";
  if ((pi->font = XLoadQueryFont(pi->dpy, name)) == NULL)
  {
    fprintf (stderr, "Could not get default font.\n");
    fflush(stderr);
    return(NULL);
  }

  pi->baseline = pi->font->ascent + pi->lineSpace;
  pi->lineheight = pi->font->ascent + pi->font->descent + pi->lineSpace;

  pi->height = PLAIN_MARGIN;

  GUIGetNamedColor(wd, "black", &(pi->fg));
  pi->bg = GUIBackgroundPixel(wd);

  XSetForeground(pi->dpy, pi->gc, pi->fg);
  
  GUISetScrollBar(wd, true);
  if (GUIGetDimensions(wd, &width, &height) == -1)
  {
    GUISetDimensions(wd, width, height);
    /* Now grab the internal dimensions again */
    if (GUIGetDimensions(wd, &width, &height) == -1)
    {
      return(NULL);
    }
  }

  return(pi);
}

/*
 * InitModule_Plain
 */
int
InitModule_Plain(cres)
ChimeraResources cres;
{
  ChimeraRenderHooks rh;

  memset(&rh, 0, sizeof(rh));
  rh.class_destroy = NULL;
  rh.content = "text/plain";
  rh.init = PlainInit;
  rh.add = PlainAdd;
  rh.end = PlainEnd;
  rh.destroy = PlainDestroy;
  rh.search = PlainSearch;
  rh.query = PlainQuery;
  rh.cancel = PlainCancel;
  rh.expose = PlainExpose;
  rh.select = PlainMouse;
  rh.motion = PlainMotion;
  RenderAddHooks(cres, &rh);

  return(0);
}
