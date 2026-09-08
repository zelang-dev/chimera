/*
 * font.c
 *
 * libhtml - HTML->X renderer
 *
 * Copyright (c) 1995-1997, John Kilburg <john@cs.unlv.edu>
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

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "port_after.h"

#include "html.h"

/*
 *
 * You really don't want to look in here.
 *
 */

#define FONT_SCALE_COUNT 6

#define XLFD_FOUNDRY 0
#define XLFD_FAMILY 1
#define XLFD_WEIGHT_NAME 2
#define XLFD_SLANT 3
#define XLFD_SETWIDTH_NAME 4
#define XLFD_STYLE_NAME 5
#define XLFD_PIXEL_SIZE 6
#define XLFD_POINT_SIZE 7
#define XLFD_RES_X 8
#define XLFD_RES_Y 9
#define XLFD_SPACING 9
#define XLFD_AVERAGE_WIDTH 10
#define XLFD_REGISTRY 11
#define XLFD_ENCODING 12
#define XLFD_COUNT 13

/* Silly font information */
struct HTMLFontP
{
  bool        fixed;            /* fixed width? */
  int         weight;           /* bold, medium, ... */
  int         slant;            /* italic, regular, ... */
  int         scale;            /* scale of font */
  int         size;             /* size of font */
  XFontStruct *xfi;             /* real font info */
};

struct HTMLFontListP
{
  HTMLFont       fontInfo;
  int            count;
  int            scale[FONT_SCALE_COUNT];
  char           **names;
};

static void ParseXLFD _ArgProto((MemPool, char *, HTMLFont));
static HTMLFontList GetFontList _ArgProto((HTMLClass, char *));
static void FreeFontList _ArgProto((HTMLClass, HTMLFontList));
static XFontStruct *GetFont _ArgProto((HTMLClass, HTMLFontList, int));

/*
 * ParseXLFD
 *
 * Yipe!
 */
static void
ParseXLFD(mp, xlfd, lfi)
MemPool mp;
char *xlfd;
HTMLFont lfi;
{
  char *fields[XLFD_COUNT];
  int i;
  char *t;

  t = MPStrDup(mp, xlfd);
  t++;
  fields[0] = t;
  for (i = 1; i < XLFD_COUNT; t++)
  {
    if (*t == '-')
    {
      *t = '\0';
      fields[i++] = t + 1;
    }
  }

  if (strcmp(fields[XLFD_WEIGHT_NAME], "bold") == 0 ||
      strcmp(fields[XLFD_WEIGHT_NAME], "demi") == 0 ||
      strcmp(fields[XLFD_WEIGHT_NAME], "demibold") == 0) 
  {
    lfi->weight = 1;
  }  
  else lfi->weight = 0;

  lfi->size = atoi(fields[XLFD_PIXEL_SIZE]);

  if (strcmp(fields[XLFD_SLANT], "i") == 0 ||
      strcmp(fields[XLFD_SLANT], "o") == 0)
  {
    lfi->slant = 1;
  }
  else lfi->slant = 0;

  return;
}

/*
 * GetFont
 */
static XFontStruct *
GetFont(lc, fl, i)
HTMLClass lc;
HTMLFontList fl;
int i;
{
  if (fl->fontInfo[i].xfi == NULL)
  {
    fl->fontInfo[i].xfi = XLoadQueryFont(lc->dpy, fl->names[i]);
    if (fl->fontInfo[i].xfi == NULL) return(lc->defaultFont);
  }
  return(fl->fontInfo[i].xfi);
}

/*
 * HTMLGetFont
 */
XFontStruct *
HTMLGetFont(li, env)
HTMLInfo li;
HTMLEnv env;
{
  int i;
  HTMLClass lc = li->lc;
  HTMLFont lfi;
  HTMLFontList fl;

  lfi = env->fi;
  if (lfi == NULL)
  {
    lfi = HTMLDupFont(li,li->cfi);
    env->fi = lfi;
  }

  /*
   * Check spacing
   */
  if (lfi->fixed) fl = lc->fixed;
  else fl = lc->prop;

  /*
   * Check slant, size, and weight
   */
  for (i = 0; i < fl->count; i++)
  {
    if (((lfi->weight > 0) == (fl->fontInfo[i].weight > 0)) &&
	((lfi->slant > 0) == (fl->fontInfo[i].slant > 0)) &&
	fl->scale[lfi->scale] == fl->fontInfo[i].size)
    {
      return(GetFont(lc, fl, i));
    }
  }

  /*
   * Check slant and size
   */
  for (i = 0; i < fl->count; i++)
  {
    if (((lfi->slant > 0) == (fl->fontInfo[i].slant > 0)) &&
	fl->scale[lfi->scale] == fl->fontInfo[i].size)
    {
      return(GetFont(lc, fl, i));
    }
  }

  /*
   * Check weight and size
   */
  for (i = 0; i < fl->count; i++)
  {
    if (((lfi->weight > 0) == (fl->fontInfo[i].weight > 0)) &&
	fl->scale[lfi->scale] == fl->fontInfo[i].size)
    {
      return(GetFont(lc, fl, i));
    }
  }

  /*
   * Check size
   */
  for (i = 0; i < fl->count; i++)
  {
    if (fl->scale[lfi->scale] == fl->fontInfo[i].size)
    {
      return(GetFont(lc, fl, i));
    }
  }

  return(lc->defaultFont);
}

/*
 * GetFontList
 */
static HTMLFontList
GetFontList(lc, pattern)
HTMLClass lc;
char *pattern;
{
  int count;
  int scale_count;
  int i, j, k;
  MemPool mp;
  HTMLFontList fl;

  fl = (HTMLFontList)MPCGet(lc->mp, sizeof(struct HTMLFontListP));

  fl->names = XListFonts(lc->dpy, pattern, 999, &count);
  if (fl->names == NULL || count == 0)
  {
    fprintf (stderr, "Could not get font list\n");
    fflush(stderr);
    exit(1);
  }

  fl->fontInfo = (HTMLFont)MPCGet(lc->mp, sizeof(struct HTMLFontP) * count);

  /*
   * Extract simple information from the XLFD strings.
   */
  mp = MPCreate();
  for (i = 0; i < count; i++)
  {
    ParseXLFD(mp, fl->names[i], &(fl->fontInfo[i]));
  }
  MPDestroy(mp);

  /*
   * Find sizes for each of the font scales.  There must be a better way.
   * This should probably not take 
   */
  scale_count = 0;
  for (i = 0; i < count && scale_count < FONT_SCALE_COUNT; i++)
  {
    if (fl->fontInfo[i].size > 0)
    {
      for (j = 0; j < scale_count; j++)
      {
	if (fl->fontInfo[i].size == fl->scale[j]) break;
	if (fl->fontInfo[i].size < fl->scale[j])
	{
	  for (k = scale_count - 1; k >= j; k--)
	  {
	    fl->scale[k + 1] = fl->scale[k];
	  }
	  fl->scale[j] = fl->fontInfo[i].size;
	  scale_count++;
	  break;
	}
      }
      if (j == scale_count)
      {
	fl->scale[j] = fl->fontInfo[i].size;
	scale_count++;
      }
    }
  }

  /*
   * This will probably never happen.
   */
  if (scale_count == 0)
  {
    fl->scale[0] = fl->fontInfo[0].size;
    scale_count++;
  }
  
  /*
   * Fill out the rest if there aren't FONT_SCALE_COUNT
   */
  if (scale_count < FONT_SCALE_COUNT)
  {
    for (i = scale_count; i < FONT_SCALE_COUNT; i++)
    {
      fl->scale[i] = fl->scale[i - 1];
    }
    scale_count = FONT_SCALE_COUNT;
  }

  /*
   * Set miscellaneous information.
   */
  fl->count = count;

  return(fl);
}

/*
 * HTMLSetupFonts
 */
void
HTMLSetupFonts(li)
HTMLInfo li;
{
  char *name;
  HTMLClass lc = li->lc;

  /*
   * Default HTML font.
   */
  li->cfi = (HTMLFont)MPCGet(li->mp, sizeof(struct HTMLFontP));
  HTMLSetFontProp(li->cfi);
  HTMLSetFontScale(li->cfi, 2);

  /*
   * Only need to do HTML font information setup once.
   */
  if (lc->font_setup_done) return;

  lc->dpy = li->dpy;

  /*
   * Font to use if all else fails
   */
  name = ResourceGetString(li->cres, "html.defaultFont");
  if (name == NULL) name = "variable";
  if ((lc->defaultFont = XLoadQueryFont(li->dpy, name)) == NULL)
  {
    fprintf (stderr, "Could not get default font.\n");
    fflush(stderr);
    exit(1);
  }

  /*
   * Get the font list pattern for the proportional fonts.  Hopefully
   * a reasonable pattern was selected.
   */
  name = ResourceGetString(li->cres, "html.propFontPattern");
  if (name == NULL) name = "-adobe-times-*-*-*-*-*-*-*-*-*-*-iso8859-1";
  lc->prop = GetFontList(lc, name);

  /*
   * Get the font list pattern for the fixed fonts.  Hopefully
   * a reasonable pattern was selected.
   */
  name = ResourceGetString(li->cres, "html.fixedFontPattern");
  if (name == NULL) name = "-misc-fixed-*-*-*-*-*-*-*-*-*-*-iso8859-1";
  lc->fixed = GetFontList(lc, name);

  lc->font_setup_done = true;

  return;
}

/*
 * FreeFontList
 */
static void
FreeFontList(lc, fl)
HTMLClass lc;
HTMLFontList fl;
{
  int i;

  if (fl->fontInfo != NULL)
  {
    for (i = 0; i < XLFD_COUNT; i++)
    {
      if (fl->fontInfo[i].xfi != NULL &&
	  fl->fontInfo[i].xfi != lc->defaultFont)
      {
	XFreeFont(lc->dpy, fl->fontInfo[i].xfi);
      }
    }
  }
  if (fl->names != NULL) XFreeFontNames(fl->names);

  return;
}

/*
 * HTMLFreeFonts
 */
void
HTMLFreeFonts(lc)
HTMLClass lc;
{
  if (lc->prop != NULL) FreeFontList(lc, lc->prop);
  if (lc->fixed != NULL) FreeFontList(lc, lc->fixed);

  if (lc->defaultFont != NULL) XFreeFont(lc->dpy, lc->defaultFont);

  return;
}

HTMLFont
HTMLDupFont(li, f)
HTMLInfo li;
HTMLFont f;
{
  HTMLFont n;
  n = (HTMLFont)MPCGet(li->mp, sizeof(struct HTMLFontP));
  memcpy(n, f, sizeof(struct HTMLFontP));
  return(n);
}

void
HTMLAddFontWeight(f)
HTMLFont f;
{
  f->weight++;
  return;
}

void
HTMLAddFontSlant(f)
HTMLFont f;
{
  f->slant++;
  return;
}

void
HTMLAddFontScale(f)
HTMLFont f;
{
  f->scale++;
  return;
}

void
HTMLSetFontScale(f, scale)
HTMLFont f;
int scale;
{
  f->scale = scale;
  return;
}

void
HTMLSetFontProp(f)
HTMLFont f;
{
  f->fixed = false;
  return;
}

void
HTMLSetFontFixed(f)
HTMLFont f;
{
  f->fixed = true;
  return;
}
