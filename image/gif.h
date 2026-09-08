/*
 * gif.h
 *
 * Copyright (C) 1995, John Kilburg (john@isri.unlv.edu)
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

#ifndef __GIF_H__
#define __GIF_H__ 1

#define STAB_SIZE  4096         /* string table size */
#define PSTK_SIZE  4096         /* pixel stack size */


/*
 * gifState
 */
typedef struct _gif_state
{
  int state;               /* state of GIF reader */
  void (*lineProc)();      /* line callback */
  void *closure;           /* closure for callback */

  Image *image;

  int  root_size;          /* root code size */
  int  clr_code;           /* clear code */
  int  eoi_code;           /* end of information code */
  int  code_size;          /* current code size */
  int  code_mask;          /* current code mask */
  int  prev_code;          /* previous code */

  int  work_data;          /* working bit buffer */
  int  work_bits;          /* working bit count */

  byte *buf;               /* byte buffer */
  int  buf_cnt;            /* byte count */
  int  buf_idx;            /* buffer index */

  int table_size;          /* string table size */
  int prefix[STAB_SIZE];   /* string table : prefixes */
  int extnsn[STAB_SIZE];   /* string table : extensions */

  byte pstk[PSTK_SIZE];    /* pixel stack */
  int  pstk_idx;           /* pixel stack pointer */

  int  pos;                 /* current read position */
  byte *data;               /* pointer to data */
  int  datalen;             /* length of data */
  int  rast_width;          /* raster width */
  int  rast_height;         /* raster height */
  byte g_cmap_flag;         /* global colormap flag */
  int  g_pixel_bits;        /* bits per pixel, global colormap */
  int  g_ncolors;           /* number of colors, global colormap */
  int  g_ncolors_pos;       /* number of colors processed, global colormap */
  Intensity g_cmap[3][256]; /* global colormap */
  int  bg_color;            /* background color index */
  int  color_bits;          /* bits of color resolution */
  int  transparent;         /* transparent color index */

  int  img_left;            /* image position on raster */
  int  img_top;             /* image position on raster */
  int  img_width;           /* image width */
  int  img_height;          /* image height */
  byte have_dimensions;     /* 1 when dimensions known */
  byte l_cmap_flag;         /* local colormap flag */
  int  l_pixel_bits;        /* bits per pixel, local colormap */
  int  l_ncolors;           /* number of colors, local colormap */
  int  l_ncolors_pos;       /* number of colors processed, local colormap */
  Intensity l_cmap[3][256]; /* local colormap */
  byte interlace_flag;      /* interlace image format flag */
} gifState;

gifState *gifInit _ArgProto((void (*)(), void *));
void gifDestroy _ArgProto((gifState *));
int gifAddData _ArgProto((gifState *, byte *, int));

#endif
