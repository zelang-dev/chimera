/*
 * dmem.c
 *
 * Copyright (C) 1995-1997, John Kilburg <john@cs.unlv.edu>
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

#include "common.h"

/*
 * free_mem
 */
void
free_mem(m)
void *m;
{
  free(m);
  return;
}

/*
 * calloc_mem
 */
void *
calloc_mem(len, elen)
size_t len, elen;
{
  return(calloc(len, elen));
}

/*
 * alloc_mem
 */
void *
alloc_mem(len)
size_t len;
{
  return(malloc(len));
}

/*
 * realloc_mem
 */
void *
realloc_mem(s, len)
void *s;
size_t len;
{
  return(realloc(s, len));
}

/*
 * alloc_string
 */
char *
alloc_string(str)
char *str;
{
  char *ns;

  ns = (char *)malloc(strlen(str) + 1);
  strcpy(ns, str);
  return(ns);
}
