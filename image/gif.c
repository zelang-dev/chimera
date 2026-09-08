/*
 * gif.c:
 *
 * Modified to be called so that GIFs can be decoded "on-the-fly".
 * Sort of.  Reparses lots of data it doesn't have to.
 * John Kilburg <john@cs.unlv.edu>
 *
 * adapted from code by kirk johnson (tuna@athena.mit.edu).  most of this
 * code is unchanged. -- jim frost 12.31.89
 *
 * gifin.c
 * kirk johnson
 * november 1989
 *
 * routines for reading GIF files
 *
 * Copyright 1989 Kirk L. Johnson
 *
 * Permission to use, copy, modify, distribute, and sell this
 * software and its documentation for any purpose is hereby granted
 * without fee, provided that the above copyright notice appear in
 * all copies and that both that copyright notice and this
 * permission notice appear in supporting documentation. The
 * author makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express
 * or implied warranty.
 *
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, INDIRECT
 * OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
#include "port_before.h"

#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "port_after.h"

#include "common.h"

#include "imagep.h"
#include "image_endian.h"
#include "image_format.h"
#include "gifp.h"

/* start line for interlacing */
static int interlace_start[4] = { 0, 4, 2, 1 };

/* rate at which we accelerate vertically */
static int interlace_rate[4] = { 8, 8, 4, 2 };

/* how often to copy the line */
static int interlace_copy[4] = { 8, 4, 2, 1 };

static int gs_read_image_header _ArgProto((gifState *));
static int gs_read_sig _ArgProto((gifState *));
static int gs_read_data_block _ArgProto((gifState *));
static int gs_read _ArgProto((gifState *, byte **, int));
static int gs_open_file _ArgProto((gifState *));
static int gs_open_image _ArgProto((gifState *));
static int gs_get_pixel _ArgProto((gifState *, int *));
static int gs_init_decoder _ArgProto((gifState *));

/*
 * read GIF data
 */
static int
gs_read(gs, b, blen)
gifState *gs;
byte **b;
int blen;
{
  if (gs->datalen < blen + gs->pos) return(0);

  *b = gs->data + gs->pos;
  gs->pos += blen;

  return(blen);
}

/*
 * gs_read_sig
 *
 */
static int
gs_read_sig(gs)
gifState *gs;
{
  byte *b;

  /* check GIF signature */
  if (gs_read(gs, &b, GIF_SIG_LEN) != GIF_SIG_LEN) return(GS_NEED_DATA);

  if ((strncmp((char *)b, GIF_SIG, strlen(GIF_SIG)) != 0) &&
      (strncmp((char *)b, GIF_SIG_89, strlen(GIF_SIG_89)) != 0))
  {
    return(GS_ERR_BAD_SIG);
  }

  gs->state = GS_OPEN_FILE;

  return(GS_SUCCESS);
}

/*
 * gs_open_file
 *
 * open a GIF file
 */
static int
gs_open_file(gs)
gifState *gs;
{
  byte *b;

  /* read screen descriptor */
  if (gs_read(gs, &b, GIF_SD_SIZE) != GIF_SD_SIZE) return(GS_NEED_DATA);

  /* decode screen descriptor */
  gs->rast_width   = (b[1] << 8) + b[0];
  gs->rast_height  = (b[3] << 8) + b[2];
  gs->g_cmap_flag  = (b[4] & 0x80) ? 1 : 0;
  gs->color_bits   = ((b[4] & 0x70) >> 4) + 1;
  gs->g_pixel_bits = (b[4] & 0x07) + 1;
  gs->bg_color     = b[5];

  /* load global colormap */
  if (gs->g_cmap_flag)
  {
    gs->g_ncolors = (1 << gs->g_pixel_bits);
    gs->g_ncolors_pos = 0;
    gs->state = GS_LOAD_G_CMAP;
  }
  else
  {
    gs->state = GS_OPEN_IMAGE;
    gs->g_ncolors_pos = 0;
    gs->g_ncolors = 0;
  }

  gs->have_dimensions = 1;

  return(GS_SUCCESS);
}

/*
 * gs_init_decoder
 *
 * This isn't a separate state.
 */
static int
gs_init_decoder(gs)
gifState *gs;
{
  int i;
  byte *b;

  if (gs_read(gs, &b, 1) != 1) return(GS_NEED_DATA);

  gs->root_size  = (int)(*b);
  gs->clr_code   = 1 << gs->root_size;
  gs->eoi_code   = gs->clr_code + 1;
  gs->table_size = gs->clr_code + 2;
  gs->code_size  = gs->root_size + 1;
  gs->code_mask  = (1 << gs->code_size) - 1;
  gs->work_bits  = 0;
  gs->work_data  = 0;
  gs->buf_idx    = 0;
  gs->buf_cnt    = 0;
  gs->first      = 0;

  /* initialize string table */
  for (i = 0; i < STAB_SIZE; i++)
  {
    gs->prefix[i] = NULL_CODE;
    gs->extnsn[i] = i;
  }
  
  /* initialize pixel stack */
  gs->pstk_idx = 0;
  
  gs->state = GS_MAKE_IMAGE;

  return(GS_SUCCESS);
}

/*
 * gs_open_image
 */
static int
gs_open_image(gs)
gifState *gs;
{
  int separator;
  byte *b;
  int rval;
  int pos = gs->pos;

  if (gs_read(gs, &b, 1) != 1) return(GS_NEED_DATA);

  separator = (int)(*b);
  if (separator == GIF_EXTENSION)
  {
    /* get the extension function byte */
    if (gs_read(gs, &b, 1) != 1)
    {
      gs->pos = pos;
      return(GS_NEED_DATA);
    }
    
    if (*b == 0xf9)
    {
      if ((rval = gs_read_data_block(gs)) != GS_SUCCESS)
      {
        gs->pos = pos;
        return(rval);
      }
      if (gs->buf[0] & 0x1) gs->transparent = gs->buf[3];
    }
    else
    {
      if ((rval = gs_read_data_block(gs)) != GS_SUCCESS)
      {
       gs->pos = pos;
       return(rval);
      }
    }

    return(GS_SUCCESS);
  }
  else if (separator == GIF_TERMINATOR) return(GS_DONE);
  /*
   * If it's a broken GIF, just keep scanning till we meet a
   * separator we recognise.
   */
  else if (separator != GIF_SEPARATOR) return(GS_SUCCESS);

  gs->state = GS_READ_IMAGE_HEADER;

  return(GS_SUCCESS);
}

/*
 * gs_read_image_header
 */
static int
gs_read_image_header(gs)
gifState *gs;
{
  byte *b;

  /* read image descriptor */
  if (gs_read(gs, &b, GIF_ID_SIZE) != GIF_ID_SIZE) return(GS_NEED_DATA);

  /* decode image descriptor */
  gs->img_left       = (b[1] << 8) + b[0];
  gs->img_top        = (b[3] << 8) + b[2];
  gs->img_width      = (b[5] << 8) + b[4];
  gs->img_height     = (b[7] << 8) + b[6];
  gs->l_cmap_flag    = (b[8] & 0x80) ? 1 : 0;
  gs->interlace_flag = (b[8] & 0x40) ? 1 : 0;
  gs->l_pixel_bits   = (b[8] & 0x07) + 1;

  /* load local colormap */
  if (gs->l_cmap_flag)
  {
    gs->l_ncolors = (1 << gs->l_pixel_bits);
    gs->l_ncolors_pos = 0;
    gs->state = GS_LOAD_L_CMAP;
  }
  else
  {
    gs->l_ncolors = 0;
    gs->l_ncolors_pos = 0;
    gs->state = GS_INIT_DECODER;
  }

  return(GS_SUCCESS);
}

/*
 * gs_load_l_cmap
 *
 * load a local colormap from the input stream
 */
static int
gs_load_l_cmap(gs)
gifState *gs;
{
  byte *b;

  if (gs_read(gs, &b, 3) != 3) return(GS_NEED_DATA);

  gs->l_cmap[GIF_RED][gs->l_ncolors_pos] = b[GIF_RED] << 8;
  gs->l_cmap[GIF_GRN][gs->l_ncolors_pos] = b[GIF_GRN] << 8;
  gs->l_cmap[GIF_BLU][gs->l_ncolors_pos] = b[GIF_BLU] << 8;

  gs->l_ncolors_pos++;
  if (gs->l_ncolors_pos == gs->l_ncolors) gs->state = GS_INIT_DECODER;

  return(GS_SUCCESS);
}
 
/*
 * gs_load_g_cmap
 *
 * load a global colormap from the input stream
 */
static int
gs_load_g_cmap(gs)
gifState *gs;
{
  byte *b;

  if (gs_read(gs, &b, 3) != 3) return(GS_NEED_DATA);
    
  gs->g_cmap[GIF_RED][gs->g_ncolors_pos] = b[GIF_RED] << 8;
  gs->g_cmap[GIF_GRN][gs->g_ncolors_pos] = b[GIF_GRN] << 8;
  gs->g_cmap[GIF_BLU][gs->g_ncolors_pos] = b[GIF_BLU] << 8;

  gs->g_ncolors_pos++;
  if (gs->g_ncolors_pos == gs->g_ncolors) gs->state = GS_OPEN_IMAGE;

  return(GS_SUCCESS);
}
 
/*
 * gs_read_data_block
 *
 * read a new data block from the input stream
 */
static int
gs_read_data_block(gs)
gifState *gs;
{
  byte *b;
  int cnt;
  int pos = gs->pos;

  /* read the data block header */
  if (gs_read(gs, &b, 1) != 1) return(GS_NEED_DATA);
  cnt = (int)(*b);

  /* read the data block body */
  if (gs_read(gs, &gs->buf, cnt) != cnt)
  {
    gs->pos = pos;
    return(GS_NEED_DATA);
  }

  gs->buf_idx = 0;
  gs->buf_cnt = cnt;

  return(GS_SUCCESS);
}

/*
 * gs_make_image
 */
static int
gs_make_image(gs)
gifState *gs;
{
  Image *image;
  int i;
  bool gray = true;

  if (gs->l_cmap_flag)
  {
    image = newRGBImage(gs->img_width, gs->img_height, gs->l_pixel_bits);
    if (!image) return(GS_ERR_NOMEM);

    image->rgb.red = gs->l_cmap[GIF_RED];
    image->rgb.green = gs->l_cmap[GIF_GRN];
    image->rgb.blue = gs->l_cmap[GIF_BLU];
    image->rgb.used = gs->l_ncolors;
  }
  else
  {
    image = newRGBImage(gs->img_width, gs->img_height, gs->g_pixel_bits);
    if (!image) return(GS_ERR_NOMEM);

    image->rgb.red = gs->g_cmap[GIF_RED];
    image->rgb.green = gs->g_cmap[GIF_GRN];
    image->rgb.blue = gs->g_cmap[GIF_BLU];  
    image->rgb.used = gs->g_ncolors;
  }

  for(i = 0; i < image->rgb.used; i++)
    if(image->rgb.red[i] >> 8 != image->rgb.green[i] >> 8 ||
       image->rgb.red[i] >> 8 != image->rgb.blue[i] >> 8)
    {
      gray = false;
      break;
    }
  if(gray) image->type = IGRAY;

  image->transparent = gs->transparent;
  gs->image = image;
  gs->state = GS_DECODE_DATA;
  image->x = 0;
  image->pass = 0;
  if (gs->interlace_flag) image->y = interlace_start[image->pass];
  else image->y = 0;

  return(GS_SUCCESS);
}

/*
 * gs_get_pixel
 *
 * try to read next pixel from the raster, return result in *pel
 */
static int
gs_get_pixel(gs, pel)
gifState *gs;
int *pel;
{
  int code;
  int rval;

  /* decode until there are some pixels on the pixel stack */
  while (gs->pstk_idx == 0)
  {
    while (gs->work_bits < gs->code_size)
    {
      int pos = gs->pos;

      if (gs->buf_idx == gs->buf_cnt)
      {
	if ((rval = gs_read_data_block(gs)) != GS_SUCCESS)
	{
	  gs->pos = pos;
	  return(rval);
        }
      }
      
      gs->work_data |= ((unsigned) gs->buf[gs->buf_idx++]) << gs->work_bits;
      gs->work_bits += 8;
    }
    
    /* get the next code */
    code            = gs->work_data & gs->code_mask;
    gs->work_data >>= gs->code_size;
    gs->work_bits  -= gs->code_size;
    
    /* interpret the code */
    if (code > gs->table_size || code == gs->eoi_code)
    {
    /* Format Error */
      return(GS_ERR_EOF);
    }

    if (code == gs->clr_code)
    {
    /* reset decoder */
      gs->code_size  = gs->root_size + 1;
      gs->code_mask  = (1 << gs->code_size) - 1;
      gs->table_size = gs->clr_code + 2;
      gs->prev_code  = NULL_CODE;
      continue;
    }

    if (gs->prev_code == NULL_CODE)
    {
      gs->pstk[gs->pstk_idx++] = gs->extnsn[code];
      gs->prev_code = code;
      gs->first = code;
      continue;
    }
    else
    {
      int in_code = code;

      if (code == gs->table_size)
      {
        gs->pstk[gs->pstk_idx++] = gs->first;
        code = gs->prev_code;
      }
      while (code > gs->clr_code)
      {
        gs->pstk[gs->pstk_idx++] = gs->extnsn[code];
        code = gs->prefix[code];
      }
      gs->first = gs->extnsn[code];
      /* Add a new string to the string table. */
      if (gs->table_size >= PSTK_SIZE)
        return(GS_ERR_EOF);
      gs->pstk[gs->pstk_idx++] = gs->first;
      gs->prefix[gs->table_size] = gs->prev_code;
      gs->extnsn[gs->table_size] = gs->first;
      gs->table_size ++;
      if (((gs->table_size & gs->code_mask)) == 0
               && (gs->table_size < PSTK_SIZE))
      {
        gs->code_size ++;
        gs->code_mask += gs->table_size;
      }
      gs->prev_code = in_code;
    }
  }

  *pel = (int)gs->pstk[--gs->pstk_idx];

  return(GS_SUCCESS);
}

/*
 * gs_decode_data
 *
 * TODO: This routine looks like something of a performance catastrophe
 */
static int
gs_decode_data(gs)
gifState *gs;
{
  Image *image = gs->image;
  int pixel;
  byte *pixptr;
  int rval;
  int xpix;

  if (gs->interlace_flag)
  {
    if (image->x == image->width)
    {
      image->y += interlace_rate[image->pass];
      while (image->y >= image->height)
      {
	image->pass++;
	if (image->pass == 4) return(GS_DONE);
	image->y = interlace_start[image->pass];
      }
      image->x = 0;
    }
  }
  else
  {
    if (image->x == image->width)
    {
      image->y++;
      image->x = 0;
    }
    if (image->y == image->height) return(GS_DONE);
  }

  xpix = image->y * image->bytes_per_line +
         (image->x * image->pixlen) / CHAR_BITS;

  if (xpix < image->height * image->bytes_per_line)
  {
    pixptr = image->data + xpix;
    if ((rval = gs_get_pixel(gs, &pixel)) != GS_SUCCESS) return(rval);
    if(image->pixlen == 1)
    {
      int bitpos = image->x & 7;
      if (bitpos == 0) *pixptr = 0;
#     ifdef CHIMERA_LITTLE_ENDIAN
      pixel <<= bitpos;
#     else
      pixel <<= 7 - bitpos;
#     endif
      *pixptr |= pixel;
    }
    else /* pixlen is 8 bits */
    {
      *pixptr = pixel;
    }
  }

  image->x++;

  if (image->x == image->width)
  {
    if (gs->interlace_flag && interlace_copy[image->pass] > 1)
    {
      byte *origline = image->data + image->y * image->bytes_per_line;
      byte *copyline = origline + image->bytes_per_line;
      int i;
      for (i = 1;
           i < interlace_copy[image->pass] && image->y + i < image->height;
           i++, copyline += image->bytes_per_line)
      {
        memcpy(copyline, origline, image->bytes_per_line);
      }
      if (gs->lineProc != NULL)
        (gs->lineProc)(gs->closure,
                       image->y,
                       MIN(image->y + interlace_copy[image->pass] - 1,
                           image->height - 1));
    }
    else
    {
      if (gs->lineProc != NULL)
        (gs->lineProc)(gs->closure, image->y, image->y);
    }
  }

  return(GS_SUCCESS);
}

/*
 * gifDestroy
 *
 * This doesn't do the whole job!
 */
static void
gifDestroy(pointer)
void *pointer;
{
  gifState *gs = (gifState *)pointer;
  if (gs->image)
    freeImage(gs->image);
  gs->image = 0;

  if (gs != NULL) free_mem(gs);

  return;
}

/*
 * gifAddData
 *
 * 0 success
 * 1 needs more data
 * -1 error
 *
 * Assumes data is the address of the beginning of the GIF data and len
 * is the total length.
 */
static int
gifAddData(pointer, data, len, data_ended)
void *pointer;
byte *data;
int len;
bool data_ended;
{
  gifState *gs = (gifState *)pointer;
  int rval;
  int pos;

  gs->data = data;
  gs->datalen = len;

  for ( ; ; )
  {
    pos = gs->pos; /* save in case we need to rewind */
    switch (gs->state)
    {
      case GS_READ_SIG:
        rval = gs_read_sig(gs);
        break;
      case GS_OPEN_FILE:
        rval = gs_open_file(gs);
	break;
      case GS_OPEN_IMAGE:
	if ((rval = gs_open_image(gs)) == GS_DONE) return(0);
	break;
      case GS_READ_IMAGE_HEADER:
	rval = gs_read_image_header(gs);
	break;
      case GS_LOAD_G_CMAP:
	rval = gs_load_g_cmap(gs);
	break;
      case GS_LOAD_L_CMAP:
	rval = gs_load_l_cmap(gs);
	break;
      case GS_MAKE_IMAGE:
	rval = gs_make_image(gs);
	break;
      case GS_DECODE_DATA:
	if ((rval = gs_decode_data(gs)) == GS_DONE) return(0);
	break;
      case GS_INIT_DECODER:
	rval = gs_init_decoder(gs);
	break;
      default:
	rval = GS_ERR_BAD_STATE;
    }
    if (rval == GS_NEED_DATA)
    {
      gs->pos = pos;
      if(data_ended) return -1;
      return(1);
    }
    else if (rval != GS_SUCCESS) return(-1);
  }

  return(-1);
}

static Image *
gifGetImage(void *pointer)
{
  gifState *gs = (gifState *)pointer;
  return gs->image;
}

/*
 * gifInit
 *
 * Initialize GIF reader state
 */
void
gifInit(lineProc, closure, if_vector)
FormatLineProc lineProc;
void *closure;
struct ifs_vector *if_vector;
{
  gifState *gs;
  
  gs = (gifState *)alloc_mem(sizeof(gifState));
  memset(gs, 0, sizeof(gifState));
  gs->state = GS_READ_SIG;
  gs->transparent = -1;
  gs->lineProc = lineProc;
  gs->closure = closure;
  
  if_vector->image_format_closure = (void *)gs;
  if_vector->initProc = &gifInit;
  if_vector->destroyProc = &gifDestroy;
  if_vector->addDataProc = &gifAddData;
  if_vector->getImageProc = &gifGetImage;
}




