/*
 * xbm.c:
 *
 * (c) 1995 Erik Corry ehcorry@inet.uni-c.dk
 *
 * modelled on gif.c.
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

#include <stdlib.h>
#include <string.h>

#include "port_after.h"

#include "common.h"

#include "image_endian.h"
#include "imagep.h"
#include "xbmp.h"

static byte lc_reverse_byte[] =
{
0,  128, 64, 192, 32, 160, 96,  224, 16, 144, 80, 208, 48, 176, 112, 240,
8,  136, 72, 200, 40, 168, 104, 232, 24, 152, 88, 216, 56, 184, 120, 248,
4,  132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212, 52, 180, 116, 244,
12, 140, 76, 204, 44, 172, 108, 236, 28, 156, 92, 220, 60, 188, 124, 252,
2,  130, 66, 194, 34, 162, 98,  226, 18, 146, 82, 210, 50, 178, 114, 242,
10, 138, 74, 202, 42, 170, 106, 234, 26, 154, 90, 218, 58, 186, 122, 250,
6,  134, 70, 198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182, 118, 246,
14, 142, 78, 206, 46, 174, 110, 238, 30, 158, 94, 222, 62, 190, 126, 254,
1,  129, 65, 193, 33, 161, 97,  225, 17, 145, 81, 209, 49, 177, 113, 241,
9,  137, 73, 201, 41, 169, 105, 233, 25, 153, 89, 217, 57, 185, 121, 249,
5,  133, 69, 197, 37, 165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245,
13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93, 221, 61, 189, 125, 253,
3,  131, 67, 195, 35, 163, 99,  227, 19, 147, 83, 211, 51, 179, 115, 243,
11, 139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219, 59, 187, 123, 251,
7,  135, 71, 199, 39, 167, 103, 231, 23, 151, 87, 215, 55, 183, 119, 247,
15, 143, 79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255};

/*
 * xbmGetImage
 */

static Image *
xbmGetImage(void *pointer)
{
  xbmState *xbm = (xbmState *)pointer;
  return xbm->image;
}

/*
 * xbmDestroy
 */
static void
xbmDestroy(pointer)
void *pointer;
{
  xbmState *xbm = (xbmState *)pointer;
  if (xbm->image)
    freeImage(xbm->image);
  xbm->image = 0;
  if (xbm != NULL) free_mem(xbm);

  return;
}

static bool
lf_read_int(
  xbmState *xbm,
  byte *data,
  int len,
  int *returnval)
{
  byte *new_pos;
  long answer = strtol((char *)(data + xbm->pos), (char **)(&new_pos), 0);
  if(new_pos == data + xbm->pos) return true;
  xbm->pos = new_pos - data;
  *returnval = answer;
  return false;
}

static bool
lf_find_xbm_token(
  xbmState *xbm,
  byte *data,
  int len,
  byte **token_start_return,
  int *token_len_return)
{
  int oldpos=xbm->pos;
  int i;
  while (xbm->pos < len && data[xbm->pos] != ' ' && data[xbm->pos] != '\t' &&
         data[xbm->pos] != '\n' && data[xbm->pos] != '\r')
    xbm->pos++;
  if(xbm->pos >= len) return true;
  if(data[xbm->pos] == '\n' || data[xbm->pos] == '\r') return true;
  for(i = xbm->pos - 2; i >= oldpos; i--)
    if(data[i] == '_') break;
  if(i < oldpos) return true;
  *token_start_return = data + i + 1;
  *token_len_return = xbm->pos - i - 1;
  return false;
}

static bool
lf_find_define(
  xbmState *xbm,
  byte *data,
  int len)
{
  if(xbm->pos + 6 > len) return true;
  if(strncmp("define", data + xbm->pos, 6) != 0) return true;
  xbm->pos += 6;
  return false;
}

static bool
lf_skip_line(
  xbmState *xbm,
  byte *data,
  int len)
{
  while (xbm->pos < len && data[xbm->pos] != '\n' && data[xbm->pos] == '\r')
    xbm->pos++;
  if(xbm->pos >= len) return true;
  /*
   * Check for CR-LF line ending. Is this necessary???
   */
  if(data[xbm->pos] == '\r' && xbm->pos + 1 < len && data[xbm->pos + 1] == '\n')
    xbm->pos++;
  xbm->pos++;
  return false;
}

static bool
lf_skip_space_and_tabs(
  xbmState *xbm,
  byte *data,
  int len)
{
  while (xbm->pos < len && (data[xbm->pos] == ' ' || data[xbm->pos] == '\t'))
    xbm->pos++;
  if(xbm->pos >= len) return true;
  return false;
}

static int
lf_header_arrived(
  xbmState *xbm,
  byte *data,
  int len)
{
  byte *token_start;
  int token_len;
  int width, height;
  bool width_found = false;
  bool height_found = false;
  while(!width_found || !height_found)
  {
    while(xbm->pos < len && data[xbm->pos] != '#')
      lf_skip_line(xbm, data, len);
    xbm->pos++; /* step past # */
    if(xbm->pos >= len) return XBM_ERROR;
    if(lf_skip_space_and_tabs(xbm, data, len)) return XBM_ERROR;
    if(lf_find_define(xbm, data, len)) return XBM_ERROR;
    if(lf_skip_space_and_tabs(xbm, data, len)) return XBM_ERROR;
    if(lf_find_xbm_token(xbm, data, len, &token_start, &token_len)) return XBM_ERROR;
    if(lf_skip_space_and_tabs(xbm, data, len)) return XBM_ERROR;
    if(token_len == 5 && strncmp("width", token_start, token_len) == 0)
    {
      if(lf_read_int(xbm, data, len, &width)) return XBM_ERROR;
      if(width < 1 || width > 65535) return XBM_ERROR;
      width_found = true;
    }
    else if(token_len == 6 && strncmp("height", token_start, token_len) == 0)
    {
      if(lf_read_int(xbm, data, len, &height)) return XBM_ERROR;
      if(height < 1 || height > 65535) return XBM_ERROR;
      height_found = true;
    }
    lf_skip_line(xbm, data, len);
  }
  while(xbm->pos < len && data[xbm->pos] != '{')                       /* } */
    xbm->pos++;
  if(xbm->pos >= len) return XBM_ERROR;

  /*
   * Header successfully found. Now make image structure.
   */

  xbm->image = newBitImage(width, height);
  if (!xbm->image) return XBM_ERROR;
  xbm->imagepos = xbm->image->data;

  return XBM_SUCCESS;
}

static int
lf_read_header(
  xbmState *xbm,
  byte *data,
  int len)
{
  int i;
  for (i = xbm->pos; i < len-1; i++)
  {
    if(data[i] == '{')
    {
      int status = lf_header_arrived(xbm, data, len);
      xbm->state = XBM_READ_IMAGE;
      xbm->pos = i+1;
      return status;
    }
  }
  return XBM_NEED_DATA;
}

static void
lf_wrap_x(xbmState *xbm)
{ 
  if(xbm->xpos >= xbm->image->width)
  {
    if (xbm->lineProc != NULL)
    {
      (xbm->lineProc)(xbm->closure, xbm->ypos, xbm->ypos);
    }
    xbm->xpos = 0;
    xbm->ypos++;
    xbm->imagepos += xbm->image->bytes_per_line -
                       ((xbm->image->width + CHAR_BITS - 1) / CHAR_BITS);
  }
}

static bool
lf_skip_non_number(
  xbmState *xbm,
  byte *data,
  int len)
{
  byte *i;
  for(i = data + xbm->pos; i < data + len; i++)
    if((*i >= '0' && *i <= '9') ||
       (*i >= 'a' && *i <= 'z') ||
       (*i >= 'A' && *i <= 'Z'))
    {
      xbm->pos = i - data;
      return true;
    }
  return false;
}

static bool
lf_punctuation_ahead(
  xbmState *xbm,
  byte *data,
  int len)
{
  byte *i;
  for(i = data + xbm->pos; i < data + len; i++)
    if((*i < '0' || *i > '9') &&
       (*i < 'a' || *i > 'z') &&
       (*i < 'A' || *i > 'Z'))
      return true;
  return false;
}

static int
lf_read_image(
  xbmState *xbm,
  byte *data,
  int len)
{
  while(lf_skip_non_number(xbm, data, len) &&
        lf_punctuation_ahead(xbm, data, len))
  {
    int t;
    lf_wrap_x(xbm);
    if(xbm->ypos >= xbm->image->height)
      { xbm->state = XBM_FINISHED; return XBM_SUCCESS; }
    if(lf_read_int(xbm, data, len, &t)) return XBM_ERROR;
    t &= 255;
#   ifdef CHIMERA_BIG_ENDIAN
      t = lc_reverse_byte[t];
#   endif
    *xbm->imagepos++ = t;
    xbm->xpos += CHAR_BITS;
  }

  lf_wrap_x(xbm);
  if(xbm->ypos >= xbm->image->height)
      { xbm->state = XBM_FINISHED; return XBM_SUCCESS; }

  return XBM_NEED_DATA;
}

/*
 * xbmAddData
 *
 * 0 success
 * 1 needs more data
 * -1 error
 *
 * Assumes data is the address of the beginning of the xbm data and len
 * is the total length.
 */
static int
xbmAddData(pointer, data, len, data_ended)
void *pointer;
byte *data;
int len;
bool data_ended;
{
  xbmState *xbm = (xbmState *)pointer;
  int rval;

  for ( ; ; )
  {
    switch (xbm->state)
    {
      case XBM_READ_HEADER:
        rval = lf_read_header(xbm, data, len);
        break;
      case XBM_READ_IMAGE:
        rval = lf_read_image(xbm, data, len);
	break;
      case XBM_FINISHED:
        return 0;
    }
    if (rval == XBM_NEED_DATA)
    {
      if (data_ended) return(-1);
      return(1);
    }
    else if (rval != XBM_SUCCESS) return(-1);
  }

  return(-1);
}

/*
 * xbmInit
 *
 * Initialize XBM reader state
 */
void
xbmInit(lineProc, closure, if_vector)
void (*lineProc)();
void *closure;
struct ifs_vector *if_vector;
{
  xbmState *xbm;
  
  xbm = (xbmState *)alloc_mem(sizeof(xbmState));
  memset(xbm, 0, sizeof(xbmState));
  xbm->state = XBM_READ_HEADER;
  xbm->lineProc = lineProc;
  xbm->closure = closure;

  if_vector->initProc = &xbmInit;
  if_vector->destroyProc = &xbmDestroy;
  if_vector->addDataProc = &xbmAddData;
  if_vector->getImageProc = &xbmGetImage;

  if_vector->image_format_closure = (void *)xbm;
}
