/*
 * head.c
 *
 * libhtml - HTML->X renderer
 *
 * Copyright (c) 1996-1997, John Kilburg <john@cs.unlv.edu>
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
#include <ctype.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "port_after.h"

#include "html.h"

void
HTMLTitleEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  li->title = HTMLGetEnvText(li->mp, env);

  return;
}

void
HTMLBase(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  char *url;

  if ((url = MLFindAttribute(p, "href")) != NULL) li->burl = url;

  return;
}

void
HTMLMeta(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  return;
}
