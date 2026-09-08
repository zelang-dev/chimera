/*
 * source.c
 *
 * Copyright (c) 1996-1998, John Kilburg <john@cs.unlv.edu>
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

#include <X11/IntrinsicP.h>

#include "port_after.h"

#include "ChimeraP.h"
#include "mime.h"

typedef enum
{
  SinkStateWaiting,       /* waiting for beginning of data */
  SinkStateLoading,       /* waiting for completion of data */
  SinkStateComplete,      /* has received all data */
  SinkStateCancelled,     /* has been cancelled */
  SinkStateIgnore,        /* ignore callbacks for this sink */
  SinkStateDestroyed      /* dead */
} SinkState;

typedef enum
{
  SourceStateWaiting,
  SourceStateBlocked,
  SourceStateComplete,
  SourceStateLame,
  SourceStateLoading
} SourceState;

struct ChimeraSinkP
{
  MemPool           mp;
  SinkState         state;
  ChimeraSource     ws;
  ChimeraSinkHooks  dhooks;
  void              *closure;
  ChimeraTask       wt;
  int               debug;
};

struct ChimeraSourceP
{
  MemPool           mp;
  ChimeraResources  cres;
  ChimeraSourceHooks shooks;
  void              *closure;
  GList		    sinks;

  int               debugcount;

  /* only used if the source is blocked.  do not free in this module. */
  ChimeraRequest    *wr;

  /* Various status goodies */
  SourceState       sstate;             /* transfer state */
  bool              cachable;           /* can this be cached? */
  bool              fromcache;          /* was this fetched from cache? */
};

/*
 * Private functions
 */
static ChimeraSource SourceCreate _ArgProto((ChimeraResources,
					     ChimeraRequest *));
void SourceDestroy _ArgProto((ChimeraSource));
static void SourceInitTask _ArgProto((void *));
static void SourceEndTask _ArgProto((void *));
static void SinkCancelTask _ArgProto((void *));
static char *GetSourceInfo _ArgProto((ChimeraSource, char *));
static void StartBlockedSource _ArgProto((ChimeraResources));
static int StartSource _ArgProto((ChimeraResources, ChimeraSource));
static ChimeraSource TryMemoryCache _ArgProto((ChimeraResources,
					       ChimeraRequest *));
static void ChangeSinkState _ArgProto((ChimeraSink, ChimeraTaskProc,
				       SinkState));

/*
 * ChangeSinkState
 *
 * Should check for reasonable transitions?
 */
static void
ChangeSinkState(wp, func, newstate)
ChimeraSink wp;
ChimeraTaskProc func;
SinkState newstate;
{
  if (wp->wt != NULL)
  {
    TaskRemove(wp->ws->cres, wp->wt);
    wp->wt = NULL;
  }
  wp->state = newstate;
  if (func != NULL) wp->wt = TaskSchedule(wp->ws->cres, func, wp);

  return;
}

/*
 * SourceDestroy
 */
void
SourceDestroy(ws)
ChimeraSource ws;
{
  ChimeraSink c;
  ChimeraResources cres = ws->cres;

  if (cres->printLoadMessages)
  {
    fprintf (stderr, "Source Destroy %s\n", ws->wr->url);
  }

  for (c = (ChimeraSink)GListGetHead(ws->sinks); c != NULL;
       c = (ChimeraSink)GListGetNext(ws->sinks))
  {
    myassert(c->state == SinkStateDestroyed, "Sink not destroyed");

    if (c->wt != NULL) TaskRemove(cres, c->wt);
    MPDestroy(c->mp);    
  }

  if (ws->closure != NULL) CMethod(ws->shooks.destroy)(ws->closure);
  GListRemoveItem(cres->sources, ws);
  MPDestroy(ws->wr->mp);
  MPDestroy(ws->mp);

  return;
}

/*
 * SourceCreate
 *
 * This function gobbles up 'wr'.
 */
static ChimeraSource
SourceCreate(cres, wr)
ChimeraResources cres;
ChimeraRequest *wr;
{
  ChimeraSource c;
  int count;
  ChimeraSource ws;
  MemPool mp;

  mp = MPCreate();
  ws = (ChimeraSource)MPCGet(mp, sizeof(struct ChimeraSourceP));
  ws->mp = mp;
  ws->cres = cres;
  ws->cachable = true;
  ws->sstate = SourceStateWaiting;
  ws->sinks = GListCreateX(mp);
  ws->wr = wr;

  /*
   * Count the number of loading and waiting (active) sources.  If there are
   * already some number (maxiocnt) of active sources then block this
   * source.
   */
  count = 0;
  for (c = (ChimeraSource)GListGetHead(cres->sources); c != NULL;
       c = (ChimeraSource)GListGetNext(cres->sources))
  {
    if (c->sstate == SourceStateLoading || c->sstate == SourceStateWaiting)
    {
      count++;
      if (count == cres->maxiocnt)
      {
	ws->sstate = SourceStateBlocked;
	GListAddHead(cres->sources, ws);
	if (cres->printLoadMessages)
	{
	  fprintf (stderr, "Source Block %s\n", wr->url);
	}
	return(ws);
      }
    }
  }

  GListAddHead(cres->sources, ws);

  if (StartSource(cres, ws) == -1) return(NULL);

  return(ws);
}

/*
 * StartSource
 */
static int
StartSource(cres, ws)
ChimeraResources cres;
ChimeraSource ws;
{
  ChimeraSourceHooks *hooks;
  ChimeraRequest *wr = ws->wr;
  char *detail = "error";

  /*
   * Look in the cache if the reload flag isn't set.  Copy the cache
   * source hooks to be used later.
   */
  if (!wr->reload && wr->input_data == NULL && cres->cc != NULL)
  {
    hooks = CacheGetHooks(cres->cc);
    if (hooks != NULL)
    {
      ws->closure = CMethodVoidPtr(hooks->init)(ws, wr, hooks->class_closure);
      if (ws->closure != NULL)
      {
	memcpy(&(ws->shooks), hooks, sizeof(ws->shooks));
	ws->fromcache = true;
	detail = "Cache";
      }
    }
  }

  /*
   * Cache could not be attempted or the cache grab failed.  Try the
   * real handler.
   */
  if (ws->closure == NULL)
  {
    ws->closure = CMethodVoidPtr(wr->hooks.init)(ws,
                                                 wr, wr->hooks.class_closure);
    memcpy(&(ws->shooks), &(wr->hooks), sizeof(ws->shooks));    
    if (ws->closure != NULL) detail = wr->hooks.name;
  }

  /*
   * Couldn't start a source.  Just give up and return nothing.
   */
  if (ws->closure == NULL)
  {
    if (cres->printLoadMessages)
    {
      fprintf (stderr, "Source StartFailed %s\n", wr->url);
    }
    SourceDestroy(ws);

    return(-1);
  }

  ws->sstate = SourceStateWaiting;

  if (cres->printLoadMessages)
  {
    fprintf (stderr, "Source StartSuccess(%s) %s\n", detail, wr->url);
  }

  return(0);
}

/*
 * StartBlockedSource
 */
static void
StartBlockedSource(cres)
ChimeraResources cres;
{
  ChimeraSource c;

  for (c = (ChimeraSource)GListGetHead(cres->sources); c != NULL; )
  {
    if (c->sstate == SourceStateBlocked)
    {
      if (cres->printLoadMessages)
      {
	fprintf (stderr, "Source StartBlocked %s\n", c->wr->url);
      }
      if (StartSource(cres, c) == 0) break;
      else c = (ChimeraSource)GListGetHead(cres->sources);
    }
    else c = (ChimeraSource)GListGetNext(cres->sources);
  }

  return;
}

/*
 * SourceEndTask
 */
static void
SourceEndTask(closure)
void *closure;
{
  ChimeraSink wp = (ChimeraSink)closure;

  myassert(wp->state == SinkStateComplete, "Sink not complete!");

  wp->wt = NULL;

  CMethod(wp->dhooks.end)(wp->closure);

  return;
}

/*
 * SourceInitTask
 */
static void
SourceInitTask(closure)
void *closure;
{
  ChimeraSink wp = (ChimeraSink)closure;
  ChimeraSource ws = wp->ws;
  int debug;

  wp->wt = NULL;

  myassert(wp->state == SinkStateWaiting, "Sink not waiting");

  debug = wp->debug;
  if (CMethodInt(wp->dhooks.init)(wp, wp->closure) != 0)
  {
    /*
     * Don't make any references to 'wp'.  The init routine probably
     * wants to do a Cancel or Destroy.
     */
    if (ws->cres->printLoadMessages)
    {
      fprintf (stderr, "Sink(%d) Init failed %s\n", debug, ws->wr->url);
    }
    return;
  }
  
  if (wp->ws->sstate == SourceStateComplete)
  {
    ChangeSinkState(wp, SourceEndTask, SinkStateComplete);
  }
  else ChangeSinkState(wp, NULL, SinkStateLoading);
  
  return;
}

/*
 * TryMemoryCache
 *
 * Rummage around in the list of current downloads to see if what we need
 * is already being downloaded.
 */
static ChimeraSource
TryMemoryCache(cres, wr)
ChimeraResources cres;
ChimeraRequest *wr;
{
  ChimeraSource ws;
  char *content;

  /*
   * Input data can make the output different everytime don't even
   * try to find something that has been cached.
   */
  if (wr->input_data != NULL) return(NULL);

  for (ws = (ChimeraSource)GListGetHead(cres->sources); ws != NULL;
       ws = (ChimeraSource)GListGetNext(cres->sources))
  {
    /*
     * Obviously the URL has to match.
     */
    if (RequestCompareURL(ws->wr, wr))
    {
      /*
       * Check to make sure the source is usable and then check to make sure
       * that the same content-types are desired.
       */
      if (ws->sstate != SourceStateLame && ws->cachable)
      {
	if (RequestCompareAccept(ws->wr, wr)) return(ws);
	if (cres->printLoadMessages)
	{
	  fprintf (stderr, "MemCache AcceptListDifferent %s\n", wr->url);
	}
      }
      else if (cres->printLoadMessages)
      {
	fprintf (stderr, "MemCache ");
	switch (ws->sstate)
	{
	  case SourceStateWaiting: fprintf (stderr, "StateWaiting: "); break;
	  case SourceStateBlocked: fprintf (stderr, "StateBlocked: "); break;
	  case SourceStateComplete: fprintf (stderr, "StateComplete: "); break;
	  case SourceStateLame: fprintf (stderr, "StateLame: "); break;
	  case SourceStateLoading: fprintf (stderr, "StateLoading: "); break;
	  default: fprintf (stderr, "EvilState(%d): ", ws->sstate);
	}
	fprintf (stderr, "%s\n", ws->wr->url);
      }
    }
  }
  
  return(NULL);
}

/*
 * SourceStop
 *
 * This function is called by the source implementation to indicate that
 * input is stopping for some reason.  Since its incomplete the source
 * is marked (Lame) so that the in-memory version won't be used again.
 * A blocked source is started.
 */
void
SourceStop(ws, reason)
ChimeraSource ws;
char *reason;
{
  if (ws->sstate == SourceStateLame) return;

  if (ws->cres->printLoadMessages)
  {
    fprintf (stderr, "Source Stop %s\n", ws->wr->url);
  }

  ws->sstate = SourceStateLame;

  StartBlockedSource(ws->cres);

  return;
}

/*
 * SourceEnd
 */
void
SourceEnd(ws)
ChimeraSource ws;
{
  ChimeraSink c;

  myassert(ws->sstate == SourceStateLoading, "End source in wrong state");

  if (ws->cres->printLoadMessages)
  {
    fprintf (stderr, "Source End %s\n", ws->wr->url);
  }

  ws->sstate = SourceStateComplete;

  for (c = (ChimeraSink)GListGetHead(ws->sinks); c != NULL;
       c = (ChimeraSink)GListGetNext(ws->sinks))
  {
    if (c->state == SinkStateLoading)
    {
      ChangeSinkState(c, SourceEndTask, SinkStateComplete);
    }
  }

  if (ws->cres->cc != NULL && ws->cachable && !ws->fromcache)
  {
    CacheWrite(ws->cres->cc, ws);
  }

  StartBlockedSource(ws->cres);

  return;
}

/*
 * SourceAddTask
 */
static void
SourceAddTask(closure)
void *closure;
{
  ChimeraSink wp = (ChimeraSink)closure;

  myassert(wp->state == SinkStateLoading, "Sink not loading!");

  wp->wt = NULL;

  CMethod(wp->dhooks.add)(wp->closure);

  return;
}

/*
 * SourceAdd
 */
void
SourceAdd(ws)
ChimeraSource ws;
{
  ChimeraSink c;

  myassert(ws->sstate == SourceStateLoading, "Add source in wrong state");

  if (ws->cres->printLoadMessages)
  {
    fprintf (stderr, "Source Add %s\n", ws->wr->url);
  }

  for (c = (ChimeraSink)GListGetHead(ws->sinks); c != NULL;
       c = (ChimeraSink)GListGetNext(ws->sinks))
  {
    if (c->state == SinkStateLoading)
    {
      ChangeSinkState(c, SourceAddTask, SinkStateLoading);
    }
  }

  return;
}

/*
 * SourceInit
 */
void
SourceInit(ws, usecache)
ChimeraSource ws;
bool usecache;
{
  ChimeraSink c;
  char *url;

  myassert(ws->sstate == SourceStateWaiting, "Init source in wrong state.");
  ws->cachable = usecache;

  ws->sstate = SourceStateLoading;

  myassert((url = GetSourceInfo(ws, "x-url")) != NULL, "URL is NULL");
  myassert(GetSourceInfo(ws, "content-type") != NULL, "Content-type is NULL");

  if (ws->cres->printLoadMessages)
  {
    fprintf (stderr, "Source Init %s\n", url);
  }

  /*
   * Check to see if any sinks are listening
   */
  if ((c = (ChimeraSink)GListGetHead(ws->sinks)) == NULL)
  {
    if (ws->cres->printLoadMessages)
    {
      fprintf (stderr, "No sinks for source!  Bad bug! (%s)\n", url);
    }
    return;
  }

  /*
   * This is broken.  The document may not be cacheable and so new
   * sources should be created for each sink.  This is the first
   * time we know if the document is cacheable.  Fix later.
   */
  for (; c != NULL;
       c = (ChimeraSink)GListGetNext(ws->sinks))
  {
    /*
     * This check has to be done since the sink may have been cancelled
     * or othwise messed with before this.
     */
    if (c->state == SinkStateWaiting)
    {
      ChangeSinkState(c, SourceInitTask, SinkStateWaiting);
    }
  }

  return;
}

/*
 * SourceSendMessage
 */
void
SourceSendMessage(ws, message)
ChimeraSource ws;
char *message;
{
  ChimeraSink c;

  for (c = (ChimeraSink)GListGetHead(ws->sinks); c != NULL;
       c = (ChimeraSink)GListGetNext(ws->sinks))
  {
    if (c->state == SinkStateWaiting || c->state == SinkStateLoading ||
	c->state == SinkStateComplete)
    {
      CMethod(c->dhooks.message)(c->closure, message);
    }
  }

  return;
}

/*
 * SourceToResources
 */
ChimeraResources
SourceToResources(ws)
ChimeraSource ws;
{
  return(ws->cres);
}

/*
 * SourceGetData
 */
void
SourceGetData(ws, data, len, mh)
ChimeraSource ws;
byte **data;
size_t *len;
MIMEHeader *mh;
{
  if (ws->shooks.getdata != NULL)
  {
    CMethod(ws->shooks.getdata)(ws->closure, data, len, mh);
  }
  return;
}

/*
 * GetSourceInfo
 */
static char *
GetSourceInfo(ws, name)
ChimeraSource ws;
char *name;
{
  byte *data;
  size_t len;
  MIMEHeader mh;
  char *value;

  SourceGetData(ws, &data, &len, &mh);
  if (mh == NULL) return(NULL);
  if (MIMEGetField(mh, name, &value) != 0) return(NULL);
  return(value);
}

/*
 * SinkCreate
 *
 * Create a sink for a source.  'wr' is eaten by this function.
 */
ChimeraSink
SinkCreate(cres, wr)
ChimeraResources cres;
ChimeraRequest *wr;
{
  ChimeraSource ws = NULL;
  ChimeraSink wp;
  MemPool mp;
  char *detail;

  if (wr->reload || (ws = TryMemoryCache(cres, wr)) == NULL)
  {
    if ((ws = SourceCreate(cres, wr)) == NULL)
    {
      if (cres->printLoadMessages)
      {
	fprintf (stderr, "Sink Create Failed %s\n", wr->url);
      }
      return(NULL);
    }
    detail = "New";
  }
  else
  {
    /*
     * This can be thrown out since the 'ws' obtained from TryMemoryCache
     * contains an identical copy.
     */
    RequestDestroy(wr);
    detail = "Cache";
  }

  mp = MPCreate();
  wp = (ChimeraSink)MPCGet(mp, sizeof(struct ChimeraSinkP));
  wp->mp = mp;
  wp->state = SinkStateWaiting;
  wp->ws = ws;
  wp->debug = ws->debugcount++;
  memset(&wp->dhooks, 1, sizeof(wp->dhooks));

  GListAddTail(ws->sinks, wp);
  
  if (cres->printLoadMessages)
  {
    fprintf (stderr, "Sink(%d) Create %s %s\n", wp->debug,
	     detail, ws->wr->url);
  }

  return(wp);
}

/*
 * SinkSetHooks
 */
void
SinkSetHooks(wp, hooks, closure)
ChimeraSink wp;
ChimeraSinkHooks *hooks;
void *closure;
{
  /*
   * If there are no hooks then ignore this sink.  If there are hooks then
   * reset it for another go around.
   */
  if (hooks == NULL)
  {
    ChangeSinkState(wp, NULL, SinkStateIgnore);
    wp->closure = NULL;
    memset(&wp->dhooks, 1, sizeof(wp->dhooks)); /* 0 causes trouble */
  }
  else
  {
    myassert(hooks->init != NULL, "Init function is NULL.");
    myassert(hooks->add != NULL, "Add function is NULL.");
    myassert(hooks->end != NULL, "End function is NULL.");
    myassert(hooks->message != NULL, "Message function is NULL.");
    
    memcpy(&wp->dhooks, hooks, sizeof(wp->dhooks));
    wp->closure = closure;

    if (wp->ws->sstate == SourceStateLoading ||
	wp->ws->sstate == SourceStateComplete)
    {
      ChangeSinkState(wp, SourceInitTask, SinkStateWaiting);
    }
    else ChangeSinkState(wp, NULL, SinkStateWaiting);
  }

  return;
}

/*
 * SinkDestroyTask
 */
static void
SinkDestroyTask(closure)
void *closure;
{
  ChimeraSink wp = (ChimeraSink)closure;
  ChimeraSource ws = wp->ws;
  ChimeraSink c;

  myassert(wp->state == SinkStateDestroyed, "Sink has been undestroyed");

  wp->wt = NULL;

  if ((c = (ChimeraSink)GListGetHead(ws->sinks)) == NULL)
  {
    fprintf (stderr, "SinkDestroyTask: source has no sinks!\n");
    return;
  }

  for (; c != NULL;
       c = (ChimeraSink)GListGetNext(ws->sinks))
  {
    if (c->state != SinkStateDestroyed) break;
  }
  if (c == NULL) SourceDestroy(ws);

  return;
}

/*
 * SinkDestroy
 */
void
SinkDestroy(wp)
ChimeraSink wp;
{
  ChimeraSource ws = wp->ws;

  myassert(wp->state != SinkStateDestroyed, "Sink already destroyed");

  if (ws->cres->printLoadMessages)
  {
    fprintf (stderr, "Sink(%d) Destroy %s\n", wp->debug, ws->wr->url);
  }

  ChangeSinkState(wp, SinkDestroyTask, SinkStateDestroyed);

  return;
}

/*
 * SinkWasReloaded
 */
bool
SinkWasReloaded(wp)
ChimeraSink wp;
{
  return(wp->ws->wr->reload);
}

/*
 * SinkToResources
 */
ChimeraResources
SinkToResources(wp)
ChimeraSink wp;
{
  return(wp->ws->cres);
}

/*
 * SinkGetData
 */
void
SinkGetData(wp, data, len, mh)
ChimeraSink wp;
byte **data;
size_t *len;
MIMEHeader *mh;
{
  if (wp->ws->shooks.getdata != NULL)
  {
    CMethod(wp->ws->shooks.getdata)(wp->ws->closure, data, len, mh);
  }
  return;
}

/*
 * SinkGetInfo
 */
char *
SinkGetInfo(wp, name)
ChimeraSink wp;
char *name;
{
  byte *data;
  size_t len;
  MIMEHeader mh;
  char *value;

  SinkGetData(wp, &data, &len, &mh);
  if (mh == NULL) return(NULL);
  if (MIMEGetField(mh, name, &value) != 0) return(NULL);
  return(value);
}

/*
 * SinkCancelTask
 *
 * If all sinks for a source are cancelled then a request is sent to
 * the source to stop sending input.  The callbacks are
 * no longer called for the sink after this function.
 */
static void
SinkCancelTask(closure)
void *closure;
{
  ChimeraSink wp = (ChimeraSink)closure;
  ChimeraSource ws = wp->ws;
  ChimeraSink c;

  wp->wt = NULL;

  myassert(wp->state == SinkStateCancelled, "Sink not cancelled\n");

  /*
   * Check to see if all sinks are cancelled.  If so then stop the source.
   */
  for (c = (ChimeraSink)GListGetHead(ws->sinks); c != NULL;
       c = (ChimeraSink)GListGetNext(ws->sinks))
  {
    if (c->state != SinkStateCancelled && c->state != SinkStateDestroyed)
    {
      break;
    }
  }
  if (c == NULL)
  {
    if (ws->shooks.stop != NULL && ws->closure != NULL)
    {
      CMethod(ws->shooks.stop)(ws->closure);
    }
    SourceStop(ws, "");
  }

  return;
}

/*
 * SinkCancel
 */
void
SinkCancel(wp)
ChimeraSink wp;
{
  myassert(wp->state != SinkStateDestroyed, "Sink can't be cancelled\n");

  if (wp->ws->cres->printLoadMessages)
  {
    fprintf (stderr, "Sink(%d) Cancel %s\n", wp->debug, wp->ws->wr->url);
  }

  ChangeSinkState(wp, SinkCancelTask, SinkStateCancelled);

  return;
}
