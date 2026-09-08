/*
 * view.c
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

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Paned.h>

#include "port_after.h"

#include "TextField.h"

#include "ChimeraP.h"

typedef struct 
{
  MemPool mp;
  ChimeraSink wp;
  char *command;
  ChimeraTask wt;
  ChimeraResources cres;
  Widget pop, message;
} ViewInfo;

static char *ViewFind _ArgProto((MemPool, char *, char *));
static void ViewDestroy _ArgProto((ViewInfo *));
static int ViewSinkInit _ArgProto((ChimeraSink, void *));
static void ViewSinkAdd _ArgProto((void *));
static void ViewSinkEnd _ArgProto((void *));
static void ViewSinkMessage _ArgProto((void *, char *));
static void ViewMakePop _ArgProto((ViewInfo *));
static void ViewDialogStop _ArgProto((Widget, XtPointer, XtPointer));

/*
 * ViewFind
 *
 * Return a command string for a type.
 */
static char *
ViewFind(mp, filelist, content)
MemPool mp;
char *filelist;
char *content;
{
  FILE *fp;
  char *f;
  char buffer[BUFSIZ];
  char ctype[BUFSIZ];
  char command[BUFSIZ];
  char junk[BUFSIZ];
  char *filename;
  MemPool tmp;

  tmp = MPCreate();

  f = filelist;
  while ((filename = mystrtok(f, ':', &f)) != NULL)
  {
    filename = FixPath(mp, filename);
    if (filename == NULL) continue;
   
    fp = fopen(filename, "r");
    if (fp == NULL) continue;
    
    while (fgets(buffer, sizeof(buffer), fp))
    {
      if (buffer[0] == '#' || buffer[0] == '\n') continue;
      
      if (sscanf(buffer, "%[^;]%[;]%[^;\n]", ctype, junk, command) == 3)
      {
	if (RequestMatchContent(tmp, ctype, content))
        {
	  fclose(fp);
          MPDestroy(tmp);
	  return(MPStrDup(mp, command));
	}
      }
    }

    fclose(fp);
  }

  MPDestroy(tmp);

  return(NULL);
}

static void
ViewSinkEnd(closure)
void *closure;
{
  ViewInfo *vi = (ViewInfo *)closure;
  char *filename;
  FILE *fp;
  char *final;
  char *path;
  byte *data;
  size_t len;
  MIMEHeader mh;
  size_t finallen;

  SinkGetData(vi->wp, &data, &len, &mh);
  if (data == NULL || len == 0)
  {
    ViewDestroy(vi);
    return;
  }

  filename = mytmpnam(vi->mp);
  if ((fp = fopen(filename, "w")) != NULL)
  {
    fwrite(data, len, 1, fp);
    fclose(fp);

    if ((path = ResourceGetString(vi->cres, "view.path")) == NULL)
    {
      if ((path = getenv("PATH")) == NULL) path = "";
    }
    
    finallen = strlen(" ; rm &") + strlen(path) + strlen(filename) * 2 +
	strlen(vi->command) + strlen("PATH=") + 1;
    final = (char *)MPGet(vi->mp, finallen);
    strcpy(final, "(PATH=");
    strcat(final, path);
    strcat(final, " ");
    sprintf (final + strlen(final), vi->command, filename);
    strcat(final, "; rm ");
    strcat(final, filename);
    strcat(final, ") &");
    system(final);
  }

  ViewDestroy(vi);

  return;
}

/*
 * ViewSinkMessage
 */
void
ViewSinkMessage(closure, message)
void *closure;
char *message;
{
  ViewInfo *vi = (ViewInfo *)closure;
  TextFieldSetString(vi->message, message);
  return;
}

/*
 * ViewDestroy
 */
static void
ViewDestroy(vi)
ViewInfo *vi;
{
  ChimeraResources cres = vi->cres;

  if (vi->pop != NULL) XtDestroyWidget(vi->pop);

  SinkDestroy(vi->wp);
  MPDestroy(vi->mp);

  ChimeraRemoveReference(cres);

  return;
}

/*
 * ViewSinkAdd
 */
void
ViewSinkAdd(closure)
void *closure;
{
  return;
}

/*
 * ViewSinkInit
 */
static int
ViewSinkInit(wp, closure)
ChimeraSink wp;
void *closure;
{
  char *url;
  char *caplist;
  ViewInfo *vi = (ViewInfo *)closure;
  ChimeraRequest *wr;

  if ((caplist = ResourceGetString(vi->cres, "view.capFiles")) == NULL)
  {
    caplist = "~/.mailcap";
  }

  if ((vi->command = ViewFind(vi->mp, caplist,
			      SinkGetInfo(wp, "content-type"))) == NULL)
  {
    url = SinkGetInfo(wp, "x-url");

    if ((wr = RequestCreate(vi->cres, url, NULL)) == NULL) return(-1);

    DownloadOpen(vi->cres, wr);

    ViewDestroy(vi);

    return(-1);
  }

  return(0);
}

static void
ViewDialogStop(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ViewDestroy((ViewInfo *)cldata);
  return;
}

static void
ViewMakePop(vi)
ViewInfo *vi;
{
  Window rw, cw;
  int rx, ry, wx, wy;
  unsigned int mask;
  Widget paned;
  Widget box;
  Widget stop;

  XQueryPointer(vi->cres->dpy, DefaultRootWindow(vi->cres->dpy),
                &rw, &cw,
                &rx, &ry,
                &wx, &wy,
                &mask);
  vi->pop = XtVaAppCreateShell("view", "View",
			       transientShellWidgetClass, vi->cres->dpy,
			       XtNx, rx - 2,
			       XtNy, ry - 2,
			       NULL);

  paned = XtCreateManagedWidget("paned",
                                panedWidgetClass, vi->pop, 
                                NULL, 0);

  box = XtCreateManagedWidget("box", boxWidgetClass, paned, NULL, 0);

  vi->message = XtVaCreateManagedWidget("message",
					textfieldWidgetClass, box,
					NULL);

  box = XtCreateManagedWidget("box2", boxWidgetClass, paned, NULL, 0);

  stop = XtVaCreateManagedWidget("stop", commandWidgetClass, box, NULL, 0);

  XtAddCallback(stop, XtNcallback, ViewDialogStop, vi);

  XtRealizeWidget(vi->pop);

  return;
}

/*
 * ViewOpen
 */
void
ViewOpen(cres, wr)
ChimeraResources cres;
ChimeraRequest *wr;
{
  ViewInfo *vi;
  ChimeraSinkHooks hooks;
  MemPool mp;

  myassert(wr != NULL, "Request cannot be NULL.");

  mp = MPCreate();
  vi = (ViewInfo *)MPCGet(mp, sizeof(ViewInfo));
  vi->mp = mp;
  vi->cres = cres;

  memset(&hooks, 0, sizeof(hooks));
  hooks.init = ViewSinkInit;
  hooks.add = ViewSinkAdd;
  hooks.end = ViewSinkEnd;
  hooks.message = ViewSinkMessage;

  if ((vi->wp = SinkCreate(cres, wr)) == NULL) MPDestroy(mp);
  else
  {
    SinkSetHooks(vi->wp, &hooks, vi);
    ViewMakePop(vi);
    ChimeraAddReference(vi->cres);
  }

  return;
}
