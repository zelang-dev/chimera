/*
 * map.c
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

#include <math.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "port_after.h"

#include "html.h"

typedef enum
{
  HTMLMapShapeRect,
  HTMLMapShapeCircle,
  HTMLMapShapePoly,
  HTMLMapShapeDefault
} HTMLMapShape;

struct HTMLAreaP
{
  HTMLMapShape shape;
  int *coords;
  int count;
  char *url;
  Region r;
};

struct HTMLMapP
{
  char *name;
  GList areas;
  HTMLArea defarea;
};

static void MakeArea _ArgProto((HTMLInfo, HTMLMap, MLElement));

static void
MakeArea(li, map, p)
HTMLInfo li;
HTMLMap map;
MLElement p;
{
  HTMLArea area;
  char *shape;
  char *href;
  char *coords;
  int i, j;
  char *cp;
  XPoint *xp;

  area = (HTMLArea)MPCGet(li->mp, sizeof(struct HTMLAreaP));

  if ((coords = MLFindAttribute(p, "coords")) != NULL)
  {
    for (cp = coords, i = 0; *cp != '\0'; cp++)
    {
      if (*cp == ',') i++;
    }
    i++;
    area->coords = (int *)MPCGet(li->mp, sizeof(int) * i);
    area->coords[0] = atoi(coords);
    for (cp = coords, i = 1; *cp != '\0'; cp++)
    {
      if (*cp == ',') area->coords[i++] = atoi(cp + 1);
    }
    area->count = i;
  }

  if ((shape = MLFindAttribute(p, "shape")) == NULL) shape = "rect";

  if (strcasecmp(shape, "circle") == 0)
  {
    area->shape = HTMLMapShapeCircle;
    GListAddTail(map->areas, area);
  }
  else if (strcasecmp(shape, "poly") == 0)
  {
    area->shape = HTMLMapShapePoly;
    if (area->count / 2 >= 3)
    {
      GListAddTail(map->areas, area);
      xp = (XPoint *)MPCGet(li->mp, sizeof(XPoint) * area->count / 2);
      for (j = 0, i = 0; j < area->count / 2; i += 2, j++)
      {
	xp[j].x = area->coords[i];
	xp[j].y = area->coords[i + 1];
      }
      area->r = XPolygonRegion(xp, area->count / 2, WindingRule);
    }
  }
  else if (strcasecmp(shape, "default") == 0)
  {
    area->shape = HTMLMapShapeDefault;
    if (map->defarea == NULL) map->defarea = area;
  }
  else
  {
    area->shape = HTMLMapShapeRect;
    GListAddTail(map->areas, area);
  }

  if ((href = MLFindAttribute(p, "href")) != NULL) area->url = href;

  return;
}

void
HTMLMapBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLMap map;
  char *name;

  if ((name = MLFindAttribute(p, "name")) == NULL) return;

  map = (HTMLMap)MPCGet(li->mp, sizeof(struct HTMLMapP));
  map->name = name;
  map->areas = GListCreateX(li->mp);
  env->closure = map;

  return;
}

void
HTMLAreaBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLMap map;

  /*
   * Area doesn't create an environment so the current environment
   * must be the map environment.
   */
  map = (HTMLMap)env->closure;
  MakeArea(li, map, p);

  return;
}

void
HTMLMapEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLMap map;

  map = (HTMLMap)env->closure;
  GListAddTail(li->maps, map);

  return;
}

char *
HTMLFindMapURL(li, url, x, y)
HTMLInfo li;
char *url;
int x, y;
{
  HTMLArea area;
  HTMLMap map;
  GList list;
  int dx, dy;
  double rd;
  URLParts *up;
  MemPool mp;

  mp = MPCreate();
  if ((up = URLParse(mp, url)) == NULL || up->hostname != NULL ||
      up->filename != NULL || up->fragment == NULL)
  {
    MPDestroy(mp);
    fprintf (stderr, "Local maps supported only.\n");
    return(NULL);
  }

  list = li->maps;
  for (map = (HTMLMap)GListGetHead(list); map != NULL;
       map = (HTMLMap)GListGetNext(list))
  {
    if (strcasecmp(map->name, up->fragment) == 0) break;
  }
  MPDestroy(mp);
  if (map == NULL) return(NULL);

  list = map->areas;
  for (area = (HTMLArea)GListGetHead(list); area != NULL; 
       area = (HTMLArea)GListGetNext(list))
  {
    if (area->shape == HTMLMapShapeRect)
    {
      if (area->count == 4 &&
	  area->coords[0] < x && x < area->coords[2] &&
	  area->coords[1] < y && y < area->coords[3])
      {
	return(area->url);
      }
    }
    else if (area->shape == HTMLMapShapeCircle)
    {
      dx = x - area->coords[0];
      dy = y - area->coords[1];
      rd = sqrt((double)(dx * dx + dy * dy));
      if ((int)rd < area->coords[2]) return(area->url);
    }
    else if (area->shape == HTMLMapShapePoly)
    {
      if (area->r != NULL && XPointInRegion(area->r, x, y)) return(area->url);
    }
  }

  if (map->defarea != NULL) return(map->defarea->url);

  return(NULL);
}

/*
 * HTMLMapAccept
 */
bool
HTMLMapAccept(li, obj)
HTMLInfo li;
HTMLObject obj;
{
  return(true);
}

/*
 * HTMLAreaInsert
 */
HTMLInsertStatus
HTMLAreaInsert(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  if (HTMLTagToID(env->tag) != TAG_MAP) return(HTMLInsertReject);
  return(HTMLInsertOK);
}
