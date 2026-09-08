/*
 * misc.c
 *
 * libhtml - HTML->X renderer
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

static void RenderAnchor _ArgProto((HTMLInfo, HTMLBox, Region));

/*
 * RenderAnchor
 */
static void
RenderAnchor(li, box, r)
HTMLInfo li;
HTMLBox box;
Region r;
{
  return;
}

void
HTMLAnchorBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  char *value;
  HTMLBox box;

  if ((value = MLFindAttribute(p, "name")) != NULL)
  {
    box = HTMLCreateBox(li, env);
    box->width = 2;
    box->height = 2;
    box->render = RenderAnchor;
    HTMLEnvAddBox(li, env, box);
    HTMLAddAnchor(li, box, value, NULL);
  }
  if (MLFindAttribute(p, "href") != NULL) env->anchor = p;
  return;
}

HTMLInsertStatus
HTMLAnchorInsert(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  if (MLFindAttribute(p, "href") == NULL)
  {
    if (MLFindAttribute(p, "name") != NULL) return(HTMLInsertEmpty);
    else return(HTMLInsertReject);
  }
  return(HTMLInsertOK);
}

void
HTMLHxBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLAttribID aid;
  HTMLTagID tagid;

  tagid = HTMLTagToID(env->tag);

  if (tagid == TAG_H1) HTMLSetFontScale(env->fi, 5);
  else if (tagid == TAG_H2) HTMLSetFontScale(env->fi, 4);
  else if (tagid == TAG_H3) HTMLSetFontScale(env->fi, 3);
  else if (tagid == TAG_H4) HTMLSetFontScale(env->fi, 2);
  else if (tagid == TAG_H5) HTMLSetFontScale(env->fi, 1);
  else if (tagid == TAG_H6) HTMLSetFontScale(env->fi, 0);

  aid = HTMLAttributeToID(p, "align");
  if (aid == ATTRIB_CENTER || aid == ATTRIB_RIGHT)
  {
    if (aid == ATTRIB_CENTER) env->ff = FLOW_CENTER_JUSTIFY;
    else if (aid == ATTRIB_RIGHT) env->ff = FLOW_RIGHT_JUSTIFY;
  }

  return;
}

void
HTMLHxEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  return;
}

void
HTMLFontBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLTagID tagid = HTMLTagToID(env->tag);

  if (tagid == TAG_EM || tagid == TAG_CITE || tagid == TAG_I)
  {
    HTMLAddFontSlant(env->fi);
  }
  else if (tagid == TAG_B || tagid == TAG_STRONG)
  {
    HTMLAddFontWeight(env->fi);
  }
  else if (tagid == TAG_TT) HTMLSetFontFixed(env->fi);
  
  return;
}

void
HTMLParaBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLAttribID aid;

  aid = HTMLAttributeToID(p, "align");
  if (aid == ATTRIB_CENTER || aid == ATTRIB_RIGHT)
  {
    if (aid == ATTRIB_CENTER) env->ff = FLOW_CENTER_JUSTIFY;
    else if (aid == ATTRIB_RIGHT) env->ff = FLOW_RIGHT_JUSTIFY;
  }

  return;
}

void
HTMLParaEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  return;
}

HTMLInsertStatus
HTMLParaInsert(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  /*
   * If the current environment is <p> then terminate the environment
   * before starting the new one.
   */
  if (HTMLTagToID(env->tag) == TAG_P) HTMLEndEnv(li, TAG_P);
  return(HTMLInsertOK);
}

/*
 * HTMLBreak
 */
void
HTMLBreak(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLAttribID aid;
  HTMLBox box;

  box = HTMLCreateBox(li, env);

  aid = HTMLAttributeToID(p, "clear");
  if (aid == ATTRIB_LEFT) HTMLSetB(box, BOX_CLEAR_LEFT);
  else if (aid == ATTRIB_RIGHT) HTMLSetB(box, BOX_CLEAR_RIGHT);
  else if (aid == ATTRIB_ALL) HTMLSetB(box, BOX_CLEAR_RIGHT | BOX_CLEAR_LEFT);
  else HTMLSetB(box, BOX_LINEBREAK);

  HTMLEnvAddBox(li, env, box);

  return;
}

void
HTMLAddressBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLAddFontSlant(env->fi);
  HTMLAddBlankLine(li, env);
  return;
}

void
HTMLAddressEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLAddBlankLine(li, env);
  return;
}

void
HTMLBQBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLAddLineBreak(li, env);
  return;
}

void
HTMLBQEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLAddLineBreak(li, env);
  return;
}

void
HTMLCenterBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  env->ff = FLOW_CENTER_JUSTIFY;
  HTMLAddLineBreak(li, env);
  return;
}

void
HTMLCenterEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLAddLineBreak(li, env);
  return;
}

void
HTMLDivBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLAttribID aid;

  aid = HTMLAttributeToID(p, "align");
  if (aid == ATTRIB_CENTER) env->ff = FLOW_CENTER_JUSTIFY;
  else if (aid == ATTRIB_LEFT) env->ff = FLOW_LEFT_JUSTIFY;
  else if (aid == ATTRIB_RIGHT) env->ff = FLOW_RIGHT_JUSTIFY;

  HTMLAddLineBreak(li, env);

  return;
}

void
HTMLDivEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLAddLineBreak(li, env);

  return;
}

