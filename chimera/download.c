/*
 * download.c
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>

#include "port_after.h"

#include "MyDialog.h"

#include "ChimeraP.h"

typedef struct
{
  MemPool mp;
  ChimeraSink wp;
  Widget pop, dialog;
  bool okd;
  char *filename;
  bool ended;
  Widget ok;
  ChimeraTask wt;
  ChimeraResources cres;
} DownloadInfo;

static void DoSaveTask _ArgProto((DownloadInfo *));
static void SaveTask _ArgProto((void *));
static void DownloadDestroy _ArgProto((DownloadInfo *));
static void DownloadMakePop _ArgProto((DownloadInfo *));
static int DownloadInit _ArgProto((ChimeraSink, void *));
static void DownloadAdd _ArgProto((void *));
static void DownloadEnd _ArgProto((void *));
static void DownloadMessage _ArgProto((void *, char *));

/*
 * DownloadDestroy
 */
static void
DownloadDestroy(di)
DownloadInfo *di;
{
  ChimeraResources cres = di->cres;

  if (di->pop != NULL) XtDestroyWidget(di->pop);
  if (di->wp != NULL) SinkDestroy(di->wp);
  MPDestroy(di->mp);

  ChimeraRemoveReference(cres);

  return;
}

/*
 * DownloadClear
 */
static void
DownloadClear(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  DownloadInfo *di = (DownloadInfo *)cldata;

  MyDialogSetValue(di->dialog, "");
  XtVaSetValues(di->ok, XtNsensitive, True, NULL);
  di->okd = false;

  return;
}

/*
 * DownloadDSave
 */
static void
DownloadDSave(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  DownloadDestroy((DownloadInfo *)cldata);
  return;
}

/*
 * SaveTask
 */
static void
SaveTask(closure)
void *closure;
{
  char *filename;
  byte *data;
  size_t len;
  MIMEHeader mh;
  FILE *fp;
  DownloadInfo *di = (DownloadInfo *)closure;

  if ((filename = MyDialogGetValue(di->dialog)) == NULL)
  {
    XBell(di->cres->dpy, 100);
    XtVaSetValues(di->ok, XtNsensitive, True, NULL);
    di->okd = false;
    di->wt = NULL;
    return;
  }

  filename = FixPath(di->mp, filename); 
  if ((fp = fopen(filename, "w")) == NULL)
  {
    XBell(di->cres->dpy, 100);
    XtVaSetValues(di->ok, XtNsensitive, True, NULL);
    di->okd = false;
    di->wt = NULL;
    MyDialogSetValue(di->dialog, di->filename);
    return;
  }

  SinkGetData(di->wp, &data, &len, &mh);
  
  fwrite(data, 1, len, fp);
  fclose(fp);
  
  DownloadDestroy(di);

  return;
}

/*
 * DoSaveTask
 */
static void
DoSaveTask(di)
DownloadInfo *di;
{
  myassert(di->wt == NULL, "Save task already started.");

  di->wt = TaskSchedule(di->cres, SaveTask, di);

  return;
}

/*
 * DownloadOSave
 */
static void
DownloadOSave(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  DownloadInfo *di = (DownloadInfo *)cldata;

  if (di->okd) return;

  if (di->ended) DoSaveTask(di);
  else
  {
    di->okd = true;
    XtVaSetValues(di->ok, XtNsensitive, False, NULL);
  }

  return;
}

static void
DownloadMakePop(di)
DownloadInfo *di;
{
  Window rw, cw;
  int rx, ry, wx, wy;
  unsigned int mask;

  XQueryPointer(di->cres->dpy, DefaultRootWindow(di->cres->dpy),
                &rw, &cw,
                &rx, &ry,
                &wx, &wy,
                &mask);
  di->pop = XtVaAppCreateShell("download", "Download",
			       transientShellWidgetClass, di->cres->dpy,
			       XtNx, rx - 2,
			       XtNy, ry - 2,
			       NULL);
  di->dialog = XtVaCreateManagedWidget("dialog",
				       mydialogWidgetClass, di->pop,
				       NULL);
  di->ok = MyDialogAddButton(di->dialog, "ok", DownloadOSave, di);
  MyDialogAddButton(di->dialog, "Clear", DownloadClear, di);
  MyDialogAddButton(di->dialog, "dismiss", DownloadDSave, di);
  XtAddCallback(di->dialog, XtNcallback, DownloadOSave, di);

  XtRealizeWidget(di->pop);

  MyDialogSetValue(di->dialog, di->filename);

  return;
}

/*
 * DownloadAdd
 */
void
DownloadAdd(closure)
void *closure;
{
  return;
}

/*
 * DownloadEnd
 */
void
DownloadEnd(closure)
void *closure;
{
  DownloadInfo *di = (DownloadInfo *)closure;

  di->ended = true;
  if (di->okd) DoSaveTask(di);

  return;
}

/*
 * DownloadMessage
 */
void
DownloadMessage(closure, message)
void *closure;
char *message;
{
  DownloadInfo *di = (DownloadInfo *)closure;

  if (di->dialog != NULL) MyDialogSetMessage(di->dialog, message);

  return;
}

/*
 * DownloadInit
 */
static int
DownloadInit(wp, closure)
ChimeraSink wp;
void *closure;
{
  return(0);
}

void
DownloadOpen(cres, wr)
ChimeraResources cres;
ChimeraRequest *wr;
{
  DownloadInfo *di;
  MemPool mp;
  ChimeraSinkHooks hooks;
  char *filename;

  myassert(wr != NULL, "NULL request not allowed.");

  mp = MPCreate();
  di = (DownloadInfo *)MPCGet(mp, sizeof(DownloadInfo));
  di->mp = mp;
  if ((filename = GetBaseFilename(wr->url)) == NULL) filename = "";
  di->filename = MPStrDup(di->mp, filename);
  di->cres = cres;

  memset(&hooks, 0, sizeof(hooks));
  hooks.init = DownloadInit;
  hooks.add = DownloadAdd;
  hooks.end = DownloadEnd;
  hooks.message = DownloadMessage;

  ChimeraAddReference(cres);

  if ((di->wp = SinkCreate(cres, wr)) != NULL)
  {
    SinkSetHooks(di->wp, &hooks, di);
    DownloadMakePop(di);
  }
  else
  {
    DownloadDestroy(di);
    RequestDestroy(wr);
  }

  return;
}
