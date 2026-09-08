/* dispdither.c
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

#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "port_after.h"

#include "imagep.h"
#include "colorcube.h"
#include "dispdither.h"
#include "image_endian.h"

/*
 * The dither routines all end up in a tight loop that has no:
 * o if's                    (pipeline catastrophe   )
 * o floating point          (SX, NexGen, etc.       )
 * o division                (system trap on Alpha   )
 * o unnecessary variables   (register spills on x86s)
 * and which isn't too long, so it can be superoptimised.
 */


static void
lf_color_dither_line_24_4_no_mapping( 
  cct_true_8_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x, xx;
  for(xx = 0, x = pixel_count >> 1; x; x--, xx += 2)
  {
    unsigned char t;
    xx = xx & 3;
#   ifdef CHIMERA_LITTLE_ENDIAN
      t = table->pixel_values[0][xx+1][input[3]];
      t += table->pixel_values[1][xx+1][input[4]];
      t = (t + table->pixel_values[2][xx+1][input[5]]) << 4;
      t += table->pixel_values[0][xx][input[0]];
      t += table->pixel_values[1][xx][input[1]];
      t += table->pixel_values[2][xx][input[2]];
#   else
      t = table->pixel_values[0][xx][input[0]];
      t += table->pixel_values[1][xx][input[1]];
      t = (t + table->pixel_values[2][xx][input[2]]) << 4;
      t += table->pixel_values[0][xx+1][input[3]];
      t += table->pixel_values[1][xx+1][input[4]];
      t += table->pixel_values[2][xx+1][input[5]];
#   endif
    *output++ = t;
    input += 6;
  }
}

static void
lf_color_dither_line_24_4_mapping( 
  cct_true_8_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x, xx;
  for(xx = 0, x = pixel_count >> 1; x; x--, xx += 2)
  {
    unsigned char t, tt;
    xx = xx & 3;
#   ifdef CHIMERA_LITTLE_ENDIAN
      t = table->pixel_values[0][xx][input[0]];
      t += table->pixel_values[1][xx][input[1]];
      t += table->pixel_values[2][xx][input[2]];
      t = table->mapping[t];
      tt = table->pixel_values[0][xx+1][input[3]];
      tt += table->pixel_values[1][xx+1][input[4]];
      tt += table->pixel_values[2][xx+1][input[5]];
      tt = table->mapping[tt] << 4;
#   else
      t = table->pixel_values[0][xx][input[0]];
      t += table->pixel_values[1][xx][input[1]];
      t += table->pixel_values[2][xx][input[2]];
      t = table->mapping[t] << 4;
      tt = table->pixel_values[0][xx+1][input[3]];
      tt += table->pixel_values[1][xx+1][input[4]];
      tt += table->pixel_values[2][xx+1][input[5]];
      tt = table->mapping[tt];
#   endif
    *output++ = t | tt;
    input += 6;
  }
}

static void
lf_color_dither_line_24_4( 
  cct_true_8_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  if(table->mapping_used)
    lf_color_dither_line_24_4_mapping(table, input, output, pixel_count);
  else
    lf_color_dither_line_24_4_no_mapping(table, input, output, pixel_count);
  return;
}

static void
lf_color_dither_line_24_8_no_mapping( 
  cct_true_8_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x, xx;
  for(xx = 0, x = pixel_count; x; x--, xx++)
  {
    xx = xx & 3;
    *output++ = table->pixel_values[0][xx][input[0]] +
                table->pixel_values[1][xx][input[1]] +
                table->pixel_values[2][xx][input[2]];
    input += 3;
  }
}

static void
lf_color_dither_line_24_8_mapping( 
  cct_true_8_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x, xx;
  for(xx = 0, x = pixel_count; x; x--, xx++)
  {
    int t;
    xx = xx & 3;
    t = table->pixel_values[0][xx][input[0]] +
        table->pixel_values[1][xx][input[1]] +
        table->pixel_values[2][xx][input[2]];
    *output++ = table->mapping[t];
    input += 3;
  }
}

static void
lf_color_dither_line_24_8( 
  cct_true_8_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  if(table->mapping_used)
    lf_color_dither_line_24_8_mapping(table, input, output, pixel_count);
  else
    lf_color_dither_line_24_8_no_mapping(table, input, output, pixel_count);
  return;
}

/*
 * Ignores mapping_used
 */

static void
lf_color_dither_line_24_16( 
  cct_true_8_dither_table table,
  unsigned char *input,
  unsigned short *output,
  int pixel_count)
{
  int x, xx;
  for(xx = 0, x = pixel_count; x; x--, xx++)
  {
    xx = xx & 3;
    *output++ = table->pixel_values[0][xx][input[0]] +
                table->pixel_values[1][xx][input[1]] +
                table->pixel_values[2][xx][input[2]];
    input += 3;
  }
}

/*
 * Works for 4,8,16 bits. You need at least 3 bits for 3 colors. Above 16
 * there's never any point in dithering. People with 13 bit screens can
 * mail me the patches.
 */

void
ddf_color_dither_line_24(
  cct_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int output_bits_per_pixel,
  int pixel_count)
{
  switch(output_bits_per_pixel)
  {
  case 4:
    if(pixel_count & 1) pixel_count++;
    lf_color_dither_line_24_4(table.true_8_dither, input,
			      output, pixel_count);
    break;
  case 8:
    lf_color_dither_line_24_8(table.true_8_dither, input,
			      output, pixel_count);
    break;
  case 16:
    lf_color_dither_line_24_16(table.true_8_dither, input,
			       (unsigned short *)output, pixel_count);
    break;
  default:
    /* see this in the debugger :-) */
    sprintf(output, "ERROR: color dither to %d bits attempted", output_bits_per_pixel);
    break;
  }
}

/***********************************************************************/

static void
lf_gray_dither_24_1(
  cct_8_8_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x;
  for(x = pixel_count >> 3; x; x--)
  {
    unsigned int t;
    unsigned int b;
#   ifdef CHIMERA_LITTLE_ENDIAN
      b = RedIntensity[input[0]] + GreenIntensity[input[1]] + BlueIntensity[input[2]];
      b >>= 8;
      t = table->pixel_values[0][b];
      b = RedIntensity[input[3]] + GreenIntensity[input[4]] + BlueIntensity[input[5]];
      b >>= 8;
      t |= table->pixel_values[1][b] << 1;
      b = RedIntensity[input[6]] + GreenIntensity[input[7]] + BlueIntensity[input[8]];
      b >>= 8;
      t |= table->pixel_values[2][b] << 2;
      b = RedIntensity[input[9]] + GreenIntensity[input[10]] + BlueIntensity[input[11]];
      b >>= 8;
      t |= table->pixel_values[3][b] << 3;
      b = RedIntensity[input[12]] + GreenIntensity[input[13]] + BlueIntensity[input[14]];
      b >>= 8;
      t |= table->pixel_values[0][b] << 4;
      b = RedIntensity[input[15]] + GreenIntensity[input[16]] + BlueIntensity[input[17]];
      b >>= 8;
      t |= table->pixel_values[1][b] << 5;
      b = RedIntensity[input[18]] + GreenIntensity[input[19]] + BlueIntensity[input[20]];
      b >>= 8;
      t |= table->pixel_values[2][b] << 6;
      b = RedIntensity[input[21]] + GreenIntensity[input[22]] + BlueIntensity[input[23]];
      b >>= 8;
      t |= table->pixel_values[3][b] << 7;
#   else
      b = RedIntensity[input[0]] + GreenIntensity[input[1]] + BlueIntensity[input[2]];
      b >>= 8;
      t = table->pixel_values[0][b] << 7;
      b = RedIntensity[input[3]] + GreenIntensity[input[4]] + BlueIntensity[input[5]];
      b >>= 8;
      t |= table->pixel_values[1][b] << 6;
      b = RedIntensity[input[6]] + GreenIntensity[input[7]] + BlueIntensity[input[8]];
      b >>= 8;
      t |= table->pixel_values[2][b] << 5;
      b = RedIntensity[input[9]] + GreenIntensity[input[10]] + BlueIntensity[input[11]];
      b >>= 8;
      t |= table->pixel_values[3][b] << 4;
      b = RedIntensity[input[12]] + GreenIntensity[input[13]] + BlueIntensity[input[14]];
      b >>= 8;
      t |= table->pixel_values[0][b] << 3;
      b = RedIntensity[input[15]] + GreenIntensity[input[16]] + BlueIntensity[input[17]];
      b >>= 8;
      t |= table->pixel_values[1][b] << 2;
      b = RedIntensity[input[18]] + GreenIntensity[input[19]] + BlueIntensity[input[20]];
      b >>= 8;
      t |= table->pixel_values[2][b] << 1;
      b = RedIntensity[input[21]] + GreenIntensity[input[22]] + BlueIntensity[input[23]];
      b >>= 8;
      t |= table->pixel_values[3][b];
#   endif
    *output++ = t;
    input += 24;
  }
}

static void
lf_gray_dither_24_2(
  cct_8_8_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x;
  for(x = pixel_count >> 2; x; x--)
  {
    unsigned int t;
    unsigned int b;
#   ifdef CHIMERA_LITTLE_ENDIAN
      b = RedIntensity[input[0]] + GreenIntensity[input[1]] + BlueIntensity[input[2]];
      b >>= 8;
      t = table->pixel_values[0][b];
      b = RedIntensity[input[3]] + GreenIntensity[input[4]] + BlueIntensity[input[5]];
      b >>= 8;
      t |= table->pixel_values[1][b] << 2;
      b = RedIntensity[input[6]] + GreenIntensity[input[7]] + BlueIntensity[input[8]];
      b >>= 8;
      t |= table->pixel_values[2][b] << 4;
      b = RedIntensity[input[9]] + GreenIntensity[input[10]] + BlueIntensity[input[11]];
      b >>= 8;
      t |= table->pixel_values[3][b] << 6;
#   else
      b = RedIntensity[input[0]] + GreenIntensity[input[1]] + BlueIntensity[input[2]];
      b >>= 8;
      t = table->pixel_values[0][b] << 6;
      b = RedIntensity[input[3]] + GreenIntensity[input[4]] + BlueIntensity[input[5]];
      b >>= 8;
      t |= table->pixel_values[1][b] << 4;
      b = RedIntensity[input[6]] + GreenIntensity[input[7]] + BlueIntensity[input[8]];
      b >>= 8;
      t |= table->pixel_values[2][b] << 2;
      b = RedIntensity[input[9]] + GreenIntensity[input[10]] + BlueIntensity[input[11]];
      b >>= 8;
      t |= table->pixel_values[3][b];
#   endif
    *output++ = t;
    input += 12;
  }
}

static void
lf_gray_dither_24_4(
  cct_8_8_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x;
  int xx = 0;
  for(x = pixel_count >> 1; x; x--, xx += 2)
  {
    unsigned int t;
    unsigned int b;
    xx &= 3;
#   ifdef CHIMERA_LITTLE_ENDIAN
      b = RedIntensity[input[0]] + GreenIntensity[input[1]] + BlueIntensity[input[2]];
      b >>= 8;
      t = table->pixel_values[xx][b];
      b = RedIntensity[input[3]] + GreenIntensity[input[4]] + BlueIntensity[input[5]];
      b >>= 8;
      t |= table->pixel_values[xx+1][b] << 4;
#   else
      b = RedIntensity[input[0]] + GreenIntensity[input[1]] + BlueIntensity[input[2]];
      b >>= 8;
      t = table->pixel_values[xx][b] << 4;
      b = RedIntensity[input[3]] + GreenIntensity[input[4]] + BlueIntensity[input[5]];
      b >>= 8;
      t |= table->pixel_values[xx+1][b];
#   endif
    *output++ = t;
    input += 6;
  }
}

static void
lf_gray_dither_24_8(
  cct_8_8_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x;
  int xx = 0;
  for(x = pixel_count; x; x--, xx++)
  {
    unsigned char t;
    unsigned int b;
    xx &= 3;
    b = RedIntensity[input[0]] + GreenIntensity[input[1]] + BlueIntensity[input[2]];
    b >>= 8;
    t = table->pixel_values[xx][b];
    *output++ = t;
    input += 3;
  }
}

static void
lf_gray_dither_24_16(
  cct_8_8_dither_table table,
  unsigned char *input,
  unsigned short *output,
  int pixel_count)
{
  int x;
  int xx = 0;
  for(x = pixel_count >> 1; x; x--, xx++)
  {
    unsigned char t;
    unsigned int b;
    xx &= 3;
    b = RedIntensity[input[0]] + GreenIntensity[input[1]] + BlueIntensity[input[2]];
    b >>= 8;
    t = table->pixel_values[xx][b];
    *output++ = t;
    input += 3;
  }
}

/*
 * Takes 24 bit input and dithers to gray
 */

void
ddf_gray_dither_line_24(
  cct_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int output_bits_per_pixel,
  int pixel_count)
{
  switch(output_bits_per_pixel)
  {
  case 1:
    while(pixel_count & 7) pixel_count++;
    lf_gray_dither_24_1(table.eight_8_dither, input, output, pixel_count);
    break;
  case 2:
    while(pixel_count & 3) pixel_count++;
    lf_gray_dither_24_2(table.eight_8_dither, input, output, pixel_count);
    break;
  case 4:
    if(pixel_count & 1) pixel_count++;
    lf_gray_dither_24_4(table.eight_8_dither, input, output, pixel_count);
    break;
  case 8:
    lf_gray_dither_24_8(table.eight_8_dither, input, output, pixel_count);
    break;
  case 16:
    lf_gray_dither_24_16(table.eight_8_dither, input,
			 (unsigned short *)output, pixel_count);
    break;
  default:
    /* see this in the debugger :-) */
    sprintf(output, "ERROR: color dither to %d bits attempted", output_bits_per_pixel);
    break;
  }
}


/************************************************************************/


static void
lf_dither_8_1(
  cct_8_8_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x;
  for(x = pixel_count >> 3; x; x--)
  {
    unsigned int t;
    unsigned int b;
#   ifdef CHIMERA_LITTLE_ENDIAN
      b = input[0];
      t = table->pixel_values[0][b];
      b = input[1];
      t |= table->pixel_values[1][b] << 1;
      b = input[2];
      t |= table->pixel_values[2][b] << 2;
      b = input[3];
      t |= table->pixel_values[3][b] << 3;
      b = input[4];
      t |= table->pixel_values[0][b] << 4;
      b = input[5];
      t |= table->pixel_values[1][b] << 5;
      b = input[6];
      t |= table->pixel_values[2][b] << 6;
      b = input[7];
      t |= table->pixel_values[3][b] << 7;
#   else
      b = input[0];
      t = table->pixel_values[0][b] << 7;
      b = input[1];
      t |= table->pixel_values[1][b] << 6;
      b = input[2];
      t |= table->pixel_values[2][b] << 5;
      b = input[3];
      t |= table->pixel_values[3][b] << 4;
      b = input[4];
      t |= table->pixel_values[0][b] << 3;
      b = input[5];
      t |= table->pixel_values[1][b] << 2;
      b = input[6];
      t |= table->pixel_values[2][b] << 1;
      b = input[7];
      t |= table->pixel_values[3][b];
#   endif
    *output++ = t;
    input += 8;
  }
}

static void
lf_dither_8_2(
  cct_8_8_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x;
  for(x = pixel_count >> 2; x; x--)
  {
    unsigned int t;
    unsigned int b;
#   ifdef CHIMERA_LITTLE_ENDIAN
      b = input[0];
      t = table->pixel_values[0][b];
      b = input[1];
      t |= table->pixel_values[1][b] << 2;
      b = input[2];
      t |= table->pixel_values[2][b] << 4;
      b = input[3];
      t |= table->pixel_values[3][b] << 6;
#   else
      b = input[0];
      t = table->pixel_values[0][b] << 6;
      b = input[1];
      t |= table->pixel_values[1][b] << 4;
      b = input[2];
      t |= table->pixel_values[2][b] << 2;
      b = input[3];
      t |= table->pixel_values[3][b];
#   endif
    *output++ = t;
    input += 4;
  }
}

static void
lf_dither_8_4(
  cct_8_8_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x;
  int xx = 0;
  for(x = pixel_count >> 1; x; x--, xx += 2)
  {
    unsigned int t;
    unsigned int b;
    xx &= 3;
#   ifdef CHIMERA_LITTLE_ENDIAN
      b = input[0];
      t = table->pixel_values[xx][b];
      b = input[1];
      t |= table->pixel_values[xx+1][b] << 4;
#   else
      b = input[0];
      t = table->pixel_values[xx][b] << 4;
      b = input[1];
      t |= table->pixel_values[xx+ 1][b];
#   endif
    *output++ = t;
    input += 2;
  }
}

/*
 * This is it! This is where Mr. averages GIFs get dithered for his
 * his 8-bit pseudocolor screen!
 */

static void
lf_dither_8_8(
  cct_8_8_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x;
  int xx = 0;

  for(x = pixel_count; x; x--, xx++)
  {
    unsigned int t;
    unsigned int b;
    xx &= 3;
    b = input[0];
    t = table->pixel_values[xx][b];
    *output++ = t;
    input ++;
  }
}

static void
lf_dither_8_16(
  cct_8_8_dither_table table,
  unsigned char *input,
  unsigned short *output,
  int pixel_count)
{
  int x;
  int xx = 0;
  for(x = pixel_count; x; x--, xx++)
  {
    unsigned int t;
    unsigned int b;
    xx &= 3;
    b = input[0];
    t = table->pixel_values[xx][b];
    *output++ = t;
    input++;
  }
}

/*
 * takes 8 bit input and dithers
 */

void
ddf_dither_line_8(
  cct_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int output_bits_per_pixel,
  int pixel_count)
{
  switch(output_bits_per_pixel)
  {
  case 1:
    while(pixel_count & 7) pixel_count++;
    lf_dither_8_1(table.eight_8_dither, input, output, pixel_count);
    break;
  case 2:
    while(pixel_count & 3) pixel_count++;
    lf_dither_8_2(table.eight_8_dither, input, output, pixel_count);
    break;
  case 4:
    if(pixel_count & 1) pixel_count++;
    lf_dither_8_4(table.eight_8_dither, input, output, pixel_count);
    break;
  case 8:
    lf_dither_8_8(table.eight_8_dither, input, output, pixel_count);
    break;
  case 16:
    lf_dither_8_16(table.eight_8_dither, input, (unsigned short *)output, pixel_count);
    break;
  default:
    /* see this in the debugger :-) */
    sprintf(output, "ERROR: color dither to %d bits attempted", output_bits_per_pixel);
    break;
  }
}


/************************************************************************/

static void
lf_convert_8_4(
  cct_8_true_conversion_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x;
  for(x = pixel_count >> 1; x; x--)
  {
    unsigned int b;
#   ifdef CHIMERA_LITTLE_ENDIAN
      b = table->pixel_values[input[0]];
      b |= table->pixel_values[input[1]] << 4;
#   else
      b = table->pixel_values[input[0]] << 4;
      b |= table->pixel_values[input[1]];
#   endif
    *output++ = b;
    input++;
  }
}

static void
lf_convert_8_8(
  cct_8_true_conversion_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x;
  for(x = pixel_count; x; x--)
  {
    output[0] = table->pixel_values[input[0]];
    output++;
    input++;
  }
}

static void
lf_convert_8_16(
  cct_8_true_conversion_table table,
  unsigned char *input,
  unsigned short *output,
  int pixel_count)
{
  int x;
  for(x = pixel_count; x; x--)
  {
    output[0] = table->pixel_values[input[0]];
    output++;
    input++;
  }
}

static void
lf_convert_8_24(
  cct_8_true_conversion_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x;
  for(x = pixel_count; x; x--)
  {
    unsigned int t = table->pixel_values[input[0]];
#   ifdef CHIMERA_LITTLE_ENDIAN
      output[0] = t & 255;
      output[1] = (t >> 8) & 255;
      output[2] = (t >> 16) & 255;
#   else
      output[0] = (t >> 16) & 255;
      output[1] = (t >> 8) & 255;
      output[2] = t & 255;
#   endif
    output += 3;
    input++;
  }
}


static void
lf_convert_8_32(
  cct_8_true_conversion_table table,
  unsigned char *input,
  unsigned int *output,
  int pixel_count)
{
  int x;
  for(x = pixel_count; x; x--)
  {
    output[0] = table->pixel_values[input[0]];
    output++;
    input++;
  }
}

/*
 * takes 8 bit input and converts
 */

void
ddf_convert_line_8(
  cct_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int output_bits_per_pixel,
  int pixel_count)
{
  switch(output_bits_per_pixel)
  {
  case 4:
    if(pixel_count & 1) pixel_count++;
    lf_convert_8_4(table.eight_true_conversion, input, output, pixel_count);
    break;
  case 8:
    lf_convert_8_8(table.eight_true_conversion, input, output, pixel_count);
    break;
  case 16:
    lf_convert_8_16(table.eight_true_conversion, input,
		    (unsigned short *)output, pixel_count);
    break;
  case 24:
    lf_convert_8_24(table.eight_true_conversion, input, output, pixel_count);
    break;
  case 32:
    lf_convert_8_32(table.eight_true_conversion, input,
		    (unsigned int *)output, pixel_count);
    break;
  default:
    /* see this in the debugger :-) */
    sprintf(output, "ERROR: color dither to %d bits attempted", output_bits_per_pixel);
    break;
  }
}


/************************************************************************/


static void
lf_convert_24_16(
  cct_true_true_conversion_table table,
  unsigned char *input,
  unsigned short *output,
  int pixel_count)
{
  int x;
  for(x = pixel_count; x; x--)
  {
    unsigned int o;
    o = table->pixel_values[0][input[0]];
    o += table->pixel_values[1][input[1]];
    o += table->pixel_values[2][input[2]];
    *output++ = o;
    input += 3;
  }
}

static void
lf_convert_24_32(
  cct_true_true_conversion_table table,
  unsigned char *input,
  unsigned int *output,
  int pixel_count)
{
  int x;
  for(x = pixel_count; x; x--)
  {
    unsigned int o;
    o = table->pixel_values[0][input[0]];
    o += table->pixel_values[1][input[1]];
    o += table->pixel_values[2][input[2]];
    *output++ = o;
    input += 3;
  }
}

static void
lf_convert_24_24(
  cct_true_true_conversion_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x;
  for(x = pixel_count; x; x--)
  {
    unsigned int t = table->pixel_values[0][input[0]];
    t += table->pixel_values[1][input[1]];
    t += table->pixel_values[2][input[2]];
#   ifdef CHIMERA_LITTLE_ENDIAN
      output[0] = t & 255;
      output[1] = (t >> 8) & 255;
      output[2] = (t >> 16) & 255;
#   else
      output[0] = (t >> 16) & 255;
      output[1] = (t >> 8) & 255;
      output[2] = t & 255;
#   endif
    output += 3;
    input += 3;
  }
}


/*
 * takes 24 bit input and converts
 */

void
ddf_convert_line_24(
  cct_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int output_bits_per_pixel,
  int pixel_count)
{
  switch(output_bits_per_pixel)
  {
  case 16:
    lf_convert_24_16(table.true_true_conversion, input,
		     (unsigned short *)output, pixel_count);
    break;
  case 24:
    lf_convert_24_24(table.true_true_conversion, input, output, pixel_count);
    break;
  case 32:
    lf_convert_24_32(table.true_true_conversion, input,
		     (unsigned int *)output, pixel_count);
    break;
  default:
    /* see this in the debugger :-) */
    sprintf(output, "ERROR: color dither to %d bits attempted", output_bits_per_pixel);
    break;
  }
}


/************************************************************************/

static void
lf_gray_convert_24_4(
  cct_8_true_conversion_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x;
  for(x = pixel_count >> 1; x; x--)
  {
    unsigned int b;
#   ifdef CHIMERA_LITTLE_ENDIAN
      b = table->pixel_values[(RedIntensity[input[0]] +
                               GreenIntensity[input[1]] +
                               BlueIntensity[input[2]]) >> 8];
      b |= table->pixel_values[(RedIntensity[input[3]] +
                                GreenIntensity[input[4]] +
                                BlueIntensity[input[5]]) >> 8] << 4;
#   else
      b = table->pixel_values[(RedIntensity[input[0]] +
                               GreenIntensity[input[1]] +
                               BlueIntensity[input[2]]) >> 8] << 4;
      b |= table->pixel_values[(RedIntensity[input[3]] +
                                GreenIntensity[input[4]] +
                                BlueIntensity[input[5]]) >> 8];
#   endif
    b >>= 8;
    *output++ = b;
    input+= 6;
  }
}

static void
lf_gray_convert_24_8(
  cct_8_true_conversion_table table,
  unsigned char *input,
  unsigned char *output,
  int pixel_count)
{
  int x;
  for(x = pixel_count; x; x--)
  {
    unsigned int b =
      RedIntensity[input[0]] + GreenIntensity[input[1]] + BlueIntensity[input[2]];
    b >>= 8;
    *output++ = table->pixel_values[b];
    input+= 3;
  }
}


static void
lf_gray_convert_24_16(
  cct_8_true_conversion_table table,
  unsigned char *input,
  unsigned short *output,
  int pixel_count)
{
  int x;
  for(x = pixel_count; x; x--)
  {
    unsigned int b =
      RedIntensity[input[0]] + GreenIntensity[input[1]] + BlueIntensity[input[2]];
    b >>= 8;
    *output++ = table->pixel_values[b];
    input+= 3;
  }
}
/*
 * Takes 24 bit input and converts to gray
 */

void
ddf_gray_convert_line_24(
  cct_dither_table table,
  unsigned char *input,
  unsigned char *output,
  int output_bits_per_pixel,
  int pixel_count)
{
  switch(output_bits_per_pixel)
  {
  case 4:
    if(pixel_count & 1) pixel_count++;
    lf_gray_convert_24_4(table.eight_true_conversion, input,
			 output, pixel_count);
    break;
  case 8:
    lf_gray_convert_24_8(table.eight_true_conversion, input,
			 output, pixel_count);
    break;
  case 16:
    lf_gray_convert_24_16(table.eight_true_conversion, input,
			  (unsigned short *)output, pixel_count);
    break;
  default:
    /* see this in the debugger :-) */
    sprintf(output, "ERROR: color dither to %d bits attempted", output_bits_per_pixel);
    break;
  }
}


/************************************************************************/

