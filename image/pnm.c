/*
 * pnm.c:
 *
 * (c) 1995 Erik Corry ehcorry@inet.uni-c.dk
 *
 * loosely modelled on gif.c.
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
#include "pnmp.h"

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
 * pnmDestroy
 */
static void
pnmDestroy(pointer)
void *pointer;
{
  pnmState *pnm = (pnmState *)pointer;
  if (pnm->free_input_table && pnm->input_table)
    free(pnm->input_table);
  pnm->input_table = 0;
  pnm->free_input_table = false;
  if (pnm->image)
    freeImage(pnm->image);
  pnm->image = 0;
  if (pnm != NULL) free_mem(pnm);

  return;
}

static int
lf_read_magic(
  pnmState *pnm,
  byte *data,
  int len)
{
  if(len < 2) return PNM_NEED_DATA;
  if(data[0] != 'P') return PNM_ERROR;
  if(data[1] < '1' || data[1] > '6') return PNM_ERROR;
  pnm->pnm_class = data[1] - '0';
  pnm->pos = 2;
  pnm->state = PNM_READ_WIDTH;
  return PNM_SUCCESS;
}

static void
lf_skip_line(
  pnmState *pnm,
  byte *data,
  int len)
{
  int pos = pnm->pos;
  while(pos < len && data[pos] != '\n' && data[pos] != '\r')
    pos++;

  /*
   * end of line not found?
   */
  if(pos >= len) return;

  /*
   * Check for CR-LF
   */
  if(pos < len-1 && data[pos] == '\r' && data[pos+1] == '\n')
  {
    pos += 2;
    pnm->pos = pos;
    return;
  }

  /*
   * Check for LF-CR (does this exist?)
   */
  if(pos < len-1 && data[pos] == '\n' && data[pos+1] == '\r')
  {
    pos += 2;
    pnm->mac_newlines = true;
    pnm->pos = pos;
    return;
  }

  /*
   * Check for LF-endofdata or CR-endofdata
   */
  if(pos >= len-1 && (data[pos] == '\n' || data[pos] == '\r'))
  {
    return;
  }

  /*
   * Check for CR alone
   */
  if(data[pos] == '\r')
    pnm->mac_newlines = true;

  pnm->pos = pos + 1;
  return;
}

static int
lf_skip_whitespace(
  pnmState *pnm,
  byte *data,
  int len)
{
  while(pnm->pos < len)
  {
    while(data[pnm->pos] == ' ' || 
          data[pnm->pos] == '\n' || 
          data[pnm->pos] == '\r' || 
          data[pnm->pos] == '\t')
    {
      if(pnm->pos && data[pnm->pos] == '\r' && data[pnm->pos-1] != '\n')
        pnm->mac_newlines = true;
      pnm->pos++;
      if(pnm->pos >= len) return PNM_NEED_DATA;
    }
    if(data[pnm->pos] == '#')
    {
      int t = pnm->pos;
      lf_skip_line(pnm, data, len);
      if(t == pnm->pos) return PNM_NEED_DATA;
    }
    else
    {
      break;
    }
  }
  return PNM_SUCCESS;
}

static bool
lf_punctuation_ahead(
  pnmState *pnm,
  byte *data,
  int len)
{
  byte *i;
  for(i = data + pnm->pos; i < data + len; i++)
    if((*i < '0' || *i > '9') &&
       (*i < 'a' || *i > 'z') &&
       (*i < 'A' || *i > 'Z'))
      return true;
  return false;
}

static bool
lf_read_int(
  pnmState *pnm,
  byte *data,
  int len,
  int *returnval)
{
  byte *new_pos;
  long answer = strtol((char *)(data + pnm->pos), (char **)(&new_pos), 10);
  if(new_pos == data + pnm->pos) return true;
  pnm->pos = new_pos - data;
  *returnval = answer;
  return false;
}

static int
lf_read_width(
  pnmState *pnm,
  byte *data,
  int len)
{
  int a;
  if(lf_skip_whitespace(pnm, data, len) == PNM_NEED_DATA) return PNM_NEED_DATA;
  if(pnm->pos >= len) return PNM_NEED_DATA;
  if(!lf_punctuation_ahead(pnm, data, len)) return PNM_NEED_DATA;
  if(lf_read_int(pnm, data, len, &a)) return PNM_ERROR;
  pnm->width = a;
  pnm->state = PNM_READ_HEIGHT;
  return PNM_SUCCESS;
}

static int
lf_read_height(
  pnmState *pnm,
  byte *data,
  int len)
{
  int height;
  int i;
  if(lf_skip_whitespace(pnm, data, len) == PNM_NEED_DATA) return PNM_NEED_DATA;
  if(pnm->pos >= len) return PNM_NEED_DATA;
  if(!lf_punctuation_ahead(pnm, data, len)) return PNM_NEED_DATA;
  if(lf_read_int(pnm, data, len, &height)) return PNM_ERROR;
  switch(pnm->pnm_class)
  {
  case 1:
  case 4:
    pnm->image = newBitImage(pnm->width, height);
    if (!pnm->image) return PNM_ERROR;
    pnm->imagepos = pnm->image->data;
    if(pnm->pnm_class == 1) pnm->state = PNM_READ_ASC_BIT;
    else                    pnm->state = PNM_READ_PRE_RAW_NEWLINE;
#   ifdef CHIMERA_LITTLE_ENDIAN
      if(pnm->pnm_class == 1)
        pnm->input_table = 0; /* ascii bytes will be assembled l-e */
      else
        pnm->input_table = lc_reverse_byte; /* raw bytes are big-endian */
#   else
      if(pnm->pnm_class == 1)
        pnm->input_table = lc_reverse_byte; /* ascii bytes will be assembled l-e */
      else
        pnm->input_table = 0; /* raw bytes are big-endian */
#   endif
    break;
  case 2:
  case 5:
    pnm->image = newRGBImage(pnm->width, height, 8);
    if (!pnm->image) return PNM_ERROR;
    pnm->imagepos = pnm->image->data;
    pnm->image->type = IGRAY;
    pnm->image->rgb.red = pnm->cmap[0];
    pnm->image->rgb.green = pnm->cmap[1];
    pnm->image->rgb.blue = pnm->cmap[2];
    for(i = 0; i < 256; i++)
    {
      pnm->image->rgb.red[i] = i | (i << 8);
      pnm->image->rgb.green[i] = i | (i << 8);
      pnm->image->rgb.blue[i] = i | (i << 8);
    }
    pnm->state = PNM_READ_MAX_VAL;
    break;
  case 3:
  case 6:
    pnm->image = newTrueImage(pnm->width, height);
    if (!pnm->image) return PNM_ERROR;
    pnm->imagepos = pnm->image->data;
    pnm->state = PNM_READ_MAX_VAL;
    break;
  }
  return PNM_SUCCESS;
}

static int
lf_read_max_val(
  pnmState *pnm,
  byte *data,
  int len)
{
  int i;
  int max_val;
  if(lf_skip_whitespace(pnm, data, len) == PNM_NEED_DATA) return PNM_NEED_DATA;
  if(pnm->pos >= len) return PNM_NEED_DATA;
  if(!lf_punctuation_ahead(pnm, data, len)) return PNM_NEED_DATA;
  if(lf_read_int(pnm, data, len, &max_val)) return PNM_ERROR;

  if(max_val > 65535) return PNM_ERROR;
  if(max_val < 1) return PNM_ERROR;
  if(pnm->pnm_class > 3 && max_val > 255) return PNM_ERROR;

  pnm->max_val = max_val;
  if(pnm->pnm_class > 3)
    pnm->state = PNM_READ_PRE_RAW_NEWLINE;
  else
    pnm->state = PNM_READ_ASC;

  if(max_val == 255) return PNM_SUCCESS;

  if(pnm->pnm_class > 3)
    pnm->input_table = (byte *)calloc_mem(1, 256);
  else
    pnm->input_table = (byte *)calloc_mem(1, max_val);

  pnm->free_input_table = true;

  for(i = 0; i < max_val; i++)
    pnm->input_table[i] = 255.0 * (((double)i) / ((double)max_val) + 0.0001);

  return PNM_SUCCESS;
}

static int
lf_read_pre_raw_newline(
  pnmState *pnm,
  byte *data,
  int len)
{
  while(pnm->pos < len && data[pnm->pos] != '\n' && data[pnm->pos] != '\r')
    pnm->pos++;
  if(pnm->pos >= len) return PNM_NEED_DATA;
  if(pnm->mac_newlines && data[pnm->pos] == '\n')
  {
    if(pnm->pos >= len-1)
      return PNM_NEED_DATA;
    else
      pnm->pos++;
  }
  pnm->pos++;
  pnm->state = PNM_READ_RAW;
  return PNM_SUCCESS;
}

static int
lf_read_raw(
  pnmState *pnm,
  byte *data,
  int len)
{
  int bytes_per_line = pnm->image->width * pnm->image->pixlen;
  bytes_per_line += CHAR_BITS - 1;
  bytes_per_line /= CHAR_BITS;
  while(pnm->ypos < pnm->image->height && len - pnm->pos >= bytes_per_line)
  {
    int i;
    byte *imagepos = pnm->imagepos;
    byte *sourcepos = data + pnm->pos;
    byte *table = pnm->input_table;
  
    if(table) for(i = bytes_per_line; i; i--)
    {
      *imagepos++ = table[*sourcepos++];
    }
    else
    {
      memcpy(imagepos, sourcepos, bytes_per_line);
      imagepos += bytes_per_line;
      sourcepos += bytes_per_line;
    }
  
    pnm->imagepos += pnm->image->bytes_per_line;
    pnm->pos += bytes_per_line;
    if(pnm->lineProc) pnm->lineProc(pnm->closure, pnm->ypos, pnm->ypos);
    pnm->ypos++;
  }
  if(pnm->ypos < pnm->image->height) return PNM_NEED_DATA;
  pnm->state = PNM_FINISHED;
  return PNM_SUCCESS;
}

static int
lf_read_asc(
  pnmState *pnm,
  byte *data,
  int len)
{
  int bytes_per_line = pnm->image->width * pnm->image->pixlen;
  bytes_per_line += CHAR_BITS - 1;
  bytes_per_line /= CHAR_BITS;
  
  while(pnm->ypos < pnm->image->height)
  {
    if(lf_skip_whitespace(pnm, data, len) == PNM_NEED_DATA)
      return PNM_NEED_DATA;
    if(data[pnm->pos] == '#')
    {
      int t = pnm->pos;
      lf_skip_line(pnm, data, len);
      if(t == pnm->pos) return PNM_NEED_DATA;
      continue;
    }
    if(!lf_punctuation_ahead(pnm, data, len))
      return PNM_NEED_DATA;
    {
      int t;
      if(lf_read_int(pnm, data, len, &t)) return PNM_ERROR;
      if(t < 0) return PNM_ERROR;
      if(t > pnm->max_val) return PNM_ERROR;
      if(pnm->input_table) *pnm->imagepos++ = pnm->input_table[t];
      else *pnm->imagepos++ = t;
      pnm->xpos++;
    }
    if(pnm->xpos >= bytes_per_line)
    {
      pnm->xpos = 0;
      pnm->imagepos += pnm->image->bytes_per_line - bytes_per_line;
      if(pnm->lineProc) pnm->lineProc(pnm->closure, pnm->ypos, pnm->ypos);
      pnm->ypos++;
    }
  }
  pnm->state = PNM_FINISHED;
  return PNM_SUCCESS;
}

/*
 * This reads those 11010101 10 01101 10 10 101010-type pbm ascii bit
 * files. I really don't like this format, so I'm not going to put a
 * lot of effort into optimising this. It's an unashamed performance
 * disaster.
 */

static int
lf_read_asc_bit(
  pnmState *pnm,
  byte *data,
  int len)
{
  int bytes_per_line = pnm->image->width;
  bytes_per_line += CHAR_BITS - 1;
  bytes_per_line /= CHAR_BITS;

  while(pnm->ypos < pnm->image->height)
  {
    while(pnm->pos < len && data[pnm->pos] != '1' && data[pnm->pos] != '0')
      if(lf_skip_whitespace(pnm, data, len) == PNM_NEED_DATA)
        return PNM_NEED_DATA;
    if(pnm->pos >= len) return PNM_NEED_DATA;

    if(data[pnm->pos] == '1')
    {
#     ifdef CHIMERA_LITTLE_ENDIAN
        *pnm->imagepos |= 1 << (pnm->xpos % CHAR_BITS);
#     else
        *pnm->imagepos |= 1 << (7 - (pnm->xpos % CHAR_BITS));
#     endif
    }
    pnm->pos++;
    pnm->xpos++;
    if(pnm->xpos % CHAR_BITS == 0)
    {
      pnm->imagepos++;
    }
    if(pnm->xpos >= pnm->image->width)
    {
      pnm->imagepos += pnm->image->bytes_per_line - bytes_per_line;
      pnm->xpos = 0;
      if(pnm->lineProc) pnm->lineProc(pnm->closure, pnm->ypos, pnm->ypos);
      pnm->ypos++;
    }
  }
  pnm->state = PNM_FINISHED;
  return PNM_SUCCESS;
}

/*
 * pnmAddData
 *
 * 0 success
 * 1 needs more data
 * -1 error
 *
 * Assumes data is the address of the beginning of the pnm data and len
 * is the total length.
 */
static int
pnmAddData(pointer, data, len, data_ended)
void *pointer;
byte *data;
int len;
bool data_ended;
{
  pnmState *pnm = (pnmState *)pointer;
  int rval;

  for ( ; ; )
  {
    switch (pnm->state)
    {
      case PNM_READ_MAGIC:
        rval = lf_read_magic(pnm, data, len);
        break;
      case PNM_READ_WIDTH:
        rval = lf_read_width(pnm, data, len);
	break;
      case PNM_READ_HEIGHT:
        rval = lf_read_height(pnm, data, len);
	break;
      case PNM_READ_MAX_VAL:
        rval = lf_read_max_val(pnm, data, len);
	break;
      case PNM_READ_PRE_RAW_NEWLINE:
        rval = lf_read_pre_raw_newline(pnm, data, len);
	break;
      case PNM_READ_RAW:
        rval = lf_read_raw(pnm, data, len);
	break;
      case PNM_READ_ASC:
        rval = lf_read_asc(pnm, data, len);
	break;
      case PNM_READ_ASC_BIT:
        rval = lf_read_asc_bit(pnm, data, len);
	break;
      case PNM_FINISHED:
        return 0;
    }
    if (rval == PNM_NEED_DATA)
    {
      if(data_ended) return -1;
      return(1);
    }
    else if (rval != PNM_SUCCESS) return(-1);
  }

  return(-1);
}

static Image *
pnmGetImage(void *pointer)
{
  pnmState *pnm = (pnmState *)pointer;
  return pnm->image;
}


/*
 * pnmInit
 *
 * Initialize PNM reader state
 */
void
pnmInit(lineProc, closure, if_vector)
FormatLineProc lineProc;
void *closure;
struct ifs_vector *if_vector;
{
  pnmState *pnm;
  
  pnm = (pnmState *)alloc_mem(sizeof(pnmState));
  memset(pnm, 0, sizeof(pnmState));
  pnm->state = PNM_READ_MAGIC;
  pnm->lineProc = lineProc;
  pnm->closure = closure;

  if_vector->image_format_closure = (void *)pnm;
  if_vector->initProc = &pnmInit;
  if_vector->destroyProc = &pnmDestroy;
  if_vector->addDataProc = &pnmAddData;
  if_vector->getImageProc = &pnmGetImage;
}

