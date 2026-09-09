/*
 * WWWP.h
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
#ifndef __WWWP_H__
#define __WWWP_H__

#include "WWW.h"

/*
 * More Xt definitions
 */
typedef struct _WWWClassPart
{
  XtPointer extension;
} WWWClassPart;

typedef struct _WWWClassRec
{
  CoreClassPart	        core_class;
  CompositeClassPart	composite_class;
  WWWClassPart	        www_class;
} WWWClassRec;

extern WWWClassRec wwwClassRec;

typedef struct _WWWPart
{
  /* Public */
  Pixel       foreground;
  Pixel       background;

  /* X Private */
  Widget      vert_bar, horiz_bar;  /* scroll bars */
  Widget      clip;                 /* clips the child */
  Widget      child;                /* the drawing window */
  Widget      menu;                 /* menu widget */

  bool        use_scroll;

  /* other private */
  void        *resize_closure;      /* garbage */
  void        (*resize_callback)(); /* more garbage */
  void        *select_closure;      /* garbage */
  void        (*select_callback)(); /* more garbage */
  void        *motion_closure;      /* garbage */
  void        (*motion_callback)(); /* more garbage */
  void        *expose_closure;      /* garbage */
  void        (*expose_callback)(); /* more garbage */
} WWWPart;

typedef struct _WWWRec
{
  CorePart		core;
  CompositePart     	composite;
  WWWPart		www;
} WWWRec;

#endif
