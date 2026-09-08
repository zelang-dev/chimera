/*
 * flow.c
 *
 * libhtml - HTML->X renderer
 *
 * Copyright (c) 1996-1997, John Kilburg <john@cs.unlv.edu>
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
#include <ctype.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "port_after.h"

#include "html.h"

typedef struct
{
  int x, y;
  int bloff;
  int bottom;
  unsigned int width, height;
  unsigned int maxwidth;
  GList boxes;                    /* list of boxes in line */
  bool ended;
  HTMLFlowAlign ff;
} HLine;

typedef struct
{
  GList lines;
  GList floaters;
  GList pending_floaters;
  GList constraints;
  unsigned int maxwidth;
  int nexty;
  int lfloaty;
  int rfloaty;
  int linecount;
  bool linebreak;
} HFlow;

typedef struct
{
  int x, y;
  unsigned int width, height;
} HCons;

/*
 *
 * Private functions
 *
 */
static void LayoutFlow _ArgProto((HTMLInfo, HTMLBox, HTMLBox));
static unsigned int WidthFlow _ArgProto((HTMLInfo, HTMLBox));
static void SetupFlow _ArgProto((HTMLInfo, HTMLBox));
static void DestroyFlow _ArgProto((HTMLInfo, HTMLBox));
static void RenderFlow _ArgProto((HTMLInfo, HTMLBox, Region));
static void SetupHLine _ArgProto((HTMLInfo, HFlow *, HLine *));
static HLine *StartLine _ArgProto((HTMLInfo, HTMLEnv, HFlow *, HTMLBox));
static void EndLine _ArgProto((HTMLInfo, HFlow *, HLine *, HTMLBox));
static void AddFloater _ArgProto((HTMLInfo, HFlow *, HTMLBox, HTMLBox)); 

/*
 * DestroyFlow
 */
static void
DestroyFlow(li, box)
HTMLInfo li;
HTMLBox box;
{
  HFlow *hf = (HFlow *)box->closure;
  HLine *hl;
  HTMLBox c;

  for (hl = (HLine *)GListGetHead(hf->lines); hl != NULL;
       hl = (HLine *)GListGetNext(hf->lines))
  {
    for (c = (HTMLBox)GListGetHead(hl->boxes); c != NULL;
	 c = (HTMLBox)GListGetNext(hl->boxes))
    {
      HTMLDestroyBox(li, c);
    }
  }

  for (c = (HTMLBox)GListGetHead(hf->floaters); c != NULL;
       c = (HTMLBox)GListGetNext(hf->floaters))
  {
    HTMLDestroyBox(li, c);
  }

  return;
}

/*
 * RenderFlow
 */
static void
RenderFlow(li, box, r)
HTMLInfo li;
HTMLBox box;
Region r;
{
  HFlow *hf = (HFlow *)box->closure;
  HLine *hl;
  HTMLBox c;
  HCons *hc;

  for (hl = (HLine *)GListGetHead(hf->lines); hl != NULL;
       hl = (HLine *)GListGetNext(hf->lines))
  {
    if (XRectInRegion(r, hl->x, hl->y,
		      hl->width, hl->height) != RectangleOut)
    {
      if (li->flowDebug)
      {
	XDrawRectangle(li->dpy, li->win, li->gc,
		       hl->x, hl->y, hl->width, hl->height);
      }
      for (c = (HTMLBox)GListGetHead(hl->boxes); c != NULL;
	   c = (HTMLBox)GListGetNext(hl->boxes))
      {
	HTMLRenderBox(li, r, c);
      }
    }
  }

  for (c = (HTMLBox)GListGetHead(hf->floaters); c != NULL;
       c = (HTMLBox)GListGetNext(hf->floaters))
  {
    if (XRectInRegion(r, c->x, c->y,
		      c->width, c->height) != RectangleOut)
    {
      HTMLRenderBox(li, r, c);
    }
  }

  if (li->constraintDebug)
  {
    for (hc = (HCons *)GListGetHead(hf->constraints); hc != NULL;
	 hc = (HCons *)GListGetNext(hf->constraints))
    {
      XDrawRectangle(li->dpy, li->win, li->gc,
		     hc->x + box->x, hc->y + box->y,
		     hc->width, hc->height);
    }
  }

  return;
}

/*
 * AddFloater
 */
static void
AddFloater(li, hf, parent, box)
HTMLInfo li;
HFlow *hf;
HTMLBox parent;
HTMLBox box;
{
  HCons *hc;

  /*
   * Determine the location of the floating box.
   * The idea here is to push the floater below any other floaters on the
   * same side of the flow box and also make sure it clears any existing
   * lines.  This is probably a bit paranoid but hopefully will prevent
   * any chance of overlap.
   */
  if (HTMLTestB(box, BOX_FLOAT_LEFT))
  {
    box->x = 0;
    box->y = hf->lfloaty;
    if (box->y < hf->nexty) box->y = hf->nexty;
    hf->lfloaty = box->y + box->height;
  }
  else
  {
    box->x = hf->maxwidth - box->width;
    if (box->x < 0) box->x = 0;
    box->y = hf->rfloaty;
    if (box->y < hf->nexty) box->y = hf->nexty;
    hf->rfloaty = box->y + box->height;
  }

  /*
   * Determine the new dimensions of the parent
   */
  if (parent->width < box->x + box->width)
  {
    parent->width = box->x + box->width;
  }
  if (parent->height < box->y + box->height)
  {
    parent->height = box->y + box->height;
  }

  /*
   * Add a new constraints box
   */
  hc = (HCons *)MPGet(li->mp, sizeof(HCons));
  if (HTMLTestB(box, BOX_FLOAT_LEFT)) hc->x = box->width;
  else hc->x = 0;
  hc->y = box->y;
  hc->width = hf->maxwidth - box->width;
  hc->height = box->height;
  GListAddHead(hf->constraints, hc);

  GListAddTail(hf->floaters, box);

  if (HTMLTestB(parent, BOX_TOPLEVEL))
  {
    box->x += parent->x;
    box->y += parent->y;
    HTMLSetupBox(li, box);
  }

  return;
}

/*
 * StartLine
 */
static HLine *
StartLine(li, env, hf, parent)
HTMLInfo li;
HTMLEnv env;
HFlow *hf;
HTMLBox parent;
{
  HLine *hl;
  HTMLBox floater;
  HCons *hc;

  while ((floater = GListPop(hf->pending_floaters)) != NULL)
  {
    AddFloater(li, hf, parent, floater);
  }

  /*
   * Add a new line to the end.
   */ 
  hl = (HLine *)MPCGet(li->mp, sizeof(HLine));
  hl->boxes = GListCreateX(li->mp);
  hl->ff = env->ff;
  GListAddTail(hf->lines, hl);

  hl->y = hf->nexty;
  hl->x = 0;
  hl->maxwidth = hf->maxwidth;

  /*
   * Determine if a constraint box has to be taken into account.
   */
  for (hc = (HCons *)GListGetHead(hf->constraints); hc != NULL;
       hc = (HCons *)GListGetNext(hf->constraints))
  {
    if (hl->y >= hc->y && hc->y + hc->height >= hl->y)
    {
      if (hc->x > hl->x) hl->x = hc->x;
      if (hc->x + hc->width < hl->x + hl->maxwidth)
      {
	/*
         * Check for extreme trouble.  It might be possible for the
	 * left edge of the line to go beyond the right side of the
	 * constraint box.  If so, try to push the line down past
	 * everything else to avoid Trouble.
         */
	if (hl->x > hc->x + hc->width)
	{
	  hl->x = 0;
	  hl->y = parent->height;
	  hl->maxwidth = hf->maxwidth;
	  break;
	}
	else hl->maxwidth = hc->x + hc->width;
      }
    }
  }
  hl->maxwidth -= hl->x;

  return(hl);
}

/*
 * EndLine
 */
static void
EndLine(li, hf, hl, parent)
HTMLInfo li;
HFlow *hf;
HLine *hl;
HTMLBox parent;
{
  myassert(!hl->ended, "Line already ended");

  hf->linecount++;
  if (hl->width + hl->x > parent->width) parent->width = hl->width + hl->x;
  hf->nexty += hl->height;
  if (hf->nexty > parent->height) parent->height = hf->nexty;
  if (HTMLTestB(parent, BOX_TOPLEVEL))
  {
    if (hf->linecount % 50 == 0)
    {
      GUISetDimensions(li->wd,
		       parent->width + li->lmargin + li->rmargin,
		       parent->height + li->tmargin + li->bmargin);
    }

    hl->x += parent->x;
    hl->y += parent->y;
    SetupHLine(li, hf, hl);
  }
  hl->ended = true;

  return;
}

/*
 * LayoutFlow
 */
static void
LayoutFlow(li, child, parent)
HTMLInfo li;
HTMLBox child;
HTMLBox parent;
{
  HFlow *hf = (HFlow *)parent->closure;
  HLine *hl;
  unsigned int temp;

  if (li->framesonly) return;
  if (HTMLTestB(parent, BOX_TOPLEVEL)) li->noframeset = true;

  /*
   * Shove the floating box in a list and deal with it just before a new
   * line is started.
   */
  if (HTMLTestB(child, BOX_FLOAT_LEFT | BOX_FLOAT_RIGHT))
  {
    GListAddTail(hf->pending_floaters, child);
    return;
  }

  /*
   * Check for clear
   */
  if (HTMLTestB(child, BOX_CLEAR_LEFT | BOX_CLEAR_RIGHT))
  {
    if (HTMLTestB(child, BOX_CLEAR_LEFT) && hf->nexty < hf->lfloaty)
    {
      hf->nexty = hf->lfloaty;
    }
    if (HTMLTestB(child, BOX_CLEAR_RIGHT) && hf->nexty < hf->rfloaty)
    {
      hf->nexty = hf->rfloaty;
    }
    hf->linebreak = true;
    return;
  }

  /*
   * Check for new line.
   */
  if ((hl = (HLine *)GListGetTail(hf->lines)) == NULL)
  {
    if (HTMLTestB(child, BOX_LINEBREAK | BOX_SPACE)) return;
    hl = StartLine(li, child->env, hf, parent);
  }
  else
  {
    if (HTMLTestB(child, BOX_LINEBREAK))
    {
      hf->linebreak = true;
      return;
    }
    if (hf->linebreak ||
	(child->width + hl->width > hl->maxwidth && !GListEmpty(hl->boxes)))
    {
      hf->linebreak = false;
      if (!hl->ended) EndLine(li, hf, hl, parent);
      hl = StartLine(li, child->env, hf, parent);
      if (HTMLTestB(child, BOX_SPACE)) return;
    }
    else hf->linebreak = false;
  }

  /*
   * Figure out how the new box changes the baseline offset (from
   * the top) and the bottom;
   */
  if (HTMLTestB(child, BOX_VCENTER))
  {
    temp = child->height / 2;
    if (temp > hl->bloff) hl->bloff = temp;
    if (temp > hl->bottom) hl->bottom = temp;
  }
  else
  {
    if (child->baseline > hl->bloff) hl->bloff = child->baseline;
    temp = child->height - child->baseline;
    if (temp > hl->bottom) hl->bottom = temp;
  }

  hl->width += child->width;
  hl->height = hl->bloff + hl->bottom;

  GListAddTail(hl->boxes, child);

  return;
}

/*
 * SetupHLine
 */
static void
SetupHLine(li, hf, hl)
HTMLInfo li;
HFlow *hf;
HLine *hl;
{
  HTMLBox c;
  int x = hl->x;

  if (hl->ff & FLOW_CENTER_JUSTIFY)
  {
    if (hl->width < hl->maxwidth) x += hl->maxwidth / 2 - hl->width / 2;
  }
  else if (hl->ff & FLOW_RIGHT_JUSTIFY)
  {
    if (hl->width < hl->maxwidth) x += hl->maxwidth - hl->width;
  }

  for (c = (HTMLBox)GListGetHead(hl->boxes); c != NULL;
       c = (HTMLBox)GListGetNext(hl->boxes))
  {
    c->x = x;
    c->y = hl->y;
    HTMLSetupBox(li, c);
    x += c->width;

    HTMLRenderBox(li, NULL, c);
  }

  return;
}

/*
 * SetupFlow
 */
static void
SetupFlow(li, box)
HTMLInfo li;
HTMLBox box;
{
  HFlow *hf = (HFlow *)box->closure;
  HLine *hl;
  HTMLBox c;

  if (HTMLTestB(box, BOX_TOPLEVEL)) return;

  for (c = (HTMLBox)GListGetHead(hf->floaters); c != NULL;
       c = (HTMLBox)GListGetNext(hf->floaters))
  {
    c->x += box->x;
    c->y += box->y;
    HTMLSetupBox(li, c);
  }

  for (hl = (HLine *)GListGetHead(hf->lines); hl != NULL;
       hl = (HLine *)GListGetNext(hf->lines))
  {
    hl->x += box->x;
    hl->y += box->y;
    SetupHLine(li, hf, hl);
  }

  return;
}

/*
 * WidthFlow
 */
static unsigned int
WidthFlow(li, box)
HTMLInfo li;
HTMLBox box;
{
  HFlow *hf = (HFlow *)box->closure;
  HLine *hl;

  if ((hl = (HLine *)GListGetTail(hf->lines)) == NULL || hf->linebreak)
  {
    return(hf->maxwidth);
  }
  return(hl->maxwidth - hl->width);
}

/*
 *
 * Public functions
 *
 */

/*
 * HTMLCreateFlowBox
 */
HTMLBox
HTMLCreateFlowBox(li, env, maxwidth)
HTMLInfo li;
HTMLEnv env;
unsigned int maxwidth;
{
  HTMLBox box;
  HFlow *hf;

  hf = (HFlow *)MPCGet(li->mp, sizeof(HFlow));
  hf->lines = GListCreateX(li->mp);
  hf->floaters = GListCreateX(li->mp);
  hf->pending_floaters = GListCreateX(li->mp);
  hf->constraints = GListCreateX(li->mp);

  /* woo hoo! */
  if (maxwidth == 0) hf->maxwidth = 64000;
  else hf->maxwidth = maxwidth;

  box = HTMLCreateBox(li, env);
  box->setup = SetupFlow;
  box->render = RenderFlow;
  box->destroy = DestroyFlow;
  box->maxwidth = WidthFlow;
  box->layout = LayoutFlow;
  box->closure = hf;

  return(box);
}

/*
 * HTMLFinishFlowBox
 */
void
HTMLFinishFlowBox(li, box)
HTMLInfo li;
HTMLBox box;
{
  HFlow *hf = (HFlow *)box->closure;
  HLine *hl;
  HTMLBox child;

  if (HTMLTestB(box, BOX_TOPLEVEL) && !li->noframeset) return;

  if ((hl = (HLine *)GListGetTail(hf->lines)) != NULL)
  {
    if (!hl->ended) EndLine(li, hf, hl, box);
  }

  while ((child = (HTMLBox)GListPop(hf->pending_floaters)) != NULL)
  {
    AddFloater(li, hf, box, child);
  }

  if (HTMLTestB(box, BOX_TOPLEVEL))
  {
    GUISetDimensions(li->wd,
		     box->width + li->lmargin + li->rmargin,
		     box->height + li->tmargin + li->bmargin);

    HTMLSetFinalPosition(li);
  }
  
  return;
}
