/*
 * head.c
 *
 * Copyright (C) 1993-1997, John D. Kilburg <john@cs.unlv.edu>
 *
 * The button table code written by Jim.Rees@umich.edu.
 * The messed up parts were written by john.
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>

#include <X11/Xaw/Form.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Label.h>

#include "port_after.h"

#include "TextField.h"

#include "common.h"

#include "MyDialog.h"

#include "ChimeraP.h"

static void Quit _ArgProto((Widget, XtPointer, XtPointer));
static void Back _ArgProto((Widget, XtPointer, XtPointer));
static void Reload _ArgProto((Widget, XtPointer, XtPointer));
static void Cancel _ArgProto((Widget, XtPointer, XtPointer));
static void Home _ArgProto((Widget, XtPointer, XtPointer));
static void Dup _ArgProto((Widget, XtPointer, XtPointer));
static void Help _ArgProto((Widget, XtPointer, XtPointer));
static void AddMark _ArgProto((Widget, XtPointer, XtPointer));
static void ViewMark _ArgProto((Widget, XtPointer, XtPointer));
static void Bookmark _ArgProto((Widget, XtPointer, XtPointer));
static void Source _ArgProto((Widget, XtPointer, XtPointer));
static void Save _ArgProto((Widget, XtPointer, XtPointer));

static void OOpen _ArgProto((Widget, XtPointer, XtPointer));
static void DOpen _ArgProto((Widget, XtPointer, XtPointer));
static void Open _ArgProto((Widget, XtPointer, XtPointer));

static void OFind _ArgProto((Widget, XtPointer, XtPointer));
static void DFind _ArgProto((Widget, XtPointer, XtPointer));
static void Find _ArgProto((Widget, XtPointer, XtPointer));

static void CreateWidgets _ArgProto((ChimeraContext, char *));

static void AddButtons _ArgProto((ChimeraContext, Widget, char *));

static void InstallAccelerators _ArgProto((Widget));

static struct ButtonTable
{
  char *name;
  Boolean toggle;
  Widget w;
  void (*cb)();
  char *accel;
} ButtonTable[] =
{
  { "quit",     False, NULL, Quit,     ":Meta<KeyUp>q"},
  { "open",	False, NULL, Open,     ":Meta<KeyUp>o" },
  { "home",	False, NULL, Home,     ":Meta<KeyUp>h" },
  { "back",	False, NULL, Back,     ":Meta<KeyUp>b" },
  { "reload",   False, NULL, Reload,   ":Meta<KeyUp>r" },
  { "cancel",   False, NULL, Cancel,   ":Meta<KeyUp>c"},
  { "dup",	False, NULL, Dup,      ":Meta<KeyUp>c" },
  { "help",	False, NULL, Help,     ":Meta<KeyUp>H" },
  { "addmark",	False, NULL, AddMark,  ":Meta<KeyUp>a" },
  { "viewmark", False, NULL, ViewMark, ":Meta<KeyUp>v" },
  { "bookmark", False, NULL, Bookmark, ":Meta<KeyUp>m" },
  { "find",     False, NULL, Find,     ":Meta<KeyUp>f" },
  { "source",   True,  NULL, Source,   ":Meta<KeyUp>s" },
  { "save",     False, NULL, Save,     ":Meta<KeyUp>S" },
  { NULL,       False, NULL, NULL,     NULL},
};

static void
AddButtons(wc, box, list)
ChimeraContext wc;
Widget box;
char *list;
{
  char name[256];
  struct ButtonTable *btp;
  char accel[256];

  while (sscanf(list, " %[^,]", name) == 1)
  {
    /*
     * Find the listed button and create its widget
     */
    for (btp = &ButtonTable[0]; btp->name != NULL; btp++)
    {
      if (!strcasecmp(btp->name, name))
      {
        if (btp->toggle)
        {
          snprintf(accel, sizeof(accel) - 1,
		   "%s: toggle() notify()", btp->accel);
	  btp->w = XtVaCreateManagedWidget(btp->name,
					   toggleWidgetClass, box,
                                           XtNaccelerators,
					   XtParseAcceleratorTable(accel),
					   NULL);
        }
        else
        {
          snprintf(accel, sizeof(accel) - 1, 
		   "%s: set() notify() unset()", btp->accel);
	  btp->w = XtVaCreateManagedWidget(btp->name,
					   commandWidgetClass, box,
                                           XtNaccelerators,
					   XtParseAcceleratorTable(accel),
					   NULL);
        }
	XtAddCallback(btp->w, XtNcallback, btp->cb, (XtPointer)wc);
	break;
      }
    }

    if (!strcasecmp(name, "open")) wc->open = btp->w;
    else if (!strcasecmp(name, "back")) wc->back = btp->w;
    else if (!strcasecmp(name, "reload")) wc->reload = btp->w;
    else if (!strcasecmp(name, "cancel")) wc->cancel = btp->w;
    else if (!strcasecmp(name, "quit")) wc->quit = btp->w;
    else if (!strcasecmp(name, "home")) wc->home = btp->w;
    else if (!strcasecmp(name, "help")) wc->help = btp->w;
    else if (!strcasecmp(name, "dup")) wc->dup = btp->w;
    else if (!strcasecmp(name, "addmark")) wc->addmark = btp->w;
    else if (!strcasecmp(name, "viewmark")) wc->viewmark = btp->w;
    else if (!strcasecmp(name, "source")) wc->source = btp->w;
    else if (!strcasecmp(name, "save")) wc->save = btp->w;
    else if (!strcasecmp(name, "find")) wc->find = btp->w;
    else if (!strcasecmp(name, "bookmark")) wc->bookmark = btp->w;

    /*
     * Skip to the next comma-delimited item in the list
     */
    while (*list && *list != ',')
      list++;
    if (*list == ',')
      list++;
  }

  return;
}

/*
 * InstallAccelerators
 */
static void
InstallAccelerators(w)
Widget w;
{
 struct ButtonTable *btp;

 for (btp = &ButtonTable[0]; btp->name != NULL; btp++)
    if(btp->w)
       XtInstallAllAccelerators(w,btp->w);
}

#define BUTTON_LIST "quit, open, home, back, reload, source, save, bookmark, dup, find, cancel"

/*
 * These are initialized in fallback.c
 */
#define offset(field) XtOffset(ChimeraContext, field)
static XtResource       resource_list[] =
{
  { "button1Box", "BoxList", XtRString, sizeof(char *),
        offset(button1Box), XtRString, BUTTON_LIST },
  { "button2Box", "BoxList", XtRString, sizeof(char *),
        offset(button2Box), XtRString, NULL },
};

/*
 * HeadCreate
 *
 * Setup chimera in a widget.
 */
static void
CreateWidgets(wc, name)
ChimeraContext wc;
char *name;
{
  Widget paned, box, form;
  Atom delete;

  wc->toplevel = XtVaAppCreateShell(name, "Chimera",
				    topLevelShellWidgetClass,
				    wc->cres->dpy,
				    NULL);

  XtGetApplicationResources(wc->toplevel, wc,
                            resource_list, XtNumber(resource_list),
                            NULL, 0);

  /*
   * Main window pane
   */
  paned = XtCreateManagedWidget("paned",
                                panedWidgetClass, wc->toplevel, 
                                NULL, 0);

  /*
   * Button pane(s)
   */
  if (wc->button1Box && *wc->button1Box)
  {
    box = XtCreateManagedWidget("box1", boxWidgetClass, paned, NULL, 0);
    AddButtons(wc, box, wc->button1Box);
  }

  if (wc->button2Box && *wc->button2Box)
  {
    box = XtCreateManagedWidget("box2", boxWidgetClass, paned, NULL, 0);
    AddButtons(wc, box, wc->button2Box);
  }

  /*
   * URL pane
   */
  form = XtVaCreateManagedWidget("urlform", formWidgetClass, paned, NULL);
  XtVaCreateManagedWidget("urllabel", labelWidgetClass, form, NULL);
  wc->url = XtVaCreateManagedWidget("url",
				    textfieldWidgetClass, form,
				    XtNstring, "",
				    NULL);
				   
  XtOverrideTranslations(wc->url,
                         XtParseTranslationTable
                         ("<Key>Return: ReturnAction()"));

  /*
   * Message pane.
   */
  box = XtCreateManagedWidget("box4", boxWidgetClass, paned, NULL, 0);
  wc->message = XtVaCreateManagedWidget("message",
					textfieldWidgetClass, box,
					NULL);

  /*
   * WWW widget
   */
  wc->tstack = StackCreateToplevel(wc, paned);

  XtRealizeWidget(wc->toplevel);

  delete = XInternAtom(XtDisplay(wc->toplevel), "WM_DELETE_WINDOW", False);
  XSetWMProtocols (wc->cres->dpy, XtWindow(wc->toplevel), &delete, 1);
  XtOverrideTranslations (wc->toplevel,
                          XtParseTranslationTable
                          ("<Message>WM_PROTOCOLS: DeleteAction()"));

  /*
   * Accelerators
   */
  InstallAccelerators(paned);

  return;
}

/*
 * HeadDestroy
 */
void
HeadDestroy(wc)
ChimeraContext wc;
{
  StackDestroy(wc->tstack);
  GListRemoveItem(wc->cres->heads, wc);
  XtDestroyWidget(wc->toplevel);
  MPDestroy(wc->mp);

  ChimeraRemoveReference(wc->cres);

  return;
}

/*
 * ClearDialogValue
 */
static void
ClearDialogValue(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  if ((w = XtParent(w)) != NULL) MyDialogSetValue(w, "");

  return;
}

/*
 * CreateDialog
 */
Widget
CreateDialog(p, name, ofunc, dfunc, rfunc, closure)
Widget p;
char *name;
void (*ofunc)();
void (*dfunc)();
void (*rfunc)();
XtPointer closure;
{
  Widget w, dw;
  Window rw, cw;
  int rx, ry, wx, wy;
  unsigned int mask;

  XQueryPointer(XtDisplay(p), DefaultRootWindow(XtDisplay(p)),
                &rw, &cw,
		&rx, &ry,
		&wx, &wy,
		&mask);
  w = XtVaCreatePopupShell(name,
			   transientShellWidgetClass, p,
			   XtNx, rx - 2,
			   XtNy, ry - 2,
			   NULL);
  dw = XtVaCreateManagedWidget("dialog",
			       mydialogWidgetClass, w,
			       XtNvalue, "",
			       NULL);
  MyDialogAddButton(dw, "ok", ofunc, closure);
  MyDialogAddButton(dw, "clear", ClearDialogValue, closure);
  MyDialogAddButton(dw, "dismiss", dfunc, closure);
  XtAddCallback(dw, XtNcallback, rfunc, closure);

  XtRealizeWidget(w);

  return(w);
}

/*
 * GetDialogWidget
 */
Widget
GetDialogWidget(w)
Widget w;
{
  return(XtNameToWidget(w, "dialog"));
}

/*
 * OOpen
 */
static void
OOpen(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;
  char *url;

  if ((url = MyDialogGetValue(GetDialogWidget(wc->openpop))) == NULL) return;

  StackOpen(wc->tstack, RequestCreate(wc->cres, url, NULL));
  XtPopdown(wc->openpop); 

  return;
}

/*
 * DOpen
 */
static void
DOpen(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;
  XtPopdown(wc->openpop); 
  return;
}

/*
 * Open
 */
static void
Open(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;

  if (wc->openpop == NULL)
  {
    wc->openpop = CreateDialog(wc->toplevel, "openpop",
			       OOpen, DOpen, OOpen, (XtPointer)wc);
  }
  XtPopup(wc->openpop, XtGrabNone);

  return;
}

/*
 * Back
 */
static void
Back(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;
  StackBack(wc->tstack);
  return;
}

/*
 * Reload
 */
static void
Reload(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;
  StackReload(wc->tstack);
  return;
}

/*
 * Cancel
 */
static void
Cancel(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;
  StackCancel(wc->tstack);
  return;
}

/*
 * Quit
 */
static void
Quit(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  HeadDestroy((ChimeraContext)cldata);
  return;
}

/*
 * Source
 */
static void
Source(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;

  if (wc->cres->plainhooks == NULL) XtVaSetValues(w, XtNstate, False, NULL);
  else
  {
    if (cbdata) StackSetRender(wc->tstack, wc->cres->plainhooks);
    else StackSetRender(wc->tstack, NULL);
    StackRedraw(wc->tstack);
  }

  return;
}

/*
 * Save
 */
static void
Save(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;
  char *url;
  ChimeraRequest *wr;

  if ((url = StackGetCurrentURL(wc->tstack)) == NULL) return;

  if ((wr = RequestCreate(wc->cres, url, NULL)) == NULL)
  {
    TextFieldSetString(wc->message, "Invalid URL.");
    return;
  }
  DownloadOpen(wc->cres, wr);

  return;
}

/*
 * Home
 */
static void
Home(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;
  StackHome(wc->tstack);
  return;
}

/*
 * Help
 */
static void
Help(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;
  char *url;

  if ((url = ResourceGetString(wc->cres, "chimera.helpURL")) != NULL)
  {
    StackOpen(wc->tstack, RequestCreate(wc->cres, url, NULL));
  }

  return;
}

/*
 * Dup
 */
static void
Dup(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;
  char *url;
  ChimeraRequest *wr;

  /* grab URL from to-be-cloned window */
  if ((url = ResourceGetString(wc->cres, "chimera.cloneHome")) == NULL)
  {
    url = StackGetCurrentURL(wc->tstack);
  }

  if (url != NULL) wr = RequestCreate(wc->cres, url, NULL);
  else wr = NULL;

  HeadCreate(wc->cres, NULL, wr);

  return;
}

/*
 * AddMark
 */
void
AddMark(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;
  char *title;
  char *url;
  ChimeraRender wn;

  if (wc->cres->bc == NULL) return;

  wn = StackToRender(wc->tstack);
  if ((url = RenderQuery(wn, "url")) == NULL) return;
  if ((title = RenderQuery(wn, "title")) == NULL) title = url;

  BookmarkAdd(wc->cres->bc, title, url);

  return;
}

/*
 * ViewMark
 */
void
ViewMark(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;
  MemPool mp;
  char *filename;
  char *url;
  const char *fformat = "file:%s";

  mp = MPCreate();

  if ((filename = ResourceGetFilename(wc->cres,
				      mp, "bookmark.filename")) == NULL)
  {
    return;
  }

  url = (char *)MPGet(mp, strlen(fformat) + strlen(filename) + 1);
  sprintf (url, fformat, filename);
  StackOpen(wc->tstack, RequestCreate(wc->cres, url, NULL));

  MPDestroy(mp);

  return;
}

/*
 * Bookmark
 */
static void
Bookmark(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;

  wc->cres->bmcontext = wc;
  if (wc->cres->bc != NULL) BookmarkShow(wc->cres->bc);

  return;
}

/*
 * OFind
 */
static void
OFind(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;
  char *str;

  if ((str = MyDialogGetValue(GetDialogWidget(wc->findpop))) == NULL)
  {
    return;
  }

  RenderSearch(StackToRender(wc->tstack), str, 0);

  XtPopdown(wc->findpop); 

  return;
}

/*
 * DFind
 */
static void
DFind(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;
  XtPopdown(wc->findpop); 
  return;
}

/*
 * Find
 */
static void
Find(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  ChimeraContext wc = (ChimeraContext)cldata;

  if (wc->findpop == NULL)
  {
    wc->findpop = CreateDialog(wc->toplevel, "findpop",
			       OFind, DFind, OFind, (XtPointer)wc);
  }
  XtPopup(wc->findpop, XtGrabNone);

  return;
}

/*
 * HeadCreate
 *
 * Frontend that sorts out which URL to use for a new head and then
 * creates a new head.
 */
void
HeadCreate(cres, first, lastresort)
ChimeraResources cres;
ChimeraRequest *first;
ChimeraRequest *lastresort;
{
  ChimeraContext wc;
  char *buffer;
  char *url;
  int count;
  MemPool mp;
  ChimeraRequest *wr = NULL;
  char base_url[255];

  /*
   * Default base URL; this allows filenames to be used on the
   * command line
   */
  strcpy( base_url, "file:" ) ;
  getcwd( base_url + 5, sizeof(base_url) - 5 ) ;
  strcat( base_url, "/" ) ;

  ChimeraAddReference(cres);

  mp = MPCreate();
  wc = (ChimeraContext)MPCGet(mp, sizeof(struct ChimeraContextP));
  wc->mp = mp;
  wc->cres = cres;

  GListAddHead(cres->heads, wc);

  CreateWidgets(wc, "chimera");

  /*
   * If an override URL is supplied then try to use that.
   */
  if (first != NULL) wr = first;

  /*
   * Look inside the cutbuffer for a valid URL.
   */
  if (wr == NULL)
  {
    buffer = XFetchBytes(cres->dpy, &count);
    if (count > 0)
    {
      mp = MPCreate();
      url = (char *)MPCGet(mp, count + 1);
      memcpy(url, buffer, count);
      url[count] = '\0';
      
      XFree(buffer);
      
      if ((wr = RequestCreate(wc->cres, url, base_url)) != NULL)
      {
	XStoreBytes(cres->dpy, "", 0);
      }
      
      MPDestroy(mp);
    }
  }

  /*
   * Look for the caller-supplied last resort.
   */
  if (wr == NULL && lastresort != NULL) wr = lastresort;

  /*
   * Check the standard environment variable.
   */
  if (wr == NULL)
  {
    if ((url = getenv("WWW_HOME")) != NULL)
    {
      if ((wr = RequestCreate(wc->cres, url, base_url)) == NULL)
      {
	fprintf (stderr, "WWW_HOME (%s) is invalid.\n", url);
      }
    }
  }

  /*
   * Check the chimera resource variable.
   */
  if (wr == NULL)
  {
    if ((url = ResourceGetString(wc->cres, "chimera.homeURL")) != NULL)
    {
      if ((wr = RequestCreate(wc->cres, url, base_url)) == NULL)
      {
	fprintf (stderr, "chimera.homeURL (%s) is invalid.\n", url);
      }
    }
  }

  /*
   * This better work...
   */
  if (wr == NULL)
  {
    wr = RequestCreate(wc->cres, "file:/", NULL);
  }

  myassert(wr != NULL, "ERROR: No valud URLs found for new head.\n");

  StackOpen(wc->tstack, wr);

  return;
}

/*
 * HeadPrintMessage
 */
void
HeadPrintMessage(wc, message)
ChimeraContext wc;
char *message;
{
  if (message == NULL) message = "";
  TextFieldSetString(wc->message, message);
  return;
}

/*
 * HeadPrintURL
 */
void
HeadPrintURL(wc, url)
ChimeraContext wc;
char *url;
{
  if (url == NULL) url = "";
  TextFieldSetString(wc->url, url);
  return;
}
