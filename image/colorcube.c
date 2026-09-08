/* colorcube.c
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
 * dispersed table dither up to 4x4 in size, from an automatically
 * generated table.
 *
 * The routines here are not necessarily fast (some of them are very
 * slow indeed), but they generate tables that can be used in fast code.
 *
 * Error diffusion dither is not used for three reasons:
 *
 * o It is somewhat slower than table dither.
 * o You cannot dither an interleaved image properly with it.
 * o You cannot alter a small part of an image and then just redither that
 *   without the edges showing.
 * o It is harder to program.
 * o You shouldn't be trying to view jpegs on a 1-bit screen anyway. :-)
 */

#include "port_before.h"

#include <stdlib.h>

#include "port_after.h"

#include "imagep.h"
#include "colorcube.h"

/*
 * Create a matrix of the type:
 *
 * (1 3)
 * (4 2)
 *
 * but of the required size, for a dispersed dither.
 */

static void
lf_create_dither_matrix(
    int matrix[4][4],
    int tr,
    int tc)
{
  int c,r;
  bool swapped = false;

  /* Make sure the column count is <= row count */

  if (tc > tr)
  { 
    int t = tc;
    tc = tr;
    tr = t;
    swapped = true;
  }

  /* Seed the matrix */

  matrix[0][0] = 1;

  /* Bring matrix up to size */

  for (c = 1, r = 2; r <= tr; r *= 2)
  {
    int x,y;
    int r2 = r/2;
    int c2 = c;
    for (y = 0; y < r2; y++)
    {
      for (x = 0; x < c; x++)
      {
        matrix[y + r2][x] = matrix[y][x] * 2 -1;
        matrix[y][x] *= 2;
      }
    }

  c *=2;
  if (c > tc) break;

    for (y = 0; y < r; y++)
    {
      for (x= 0; x < c2; x++)
      { 
        matrix[(y + r2) % r][x + c2] = matrix[y][x] * 2 -1;
        matrix[y][x] *= 2;
      }
    }
  }

  /* Swap matrix width/height if necessary */

  if (swapped)
  { 
    for (r = 0; r < tr; r++)
    {
      for (c = 0; c < r; c++)
      {
        int t = matrix[r][c];
        matrix[r][c] = matrix[c][r];
        matrix[c][r] = t;
      }
    }
  }
}

static bool
lf_power_of_2(int t)
{
  if (t == (t&1)) return true;
  return lf_power_of_2(t >> 1);
}

static void
lf_calculate_dither_table(
    unsigned int *brightnesses,
             int  value_count,
             double  max,
             int  total_columns,
             int  matrix[8],
    unsigned int  answer_pixel_values[8][256],
    unsigned long pixel_values[256])
 {
    int i, c, j;
    double brightness_factor = 255.0 / 
      (brightnesses[value_count - 1] -
       brightnesses[0]);

    for (c = 0; c < total_columns; c++)
    {
      for (i = 1; i < value_count; i++)
      {
        int l = brightnesses[i-1];
        int u = brightnesses[i];

        l -= brightnesses[0];
        u -= brightnesses[0];
        l = (int)((double)l * brightness_factor + 0.001);
        u = (int)((double)u * brightness_factor + 0.001);

        if(u != l) for (j = l; j <= u; j++)
        {
          if (((double)(j-l) / (double)(u-l)) <=
                ((double)(matrix[c]-1) / max))
            answer_pixel_values[c][j] = pixel_values[i-1];
	  else
            answer_pixel_values[c][j] = pixel_values[i];
	  /*
	   * Special hack to avoid spotty saturated colors
	   */
	  if(u == 255 && j == 255)
	    answer_pixel_values[c][j] = pixel_values[i];
        }
      }
    }
    for (c = total_columns; c < 4; c++)
    {
      for (i = 0; i < 256; i++)
      {
        answer_pixel_values[c][i] = answer_pixel_values[c % total_columns][i];
      }
    }
  }

static bool
lf_check_rows_columns(int total_rows, int total_columns)
{
  /* Check inputs are reasonable */

  if (total_rows > 4 ||
     total_columns > 4 ||
     total_rows < 1 ||
     total_columns < 1 ||
     !lf_power_of_2(total_rows) ||
     !lf_power_of_2(total_columns))
  {
    return false;
  }

  if (total_rows / total_columns > 2) return false;
  if (total_columns / total_rows > 2) return false;

  return true;
}

cct_true_8_dither_table
ccf_create_true_8_dither_table(
    int row,
    int total_rows,
    int total_columns,
    cct_cube cube)
{
  cct_true_8_dither_table answer;
  int matrix[4][4];
  int i;
  int rgb;
  double max;

  if (!lf_check_rows_columns(total_rows, total_columns))
    return 0;
  for (rgb = 0; rgb < 3; rgb++)
  {
    /* relies on union fields coinciding */
    if (cube->u.mapping.value_count[rgb] < 2) return 0;
  }

  lf_create_dither_matrix(matrix, total_rows, total_columns);

  max = total_rows * total_columns - 1;

  answer = (cct_true_8_dither_table)
    calloc(1, sizeof(struct ccs_true_8_dither_table));

  answer->column_count = total_columns;

  answer->mapping_used = (cube->cube_type == cube_mapping);
  if (answer->mapping_used)
  {
    for (i = 0; i < 256; i++)
      answer->mapping[i] = cube->u.mapping.mapping[i];
  }

  for (rgb = 0; rgb < 3; rgb++)
  {
    lf_calculate_dither_table(
      cube->u.mapping.brightnesses[rgb],
      cube->u.mapping.value_count[rgb],
      max,
      total_columns,
      matrix[row],
      answer->pixel_values[rgb],
      cube->u.mapping.pixel_values[rgb]);
  }

  return answer;
}

/*
 * If a color is exactly allocated on the screen, outside of the
 * color cube, there is no need to dither that color, it can be
 * displayed directly. This routine amends the tables accordingly
 */

void
ccf_set_specially_allocated(
    cct_8_8_dither_table table,
    unsigned char entry,
    unsigned int pixel)
{
  int c;
  for(c = 0; c < 4; c++)
  {
    table->pixel_values[c][entry] = pixel;
  }
}

cct_8_8_dither_table
ccf_create_gray_dither_table(
    int row,
    int total_rows,
    int total_columns,
    cct_cube cube)
{
  cct_8_8_dither_table answer;
  int matrix[4][4];
  double max;

  if(!lf_check_rows_columns(total_rows, total_columns)) return 0;

  if (cube->u.grayscale.value_count < 2) return 0;

  lf_create_dither_matrix(matrix, total_rows, total_columns);

  max = total_rows * total_columns - 1;

  answer = (cct_8_8_dither_table)
    calloc(1, sizeof(struct ccs_8_8_dither_table));

  answer->column_count = total_columns;

  lf_calculate_dither_table(
    cube->u.grayscale.brightnesses,
    cube->u.grayscale.value_count,
    max,
    total_columns,
    matrix[row],
    answer->pixel_values,
    cube->u.grayscale.pixel_values);

  return answer;
}

/*
 * For 8-bit palette input we do not need to convert to true-color
 * before dithering. We simply make a new table that takes palette
 * entries as inputs
 */

cct_8_8_dither_table
ccf_true_8_to_8_8_dither_table(
  cct_true_8_dither_table true_8,
  int map_size,
  unsigned char *colormaps[3])
{
  cct_8_8_dither_table answer;
  int c,i,rgb;

  answer =
    (cct_8_8_dither_table) calloc(1, sizeof(struct ccs_8_8_dither_table));

  answer->column_count = true_8->column_count;

  for (c = 0; c < 4; c++)
  {
    for (i = 0; i < map_size; i++)
    {
      for (rgb = 0; rgb < 3; rgb++)
      {
        answer->pixel_values[c][i] +=
          true_8->pixel_values[rgb][c][colormaps[rgb][i]];
      }
      if (true_8->mapping_used)
        answer->pixel_values[c][i] =
          true_8->mapping[answer->pixel_values[c][i]];
    }
  }
  return answer;
}


/*
 * How many 1 bits are there in the longword.
 * Low performance, do not call often.
 */
static int
lf_number_of_bits_set(unsigned long a)
{
    if (!a) return 0;
    if (a & 1) return 1 + lf_number_of_bits_set(a >> 1);
    return(lf_number_of_bits_set(a >> 1));
}

/*
 * Shift the 0s in the least significant end out of the longword.
 * Low performance, do not call often.
 */
static unsigned long
lf_shifted_down(unsigned long a)
{
    if (!a) return 0ul;
    if (a & 1) return a;
    return lf_shifted_down(a >> 1);
}

/*
 * How many 0 bits are there at most significant end of longword.
 * Low performance, do not call often.
 */
static int
lf_free_bits_at_top(unsigned long a)
{
      /* assume char is 8 bits */
    if (!a) return sizeof(unsigned long) * 8;
        /* assume twos complement */
    if (((long)a) < 0l) return 0;
    return 1 + lf_free_bits_at_top(a << 1);
}

/*
 * How many 0 bits are there at least significant end of longword.
 * Low performance, do not call often.
 */
static int
lf_free_bits_at_bottom(unsigned long a)
{
      /* assume char is 8 bits */
    if (!a) return sizeof(unsigned long) * 8;
    if (((long)a) & 1l) return 0;
    return 1 + lf_free_bits_at_bottom(a >> 1);
}


cct_true_true_conversion_table
ccf_create_true_true_conversion_table(cct_cube cube)
{
  cct_true_true_conversion_table answer;
  int i,rgb;

  answer = (cct_true_true_conversion_table)
    calloc(1, sizeof(struct ccs_true_true_conversion_table));

  for (rgb = 0; rgb < 3; rgb++)
  {
    int bits_set = lf_number_of_bits_set(cube->u.true_color.color_mask[rgb]);
    int free_bits_at_bottom =
      lf_free_bits_at_bottom(cube->u.true_color.color_mask[rgb]);
    for (i = 0; i < 256; i++)
    {
      answer->pixel_values[rgb][i] = (i >> (8 - bits_set)) << free_bits_at_bottom;
      if (rgb == 0)
        answer->pixel_values[rgb][i] += cube->u.true_color.color_base;
    }
  }
  return answer;
}
  
/*
 * For 8-bit palette input we do not need to convert to true-color
 * one component at a time. We can do it in one lookup. This generates
 * the table.
 */

cct_8_true_conversion_table
ccf_true_true_to_8_true_conversion_table(
  cct_true_true_conversion_table true_true,
  int map_size,
  unsigned char *colormaps[3])
{
  cct_8_true_conversion_table answer;
  int i, rgb;

  answer = (cct_8_true_conversion_table)
    calloc(1, sizeof(struct ccs_8_true_conversion_table));

  for (i = 0; i < map_size; i++)
  {
    for (rgb = 0; rgb < 3; rgb ++)
    {
      answer->pixel_values[i] +=
        true_true->pixel_values[rgb][colormaps[rgb][i]];
    }
  }
  return answer;
}

/*
 * Create gray table. This is assuming we have enough entries in the
 * colormap that we do not need to dither, but can simply remap. This
 * is about 64 entries, 32 at a push.
 */

cct_8_true_conversion_table
ccf_create_gray_conversion_table(cct_cube cube)
{
  cct_8_true_conversion_table answer;
  int i;

  answer = (cct_8_true_conversion_table)
    calloc(1, sizeof(struct ccs_8_true_conversion_table));

  for (i = 0; i < 256; i++)
  {
    int j;
    int min_dist = 400000; /* big number ! */
    int min_entry;
    for (j = 0; j < cube->u.grayscale.value_count; j++)
    {
      int dist;
      dist = i - cube->u.grayscale.brightnesses[j];
      if (dist < 0) dist = -dist;
      if (dist < min_dist)
      {
        min_dist = dist;
        min_entry = j;
      }
    }
    answer->pixel_values[i] = cube->u.grayscale.pixel_values[min_entry];
  }
  return answer;
}

/*
 * If we are converting 8-bit palette to grayscale, we do not
 * need to convert every pixel to gray, we can simply integrate
 * the color map into the dither table
 */

cct_8_8_dither_table
ccf_gray_to_gray_dither_convert(
  cct_8_8_dither_table table,
  int map_size,
  unsigned char *colormaps[3])
{
  int i;
  int c;
  cct_8_8_dither_table answer;

  answer = (cct_8_8_dither_table)calloc(1, sizeof(struct ccs_8_8_dither_table));

  answer->column_count = table->column_count;

  for (i = 0; i < map_size; i++)
  {
    unsigned int bright =
      RedIntensity[colormaps[0][i]] +
      GreenIntensity[colormaps[1][i]] +
      BlueIntensity[colormaps[2][i]];

    bright >>= 8;

    for (c = 0; c < 4; c++)
    {
      answer->pixel_values[c][i] = table->pixel_values[c][bright];
    }
  }

  return answer;
}
  
/*
 * If we are converting 8-bit palette to grayscale, we do not
 * need to convert every pixel to gray, we can simply integrate
 * the color map into the conversion table
 */

cct_8_true_conversion_table
ccf_gray_to_gray_conversion_convert(
  cct_8_true_conversion_table table,
  int map_size,
  unsigned char *colormaps[3])
{
  int i;
  cct_8_true_conversion_table answer;

  answer = (cct_8_true_conversion_table)
    calloc(1, sizeof(struct ccs_8_true_conversion_table));

  for (i = 0; i < map_size; i++)
  {
    unsigned int bright =
      RedIntensity[colormaps[0][i]] +
      GreenIntensity[colormaps[1][i]] +
      BlueIntensity[colormaps[2][i]];

    bright >>= 8;

    answer->pixel_values[i] = table->pixel_values[bright];
  }

  return answer;
}

