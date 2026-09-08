/* colorcube.h
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
 * This contains routines which take a general description of which
 * colors we have managed to allocate from the device, and produces
 * tables that can be used to convert various input formats to use
 * these colors. The dither algorithm used (where necessary) is a
 * dispersed table dither up to 8x8 in size, from an automatically
 * generated table.
 *
 * The routines here are not necessarily fast (some of them are very
 * slow indeed), but they generate tables that can be used in fast code.
 *
 */

#ifndef COLORCUBE_H_INCLUDED
#define COLORCUBE_H_INCLUDED 1

typedef struct ccs_cube *cct_cube;
typedef union ccu_dither_table cct_dither_table;
typedef struct ccs_true_8_dither_table *cct_true_8_dither_table;
typedef struct ccs_8_8_dither_table *cct_8_8_dither_table;
typedef struct ccs_8_true_conversion_table *cct_8_true_conversion_table;
typedef struct ccs_true_true_conversion_table *cct_true_true_conversion_table;

union ccu_dither_table 
{
  void *generic_dither_table;
  cct_true_8_dither_table true_8_dither;
  cct_8_8_dither_table eight_8_dither;
  cct_8_true_conversion_table eight_true_conversion;
  cct_true_true_conversion_table true_true_conversion;
};

typedef enum
{
  cube_true_color,
  cube_mapping,
  cube_no_mapping,
  cube_grayscale
} cce_cube_type;

/*
 * General description of colors that have been allocated on output
 * device.
 *
 * For a small rxbxg cuboid, mapping or no_mapping is used.
 * value_count[0] is the number of red planes, etc. Brightness
 * is measured 0-255. To get a pixel number, add the pixel
 * values together for r, g and b and then use the mapping table if
 * necessary.
 *
 * For true_color (incl. 'hicolor') no dithering is necessary.
 * Just shift and mask.
 *
 * brightnesses should be sorted in ascending order!
 *
 */

struct ccs_cube
{
  union
  {
    struct
    {
      unsigned long pixel_values[3][256];
      unsigned int  brightnesses[3][256];
               int  value_count[3];
    } no_mapping;
    struct
    {
      unsigned long pixel_values[3][256];
      unsigned int  brightnesses[3][256];
               int  value_count[3];
      unsigned int  mapping[256];
    } mapping;
    struct
    {
      unsigned int  color_mask[3];
      unsigned int  color_base;
    } true_color;
    struct
    {
      unsigned long pixel_values[256];
      unsigned int  brightnesses[256];
      int value_count;
    } grayscale;
  } u;
  cce_cube_type cube_type;
};

struct ccs_special_color
{
  unsigned long pixel;
  unsigned char brightnesses[3];
};

typedef struct ccs_special_color *cct_special_color;

/*
 * Table for dithering from true-color to a colorcube allocated on
 * an 4-16 bit pseudocolor screen. This table is only good for one
 * row of a dither table, (which could be 2x2 up to 4x4). If the
 * table has less than 4 columns, the first entries are duplicated
 * so that 4 can always be assumed when using the table for simplicity.
 *
 * Since the mapping table has only 256 entries it can only be used on
 * screens with 8 bits per pixel or less.
 */
struct ccs_true_8_dither_table
{
  unsigned int  mapping[256];
  unsigned int  pixel_values[3][4][256];
           int  column_count; /* 2-8 */
           bool mapping_used;
};

/*
 * Table for dithering from an 8-bit palette image to a color image
 * based on a fixed allocation of colors on the screen.
 *
 * Also used as a:
 * Table for dithering from gray 8-bit pixels to less gray levels.
 * Works right down to only 2 different colors i.e. black and white
 * See above wrt. column_count
 */
struct ccs_8_8_dither_table
{
  unsigned int  pixel_values[4][256];
           int  column_count; /* 2-8 */
};

/*
 * Table for converting 8-bit colormap pixels to true color (or
 * hi-color) pixels on screen.
 */
struct ccs_8_true_conversion_table
{
  unsigned int  pixel_values[256];
};

/*
 * Table for converting a true-color 24-bit image to a true-color
 * or hi-color screen with a different information layout.
 */
struct ccs_true_true_conversion_table
{
  unsigned int  pixel_values[3][256];
};

cct_true_8_dither_table
ccf_create_true_8_dither_table(
    int row,
    int total_rows,
    int total_columns,
    cct_cube cube);

cct_8_8_dither_table
ccf_create_gray_dither_table(
    int row,
    int total_rows,
    int total_columns,
    cct_cube cube);

cct_8_8_dither_table
ccf_true_8_to_8_8_dither_table(
  cct_true_8_dither_table true_8,
  int map_size,
  unsigned char *colormaps[3]);

cct_true_true_conversion_table
ccf_create_true_true_conversion_table(cct_cube cube);

cct_8_true_conversion_table
ccf_true_true_to_8_true_conversion_table(
  cct_true_true_conversion_table true_true,
  int map_size,
  unsigned char *colormaps[3]);

cct_8_true_conversion_table
ccf_create_gray_conversion_table(cct_cube cube);

cct_8_8_dither_table
ccf_gray_to_gray_dither_convert(
  cct_8_8_dither_table table,
  int map_size,
  unsigned char *colormaps[3]);

cct_8_true_conversion_table
ccf_gray_to_gray_conversion_convert(
  cct_8_true_conversion_table table,
  int map_size,
  unsigned char *colormaps[3]);

void
ccf_set_specially_allocated(
  cct_8_8_dither_table table,
  unsigned char entry,
  unsigned int pixel);

#endif /* COLORCUBE_H_INCLUDED */
