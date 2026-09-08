/* xcolorcube.h
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

#ifndef XCOLORCUBE_H_INCLUDED
#define XCOLORCUBE_H_INCLUDED

#include "colorcube.h"

/*
 * Allocates the best possible color cuboid for the given colormap
 * and display. If you ask for a cuboid for a display/colormap that
 * was already asked for it returns the cached value from last time
 * so don't free the return value without thinking about it.
 * Should be able to cope with a program displaying on several different
 * displays
 */

cct_cube
xccf_allocate_cube(
    Display *display,
    Colormap colormap,
    Visual *visual,
    int depth);

/*
 * Allocates a decent number of gray colormap entries on the colormap
 * and display. If you ask for grays for a display/colormap that
 * was already asked for it returns the cached value from last time
 * so don't free the return value without thinking about it.
 * Should be able to cope with a program displaying on several different
 * displays
 */

cct_cube
xccf_allocate_grays(
    Display *display,
    Colormap colormap,
    Visual *visual,
    int depth);

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
    unsigned long       *special_return);

bool
xccf_color_compare(
  int r1,
  int g1,
  int b1,
  int r2,
  int g2,
  int b2);


#endif /* XCOLORCUBE_H_INCLUDED */
