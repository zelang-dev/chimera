/*
 * list.c
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
#include <ctype.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "port_after.h"

#include "html.h"

/*
 * Numeric label stuff for <OL> supplied by
 * Jake Kesinger <kesinger@math.ttu.edu>
 */
typedef enum
{
  OL_NUMERIC,
  OL_ALPHA_CAP,
  OL_ALPHA_MINISCULE,
  OL_ROMAN_CAP,
  OL_ROMAN_MINISCULE
} OListType;

typedef struct
{
  OListType type;                  /* type for ol list */
  int count;                       /* count for ol list */
  int bullet_diam;                 /* cached bullet size */
  GList klist;                     /* list of item boxes */
  HTMLBox lbox;                    /* list box */
  HTMLBox ibox;                    /* item box */
  HTMLBox tbox;                    /* text box inside item (dd, li) */
} HList;

/*
 *
 * Private functions
 *
 */
static void RenderLI _ArgProto((HTMLInfo, HTMLBox, Region));
static void SetupList _ArgProto((HTMLInfo, HTMLBox));
static void RenderList _ArgProto((HTMLInfo, HTMLBox, Region));
static void DestroyList _ArgProto((HTMLInfo, HTMLBox));
static HList *CreateList _ArgProto((HTMLInfo, HTMLEnv));
static void OLNext _ArgProto((char *, size_t, int, OListType));

/*
 * SetupList
 */
static void
SetupList(li, box)
HTMLInfo li;
HTMLBox box;
{
  HList *hl = (HList *)box->closure;
  HTMLBox c;
  int ty = box->y;

  for (c = (HTMLBox)GListGetHead(hl->klist); c != NULL;
       c = (HTMLBox)GListGetNext(hl->klist))
  {
    c->x = box->x;
    c->y = ty;
    HTMLSetupBox(li, c);
    ty += c->height;
  }

  return;
}

/*
 * RenderList
 */
static void
RenderList(li, box, r)
HTMLInfo li;
HTMLBox box;
Region r;
{
  HList *hl = (HList *)box->closure;
  HTMLBox c;

  for (c = (HTMLBox)GListGetHead(hl->klist); c != NULL;
       c = (HTMLBox)GListGetNext(hl->klist))
  {
    HTMLRenderBox(li, r, c);
  }

  return;
}

static void
DestroyList(li, box)
HTMLInfo li;
HTMLBox box;
{
  HList *ls = (HList *)box->closure;
  HTMLBox c;

  for (c = (HTMLBox)GListGetHead(ls->klist); c != NULL;
       c = (HTMLBox)GListGetNext(ls->klist))
  {
    HTMLDestroyBox(li, c);
  }

  return;
}

/*
 * RenderLI
 */
static void
RenderLI(li, box, r)
HTMLInfo li;
HTMLBox box;
Region r;
{
  unsigned int half = box->width / 2;

  XFillArc(li->dpy, li->win, li->gc,
	   box->x + half, box->y + half, half, half,
	   0, 360 * 64);

  return;
}

/*
 * CreateList
 */
static HList *
CreateList(li, env)
HTMLInfo li;
HTMLEnv env;
{
  HList *ls; 

  ls = (HList *)MPCGet(li->mp, sizeof(HList));
  ls->lbox = HTMLCreateBox(li, env);
  ls->lbox->setup = SetupList;
  ls->lbox->render = RenderList;
  ls->lbox->destroy = DestroyList;
  ls->lbox->closure = ls;
  ls->klist = GListCreateX(li->mp);

  return(ls);
}

/*
 *
 * Public functions
 *
 */
void
HTMLULBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HList *ls;
  XFontStruct *font;

  font = HTMLGetFont(li, env);

  ls = CreateList(li, env);
  ls->bullet_diam = font->ascent;
  env->closure = ls;
  env->ff = FLOW_LEFT_JUSTIFY;

  return;
}

void
HTMLListEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HList *ls = (HList *)env->closure;

  HTMLEnvAddBox(li, env->penv, ls->lbox);

  return;
}

void
HTMLOLBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HList *ls;
  char *startstr;
  char *variantstr;

  ls = CreateList(li, env);
  env->closure = ls;
  env->ff = FLOW_LEFT_JUSTIFY;

  /*
   * If the <ol> specifies a START or SEQNUM attribute, handle that,
   * otherwise the default is 1
   */
  if((startstr = MLFindAttribute(p, "start"))!= NULL) 
  {
    ls->count = atoi(startstr);
  }
  else if((startstr = MLFindAttribute(p, "seqnum"))!= NULL) 
  {
    ls->count = atoi(startstr);
  }
  else
  {
    ls->count = 1;
  }
  /*
   * Now see if said <ol> has a TYPE attribute.  
   * Types are:
   *     1 ==> std numbering
   *     aA ==>[aA]lphabetic
   *     iI ==> [rR]oman
   */
  if ((variantstr = MLFindAttribute(p, "type")) != NULL)
  {
    if (variantstr[0] == 'a') ls->type = OL_ALPHA_MINISCULE;
    else if (variantstr[0] == 'A') ls->type = OL_ALPHA_CAP;
    else if (variantstr[0] == 'i') ls->type = OL_ROMAN_MINISCULE;
    else if (variantstr[0] == 'I') ls->type = OL_ROMAN_CAP;
    else ls->type = OL_NUMERIC;
  }
  else
  {
    ls->type = OL_NUMERIC;
  }
  	 
  return;
}

void
HTMLLIBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLBox box;
  HList *ls;
  char buffer[BUFSIZ];

  ls = (HList *)env->penv->closure;
  ls->ibox = HTMLCreateFlowBox(li, env, HTMLGetMaxWidth(li, env->penv));
  GListAddTail(ls->klist, ls->ibox);
  env->closure = ls;
  
  if (HTMLTagToID(env->penv->tag) == TAG_OL)
  {
    OLNext(buffer, sizeof(buffer), ls->count++, ls->type);
    box = HTMLCreateTextBox(li, env,
			    MPStrDup(li->mp, buffer), strlen(buffer));
  }
  else
  {
    box = HTMLCreateBox(li, env);
    box->width = ls->bullet_diam;
    box->height = ls->bullet_diam;
    box->render = RenderLI;
  }
  HTMLSetB(box, BOX_FLOAT_LEFT);
  HTMLLayoutBox(li, ls->ibox, box);

  ls->tbox = HTMLCreateFlowBox(li, env,
                               HTMLGetMaxWidth(li, env->penv) - box->width);

  return;
}

void
HTMLItemEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HList *ls = (HList *)env->closure;

  if (ls->tbox != NULL)
  {
    HTMLFinishFlowBox(li, ls->tbox);
    HTMLLayoutBox(li, ls->ibox, ls->tbox);
  }
  HTMLFinishFlowBox(li, ls->ibox);

  HTMLEnvAddBox(li, env->penv, ls->ibox);

  return;
}

void
HTMLDDBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HList *ls;
  HTMLBox indent;

  ls = (HList *)env->penv->closure;
  ls->ibox = HTMLCreateFlowBox(li, env, HTMLGetMaxWidth(li, env->penv));
  GListAddTail(ls->klist, ls->ibox);
  env->closure = ls;

  indent = HTMLCreateBox(li, env);
  if (ResourceGetUInt(li->cres, "html.dlIndent", &(indent->width)) == NULL)
  {
    indent->width = 20;
  }
  indent->height = 1;
  HTMLSetB(indent, BOX_FLOAT_LEFT);
  HTMLLayoutBox(li, ls->ibox, indent);

  ls->tbox = HTMLCreateFlowBox(li, env,
                               HTMLGetMaxWidth(li, env->penv) - indent->width);

  return;
}

void
HTMLDTBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HList *ls;

  ls = (HList *)env->penv->closure;
  ls->ibox = HTMLCreateFlowBox(li, env, HTMLGetMaxWidth(li, env->penv));
  GListAddTail(ls->klist, ls->ibox);
  env->closure = ls;
  ls->tbox = NULL;              /* this is important */

  return;
}

void
HTMLDLBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HList *ls;

  ls = CreateList(li, env);
  env->closure = ls;
  env->ff = FLOW_LEFT_JUSTIFY;

  return;
}

/*
 * OLNext
 */
static void
OLNext(buffer, bsize, number, oltype)
char *buffer;
size_t bsize;
int number;
OListType oltype;
{
  int i;
  char *cp;

  if (oltype == OL_ALPHA_CAP)
  {
    number = (number-1) % 26; 
    number += 65;
    snprintf(buffer, bsize, "%c.", (char)number);
  }
  else if (oltype == OL_ALPHA_MINISCULE)
  {
    number = (number-1) % 26; 
    number += 97;
    snprintf(buffer, bsize, "%c.", (char)number);
  }
  else if ((oltype==OL_ROMAN_CAP)||(oltype==OL_ROMAN_MINISCULE))
  {
    /*Convert an integer to roman*/
    i=0;
    if (number <0)
    {
      buffer[i++]='-';
      number *= -1;
    }
    if (number==0)
    {
      buffer[i++]='0';
    }
    while ((number > 0) && (i < BUFSIZ - 2))
    {
      if (number >=1000)
      {
	number -=1000;
	buffer[i++]='M';
      }
      else if (number >=900)
      {
	number -=100;
	buffer[i++]='C';
	buffer[i++]='M';
      }
      else if (number >= 500)
      {
	number -= 500;
	buffer[i++] ='D';
      }
      else if (number >= 100)
      {
	number -= 100;
	buffer[i++] ='C';
      }
      else if (number >= 90)
      {
	number-= 10;
	buffer[i++]='X';
	buffer[i++]='C';
      }
      else if (number >= 50)
      {
	number -= 50;
	buffer[i++] = 'L';
      }
      else if (number >= 40)
      {
	number -= 10;
	buffer[i++]='X';
	buffer[i++]='L';
      }
      else if (number >= 10)
      {
	number -= 10;
	buffer[i++]='X';
      }
      else if (number == 9)
      {
	number = 0;
	buffer[i++]='I';
	buffer[i++]='X';
      }
      else if (number >= 5)
      {
	number -= 5;
	buffer[i++]='V';
      }
      else if (number == 4)
      {
	number = 0;
	buffer[i++]='I';
	buffer[i++]='V';
      }
      else if (number >=1)
      {
	number--;
	buffer[i++]='I';
      }
    }
    buffer[i++] = '.';
    buffer[i] = '\0';
    /*We've done everything in caps, now convert to lowercase if
      necessary*/
    if (oltype == OL_ROMAN_MINISCULE)
    {
      for (cp = buffer; *cp != '\0'; cp++)
      {
	*cp = tolower(*cp);
      }
    }
  }
  else
  {
    snprintf(buffer, sizeof(buffer), "%d.", number);
  }

  return;
}

bool
HTMLListAccept(li, obj)
HTMLInfo li;
HTMLObject obj;
{
  if (obj->type != HTML_ENV) return(false);
  if (HTMLTagToID(obj->o.env->tag) != TAG_LI) return(false);
  return(true);
}

bool
HTMLDLAccept(li, obj)
HTMLInfo li;
HTMLObject obj;
{
  if (obj->type != HTML_ENV) return(false);
  if (HTMLTagToID(obj->o.env->tag) != TAG_DD &&
      HTMLTagToID(obj->o.env->tag) != TAG_DT)
  {
    return(false);
  }
  return(true);
}

HTMLInsertStatus
HTMLLIInsert(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLEnv tenv;

  if ((tenv = HTMLFindEnv(li, TAG_UL)) == NULL &&
      (tenv = HTMLFindEnv(li, TAG_OL)) == NULL)
  {
    return(HTMLInsertReject);
  }

  if (HTMLTagToID(tenv->tag) == TAG_UL) HTMLPopEnv(li, TAG_UL);
  else HTMLPopEnv(li, TAG_OL);

  return(HTMLInsertOK);
}

HTMLInsertStatus
HTMLDDInsert(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLEnv tenv;

  if ((tenv = HTMLFindEnv(li, TAG_DL)) == NULL)
  {
    return(HTMLInsertReject);
  }

  HTMLPopEnv(li, TAG_DL);

  return(HTMLInsertOK);
}

HTMLInsertStatus
HTMLDTInsert(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLEnv tenv;

  if ((tenv = HTMLFindEnv(li, TAG_DL)) == NULL)
  {
    return(HTMLInsertReject);
  }

  HTMLPopEnv(li, TAG_DL);

  return(HTMLInsertOK);
}

/*
 * HTMLItemAddBox
 */
void
HTMLItemAddBox(li, env, box)
HTMLInfo li;
HTMLEnv env;
HTMLBox box;
{
  HList *ls = (HList *)env->closure;

  if (ls->tbox != NULL) HTMLLayoutBox(li, ls->tbox, box);
  else HTMLLayoutBox(li, ls->ibox, box);

  return;
}

/*
 * HTMLListAddBox
 */
void
HTMLListAddBox(li, env, box)
HTMLInfo li;
HTMLEnv env;
HTMLBox box;
{
  HList *ls = (HList *)env->closure;
  HTMLBox parent = ls->lbox;

  if (box->width > parent->width) parent->width = box->width;
  parent->height += box->height;

  return;
}

/*
 * HTMLItemWidth
 */
unsigned int
HTMLItemWidth(li, env)
HTMLInfo li;
HTMLEnv env;
{
  HList *ls = (HList *)env->closure;

  if (ls->tbox != NULL) return(HTMLGetBoxWidth(li, ls->tbox));
  else return(HTMLGetBoxWidth(li, ls->ibox));
}
