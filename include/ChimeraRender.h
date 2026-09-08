/*
 * ChimeraRender.h
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
#ifndef __CHIMERARENDER_H__
#define __CHIMERARENDER_H__ 1

#include "common.h"
#include "url.h"

#include "ChimeraGUI.h"
#include "ChimeraSource.h"

/*
 * A whole bunch of types with which to cause trouble and confuse.
 */
typedef struct ChimeraRenderP *ChimeraRender;

typedef struct
{
  char *content;
  void *class_context;
  void *(*init) _ArgProto((ChimeraRender, void *, void *));
  void (*add) _ArgProto((void *));
  void (*end) _ArgProto((void *));
  void (*destroy) _ArgProto((void *));
  void (*cancel) _ArgProto((void *));
  void *(*getstate) _ArgProto((void *));
  void (*class_destroy) _ArgProto((void *));
  byte *(*query) _ArgProto((void *, char *));
  int (*search) _ArgProto((void *, char *, int));
  bool (*select) _ArgProto((void *, int, int, char *));
  bool (*motion) _ArgProto((void *, int, int));
  bool (*expose) _ArgProto((void *, int, int, unsigned int, unsigned int));
} ChimeraRenderHooks;

/*
 * render.c
 */

typedef void (*ChimeraRenderActionProc) _ArgProto((void *, ChimeraRender,
						   ChimeraRequest *,
						   char *));

/* Called by the render user/caller */
ChimeraRender RenderCreate _ArgProto((ChimeraContext,
				      ChimeraGUI, ChimeraSink,
				      ChimeraRenderHooks *,
				      ChimeraRenderHooks *,
				      void *, void *,
				      ChimeraRenderActionProc,
				      void *));
void RenderAdd _ArgProto((ChimeraRender));
void RenderEnd _ArgProto((ChimeraRender));
void RenderDestroy _ArgProto((ChimeraRender));
void RenderCancel _ArgProto((ChimeraRender));
char *RenderQuery _ArgProto((ChimeraRender, char *));

/*
 * Returns -1 for not found
 * Returns -2 for end of document
 * Returns 0 for found
 *
 * third argument 0 for continue search
 * third argument 1 for start search from beginning
 */
int RenderSearch _ArgProto((ChimeraRender, char *, int));
void RenderSelect _ArgProto((ChimeraRender, int, int, char *));
void RenderMotion _ArgProto((ChimeraRender, int, int));
void RenderExpose _ArgProto((ChimeraRender, int, int,
			     unsigned int, unsigned int));

void *RenderGetState _ArgProto((ChimeraRender));

ChimeraGUI RenderToGUI _ArgProto((ChimeraRender));
ChimeraSink RenderToSink _ArgProto((ChimeraRender));
ChimeraContext RenderToContext _ArgProto((ChimeraRender));

void RenderSendMessage _ArgProto((ChimeraRender, char *));

void RenderAction _ArgProto((ChimeraRender, ChimeraRequest *, char *));

int RenderAddHooks _ArgProto((ChimeraResources, ChimeraRenderHooks *));
ChimeraRenderHooks *RenderGetHooks _ArgProto((ChimeraResources, char *));

ChimeraResources RenderToResources _ArgProto((ChimeraRender));

#endif
