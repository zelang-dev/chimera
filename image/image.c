/*
 * image.c
 *
 * Copyright (c) 1995 Erik Corry ehcorry@inet.uni-c.dk
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
 *
 */

#include "port_before.h"

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>

#include "port_after.h"

#include "colorcube.h"
#include "dispdither.h"
#include "xcolorcube.h"
#include "image_endian.h"
#include "imagep.h"
#include "image_format.h"

#include "Chimera.h"
#include "ChimeraGUI.h"
#include "ChimeraRender.h"

#define TCOUNT 4

typedef void (*lt_expansion_fn) _ArgProto((byte *, byte *,
					   int, Intensity *, Intensity *,
					   Intensity *, int,
					   Intensity, Intensity,
					   Intensity));

#define IMAGE_GIF 0 
#define IMAGE_XBM 1
#define IMAGE_JPEG 2
#define IMAGE_PNG 3
#define IMAGE_PNM 5
#define IMAGE_UNKNOWN 6

static struct content_map
{
  char *name;
  int id;
} content_map[] =
{
  { "image/gif", IMAGE_GIF },
  { "image/xbm", IMAGE_XBM },
  { "image/pnm", IMAGE_PNM },
#ifdef HAVE_JPEG
  { "image/jpeg", IMAGE_JPEG },
#endif
#ifdef HAVE_PNG
  { "image/x-png", IMAGE_PNG },
  { "image/png", IMAGE_PNG },
#endif
  { "image/x-xbitmap", IMAGE_XBM },
  { "image/x-portable-anymap", IMAGE_PNM },
  { "image/x-portable-bitmap", IMAGE_PNM },
  { "image/x-portable-graymap", IMAGE_PNM },
  { "image/x-portable-pixmap", IMAGE_PNM },
  { NULL, IMAGE_UNKNOWN },
};

/*
 * Holds the infomation about a particular display
 */
typedef struct imagestate
{
  MemPool mp;

  /* X information */
  Display *dpy;
  Colormap cmap;
  int depth;
  Visual *v;
  Widget w;
  Window win;                        /* draw window */
  Pixel bg, fg;                      /* background, foreground for bitmaps */
  XColor bgcolor;

  XImage *xi;
  int last_line;
  int ref_count;
  char *hash;

  /* Generic Image decoding vars */
  cct_dither_table dither_table[TCOUNT];/* Table to dither/convert to screen */
  int free_dither_table;            /* How many tables were allocated (0-4) */
  ddt_dither_fn dither_function;    /* Function to dither/convert with */
  bool dithering;

  struct ifs_vector if_vector;      /* Image format decoder info */

  byte *expansion_buf;              /* Area for upgrading to bigger depth */
  lt_expansion_fn expansion_function; /* Func for upgrading to bigger depth */

  int expanded_type;                /* Image type after expansion */
  int image_special_count;          /* colors allocated for this image */
  int image_special_max;            /* max colors to alloc for this image */
  bool no_more_specials;
  bool special_blacklist[256];      /* already tried to allocate pixel once */
  bool special_real_blacklist[256]; /* really tried to allocate pixel once */

  /* control stuff */
  ChimeraSink wp;
  ChimeraGUI wd;
  struct imageclass *ic;
  struct imagestate *prev, *next;
} ImageState;

/*
 * Hold global information about the image module
 */
typedef struct imageclass
{
  MemPool     mp;
  int         icount;
  bool        init;
  GC          gc;
  int         refcount;

  /* Image decoding stuff */
  cct_cube    colorcube;            /* cuboid of allocated colors on screen */
  cct_cube    grayscale;            /* gray shades allocated on screen */
  cct_dither_table color_tables[TCOUNT]; /* color dither table */
  cct_dither_table gray_tables[TCOUNT];  /* color dither table */
  struct ccs_special_color special_colors[256]; /* specially alloc'd colors */
  int         special_count;
  int         special_max;
} ImageClass;

void SetSize _ArgProto((ImageState *));
static void ImageToXImage _ArgProto((void *, int, int));
static Image *lf_get_image _ArgProto((ImageState *));
static void lf_set_up_bitmap _ArgProto((ImageState *));
static void lf_set_up_expansion _ArgProto((ImageState *));
static byte *lf_get_8_bit_map _ArgProto((MemPool, Intensity *));
static bool lf_null_gray_map _ArgProto((byte **));
static void lf_set_up_dither _ArgProto((ImageState *));
static void lf_get_new_specials _ArgProto((ImageState *, ImageClass *,
					   Image *, byte *));
static void ImageClassInit _ArgProto((ImageClass *, Display *));
static void ImageAdd _ArgProto((void *));
static void ImageEnd _ArgProto((void *));
static void ImageDestroy _ArgProto((void *));
void *ImageInit _ArgProto((ChimeraRender, void *, void *));
static bool ImageExpose _ArgProto((void *, int, int,
				   unsigned int, unsigned int));

static byte *
lf_get_8_bit_map(mp, old_table)
MemPool mp;
Intensity *old_table;
{
  byte *answer = (byte *)MPCGet(mp, sizeof(byte) * 256);
  int i;

  for (i = 0; i < 256; i++)
    answer[i] = old_table[i] >> 8;

  return answer;
}

static bool
lf_null_gray_map(maps)
byte *maps[3];
{
  int i;
  for (i = 0; i < 256; i++)
  {
    if (maps[0][i] != maps[1][i]) return false;
    if (maps[2][i] != maps[1][i]) return false;
    if (maps[2][i] != i) return false;
  }
  return true;
}

static Image *
lf_get_image(is)
ImageState *is;
{
  if (is->if_vector.getImageProc && is->if_vector.image_format_closure)
    return(is->if_vector.getImageProc(is->if_vector.image_format_closure));
  return(NULL);
}

static void
lf_set_up_bitmap(is)
ImageState *is;
{
  Image *image = lf_get_image(is);
  ImageClass *ic = is->ic;
  bool really_allocated = false;
  int i;
  Intensity intensities[3];

  is->expanded_type = image->type;
  
  if(image->type == IBITMAP)
  {
    is->bg = is->bgcolor.pixel;
    is->fg = BlackPixel(is->dpy, DefaultScreen(is->dpy));
    return;
  }

  for(i = 0; i < 2; i++)
  {
    bool really_allocated_last_time = really_allocated;
    if(image->transparent != i)
    {
      intensities[0] = image->rgb.red[i];
      intensities[1] = image->rgb.green[i];
      intensities[2] = image->rgb.blue[i];
      if (!xccf_allocate_special(is->dpy,
				 is->cmap,
				 ic->colorcube,
				 ic->grayscale,
				 intensities,
				 &really_allocated,
				 /* don't-really-allocate-flag */
				 ic->special_count >= ic->special_max,
				 ic->special_colors,
				 ic->special_count,
				 i ? &(is->fg): &(is->bg)))
      {
        if(really_allocated_last_time)
        {
          XFreeColors(is->dpy,
		      is->cmap,
		      &(ic->special_colors[ic->special_count - 1].pixel),
		      1,
		      0);
	  ic->special_count--;
        }

	if(image->rgb.red[0] == image->rgb.green[0] &&
	   image->rgb.red[0] == image->rgb.blue[0] &&
	   image->rgb.red[1] == image->rgb.green[1] &&
	   image->rgb.red[1] == image->rgb.blue[1])
        {
          is->expanded_type = IGRAY;
	  is->expansion_buf = MPCGet(is->mp, image->width + 64);
        }
        else
        {
	  is->expanded_type = ITRUE;
	  is->expansion_buf = MPCGet(is->mp, (3 * image->width) + 192);
        }

	return;
      }
      if(really_allocated) ic->special_count++;
    }
    else
    {
      if (i) is->fg = is->bgcolor.pixel;
      else is->bg = is->bgcolor.pixel;
    }
  }

  return;
}

static void
lf_expand_bitmap_to_true(
  byte *expansion_buffer,
  byte *source_buffer,
  int width,
  Intensity *red_cmap,
  Intensity *green_cmap,
  Intensity *blue_cmap,
  int transparent,
  Intensity red_bg,
  Intensity green_bg,
  Intensity blue_bg)
{
  int i;
  byte red[2];
  byte green[2];
  byte blue[2];
  red[0] = red_cmap[0] >> 8;
  red[1] = red_cmap[1] >> 8;
  green[0] = green_cmap[0] >> 8;
  green[1] = green_cmap[1] >> 8;
  blue[0] = blue_cmap[0] >> 8;
  blue[1] = blue_cmap[1] >> 8;
  if(transparent != -1)
  {
    red[transparent] = red_bg >> 8;
    green[transparent] = green_bg >> 8;
    blue[transparent] = blue_bg >> 8;
  }
  for(i = (width + CHAR_BITS - 1) / CHAR_BITS; i; i--)
  {
    byte t = *source_buffer++;
#   ifdef CHIMERA_LITTLE_ENDIAN
      expansion_buffer[0] =   red[t & 1];
      expansion_buffer[1] = green[t & 1];
      expansion_buffer[2] =  blue[t & 1];
      expansion_buffer[3] =   red[t >> 1 & 1];
      expansion_buffer[4] = green[t >> 1 & 1];
      expansion_buffer[5] =  blue[t >> 1 & 1];
      expansion_buffer[6] =   red[t >> 2 & 1];
      expansion_buffer[7] = green[t >> 2 & 1];
      expansion_buffer[8] =  blue[t >> 2 & 1];
      expansion_buffer[9] =   red[t >> 3 & 1];
      expansion_buffer[10] = green[t >> 3 & 1];
      expansion_buffer[11] =  blue[t >> 3 & 1];
      expansion_buffer[12] =   red[t >> 4 & 1];
      expansion_buffer[13] = green[t >> 4 & 1];
      expansion_buffer[14] =  blue[t >> 4 & 1];
      expansion_buffer[15] =   red[t >> 5 & 1];
      expansion_buffer[16] = green[t >> 5 & 1];
      expansion_buffer[17] =  blue[t >> 5 & 1];
      expansion_buffer[18] =   red[t >> 6 & 1];
      expansion_buffer[19] = green[t >> 6 & 1];
      expansion_buffer[20] =  blue[t >> 6 & 1];
      expansion_buffer[21] =   red[t >> 7 & 1];
      expansion_buffer[22] = green[t >> 7 & 1];
      expansion_buffer[23] =  blue[t >> 7 & 1];
#   else
      expansion_buffer[0] =   red[t >> 7 & 1];
      expansion_buffer[1] = green[t >> 7 & 1];
      expansion_buffer[2] =  blue[t >> 7 & 1];
      expansion_buffer[3] =   red[t >> 6 & 1];
      expansion_buffer[4] = green[t >> 6 & 1];
      expansion_buffer[5] =  blue[t >> 6 & 1];
      expansion_buffer[6] =   red[t >> 5 & 1];
      expansion_buffer[7] = green[t >> 5 & 1];
      expansion_buffer[8] =  blue[t >> 5 & 1];
      expansion_buffer[9] =   red[t >> 4 & 1];
      expansion_buffer[10] = green[t >> 4 & 1];
      expansion_buffer[11] =  blue[t >> 4 & 1];
      expansion_buffer[12] =   red[t >> 3 & 1];
      expansion_buffer[13] = green[t >> 3 & 1];
      expansion_buffer[14] =  blue[t >> 3 & 1];
      expansion_buffer[15] =   red[t >> 2 & 1];
      expansion_buffer[16] = green[t >> 2 & 1];
      expansion_buffer[17] =  blue[t >> 2 & 1];
      expansion_buffer[18] =   red[t >> 1 & 1];
      expansion_buffer[19] = green[t >> 1 & 1];
      expansion_buffer[20] =  blue[t >> 1 & 1];
      expansion_buffer[21] =   red[t & 1];
      expansion_buffer[22] = green[t & 1];
      expansion_buffer[23] =  blue[t & 1];
#   endif
    expansion_buffer += 24;
  }
}

static void
lf_expand_bitmap_to_gray(
  byte *expansion_buffer,
  byte *source_buffer,
  int width,
  Intensity *red_cmap,
  Intensity *green_cmap,
  Intensity *blue_cmap,
  int transparent,
  Intensity red_bg,
  Intensity green_bg,
  Intensity blue_bg)
{
  int i;
  byte gray[2];
  gray[0] = red_cmap[0] >> 8;
  gray[1] = red_cmap[1] >> 8;
  if(transparent != -1)
    gray[transparent] = red_bg >> 8;

  for(i = (width + CHAR_BITS - 1) / CHAR_BITS; i; i--)
  {
    byte t = *source_buffer++;
#   ifdef CHIMERA_LITTLE_ENDIAN
      expansion_buffer[0] =   gray[t & 1];
      expansion_buffer[1] =   gray[t >> 1 & 1];
      expansion_buffer[2] =   gray[t >> 2 & 1];
      expansion_buffer[3] =   gray[t >> 3 & 1];
      expansion_buffer[4] =   gray[t >> 4 & 1];
      expansion_buffer[5] =   gray[t >> 5 & 1];
      expansion_buffer[6] =   gray[t >> 6 & 1];
      expansion_buffer[7] =   gray[t >> 7 & 1];
#   else
      expansion_buffer[0] =   gray[t >> 7 & 1];
      expansion_buffer[1] =   gray[t >> 6 & 1];
      expansion_buffer[2] =   gray[t >> 5 & 1];
      expansion_buffer[3] =   gray[t >> 4 & 1];
      expansion_buffer[4] =   gray[t >> 3 & 1];
      expansion_buffer[5] =   gray[t >> 2 & 1];
      expansion_buffer[6] =   gray[t >> 1 & 1];
      expansion_buffer[7] =   gray[t & 1];
#   endif
    expansion_buffer += 8;
  }
}


static void
lf_set_up_expansion(is)
ImageState *is;
{
  if(is->expanded_type == IGRAY)
    is->expansion_function = &lf_expand_bitmap_to_gray;
  else
    is->expansion_function = &lf_expand_bitmap_to_true;
}

static void
lf_set_up_dither(is)
ImageState *is;
{
  byte *maps[3];
  int i;
  cct_8_true_conversion_table cdt;
  ImageClass *ic = is->ic;
  Image *image = lf_get_image(is);
  int image_type;

  is->dithering = false;

  if(is->expansion_buf)
  {
    if(image->depth > 1) is->expanded_type = image_type = ITRUE;
    else image_type = is->expanded_type; /* lf_set_up_bitmap already set it */
  }
  else
  {
    image_type = image->type;
  }

  maps[0] = maps[1] = maps[2] = 0;

  if (image->type == IRGB || image->type == IGRAY)
  {
    maps[0] = lf_get_8_bit_map(is->mp, image->rgb.red);
    maps[1] = lf_get_8_bit_map(is->mp, image->rgb.green);
    maps[2] = lf_get_8_bit_map(is->mp, image->rgb.blue);
  }

  if (image->transparent != -1)
  {
    maps[0][image->transparent] = is->bgcolor.red >> 8;
    maps[1][image->transparent] = is->bgcolor.green >> 8;
    maps[2][image->transparent] = is->bgcolor.blue >> 8;    
  }

  /*
   * A real color dither/conversion?
   */
  if ((image_type == IRGB || image_type == ITRUE) && ic->colorcube)
  {
    if (image_type == IRGB && ic->colorcube->cube_type == cube_true_color)
    {
      /*
       * colormap to true-color conversion needed
       */
      if (!ic->color_tables[0].true_true_conversion)
      {
	ic->color_tables[0].true_true_conversion =
	    ccf_create_true_true_conversion_table(ic->colorcube);
      }
      cdt = ccf_true_true_to_8_true_conversion_table(
						     ic->color_tables[0].
						     true_true_conversion,
						     image->rgb.used,
						     maps);
      is->dither_table[0].eight_true_conversion = cdt;
      is->dither_table[1].eight_true_conversion = cdt;
      is->dither_table[2].eight_true_conversion = cdt;
      is->dither_table[3].eight_true_conversion = cdt;
      is->free_dither_table = 1;
      is->dither_function = &ddf_convert_line_8;
      is->dithering = false;
    }
    else if (image_type == IRGB &&
	     (ic->colorcube->cube_type == cube_mapping ||
	      ic->colorcube->cube_type == cube_no_mapping))
    {
      /*
       * 8-bit palette to 8-bit screen dither
       */
      if (!ic->color_tables[0].true_8_dither)
	  for (i = 0; i < TCOUNT; i++)
	      ic->color_tables[i].true_8_dither =
		  ccf_create_true_8_dither_table(i, 4, 4, ic->colorcube);
      for (i = 0; i < TCOUNT; i++)
      {
	is->dither_table[i].eight_8_dither =
	    ccf_true_8_to_8_8_dither_table(ic->color_tables[i].true_8_dither,
					   image->rgb.used, maps);
        if(image->transparent != -1)
	    ccf_set_specially_allocated(is->dither_table[i].eight_8_dither,
					image->transparent,
					is->bgcolor.pixel);
      }
      is->free_dither_table = TCOUNT;
      is->dither_function = &ddf_dither_line_8;
      is->dithering = true;
    }
    else if (image_type == ITRUE &&
	     ic->colorcube->cube_type == cube_true_color)
    {
      /*
       * true-true conversion job
       */
      if (!ic->color_tables[0].true_true_conversion)
	  ic->color_tables[0].true_true_conversion =
	      ccf_create_true_true_conversion_table(ic->colorcube);
      for (i = 0; i < TCOUNT; i++)
	  is->dither_table[i].true_true_conversion =
	      ic->color_tables[0].true_true_conversion;
      is->free_dither_table = 0; /* don't free it its a copied pointer!! */
      is->dither_function = &ddf_convert_line_24;
      is->dithering = false;
    }
    else if (image_type == ITRUE &&
	     (ic->colorcube->cube_type == cube_mapping ||
	      ic->colorcube->cube_type == cube_no_mapping))
    {
      /*
       * true-color to 8-bit dither-type-situation
       */
      if (!ic->color_tables[0].true_8_dither)
	  for (i = 0; i < TCOUNT; i++)
	      ic->color_tables[i].true_8_dither =
		  ccf_create_true_8_dither_table(i, 4, 4, ic->colorcube);
      for (i = 0; i < TCOUNT; i++)
	  is->dither_table[i].true_8_dither =
	      ic->color_tables[i].true_8_dither;
      is->free_dither_table = 0; /* don't free it its a copied pointer!! */
      is->dither_function = &ddf_color_dither_line_24;
      is->dithering = true;
    }
  }
  /*
   * else a black and white dither
   */
  else if ((image_type == IRGB || image_type == ITRUE ||
	    image_type == IGRAY) &&
	   ic->grayscale && ic->grayscale->u.grayscale.value_count < 29)
  {
    if (!ic->gray_tables[0].eight_8_dither)
	for (i = 0; i < TCOUNT; i++)
	    ic->gray_tables[i].eight_8_dither =
		ccf_create_gray_dither_table(i, 4, 4, ic->grayscale);
    if(is->depth == 1)
    {
      /* 1-bit images need special handling because they are not
         displayed using the colormap in an XPutImage. Instead the
         Graphics context foreground and background colors are used */
      if(ic->grayscale->u.grayscale.pixel_values[1])
      {
        is->fg = ic->grayscale->u.grayscale.pixel_values[1];
        is->bg = ic->grayscale->u.grayscale.pixel_values[0];
      }
      else 
      {
        is->fg = ic->grayscale->u.grayscale.pixel_values[0];
        is->bg = ic->grayscale->u.grayscale.pixel_values[1];
      }
    }
    if (image_type == IRGB || image_type == IGRAY)
    {
      /*
       * colormap to grayscale dither
       */
      if (lf_null_gray_map(maps) && image->transparent == -1)
      {
	for (i = 0; i < TCOUNT; i++)
	    is->dither_table[i].eight_8_dither =
		ic->gray_tables[i].eight_8_dither;
	is->free_dither_table = 0; /* don't free it its a copied pointer */
      }
      else
      {
	for (i = 0; i < TCOUNT; i++)
	{
	  is->dither_table[i].eight_8_dither =
	      ccf_gray_to_gray_dither_convert(ic->gray_tables[i].
					      eight_8_dither,
					      image->rgb.used,
					      maps);
          if(image->transparent != -1)
	      ccf_set_specially_allocated(is->dither_table[i].eight_8_dither,
					  image->transparent,
					  is->bgcolor.pixel);

	}
	is->free_dither_table = 4;
      }
      is->dither_function = &ddf_dither_line_8;
      is->dithering = true;
    }
    else
    {
      /*
       * True-color-to-grayscale type situtation
       */
      for (i = 0; i < TCOUNT; i++)
	  is->dither_table[i].true_8_dither = 
	      ic->gray_tables[i].true_8_dither;
      is->free_dither_table = 0; /* don't free it its a copied pointer!! */
      is->dither_function = &ddf_gray_dither_line_24;
      is->dithering = true;
    }
  }
  /*
   * else a bw conversion
   */
  else if ((image_type == IRGB || image_type == ITRUE ||
	    image_type == IGRAY) &&
	   ic->grayscale &&
	   ic->grayscale->u.grayscale.value_count >= 29)
  {
    if (image->transparent != -1 &&
	(image_type == IRGB || image_type == IGRAY))
    {
      maps[0][image->transparent] = is->bgcolor.red >> 8;
      maps[1][image->transparent] = is->bgcolor.green >> 8;
      maps[2][image->transparent] = is->bgcolor.blue >> 8;
    }
    
    if (!ic->gray_tables[0].eight_true_conversion)
	ic->gray_tables[0].eight_true_conversion =
	    ccf_create_gray_conversion_table(ic->grayscale);
    if (image_type == IGRAY || image_type == IRGB)
    {
      /*
       * Simple remap of input values to output values
       */
      if (lf_null_gray_map(maps))
      {
	for (i = 0; i < TCOUNT; i++)
	    is->dither_table[i].eight_true_conversion =
		ic->gray_tables[0].eight_true_conversion;
	is->free_dither_table = 0; /* don't free it its a copied pointer */
      }
      else
      {
	for (i = 0; i < TCOUNT; i++)
	    is->dither_table[i].eight_true_conversion =
		ccf_gray_to_gray_conversion_convert(ic->gray_tables[0].
						    eight_true_conversion, 
						    image->rgb.used,
						    maps);
	is->free_dither_table = TCOUNT;
      }
      is->dither_function = &ddf_convert_line_8;
      is->dithering = false;
    }
    else
    {
      /*
       * Reduce 24-bit color to grayscale without dithering
       */
      for (i = 0; i < TCOUNT; i++)
	    is->dither_table[i].eight_true_conversion =
		ic->gray_tables[0].eight_true_conversion;
      is->free_dither_table = 0; /* don't free it its a copied pointer */
      is->dither_function = &ddf_gray_convert_line_24;
      is->dithering = true;
    }
  }
  
  maps[0] = maps[1] = maps[2] = 0;

  return;
}


/*
 * I'm going to need this soon:

       *special_return =
       cube_table.true_true_conversion->pixel_values[0][intensities[0] >> 8] +
       cube_table.true_true_conversion->pixel_values[1][intensities[1] >> 8] +
       cube_table.true_true_conversion->pixel_values[2][intensities[2] >> 8];
 *
 */


static void
lf_get_new_specials(is, ic, image, input)
ImageState *is;
ImageClass *ic;
Image *image;
byte *input;
{
  bool *blacklist = is->special_blacklist;
  bool *real_blacklist = is->special_real_blacklist;
  /*
   * Find 3 identical pixels and win a new special color allocation. The
   * pixels are searched for spaced 4 apart, because this ensures that
   * they are not just antialias pixels on the edge of writing or other
   * unimportantly strewn pixels. Also, this catches the case where the
   * GIF has already beed dithered with a 4x4 matrix. This is an important
   * case because it can interfere with our own dithering and produce
   * ugly artefacts.
   */
  if (!is->expansion_buf &&
      is->dithering &&
      image->type == IRGB &&
      !is->no_more_specials)
  {
    int i;
    for(i = 0; i < image->width - 8; i++, input++)
    {
      byte t = *input;
      bool dont_really_allocate;

      if (real_blacklist[t]) continue;

      if (ic->special_count < ic->special_max &&
	  is->image_special_count < is->image_special_max &&
          t == input[4] &&
          t == input[8])
      {
        real_blacklist[t] = true;
        dont_really_allocate = false;
      }
      else
      {
        if(blacklist[t]) continue;
        dont_really_allocate = true;
      }
      blacklist[t] = true;
      {
	Intensity intensities[3];
	bool really_allocated;
	unsigned long special_entry;

	if(t == image->transparent) continue;

	intensities[0] = image->rgb.red[t];
	intensities[1] = image->rgb.green[t];
	intensities[2] = image->rgb.blue[t];
	if (!xccf_allocate_special(is->dpy,
				   is->cmap,
				   ic->colorcube,
				   ic->grayscale,
				   intensities,
				   &really_allocated,
				   dont_really_allocate,
				   ic->special_colors,
				   ic->special_count,
				   &special_entry))
	{
	  /*
	   * If a color allocation fails once, we won't bother the
	   * X server again. Color allocation is a slow round trip
	   */
	  if (!dont_really_allocate)
	    is->image_special_max = is->image_special_count;
	}
	else
	{
	  int j;
	  if(really_allocated)
	  {
	    ic->special_count++;
	    is->image_special_count++;
	    if(is->image_special_count >= image->rgb.used)
		is->no_more_specials = true;
	    for(j = 0; j < 256; j++)
		blacklist[j] = false;
	  }
          for(j = 0; j < TCOUNT; j++)
          {
	    ccf_set_specially_allocated(is->dither_table[j].eight_8_dither,
	                                t, special_entry);
	  }
	}
      }
    }
  }
}

void
SetSize(is)
ImageState *is;
{
  unsigned int fw, fh;
  XImage *xi = is->xi;
  bool sbstate;

  if (GUIGetDimensions(is->wd, &fw, &fh) == -1)
  {
    GUISetScrollBar(is->wd, false);
    GUISetInitialDimensions(is->wd, xi->width, xi->height);
    GUISetDimensions(is->wd, xi->width, xi->height);
  }
  else
  {
    if (xi->width > fw || xi->height > fh) sbstate = true;
    else sbstate = false;
    GUISetScrollBar(is->wd, sbstate);
    GUISetDimensions(is->wd, xi->width, xi->height);
  }

  return;
}

static void
ImageToXImage(pointer, fline, lline)
void *pointer;
int fline;
int lline;
{
  ImageState *is = (ImageState *)pointer;
  ImageClass *ic = is->ic;
  byte *input;
  char *output;
  int depth;
  int line;
  Image *image = lf_get_image(is);
  int image_type = image->type;

  /*
   * First time
   */
  if (is->xi == NULL)
  {
    /*
     * No image gets more than half the special colors
     */
    is->image_special_max = (ic->special_max - ic->special_count) / 2;

    if (image->pixlen == 1) depth = 1;
    else depth = is->depth;
    
    if(image->depth == 1)
    {
      lf_set_up_bitmap(is);
      if(is->expansion_buf) lf_set_up_expansion(is);
      image_type = is->expanded_type;
    }
    else
    {
      is->expanded_type = image->type;
    }

    if (is->expansion_buf) depth = is->depth;

    if (image_type == IRGB || image_type == ITRUE)
    {
      if (!ic->colorcube)
      {
        ic->colorcube =
	    xccf_allocate_cube(is->dpy, is->cmap, is->v, is->depth);
      }
    }
    if ((image_type == IGRAY || !ic->colorcube) && image->pixlen != 1)
    {
      if (!ic->grayscale)
      {
	ic->grayscale =
	    xccf_allocate_grays(is->dpy, is->cmap, is->v, is->depth);
      }
    }
    
    is->xi = XCreateImage(is->dpy, is->v,
			  depth,
                          depth == 1 ? XYBitmap : ZPixmap,
                          0, NULL,
			  image->width, image->height,
			  32, 0);
    
    /* Make sure we have plenty of padding at the end of each line */
    is->xi->bytes_per_line += 16;
    is->xi->data = (char *)MPCGet(is->mp,
				  image->height * is->xi->bytes_per_line);
    
#ifdef CHIMERA_LITTLE_ENDIAN
    is->xi->byte_order = is->xi->bitmap_bit_order = LSBFirst;
#else
    is->xi->byte_order = is->xi->bitmap_bit_order = MSBFirst;
#endif

    if(depth != 1) XAddPixel(is->xi, is->bgcolor.pixel);
    if(image->depth != 1 || is->expansion_buf) lf_set_up_dither(is);

    SetSize(is);
  }

  /*
   * Every time
   */
  input = image->data + image->bytes_per_line * fline;
  output = is->xi->data + is->xi->bytes_per_line * fline;

/*
  lf_get_new_specials(is, ic, image, input);
*/

  for (line = fline;
       line <= lline;
       line++, input += image->bytes_per_line,
       output += is->xi->bytes_per_line)
  {
    if (image->pixlen == 1 && !is->expansion_buf)
    {
      memcpy(output, input,
	     MIN(is->xi->bytes_per_line, image->bytes_per_line));
    }
    else
    {
      byte *expanded_input;
      if (is->expansion_buf)
      {
        is->expansion_function(is->expansion_buf,
                               input,
                               image->width,
                               image->rgb.red,
                               image->rgb.green,
                               image->rgb.blue,
                               image->transparent,
                               (Intensity)is->bgcolor.red,
                               (Intensity)is->bgcolor.green,
                               (Intensity)is->bgcolor.blue);
        expanded_input = is->expansion_buf;
      }
      else
      {
        expanded_input = input;
      }
      if(is->dither_function)
      {
        is->dither_function(is->dither_table[line & 3],
			    expanded_input,
			    output,
			    is->xi->bits_per_pixel,
			    image->width);
      }
      else
      {
        printf("Warning:no dither function defined for image\n");
      }
    }
  }
  
  /*
   * Paint line
   */
  if (is->xi->depth == 1)
  {
    XSetForeground(is->dpy, ic->gc, is->fg);
    XSetBackground(is->dpy, ic->gc, is->bg);
  }
  XPutImage(is->dpy, is->win, ic->gc, is->xi,
	    0, fline,
	    0, fline,
	    is->xi->width, lline - fline + 1);

  if (lline > is->last_line) is->last_line = lline;

  return;
}

/*
 * ImageClassInit
 */
static void
ImageClassInit(ic, dpy)
ImageClass *ic;
Display *dpy;
{
/*  XGCValues xgcv; */

  ic->icount = 0;
  ic->init = True;
  ic->gc = XCreateGC(dpy, RootWindow(dpy, DefaultScreen(dpy)), 0, NULL);
  
  if (getenv("CHIMERA_MAX_SPECIAL_COLORS"))
  {
    ic->special_max = atoi(getenv("CHIMERA_MAX_SPECIAL_COLORS"));
  }
  else
  {
    ic->special_max = 64;
  }

  return;
}

/*
 * ImageDestroy
 */
static void
ImageDestroy(closure)
void *closure;
{
  ImageState *is = (ImageState *)closure;
  int i;

  if (is->if_vector.destroyProc && is->if_vector.image_format_closure != NULL)
  {
    is->if_vector.destroyProc(is->if_vector.image_format_closure);
    is->if_vector.image_format_closure = NULL;
  }
  if(is->xi)
  {
    is->xi->data = NULL;
    XDestroyImage(is->xi);
    is->xi = 0;
  }
  for(i = 0; i < is->free_dither_table; i++)
  {
    if(is->dither_table[i].generic_dither_table)
      free_mem(is->dither_table[i].generic_dither_table);
    is->dither_table[i].generic_dither_table = 0;
  }
  for( ; i < TCOUNT; i++)
    is->dither_table[i].generic_dither_table = 0;
  is->free_dither_table = 0;

  if (--is->ic->icount == 0)
  {
    /* free specially allocated colors here? */
  }

  MPDestroy(is->mp);

  return;
}

/*
 * ImageInit
 *
 * This is where the initialization for a frame begins.  It is
 * called when the content-type becomes known.
 */
void *
ImageInit(wn, class_closure, state)
ChimeraRender wn;
void *class_closure;
void *state;  /* ignored */
{
  ImageClass *ic = (ImageClass *)class_closure;
  ImageState *is;
  int format;
  XWindowAttributes xwa;
  int status;
  char *content;
  int i;
  ChimeraSink wp;
  ChimeraGUI wd;
  MemPool mp;

  wp = RenderToSink(wn);
  wd = RenderToGUI(wn);

  content = SinkGetInfo(wp, "content-type");

  for (i = 0; content_map[i].name != NULL; i++)
  {
    if (strcasecmp(content_map[i].name, content) == 0) break;
  }
  if (content_map[i].name == NULL) return(NULL);
  format = content_map[i].id;

  mp = MPCreate();
  is = (ImageState *)MPCGet(mp, sizeof(ImageState));
  is->mp = mp;

  is->win = GUIToWindow(wd);
  is->dpy = GUIToDisplay(wd);

  if (!ic->init) ImageClassInit(ic, is->dpy);

  status = XGetWindowAttributes(is->dpy, is->win, &xwa);
  is->v = xwa.visual;
  is->depth = xwa.depth;
  is->cmap = xwa.colormap;
  is->ic = ic;
  is->wd = wd;
  is->wp = wp;
  is->bgcolor.pixel = GUIBackgroundPixel(wd);
  XQueryColor(is->dpy, is->cmap, &(is->bgcolor));

  if (format == IMAGE_GIF) gifInit(ImageToXImage, is, &is->if_vector);
  else if (format == IMAGE_PNM) pnmInit(ImageToXImage, is, &is->if_vector);
  else if (format == IMAGE_XBM) xbmInit(ImageToXImage, is, &is->if_vector);
#ifdef HAVE_JPEG
  else if (format == IMAGE_JPEG) jpegInit(ImageToXImage, is, &is->if_vector);
#endif
#ifdef HAVE_PNG
  else if (format == IMAGE_PNG) pngInit(ImageToXImage, is, &is->if_vector);
#endif

  ic->icount++;

  return(is);
}

/*
 * ImageAdd
 */
static void
ImageAdd(closure)
void *closure;
{
  ImageState *is = (ImageState *)closure;
  byte *data;
  size_t len;
  MIMEHeader mh;

  SinkGetData(is->wp, &data, &len, &mh);

  if (is->if_vector.addDataProc && is->if_vector.image_format_closure)
  {
    is->if_vector.addDataProc(is->if_vector.image_format_closure,
			      data, len, false);
  }
  
  return;
}

/*
 * ImageEnd
 */
static void
ImageEnd(closure)
void *closure;
{
  ImageState *is = (ImageState *)closure;
  byte *data;
  size_t len;
  MIMEHeader mh;

  SinkGetData(is->wp, &data, &len, &mh);

  if (is->if_vector.addDataProc && is->if_vector.image_format_closure)
  {
    is->if_vector.addDataProc(is->if_vector.image_format_closure,
			      data, len, true);
  }

  return;
}

static bool
ImageExpose(closure, ex, ey, ewidth, eheight)
void *closure;
int ex, ey;
unsigned int ewidth, eheight;
{
  ImageState *is = (ImageState *)closure;
  unsigned int height;

  if (is->xi == NULL) return(true);

  if (ey > is->last_line) return(true);

  if (ey + eheight > is->last_line) height = is->last_line - ey;
  else height = eheight;

  XPutImage(is->dpy, is->win, is->ic->gc, is->xi,
            ex, ey,
            ex, ey,
            ewidth, height + 1);

  return(true);
}

static void
ImageClassDestroy(closure)
void *closure;
{
  ImageClass *ic = (ImageClass *)closure;
  int i;

  ic->refcount--;
  if (ic->refcount > 0) return;

  free_mem(ic->colorcube);
  free_mem(ic->grayscale);
  for (i = 0; i < TCOUNT; i++)
  {
    if (ic->color_tables[i].generic_dither_table != NULL)
    {
      free_mem(ic->color_tables[i].generic_dither_table);
    }
  }
  for (i = 0; i < TCOUNT; i++)
  {
    if (ic->gray_tables[i].generic_dither_table != NULL)
    {
      free_mem(ic->gray_tables[i].generic_dither_table);
    }
  }
  MPDestroy(ic->mp);

  return;
}

static void
ImageCancel(closure)
void *closure;
{
  return;
}

int
InitModule_Image(cres)
ChimeraResources cres;
{
  ChimeraRenderHooks rh;
  ImageClass *ic;
  int i;
  MemPool mp;

  mp = MPCreate();
  ic = (ImageClass *)MPCGet(mp, sizeof(ImageClass));
  ic->mp = mp;

  for (i = 0; content_map[i].name != NULL; i++)
  {
    memset(&rh, 0, sizeof(ChimeraRenderHooks));
    rh.content = content_map[i].name;
    rh.class_context = ic;
    ic->refcount++;
    rh.class_destroy = ImageClassDestroy;
    rh.init = ImageInit;
    rh.add = ImageAdd;
    rh.end = ImageEnd;
    rh.destroy = ImageDestroy;
    rh.cancel = ImageCancel;
    rh.expose = ImageExpose;
    RenderAddHooks(cres, &rh);
  }

  return(0);
}
