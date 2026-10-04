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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
void GUIAddRender(ChimeraGUI wd, ChimeraRender wn) {
  wd->wn = wn;
  return;
}

/*
 * GUICreate
 */
ChimeraGUI GUICreate(ChimeraContext wc, ChimeraGUI parent,
	GUISizeCallback size_callback, void *size_closure) {
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
void GUIDestroy(ChimeraGUI wd) {
  MemPool mp = wd->mp;

  if (wd->www != NULL) XtDestroyWidget(wd->www);
  if (wd->resize_task != NULL) TaskRemove(wd->wc->cres, wd->resize_task);
  memset(wd, 0, sizeof(struct ChimeraGUIP));
  MPDestroy(mp);
}

/*
 * GUIToWindow
 */
Window GUIToWindow(ChimeraGUI wd) {
  return(XtWindow(WWWGetDrawWidget(wd->www)));
}

/*
 * GUIToDisplay
 */
Display *GUIToDisplay(ChimeraGUI wd) {
  return(XtDisplay(wd->www));
}

/*
 * GUICreateToplevel
 */
ChimeraGUI GUICreateToplevel(ChimeraContext wc, Widget parent,
	GUISizeCallback size_callback, void *size_closure) {
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
	  wwwWidgetClass, parent, XtNwidth, wd->wc->athena->width, XtNheight, wd->wc->athena->height, NULL);

  WWWSetResizeCallback(wd->www, GUIToplevelResize, wd);
  WWWSetSelectCallback(wd->www, GUISelect, wd);
  WWWSetMotionCallback(wd->www, GUIMotion, wd);
  WWWSetExposeCallback(wd->www, GUIExpose, wd);

  return(wd);
}

/*
 * GUISetScrollBar
 */
void GUISetScrollBar(ChimeraGUI wd, bool use_scroll) {
  WWWSetScrollBar(wd->www, use_scroll);
}

/*
 * GUIGetDimenions
 */
int GUIGetDimensions(ChimeraGUI wd, unsigned int *width, unsigned int *height) {
  if (!wd->size_set) return(-1);
  WWWGetDrawSize(wd->www, width, height);
  return(0);
}

/*
 * GUISetDimensions
 */
void GUISetDimensions(ChimeraGUI wd, unsigned int width, unsigned int height) {
  WWWSetDrawSize(wd->www, width, height);
}

int GUIGetNamedColor(ChimeraGUI wd, char *name, Pixel *_pixel) {
  XColor sxc, exc;
  Display *dpy = XtDisplay(wd->www);

  XAllocNamedColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)),
                   name, &sxc, &exc);

  *_pixel = sxc.pixel;

  return(0);
}

/*
 * GUIGetOnScreenDimensions
 */
void GUIGetOnScreenDimensions(ChimeraGUI wd, int *x, int *y, unsigned int *width, unsigned int *height) {
  WWWWidget rw = (WWWWidget)wd->www;

  *x = -(int)rw->www.child->core.x;
  *y = -(int)rw->www.child->core.y;
  *width = (unsigned int)rw->www.child->core.width;
  *height = (unsigned int)rw->www.clip->core.height;
}

/*
 * GUIReset
 */
void GUIReset(ChimeraGUI wd) {
  WWWWidget rw = (WWWWidget)wd->www;

  wd->wn = NULL;
  XClearWindow(XtDisplay(rw->www.child), XtWindow(rw->www.child));
  WWWMoveChild(wd->www, 0, 0);
  WWWSetDrawSize(wd->www, 0, 0);
}

/*
 * GUIExpose
 */
static void GUIExpose(Widget w, void *closure, int x, int y,
	unsigned int width, unsigned int  height) {
	ChimeraGUI wd = (ChimeraGUI)closure;

  if (wd->wn != NULL) RenderExpose(wd->wn, x, y, width, height);

  return;
}

/*
 * GUISelect
 */
static void GUISelect(Widget w, void *closure, int x, int y, int button) {
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
static void GUIMotion(Widget w, void *closure, int x, int y, int button) {
  ChimeraGUI wd = (ChimeraGUI)closure;

  if (wd->wn != NULL) RenderMotion(wd->wn, x, y);

  return;
}

/*
 * GUIToplevelResize
 */
void GUIToplevelResize(Widget w, void *closure, void *junk) {
  ChimeraGUI wd = (ChimeraGUI)closure;

  if (wd->size_callback != NULL)
  {
    Dimension width, height;

    XtVaGetValues(wd->www, XtNwidth, &width, XtNheight, &height, NULL);
    wd->width = width;
    wd->height = height;
    CMethod(wd->size_callback)(wd, wd->size_closure, wd->width, wd->height);
  }
}

/*
 * GUIMap
 */
void GUIMap(ChimeraGUI wd, int x, int y) {
  if (!wd->size_set)   {
    fprintf (stderr, "GUIMap: GUI dimensions not set yet.\n");
    return;
  }

  if (wd->position_set) {
    fprintf (stderr, "GUIMap: already mapped.\n");
    return;
  }

  XtConfigureWidget(wd->www, (Position)x, (Position)y,
		    (Dimension)wd->width, (Dimension)wd->height, 0);

  XtSetMappedWhenManaged(wd->www, True);
}

/*
 * GUIUnmap
 */
void GUIUnmap(ChimeraGUI wd) {
  XtSetMappedWhenManaged(wd->www, False);
  wd->size_set = false;
  wd->position_set = false;
}

/*
 * ResizeTask
 */
static void ResizeTask(void *closure) {
  ChimeraGUI wd = (ChimeraGUI)closure;

  if (wd->size_callback != NULL)
  {
    wd->resize_task = NULL;
    CMethod(wd->size_callback)(wd, wd->size_closure, wd->width, wd->height);
  }
}

/*
 * GUISetInitialDimensions
 */
void GUISetInitialDimensions(ChimeraGUI wd, unsigned int width, unsigned int height) {
  myassert(!wd->size_set, "GUISetInitialDimensions: dimensions already set");

  if (wd->resize_task != NULL) TaskRemove(wd->wc->cres, wd->resize_task);

  wd->width = width;
  wd->height = height;
  wd->size_set = true;

  XtResizeWidget(wd->www, (Dimension)width, (Dimension)height, 0);
  WWWSetDrawSize(wd->www, width, height);

  wd->resize_task = TaskSchedule(wd->wc->cres, ResizeTask, wd);
}

/*
 * GUISetScrollPosition
 */
void GUISetScrollPosition(ChimeraGUI wd, int x, int y) {
  WWWMoveChild(wd->www, x, y);
}

/*
 * GUIGetScrollPosition
 */
void GUIGetScrollPosition(ChimeraGUI wd, int *x, int *y) {
  WWWGetScrollPosition(wd->www, x, y);
}

/*
 * GUIBackgroundPixel
 */
Pixel GUIBackgroundPixel(ChimeraGUI wd) {
  Pixel bg;
  XtVaGetValues(wd->www, XtNbackground, &bg, NULL);
  return(bg);
}

/*
 * GUIToWidget
 */
Widget GUIToWidget(ChimeraGUI wd) {
  return(WWWGetDrawWidget(wd->www));
}
