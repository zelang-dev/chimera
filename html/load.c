/*
 * load.c
 *
 * libhtml - HTML->X renderer 
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

#include "port_after.h"

#include "html.h"

#include "ChimeraStack.h"

int
HTMLLoadURL(li, frame, url, action)
HTMLInfo li;
char *frame;
char *url;
char *action;
{
  ChimeraRequest *wr;

  if (url == NULL) return(-1);

  if (url[0] == '#')
  {
    HTMLFindName(li, url + 1);
    return(0);
  }
  
  wr = RequestCreate(li->cres, url, li->burl);
  RenderAction(li->wn, wr, action);

  return(0);
}

int
HTMLLoadAnchor(li, anchor, x, y, action, ismap)
HTMLInfo li;
HTMLAnchor anchor;
int x, y;
char *action;
bool ismap;
{
  char *aurl, *url;
  char *target;
  MemPool mp;
  ChimeraRequest *wr;
  ChimeraStack cs;

  if ((url = MLFindAttribute(anchor->p, "href")) == NULL) return(-1);

  /*
   * This could be trouble.  What if there is a 'target' attribute?
   * I think the solutuion is to prepend the base URL to the fragment
   * and submit it so that fragment stuff will end up on the document
   * stack.
   */
  if (url[0] == '#')
  {
    HTMLFindName(li, url + 1);
    return(0);
  }

  if (ismap)
  {
    mp = MPCreate();
    aurl = (char *)MPGet(mp, strlen(url) + 200);
    snprintf (aurl, strlen(url) + 200, "%s?%d,%d", url, x, y);
    url = aurl;
  }
  else mp = NULL;

  wr = RequestCreate(li->cres, url, li->burl);
  if (mp != NULL) MPDestroy(mp);

  if (wr == NULL) return(-1);

  if ((target = MLFindAttribute(anchor->p, "target")) != NULL)
  {
    HTMLFrameLoad(li, wr, target);
  }
  else
  {
    if ((cs = StackGetParent(StackFromGUI(li->wc, li->wd))) == NULL)
    {
      RenderAction(li->wn, wr, action);
    }
    else
    {
      StackAction(cs, wr, action);
    }
  }

  return(0);
}

void
HTMLPrintURL(li, url)
HTMLInfo li;
char *url;
{
  ChimeraRequest *wr;
  size_t len;
  char buffer[BUFSIZ + 100];
  const char *msg = "Cannot handle "; /* this is the wrong thing to do! */

  if (url == NULL) return;

  if ((wr = RequestCreate(li->cres, url, li->burl)) != NULL)
  {
    url = wr->url;
  }
  else
  {
    strcpy(buffer, msg);
    len = sizeof(buffer) - strlen(msg) - 1;
    strncat(buffer, url, len);
    buffer[sizeof(buffer) - 1] = '\0';
    url = buffer;
  }

  RenderSendMessage(li->wn, url);

  if (wr != NULL) RequestDestroy(wr);

  return;
}

void
HTMLPrintAnchor(li, anchor, x, y, ismap)
HTMLInfo li;
HTMLAnchor anchor;
int x, y;
bool ismap;
{ 
  char *url, *aurl;
  MemPool mp;

  if ((url = MLFindAttribute(anchor->p, "href")) == NULL) return;

  if (ismap)
  {
    mp = MPCreate();
    aurl = (char *)MPGet(mp, strlen(url) + 200);
    snprintf (aurl, strlen(url) + 200, "%s?%d,%d", url, x, y);
    url = aurl;
    HTMLPrintURL(li, url);
    MPDestroy(mp);
  }
  else HTMLPrintURL(li, url);


  return;
}

/*
 * HTMLAddAnchor
 */
void
HTMLAddAnchor(li, box, name, p)
HTMLInfo li;
HTMLBox box;
char *name;
MLElement p;
{
  HTMLAnchor a;

  a = (HTMLAnchor)MPCGet(li->mp, sizeof(struct HTMLAnchorP));
  a->name = name;
  a->p = p;
  a->box = box;
  GListAddHead(li->alist, a);

  return;
}

/*
 * HTMLFindName
 */
void
HTMLFindName(li, name)
HTMLInfo li;
char *name;
{
  HTMLAnchor a;
  GList list;

  list = li->alist;
  for (a = (HTMLAnchor)GListGetHead(list); a != NULL;
       a = (HTMLAnchor)GListGetNext(list))
  {
    if (a->name != NULL && strlen(name) == strlen(a->name) &&
	strcasecmp(name, a->name) == 0)
    {
      GUISetScrollPosition(li->wd, 0, -(a->box->y));
      return;
    }
  }

  return;
}
