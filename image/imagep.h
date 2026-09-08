/* image.h:
 *
 * portable image type declarations
 *
 * jim frost 10.02.89
 *
 * Copyright 1989 Jim Frost.
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
 *
 * Changes (c) Copyright 1995 Erik Corry ehcorry@inet.uni-c.dk
 */

#ifndef __IMAGEP_H__
#define __IMAGEP_H__ 1

#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif
 
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

/*
 * should be in limits.h
 */
#ifndef CHAR_BITS
#define CHAR_BITS 8
#endif

typedef unsigned short Intensity;

typedef struct rgbcolor
{
  Intensity red, green, blue;
} RGBColor;

typedef struct rgbmap
{
  unsigned int  size;       /* size of RGB map (bytes per enty) */
  unsigned int  used;       /* number of colors used in RGB map */
  unsigned int  compressed; /* image uses colormap fully */
  Intensity    *red;        /* color values in X style */
  Intensity    *green;
  Intensity    *blue;
} RGBMap;

/* image structure
 */

typedef struct
{
  unsigned int  type;   /* type of image */
  int x, y;             /* x, y of decoded image */
  int pass;             /* interlace pass */
  int transparent;      /* transparent color index */
  RGBMap        rgb;    /* RGB map of image if IRGB or IGRAY type */
  unsigned int  width;  /* width of image in pixels */
  unsigned int  height; /* height of image in pixels */
  unsigned int  depth;  /* depth of image in bits */
  unsigned int  pixlen; /* length of pixel in bits after padding */
  unsigned int  bytes_per_line; /* After padding */
  float		gamma;	/* gamma of display the image is adjusted for */
  byte         *data;   /* data rounded to full byte for each row */
} Image;

#define IBAD    0 /* invalid image type (used when freeing) */
#define IBITMAP 1 /* image is a bitmap */
#define IRGB    2 /* image is RGB */
#define ITRUE   3 /* image is true color */
#define IGRAY   4 /* image is gray scale */

/* new.c */
Image *newBitImage _ArgProto((unsigned int width, unsigned int height));
Image *newTrueImage _ArgProto((unsigned int width, unsigned int height));
Image *newRGBImage _ArgProto((unsigned int width, unsigned int height,
			      unsigned int depth));
void   freeImage _ArgProto((Image *image));

/*
 * this returns the (approximate) intensity of an RGB triple
 */

#define colorIntensity(R,G,B) \
  (RedIntensity[(R) >> 8] + GreenIntensity[(G) >> 8] + BlueIntensity[(B) >> 8])

extern unsigned short RedIntensity[];
extern unsigned short GreenIntensity[];
extern unsigned short BlueIntensity[];

#endif /* __IMAGEP_H__ */
