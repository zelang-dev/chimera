/*
 * ChimeraSource.h
 *
 * Copyright (c) 1995-1997,1999, John Kilburg <john@cs.unlv.edu>
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
#ifndef __CHIMERASOURCE_H__
#define __CHIMERASOURCE_H__ 1

#include "common.h"
#include "mime.h"
#include "url.h"

typedef struct ChimeraSourceP *ChimeraSource;
typedef struct ChimeraSinkP *ChimeraSink;
typedef struct ChimeraRequestP ChimeraRequest;

typedef struct
{
  void *closure;
  int (*init) _ArgProto((ChimeraSink, void *));  /* returns 0 for success */
  void (*add) _ArgProto((void *));
  void (*end) _ArgProto((void *));
  void (*message) _ArgProto((void *, char *));
} ChimeraSinkHooks;

typedef struct
{
  char *name;
  void *class_closure;
  void *(*init) _ArgProto((ChimeraSource, ChimeraRequest *, void *));
  void (*stop) _ArgProto((void *));
  void (*destroy) _ArgProto((void *));
  void (*class_destroy) _ArgProto((void *));
  void (*getdata) _ArgProto((void *, byte **, size_t *, MIMEHeader *));
  char *(*resolve_url) _ArgProto((MemPool, char *, char *));
} ChimeraSourceHooks;

struct ChimeraRequestP
{
  MemPool mp;                /* */
  char *url;                 /* Absolute URL not-parsed */
  URLParts *up;              /* Absolute URL parsed */
  URLParts *pup;             /* Absolute proxy URL parsed */
  void *input_data;          /* input data */
  size_t input_len;          /* input data len */
  char *input_type;          /* input data MIME content-type */
  char *input_method;        /* input method GET/POST */
  bool reload;               /* don't use cache? */
  GList contents;            /* acceptable contents, NULL = '*' */
  ChimeraSourceHooks hooks;  /* */

  char *scheme;              /* convienence */
  char *parent_url;          /* parent/base url saved from RequestCreate */
};

/*
 * Source
 *
 *  Functions called by the source implementation.
 */
void SourceInit _ArgProto((ChimeraSource, bool));
void SourceAdd _ArgProto((ChimeraSource));
void SourceEnd _ArgProto((ChimeraSource));
void SourceStop _ArgProto((ChimeraSource, char *));
void SourceSendMessage _ArgProto((ChimeraSource, char *));
ChimeraResources SourceToResources _ArgProto((ChimeraSource));

int SourceAddHooks _ArgProto((ChimeraResources cres,
			      ChimeraSourceHooks *shooks));

/*
 * Sink
 *
 * Functions called by the sink creator.
 */
ChimeraSink SinkCreate _ArgProto((ChimeraResources, ChimeraRequest *));
void SinkSetHooks _ArgProto((ChimeraSink, ChimeraSinkHooks *, void *));
void SinkDestroy _ArgProto((ChimeraSink));
void SinkCancel _ArgProto((ChimeraSink));
void SinkGetData _ArgProto((ChimeraSink, byte **, size_t *, MIMEHeader *));
char *SinkGetInfo _ArgProto((ChimeraSink, char *));
ChimeraResources SinkToResources _ArgProto((ChimeraSink));
bool SinkWasReloaded _ArgProto((ChimeraSink));

/*
 * request.c
 */
ChimeraRequest *RequestCreate _ArgProto((ChimeraResources, char *, char *));
int RequestAddRegexContent _ArgProto((ChimeraResources,
				      ChimeraRequest *, char *));
void RequestAddContent _ArgProto((ChimeraRequest *, char *));
void RequestDestroy _ArgProto((ChimeraRequest *));
void RequestReload _ArgProto((ChimeraRequest *, bool));
bool RequestCompareURL _ArgProto((ChimeraRequest *, ChimeraRequest *));
bool RequestCompareAccept _ArgProto((ChimeraRequest *, ChimeraRequest *));
bool RequestMatchContent _ArgProto((MemPool, char *, char *));
bool RequestMatchContent2 _ArgProto((ChimeraRequest *, char *));

char *ChimeraExt2Content _ArgProto((ChimeraResources, char *));

#endif
