/*
 * list.c
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "port_after.h"

#include "common.h"

struct GNodeP
{
  void *thing;
  struct GNodeP *prev, *next;
};

struct GListP
{
  bool localmp;
  MemPool mp;
  struct GNodeP *f;
  struct GNodeP *head, *tail;
  struct GNodeP *c;
};

/*
#define TRACKER 1
*/

#ifdef TRACKER
struct track
{
  GList gl;
  int line;
  char *file;
  struct track *next;
};

static struct track *track_list = NULL;
#endif

static void FreeListNode _ArgProto((GList, struct GNodeP *));
static struct GNodeP *GetFreedNode _ArgProto((GList));

static void
FreeListNode(gl, f)
GList gl;
struct GNodeP *f;
{
#ifdef TRACKER
  memset(f, 0, sizeof(struct GNodeP));
#endif
  f->next = gl->f;
  gl->f = f;
  return;
}

static struct GNodeP *
GetFreedNode(gl)
GList gl;
{
  struct GNodeP *f;

  return(NULL);

  if (gl->f == NULL) return(NULL);
  f = gl->f;
  gl->f = f->next;
  memset(f, 0, sizeof(struct GNodeP));
  return(f);
}

GList
GListCreateX(mp)
MemPool mp;
{
  GList gl;

  gl = (GList)MPCGet(mp, sizeof(struct GListP));
  gl->mp = mp;
  gl->localmp = false;

  return(gl);
}

GList
GListCreateTrack(line, file)
int line;
char *file;
{
  MemPool mp;
  GList gl;
#ifdef TRACKER
  struct track *f;
#endif

  mp = MPCreate();
  gl = (GList)MPCGet(mp, sizeof(struct GListP));
  gl->mp = mp;
  gl->localmp = true;

#ifdef TRACKER
  f = (struct track *)malloc(sizeof(struct track));
  f->gl = gl;
  f->line = line;
  f->file = file;
  f->next = track_list;
  track_list = f;
#endif

  return(gl);
}

void *
GListPop(gl)
GList gl;
{
  void *thing;
  struct GNodeP *f;

  if (gl->head != NULL)
  {
    thing = gl->head->thing;
    f = gl->head;
    if (gl->head != NULL) gl->head = gl->head->next;
    if (gl->head != NULL) gl->head->prev = NULL;
    gl->c = gl->head;
    FreeListNode(gl, f);

    return(thing);
  }
  return(NULL);
}

void
GListAddHead(gl, thing)
GList gl;
void *thing;
{
  struct GNodeP *gn;

  if ((gn = GetFreedNode(gl)) == NULL)
  {
    gn = (struct GNodeP *)MPCGet(gl->mp, sizeof(struct GNodeP));
  }
  gn->next = gl->head;
  if (gl->head != NULL) gl->head->prev = gn;
  gl->head = gn;
  if (gl->tail == NULL) gl->tail = gn;
  gn->thing = thing;

  return;
}

void
GListAddTail(gl, thing)
GList gl;
void *thing;
{
  struct GNodeP *gn;

  if ((gn = GetFreedNode(gl)) == NULL)
  {
    gn = (struct GNodeP *)MPCGet(gl->mp, sizeof(struct GNodeP));
  }
  gn->prev = gl->tail;
  if (gl->tail != NULL) gl->tail->next = gn;
  gl->tail = gn;
  if (gl->head == NULL) gl->head = gn;
  gn->thing = thing;

  return;
}

void
GListDestroy(gl)
GList gl;
{
#ifdef TRACKER
  struct track *f;
#endif

  if (gl->localmp) MPDestroy(gl->mp);

#ifdef TRACKER
  for (f = track_list; f != NULL; f = f->next)
  {
    if (f->gl == gl)
    {
      f->gl = NULL;
      break;
    }
  }
#endif

  return;
}

void *
GListGetHead(gl)
GList gl;
{
  if (gl->head == NULL) return(NULL);
  gl->c = gl->head;
  return(gl->head->thing);
}

void *
GListGetTail(gl)
GList gl;
{
  if (gl->tail == NULL) return(NULL);
  gl->c = gl->tail;
  return(gl->tail->thing);
}

void *
GListGetNext(gl)
GList gl;
{
  if (gl->c == NULL) return(NULL);
  gl->c = gl->c->next;
  if (gl->c == NULL) return(NULL);
  return(gl->c->thing);
}

void *
GListGetPrev(gl)
GList gl;
{
  if (gl->c == NULL) return(NULL);
  gl->c = gl->c->prev;
  if (gl->c == NULL) return(NULL);
  return(gl->c->thing);
}

void *
GListGetCurrent(gl)
GList gl;
{
  if (gl->c == NULL) return(NULL);
  return(gl->c->thing);
}

void
GListRemoveItem(gl, thing)
GList gl;
void *thing;
{
  struct GNodeP *c, *n;

  for (c = gl->head; c != NULL; )
  {
    if (c->thing == thing)
    {
      if (gl->c == c) gl->c = NULL;

      n = c->next;

      if (c == gl->head) gl->head = c->next;
      if (c == gl->tail) gl->tail = c->prev;
      if (c->next != NULL) c->next->prev = c->prev;
      if (c->prev != NULL) c->prev->next = c->next;

      FreeListNode(gl, c);

      c = n;
    }
    else c = c->next;
  }

  return;
}

bool
GListEmpty(gl)
GList gl;
{
  return(gl == NULL || gl->head == NULL);
}

MemPool
GListMP(gl)
GList gl;
{
  return(gl->mp);
}

void
GListPrintStatus()
{
#ifdef TRACKER
  struct track *f;

  for (f = track_list; f != NULL; f = f->next)
  {
    if (f->gl != NULL)
    {
      fprintf (stderr, "GList unfreed: Line %d, File %s\n", f->line, f->file);
    }
  }
#endif
  return;
}

void
GListClear(gl)
GList gl;
{
  struct GNodeP *c, *t;
  gl->c = NULL;
  c = gl->head;
  t = NULL;
  while (c != NULL)
  {
    t = c;
    c = c->next;
  }
  if (t != NULL)
  {
    t->next = gl->f;
    gl->f = gl->head;
  }
  gl->head = NULL;
  gl->tail = NULL;
  return;
}
