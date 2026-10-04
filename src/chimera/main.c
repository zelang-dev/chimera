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


#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <athena.h>

#include "MyDialog.h"

#include "ChimeraP.h"

static void sigigh_handler _ArgProto(());
static void xtWarningHandler _ArgProto((String));
static int xErrorHandler _ArgProto((Display *, XErrorEvent *));
static void ChimeraCleanup _ArgProto((ChimeraResources));
static ChimeraResources ResourcesCreate _ArgProto((int *, char **));
static void ResourcesDestroy _ArgProto((ChimeraResources));

extern char *fallback_resources[];

static ChimeraResources globalcres;
static char *default_resource = "bookmark.filename: ~/.chimera/bookmarks.html\n"
"cache.directory: ~/.chimera/cache\n"
"cache.persist: true\n"
"chimera.homeURL: http://www.google.com\n"
"html.propFontPattern: -adobe-helvetica-*-*-*-*-*-*-*-*-*-*-iso8859-1\n"
"view.capFiles: ~/.chimera/mailcap:~/.mailcap\n";

int main(int argc, char **argv) {
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
	strcpy(base_url, "file:");
	getcwd(base_url + 5, sizeof(base_url) - 5);
	strcat(base_url, "/");

	ats_t ui = {0};
	globalcres->ats = &ui;
	globalcres->ats->app_con = globalcres->appcon;
	globalcres->ats->dpy = globalcres->dpy;
	ats_athena_set(globalcres->ats, icon_32x32, "Browser", 650, 500);

	if (argc > 1)
		HeadCreate(globalcres, RequestCreate(globalcres, argv[argc - 1], base_url), NULL);
	else
		HeadCreate(globalcres, NULL, NULL);

	/*
	 * And away we go...
	 */
	ats_handler(globalcres->ats);
	ats_close(globalcres->ats);
	return 0;
}

/*
 * sigiqh_handler
 */
static void sigigh_handler() {
}

/*
 * xErrorHandler
 */
 int xErrorHandler(Display *dpy, XErrorEvent *xe) {
	fprintf(stderr, "X error\n");
	fflush(stderr);
	return 0;
}

/*
 * xtErrorHandler
 */
 static _X_NORETURN void xtErrorHandler(String msg) {
	fprintf(stderr, "%s\n", msg);
	fflush(stderr);
 }

/*
 * xtWarningHandler
 */
static void xtWarningHandler(String msg) {
	fprintf(stderr, "%s\n", msg);
	fflush(stderr);
}

/*
 * ChimeraAddReference
 */
void ChimeraAddReference(ChimeraResources cres) {
	cres->refcount++;
	return;
}

/*
 * ChimeraCleanup
 */
static void ChimeraCleanup(ChimeraResources cres) {
	if (cres->bc != NULL) BookmarkDestroyContext(cres->bc);

	ResourcesDestroy(cres);

	if (cres->logfp != NULL) fclose(cres->logfp);

	GListDestroy(cres->heads);

	MPPrintStatus();
	GListPrintStatus();

}

/*
 * ChimeraRemoveReference
 */
void ChimeraRemoveReference(ChimeraResources cres) {
	cres->refcount--;
	if (cres->refcount == 0)
		ChimeraCleanup(cres);
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
DeleteAction() {
	ChimeraCleanup(globalcres);
}

/*
 * ReturnAction
 */
static void ReturnAction(Widget w, XEvent *xe, String *params, Cardinal *num_params) {
	ChimeraContext c;
	char *url;

	for (c = (ChimeraContext)GListGetHead(globalcres->heads); c != NULL;
		c = (ChimeraContext)GListGetNext(globalcres->heads)) {
		if (c->url == w) {
			url = TextFieldGetString(c->url);
			if (url == NULL || url[0] == '\0') {
				TextFieldSetString(c->url, url);
				break;
			} else {
				StackOpen(c->tstack, RequestCreate(globalcres, url, NULL));
			}
			break;
		}
	}

	return;
}

ChimeraResources ResourcesCreate(int *argcp, char **argv) {
	ChimeraResources cres;
	MemPool mp, tmp;
	char *f, *filename;
	char *logfile;
	char *dbfiles;
	char db[1024], *p;
	struct stat s;

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

	if ((dbfiles = getenv("CHIMERA_DBFILES")) == NULL) {
		dbfiles = "~/.chimera/resources";
		if (!(p = getenv("HOME")))
			p = "/tmp";

		sprintf(db, "%s/.chimera", p);
		mkdir(db, 0700);
		strcat(db, "/resources");
		if (stat(db, &s) != 0) {
			FILE *dbf = fopen(db, "w");
			if (dbf) {
				fprintf(dbf, "%s", default_resource);
				fflush(dbf);
				fclose(dbf);
			}
		}
	}

	f = dbfiles;
	while ((filename = mystrtok(f, ':', &f)) != NULL) {
		ResourceAddFile(cres, filename);
	}

	cres->cc = CacheCreate(cres);

	if (ResourceGetInt(cres, "chimera.maxDownloads", &cres->maxiocnt) == NULL) {
		cres->maxiocnt = 4;
	} else if (cres->maxiocnt <= 1) cres->maxiocnt = 1;

	if (ResourceGetBool(cres, "chimera.printLoadMessages",
		&cres->printLoadMessages) == NULL) {
		cres->printLoadMessages = false;
	}

	if (ResourceGetBool(cres, "chimera.printTaskInfo",
		&cres->printTaskInfo) == NULL) {
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
	if (cres->dpy == NULL) {
		fprintf(stderr, "Could not open display.\n");
		exit(1);
	}

	XtAppAddActions(cres->appcon, actionsList, XtNumber(actionsList));

	cres->bc = BookmarkCreateContext(cres);

	InitChimeraBuiltins(cres);

	tmp = MPCreate();
	if ((logfile = ResourceGetFilename(cres, tmp,
		"chimera.urlLogFile")) != NULL) {
		cres->logfp = fopen(logfile, "a");
	}
	MPDestroy(tmp);

	cres->plainhooks = RenderGetHooks(cres, "text/plain");

	return(cres);
}

static void ResourcesDestroy(ChimeraResources cres) {
	ChimeraRenderHooks *rhooks;
	ChimeraSourceHooks *shooks;
	GList list;

	if (cres->cc != NULL) CacheDestroy(cres->cc);

	list = cres->renderhooks;
	for (rhooks = (ChimeraRenderHooks *)GListGetHead(list); rhooks != NULL;
		rhooks = (ChimeraRenderHooks *)GListGetNext(list)) {
		if (rhooks->class_destroy != NULL) {
			CMethod(rhooks->class_destroy)(rhooks->class_context);
		}
	}
	list = cres->sourcehooks;
	for (shooks = (ChimeraSourceHooks *)GListGetHead(list); shooks != NULL;
		shooks = (ChimeraSourceHooks *)GListGetNext(list)) {
		if (shooks->class_destroy != NULL) {
			CMethod(shooks->class_destroy)(shooks->class_closure);
		}
	}
	if (cres->db != NULL) XrmDestroyDatabase(cres->db);

	MPDestroy(cres->mp);

	return;
}
