/*
 * bookmark.c
 *
 * Copyright (c) 1996-1997, John Kilburg <john@cs.unlv.edu>
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

#include <sys/types.h>
#include <sys/stat.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>

#include <X11/Xaw/Label.h>
#include <X11/Xaw/List.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Viewport.h>

#include "port_after.h"

#include "ChimeraP.h"
#include "ml.h"

#include "MyDialog.h"

typedef struct
{
  char        *title;
  char        *url;
} BMark;

typedef struct
{
  char        *name;
  GList       mlist;
} BGroup;

struct BookmarkContextP
{
  MemPool     mp;
  GList       glist;
  GList       nel;              /* name element list <h3></h3> or <a></a> */
  bool        is_group_element;
  bool        is_mark_element;
  MLState     ml;
  char        *filename;        /* bookmark filename */
  Widget      bw;               /* bookmark shell widget */
  Widget      glw;
  Widget      mlw;
  Widget      ampop;
  bool        ampopped;
  Widget      agpop;
  bool        agpopped;
  char        **gnames;         /* group name array for list widget */
  int         glen;
  char        **mnames;         /* bookmark array for list widget */
  int         mlen;
  char        *header;
  char        *footer;
  ChimeraResources cres;
};

static BGroup *GroupCreate _ArgProto((BookmarkContext, char *, bool));
static void BMChangeGroupList _ArgProto((BookmarkContext));
static void BMChangeMarkList _ArgProto((BookmarkContext));
static BGroup *BMFindGroup _ArgProto((BookmarkContext));
static BMark *BMFindMark _ArgProto((BookmarkContext));
static void BMWrite _ArgProto((BookmarkContext));
static void BMCreate _ArgProto((BookmarkContext, BGroup *,
                               char *, char *));
static void BMElementHandler _ArgProto((void *, MLElement));
static char *BMGetText _ArgProto((MemPool, GList));

static void BMDAddGroup _ArgProto((Widget, XtPointer, XtPointer));
static void BMDAddMark _ArgProto((Widget, XtPointer, XtPointer));

/*
 * BMChangeGroupList
 */
static void
BMChangeGroupList(bc)
BookmarkContext bc;
{
  int cnt;
  BGroup *g;

  for (cnt = 0, g = (BGroup *)GListGetHead(bc->glist); g != NULL;
       cnt++, g = (BGroup *)GListGetNext(bc->glist))
  {
    ;
  }
  if (bc->gnames == NULL)
  {
    bc->gnames = (char **)alloc_mem(sizeof(char *) * (cnt + 1));
    bc->glen = cnt;
  }
  else if (bc->glen < cnt)
  {
    bc->gnames = (char **)realloc_mem(bc->gnames, sizeof(char *) * (cnt + 1));
    bc->glen = cnt;
  }
  for (g = (BGroup *)GListGetHead(bc->glist), cnt = 0; g != NULL;
       g = (BGroup *)GListGetNext(bc->glist))
  {
    bc->gnames[cnt++] = g->name;
  }
  bc->gnames[cnt] = NULL;
  XawListChange(bc->glw, bc->gnames, 0, 0, True);
  if (cnt > 0) XawListHighlight(bc->glw, 0);

  BMChangeMarkList(bc);

  return;
}

/*
 * BMChangeMarkList
 */
static void
BMChangeMarkList(bc)
BookmarkContext bc;
{
  int cnt;
  BMark *m;
  BGroup *g;

  if ((g = BMFindGroup(bc)) == NULL) return;

  for (cnt = 0, m = (BMark *)GListGetHead(g->mlist); m != NULL;
       cnt++, m = (BMark *)GListGetNext(g->mlist))
  {
    ;
  }
  if (bc->mnames == NULL)
  {
    bc->mnames = (char **)alloc_mem(sizeof(char *) * (cnt + 2));
    bc->mlen = cnt;
  }
  else if (bc->mlen < cnt)
  {
    bc->mnames = (char **)realloc_mem(bc->mnames, sizeof(char *) * (cnt + 2));
    bc->mlen = cnt;
  }
  for (m = (BMark *)GListGetHead(g->mlist), cnt = 0; m != NULL;
       m = (BMark *)GListGetNext(g->mlist))
  {
    bc->mnames[cnt++] = m->title;
  }
  if (cnt > 0)
  {
    bc->mnames[cnt] = NULL;
    XawListChange(bc->mlw, bc->mnames, 0, 0, True);
    XawListHighlight(bc->mlw, 0);
  }
  else
  {
    bc->mnames[cnt] = "";
    bc->mnames[cnt + 1] = NULL;
    XawListChange(bc->mlw, bc->mnames, 0, 0, True);
  }

  return;
}

/*
 * BMWrite
 */
static void
BMWrite(bc)
BookmarkContext bc;
{
  FILE *fp;
  const char *brec = "<li><a href=\"%s\">%s</a>\n";
  BMark *c;
  BGroup *g;

  if ((fp = fopen(bc->filename, "w")) != NULL)
  {
    fprintf (fp, bc->header);
    fprintf (fp, "\n");
    for (g = (BGroup *)GListGetHead(bc->glist); g != NULL;
	 g = (BGroup *)GListGetNext(bc->glist))
    {
      fprintf (fp, "<h3>%s</h3>\n<ul>\n", g->name);
      for (c = (BMark *)GListGetHead(g->mlist); c != NULL;
	   c = (BMark *)GListGetNext(g->mlist))
      {
	fprintf (fp, brec, c->url, c->title);
      }
      fprintf (fp, "</ul>\n");
    }
    fprintf (fp, bc->footer);
    fprintf (fp, "\n");
    fclose(fp);
  }

  return;
}

/*
 * GroupCreate
 */
static BGroup *
GroupCreate(bc, group, top)
BookmarkContext bc;
char *group;
bool top;
{
  BGroup *g;

  g = (BGroup *)MPCGet(bc->mp, sizeof(BGroup));
  g->name = MPStrDup(bc->mp, group);
  g->mlist = GListCreateX(bc->mp);

  if (top) GListAddHead(bc->glist, g);
  else GListAddTail(bc->glist, g);

  BMChangeGroupList(bc);

  return(g);
}

/*
 * BMCreate
 */
static void
BMCreate(bc, g, title, url)
BookmarkContext bc;
BGroup *g;
char *title;
char *url;
{
  BMark *n;

  n = (BMark *)MPCGet(bc->mp, sizeof(BMark));
  n->url = MPStrDup(bc->mp, url);
  if (title == NULL) n->title = MPStrDup(bc->mp, url);
  else n->title = MPStrDup(bc->mp, title);

  GListAddTail(g->mlist, n);

  BMChangeMarkList(bc);

  return;
}

/*
 * BMGetText
 */
static char *
BMGetText(mp, list)
MemPool mp;
GList list;
{
  MLElement c;
  char *text, *str;
  size_t tlen = 0;
  size_t len;

  if (GListEmpty(list)) return(NULL);

  /* Skip first element...it is the opening tag */
  GListGetHead(list);
  for (c = (MLElement)GListGetNext(list); c != NULL;
       c = (MLElement)GListGetNext(list))
  {
    MLGetText(c, &str, &len);
    tlen += len;
  }

  text = MPGet(mp, tlen + 1);

  tlen = 0;
  GListGetHead(list);
  for (c = (MLElement)GListGetNext(list); c != NULL;
       c = (MLElement)GListGetNext(list))
  {
    MLGetText(c, &str, &len);
    strncpy(text + tlen, str, len);
    tlen += len;
  }
  text[tlen] = '\0';

  return(text);
}

/*
 * BMElementHandler
 */
static void
BMElementHandler(closure, p)
void *closure;
MLElement p;
{
  BookmarkContext bc = (BookmarkContext)closure;
  char *value;
  char *title;
  char *name;
  MLElementType mt;
  MLElement h;

  mt = MLGetType(p);
  if (mt == ML_EOF) return;

  if ((name = MLTagName(p)) == NULL)
  {
    if (bc->is_group_element || bc->is_mark_element) GListAddTail(bc->nel, p);
    return;
  }
  
  if (strlen(name) == 2 && strcasecmp("h3", name) == 0)
  {
    if (MLGetType(p) == ML_ENDTAG)
    {
      if (bc->is_group_element)
      {
	GroupCreate(bc, BMGetText(bc->mp, bc->nel), false);
	bc->is_group_element = false;
      }
    }
    else
    {
      bc->is_group_element = true;
      GListClear(bc->nel);
      GListAddHead(bc->nel, p);
    }
  }
  else if (strlen(name) == 1 && strcasecmp("a", name) == 0)
  {
    if (MLGetType(p) == ML_ENDTAG)
    {
      if (bc->is_mark_element)
      {
	bc->is_mark_element = false;
	h = (MLElement)GListGetHead(bc->nel);
	if (h == NULL) return;
	if ((value = MLFindAttribute(h, "href")) != NULL)
	{
	  title = BMGetText(bc->mp, bc->nel);
	  if (title == NULL || title[0] == '\0')
	  {
	    title = MPStrDup(bc->mp, value);
	  }
	  if (GListEmpty(bc->glist)) GroupCreate(bc, "default", true);
	  BMCreate(bc, (BGroup *)GListGetTail(bc->glist),
		   title, MPStrDup(bc->mp, value));
	}
      }
    }
    else
    {
      bc->is_mark_element = true;
      GListClear(bc->nel);
      GListAddHead(bc->nel, p);
    }
  }

  return;
}

/*
 * BookmarkAdd
 */
void
BookmarkAdd(bc, title, url)
BookmarkContext bc;
char *title;
char *url;
{
  BGroup *g;

  if ((g = BMFindGroup(bc)) == NULL) return;

  BMCreate(bc, g, title, url);

  return;
}

/*
 * BookmarkDestroyContext
 */
void
BookmarkDestroyContext(bc)
BookmarkContext bc;
{
  XtDestroyWidget(bc->bw);
  if (bc->mnames != NULL) free_mem(bc->mnames);
  if (bc->gnames != NULL) free_mem(bc->gnames);
  MPDestroy(bc->mp);
  return;
}

/*
 * BookmarkShow
 */
void
BookmarkShow(bc)
BookmarkContext bc;
{
  XtMapWidget(bc->bw);
  return;
}

/*
 * BMFindGroup
 */
static BGroup *
BMFindGroup(bc)
BookmarkContext bc;
{
  int i;
  BGroup *g;
  XawListReturnStruct *lrs;

  if ((lrs = XawListShowCurrent(bc->glw)) == NULL) return(NULL);
  if (lrs->list_index < 0) return(NULL);

  for (i = 0, g = (BGroup *)GListGetHead(bc->glist);
       i < lrs->list_index && g != NULL;
       i++, g = (BGroup *)GListGetNext(bc->glist))
  {
    ;
  }
  return(g);
}

/*
 * BMFindMark
 */
static BMark *
BMFindMark(bc)
BookmarkContext bc;
{
  int i;
  BMark *m;
  BGroup *g;
  XawListReturnStruct *lrs;

  if ((g = BMFindGroup(bc)) == NULL) return(NULL);
  if ((lrs = XawListShowCurrent(bc->mlw)) == NULL) return(NULL);
  if (lrs->list_index < 0) return(NULL);

  for (i = 0, m = (BMark *)GListGetHead(g->mlist);
       i < lrs->list_index && m != NULL;
       i++, m = (BMark *)GListGetNext(g->mlist))
  {
    ;
  }
  return(m);
}

/*
 * BMDAddGroup
 */
static void
BMDAddGroup(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  BookmarkContext bc = (BookmarkContext)cldata;

  XtPopdown(bc->agpop);
  bc->agpopped = false;

  return;
}

/*
 * BMOAddGroup
 */
static void
BMOAddGroup(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  BookmarkContext bc = (BookmarkContext)cldata;
  char *name;

  if ((name = MyDialogGetValue(GetDialogWidget(bc->agpop))) == NULL ||
      name[0] == '\0')
  {
    return;
  }

  GroupCreate(bc, name, true);

  BMDAddGroup(w, cldata, cbdata);

  BMWrite(bc);

  return;
}

/*
 * BMAddGroup
 */
static void
BMAddGroup(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  BookmarkContext bc = (BookmarkContext)cldata;

  if (bc->agpopped) return;

  if (bc->agpop == NULL)
  {
    bc->agpop = CreateDialog(bc->bw, "agpop",
                             BMOAddGroup, BMDAddGroup, BMOAddGroup, bc);
  }
  MyDialogSetValue(GetDialogWidget(bc->agpop), "");

  XtPopup(bc->agpop, XtGrabNone);
  bc->agpopped = true;

  return;
}

/*
 * BMRMGroup
 */
static void
BMRMGroup(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  BookmarkContext bc = (BookmarkContext)cldata;
  BGroup *g;

  if ((g = BMFindGroup(bc)) != NULL)
  {
    GListRemoveItem(bc->glist, g);
    if (GListEmpty(bc->glist)) GroupCreate(bc, "default", true);

    BMChangeGroupList(bc);
    BMChangeMarkList(bc);
    
    BMWrite(bc);    
  }

  return;
}

/*
 * BMDAddMark
 */
static void
BMDAddMark(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  BookmarkContext bc = (BookmarkContext)cldata;

  XtPopdown(bc->ampop);
  bc->ampopped = false;

  return;
}

/*
 * BMOAddMark
 */
static void
BMOAddMark(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  BookmarkContext bc = (BookmarkContext)cldata;
  char *name;
  char *url;

  if ((name = MyDialogGetValue(GetDialogWidget(bc->ampop))) == NULL ||
      name[0] == '\0')
  {
    return;
  }

  if ((url = StackGetCurrentURL(bc->cres->bmcontext->tstack)) != NULL)
  {
    BookmarkAdd(bc, name, url);
  }

  BMDAddMark(w, cldata, cbdata);

  BMWrite(bc);

  return;
}

/*
 * BMAddMark
 */
static void
BMAddMark(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  BookmarkContext bc = (BookmarkContext)cldata;
  char *title;
  ChimeraRender wn;

  if (bc->ampopped) return;

  if (bc->cres->bmcontext == NULL) return;

  wn = StackToRender(bc->cres->bmcontext->tstack);
  if ((title = RenderQuery(wn, "title")) == NULL)
  {
    if ((title = StackGetCurrentURL(bc->cres->bmcontext->tstack)) == NULL)
    {
      title = "unknown";
    }
  }
  if (bc->ampop == NULL)
  {
    bc->ampop = CreateDialog(bc->bw, "ampop",
                             BMOAddMark, BMDAddMark, BMOAddMark, bc);
  }
  MyDialogSetValue(GetDialogWidget(bc->ampop), title);

  XtPopup(bc->ampop, XtGrabNone);
  bc->ampopped = true;

  return;
}

/*
 * BMRMMark
 */
static void
BMRMMark(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  BookmarkContext bc = (BookmarkContext)cldata;
  BMark *m;
  BGroup *g;

  if ((g = BMFindGroup(bc)) == NULL) return;

  if ((m = BMFindMark(bc)) != NULL)
  {
    GListRemoveItem(g->mlist, m);
    BMChangeMarkList(bc);
    
    BMWrite(bc);
  }

  return;
}

/*
 * BMDismiss
 */
static void
BMDismiss(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  BookmarkContext bc = (BookmarkContext)cldata;

  if (bc->ampop != NULL) XtPopdown(bc->ampop);
  if (bc->agpop != NULL) XtPopdown(bc->agpop);
  XtUnmapWidget(bc->bw);

  return;
}

/*
 * BMGroupList
 */
static void
BMGroupList(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  BookmarkContext bc = (BookmarkContext)cldata;

  BMChangeMarkList(bc);
 
  return;
}

/*
 * BMMarkList
 */
static void
BMMarkList(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  BookmarkContext bc = (BookmarkContext)cldata;
  BMark *m;

  if ((m = BMFindMark(bc)) == NULL) return;

  StackOpen(bc->cres->bmcontext->tstack,
	    RequestCreate(bc->cres, m->url, NULL));

  return;
}

/*
 * BookmarkCreateContext
 */
BookmarkContext
BookmarkCreateContext(cres)
ChimeraResources cres;
{
  Widget fw, w, gvw, mvw;
  BookmarkContext bc;
  struct stat s;
  FILE *fp;
  char *bdata;
  char *filename;
  MemPool mp;
  Window rw, cw;
  int rx, ry, wx, wy;
  unsigned int mask;

  if ((filename = ResourceGetString(cres, "bookmark.filename")) == NULL)
  {
    return(NULL);
  }
  
  mp = MPCreate();
  bc = (BookmarkContext)MPCGet(mp, sizeof(struct BookmarkContextP));
  bc->mp = mp;
  bc->cres = cres;

  XQueryPointer(cres->dpy, DefaultRootWindow(cres->dpy),
                &rw, &cw,
                &rx, &ry,
                &wx, &wy,
                &mask);
  bc->bw = XtVaAppCreateShell("bookmark", "Bookmark",
			      transientShellWidgetClass, cres->dpy,
			      XtNx, rx, XtNy, ry,
			      NULL);

  /* group widgets */
  fw = XtVaCreateManagedWidget("form",
			       formWidgetClass, bc->bw,
                               NULL);
  gvw = XtVaCreateManagedWidget("groupview",
				viewportWidgetClass, fw,
				NULL);
  bc->glw = XtVaCreateManagedWidget("grouplist",
				    listWidgetClass, gvw,
				    NULL);
  XtAddCallback(bc->glw, XtNcallback, BMGroupList, (XtPointer)bc);
  w = XtVaCreateManagedWidget("addgroup",
			      commandWidgetClass, fw,
			      XtNfromVert, gvw,
			      NULL);
  XtAddCallback(w, XtNcallback, BMAddGroup, (XtPointer)bc);
  w = XtVaCreateManagedWidget("rmgroup",
			      commandWidgetClass, fw,
			      XtNfromVert, gvw,
			      XtNfromHoriz, w,
			      NULL);
  XtAddCallback(w, XtNcallback, BMRMGroup, (XtPointer)bc);
  
  /* Mark widgets */
  mvw = XtVaCreateManagedWidget("markview",
				viewportWidgetClass, fw,
				XtNfromVert, w,
				NULL);
  bc->mlw = XtVaCreateManagedWidget("marklist",
				    listWidgetClass, mvw,
				    NULL);
  XtAddCallback(bc->mlw, XtNcallback, BMMarkList, (XtPointer)bc);
  w = XtVaCreateManagedWidget("addmark",
			      commandWidgetClass, fw,
			      XtNfromVert, mvw,
			      NULL);
  XtAddCallback(w, XtNcallback, BMAddMark, (XtPointer)bc);
  w = XtVaCreateManagedWidget("rmmark",
			      commandWidgetClass, fw,
			      XtNfromVert, mvw,
			      XtNfromHoriz, w,
			      NULL);
  XtAddCallback(w, XtNcallback, BMRMMark, (XtPointer)bc);
  w = XtVaCreateManagedWidget("dismiss",
			      commandWidgetClass, fw,
			      XtNfromVert, mvw,
			      XtNfromHoriz, w,
			      NULL);
  XtAddCallback(w, XtNcallback, BMDismiss, (XtPointer)bc);

  if ((bc->header = ResourceGetString(cres, "bookmark.header")) == NULL)
  {
    bc->header = "";
  }
  if ((bc->footer = ResourceGetString(cres, "bookmark.footer")) == NULL)
  {
    bc->footer = "";
  }

  bc->glist = GListCreateX(bc->mp);
  bc->nel = GListCreateX(bc->mp);

  bc->filename = FixPath(bc->mp, filename);
  if (stat(bc->filename, &s) == 0)
  {
    if ((fp = fopen(bc->filename, "r")) != NULL)
    {
      bdata = (char *)alloc_mem(s.st_size);
      if (fread(bdata, 1, s.st_size, fp) == s.st_size)
      {
	bc->ml = MLInit(BMElementHandler, bc);
	MLEndData(bc->ml, bdata, s.st_size);
	MLDestroy(bc->ml);
      }
      free_mem(bdata);
      fclose(fp);
    }
  }

  if (GListEmpty(bc->glist)) GroupCreate(bc, "default", true);

  XtSetMappedWhenManaged(bc->bw, False);
  XtRealizeWidget(bc->bw);

  return(bc);
}
