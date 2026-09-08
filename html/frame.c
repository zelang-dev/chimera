/*
 * frame.c
 *
 * libhtml - HTML->X renderer
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

#include "port_after.h"

#include "html.h"

#include "ChimeraStack.h"

#define LASTRESORTSIZE 100

typedef struct HTMLFrameInfoP *HTMLFrameInfo;
typedef struct HTMLFrameSetInfoP *HTMLFrameSetInfo;
typedef struct HTMLFrameSizeP *HTMLFrameSize;

struct HTMLFrameSizeP
{
  unsigned int size;
};

struct HTMLFrameInfoP
{
  HTMLInfo        li;
  HTMLBox         box;
  ChimeraStack    cs;
  ChimeraRequest  *wr;
  int             x, y;
  unsigned int    width, height;
  char            *name;
  HTMLFrameSetInfo fset;
};

struct HTMLFrameSetInfoP
{
  HTMLInfo        li;
  bool            horiz;
  GList           sizes;
  GList           frames;
  unsigned int    other_size;
  HTMLFrameSize   current_size;
  int             x, y;
};

static void SetupIFrame _ArgProto((HTMLInfo, HTMLBox));
static void DestroyIFrame _ArgProto((HTMLInfo, HTMLBox));
static GList GetSizes _ArgProto((HTMLInfo, char *, unsigned int));
/* not used
static void FrameRenderAction _ArgProto((void *, ChimeraRender,
					 ChimeraRequest *, char *));
static HTMLFrameInfo FindNamedFrame _ArgProto((HTMLInfo, char *));
*/

/*
 *
 * <FRAMESET><FRAME><FRAMESET> ignores all the usual layout methods and
 * blasts the frames right onto the window.  No point in messing
 * around with that stuff.  The old-style frames are evil.
 *
 */

/*
 * SetupIFrame
 */
static void
SetupIFrame(li, box)
HTMLInfo li;
HTMLBox box;
{
  HTMLFrameInfo fi = (HTMLFrameInfo)box->closure;

  fi->cs = StackCreate(StackFromGUI(li->wc, li->wd),
			 box->x, box->y, box->width, box->height,
			 NULL, NULL);

  StackOpen(fi->cs, fi->wr);

  return;
}

/*
 * DestroyIFrame
 */
static void
DestroyIFrame(li, box)
HTMLInfo li;
HTMLBox box;
{
  HTMLFrameInfo fi = (HTMLFrameInfo)box->closure;

  if (fi->cs != NULL) StackDestroy(fi->cs);

  return;
}

/*
 * HTMLIFrame
 */
void
HTMLIFrame(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLBox box;
  HTMLAttribID aid;
  HTMLFrameInfo fi;
  int width, height;
  char *url;
  ChimeraRequest *wr;

  if ((url = MLFindAttribute(p, "src")) == NULL) return;
  if ((wr = RequestCreate(li->cres, url, NULL)) == NULL) return;

  fi = (HTMLFrameInfo)MPCGet(li->mp, sizeof(struct HTMLFrameInfoP));

  if ((width = MLAttributeToInt(p, "width")) < 0) width = 500;
  if ((height = MLAttributeToInt(p, "height")) < 0) height = 500;

  box = HTMLCreateBox(li, env);
  
  aid = HTMLAttributeToID(p, "align");
  if (aid == ATTRIB_MIDDLE) box->baseline = height / 2;
  else if (aid == ATTRIB_TOP) box->baseline = 0;
  else if (aid == ATTRIB_LEFT) HTMLSetB(box, BOX_FLOAT_LEFT);
  else if (aid == ATTRIB_RIGHT) HTMLSetB(box, BOX_FLOAT_RIGHT);
  else box->baseline = height;

  box->setup = SetupIFrame;
  box->destroy = DestroyIFrame;
  box->width = width;
  box->height = height;
  box->closure = fi;

  fi->box = box;
  fi->li = li;
  fi->wr = wr;

  HTMLEnvAddBox(li, env, box);

  return;
}

/*
 * HTMLFrameInsert
 */
HTMLInsertStatus
HTMLFrameInsert(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  if (HTMLTagToID(env->tag) != TAG_FRAMESET) return(HTMLInsertReject);
  return(HTMLInsertOK);
}

/*
 * HTMLFrame
 */
void
HTMLFrame(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLFrameInfo fi;
  char *url;
  ChimeraRequest *wr;
  HTMLFrameSetInfo fset;
  unsigned int size;

  fset = (HTMLFrameSetInfo)env->closure;

  if ((url = MLFindAttribute(p, "src")) == NULL) return;

  if ((wr = RequestCreate(li->cres, url, li->burl)) == NULL) return;

  fi = (HTMLFrameInfo)MPCGet(li->mp, sizeof(struct HTMLFrameInfoP));

  fi->li = li;
  fi->wr = wr;

  fi->box = HTMLCreateBox(li, env);
  fi->box->destroy = DestroyIFrame;
  fi->box->closure = fi;
  fi->x = fset->x;
  fi->y = fset->y;
  fi->name = MLFindAttribute(p, "name");
  fi->fset = fset;

  if (fset->current_size == NULL) size = LASTRESORTSIZE;
  else size = fset->current_size->size;

  if (fset->horiz)
  {
    fi->width = size;
    fi->height = fset->other_size;
    fset->x += size;
  }
  else
  {
    fi->width = fset->other_size;
    fi->height = size;
    fset->y += size;
  }

  GListAddHead(fset->frames, fi);

  fset->current_size = (HTMLFrameSize)GListGetNext(fset->sizes);

  return;
}

/*
 * HTMLFrameSetAccept
 */
bool
HTMLFrameSetAccept(li, obj)
HTMLInfo li;
HTMLObject obj;
{
  HTMLTag tag;

  if (obj->type == HTML_ENV && HTMLTagToID(obj->o.env->tag) != TAG_FRAMESET)
  {
    return(true);
  }
  if (obj->type == HTML_TAG)
  {
    if ((tag = HTMLGetTag(obj->o.p)) != NULL && HTMLTagToID(tag) == TAG_FRAME)
    {
      return(true);
    }
  }
  return(true);
}

/*
 * HTMLFrameSetInsert
 */
HTMLInsertStatus
HTMLFrameSetInsert(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLEnv c;
  HTMLTagID tid;

  /*
   * Quick and dirty way to kill off frames from another location.
   */
  if (li->noframeset) return(HTMLInsertReject);
  li->framesonly = true;

  for (c = (HTMLEnv)GListGetHead(li->envstack); c != NULL;
       c = (HTMLEnv)GListGetNext(li->envstack))
  {
    tid = HTMLTagToID(c->tag);
    if (tid != TAG_FRAMESET && tid != TAG_HTML && tid != TAG_DOCUMENT)
    {
      return(HTMLInsertReject);
    }
  }

  return(HTMLInsertOK);
}

/*
 * GetSizes
 */
static GList
GetSizes(li, s, len)
HTMLInfo li;
char *s;
unsigned int len;
{
  char *cp;
  GList list;
  HTMLFrameSize size;
  int x;
  int totalpc;

  list = GListCreateX(li->mp);
  size = MPGet(li->mp, sizeof(struct HTMLFrameSizeP));

  totalpc = x = atoi(s);
  if (x <= 0) size->size = LASTRESORTSIZE;
  else size->size = len * x / 100;
  GListAddHead(list, size);

  for (cp = s; *cp != '\0'; cp++)
  {
    if (*cp == ',')
    {
      size = MPGet(li->mp, sizeof(struct HTMLFrameSizeP));
      if (*(cp+1) == '*') x = 100 - totalpc;
      else x = atoi(cp + 1);
      x = atoi(cp + 1);
      if (x <= 0) x = 10;
      size->size = len * x / 100;
      GListAddTail(list, size);
    }
  }

  return(list);
}

/*
 * HTMLFrameSetBegin
 */
void
HTMLFrameSetBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLFrameSetInfo pfset, fset;
  char *sizestr;
  unsigned int width, height;

  GUISetScrollBar(li->wd, false);
  GUIGetDimensions(li->wd, &width, &height);

  fset = (HTMLFrameSetInfo)MPCGet(li->mp, sizeof(struct HTMLFrameSetInfoP));
  env->closure = fset;
  fset->frames = GListCreateX(li->mp);

  if ((sizestr = MLFindAttribute(p, "rows")) != NULL)
  {
    fset->horiz = false;
    fset->sizes = GetSizes(li, sizestr, height);
  }
  else if ((sizestr = MLFindAttribute(p, "cols")) != NULL)
  {
    fset->horiz = true;
    fset->sizes = GetSizes(li, sizestr, width);
  }
  else
  {
    fset->horiz = false;
    fset->sizes = GListCreateX(li->mp);
  }

  if (HTMLTagToID(env->penv->tag) == TAG_FRAMESET)
  {
    /*
     * Frameset inside a frameset so look at the parent to see if there
     * is a size given.
     */
    pfset = (HTMLFrameSetInfo)env->penv->closure;
    if (pfset->current_size == NULL) fset->other_size = LASTRESORTSIZE;
    else
    {
      fset->other_size = pfset->current_size->size;
      pfset->current_size = (HTMLFrameSize)GListGetNext(pfset->sizes);
      fset->x = pfset->x;
      fset->y = pfset->y;
      if (pfset->horiz) pfset->x += fset->other_size;
      else pfset->y += fset->other_size;
    }
  }
  else
  {
    /*
     * Must be first frameset
     */
    if (fset->horiz) fset->other_size = width;
    else fset->other_size = height;
  }

  fset->current_size = (HTMLFrameSize)GListGetHead(fset->sizes);

  GListAddHead(li->framesets, fset);

  return;
}

/*
 * HTMLFrameSetEnd
 */
void
HTMLFrameSetEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLFrameInfo fi;
  HTMLFrameSetInfo fset = (HTMLFrameSetInfo)env->closure;

  for (fi = (HTMLFrameInfo)GListGetHead(fset->frames); fi != NULL;
       fi = (HTMLFrameInfo)GListGetNext(fset->frames))
  {
    fi->cs = StackCreate(StackFromGUI(li->wc, li->wd),
			   fi->x, fi->y, fi->width, fi->height,
			   NULL, NULL);
    StackOpen(fi->cs, fi->wr);
  }

  return;
}

/*
 * HTMLNoFramesBegin
 */
void
HTMLNoFramesBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  return;
}

/*
 * HTMLNoFramesEnd
 */
void
HTMLNoFramesEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  return;
}

/*
 * HTMLDestroyFrameSets
 */
void
HTMLDestroyFrameSets(li)
HTMLInfo li;
{
  HTMLFrameInfo fi;
  HTMLFrameSetInfo fset;

  for (fset = (HTMLFrameSetInfo)GListGetHead(li->framesets); fset != NULL;
       fset = (HTMLFrameSetInfo)GListGetNext(li->framesets))
  {
    for (fi = (HTMLFrameInfo)GListGetHead(fset->frames); fi != NULL;
	 fi = (HTMLFrameInfo)GListGetNext(fset->frames))
    {
      StackDestroy(fi->cs);
    }
  }

  return;
}

/*
 * FindNamedFrame
 */
/* not used
static HTMLFrameInfo
FindNamedFrame(li, name)
HTMLInfo li;
char *name;
{
  HTMLFrameSetInfo fset;
  HTMLFrameInfo fi;

  for (fset = (HTMLFrameSetInfo)GListGetHead(li->framesets); fset != NULL;
       fset = (HTMLFrameSetInfo)GListGetNext(li->framesets))
  {
    for (fi = (HTMLFrameInfo)GListGetHead(fset->frames); fi != NULL;
	 fi = (HTMLFrameInfo)GListGetNext(fset->frames))
    {
      if (fi->name != NULL &&
	  strlen(fi->name) == strlen(name) &&
	  strcasecmp(fi->name, name) == 0)
      {
	return(fi);
      }
    }
  }

  return(NULL);
}
*/

/*
 * HTMLFrameLoad
 *
 * This could fail in silly ways if there are multiple frames with the
 * same name.  Worry about this later.
 */
void
HTMLFrameLoad(li, wr, target)
HTMLInfo li;
ChimeraRequest *wr;
char *target;
{
  HTMLInfo hi;
  HTMLFrameSetInfo fset;
  HTMLFrameInfo fi;

  for (hi = (HTMLInfo)GListGetHead(li->lc->contexts); hi != NULL;
       hi = (HTMLInfo)GListGetNext(li->lc->contexts))
  {
    for (fset = (HTMLFrameSetInfo)GListGetHead(hi->framesets); fset != NULL;
	 fset = (HTMLFrameSetInfo)GListGetNext(hi->framesets))
    {
      for (fi = (HTMLFrameInfo)GListGetHead(fset->frames); fi != NULL;
	   fi = (HTMLFrameInfo)GListGetNext(fset->frames))
      {
	if (fi->name != NULL &&
	    strlen(fi->name) == strlen(target) &&
	    strcasecmp(fi->name, target) == 0)
	{
	  StackOpen(fi->cs, wr);
	}
      }
    }
  }

  return;
}
