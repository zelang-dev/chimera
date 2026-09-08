/* new.c:
 *
 * functions to allocate and deallocate structures and structure data
 *
 * jim frost 09.29.89
 *
 * Copyright 1989, 1991 Jim Frost.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  The author makes no representations
 * about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "port_before.h"

#include <stdio.h>
#include <string.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "port_after.h"

#include "common.h"

#include "imagep.h"

Image *
newBitImage(width, height)
unsigned int width, height;
{
  unsigned long datasize;
  Image        *image;

  image = (Image *)alloc_mem(sizeof(Image));
  if (!image) return((Image *) 0);

  memset(image, 0, sizeof(Image));
  image->type = IBITMAP;
  image->width = width;
  image->height = height;
  image->depth = 1;

  image->pixlen = 1;
  /* round bytes_per_line up to nearest byte */
  image->bytes_per_line = ((width - 1) / CHAR_BITS) + 1;
  /* round bytes_per_line up to nearest longword */
  image->bytes_per_line = ((image->bytes_per_line - 1) / sizeof(long)) + 1;
  image->bytes_per_line *= sizeof(long);
  /* Allocate a little too much memory, to allow overread */
  datasize = image->bytes_per_line * height + 32;
  image->data = (byte *)alloc_mem(datasize);
  memset(image->data, 0, datasize);
  image->transparent = -1;

  return(image);
}

Image *
newRGBImage(width, height, depth)
unsigned int width, height, depth;
{
  Image *image;
  unsigned int pixlen;
  unsigned long datasize;
  
  if(depth == 1)
  {
    image = newBitImage(width, height);
    image->type = IRGB;
    return image;
  }
  
  pixlen = ((depth - 1) / CHAR_BITS) + 1;
  pixlen *= CHAR_BITS;
  
  if (pixlen == 0) pixlen = 1;
  
  image = (Image *)alloc_mem(sizeof(Image));
  if (!image) return((Image *)0);

  memset(image, 0, sizeof(Image));
  image->type = IRGB;
  image->width = width;
  image->height = height;
  image->depth = depth;
  image->pixlen = pixlen;
  /* set bytes_per_line (pixlen is a multiple of CHAR_BITS) */
  image->bytes_per_line = (width * pixlen) / CHAR_BITS;
  /* round bytes_per_line up to nearest longword */
  image->bytes_per_line = ((image->bytes_per_line - 1) / sizeof(long)) + 1;
  image->bytes_per_line *= sizeof(long);
  /* Allocate a little too much memory, to allow overread */
  datasize = image->bytes_per_line * height + 32;
  image->data = (byte *)alloc_mem(datasize);
  if (!image->data)
  {
    free_mem((char *)image);
    return((Image *)0);
  }

  memset(image->data, 0, datasize);
  
  image->rgb.used = 0;
  image->rgb.compressed = 0;
  image->rgb.size = 2;
  
  image->transparent = -1;
  
  return(image);
}

Image *
newTrueImage(width, height)
unsigned int width, height;
{
  Image *image;
  unsigned long datasize;
  
  image = (Image *)alloc_mem(sizeof(Image));
  if (!image) return((Image *) 0);

  memset(image, 0, sizeof(Image));
  image->type = ITRUE;
  image->width = width;
  image->height = height;
  image->depth = 8;
  image->pixlen = 24;
  /* set bytes_per_line */
  image->bytes_per_line = width * 3;
  /* round bytes_per_line up to nearest longword */
  image->bytes_per_line = ((image->bytes_per_line - 1) / sizeof(long)) + 1;
  image->bytes_per_line *= sizeof(long);
  /* Allocate a little too much memory, to allow overread */
  datasize = image->bytes_per_line * height + 32;
  image->data = (byte *)alloc_mem(datasize);
  if (!image->data)
  {
    free_mem((char *)image);
    return ((Image *)0);
  }

  memset(image->data, 0, datasize);
  
  image->rgb.used = 0;
  image->rgb.compressed = 0;
  image->rgb.size = 2;
  
  image->transparent = -1;
  
  return(image);
}

void
freeImage(image)
Image *image;
{
  if (image->data != NULL) free_mem((char *)image->data);
  image->data = NULL;
  free_mem((char *)image);

  return;
}
