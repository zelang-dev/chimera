/*
 * xcolorcube.c - Manage color allocation on X display
 *
 * (c) Copyright 1995 Erik Corry ehcorry@inet.uni-c.dk erik@kroete2.freinet.de
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
#include <stdio.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include "port_after.h"

#include "colorcube.h"
#include "xcolorcube.h"

typedef struct ls_cubealist
{
  Display *display;
  Colormap colormap;
  cct_cube cube;
  struct ls_cubealist *next;
} *lt_cubealist;

static lt_cubealist lv_cubealist = 0;
static lt_cubealist lv_grayalist = 0;

static bool
lf_attempt_axbxc(
  cct_cube cube,
  Display *display,
  Colormap colormap,
  int a,
  int b,
  int c)
{
  unsigned long allocated[256];
  int la, lb, lc;
  int n = 0;

  for(la = 0; la < a; la++)
  {
    cube->u.mapping.pixel_values[0][la] = b*c*la;
    for(lb = 0; lb < b; lb++)
    {
      cube->u.mapping.pixel_values[1][lb] = c*lb;
      for(lc = 0; lc < c; lc++)
      {
        XColor try;
        int status;
        cube->u.mapping.pixel_values[2][lc] = lc;
        try.red = (unsigned short)   (0.001+(65535.0 * la) / (double) (a - 1));
        try.green = (unsigned short) (0.001+(65535.0 * lb) / (double) (b - 1));
        try.blue = (unsigned short)  (0.001+(65535.0 * lc) / (double) (c - 1));
        cube->u.mapping.brightnesses[0][la] = try.red >> 8;
        cube->u.mapping.brightnesses[1][lb] = try.green >> 8;
        cube->u.mapping.brightnesses[2][lc] = try.blue >> 8;
        try.flags = DoRed | DoGreen | DoBlue;
        status = XAllocColor(display, colormap, &try);
        if(!status && n)
        {
          XFreeColors(display, colormap, allocated, n, 0);
          return(false);
        }
        cube->u.mapping.mapping[n] = try.pixel;
        allocated[n] = try.pixel;
        n++;
      }
    }
  }

  /* TODO: test whether mapping is really necessary */

  cube->cube_type = cube_mapping;
  cube->u.mapping.value_count[0] = a;
  cube->u.mapping.value_count[1] = b;
  cube->u.mapping.value_count[2] = c;

  if(getenv("CHIMERA_TEST_COLOR_ALLOCATION"))
    printf("%dx%dx%d color cube allocated\n", a, b, c);

  return true;
}


cct_cube
xccf_allocate_cube(
    Display *display,
    Colormap colormap,
    Visual *visual,
    int depth)
{
  cct_cube cube;
  lt_cubealist liststepper;
  lt_cubealist *listextension;

  for(liststepper = lv_cubealist, listextension = &lv_cubealist;
      liststepper;
      listextension = &(liststepper->next), liststepper = liststepper->next)
  {
    if(liststepper->display == display && liststepper->colormap == colormap)
      return liststepper->cube;
  }

  *listextension = (lt_cubealist)calloc(1, sizeof(struct ls_cubealist));
  (*listextension)->display = display;
  (*listextension)->colormap = colormap;
  cube = (cct_cube)calloc(1, sizeof(struct ccs_cube));
  (*listextension)->cube = cube;

  if(visual->class == TrueColor)
  {
    cube->cube_type = cube_true_color;
    cube->u.true_color.color_mask[0] = visual->red_mask;
    cube->u.true_color.color_mask[1] = visual->green_mask;
    cube->u.true_color.color_mask[2] = visual->blue_mask;
    return cube;
  }
  else if(visual->class == StaticGray || visual->class == GrayScale) goto failure;

  /* else PseudoColor (or StaticColor or DirectColor: do they exist?) */

  if(visual->class == PseudoColor && visual->map_entries < 8) goto failure;

  /* TODO get a standard colormap (if there is one) from the Xmu routines */

  if(depth > 8 || getenv("CHIMERA_AGGRESSIVE_COLORS"))
  {
    if(lf_attempt_axbxc(cube, display, colormap, 6, 10, 4))
      return cube;
    if(lf_attempt_axbxc(cube, display, colormap, 7, 9, 4))
      return cube;
    if(lf_attempt_axbxc(cube, display, colormap, 6, 9, 4))
      return cube;
    if(lf_attempt_axbxc(cube, display, colormap, 6, 8, 4))
      return cube;
    if(lf_attempt_axbxc(cube, display, colormap, 6, 8, 4))
      return cube;
    if(lf_attempt_axbxc(cube, display, colormap, 5, 10, 4))
      return cube;
    if(lf_attempt_axbxc(cube, display, colormap, 5, 9, 4))
      return cube;
    if(lf_attempt_axbxc(cube, display, colormap, 4, 10, 4))
      return cube;
    if(lf_attempt_axbxc(cube, display, colormap, 4, 9, 4))
      return cube;
  }
  if(depth == 8)
  {
    /* share with xv */
    if(lf_attempt_axbxc(cube, display, colormap, 4, 8, 4))
      return cube;
    /* share with ghostscript */
    if(lf_attempt_axbxc(cube, display, colormap, 5, 5, 5))
      return cube;
    /* share with old versions of xv and N******e */
    if(lf_attempt_axbxc(cube, display, colormap, 6, 6, 6))
      return cube;
    /* share with N*******e -ncols 64 */
    if(lf_attempt_axbxc(cube, display, colormap, 4, 4, 4))
      return cube;
    /* getting desperate */
    if(lf_attempt_axbxc(cube, display, colormap, 4, 6, 4))
      return cube;
    if(lf_attempt_axbxc(cube, display, colormap, 3, 5, 3))
      return cube;
    if(lf_attempt_axbxc(cube, display, colormap, 3, 4, 3))
      return cube;
    if(lf_attempt_axbxc(cube, display, colormap, 3, 3, 3))
      return cube;
    if(lf_attempt_axbxc(cube, display, colormap, 2, 5, 2))
      return cube;
  }
  if(lf_attempt_axbxc(cube, display, colormap, 2, 4, 2))
    return cube;
  /* almost total desperation */
  if(lf_attempt_axbxc(cube, display, colormap, 2, 3, 2))
    return cube;
  /* total desperation */
  if(lf_attempt_axbxc(cube, display, colormap, 2, 2, 2))
    return cube;

  failure:
  free(cube);
  (*listextension)->cube = 0;
  return 0;
}


static bool
lf_get_gray(
  Display *display,
  Colormap colormap,
  cct_cube cube,
  int n)
{
    XColor try;
    int status;
    try.red = cube->u.grayscale.brightnesses[n] << 8;
    try.green = cube->u.grayscale.brightnesses[n] << 8;
    try.blue = cube->u.grayscale.brightnesses[n] << 8;
    try.flags = DoRed | DoGreen | DoBlue;
    status = XAllocColor(display, colormap, &try);
    if(!status) return false;
    cube->u.grayscale.pixel_values[n] = try.pixel;
    cube->u.grayscale.brightnesses[n] = try.green >> 8;
    return true;
}

static int
lf_reverse_byte(int a)
{
  return ((a & 128) >> 7) |
         ((a & 64) >> 5) |
         ((a & 32) >> 3) |
         ((a & 16) >> 1) |
         ((a & 8) << 1) |
         ((a & 4) << 3) |
         ((a & 2) << 5) |
         ((a & 1) << 7);
}

/*
 * The Window parameter is only needed to allow the Visual to be
 * determined
 */

cct_cube
xccf_allocate_grays(
    Display *display,
    Colormap colormap,
    Visual *visual,
    int depth)
{
  cct_cube cube;
  lt_cubealist liststepper;
  lt_cubealist *listextension;
  int n = 0;
  int maxcols, i, j;

  /*
   * Make sure that the color cube is always allocated before the gray
   * colormap entries
   */
  xccf_allocate_cube(display, colormap, visual, depth);

  for(liststepper = lv_grayalist, listextension = &lv_grayalist;
      liststepper;
      listextension = &(liststepper->next), liststepper = liststepper->next)
  {
    if(liststepper->display == display && liststepper->colormap == colormap)
      return liststepper->cube;
  }

  (*listextension) = (lt_cubealist)calloc(1, sizeof(struct ls_cubealist));
  (*listextension)->display = display;
  (*listextension)->colormap = colormap;
  cube = (cct_cube)calloc(1, sizeof(struct ccs_cube));
  (*listextension)->cube = cube;

  /*
   * We are really saving grays here. With only 16 grays we will dither, not
   * just remap.
   */
  if(visual->class != PseudoColor || depth > 8)
    maxcols = 256;
  else
    maxcols = 16;

  if(visual->class == PseudoColor && visual->map_entries < 8)
    maxcols = 3;

  /* TODO get a standard colormap (if there is one) from the Xmu routines */

  cube->u.grayscale.brightnesses[0] = 0;
  cube->u.grayscale.brightnesses[1] = 255;
  if(!lf_get_gray(display, colormap, cube, 0)) goto failure;
  n = 1;
  if(!lf_get_gray(display, colormap, cube, 1)) goto failure;
  n = 2;
  for(i = 1; i < 255 && n < maxcols; i++, n++)
  {
    cube->u.grayscale.brightnesses[n] = lf_reverse_byte(i);
    if(!lf_get_gray(display, colormap, cube, n))
    {
      n--;
    }
  }

  cube->u.grayscale.value_count = n;

  if(getenv("CHIMERA_TEST_COLOR_ALLOCATION"))
    printf("Allocated %d grays\n", n);

  /*
   * Bubble sort (I know...)
   */
  {
    bool something_happened;
    do
    {
      something_happened = false;
      for(i = 1; i < n; i++)
      {
        if(cube->u.grayscale.brightnesses[i] <
           cube->u.grayscale.brightnesses[i-1])
        {
          unsigned long t;
          t = cube->u.grayscale.brightnesses[i];
          cube->u.grayscale.brightnesses[i] =
            cube->u.grayscale.brightnesses[i-1];
          cube->u.grayscale.brightnesses[i-1] = t;
          t = cube->u.grayscale.pixel_values[i];
          cube->u.grayscale.pixel_values[i] =
            cube->u.grayscale.pixel_values[i-1];
          cube->u.grayscale.pixel_values[i-1] = t;
          something_happened = true;
        }
      }
      for(i = n-1; i; i--)
      {
        if(cube->u.grayscale.brightnesses[i] <
           cube->u.grayscale.brightnesses[i-1])
        {
          unsigned long t;
          t = cube->u.grayscale.brightnesses[i];
          cube->u.grayscale.brightnesses[i] =
            cube->u.grayscale.brightnesses[i-1];
          cube->u.grayscale.brightnesses[i-1] = t;
          t = cube->u.grayscale.pixel_values[i];
          cube->u.grayscale.pixel_values[i] =
            cube->u.grayscale.pixel_values[i-1];
          cube->u.grayscale.pixel_values[i-1] = t;
          something_happened = true;
        }
      }
    } while(something_happened);
  }

  /*
   * Remove duplicates
   */
  for(j = i = 0; i < n; i++)
  {
    if(cube->u.grayscale.brightnesses[i] != cube->u.grayscale.brightnesses[j])
    {
      j++;
    }
    cube->u.grayscale.brightnesses[j] = cube->u.grayscale.brightnesses[i];
    cube->u.grayscale.pixel_values[j] = cube->u.grayscale.pixel_values[i];
  }

  n = j + 1;

  cube->u.grayscale.value_count = n;

  if(getenv("CHIMERA_TEST_COLOR_ALLOCATION"))
    printf("That was %d unique grays\n", n);

  return cube;

  failure:
  if(n)
  {
    XFreeColors(display, colormap, cube->u.grayscale.pixel_values, n, 0);
  }
  free(cube);
  (*listextension)->cube = 0;
  return 0;
}

static int lf_abs(int a)
{
  if(a >= 0) return a;
  return -a;
}

static int
lf_get_color_index(
  unsigned short xp_intensity,
  unsigned int *brightnesses,
  int           value_count)
{
  int i;
  int intensity = xp_intensity >> 8;
  int answer = 0;
  int best_brightness = intensity;
  for (i = 0; i < value_count; i++)
  {
    int bs = brightnesses[i];
    if(lf_abs(bs - intensity) < best_brightness)
    {
      best_brightness = lf_abs(bs - intensity);
      answer = i;
    }
  }
  return answer;
}


#ifdef __GNUC__
__inline__
#endif
bool
xccf_color_compare(
  int r1,
  int g1,
  int b1,
  int r2,
  int g2,
  int b2)
{ 
  if(lf_abs(r1 - r2) > 6) return false;
  if(lf_abs(g1 - g2) > 3) return false;
  if(lf_abs(b1 - b2) > 6) return false;
  return true;
}


bool
xccf_allocate_special(
    Display             *display,
    Colormap             colormap,
    cct_cube             cube,
    cct_cube             grayscale,
    unsigned short       intensities[3],
    bool                *really_allocated_return,
    bool                 dont_really_allocate,
    cct_special_color    specials,
    int                  special_count,
    unsigned long       *special_return)
{
  int i;
  int r = intensities[0] >> 8;
  int g = intensities[1] >> 8;
  int b = intensities[2] >> 8;
  int gr;
  if(really_allocated_return) *really_allocated_return = false;
  if(cube)
  {
    switch(cube->cube_type)
    {
    case cube_mapping:
    case cube_no_mapping:
      {
        int color_index[3];
        for(i = 0; i < 3; i++)
          color_index[i] =
	      lf_get_color_index(intensities[i],
				 cube->u.no_mapping.brightnesses[i],
				 cube->u.no_mapping.value_count[i]);
        if(xccf_color_compare(
             cube->u.no_mapping.brightnesses[0][color_index[0]],
             cube->u.no_mapping.brightnesses[1][color_index[1]],
             cube->u.no_mapping.brightnesses[2][color_index[2]],
             r,
             g,
             b))
        {
          *special_return =
            cube->u.no_mapping.pixel_values[0][color_index[0]] +
            cube->u.no_mapping.pixel_values[1][color_index[1]] +
            cube->u.no_mapping.pixel_values[2][color_index[2]];
         if(cube->cube_type == cube_mapping)
           *special_return =
             cube->u.mapping.mapping[*special_return];
         return true;
        }
        break;
      }
    case cube_true_color:
    case cube_grayscale:
      break;
    }
  }
  gr = (r + g + b) / 3;
  if(grayscale /* && xccf_color_compare(r,g,b,gr,gr,gr)*/)
  {
    for(i = 0; i < grayscale->u.grayscale.value_count; i++)
    {
      if(xccf_color_compare(grayscale->u.grayscale.brightnesses[i],
                           grayscale->u.grayscale.brightnesses[i],
                           grayscale->u.grayscale.brightnesses[i],
                           r,
                           g,
                           b))
      {
        *special_return = grayscale->u.grayscale.pixel_values[i];
        return true;
      }
    }
  }
  for(i = 0; i < special_count; i++)
  {
    if(xccf_color_compare(specials[i].brightnesses[0],
                         specials[i].brightnesses[1],
                         specials[i].brightnesses[2],
                         r,
                         g,
                         b))
    {
      *special_return = specials[i].pixel;
      return true;
    }
  }
  if(!dont_really_allocate)
  {
    XColor try;
    int status;
    try.red = intensities[0];
    try.green = intensities[1];
    try.blue = intensities[2];
    try.flags = DoRed | DoGreen | DoBlue;
    status = XAllocColor(display, colormap, &try);
    if(status)
    {
      if(!xccf_color_compare(
                     intensities[0] >> 8, intensities[1] >> 8,
                     intensities[2] >> 8, try.red >> 8, try.green >> 8,
                     try.blue >> 8))
      {
        /* The server gave us a color but it was a really bad match */
        XFreeColors(display, colormap, &try.pixel, 1, 0);
      }
      else
      {
        /* The server gave us a color that worked. */
        *special_return = try.pixel;
        specials[special_count].pixel = try.pixel;
        specials[special_count].brightnesses[0] = try.red >> 8;
        specials[special_count].brightnesses[1] = try.green >> 8;
        specials[special_count].brightnesses[2] = try.blue >> 8;
        if(really_allocated_return) *really_allocated_return = true;
        return true;
      }
    }
  }
  return false;
}
