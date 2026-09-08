/*
 * main.c
 *
 * Copyright (C) 1993-1997, John Kilburg <john@cs.unlv.edu>
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
#include <signal.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xaw/Toggle.h>

#include "port_after.h"

#include "TextField.h"
#include "MyDialog.h"

#include "ChimeraP.h"

static void sigigh_handler _ArgProto(());
static void xtWarningHandler _ArgProto((String));
static void xtErrorHandler _ArgProto((String));
static int xErrorHandler _ArgProto((Display *, XErrorEvent *));
static void ChimeraCleanup _ArgProto((ChimeraResources));
static ChimeraResources ResourcesCreate _ArgProto((int *, char **));
static void ResourcesDestroy _ArgProto((ChimeraResources));

extern char *fallback_resources[];

static ChimeraResources globalcres;

void
main(argc, argv)
int argc;
char **argv;
{
  char base_url[255];

  signal(SIGINT, sigigh_handler);
  signal(SIGQUIT, sigigh_handler);
  signal(SIGHUP, sigigh_handler);
  signal(SIGTERM, sigigh_handler);
  signal(SIGPIPE, SIG_IGN);

  StartReaper();

  globalcres = ResourcesCreate(&argc, argv);

  /*
   * Default base URL; this allows filenames to be used on the
   * command line
   */
  strcpy( base_url, "file:" ) ;
  getcwd( base_url + 5, sizeof(base_url) - 5 ) ;
  strcat( base_url, "/" ) ;

  if (argc > 1) HeadCreate(globalcres,
			   RequestCreate(globalcres, argv[argc - 1], base_url),
			   NULL);
  else HeadCreate(globalcres, NULL, NULL);

  /*
   * And away we go...
   */
  XtAppMainLoop(globalcres->appcon);
}

/*
 * sigiqh_handler
 */
static void
sigigh_handler()
{
  return;
}

/*
 * xErrorHandler
 */
int
xErrorHandler(dpy, xe)
Display *dpy;
XErrorEvent *xe;
{
  fprintf (stderr, "X error\n");
  fflush(stderr);
  return(0);
}

/*
 * xtErrorHandler
 */
static void
xtErrorHandler(msg)
String msg;
{
  fprintf (stderr, "%s\n", msg);
  fflush(stderr);
  return;
}

/*
 * xtWarningHandler
 */
static void
xtWarningHandler(msg)
String msg;
{
  fprintf (stderr, "%s\n", msg);
  fflush(stderr);
  return;
}

/*
 * ChimeraAddReference
 */
void
ChimeraAddReference(cres)
ChimeraResources cres;
{
  cres->refcount++;
  return;
}

/*
 * ChimeraCleanup
 */
static void
ChimeraCleanup(cres)
ChimeraResources cres;
{
  if (cres->bc != NULL) BookmarkDestroyContext(cres->bc);
  
  ResourcesDestroy(cres);
  
  if (cres->logfp != NULL) fclose(cres->logfp);
  
  GListDestroy(cres->heads);
  
  MPPrintStatus();
  GListPrintStatus();
  
  exit(0);
}

/*
 * ChimeraRemoveReference
 */
void
ChimeraRemoveReference(cres)
ChimeraResources cres;
{
  cres->refcount--;
  if (cres->refcount == 0) ChimeraCleanup(cres);
  return;
}

static void DeleteAction(), ReturnAction();

static XtActionsRec actionsList[] =
{
  { "ReturnAction", (XtActionProc)ReturnAction },
  { "DeleteAction",  (XtActionProc)DeleteAction  },
};

/*
 * DeleteAction
 */
static void
DeleteAction()
{
  ChimeraCleanup(globalcres);
}

/*
 * ReturnAction
 */
static void
ReturnAction(w, xe, params, num_params)
Widget w;
XEvent *xe;
String *params;
Cardinal *num_params;
{
  ChimeraContext c;
  char *url;

  for (c = (ChimeraContext)GListGetHead(globalcres->heads); c != NULL;
       c = (ChimeraContext)GListGetNext(globalcres->heads))
  {
    if (c->url == w)
    {
      url = TextFieldGetString(c->url);
      if (url == NULL || url[0] == '\0')
      {
	TextFieldSetString(c->url, url);
	break;
      }
      else
      {
	StackOpen(c->tstack, RequestCreate(globalcres, url, NULL));
      }
      break;
    }
  }

  return;
}

ChimeraResources
ResourcesCreate(argcp, argv)
int *argcp;
char *argv[];
{
  ChimeraResources cres;
  MemPool mp, tmp;
  char *f, *filename;
  char *logfile;
  char *dbfiles;

  mp = MPCreate();
  cres = (ChimeraResources)MPCGet(mp, sizeof(struct ChimeraResourcesP));
  cres->mp = mp;
  cres->sources = GListCreateX(mp);
  cres->sourcehooks = GListCreateX(mp);
  cres->renderhooks = GListCreateX(mp);
  cres->mimes = GListCreateX(mp);
  cres->timeouts = GListCreateX(mp);
  cres->oldtimeouts = GListCreateX(mp);
  cres->heads = GListCreateX(mp);
  cres->stacks = GListCreateX(mp);

  cres->cs = SchedulerCreate();

  ResourceAddString(cres, "cache.Directory: /tmp");
  ResourceAddString(cres, "http.userAgent: Chimera/2.0alpha");
  ResourceAddString(cres, "mailto.newhead: true");

  if ((dbfiles = getenv("CHIMERA_DBFILES")) == NULL)
  {
    dbfiles = "~/.chimera/resources";
  }

  f = dbfiles;
  while ((filename = mystrtok(f, ':', &f)) != NULL)
  {
    ResourceAddFile(cres, filename);
  }

  cres->cc = CacheCreate(cres);

  if (ResourceGetInt(cres, "chimera.maxDownloads", &cres->maxiocnt) == NULL)
  {
    cres->maxiocnt = 4;
  }
  else if (cres->maxiocnt <= 1) cres->maxiocnt = 1;

  if (ResourceGetBool(cres, "chimera.printLoadMessages",
		      &cres->printLoadMessages) == NULL)
  {
    cres->printLoadMessages = false;
  }

  if (ResourceGetBool(cres, "chimera.printTaskInfo",
		      &cres->printTaskInfo) == NULL)
  {
    cres->printTaskInfo = false;
  }

  /*
   * Initialize the Xt stuff.
   */
  XtToolkitInitialize();

  cres->appcon = XtCreateApplicationContext();
  XSetErrorHandler(xErrorHandler);
  XtAppSetErrorHandler(cres->appcon, xtErrorHandler);
  XtAppSetWarningHandler(cres->appcon, xtWarningHandler);

  XtAppSetFallbackResources(cres->appcon, fallback_resources);
  cres->dpy = XtOpenDisplay(cres->appcon, NULL,
			  NULL, "Chimera",
			  NULL, 0,
			  argcp, argv);
  if (cres->dpy == NULL)
  {
    fprintf (stderr, "Could not open display.\n");
    exit(1);
  }

  XtAppAddActions(cres->appcon, actionsList, XtNumber(actionsList));

  cres->bc = BookmarkCreateContext(cres);

  InitChimeraBuiltins(cres);

  tmp = MPCreate();
  if ((logfile = ResourceGetFilename(cres, tmp,
				     "chimera.urlLogFile")) != NULL)
  {
    cres->logfp = fopen(logfile, "a");
  }
  MPDestroy(tmp);

  cres->plainhooks = RenderGetHooks(cres, "text/plain");

  return(cres);
}

static void
ResourcesDestroy(cres)
ChimeraResources cres;
{
  ChimeraRenderHooks *rhooks;
  ChimeraSourceHooks *shooks;
  GList list;

  if (cres->cc != NULL) CacheDestroy(cres->cc);

  list = cres->renderhooks;
  for (rhooks = (ChimeraRenderHooks *)GListGetHead(list); rhooks != NULL;
       rhooks = (ChimeraRenderHooks *)GListGetNext(list))
  {
    if (rhooks->class_destroy != NULL)
    {
      CMethod(rhooks->class_destroy)(rhooks->class_context);
    }
  }
  list = cres->sourcehooks;
  for (shooks = (ChimeraSourceHooks *)GListGetHead(list); shooks != NULL;
       shooks = (ChimeraSourceHooks *)GListGetNext(list))
  {
    if (shooks->class_destroy != NULL)
    {
      CMethod(shooks->class_destroy)(shooks->class_closure);
    }
  }
  if (cres->db != NULL) XrmDestroyDatabase(cres->db);

  MPDestroy(cres->mp);

  return;
}
