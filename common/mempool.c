/*
 * mempool.c
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "port_after.h"

#include "common.h"

/*
 * MPDEBUG - If this is defined then all memory pooling goes away and
 *           malloc is used.  This is useful if you suspect problems
 *           with the memory pooling code (e.g. alignment problems)
 *           or you want to use a malloc checker.
 *
 * MPALIGN - If the alignment size is guessed wrong then this can be
 *           defined to be the appropriate alignment size to override
 *           the automatic thing.
 *
 * MPPOOLSIZE - This is the minimum allocation size.  Maybe changing
 *              it would result in better performance.  Must be
 *              larger than sizeof(struct pool)...probably much larger.
 */

/*
#define MPDEBUG 1
*/

#define MPPOOLSIZE BUFSIZ

#ifndef MPALIGN
typedef struct Alignment
{
  union { int a; char *b; size_t c; off_t d; long e; } align;
} Alignment;
#define MPALIGNSIZE(x) ((x / sizeof(Alignment) + 1) * sizeof(Alignment))
#else
#define MPALIGNSIZE(x) ((x / MPALIGN + 1) * MPALIGN)
#endif

/*
#define TRACKER 1
*/

#ifdef TRACKER
struct track
{
  MemPool mp;
  int line;
  char *file;
  struct track *next;
};

static struct track *track_list = NULL;
#endif

struct pool
{
  size_t used, len;
#ifdef MPDEBUG
  void *mem;
#endif
  struct pool *next;
};

struct MemPoolP
{
  struct pool *plist;
  size_t len;
};

#ifndef MPDEBUG
static struct pool *create_pool _ArgProto((size_t));
static void *use_pool _ArgProto((struct pool *, size_t));

/*
 * create_pool
 */
static struct pool *
create_pool(alen)
size_t alen;
{
  size_t blen, len, plen;
  struct pool *pool;

  plen = MPALIGNSIZE(sizeof(struct pool));
  len = MPALIGNSIZE(alen) + plen;
  blen = (len / MPPOOLSIZE + 1) * MPPOOLSIZE;
  pool = (struct pool *)malloc(blen);
  if (pool == NULL)
  {
    fprintf (stderr, "create_pool malloc failed: %ld\n", (long)blen);
    return(NULL);
  }
  pool->len = blen;
  pool->used = plen;
  pool->next = NULL;

  return(pool);
}

/*
 * use_pool
 */
static void *
use_pool(pool, len)
struct pool *pool;
size_t len;
{
  byte *mem;

  len = MPALIGNSIZE(len);
  if (pool->used + len > pool->len) return(NULL);
  mem = (byte *)pool + pool->used;
  pool->used += len;
  return((void *)mem);
}
#endif

/*
 * MPCreate
 */
MemPool
MPCreateTrack(line, file)
int line;
char *file;
{
  MemPool mp;
  struct pool *pool;
#ifdef TRACKER
  struct track *f;
#endif

#ifndef MPDEBUG
  pool = create_pool(sizeof(struct MemPoolP));
  if (pool == NULL) return(NULL);
  mp = (MemPool)use_pool(pool, sizeof(struct MemPoolP));
  myassert(mp != NULL, "MPCreate: use_pool failed!");
  mp->plist = pool;
  mp->len = pool->len;
#else
  mp = (MemPool)malloc(sizeof(struct MemPoolP));
  if (mp == NULL) return(NULL);
  memset(mp, 0, sizeof(struct MemPoolP));
#endif

#ifdef TRACKER
  f = (struct track *)malloc(sizeof(struct track));
  if (f == NULL) abort();
  f->mp = mp;
  f->line = line;
  f->file = file;
  f->next = track_list;
  track_list = f;
#endif
  
  return(mp);
}

/*
 * MPGet
 */
void *
MPGet(mp, len)
MemPool mp;
size_t len;
{
  struct pool *pool;
  void *mem;

  myassert(len > 0, "MPGet: must allocate sizes greater than zero!");

#ifndef MPDEBUG
  pool = mp->plist;
  if ((mem = use_pool(pool, len)) == NULL)
  {
    pool = create_pool(len);
    pool->next = mp->plist;
    mp->len += pool->len;
    mp->plist = pool;
    mem = use_pool(pool, len);
    myassert(mem != NULL, "MPGet: use_pool failed!");
  }
#else
  pool = (struct pool *)malloc(sizeof(struct pool));
  if (pool == NULL) return(NULL);
  pool->next = mp->plist;
  mp->plist = pool;
  mem = pool->mem = malloc(len);
#endif

  return(mem);
}

/*
 * MPCGet
 */
void *
MPCGet(mp, len)
MemPool mp;
size_t len;
{
  void *m;
  m = MPGet(mp, len);
  memset(m, 0, len);
  return(m);
}

/*
 * MPStrDup
 */
char *
MPStrDup(mp, s)
MemPool mp;
const char *s;
{
  size_t len;
  char *ns;

  if (s == NULL) return(NULL);
  len = strlen(s);
  ns = (char *)MPGet(mp, len + 1);
  strcpy(ns, s);
  return(ns);
}

/*
 * MPDestroy
 */
void
MPDestroy(mp)
MemPool mp;
{
  struct pool *m, *t;
#ifdef TRACKER
  struct track *f;
#endif

  for (m = mp->plist; m != NULL; )
  {
    t = m;
    m = m->next;
#ifdef MPDEBUG
    free(t->mem);
#endif
    free(t);
  }
#ifdef MPDEBUG
  free(mp);
#endif

#ifdef TRACKER
  for (f = track_list; f != NULL; f = f->next)
  {
    if (f->mp == mp)
    {
      f->mp = NULL;
      break;
    }
  }
#endif

  return;
}

/*
 * MPPrintStats
 */
void
MPPrintStatus()
{
#ifdef TRACKER
  struct track *f;
  size_t total;

  total = 0;
  for (f = track_list; f != NULL; f = f->next)
  {
    if (f->mp != NULL)
    {
      fprintf (stderr, "MP unfreed: Line %d, File %s, Length %ld\n",
	       f->line, f->file, (long)f->mp->len);
      total += f->mp->len;
    }
  }

  fprintf (stderr, "Total unfreed: %ld\n", (long)total);
#endif

  return;
}
