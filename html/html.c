/*
 * html.c
 *
 * libhtml - HTML->X renderer
 *
 * Copyright (c) 1994-1997, John Kilburg <john@cs.unlv.edu>
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

static void EndTag _ArgProto((HTMLInfo, HTMLTag, MLElement));
static void StartTag _ArgProto((HTMLInfo, HTMLTag, MLElement));
static void AddObject _ArgProto((HTMLInfo, HTMLObjectType, void *));
static void DoEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
static bool BoxifyObject _ArgProto((HTMLInfo, HTMLEnv, HTMLObject));
static bool Boxify _ArgProto((HTMLInfo, HTMLEnv));

static void HTMLDocumentBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
static void HTMLDocumentEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
static void HTMLDocumentAddBox _ArgProto((HTMLInfo, HTMLEnv, HTMLBox));
static unsigned int HTMLDocumentWidth _ArgProto((HTMLInfo, HTMLEnv));
static void Unboxify _ArgProto((HTMLInfo, HTMLEnv));
static void PrintObject _ArgProto((HTMLObject));

#include "htmltags.h"

typedef struct
{
  HTMLTag tag;
  MLElement p;
} PendingEnd;

struct av
{
  char *name;
  HTMLAttribID align;
};

struct av alist[] =
{
  { "left", ATTRIB_LEFT },
  { "right", ATTRIB_RIGHT },
  { "center", ATTRIB_CENTER },
  { "top", ATTRIB_TOP },
  { "bottom", ATTRIB_BOTTOM },
  { "middle", ATTRIB_MIDDLE },
  { "all", ATTRIB_ALL },
  { NULL, ATTRIB_UNKNOWN }
};

static int alist_len = sizeof(alist) / sizeof(alist[0]);

/*
 * HTMLAttributeToID
 */
HTMLAttribID
HTMLAttributeToID(p, name)
MLElement p;
char *name;
{
  char *value;
  int i;

  if ((value = MLFindAttribute(p, name)) == NULL) return(ATTRIB_UNKNOWN);
  for (i = 0; i < alist_len; i++)
  {
    if (alist[i].name != NULL && strcasecmp(value, alist[i].name) == 0)
    {
      return(alist[i].align);
    }
  }

  return(ATTRIB_UNKNOWN);
}

/*
 * HTMLGetTag
 */
HTMLTag
HTMLGetTag(p)
MLElement p;
{
  int i;
  char *name;

  if ((name = MLTagName(p)) == NULL) return(NULL);

  for (i = 0; i < tlist_len; i++)
  {
    if (tlist[i].name != NULL &&
	strlen(name) == strlen(tlist[i].name) &&
	strcasecmp(name, tlist[i].name) == 0 &&
	!HTMLTestM(&tlist[i], MARKUP_FAKE))
    {
      return(&tlist[i]);
    }
  }

  return(NULL);
}

/*
 * HTMLTagIDToTag
 */
HTMLTag
HTMLTagIDToTag(tagid)
HTMLTagID tagid;
{
  int i;

  for (i = 0; i < tlist_len; i++)
  {
    if (tlist[i].id == tagid) return(&tlist[i]);
  }

  return(NULL);
}

/*
 * HTMLFindEnv
 */
HTMLEnv
HTMLFindEnv(li, tagid)
HTMLInfo li;
HTMLTagID tagid;
{
  HTMLEnv env;
  GList list;

  list = li->envstack;
  for (env = (HTMLEnv)GListGetHead(list); env != NULL; 
       env = (HTMLEnv)GListGetNext(list))
  {
    if (env->tag->id == tagid) return(env);
  }

  return(NULL);
}

/*
 * HTMLDelayLayout
 */
void
HTMLDelayLayout(li)
HTMLInfo li;
{
  li->delayed++;
  return;
}

/*
 * HTMLContinueLayout
 */
void
HTMLContinueLayout(li)
HTMLInfo li;
{
  myassert(li->delayed > 0, "Layout was not delayed.");

  li->delayed--;

  if (li->delayed == 0) Boxify(li, li->topenv);

  return;
}

/*
 * HTMLTagToID
 */
HTMLTagID
HTMLTagToID(tag)
HTMLTag tag;
{
  return(tag->id);
}

/*
 * HTMLPopEnv
 */
HTMLEnv
HTMLPopEnv(li, tagid)
HTMLInfo li;
HTMLTagID tagid;
{
  HTMLEnv c;

  /* if (tagid == TAG_DOCUMENT) abort(); */

  for (c = (HTMLEnv)GListGetHead(li->envstack); c != NULL;
       c = (HTMLEnv)GListGetNext(li->envstack))
  {
    if (c->tag->id == tagid) break;
  }

  if (c == NULL) return(NULL);

  while ((c = (HTMLEnv)GListGetHead(li->envstack)) != NULL)
  {
    if (c->tag->id == tagid) break;
    DoEnd(li, c, NULL);
  }

  return(c);
}

/*
 * HTMLStartEnv
 */
void
HTMLStartEnv(li, tagid, p)
HTMLInfo li;
HTMLTagID tagid;
MLElement p;
{
  char *str;
  HTMLTag tag;

  tag = HTMLTagIDToTag(tagid);
  if (p == NULL)
  {
    str = MPGet(li->mp, strlen(tag->name) + 3);
    strcpy(str, "<");
    strcat(str, tag->name);
    strcat(str, ">");
    p = MLCreateTag(li->hs, str, strlen(str));
  }
  StartTag(li, tag, p);
  return;
}

/*
 * HTMLEndEnv
 */
void
HTMLEndEnv(li, tagid)
HTMLInfo li;
HTMLTagID tagid;
{
  EndTag(li, HTMLTagIDToTag(tagid), NULL);
  return;
}

/*
 * HTMLGetMaxWidth
 */
unsigned int
HTMLGetMaxWidth(li, env)
HTMLInfo li;
HTMLEnv env;
{
  HTMLEnv c;

  for (c = env; c != NULL; c = c->penv)
  {
    if (c->tag->w != NULL) return((c->tag->w)(li, c));
  }

  return(0);
}

/*
 * PrintObject
 */
static void
PrintObject(ho)
HTMLObject ho;
{
  char *text;
  size_t len;
  MLElementType mt;

  if (ho->type == HTML_ELEMENT)
  {
    MLGetText(ho->o.p, &text, &len);
    fwrite (text, 1, len, stdout);
    printf ("\n");
  }
  else if (ho->type == HTML_TAG || ho->type == HTML_BEGINTAG ||
	   ho->type == HTML_ENDTAG)
  {
    mt = MLGetType(ho->o.p);
    if (mt == ML_ENDTAG) printf ("End tag: %s\n", MLTagName(ho->o.p));
    else printf ("Begin tag: %s\n", MLTagName(ho->o.p));
  }

  return;
}

/*
 * AddObject
 */
static void
AddObject(li, hot, obj)
HTMLInfo li;
HTMLObjectType hot;
void *obj;
{
  HTMLEnv env;
  HTMLObject ho;

  ho = (HTMLObject)MPGet(li->mp, sizeof(struct HTMLObjectP));
  ho->type = hot;
  if (hot == HTML_ENV) ho->o.env = (HTMLEnv)obj;
  else if (hot == HTML_ELEMENT) ho->o.p = (MLElement)obj;
  else if (hot == HTML_TAG) ho->o.p = (MLElement)obj;
  else if (hot == HTML_BEGINTAG) ho->o.p = (MLElement)obj;
  else if (hot == HTML_ENDTAG) ho->o.p = (MLElement)obj;
  else abort();

  if (li->printTags) PrintObject(ho);

  if (hot != HTML_BEGINTAG && hot != HTML_ENDTAG)
  {
    for (env = (HTMLEnv)GListGetHead(li->envstack); env != NULL;
	 env = (HTMLEnv)GListGetNext(li->envstack))
    {
      if (env->tag->m == NULL || (env->tag->m)(li, ho)) break;
    }
    if (env == NULL) return;
  }
  else env = (HTMLEnv)GListGetHead(li->envstack);

  if (hot == HTML_ENV) ho->o.env->penv = env;

  /*
   * Add object to the object list for the environment.  The first list
   * will be modified later.  The second list will always stay the
   * same so there is always a place to find all the objects in an
   * environment.
   */
  GListAddTail(env->olist, ho);
  GListAddTail(env->slist, ho);

  return;
}

/*
 * DoEnd
 */
static void
DoEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  char *name;
  char *str;

  if (p == NULL)
  {
    if (env->tag->name == NULL) name = "internal";
    else name = env->tag->name;
    str = (char *)MPGet(li->mp, strlen(name) + 4);
    strcpy(str, "</");
    strcat(str, name);
    strcat(str, ">");
    p = MLCreateTag(li->hs, str, strlen(str));
  }

  AddObject(li, HTML_ENDTAG, p);

  env = (HTMLEnv)GListPop(li->envstack);

  AddObject(li, HTML_ENV, env);
  
  return;
}

/*
 * EndTag
 */
static void
EndTag(li, tag, p)
HTMLInfo li;
HTMLTag tag;
MLElement p;
{
  HTMLEnv etop;
  PendingEnd *pe;
  HTMLEnv env;

  /*
   * If there was no start tag then ignore.
   */
  if (HTMLFindEnv(li, tag->id) == NULL) return;

  /*
   * Check to see if the end tag is supposed to clamp down on all
   * unterminated environments.
   */
  if (HTMLTestM(tag, MARKUP_CLAMP))
  {
    if (tag->c != NULL)
    {
      if ((tag->c)(li, (HTMLEnv)GListGetHead(li->envstack)))
      {
	while ((env = (HTMLEnv)GListGetHead(li->envstack)) != NULL)
	{
	  DoEnd(li, env, NULL);
	  if (env->tag->id == tag->id) break;
	}
      }
    }
    else
    {
      while ((env = (HTMLEnv)GListGetHead(li->envstack)) != NULL)
      {
	DoEnd(li, env, NULL);
	if (env->tag->id == tag->id) break;
      }
    }
    if ((pe = (PendingEnd *)GListPop(li->endstack)) != NULL)
    {
      EndTag(li, pe->tag, pe->p);
      return;
    }
  }
  else
  {
    /*
     * Make sure the end tag matches the current environment before
     * terminating the environment.  If it doesn't match then stick it
     * in a list for possible use later.
     */
    etop = (HTMLEnv)GListGetHead(li->envstack);
    if (tag->id == etop->tag->id)
    {
      DoEnd(li, etop, p);

      if ((pe = (PendingEnd *)GListPop(li->endstack)) != NULL)
      {
	EndTag(li, pe->tag, pe->p);
	return;
      }
    }
    else
    {
      pe = MPGet(li->mp, sizeof(PendingEnd));
      pe->tag = tag;
      pe->p = p;
      GListAddTail(li->endstack, pe);
    }
  }

  return;
}

/*
 * StartTag
 */
static void
StartTag(li, tag, p)
HTMLInfo li;
HTMLTag tag;
MLElement p;
{
  HTMLEnv env;
  HTMLEnv etop;
  HTMLInsertStatus status;

  if ((etop = (HTMLEnv)GListGetHead(li->envstack)) != NULL)
  {
    if (tag->p != NULL)
    {
      if ((status = (tag->p)(li, etop, p)) == HTMLInsertReject) return;
    }
    else status = HTMLInsertOK;
  }
  else status = HTMLInsertOK;

  if (!HTMLTestM(tag, MARKUP_EMPTY) || status == HTMLInsertEmpty)
  {
    env = MPCGet(li->mp, sizeof(struct HTMLEnvP));
    env->tag = tag;
    env->olist = GListCreateX(li->mp);
    env->blist = GListCreateX(li->mp);
    env->slist = GListCreateX(li->mp);

    if (etop == NULL) li->topenv = env;

    GListAddHead(li->envstack, env);

    AddObject(li, HTML_BEGINTAG, p);
  }
  else AddObject(li, HTML_TAG, p);
  
  return;
}

/*
 * HTMLHandler
 *
 * This is where elements from the ML scanner come into the HTML parser.
 */
void
HTMLHandler(closure, p)
void *closure;
MLElement p;
{
  HTMLInfo li = (HTMLInfo)closure;
  HTMLTag tag;
  MLElementType mt;
  
  if ((mt = MLGetType(p)) == ML_EOF)
  {
    /*
     * fake a </xxx-document> tag.
     */
    HTMLFinish(li);

    /*
     * Render the parsed HTML
     */
    Boxify(li, li->topenv);
    return;
  }
  else if (mt == ML_DATA) AddObject(li, HTML_ELEMENT, p);
  else if ((tag = HTMLGetTag(p)) != NULL)
  {
    if (mt == ML_ENDTAG) EndTag(li, tag, p);
    else StartTag(li, tag, p);
  }
  else if (li->printTags)
  {
    printf ("Unknown tag: %s\n", MLTagName(p));
  }
  
  return;
}

/*
 * BoxifyObject
 */
static bool
BoxifyObject(li, env, obj)
HTMLInfo li;
HTMLEnv env;
HTMLObject obj;
{
  HTMLTag tag;
  HTMLEnv cenv;
  CSSSelector cs;

  if (obj->type == HTML_ELEMENT)
  {
    if (env->tag->d != NULL) (env->tag->d)(li, env, obj->o.p);
  }
  else if (obj->type == HTML_TAG)
  {
    if ((tag = HTMLGetTag(obj->o.p)) != NULL)
    {
      myassert(tag->b != NULL, "No tag handler for lone tag!");
      (tag->b)(li, env, obj->o.p);
    }
  }
  else if (obj->type == HTML_ENV)
  {
    cenv = obj->o.env;
    if (!cenv->visited)
    {
      cenv->ff = env->ff;
      cenv->anchor = env->anchor;
      cenv->fi = HTMLDupFont(li, env->fi);
      
      if (HTMLTestM(cenv->tag, MARKUP_SPACER))
      {
	HTMLAddBlankLine(li, env);
      }
      cenv->visited = true;
    }

    if ((cs = (CSSSelector)GListPop(li->oldselectors)) == NULL)
    {
      cs = CSSCreateSelector(li->mp);
    }
    CSSSetSelector(cs, cenv->tag->name, NULL, NULL, NULL);
    GListAddHead(li->selectors, cs);

    if (Boxify(li, cenv)) return(true);

    GListAddHead(li->oldselectors, GListPop(li->selectors));

    if (HTMLTestM(cenv->tag, MARKUP_SPACER))
    {
      HTMLAddBlankLine(li, env);
    }
  }
  else if (obj->type == HTML_BEGINTAG)
  {
    if (env->tag->b != NULL) (env->tag->b)(li, env, obj->o.p);
  }
  else if (obj->type == HTML_ENDTAG)
  {
    if (env->tag->e != NULL) (env->tag->e)(li, env, obj->o.p);
  }
  else abort();

  if (li->delayed > 0)
  {
    return(true);
  }

  return(false);
}

/*
 * Unboxify
 */
static void
Unboxify(li, env)
HTMLInfo li;
HTMLEnv env;
{
  HTMLObject c;
  GList t;

  env->visited = false;

  while ((c = (HTMLObject)GListPop(env->olist)) != NULL)
  {
    GListAddTail(env->blist, c);
  }

  t = env->olist;
  env->olist = env->blist;
  env->blist = t;

  for (c = (HTMLObject)GListGetHead(env->olist); c != NULL;
       c = (HTMLObject)GListGetNext(env->olist))
  {
    if (c->type == HTML_ENV) Unboxify(li, c->o.env);
  }

  return;
}

/*
 * Boxify
 */
bool
Boxify(li, env)
HTMLInfo li;
HTMLEnv env;
{
  HTMLObject c;

  myassert(li->delayed == 0, "yikes");

  while ((c = (HTMLObject)GListPop(env->olist)) != NULL)
  {
    if (BoxifyObject(li, env, c))
    {
      if (c->type == HTML_ENV) GListAddHead(env->olist, c);
      else GListAddTail(env->blist, c);
      return(true);
    }
    else GListAddTail(env->blist, c);
  }

  if (HTMLTestM(env->tag, MARKUP_TWOPASS) && env->pass == 0)
  {
    env->pass++;
    Unboxify(li, env);
    Boxify(li, env);
  }

  return(false);
}

/*
 * HTMLEnvAddBox
 */
void
HTMLEnvAddBox(li, env, box)
HTMLInfo li;
HTMLEnv env;
HTMLBox box;
{
  HTMLEnv c;

  for (c = env; c != NULL; c = c->penv)
  {
    if (c->tag->a != NULL)
    {
      (c->tag->a)(li, c, box);
      break;
    }
  }

  return;
}

/*
 * HTMLDocumentEnd
 */
static void
HTMLDocumentEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLFinishFlowBox(li, li->firstbox);
  return;
}

/*
 * HTMLDocumentBegin
 */
static void
HTMLDocumentBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  return;
}

/*
 * HTMLDocumentWidth
 */
static unsigned int
HTMLDocumentWidth(li, env)
HTMLInfo li;
HTMLEnv env;
{
  return(HTMLGetBoxWidth(li, li->firstbox));
}

/*
 * HTMLDocumentAddBox
 */
static void
HTMLDocumentAddBox(li, env, box)
HTMLInfo li;
HTMLEnv env;
HTMLBox box;
{
  HTMLLayoutBox(li, li->firstbox, box);
  return;
}

/*
 * HTMLFinish
 */
void
HTMLFinish(li)
HTMLInfo li;
{
  HTMLPopEnv(li, TAG_DOCUMENT);
  HTMLEndEnv(li, TAG_DOCUMENT);

  return;
}

/*
 * HTMLStart
 */
void
HTMLStart(li)
HTMLInfo li;
{
  HTMLStartEnv(li, TAG_DOCUMENT,
	       MLCreateTag(li->hs,
			   "<xxx-document>", strlen("<xxx-document>")));

  li->topenv->ff = FLOW_LEFT_JUSTIFY;
  li->topenv->anchor = NULL;
  li->topenv->fi = HTMLDupFont(li, li->cfi);

  li->firstbox = HTMLCreateFlowBox(li, li->topenv,
                                   li->maxwidth - (li->lmargin + li->rmargin));
  li->firstbox->x = li->lmargin;
  li->firstbox->y = li->rmargin;
  HTMLSetB(li->firstbox, BOX_TOPLEVEL);

  return;
}

/*
 * HTMLGetIDEnv
 */
HTMLEnv
HTMLGetIDEnv(env, tid)
HTMLEnv env;
HTMLTagID tid;
{
  HTMLEnv penv;
  HTMLTagID ptid;

  penv = env;
  while ((ptid = HTMLTagToID(penv->tag)) != TAG_DOCUMENT)
  {
    if (ptid == tid) return(penv);
    penv = penv->penv;
  }

  return(NULL);
}
