/*
 * auth.c
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

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>

#include "port_after.h"

#include "AuthDialog.h"

#include "ChimeraP.h"

struct ChimeraAuthP
{
  MemPool         mp;
  ChimeraAuthCallback callback;
  void            *closure;
  ChimeraResources cres;
  Widget          w;
};

static void AuthOKCallback _ArgProto((Widget, XtPointer, XtPointer));
static void AuthActivateCallback _ArgProto((Widget, XtPointer, XtPointer));
static void AuthDismissCallback _ArgProto((Widget, XtPointer, XtPointer));

/*
 * AuthOKCallback
 */
static void
AuthOKCallback(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraAuth wa = (ChimeraAuth)cldata;
  char *username;
  char *password;

  username = AuthDialogGetUsername(XtParent(w));
  password = AuthDialogGetPassword(XtParent(w));

  CMethod(wa->callback)(wa->closure, username, password);
  /* don't access wa after this since AuthDestroy may have been called */

  return;
}

/*
 * AuthActivateCallback
 */
static void
AuthActivateCallback(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraAuth wa = (ChimeraAuth)cldata;
  char *username;
  char *password;

  username = AuthDialogGetUsername(w);
  password = AuthDialogGetPassword(w);

  CMethod(wa->callback)(wa->closure, username, password);
  /* don't access wa after this since AuthDestroy may have been called */

  return;
}

/*
 * AuthDismissCallback
 */
static void
AuthDismissCallback(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraAuth wa = (ChimeraAuth)cldata;

  CMethod(wa->callback)(wa->closure, NULL, NULL);
  /* don't access wa after this since AuthDestroy may have been called */

  return;
}

/*
 * AuthCreate
 */
ChimeraAuth
AuthCreate(cres, message, username, callback, closure)
ChimeraResources cres;
char *message;
char *username;
ChimeraAuthCallback callback;
void *closure;
{
  ChimeraAuth wa;
  MemPool mp;
  Widget dw;
  Window rw, cw;
  int rx, ry, wx, wy;
  unsigned int mask;

  mp = MPCreate();
  wa = (ChimeraAuth)MPCGet(mp, sizeof(struct ChimeraAuthP));
  wa->mp = mp;
  wa->callback = callback;
  wa->closure = closure;
  wa->cres = cres;
  
  XQueryPointer(cres->dpy, DefaultRootWindow(cres->dpy),
                &rw, &cw,
		&rx, &ry,
		&wx, &wy,
		&mask);
  wa->w = XtVaAppCreateShell("authorization", "Authorization",
			     topLevelShellWidgetClass,
			     cres->dpy,
			     XtNx, rx - 2,
			     XtNy, ry - 2,
			     NULL);
  dw = XtVaCreateManagedWidget("dialog",
			       authdialogWidgetClass, wa->w,
			       XtNlabel, message,
			       "username", username,
			       NULL);
  AuthDialogAddButton(dw, "ok", AuthOKCallback, wa);
  AuthDialogAddButton(dw, "dismiss", AuthDismissCallback, wa);
  XtAddCallback(dw, XtNcallback, AuthActivateCallback, wa);
  
  XtRealizeWidget(wa->w);

  return(wa);
}

/*
 * AuthDestroy
 */
void
AuthDestroy(wa)
ChimeraAuth wa;
{
  XtDestroyWidget(wa->w);
  MPDestroy(wa->mp);
  return;
}
