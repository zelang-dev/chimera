/*
 * text.c
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

#define TAB_EXPANSION 4

typedef struct TextStateP
{
  char *s;
  size_t len;
} TextState;

/*
 *
 * Private functions
 *
 */
static void RenderText _ArgProto((HTMLInfo, HTMLBox, Region));
static char *ExpandTabs _ArgProto((HTMLInfo, char *, size_t, size_t));
static void LineMode _ArgProto((HTMLInfo, HTMLEnv, MLElement));
static void HTMLPreformatted _ArgProto((HTMLInfo, HTMLEnv, char *, size_t));

/*
 * RenderText
 */
static void
RenderText(li, box, r)
HTMLInfo li;
HTMLBox box;
Region r;
{
  TextState *ts = (TextState *)box->closure;
  int x, y;
  XFontStruct *font = HTMLGetFont(li, box->env);

  XSetFont(li->dpy, li->gc, font->fid);

  /* y position + ascent to line up correctly + descent for line spacing */
  y = box->y + font->ascent;
  x = box->x;

  if (box->env->anchor != NULL)
  {
    if (HTMLTestB(box, BOX_SELECT))
    {
      XSetForeground(li->dpy, li->gc, li->anchor_select_color);
    }
    else XSetForeground(li->dpy, li->gc, li->anchor_color);

    XDrawString(li->dpy, li->win, li->gc, x, y, ts->s, (int)ts->len);
    XDrawLine(li->dpy, li->win, li->gc, x, y + font->descent - 1,
	      x + box->width, y + font->descent - 1);

    XSetForeground(li->dpy, li->gc, li->fg);
  }
  else
  {
    XDrawString(li->dpy, li->win, li->gc, x, y, ts->s, (int)ts->len);
  }

  return;
}

/*
 * HTMLCreateTextBox
 */
HTMLBox
HTMLCreateTextBox(li, env, s, slen)
HTMLInfo li;
HTMLEnv env;
char *s;
size_t slen;
{
  HTMLBox box;
  TextState *ts;
  HTMLBoxFlags bflags;
  XFontStruct *font = HTMLGetFont(li, env);

  if (s == NULL)
  {
    s = " ";
    slen = 1;
    bflags = BOX_SPACE;
  }
  else
  {
    bflags = BOX_NONE;
  }

  ts = (TextState *)MPCGet(li->mp, sizeof(TextState));
  ts->s = s;
  ts->len = slen;

  box = HTMLCreateBox(li, env);
  HTMLSetB(box, bflags);
  box->closure = ts;
  box->render = RenderText;
  box->width = XTextWidth(font, s, slen);
  box->height = font->ascent + font->descent + li->textLineSpace;
  box->baseline = font->ascent + li->textLineSpace;

  return(box);
}

/*
 * HTMLPreformatted - basically, the guts of LineMode()
 */
static void
HTMLPreformatted(li, env, text, len)
HTMLInfo li;
HTMLEnv env;
char *text;
size_t len;
{
  char *cp, *lastcp, *start;
  size_t ptlen;
  char *s;
  bool tabbed;

  lastcp = text + len;
  tabbed = false;
  for (ptlen = 0, start = cp = text; cp < lastcp; cp++)
  {
    if (*cp == '\r') continue;
    else if (*cp == '\n')
    {
      if (tabbed)
      {
	s = ExpandTabs(li, start, cp - start, ptlen);
	HTMLEnvAddBox(li, env, HTMLCreateTextBox(li, env, s, ptlen));
      }
      else
      {
	HTMLEnvAddBox(li, env, HTMLCreateTextBox(li, env, start, ptlen));
      }

      HTMLAddLineBreak(li, env);

      ptlen = 0;
      start = cp + 1;
      tabbed = false;
    }
    else if (*cp == '\t')
    {
      ptlen += TAB_EXPANSION;
      tabbed = true;
    }
    else ptlen++;
  }
  
  if (tabbed)
  {
    s = ExpandTabs(li, start, cp - start, ptlen);
    HTMLEnvAddBox(li, env, HTMLCreateTextBox(li, env, s, ptlen));
  }
  else
  {
    HTMLEnvAddBox(li, env, HTMLCreateTextBox(li, env, start, ptlen));
  }

  return;
}

/*
 *
 * Public functions
 *
 */

/*
 * ExpandTabs
 */
static char *
ExpandTabs(li, t, len, tlen)
HTMLInfo li;
char *t;
size_t len;
size_t tlen;
{
  char *s, *cp, *lastcp, *xs;

  lastcp = t + len;
  s = (char *)MPGet(li->mp, tlen);
  for (cp = t, xs = s; cp < lastcp; cp++)
  {
    if (*cp == '\t')
    {
      memset(xs, ' ', TAB_EXPANSION);
      xs += TAB_EXPANSION;
    }
    else if (*cp != '\r')
    {
      *xs = *cp;
      xs++;
    }
  }

  return(s);
}

/*
 * LineMode
 */
static void
LineMode(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  char *text;
  size_t len;

  MLGetText(p, &text, &len);
  if (text == NULL) return;

  HTMLPreformatted(li, env, text, len);
  
  return;
}

/*
 * HTMLPlainData
 */
void
HTMLPlainData(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  LineMode(li, env, p);
  return;
}

/*
 * HTMLPreData
 */
void
HTMLPreData(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  LineMode(li, env, p);
  return;
}

/*
 * HTMLFillData
 */
void
HTMLFillData(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  char *s, *cp, *lastcp;
  bool found_space;
  char *text;
  size_t len;

  MLGetText(p, &text, &len);
  if (text == NULL) return;

  if (li->formatted) HTMLPreformatted(li, env, text, len);
  else
  {
    for (lastcp = text + len, cp = text; cp < lastcp; )
    {
      for (found_space = false; cp < lastcp && isspace8(*cp); cp++)
      {
	found_space = true;
      }
      
      if (found_space)
      {
	HTMLEnvAddBox(li, env, HTMLCreateTextBox(li, env, NULL, 0));
      }
      if (cp == lastcp) break;
      
      for (s = cp; cp < lastcp && !isspace8(*cp); cp++)
	  ;
      
      HTMLEnvAddBox(li, env, HTMLCreateTextBox(li, env, s, cp - s));
    }
  }

  return;
}

void
HTMLPreBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLSetFontFixed(env->fi);
  li->formatted = true;	/* would anyone actually nest PREs? - djhjr */
  return;
}

void
HTMLPreEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  li->formatted = false;	/* nah... - djhjr */
  return;
}

void
HTMLPlainEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  return;
}

void
HTMLPlainBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLSetFontFixed(env->fi);
  return;
}

void
HTMLNoFrameBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  return;
}

/*
 * HTMLNoFrameEnd
 */
void
HTMLNoFrameEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  return;
}

/*
 * HTMLGetText
 */
char *
HTMLGetEnvText(mp, env)
MemPool mp;
HTMLEnv env;
{
  HTMLObject c, b, e;
  char *text, *str;
  size_t tlen = 0;
  size_t len;
  GList list = env->slist;

  if (GListEmpty(list)) return(NULL);

  e = (HTMLObject)GListGetTail(list);
  for (b = c = (HTMLObject)GListGetHead(list); c != NULL;
       c = (HTMLObject)GListGetNext(list))
  {
    if (c != b && c != e)
    {
      if (c->type != HTML_ENV)
      {
	MLGetText(c->o.p, &str, &len);
	tlen += len;
      }
    }
  }

  text = MPGet(mp, tlen + 1);

  tlen = 0;
  for (c = (HTMLObject)GListGetHead(list); c != NULL;
       c = (HTMLObject)GListGetNext(list))
  {
    if (c != b && c != e)
    {
      if (c->type != HTML_ENV)
      {
	MLGetText(c->o.p, &str, &len);
	strncpy(text + tlen, str, len);
	tlen += len;
      }
    }
  }
  text[tlen] = '\0';

  return(text);
}

/*
 * HTMLAddLineBreak
 */
void
HTMLAddLineBreak(li, env)
HTMLInfo li;
HTMLEnv env;
{
  HTMLBox box;

  box = HTMLCreateBox(li, env);
  HTMLSetB(box, BOX_LINEBREAK);
  HTMLEnvAddBox(li, env, box);

  return;
}

/*
 * HTMLAddBlankLine
 */
void
HTMLAddBlankLine(li, env)
HTMLInfo li;
HTMLEnv env;
{
  HTMLBox box;

  HTMLAddLineBreak(li, env);

  box = HTMLCreateBox(li, env);
  box->height = 6;
  box->width = 0;            /* really really want this to be zero */
  HTMLEnvAddBox(li, env, box);

  HTMLAddLineBreak(li, env);

  return;
}

/*
 * HTMLStringSpacify
 *
 * Turns all whitespace to spaces
 */
void
HTMLStringSpacify(s, len)
char *s;
size_t len;
{
  char *cp, *lastcp = s + len;

  for (cp = s; cp < lastcp; cp++)
  {
    if (isspace8(*cp)) *cp = ' ';
  }

  return;
}
