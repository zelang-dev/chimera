/*
 * WWW.c
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
 *

Some of the code was borrowed from the Viewport widget:

Copyright (c) 1989, 1994  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.

 */

#include "port_before.h"

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Scrollbar.h>

#include <X11/keysym.h>

#include "port_after.h"

#include "WWWP.h"
#include "Geom.h"

#define WMethod(x) ((x) != NULL ? (x):10)

/*
 * Action prototypes
 */
static void WWWMotionAction _ArgProto((Widget, XEvent *,
				       String *, Cardinal *));
static void WWWSelectAction _ArgProto((Widget, XEvent *,
				       String *, Cardinal *));
static void WWWExposeAction _ArgProto((Widget, XEvent *,
				       String *, Cardinal *));
static void WWWPageUpAction    _ArgProto((Widget, XEvent *,
                                          String *, Cardinal *));
static void WWWPageDownAction  _ArgProto((Widget, XEvent *,
                                          String *, Cardinal *));
static void WWWPageLeftAction  _ArgProto((Widget, XEvent *,
                                          String *, Cardinal *));
static void WWWPageRightAction _ArgProto((Widget, XEvent *,
                                          String *, Cardinal *));
static void WWWScrollUpAction    _ArgProto((Widget, XEvent *,
                                            String *, Cardinal *));
static void WWWScrollDownAction  _ArgProto((Widget, XEvent *,
                                            String *, Cardinal *));
static void WWWScrollLeftAction  _ArgProto((Widget, XEvent *,
                                            String *, Cardinal *));
static void WWWScrollRightAction _ArgProto((Widget, XEvent *,
                                            String *, Cardinal *));

static char defaultTranslations[] =
"<Btn1Up>:      select() \n\
 <Btn2Up>:      select() \n\
 <Btn3Up>:      select() \n\
 <Expose>:      expose() \n\
 <Motion>:      motion() \n\
 :<Key>BackSpace: page_up()\n\
 :<Key>Page_Up: page_up()\n\
 :<Key>KP_Page_Up: page_up()\n\
 :<Key>0x20: page_down()\n\
 :<Key>Page_Down: page_down()\n\
 :<Key>KP_Page_Down: page_down()\n\
 :s<Key>Up: page_up()\n\
 :s<Key>KP_Up: page_up()\n\
 :s<Key>Down: page_down()\n\
 :s<Key>KP_Down: page_down()\n\
 :s<Key>Left: page_left()\n\
 :s<Key>KP_Left: page_left()\n\
 :s<Key>Right: page_right()\n\
 :s<Key>KP_Right: page_right()\n\
 :<Key>Up: scroll_up()\n\
 :<Key>KP_Up: scroll_up()\n\
 :<Key>Down: scroll_down()\n\
 :<Key>KP_Down: scroll_down()\n\
 :<Key>Left: scroll_left()\n\
 :<Key>KP_Left: scroll_left()\n\
 :<Key>Right: scroll_right()\n\
 :<Key>KP_Right: scroll_right()\n\
";

static XtActionsRec actionsList[] =
{
  { "expose", WWWExposeAction },
  { "select", WWWSelectAction },
  { "motion", WWWMotionAction },
  { "page_up",    WWWPageUpAction   },
  { "page_down",  WWWPageDownAction },
  { "page_left",  WWWPageLeftAction },
  { "page_right", WWWPageRightAction},
  { "scroll_up",    WWWScrollUpAction   },
  { "scroll_down",  WWWScrollDownAction },
  { "scroll_left",  WWWScrollLeftAction },
  { "scroll_right", WWWScrollRightAction},
};

#define offset(field) XtOffsetOf(WWWRec, www.field)
static XtResource resources[] =
{
  { XtNbackground, XtCBackground, XtRPixel, sizeof(Pixel),
	offset(background), XtRString, (XtPointer)"moccasin" },

  { XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
	offset(foreground), XtRString, (XtPointer)"black" },
};
#undef offset

static void WWWInitialize _ArgProto((Widget, Widget, ArgList, Cardinal *));
void WWWResize _ArgProto((Widget));
static void WWWRealize _ArgProto((Widget, XtValueMask *,
				  XSetWindowAttributes *));
static Boolean WWWSetValues _ArgProto((Widget, Widget, Widget,
				       ArgList, Cardinal *));
static XtGeometryResult WWWGeometryManager _ArgProto((Widget,
						      XtWidgetGeometry *,
						      XtWidgetGeometry *));
static XtGeometryResult WWWPreferredGeometry _ArgProto((Widget,
							XtWidgetGeometry *,
							XtWidgetGeometry *));

static void WWWGetValuesHook _ArgProto((Widget, ArgList, Cardinal *));

static Dimension FixDim _ArgProto((unsigned int));
static void ConfigureWidgets _ArgProto((WWWWidget,
					unsigned int, unsigned int));

/* Scrollbar handling functions */
static void RedrawThumbs _ArgProto((WWWWidget));
static void ScrollUpDownProc _ArgProto((Widget, XtPointer, XtPointer));
static void ThumbProc _ArgProto((Widget, XtPointer, XtPointer));

WWWClassRec wwwClassRec =
{
  { /* core_class fields */
    /* superclass         */    (WidgetClass) &compositeClassRec,
    /* class_name         */    "WWW",
    /* widget_size        */    sizeof(WWWRec),
    /* class_initialize   */    NULL,
    /* class_part_init    */    NULL,
    /* class_inited       */    FALSE,
    /* initialize         */    WWWInitialize,
    /* initialize_hook    */    NULL,
    /* realize            */    WWWRealize,
    /* actions            */    actionsList,
    /* num_actions        */    XtNumber(actionsList),
    /* resources          */    resources,
    /* num_resources      */    XtNumber(resources),
    /* xrm_class          */    NULLQUARK,
    /* compress_motion    */    FALSE,
    /* compress_exposure  */    FALSE,
    /* compress_enterleave*/    FALSE,
    /* visible_interest   */    FALSE,
    /* destroy            */    NULL,
    /* resize             */    WWWResize,
    /* expose             */    NULL,
    /* set_values         */    WWWSetValues,
    /* set_values_hook    */    NULL,
    /* set_values_almost  */    XtInheritSetValuesAlmost,
    /* get_values_hook    */    WWWGetValuesHook,
    /* accept_focus       */    NULL,
    /* version            */    XtVersion,
    /* callback_private   */    NULL,
    /* tm_table           */    NULL,
    /* query_geometry     */	WWWPreferredGeometry,
    /* display_accelerator*/	XtInheritDisplayAccelerator,
    /* extension          */	NULL
  },
  { /* composite_class fields */
    /* geometry_manager   */   WWWGeometryManager,
    /* change_managed     */   NULL,
    /* insert_child       */   XtInheritInsertChild,
    /* delete_child       */   XtInheritDeleteChild,
    /* extension          */   NULL
  },
  { /* www_class fields */
    NULL,
  }
};

WidgetClass wwwWidgetClass = (WidgetClass)&wwwClassRec;

/*
 * WWWGetValuesHook
 */
static void
WWWGetValuesHook(w, args, num_args)
Widget w;
ArgList args;
Cardinal * num_args;
{
  WWWWidget rw = (WWWWidget)w;
  int i;

  for (i = 0; i < *num_args; i++)
  {
    if (strcmp(args[i].name, XtNbackground) == 0)
    {
      *((Pixel *)args[i].value) = rw->www.background;
    }
    if (strcmp(args[i].name, XtNforeground) == 0)
    {
      *((Pixel *)args[i].value) = rw->www.foreground;
    }
  }

  return;
}

/*
 * WWWResize
 */
void
WWWResize(w)
Widget w;
{
  WWWWidget rw;

  if (XtClass(w) != wwwWidgetClass) return;

  rw = (WWWWidget)w;
  if (rw->www.resize_callback != NULL)
  {
    (rw->www.resize_callback)(w, rw->www.resize_closure, NULL);
  }

  return;
}

/*
 * WWWRealize
 */
static void
WWWRealize(w, valueMask, attributes)
Widget w;
XtValueMask *valueMask;
XSetWindowAttributes *attributes;
{
  (*wwwWidgetClass->core_class.superclass->core_class.realize)
      (w, valueMask, attributes);

  return;
}

static XtGeometryResult
WWWPreferredGeometry(w, con, reply)
Widget w;
XtWidgetGeometry *con, *reply;
{
  WWWWidget rw = (WWWWidget)w;

  XtResizeWidget(rw->www.clip, con->width > 0 ? con->width:100,
		 con->height > 0 ? con->height:100, 0);

  return(XtGeometryYes);
}

/*
 * WWWGeometryManager
 */
static XtGeometryResult
WWWGeometryManager(w, request, reply)
Widget w;
XtWidgetGeometry *request;
XtWidgetGeometry *reply;
{ 
  return(XtGeometryNo);
}

/*
 * WWWSetValues
 */
Boolean
WWWSetValues(c, r, n, args, count)
Widget c, r, n;
ArgList args;
Cardinal *count;
{
  return(False);
}

static void
RedrawThumbs(rw)
WWWWidget rw;
{
  Widget child = rw->www.child;
  Widget clip = rw->www.clip;
  Dimension length, total;
  Position top;

  if (rw->www.horiz_bar != (Widget)NULL)
  {
    top = -(child->core.x);
    length = clip->core.width;
    total = child->core.width;

    XawScrollbarSetThumb(rw->www.horiz_bar,
			 (float)top/(float)total,
			 (float)length/(float)total);
  }
  
  if (rw->www.vert_bar != (Widget)NULL)
  {
    top = -(child->core.y);
    length = clip->core.height;
    total = child->core.height;

    XawScrollbarSetThumb(rw->www.vert_bar,
			 (float)top/(float)total,
			 (float)length/(float)total);
  }

  return;
}

void
WWWMoveChild(w, x, y)
Widget w;
int x, y;
{
  WWWWidget rw = (WWWWidget)w;
  register Widget child = rw->www.child;
  register Widget clip = rw->www.clip;

  if (-x + (int)clip->core.width > (int)child->core.width)
  {
    x = -(child->core.width - clip->core.width);
  }
  
  if (-y + (int)clip->core.height > (int)child->core.height)
  {
    y = -(child->core.height - clip->core.height);
  }
  
  if (x >= 0) x = 0;
  if (y >= 0) y = 0;

  /* Mmmm hairy cast */
  XtMoveWidget(child, (Position)x, (Position)y);
  
  RedrawThumbs(rw);

  return;
}

static void
ScrollUpDownProc(widget, closure, call_data)
Widget widget;
XtPointer closure;
XtPointer call_data;
{
  WWWWidget rw = (WWWWidget)XtParent(widget);
  register Widget child = rw->www.child;
  int pix = (int)call_data;
  Position x, y;
  
  if (child == NULL) return;  /* no child to scroll. */
  
  x = child->core.x - ((widget == rw->www.horiz_bar) ? pix : 0);
  y = child->core.y - ((widget == rw->www.vert_bar) ? pix : 0);
  WWWMoveChild((Widget)rw, (int)x, (int)y);

  return;
}

static void
ThumbProc(widget, closure, call_data)
Widget widget;
XtPointer closure;
XtPointer call_data;
{
  WWWWidget rw = (WWWWidget)XtParent(widget);
  register Widget child = rw->www.child;
  Position x, y;
  float *percent = (float *)call_data;
  
  if (child == NULL) return;  /* no child to scroll. */
  
  if (widget == rw->www.horiz_bar) x = -(int)(*percent * child->core.width);
  else x = child->core.x;

  if (widget == rw->www.vert_bar) y = -(int)(*percent * child->core.height);
  else y = child->core.y;

  WWWMoveChild((Widget)rw, (int)x, (int)y);

  return;
}

/*
 * WWWInitialize
 */
static void
WWWInitialize(r, n, args, count)
Widget r, n;
ArgList args;
Cardinal *count;
{
  WWWWidget new = (WWWWidget)n;

  /*
   * Avoid zero.
   */
  if (new->core.width < 5) new->core.width = 5;
  if (new->core.height < 5) new->core.height = 5;

  new->www.use_scroll = false;

  new->www.resize_closure = NULL;
  new->www.resize_callback = NULL;
  new->www.motion_callback = NULL;
  new->www.motion_closure = NULL;
  new->www.select_callback = NULL;
  new->www.select_closure = NULL;
  new->www.expose_callback = NULL;
  new->www.expose_closure = NULL;

  /*
   * Create scrollbars, clip, and the child widget.  The child widget is
   * the one that we draw into.
   */
  new->www.vert_bar = XtVaCreateWidget("vert_bar",
				       scrollbarWidgetClass, n,
				       XtNorientation, XtorientVertical,
				       XtNmappedWhenManaged, False,
				       NULL);
  XtManageChild(new->www.vert_bar);

  XtAddCallback(new->www.vert_bar, XtNscrollProc,
		ScrollUpDownProc, (XtPointer)new);
  XtAddCallback(new->www.vert_bar, XtNjumpProc,
		ThumbProc, (XtPointer)new);
  
  new->www.horiz_bar = XtVaCreateWidget("horiz_bar",
					scrollbarWidgetClass, n,
					XtNorientation, XtorientHorizontal,
					XtNmappedWhenManaged, False,
					NULL);
  XtManageChild(new->www.horiz_bar);

  XtAddCallback(new->www.horiz_bar, XtNscrollProc,
		ScrollUpDownProc, (XtPointer)new);
  XtAddCallback(new->www.horiz_bar, XtNjumpProc,
		ThumbProc, (XtPointer)new);

  new->www.clip = XtVaCreateWidget("clip",
				   geomWidgetClass, n,
				   XtNwidth, new->core.width,
				   XtNheight, new->core.height,
				   XtNborderWidth, 0,
				   NULL);
  XtManageChild(new->www.clip);

  new->www.child = XtVaCreateWidget("child",
				    geomWidgetClass, new->www.clip,
				    XtNwidth, new->www.clip->core.width,
				    XtNheight, new->www.clip->core.height,
				    XtNborderWidth, 0,
				    NULL);
  XtManageChild(new->www.child);

  XtAugmentTranslations(new->www.child,
			 XtParseTranslationTable(defaultTranslations));

  return;
}

/*
 * FixDim
 */
static Dimension
FixDim(d)
unsigned int d;
{
  if (d > 0x7ffd) return(0x7ffd);
  return((Dimension)d);
}

/*
 * ConfigureWidgets
 */
static void
ConfigureWidgets(rw, width, height)
WWWWidget rw;
unsigned int width, height;
{
  Dimension child_width, child_height;
  Dimension clip_width, clip_height;
  Dimension vx, hy;
  Dimension vw, hh;
  float shown;

  if (!XtIsRealized((Widget)rw)) return;

  vw = rw->www.vert_bar->core.width;
  hh = rw->www.horiz_bar->core.height;

  child_width = FixDim(width);
  child_height = FixDim(height);

  clip_width = rw->core.width - vw;
  clip_height = rw->core.height - hh;

  if (child_width > clip_width) hy = hh;
  else
  {
    hy = 0;
    child_width = clip_width;
  }

  if (child_height > clip_height) vx = vw;
  else
  {
    vx = 0;
    child_height = clip_height;
  }

  if (child_height > clip_height)
  {
    XtConfigureWidget(rw->www.vert_bar,
		      0, hy,
		      vw, rw->core.height - hy, 0);
    shown = (float)clip_height / (float)child_height;
    XawScrollbarSetThumb(rw->www.vert_bar, (float)-1.0, shown);
    XtSetMappedWhenManaged(rw->www.vert_bar, True);
  }
  else XtSetMappedWhenManaged(rw->www.vert_bar, False);

  if (child_width > clip_width)
  {
    XtConfigureWidget(rw->www.horiz_bar,
		      vx, 0,
		      rw->core.width - vx, hh, 0);
    shown = (float)clip_width / (float)child_width;
    XawScrollbarSetThumb(rw->www.horiz_bar, (float)-1.0, shown);
    XtSetMappedWhenManaged(rw->www.horiz_bar, True);
  }
  else XtSetMappedWhenManaged(rw->www.horiz_bar, False);

  XtConfigureWidget(rw->www.clip,
		    vw, hh,
		    clip_width, clip_height, 0);

  XtResizeWidget(rw->www.child, child_width, child_height, 0);

  return;
}

/*
 * WWWGetDrawWidget
 */
Widget
WWWGetDrawWidget(w)
Widget w;
{
  WWWWidget rw = (WWWWidget)w;
  return(rw->www.child);
}

/*
 * WWWGetSize
 */
int
WWWGetDrawSize(w, width, height)
Widget w;
unsigned int *width, *height;
{
  WWWWidget rw = (WWWWidget)w;

  *width = (unsigned int)rw->www.child->core.width;
  *height = (unsigned int)rw->www.child->core.height;

  return(0);
}

/*
 * WWWSetDrawSize
 */
int
WWWSetDrawSize(w, width, height)
Widget w;
unsigned int width, height;
{
  WWWWidget rw = (WWWWidget)w;

  if (!rw->www.use_scroll)
  {
    XtSetMappedWhenManaged(rw->www.vert_bar, False);
    XtSetMappedWhenManaged(rw->www.horiz_bar, False);
    XtConfigureWidget(rw->www.clip, 0, 0, FixDim(width), FixDim(height), 0);
    XtConfigureWidget(rw->www.child, 0, 0, FixDim(width), FixDim(height), 0);
  }
  else
  {
    ConfigureWidgets(rw, width, height);
  }

  return(0);
}

/*
 * WWWSetScrollBar
 */
void
WWWSetScrollBar(w, use_scroll)
Widget w;
bool use_scroll;
{
  WWWWidget rw = (WWWWidget)w;
  
  rw->www.use_scroll = use_scroll;
  if (!rw->www.use_scroll)
  {
    XtSetMappedWhenManaged(rw->www.vert_bar, False);
    XtSetMappedWhenManaged(rw->www.horiz_bar, False);
    XtConfigureWidget(rw->www.clip, 0, 0,
		      rw->core.width, rw->core.height, 0);
    XtConfigureWidget(rw->www.child, 0, 0,
		      rw->core.width, rw->core.height, 0);
  }

  return;
}

/*
 * WWWSetResizeCallback
 */
void
WWWSetResizeCallback(w, callback, closure)
Widget w;
void (*callback)();
void *closure;
{
  WWWWidget rw = (WWWWidget)w;

  rw->www.resize_callback = callback;
  rw->www.resize_closure = closure;

  return;
}

/*
 * WWWSetMotionCallback
 */
void
WWWSetMotionCallback(w, callback, closure)
Widget w;
void (*callback)();
void *closure;
{
  WWWWidget rw = (WWWWidget)w;

  rw->www.motion_callback = callback;
  rw->www.motion_closure = closure;

  return;
}

/*
 * WWWSetSelectCallback
 */
void
WWWSetSelectCallback(w, callback, closure)
Widget w;
void (*callback)();
void *closure;
{
  WWWWidget rw = (WWWWidget)w;

  rw->www.select_callback = callback;
  rw->www.select_closure = closure;

  return;
}

/*
 * WWWSetExposeCallback
 */
void
WWWSetExposeCallback(w, callback, closure)
Widget w;
void (*callback)();
void *closure;
{
  WWWWidget rw = (WWWWidget)w;

  rw->www.expose_callback = callback;
  rw->www.expose_closure = closure;

  return;
}

/*
 * WWWExposeAction
 */
static void
WWWExposeAction(w, xe, params, num_params)
Widget w;
XEvent *xe;
String *params;         /* unused */
Cardinal *num_params;   /* unused */
{
  WWWWidget rw = (WWWWidget)XtParent(XtParent(w));

  if (rw->www.expose_callback != NULL)
  {
    (rw->www.expose_callback) (w, rw->www.expose_closure,
			       xe->xexpose.x, xe->xexpose.y,
			       xe->xexpose.width, xe->xexpose.height);
  }

  return;
}

/*
 * WWWSelectAction
 */
static void
WWWSelectAction(w, xe, params, num_params)
Widget w;
XEvent *xe;
String *params;         /* unused */
Cardinal *num_params;   /* unused */
{
  WWWWidget rw = (WWWWidget)XtParent(XtParent(w));

  if (rw->www.select_callback != NULL)
  {
    (rw->www.select_callback) (w, rw->www.select_closure,
			       xe->xbutton.x, xe->xbutton.y,
			       xe->xbutton.button);
  }

  return;
}

/*
 * WWWMotionAction
 */
static void
WWWMotionAction(w, xe, params, num_params)
Widget w;
XEvent *xe;
String *params;         /* unused */
Cardinal *num_params;   /* unused */
{
  WWWWidget rw = (WWWWidget)XtParent(XtParent(w));

  if (rw->www.motion_callback != NULL)
  {
    (rw->www.motion_callback) (w, rw->www.motion_closure,
			       xe->xmotion.x, xe->xmotion.y);
  }

  return;
}

/*
 * WWWGetScrollPosition
 */
void
WWWGetScrollPosition(w, x, y)
Widget w;
int *x, *y;
{
  WWWWidget rw = (WWWWidget)w;

  *x = (int)(rw->www.child->core.x);
  *y = (int)(rw->www.child->core.y);

  return;
}

/*
 * WWWPageUpAction
 */
static void
WWWPageUpAction(w, xe, params, num_params)
Widget w;
XEvent *xe;
String *params;         /* unused */
Cardinal *num_params;   /* unused */
{
 WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
 Widget child = rw->www.child;
 Widget clip  = rw->www.clip;

 WWWMoveChild((Widget)rw,child->core.x,child->core.y+clip->core.height/2);
}

/*
 * WWWPageDownAction
 */
static void
WWWPageDownAction(w, xe, params, num_params)
Widget w;
XEvent *xe;
String *params;         /* unused */
Cardinal *num_params;   /* unused */
{
 WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
 Widget child = rw->www.child;
 Widget clip  = rw->www.clip;

 WWWMoveChild((Widget)rw,child->core.x,child->core.y-clip->core.height/2);
}

/*
 * WWWPageLeftAction
 */
static void
WWWPageLeftAction(w, xe, params, num_params)
Widget w;
XEvent *xe;
String *params;         /* unused */
Cardinal *num_params;   /* unused */
{
 WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
 Widget child = rw->www.child;
 Widget clip  = rw->www.clip;

 WWWMoveChild((Widget)rw,child->core.x+clip->core.width/2,child->core.y);
}

/*
 * WWWPageRightAction
 */
static void
WWWPageRightAction(w, xe, params, num_params)
Widget w;
XEvent *xe;
String *params;         /* unused */
Cardinal *num_params;   /* unused */
{
 WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
 Widget child = rw->www.child;
 Widget clip  = rw->www.clip;

 WWWMoveChild((Widget)rw,child->core.x-clip->core.width/2,child->core.y);
}

/*
 * WWWScrollUpAction
 */
static void
WWWScrollUpAction(w, xe, params, num_params)
Widget w;
XEvent *xe;
String *params;         /* unused */
Cardinal *num_params;   /* unused */
{
 WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
 Widget child = rw->www.child;

 WWWMoveChild((Widget)rw,child->core.x,child->core.y+10);
}

/*
 * WWWScrollDownAction
 */
static void
WWWScrollDownAction(w, xe, params, num_params)
Widget w;
XEvent *xe;
String *params;         /* unused */
Cardinal *num_params;   /* unused */
{
 WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
 Widget child = rw->www.child;

 WWWMoveChild((Widget)rw,child->core.x,child->core.y-10);
}

/*
 * WWWScrolLeftAction
 */
static void
WWWScrollLeftAction(w, xe, params, num_params)
Widget w;
XEvent *xe;
String *params;         /* unused */
Cardinal *num_params;   /* unused */
{
 WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
 Widget child = rw->www.child;

 WWWMoveChild((Widget)rw,child->core.x+10,child->core.y);
}

/*
 * WWWScrollRightAction
 */
static void
WWWScrollRightAction(w, xe, params, num_params)
Widget w;
XEvent *xe;
String *params;         /* unused */
Cardinal *num_params;   /* unused */
{
 WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
 Widget child = rw->www.child;

 WWWMoveChild((Widget)rw,child->core.x-10,child->core.y);
}
