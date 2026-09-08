/*
 * hr.c
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

#define HR_HEIGHT 20
#define HR_HALF_HEIGHT 10

/*
 *
 * Private functions
 *
 */

static void RenderHR _ArgProto((HTMLInfo, HTMLBox, Region));

/*
 * RenderHR
 */
static void
RenderHR(li, box, r)
HTMLInfo li;
HTMLBox box;
Region r;
{
  XSetLineAttributes(li->dpy, li->gc,
		     1, LineSolid, CapNotLast, JoinRound);

  XDrawLine(li->dpy, li->win, li->gc,
	    box->x, box->y + HR_HALF_HEIGHT,
	    box->x + box->width, box->y + HR_HALF_HEIGHT);

  return;
}

/*
 *
 * Public Functions
 *
 */

/*
 * HandleHR
 */
void
HTMLHorizontalRule(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLBox box;

  HTMLAddLineBreak(li, env);

  box = HTMLCreateBox(li, env);
  box->render = RenderHR;
  box->width = HTMLGetMaxWidth(li, env);
  box->height = HR_HEIGHT;
  HTMLEnvAddBox(li, env, box);

  HTMLAddLineBreak(li, env);

  return;
}
