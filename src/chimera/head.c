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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "common.h"
#include <athena.h>

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
static void Go _ArgProto((Widget, XtPointer, XtPointer));

static void OOpen _ArgProto((Widget, XtPointer, XtPointer));
static void DOpen _ArgProto((Widget, XtPointer, XtPointer));
static void Open _ArgProto((Widget, XtPointer, XtPointer));

static void OFind _ArgProto((Widget, XtPointer, XtPointer));
static void DFind _ArgProto((Widget, XtPointer, XtPointer));
static void Find _ArgProto((Widget, XtPointer, XtPointer));

static void CreateWidgets _ArgProto((ChimeraContext));

static void AddButtons _ArgProto((ChimeraContext, Widget, char *));

static void InstallAccelerators _ArgProto((Widget));

static struct ButtonTable {
	char *name;
	Boolean toggle;
	Widget w;
	void (*cb)();
	char *accel;
} ButtonTable[] =
{
  {"quit",     False, NULL, Quit,     ":Meta<KeyUp>q"},
  {"open",	False, NULL, Open,     ":Meta<KeyUp>o"},
  {"home",	False, NULL, Home,     ":Meta<KeyUp>h"},
  {"back",	False, NULL, Back,     ":Meta<KeyUp>b"},
  {"reload",   False, NULL, Reload,   ":Meta<KeyUp>r"},
  {"cancel",   False, NULL, Cancel,   ":Meta<KeyUp>c"},
  {"dup",	False, NULL, Dup,      ":Meta<KeyUp>c"},
  {"help",	False, NULL, Help,     ":Meta<KeyUp>H"},
  {"addmark",	False, NULL, AddMark,  ":Meta<KeyUp>a"},
  {"viewmark", False, NULL, ViewMark, ":Meta<KeyUp>v"},
  {"bookmark", False, NULL, Bookmark, ":Meta<KeyUp>m"},
  {"find",     False, NULL, Find,     ":Meta<KeyUp>f"},
  {"source",   True,  NULL, Source,   ":Meta<KeyUp>s"},
  {"save",     False, NULL, Save,     ":Meta<KeyUp>S"},
  {NULL,       False, NULL, NULL,     NULL},
};

static void AddButtons(ChimeraContext wc, Widget box, char *list) {
	char name[256];
	struct ButtonTable *btp;
	char accel[256];

	while (sscanf(list, " %[^,]", name) == 1) {
	  /*
	   * Find the listed button and create its widget
	   */
		for (btp = &ButtonTable[0]; btp->name != NULL; btp++) {
			if (!strcasecmp(btp->name, name)) {
				if (btp->toggle) {
					snprintf(accel, sizeof(accel) - 1,
						"%s: toggle() notify()", btp->accel);
					btp->w = XtVaCreateManagedWidget(btp->name,
						toggleWidgetClass, box,
						XtNaccelerators,
						XtParseAcceleratorTable(accel),
						NULL);
				} else {
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
static void InstallAccelerators(Widget w) {
	struct ButtonTable *btp;

	for (btp = &ButtonTable[0]; btp->name != NULL; btp++)
		if (btp->w)
			XtInstallAllAccelerators(w, btp->w);
}

#define BUTTON_LIST "quit, open, home, back, reload, source, save, bookmark, dup, find, cancel"

/*
 * These are initialized in fallback.c
 */
#define offset(field) XtOffset(ChimeraContext, field)
static XtResource       resource_list[] =
{
  {"button1Box", "BoxList", XtRString, sizeof(char *),
		offset(button1Box), XtRString, BUTTON_LIST},
  {"button2Box", "BoxList", XtRString, sizeof(char *),
		offset(button2Box), XtRString, NULL},
};

/*
 * HeadCreate
 *
 * Setup chimera in a widget.
 */
static void CreateWidgets(ChimeraContext wc) {
	Widget paned, box, form;
	Atom delete;

	wc->toplevel = XtAppCreateShell(wc->athena->title, "Chimera",
		mwApplicationShellWidgetClass,
		wc->cres->dpy,
		NULL, 0);

	MwInitFormat(wc->cres->dpy);
	XtGetApplicationResources(wc->toplevel, wc,
		resource_list, XtNumber(resource_list),
		NULL, 0);

	wc->athena->topLevel = wc->toplevel;

/*
 * Main window, Button and Menu pane
 */
	wc->athena->toolheight = 30;
	ats_menubar_set(wc->athena, 2);
	paned = wc->athena->grid;

	static menuitem_t menu_items[] = {
		{110, "Open", (_menu_cb)Open, "o", NULL},
		{111, "Save", (_menu_cb)Save, "S", NULL},
		{__ATS_SEPERATOR__},
		{112, "Quit", (_menu_cb)Quit, "q", NULL},
	};

	static menuitem_t menu_items_two[] = {
		{113, "Bookmark", (_menu_cb)Bookmark, "m", NULL},
		{114, "Find", (_menu_cb)Find, "f", NULL},
		{115, "Source", (_menu_cb)Source, "V", NULL},
		{__ATS_SEPERATOR__},
		{116, "Help", (_menu_cb)Help, "h", NULL},
	};

	wc->athena->bar_info->override = true;
	wc->athena->bar_info->override_data = wc;
	if (!ats_font_set(wc->athena, lucida)
		|| !ats_menu_set(wc->athena, 0, menu_items, 4, 1, "File")
		|| !ats_menu_set(wc->athena, 1, menu_items_two, 5, 2, "Tools")) {
		fprintf(stderr, "Error: can't create `font` or `menu` items!\n");
		return;
	}

/*
 * Button pane(s)
 */
	box = ats_navigation_set(paned, 1);
	wc->athena->app->app_data = (void *)wc;
	ats_toolbar_set(wc->athena, box, Home, "home.xpm", "Home", true);
	ats_toolbar_set(wc->athena, box, Back, "back.xpm", "Back", true);
	//toolcmd = ats_toolbar_set(wc->athena, form, cb_forward, "forward.xpm", "Forward", true);
	ats_toolbar_set(wc->athena, box, Reload, "reload.xpm", "Reload", true);
	ats_toolbar_set(wc->athena, box, Cancel, "cancel.xpm", "Cancel", true);
	ats_toolbar_set(wc->athena, box, Open, "fld_open.xpm", "Open", true);
	form = ats_toolbar_set(wc->athena, box, Save, "save.xpm", "Save", true);

	/*
	 * URL pane
	 */
	int numtools = 7;
	wc->url = ats_textfield_set(box, "", 0, 1, (wc->athena->width - (38 * numtools)), 28);
	ats_alignfield(wc->url, form, false);

	form = ats_toolbar_set(wc->athena, box, Go, "preview.xpm", "Go", true);
	ats_alignfield(form, wc->url, false);
	XtOverrideTranslations(wc->url,
		XtParseTranslationTable
		("<Key>Return: ReturnAction()"));

/*
 * WWW widget
 */
	box = ats_mainarea_set(wc->athena, paned, 2);

/*
 * Message `statusline` pane.
 */
	wc->message = ats_status_set(paned, "", 3, 500, 10);
	ats_alignfield(wc->message, box, false);

	paned = ats_viewport_set(box, false, false);
	wc->tstack = StackCreateToplevel(wc, paned);

	XtRealizeWidget(wc->toplevel);

	wc->athena->win = XtWindow(wc->athena->topLevel);
	wc->athena->screen = DefaultScreen(wc->athena->dpy);
	ats_icon_set(wc->athena, "athena.xpm");

	wc->athena->wmDeleteMessage = XInternAtom(XtDisplay(wc->toplevel), "WM_DELETE_WINDOW", False);
	XSetWMProtocols(wc->cres->dpy, wc->athena->win, &wc->athena->wmDeleteMessage, 1);
	XtOverrideTranslations(wc->toplevel,
		XtParseTranslationTable
		("<Message>WM_PROTOCOLS: DeleteAction()"));

/*
 * Accelerators
 */
	InstallAccelerators(paned);
}

/*
 * HeadDestroy
 */
void HeadDestroy(ChimeraContext wc) {
	StackDestroy(wc->tstack);
	GListRemoveItem(wc->cres->heads, wc);
	ats_t *ui = wc->athena;
	ats_cancel(ui->topLevel);
	//XtDestroyWidget(wc->toplevel);
	MPDestroy(wc->mp);
	ChimeraRemoveReference(wc->cres);
	ui->app_con = NULL;
}

/*
 * ClearDialogValue
 */
static void ClearDialogValue(Widget w, XtPointer cldata, XtPointer cbdata) {
	if ((w = XtParent(w)) != NULL) MyDialogSetValue(w, "");

	return;
}

/*
 * CreateDialog
 */
Widget CreateDialog(Widget p, char *name, void (*ofunc)(), void (*dfunc)(),
	void (*rfunc)(), XtPointer closure) {
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
Widget GetDialogWidget(Widget w) {
	return(XtNameToWidget(w, "dialog"));
}

/*
 * OOpen
 */
static void OOpen(Widget ww, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;
	char *url;

	if ((url = MyDialogGetValue(GetDialogWidget(wc->openpop))) == NULL) return;

	StackOpen(wc->tstack, RequestCreate(wc->cres, url, NULL));
	XtPopdown(wc->openpop);
}

/*
 * DOpen
 */
static void DOpen(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;
	XtPopdown(wc->openpop);
}

/*
 * Open
 */
static void Open(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;

	if (wc->openpop == NULL) {
		wc->openpop = CreateDialog(wc->toplevel, "openpop",
			OOpen, DOpen, OOpen, (XtPointer)wc);
		MwSetIcon(wc->openpop, icon_32x32);
	}
	XtPopup(wc->openpop, XtGrabNone);
}

/*
 * Back
 */
static void Back(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;
	StackBack(wc->tstack);
}

/*
 * Reload
 */
static void Reload(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;
	StackReload(wc->tstack);
}

/*
 * Cancel
 */
static void
Cancel(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;
	StackCancel(wc->tstack);
}

/*
 * Quit
 */
static void Quit(Widget w, XtPointer cldata, XtPointer cbdata) {
	HeadDestroy((ChimeraContext)cldata);
}

/*
 * Source
 */
static void Source(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;

	if (wc->cres->plainhooks == NULL) XtVaSetValues(w, XtNstate, False, NULL);
	else {
		if (cbdata) StackSetRender(wc->tstack, wc->cres->plainhooks);
		else StackSetRender(wc->tstack, NULL);
		StackRedraw(wc->tstack);
	}
}

/*
 * Save
 */
static void Save(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;
	char *url;
	ChimeraRequest *wr;

	if ((url = StackGetCurrentURL(wc->tstack)) == NULL) return;

	if ((wr = RequestCreate(wc->cres, url, NULL)) == NULL) {
		TextFieldSetString(wc->message, "Invalid URL.");
		return;
	}
	DownloadOpen(wc->cres, wr);
}

/*
 * Home
 */
static void Home(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;
	StackHome(wc->tstack);
}

/*
 * Help
 */
static void Help(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;
	char *url;

	if ((url = ResourceGetString(wc->cres, "chimera.helpURL")) != NULL) {
		StackOpen(wc->tstack, RequestCreate(wc->cres, url, NULL));
	}
}

/*
 * Dup
 */
static void Dup(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;
	char *url;
	ChimeraRequest *wr;

	/* grab URL from to-be-cloned window */
	if ((url = ResourceGetString(wc->cres, "chimera.cloneHome")) == NULL) {
		url = StackGetCurrentURL(wc->tstack);
	}

	if (url != NULL) wr = RequestCreate(wc->cres, url, NULL);
	else wr = NULL;

	HeadCreate(wc->cres, NULL, wr);
}

static void Go(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraResources globalcres = ((ChimeraContext)cldata)->cres;
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
}

/*
 * AddMark
 */
void AddMark(Widget w, XtPointer cldata, XtPointer cbdata) {
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
void ViewMark(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;
	MemPool mp;
	char *filename;
	char *url;
	const char *fformat = "file:%s";

	mp = MPCreate();

	if ((filename = ResourceGetFilename(wc->cres,
		mp, "bookmark.filename")) == NULL) {
		return;
	}

	url = (char *)MPGet(mp, strlen(fformat) + strlen(filename) + 1);
	sprintf(url, fformat, filename);
	StackOpen(wc->tstack, RequestCreate(wc->cres, url, NULL));

	MPDestroy(mp);

	return;
}

/*
 * Bookmark
 */
static void Bookmark(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;

	wc->cres->bmcontext = wc;
	if (wc->cres->bc != NULL) BookmarkShow(wc->cres->bc);

	return;
}

/*
 * OFind
 */
static void OFind(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;
	char *str;

	if ((str = MyDialogGetValue(GetDialogWidget(wc->findpop))) == NULL) {
		return;
	}

	RenderSearch(StackToRender(wc->tstack), str, 0);

	XtPopdown(wc->findpop);

	return;
}

/*
 * DFind
 */
static void DFind(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;
	XtPopdown(wc->findpop);
	return;
}

/*
 * Find
 */
static void Find(Widget w, XtPointer cldata, XtPointer cbdata) {
	ChimeraContext wc = (ChimeraContext)cldata;

	if (wc->findpop == NULL) {
		wc->findpop = CreateDialog(wc->toplevel, "findpop",
			OFind, DFind, OFind, (XtPointer)wc);

		MwSetIcon(wc->findpop, icon_32x32);
	}
	XtPopup(wc->findpop, XtGrabNone);
}

/*
 * HeadCreate
 *
 * Frontend that sorts out which URL to use for a new head and then
 * creates a new head.
 */
void HeadCreate(ChimeraResources cres, ChimeraRequest *first, ChimeraRequest *lastresort) {
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
	strcpy(base_url, "file:");
	getcwd(base_url + 5, sizeof(base_url) - 5);
	strcat(base_url, "/");

	ChimeraAddReference(cres);

	mp = MPCreate();
	wc = (ChimeraContext)MPCGet(mp, sizeof(struct ChimeraContextP));
	wc->athena = cres->ats;
	wc->mp = mp;
	wc->cres = cres;

	GListAddHead(cres->heads, wc);

	CreateWidgets(wc);

	/*
	 * If an override URL is supplied then try to use that.
	 */
	if (first != NULL) wr = first;

	/*
	 * Look inside the cutbuffer for a valid URL.
	 */
	if (wr == NULL) {
		buffer = XFetchBytes(cres->dpy, &count);
		if (count > 0) {
			mp = MPCreate();
			url = (char *)MPCGet(mp, count + 1);
			memcpy(url, buffer, count);
			url[count] = '\0';

			XFree(buffer);

			if ((wr = RequestCreate(wc->cres, url, base_url)) != NULL) {
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
	if (wr == NULL) {
		if ((url = getenv("WWW_HOME")) != NULL) {
			if ((wr = RequestCreate(wc->cres, url, base_url)) == NULL) {
				fprintf(stderr, "WWW_HOME (%s) is invalid.\n", url);
			}
		}
	}

	/*
	 * Check the chimera resource variable.
	 */
	if (wr == NULL) {
		if ((url = ResourceGetString(wc->cres, "chimera.homeURL")) != NULL) {
			if ((wr = RequestCreate(wc->cres, url, base_url)) == NULL) {
				fprintf(stderr, "chimera.homeURL (%s) is invalid.\n", url);
			}
		}
	}

	/*
	 * This better work...
	 */
	if (wr == NULL) {
		wr = RequestCreate(wc->cres, "file:/", NULL);
	}

	myassert(wr != NULL, "ERROR: No valud URLs found for new head.\n");

	StackOpen(wc->tstack, wr);

	return;
}

/*
 * HeadPrintMessage
 */
void HeadPrintMessage(ChimeraContext wc, char *message) {
	if (message == NULL) message = "";
	TextFieldSetString(wc->message, message);
	return;
}

/*
 * HeadPrintURL
 */
void HeadPrintURL(ChimeraContext wc, char *url) {
	if (url == NULL) url = "";
	TextFieldSetString(wc->url, url);
	return;
}
