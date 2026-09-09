/*
 * pnm.h
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

#ifndef PNM_H_INCLUDED
#define PNM_H_INCLUDED 1

#include "image_format.h"

/*
 * pnmState
 */
typedef struct pnm_state
{
  int state;               /* state of PNM reader */
  FormatLineProc lineProc; /* line callback */
  void *closure;           /* closure for callback */

  Image *image;

  int   pos;               /* current read position */
  int   ypos;              /* current line */
  int   xpos;              /* current pixel */
  byte *imagepos;          /* current posn. in output */
  bool  mac_newlines;      /* CR or LFCR for newlines */
  bool  raw;               /* raw file */
  byte *input_table;
  bool  free_input_table;
  Intensity cmap[3][256]; /* colormap */
  int   max_val;
  int   pnm_class;
  int   width;

} pnmState;

/*
 * state of PNM reader
 */
#define PNM_READ_MAGIC 1
#define PNM_READ_WIDTH 2
#define PNM_READ_HEIGHT 3
#define PNM_READ_MAX_VAL 4
#define PNM_READ_PRE_RAW_NEWLINE 5
#define PNM_READ_RAW 6
#define PNM_READ_ASC 7
#define PNM_READ_ASC_BIT 8
#define PNM_FINISHED 0

/*
 * return values from pnm processing fns
 */
#define PNM_ERROR 0
#define PNM_SUCCESS 1
#define PNM_NEED_DATA 2

#endif
