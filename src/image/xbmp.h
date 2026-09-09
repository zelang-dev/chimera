/*
 * xbmp.h
 *
 * Copyright (C) 1995, Erik Corry (ehcorry@inet.uni-c.dk)
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

#ifndef XBM_H_INCLUDED
#define XBM_H_INCLUDED 1

#include "image_format.h"

/*
 * xbmState
 */
typedef struct xbm_state
{
  int state;               /* state of XBM reader */
  FormatLineProc lineProc; /* line callback */
  void *closure;           /* closure for callback */

  Image *image;

  int   pos;               /* current read position */
  int   ypos;              /* current line */
  int   xpos;              /* current pixel */
  byte *imagepos;          /* current posn. in output */

} xbmState;

/*
 * state of XBM reader
 */
#define XBM_FINISHED 0
#define XBM_READ_HEADER 1
#define XBM_READ_IMAGE 2

/*
 * return values from xbm processing fns
 */
#define XBM_ERROR 0
#define XBM_SUCCESS 1
#define XBM_NEED_DATA 2

#endif
