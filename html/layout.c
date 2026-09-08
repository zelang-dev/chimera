/*
 * layout.c
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
#include <ctype.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "port_after.h"

#include "html.h"

/*
 * HTMLDestroyBox
 */
void
HTMLDestroyBox(li, box)
HTMLInfo li;
HTMLBox box;
{
  if (box->destroy != NULL) (box->destroy)(li, box);
  return;
}

/*
 * HTMLCreateBox
 */
HTMLBox
HTMLCreateBox(li, env)
HTMLInfo li;
HTMLEnv env;
{
  HTMLBox box;
  box = (HTMLBox)MPCGet(li->mp, sizeof(struct HTMLBoxP));
  box->env = env;
  return(box);
}

/*
 * HTMLRenderBox
 */
void
HTMLRenderBox(li, r, box)
HTMLInfo li;
Region r;
HTMLBox box;
{
  Region xr;
  int x, y;
  unsigned int width, height;
  XRectangle rec;

  if (box->render == NULL) return;

  if (r == NULL)
  {
    GUIGetOnScreenDimensions(li->wd, &x, &y, &width, &height);
    
    r = XCreateRegion();
    rec.x = (short)x;
    rec.y = (short)y;
    rec.width = (unsigned short)width;
    rec.height = (unsigned short)height;
    XUnionRectWithRegion(&rec, r, r);
    xr = r;
  }
  else xr = NULL;

  if (XRectInRegion(r, box->x, box->y, box->width, box->height))
  {
    (box->render)(li, box, r);
  }

  if (xr != NULL) XDestroyRegion(xr);

  return;
}

/*
 * HTMLLayoutBox
 */
void
HTMLLayoutBox(li, parent, box)
HTMLInfo li;
HTMLBox parent;
HTMLBox box;
{
  myassert(parent->layout != NULL, "Box cannot contain other boxes");
  (parent->layout)(li, box, parent);
  return;
}

/*
 * HTMLSetupBox
 */
void
HTMLSetupBox(li, box)
HTMLInfo li;
HTMLBox box;
{
  HTMLEnv env = box->env;

  if (env->anchor != NULL) HTMLAddAnchor(li, box, NULL, env->anchor);
  if (box->setup != NULL) (box->setup)(li, box);

  return;
}

/*
 * HTMLGetBoxWidth
 */
unsigned int
HTMLGetBoxWidth(li, box)
HTMLInfo li;
HTMLBox box;
{
  myassert(box->maxwidth != NULL, "Box cannot contain other boxes");
  return((box->maxwidth)(li, box));
}
