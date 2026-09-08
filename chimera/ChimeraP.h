/*
 * ChimeraP.h
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
#ifndef __CHIMERAP_H__
#define __CHIMERAP_H__

#include "common.h"

#include "Chimera.h"
#include "ChimeraSource.h"
#include "ChimeraGUI.h"
#include "ChimeraRender.h"
#include "ChimeraAuth.h"
#include "ChimeraStack.h"

typedef struct ChimeraCacheP *ChimeraCache;
typedef struct BookmarkContextP *BookmarkContext;
typedef struct ChimeraSchedulerP *ChimeraScheduler;

struct ChimeraContextP
{
  MemPool mp;

  Widget toplevel;
  Widget file;
  Widget www;
  Widget open;
  Widget back;
  Widget reload;
  Widget cancel;
  Widget quit;
  Widget home;
  Widget addmark;
  Widget viewmark;
  Widget message;
  Widget dup;
  Widget source;
  Widget save;
  Widget openpop;
  Widget savepop;
  Widget findpop;
  Widget bookpop;
  Widget url;
  Widget help;
  Widget bookmark;
  Widget find;

  ChimeraStack       tstack;  /* top stack */
  ChimeraStack       cstack;  /* current stack */
  MemPool            tmp;

  ChimeraResources cres;

  char *button1Box;           /* list of widgets in the first button box */
  char *button2Box;           /* list of widgets in the second button box */
};

/*
 * Chimera runtime context
 */
struct ChimeraResourcesP
{
  int              maxiocnt;          /* maximum IOs */
  bool             printLoadMessages;
  bool             printTaskInfo;
  MemPool          mp;                /* memory pool */
  XrmDatabase      db;                /* message database */

  ChimeraCache     cc;

  /* Lists of stuff we need to know about */
  GList            sourcehooks;       /* list of registered sources */
  GList            renderhooks;       /* list of registered renderers */
  GList            mimes;             /* list of mimes */
  GList            renderers;         /* list of active renderers */
  GList            sources;           /* list of active sources */
  GList            timeouts;          /* list of timeouts */
  GList            oldtimeouts;       /* list of old timeouts */
  GList            heads;             /* list of heads */
  GList            stacks;            /* list of stacks */

  /* */
  void             (*authui_callback)();
  void             *authui_closure;

  XtAppContext     appcon;            /* X application context */
  size_t           id;                /* next unique ID */

  Display          *dpy;

  ChimeraScheduler cs;                /* task scheduler context */
  ChimeraContext   bmcontext;         /* head associated with bookmarks */
  ChimeraRenderHooks  *plainhooks;    /* plain text renderer */
  BookmarkContext  bc;                /* bookmark routines context */
  FILE             *logfp;            /* log file */
  int              refcount;          /* reference count */
};

/*
 *
 * Prototypes
 *
 */

/*
 * cache.c
 */
ChimeraCache CacheCreate _ArgProto((ChimeraResources));
void CacheDestroy _ArgProto((ChimeraCache));
bool CacheIsDiskCached _ArgProto((ChimeraCache, char *));
int CacheWrite _ArgProto((ChimeraCache, ChimeraSource));
ChimeraSourceHooks *CacheGetHooks _ArgProto((ChimeraCache));

/*
 * data.c
 */
void SourceGetData _ArgProto((ChimeraSource,
			      byte **, size_t *, MIMEHeader *));

/*
 * gui.c
 */
ChimeraGUI GUICreateToplevel _ArgProto((ChimeraContext, Widget,
					GUISizeCallback, void *));
void GUIReset _ArgProto((ChimeraGUI));
void GUIAddRender _ArgProto((ChimeraGUI, ChimeraRender));

/*
 * bookmark.c
 */
BookmarkContext BookmarkCreateContext _ArgProto((ChimeraResources));
void BookmarkDestroyContext _ArgProto((BookmarkContext));
void BookmarkAdd _ArgProto((BookmarkContext, char *, char *));
void BookmarkShow _ArgProto((BookmarkContext));

/*
 * head.c
 */
void HeadCreate _ArgProto((ChimeraResources, ChimeraRequest *,
			   ChimeraRequest *));
void HeadDestroy _ArgProto((ChimeraContext));
void HeadPrintMessage _ArgProto((ChimeraContext, char *));
void HeadPrintURL _ArgProto((ChimeraContext, char *));

Widget CreateDialog _ArgProto((Widget, char *,
			       void (*)(), void (*)(), void (*)(), XtPointer));
Widget GetDialogWidget _ArgProto((Widget));

/*
 * main.c
 */
void ChimeraAddReference _ArgProto((ChimeraResources));
void ChimeraRemoveReference _ArgProto((ChimeraResources));

/*
 * callback.c
 */
bool MessageCallback _ArgProto((void *, char *));
bool ActionCallback _ArgProto((void *, ChimeraRequest *, char *));
bool RedrawCallback _ArgProto((void *));
bool ResizeCallback _ArgProto((void *));

/*
 * builtin.c
 */
void InitChimeraBuiltins _ArgProto((ChimeraResources));

/*
 * view.c
 */
void ViewOpen _ArgProto((ChimeraResources, ChimeraRequest *));

/*
 * download.c
 */
void DownloadOpen _ArgProto((ChimeraResources, ChimeraRequest *));

/*
 * stack.c
 */
ChimeraStack StackCreateToplevel _ArgProto((ChimeraContext, Widget));
ChimeraGUI StackToGUI _ArgProto((ChimeraStack));
ChimeraRender StackToRender _ArgProto((ChimeraStack));
void StackSetRender _ArgProto((ChimeraStack, ChimeraRenderHooks *));

/*
 * resource.c
 */
char *ResourceGetStringP _ArgProto((ChimeraResources, char *));

/*
 * cmisc.c
 */
ChimeraSourceHooks *SourceGetHooks _ArgProto((ChimeraResources, char *));

/*
 * task.c
 */
ChimeraScheduler SchedulerCreate _ArgProto((void));

#endif
