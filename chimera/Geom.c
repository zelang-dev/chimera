/*
 * Geom.c
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

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>

#include "port_after.h"

#include "GeomP.h"

static void GeomInitialize _ArgProto((Widget, Widget, ArgList, Cardinal *));
static XtGeometryResult GeomGeometryManager _ArgProto((Widget,
						       XtWidgetGeometry *,
						       XtWidgetGeometry *));
static XtGeometryResult GeomPreferredGeometry _ArgProto((Widget,
							 XtWidgetGeometry *,
							 XtWidgetGeometry *));

GeomClassRec geomClassRec =
{
  { /* core_class fields */
    /* superclass         */    (WidgetClass)&compositeClassRec,
    /* class_name         */    "Geom",
    /* widget_size        */    sizeof(GeomRec),
    /* class_initialize   */    NULL,
    /* class_part_init    */    NULL,
    /* class_inited       */    FALSE,
    /* initialize         */    GeomInitialize,
    /* initialize_hook    */    NULL,
    /* realize            */    XtInheritRealize,
    /* actions            */    NULL,
    /* num_actions        */    0,
    /* resources          */    NULL,
    /* num_resources      */    0,
    /* xrm_class          */    NULLQUARK,
    /* compress_motion    */    FALSE,
    /* compress_exposure  */    FALSE,
    /* compress_enterleave*/    FALSE,
    /* visible_interest   */    FALSE,
    /* destroy            */    NULL,
    /* resize             */    NULL,
    /* expose             */    NULL,
    /* set_values         */    NULL,
    /* set_values_hook    */    NULL,
    /* set_values_almost  */    XtInheritSetValuesAlmost,
    /* get_values_hook    */    NULL,
    /* accept_focus       */    NULL,
    /* version            */    XtVersion,
    /* callback_private   */    NULL,
    /* tm_table           */    NULL,
    /* query_geometry     */	GeomPreferredGeometry,
    /* display_accelerator*/	XtInheritDisplayAccelerator,
    /* extension          */	NULL
  },
  { /* composite_class fields */
    /* geometry_manager   */   GeomGeometryManager,
    /* change_managed     */   NULL,
    /* insert_child       */   XtInheritInsertChild,
    /* delete_child       */   XtInheritDeleteChild,
    /* extension          */   NULL
  },
  { /* geom_class fields */
    NULL,
  }
};

WidgetClass geomWidgetClass = (WidgetClass)&geomClassRec;

static XtGeometryResult
GeomPreferredGeometry(w, con, reply)
Widget w;
XtWidgetGeometry *con, *reply;
{
  return(XtGeometryYes);
}

/*
 * GeomGeometryManager
 */
static XtGeometryResult
GeomGeometryManager(w, request, reply)
Widget w;
XtWidgetGeometry *request;
XtWidgetGeometry *reply;
{ 
  return(XtGeometryNo);
}

/*
 * GeomInitialize
 */
static void
GeomInitialize(r, n, args, count)
Widget r, n;
ArgList args;
Cardinal *count;
{
  return;
}
