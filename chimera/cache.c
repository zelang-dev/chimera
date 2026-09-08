/*
 * cache.c
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
#include <ctype.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <X11/IntrinsicP.h>

#include "port_after.h"

#include "ChimeraP.h"
#include "mime.h"
#include "ml.h"

typedef struct
{
  MemPool mp;                         /* memory descriptor */
  char *filename;                     /* name of cache file */ 
  char *url;                          /* URL string */
  int cid;                            /* cache ID */

  time_t atime;                       /* last access time */
  time_t expires;                     /* expires time */
  time_t mtime;                       /* modified time */

  off_t size;                         /* diskspace used */

  ChimeraCache cc;
} CacheEntry;

struct ChimeraCacheP
{
  MemPool    mp;                      /* memory descriptor */
  char       *dirname;                /* name of cache directory */
  char       *cindex;                 /* cache index file */
  GList      cachelist;               /* list of cache entries */
  bool       persist;                 /* clear cache on exit ? */
  off_t      max_size;                /* maximum size of cache */
  off_t      curr_size;               /* current size of cache */
  bool       ignore_expires;          /* ignore expiration info */
  time_t     ttl;                     /* time-to-live */
  MLState    ml;
  int        refcount;                /* so multiple class destroys work */
  int        next_cid;                /* next cache entry ID */
  ChimeraSourceHooks *chooks;
  ChimeraResources cres;
};

typedef struct
{
  MemPool      mp;
  CacheEntry   *ce;
  FILE         *fp;
  ChimeraSource ws;
  ChimeraResources cres;
  byte         *buffer;
  size_t       len;
  size_t       doff;
  struct stat  s;
  MIMEHeader   mh;
  ChimeraTask  wt;
} CRInfo;

static void CacheEntryDestroy _ArgProto((ChimeraCache, CacheEntry *, bool));
static CacheEntry *AddCacheEntry _ArgProto((ChimeraCache, char *, int));
static void ReadCacheIndex _ArgProto((ChimeraCache));
static CacheEntry *FindCacheEntry _ArgProto((ChimeraCache, char *));
static void CacheElementHandler _ArgProto((void *, MLElement));

static void CRRead _ArgProto((void *));
static void CRGetData _ArgProto((void *, byte **, size_t *, MIMEHeader *));
static void *CRInit _ArgProto((ChimeraSource, ChimeraRequest *, void *));
static void CRDestroy _ArgProto((void *));
static void CRStop _ArgProto((void *));

static void *CSInit _ArgProto((ChimeraSource, ChimeraRequest *, void *));
static char *CSResolve _ArgProto((MemPool, char *, char *));

/*
 * FindCacheEntry
 */
static CacheEntry *
FindCacheEntry(cc, url)
ChimeraCache cc;
char *url;
{
  CacheEntry *ce;
  GList list;

  list = cc->cachelist;
  for (ce = (CacheEntry *)GListGetHead(list); ce != NULL;
       ce = (CacheEntry *)GListGetNext(list))
  {
    if (strlen(url) == strlen(ce->url) &&
	strcasecmp(url, ce->url) == 0) return(ce);
  }

  return(NULL);
}

/*
 * CacheIsDiskCached
 */
bool
CacheIsDiskCached(cc, url)
ChimeraCache cc;
char *url;
{
  return(FindCacheEntry(cc, url) != NULL ? true:false);
}

/*
 * CRGetData
 */
static void
CRGetData(closure, data, len, mh)
void *closure;
byte **data;
size_t *len;
MIMEHeader *mh;
{
  CRInfo *cr = (CRInfo *)closure;

  if (cr->mh == NULL)
  {
    *data = NULL;
    *len = 0;
    *mh = NULL;
  }
  else
  {
    *data = cr->buffer + cr->doff;
    *len = cr->s.st_size - cr->doff;
    *mh = cr->mh;
  }

  return;
}

/*
 * CRStop
 */
static void
CRStop(closure)
void *closure;
{
  CRInfo *cr = (CRInfo *)closure;

  if (cr->wt != NULL)
  {
    TaskRemove(cr->cres, cr->wt);
    cr->wt = NULL;
  }

  return;
}

/*
 * CRDestroy
 */
static void
CRDestroy(closure)
void *closure;
{
  CRInfo *cr = (CRInfo *)closure;

  CRStop(cr);
  if (cr->buffer != NULL) free_mem(cr->buffer);
  if (cr->mh != NULL) MIMEDestroyHeader(cr->mh);
  MPDestroy(cr->mp);

  return;
}

/*
 * CRRead
 */
static void
CRRead(closure)
void *closure;
{
  ssize_t rval;
  CRInfo *cr = (CRInfo *)closure;

  cr->wt = NULL;

  cr->buffer = (byte *)alloc_mem(cr->s.st_size);
  rval = fread(cr->buffer, 1, cr->s.st_size, cr->fp);
  if (rval == cr->s.st_size)
  {
    if (MIMEFindData(cr->mh, cr->buffer, cr->s.st_size, &cr->doff) == 0)
    {
      MIMEParseBuffer(cr->mh, cr->buffer, cr->doff);
      SourceInit(cr->ws, true);
      SourceEnd(cr->ws);
    }
    else SourceStop(cr->ws, "");
  }
  else SourceStop(cr->ws, "");

  fclose(cr->fp);
  cr->fp = NULL;

  return;
}

/*
 * CRInit
 */
static void *
CRInit(ws, wr, closure)
ChimeraSource ws;
ChimeraRequest *wr;
void *closure;
{
  ChimeraCache cc = (ChimeraCache)closure;
  CRInfo *cr;
  CacheEntry *ce;
  MemPool mp;

  if ((ce = FindCacheEntry(cc, wr->url)) == NULL) return(NULL);

  mp = MPCreate();
  cr = (CRInfo *)MPCGet(mp, sizeof(CRInfo));
  cr->mp = mp;
  cr->ws = ws;
  cr->ce = ce;
  cr->cres = cc->cres;
  cr->mh = MIMECreateHeader();

  if (ce->filename == NULL || stat(ce->filename, &cr->s) == -1 ||
      (cr->fp = fopen(ce->filename, "r")) == NULL)
  {
    CRDestroy(cr);
    return(NULL);
  }
  
  cr->wt = TaskSchedule(cr->cres, CRRead, cr);

  return(cr);
}

/*
 * AddCacheEntry
 */
static CacheEntry *
AddCacheEntry(cc, url, cid)
ChimeraCache cc;
char *url;
int cid;
{
  CacheEntry *ce;
  char *filename;
  MemPool mp;
  size_t len;

  mp = MPCreate();
  ce = (CacheEntry *)MPCGet(mp, sizeof(CacheEntry));
  ce->mp = mp;
  ce->cc = cc;

  GListAddHead(cc->cachelist, ce);

  len = strlen(cc->dirname) + strlen("/.ccf") + 26;
  filename = (char *)MPGet(mp, len);

  if (cid == -1) ce->cid = cc->next_cid++;
  else
  {
    ce->cid = cid;
    if (cid > cc->next_cid) cc->next_cid = cid + 1;
  }
  
  snprintf (filename, len, "%s/%d.ccf", cc->dirname, ce->cid);

  ce->filename = filename;
  ce->url = MPStrDup(mp, url);

  return(ce);
}

/*
 * CacheWrite
 */
int
CacheWrite(cc, ws)
ChimeraCache cc;
ChimeraSource ws;
{
  FILE *fp;
  struct stat s;
  CacheEntry *ce;
  char *url;
  byte *data;
  size_t len;
  MIMEHeader mh;

  SourceGetData(ws, &data, &len, &mh);
  if (data == NULL || len == 0 || mh == NULL) return(-1);

  if (MIMEGetField(mh, "x-url", &url) != 0 || url == NULL) return(-1);

  if ((ce = FindCacheEntry(cc, url)) != NULL)
  {
    CacheEntryDestroy(cc, ce, false);
  }

  ce = AddCacheEntry(cc, url, -1);

  if ((fp = fopen(ce->filename, "w")) == NULL)
  {
    CacheEntryDestroy(cc, ce, false);
    return(-1);
  }

  MIMEWriteHeader(mh, fp);

  if (fwrite(data, 1, len, fp) < len)
  {
    fclose(fp);
    CacheEntryDestroy(cc, ce, false);
    return(-1);
  }
  fclose(fp);

  s.st_size = 0;
  stat(ce->filename, &s);
  ce->size = s.st_size;

  cc->curr_size += ce->size;

  return(0);
}

/*
 * CacheEntryDestroy
 */
void
CacheEntryDestroy(cc, ce, persist)
ChimeraCache cc;
CacheEntry *ce;
bool persist;
{
  GListRemoveItem(cc->cachelist, ce);
  cc->curr_size -= ce->size;
  if (!persist && ce->filename != NULL) unlink(ce->filename);
  MPDestroy(ce->mp);

  return;
}

/*
 * CacheDestroy
 */
void
CacheDestroy(cc)
ChimeraCache cc;
{
  FILE *fp;
  CacheEntry *ce;
  GList list;

  list = cc->cachelist;
  cc->refcount--;
  if (cc->refcount > 0) return;

  if (cc->persist)
  {
    /*
     * This can take a really long time so its only done once at the end.
     */
    if ((fp = fopen(cc->cindex, "w")) != NULL)
    {
      fprintf (fp, "<html>\n");
      fprintf (fp, "<head><title>Chimera Cache Index</title></head>\n");
      fprintf (fp, "<body>\n");
      fprintf (fp, "<ul>\n");
      for (ce = (CacheEntry *)GListGetHead(list); ce != NULL;
	   ce = (CacheEntry *)GListGetNext(list))
      {
	fprintf (fp, "<li><a size=%lu cid=%d href=\"%s\">%s</a>\n",
		 (unsigned long)ce->size, ce->cid, ce->url, ce->url);
      }
      fprintf (fp, "</ul>\n");
      fprintf (fp, "</body>\n");
      fprintf (fp, "</html>\n");
      fclose(fp);
    }
  }
  else unlink(cc->cindex);

  while ((ce = GListPop(list)) != NULL)
  {
    CacheEntryDestroy(cc, ce, cc->persist);
  }

  MPDestroy(cc->mp);

  return;
}

/*
 * CacheElementHandler
 */
static void
CacheElementHandler(closure, p)
void *closure;
MLElement p;
{
  ChimeraCache cc = (ChimeraCache)closure;
  CacheEntry *ce;
  char *href;
  char *sstr;
  char *cidstr;
  size_t size;
  char *name;

  if ((name = MLTagName(p)) == NULL || MLGetType(p) == ML_ENDTAG) return;

  if (strcasecmp("a", name) == 0)
  {
    if ((href = MLFindAttribute(p, "href")) != NULL &&
        (sstr = MLFindAttribute(p, "size")) != NULL &&
        (cidstr = MLFindAttribute(p, "cid")) != NULL)
    {
      size = (size_t)atoi(sstr);
      if (size > 0 && (ce = AddCacheEntry(cc, href, atoi(cidstr))) != NULL)
      {
        ce->size = size;
        cc->curr_size += size;
      }
    }
  }

  return;
}

/*
 * ReadCacheIndex
 */
static void
ReadCacheIndex(cc)
ChimeraCache cc;
{
  FILE *fp;
  struct stat s;
  char *bdata;

  if (stat(cc->cindex, &s) != -1 && (fp = fopen(cc->cindex, "r")) != NULL)
  {
    bdata = (char *)alloc_mem(s.st_size);
    if (fread(bdata, 1, s.st_size, fp) == s.st_size)
    {
      cc->ml = MLInit(CacheElementHandler, cc);
      MLEndData(cc->ml, bdata, (size_t)s.st_size);
      MLDestroy(cc->ml);
    }
    free_mem(bdata);
    fclose(fp);
  }

  return;
}

/*
 * CSInit
 */
static void *
CSInit(ws, wr, closure)
ChimeraSource ws;
ChimeraRequest *wr;
void *closure;
{
  ChimeraCache cc = (ChimeraCache)closure;
  CRInfo *cr;
  CacheEntry *ce;
  MemPool mp;
  char *url;

  for (url = wr->url; *url != '\0' && *url != ':'; url++)
      ;
  if (*url == '\0') return(NULL);
  url++;

  if ((ce = FindCacheEntry(cc, url)) == NULL) return(NULL);

  mp = MPCreate();
  cr = (CRInfo *)MPCGet(mp, sizeof(CRInfo));
  cr->mp = mp;
  cr->ws = ws;
  cr->ce = ce;

  if (ce->filename == NULL || stat(ce->filename, &cr->s) == -1 ||
      (cr->fp = fopen(ce->filename, "r")) == NULL)
  {
    CRDestroy(cr);
    return(NULL);
  }
  
  cr->wt = TaskSchedule(cc->cres, CRRead, cr);

  return(cr);
}

/*
 * CSResolve
 */
static char *
CSResolve(mp, url, base)
MemPool mp;
char *url;
char *base;
{
  return(MPStrDup(mp, url));
}

/*
 * CacheGetHooks
 */
ChimeraSourceHooks *
CacheGetHooks(cc)
ChimeraCache cc;
{
  return(cc->chooks);
}

/*
 * CacheCreate
 */
ChimeraCache
CacheCreate(cres)
ChimeraResources cres;
{
  ChimeraCache cc;
  char *dirname;
  MemPool mp;
  char filename[MAXFILENAMELEN + 1];
  ChimeraSourceHooks hooks;
  struct stat s;
  int t;

  mp = MPCreate();

  if ((dirname = ResourceGetFilename(cres, mp, "cache.directory")) == NULL)
  {
    fprintf (stderr, "Cache resource not found.  Not caching.\n");
    MPDestroy(mp);
    return(NULL);
  }

  if (stat(dirname, &s) != 0)
  {
    fprintf (stderr, "Cache directory not found.  Not caching.\n");
    MPDestroy(mp);
    return(NULL);
  }

  cc = (ChimeraCache)MPCGet(mp, sizeof(struct ChimeraCacheP));
  cc->mp = mp;
  cc->dirname = dirname;
  strcpy(filename, cc->dirname);
  strcat(filename, "/");
  strcat(filename, "ccfindex.html");
  cc->cindex = FixPath(mp, filename);
  cc->cachelist = GListCreateX(cc->mp);
  cc->cres = cres;

  ResourceGetBool(cres, "cache.persist", &cc->persist);
  ResourceGetBool(cres, "cache.ignoreExpires", &cc->ignore_expires);
  if (ResourceGetInt(cres, "cache.maxSize", &t) == NULL || t < 0)
  {
    cc->max_size = 0;
  }
  else cc->max_size = (off_t)t;

  if (ResourceGetInt(cres, "cache.ttl", &t) == NULL || t < 0)
  {
    cc->ttl = 0;
  }
  else cc->ttl = (time_t)t;

  ReadCacheIndex(cc);

  cc->chooks = (ChimeraSourceHooks *)MPCGet(cres->mp,
					    sizeof(ChimeraSourceHooks));
  cc->chooks->init = CRInit;
  cc->chooks->destroy = CRDestroy;
  cc->chooks->stop = CRStop;
  cc->chooks->getdata = CRGetData;
  cc->chooks->class_closure = cc;

  memset(&hooks, 0, sizeof(hooks));
  hooks.name = "x-cache";
  hooks.init = CSInit;
  hooks.destroy = CRDestroy;
  hooks.stop = CRStop;
  hooks.getdata = CRGetData;
  hooks.class_closure = cc;
  hooks.resolve_url = CSResolve;
  SourceAddHooks(cres, &hooks);

  return(cc);
}
