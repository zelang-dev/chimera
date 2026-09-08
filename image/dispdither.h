/* dispdither.h
 *
 * (c) 1995 Erik Corry. ehcorry@inet.uni-c.dk erik@kroete2.freinet.de
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

/*
 * This contains routines which dither or convert image data from 8 or
 * 24 bit input data to 1/2/4/8/16/24/32 bit output, possibly dithered.
 */

#ifndef DISPDITHER_H_INCLUDED
#define DISPDITHER_H_INCLUDED

#include "colorcube.h"

/*
 * ddt_dither_fn is a pointer to a function returning void
 */
typedef void (*ddt_dither_fn)();

/*
 * 4, 8, 16 obpp
 */
void
ddf_color_dither_line_24(
  cct_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int output_bits_per_pixel,
  int pixel_count);

/*
 * 1, 2, 4, 8, 16 obpp
 */
void
ddf_gray_dither_line_24(
  cct_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int output_bits_per_pixel,
  int pixel_count);

/*
 * 4, 8, 16 obpp
 */
void
ddf_gray_convert_line_24(
  cct_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int output_bits_per_pixel,
  int pixel_count);

/*
 * 1, 2, 4, 8, 16 obpp
 */
void
ddf_dither_line_8(
  cct_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int output_bits_per_pixel,
  int pixel_count);

/*
 * 4, 8, 16, 24, 32 obpp
 */
void
ddf_convert_line_8(
  cct_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int output_bits_per_pixel,
  int pixel_count);

/*
 * 16, 24, 32 obpp
 */
void
ddf_convert_line_24(
  cct_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int output_bits_per_pixel,
  int pixel_count);

#endif /* DISPDITHER_H_INCLUDED */
