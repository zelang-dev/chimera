/*
 * gui.c
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

#include "WWWP.h"

struct ChimeraGUIP
{
  Widget             www;                    /* Xt widget */
  MemPool            mp;                     /* memory pool */
  ChimeraContext     wc;
  ChimeraRender      wn;

  /* state flags */
  bool               position_set;

  bool               size_set;
  unsigned int       width, height;

  GUISizeCallback    size_callback;
  void               *size_closure;

  ChimeraTask        resize_task;
};

static void ResizeTask _ArgProto((void *));

void GUIToplevelResize _ArgProto((Widget, void *, void *));
static void GUIExpose _ArgProto((Widget, void *, int, int,
				 unsigned int, unsigned int));
static void GUIMotion _ArgProto((Widget, void *, int, int, int));
static void GUISelect _ArgProto((Widget, void *, int, int, int));

/*
 * GUIAddRender
 */
void
GUIAddRender(wd, wn)
ChimeraGUI wd;
ChimeraRender wn;
{
  wd->wn = wn;
  return;
}

/*
 * GUICreate
 */
ChimeraGUI
GUICreate(wc, parent, size_callback, size_closure)
ChimeraContext wc;
ChimeraGUI parent;
GUISizeCallback size_callback;
void *size_closure;
{
  ChimeraGUI wd;
  MemPool mp;

  myassert(parent != NULL, "NULL parent not allowed");

  mp = MPCreate();
  wd = (ChimeraGUI)MPCGet(mp, sizeof(struct ChimeraGUIP));
  wd->mp = mp;
  wd->wc = wc;
  wd->size_set = false;
  wd->position_set = false;
  wd->size_callback = size_callback;
  wd->size_closure = size_closure;

  wd->www = XtVaCreateManagedWidget("www_child",
				    wwwWidgetClass,
				    WWWGetDrawWidget(parent->www),
				    XtNmappedWhenManaged, False,
				    NULL);

  WWWSetSelectCallback(wd->www, GUISelect, wd);
  WWWSetMotionCallback(wd->www, GUIMotion, wd);
  WWWSetExposeCallback(wd->www, GUIExpose, wd);

  return(wd);
}

/*
 * GUIDestroy
 */
void
GUIDestroy(wd)
ChimeraGUI wd;
{
  MemPool mp = wd->mp;

  if (wd->www != NULL) XtDestroyWidget(wd->www);
  if (wd->resize_task != NULL) TaskRemove(wd->wc->cres, wd->resize_task);
  memset(wd, 0, sizeof(struct ChimeraGUIP));
  MPDestroy(mp);

  return;
}

/*
 * GUIToWindow
 */
Window
GUIToWindow(wd)
ChimeraGUI wd;
{
  return(XtWindow(WWWGetDrawWidget(wd->www)));
}

/*
 * GUIToDisplay
 */
Display *
GUIToDisplay(wd)
ChimeraGUI wd;
{
  return(XtDisplay(wd->www));
}

/*
 * GUICreateToplevel
 */
ChimeraGUI
GUICreateToplevel(wc, parent, size_callback, size_closure)
ChimeraContext wc;
Widget parent;
GUISizeCallback size_callback;
void *size_closure;
{
  ChimeraGUI wd;
  MemPool mp;

  mp = MPCreate();
  wd = (ChimeraGUI)MPCGet(mp, sizeof(struct ChimeraGUIP));
  wd->mp = mp;
  wd->wc = wc;
  wd->size_set = true;
  wd->position_set = true;
  wd->size_callback = size_callback;
  wd->size_closure = size_closure;

  wd->www = XtVaCreateManagedWidget("www_toplevel",
				    wwwWidgetClass, parent,
				    NULL);

  WWWSetResizeCallback(wd->www, GUIToplevelResize, wd);
  WWWSetSelectCallback(wd->www, GUISelect, wd);
  WWWSetMotionCallback(wd->www, GUIMotion, wd);
  WWWSetExposeCallback(wd->www, GUIExpose, wd);

  return(wd);
}

/*
 * GUISetScrollBar
 */
void
GUISetScrollBar(wd, use_scroll)
ChimeraGUI wd;
bool use_scroll;
{
  WWWSetScrollBar(wd->www, use_scroll);
  return;
}

/*
 * GUIGetDimenions
 */
int
GUIGetDimensions(wd, width, height)
ChimeraGUI wd;
unsigned int *width, *height;
{
  if (!wd->size_set) return(-1);
  WWWGetDrawSize(wd->www, width, height);     
  return(0);
}

/*
 * GUISetDimensions
 */
void
GUISetDimensions(wd, width, height)
ChimeraGUI wd;
unsigned int width, height;
{
  WWWSetDrawSize(wd->www, width, height);
  return;
}

int
GUIGetNamedColor(wd, name, pixel)
ChimeraGUI wd;
char *name;
Pixel *pixel;
{
  XColor sxc, exc;
  Display *dpy = XtDisplay(wd->www);

  XAllocNamedColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)),
                   name, &sxc, &exc);

  *pixel = sxc.pixel;

  return(0);
}

/*
 * GUIGetOnScreenDimensions
 */
void
GUIGetOnScreenDimensions(wd, x, y, width, height)
ChimeraGUI wd;
int *x, *y;
unsigned int *width, *height;
{
  WWWWidget rw = (WWWWidget)wd->www;
  
  *x = -(int)rw->www.child->core.x;
  *y = -(int)rw->www.child->core.y;
  *width = (unsigned int)rw->www.child->core.width;
  *height = (unsigned int)rw->www.clip->core.height;
    
  return;
}     

/*
 * GUIReset
 */
void
GUIReset(wd)
ChimeraGUI wd;
{
  WWWWidget rw = (WWWWidget)wd->www;

  wd->wn = NULL;
  XClearWindow(XtDisplay(rw->www.child), XtWindow(rw->www.child));
  WWWMoveChild(wd->www, 0, 0);
  WWWSetDrawSize(wd->www, 0, 0);

  return;
}

/*
 * GUIExpose
 */
static void
GUIExpose(w, closure, x, y, width, height)
Widget w;
void *closure;
int x, y;
unsigned int width, height;
{
  ChimeraGUI wd = (ChimeraGUI)closure;

  if (wd->wn != NULL) RenderExpose(wd->wn, x, y, width, height);

  return;
}

/*
 * GUISelect
 */
static void
GUISelect(w, closure, x, y, button)
Widget w;
void *closure;
int x, y;
int button;
{
  ChimeraGUI wd = (ChimeraGUI)closure;
  char *action;

  if (wd->wn != NULL)
  {
    if (button == 1) action = "open";
    else if (button == 2) action = "download";
    else if (button == 3) action = "external";
    else action = "open";

    RenderSelect(wd->wn, x, y, action);
  }

  return;
}

/*
 * GUIMotion
 */
static void
GUIMotion(w, closure, x, y, button)
Widget w;
void *closure;
int x, y;
int button;
{
  ChimeraGUI wd = (ChimeraGUI)closure;

  if (wd->wn != NULL) RenderMotion(wd->wn, x, y);

  return;
}

/*
 * GUIToplevelResize
 */
void
GUIToplevelResize(w, closure, junk)
Widget w;
void *closure;
void *junk;
{
  ChimeraGUI wd = (ChimeraGUI)closure;

  if (wd->size_callback != NULL)
  {
    Dimension width, height;

    XtVaGetValues(wd->www, XtNwidth, &width, XtNheight, &height, NULL);
    wd->width = width;
    wd->height = height;
    CMethod(wd->size_callback)(wd, wd->size_closure, wd->width, wd->height);
  }

  return;
}

/*
 * GUIMap
 */
void
GUIMap(wd, x, y)
ChimeraGUI wd;
int x, y;
{
  if (!wd->size_set)
  {
    fprintf (stderr, "GUIMap: GUI dimensions not set yet.\n");
    return;
  }
  if (wd->position_set)
  {
    fprintf (stderr, "GUIMap: already mapped.\n");
    return;
  }

  XtConfigureWidget(wd->www, (Position)x, (Position)y,
		    (Dimension)wd->width, (Dimension)wd->height, 0);

  XtSetMappedWhenManaged(wd->www, True);

  return;
}

/*
 * GUIUnmap
 */
void
GUIUnmap(wd)
ChimeraGUI wd;
{
  XtSetMappedWhenManaged(wd->www, False);
  wd->size_set = false;
  wd->position_set = false;
  return;
}

/*
 * ResizeTask
 */
static void
ResizeTask(closure)
void *closure;
{
  ChimeraGUI wd = (ChimeraGUI)closure;

  if (wd->size_callback != NULL)
  {
    wd->resize_task = NULL;
    CMethod(wd->size_callback)(wd, wd->size_closure, wd->width, wd->height);
  }

  return;
}

/*
 * GUISetInitialDimensions
 */
void
GUISetInitialDimensions(wd, width, height)
ChimeraGUI wd;
unsigned int width, height;
{
  myassert(!wd->size_set, "GUISetInitialDimensions: dimensions already set");

  if (wd->resize_task != NULL) TaskRemove(wd->wc->cres, wd->resize_task);

  wd->width = width;
  wd->height = height;
  wd->size_set = true;

  XtResizeWidget(wd->www, (Dimension)width, (Dimension)height, 0);
  WWWSetDrawSize(wd->www, width, height);

  wd->resize_task = TaskSchedule(wd->wc->cres, ResizeTask, wd);

  return;
}

/*
 * GUISetScrollPosition
 */
void
GUISetScrollPosition(wd, x, y)
ChimeraGUI wd;
int x, y;
{
  WWWMoveChild(wd->www, x, y);
  return;
}

/*
 * GUIGetScrollPosition
 */
void
GUIGetScrollPosition(wd, x, y)
ChimeraGUI wd;
int *x, *y;
{
  WWWGetScrollPosition(wd->www, x, y);
  return;
}

/*
 * GUIBackgroundPixel
 */ 
Pixel
GUIBackgroundPixel(wd)
ChimeraGUI wd;
{
  Pixel bg;
  XtVaGetValues(wd->www, XtNbackground, &bg, NULL);
  return(bg);
}

/*
 * GUIToWidget
 */
Widget
GUIToWidget(wd)
ChimeraGUI wd;
{
  return(WWWGetDrawWidget(wd->www));
}

