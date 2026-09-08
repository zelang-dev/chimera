/*
 * css.c
 *
 * Copyright (c) 1998, John Kilburg <john@cs.unlv.edu>
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "port_after.h"

#include "common.h"

#include "Chimera.h"

#include "css.h"

struct CSSPropertyP
{
  CSSRule cr;
  char *name;
  char *value;
};

struct CSSSelectorP
{
  char *tag;
  char *id;
  char *class;
  char *pclass;
};

struct CSSRuleP
{
  GList selectors;      /* list of lists of CSSSelector's */
  GList properties;     /* list of CSSProperty's */
};

/*
 * Used to keep track of which buffer to take input from.  Since new
 * text can be inserted at any time have to be able to store and restore
 * buffer context.
 */
struct CSSInputP
{
  char *b;
  size_t blen;
  char *cp;
  char *ep;
};

struct CSSContextP
{
  MemPool mp;
  ChimeraContext cs;

  /* parser bookkeeping */
  GList inputs;

  /* The Result: a list of rules and properties */
  GList rules;
  GList props;

  /* callback information */
  CSSProc proc;
  void *closure;
  ChimeraTask task;
};

typedef struct CSSInputP *CSSInput;

static char *ParseSpace _ArgProto((char *, char *));
static CSSSelector ParseSelector2 _ArgProto((MemPool, char *, size_t));
static GList ParseSelector1 _ArgProto((MemPool, char *, size_t));
static GList ParseSelector _ArgProto((MemPool, char *, size_t));
static CSSProperty ParseProperty _ArgProto((MemPool, char *, size_t));
static GList ParseProperties _ArgProto((MemPool, char *, size_t));
static bool ParseRule _ArgProto((CSSContext, CSSInput));
static bool ParseAt _ArgProto((CSSContext, CSSInput));
static void ParseCSS _ArgProto((CSSContext));
static int scompare _ArgProto((char *, char *));
static bool SelectorListMatch _ArgProto((GList, GList));
static char *ParseToChar _ArgProto((char *, char *, int, bool));

/*
 * ParseToChar
 *
 * Search for an expected character.  Deals with escaped characters,
 * quoted strings, and pairs of braces/parens/brackets.
 */
static char *
ParseToChar(cp, ep, c, chkws)
char *cp, *ep;
int c;
bool chkws;
{
  bool sq, dq, esc;

  sq = false; /* in single quote? */
  dq = false; /* in double quote? */
  esc = false; /* escape mode? */

  while (cp < ep)
  {
    if (esc) esc = false;
    else if (*cp == '\\') esc = true;
    else if (dq && *cp != '"') dq = false;
    else if (sq && *cp != '\'') sq = false;
    else if (*cp == '"') dq = true;
    else if (*cp == '\'') sq = true;
    else if (*cp == c) return(cp);
    else if (chkws && isspace8(*cp)) return(cp);
    else if (*cp == '{')
    {
      if ((cp = ParseToChar(cp + 1, ep, '}', false)) == NULL) return(NULL);
    }
    else if (*cp == '(')
    {
      if ((cp = ParseToChar(cp + 1, ep, ')', false)) == NULL) return(NULL);
    }
    else if (*cp == '[')
    {
      if ((cp = ParseToChar(cp + 1, ep, ']', false)) == NULL) return(NULL);
    }
    cp++;
  }

  return(NULL);
}

/*
 * ParseAt
 *
 * This is lame.  Handle @ statements later.
 *
 * Return false if @import occured.  Return true if not.
 */
static bool
ParseAt(css, ci)
CSSContext css;
CSSInput ci;
{
  while (ci->cp < ci->ep)
  {
    if (*ci->cp == '{')
    {
      if ((ci->cp = ParseToChar(ci->cp + 1, ci->ep, '}', false)) == NULL)
      {
	ci->cp = ci->ep;
      }
      return(true);
    }
    else if (*ci->cp == ';')
    {
      ci->cp++;
      return(false);
    }
    ci->cp++;
  }

  return(true);
}

/*
 * ParseSpace
 *
 * Look for non-space and deal with comments.
 */
static char *
ParseSpace(s, e)
char *s, *e;
{
  bool sc, ec, ic;

  sc = false; /* possible start comment? */
  ec = false; /* possible end comment? */
  ic = false; /* in comment? */

  while (s < e)
  {
    if (ic)
    {
      if (ec && *s == '/') ic = false;
      else if (*s == '*') ec = true;
      else ec = false;
    }
    else if (sc && *s == '*') ic = true;
    else if (*s == '/') sc = true;
    else if (!isspace8(*s)) return(s);
    else sc = false;
    s++;
  }

  return(NULL);
}

static CSSProperty
ParseProperty(mp, s, slen)
MemPool mp;
char *s;
size_t slen;
{
  char *ep, *colon, *x;
  CSSProperty prop;

  ep = s + slen;

  /* get rid of space in front of the declaration */
  if ((s = ParseSpace(s, ep)) == NULL) return(NULL);

  /* search for the required colon or white space */
  if ((colon = ParseToChar(s, ep, ':', true)) == NULL) return(NULL);

  prop = (CSSProperty)MPCGet(mp, sizeof(struct CSSPropertyP));
  
  prop->name = (char *)MPGet(mp, colon - s + 1);
  strncpy(prop->name, s, colon - s);
  prop->name[colon - s] = '\0';

  /* May have only reached white space before */
  if (*colon != ':')
  {
    /* search for the required colon */
    if ((colon = ParseToChar(colon, ep, ':', false)) == NULL) return(NULL);
  }

  /* remove space in front of the value */
  if ((s = ParseSpace(colon + 1, ep)) == NULL) return(NULL);

  prop->value = (char *)MPGet(mp, ep - s + 1);

  /* Copy the value until end of input or whitespace is seen */
  x = prop->value;
  while (s < ep)
  {
    if (isspace8(*s)) break;
    *x++ = *s++;
  }
  *x = '\0';

  if (strlen(prop->name) == 0 || strlen(prop->value) == 0) return(NULL);

  return(prop);
}

static GList
ParseProperties(mp, s, slen)
MemPool mp;
char *s;
size_t slen;
{
  char *last;
  char *ep;
  GList properties;
  CSSProperty prop;

  properties = GListCreateX(mp);

  ep = s + slen;
  while (s < ep)
  {
    last = s;
    s = ParseToChar(s, ep, ';', false);
    if (s == NULL) s = ep;

    if (last < s && (prop = ParseProperty(mp, last, s - last)) != NULL)
    {
      GListAddTail(properties, prop);
    }

    s++;
  }

  if (GListGetHead(properties) == NULL) return(NULL);

  return(properties);
}

static CSSSelector
ParseSelector2(mp, s, slen)
MemPool mp;
char *s;
size_t slen;
{
  CSSSelector cs;
  char *tag = NULL;
  char *id = NULL;
  char *class = NULL;
  char *pclass = NULL;
  char *ep;
  char *t;

  cs = (CSSSelector)MPCGet(mp, sizeof(struct CSSSelectorP));

  t = MPGet(mp, slen + 1);
  strncpy(t, s, slen);
  t[slen] = '\0';
  s = t;

  if (*s == '#') id = s + 1;
  else if (*s == '.') class = s + 1;
  else if (*s == ':') pclass = s + 1;
  else
  {
    tag = s;
    ep = s + slen;
    while (s < ep)
    {
      if (*s == '#') { id = s + 1; *s = '\0'; break; }
      else if (*s == '.') { class = s + 1; *s = '\0'; break; }
      else if (*s == ':') { pclass = s + 1; *s = '\0'; break; }
      else s++;
    }
  }

  cs->tag = tag;
  cs->id = id;
  cs->class = class;
  cs->pclass = pclass;

  return(cs);
}

static GList
ParseSelector1(mp, s, slen)
MemPool mp;
char *s;
size_t slen;
{
  char *ep = s + slen;
  char *cp;
  char *last;
  CSSSelector cs;
  GList selectors;

  if ((s = ParseSpace(s, ep)) == NULL) return(NULL);

  selectors = GListCreateX(mp);

  last = s;
  cp = s;
  while (cp < ep)
  {
    if (isspace8(*cp))
    {
      if ((cs = ParseSelector2(mp, last, cp - last)) != NULL)
      {
	GListAddTail(selectors, cs);
      }

      if ((cp = ParseSpace(cp, ep)) == NULL) break;
      last = cp;
    }
    else cp++;
  }

  if (last < cp)
  {
    if ((cs = ParseSelector2(mp, last, cp - last)) != NULL)
    {
      GListAddTail(selectors, cs);
    }
  }

  if (GListGetHead(selectors) == NULL) return(NULL);

  return(selectors);
}

/*
 * ParseSelector
 */
static GList
ParseSelector(mp, s, slen)
MemPool mp;
char *s;
size_t slen;
{
  GList slist, xlist;
  char *ep;
  char *last;

  slist = GListCreateX(mp);

  ep = s + slen;
  while (s < ep)
  {
    last = s;
    s = ParseToChar(s, ep, ',', false);
    if (s == NULL) s = ep;

    if (last < s && (xlist = ParseSelector1(mp, last, s - last)) != NULL)
    {
      GListAddTail(slist, xlist);
    }
    s++;
  }

  if (GListGetHead(slist) == NULL) return(NULL);

  return(slist);
}

/*
 * scompare
 *
 * do the sort of string comparison that we need here
 */
static int
scompare(s1, s2)
char *s1, *s2;
{
  if (s1 == s2) return(0);
  if (s1 == NULL || s2 == NULL) return(-1);
  if (strlen(s1) != strlen(s2)) return(-1);
  return(strcasecmp(s1, s2));
}

/*
 * ParseRule
 *
 * Parse a single rule.  Return false if parsing error that leads to EOF.
 */
static bool
ParseRule(css, ci)
CSSContext css;
CSSInput ci;
{
  char *last;
  GList slist;
  GList plist;
  CSSRule cr;

  slist = GListCreateX(css->mp);

  last = ci->cp;
  if ((ci->cp = ParseToChar(ci->cp, ci->ep, '{', false)) == NULL)
  {
    ci->cp = ci->ep;
    return(false);
  }

  slist = ParseSelector(css->mp, last, ci->cp - last);

  ci->cp++;    /* need to get past the '{' from above */
  last = ci->cp;
  if ((ci->cp = ParseToChar(ci->cp, ci->ep, '}', false)) == NULL)
  {
    ci->cp = ci->ep;
    return(false);
  }

  plist = ParseProperties(css->mp, last, ci->cp - last);

  ci->cp++;

  /*
   * Return true because parse was successful.
   */
  if (plist == NULL || GListGetHead(plist) == NULL ||
      slist == NULL || GListGetHead(slist) == NULL) return(true);

  cr = (CSSRule)MPCGet(css->mp, sizeof(struct CSSRuleP));
  cr->selectors = slist;
  cr->properties = plist;
  GListAddTail(css->rules, cr);

  return(true);
}

/*
 * ParseCSS
 *
 * Toplevel parser loop.
 */
static void
ParseCSS(css)
CSSContext css;
{
  CSSInput ci = (CSSInput)GListGetHead(css->inputs);

  if (ci == NULL)
  {
    if (css->proc == NULL) return;

    /*
     * Should call callback here because this means all CSS text including
     * remote text has been parsed.  Callback must be made from a toplevel
     * task so create a task first.
     */
    return;
  }

  ci->cp = ParseSpace(ci->cp, ci->ep);
  while (ci->cp != NULL && ci->cp < ci->ep)
  {
    if (*ci->cp == '@')
    {
      if (!ParseAt(css, ci)) return;
    }
    else if (!ParseRule(css, ci)) return;
    
    ci->cp = ParseSpace(ci->cp, ci->ep);
  }

  GListPop(css->inputs);

  ParseCSS(css);

  return;
}

CSSContext
CSSParseBuffer(cs, b, blen, proc, closure)
ChimeraContext cs;
char *b;
size_t blen;
CSSProc proc;
void *closure;
{
  CSSContext css;
  MemPool mp;
  CSSInput ci;

  mp = MPCreate();
  css = (CSSContext)MPCGet(mp, sizeof(struct CSSContextP));
  css->mp = mp;
  css->cs = cs;
  css->rules = GListCreateX(mp);
  css->props = GListCreateX(mp);
  css->inputs = GListCreateX(mp);
  css->proc = proc;
  css->closure = closure;

  ci = (CSSInput)MPCGet(mp, sizeof(struct CSSInputP));
  ci->b = b;
  ci->blen = blen;
  ci->cp = b;
  ci->ep = b + blen;
  GListAddHead(css->inputs, ci);

  ParseCSS(css);

  return(css);
}

/*
 * CSSDestroyContext
 */
void
CSSDestroyContext(css)
CSSContext css;
{
  MPDestroy(css->mp);
  return;
}

static bool
SelectorListMatch(slist1, slist2)
GList slist1, slist2;
{
  CSSSelector cs, ncs;

  /*
   * This code needs attention.  It compares two selector lists.
   */
  for (cs = (CSSSelector)GListGetHead(slist1),
       ncs = (CSSSelector)GListGetHead(slist2);
       cs != NULL && ncs != NULL;
       cs = (CSSSelector)GListGetNext(slist1),
       ncs = (CSSSelector)GListGetNext(slist2))
  {
    if (scompare(ncs->tag, cs->tag) != 0 ||
	scompare(ncs->id, cs->id) != 0 ||
	scompare(ncs->class, cs->class) != 0 ||
	scompare(ncs->pclass, cs->pclass) != 0)
    {
      return(false);
    }
  }

  if (ncs != NULL || cs != NULL) return(false);

  return(true);
}

/*
 * CSSCreateSelector
 */
CSSSelector
CSSCreateSelector(mp)
MemPool mp;
{
  return((CSSSelector)MPCGet(mp, sizeof(struct CSSSelectorP)));
}

/*
 * CSSSetSelector
 */
void
CSSSetSelector(cs, tag, id, class, pclass)
CSSSelector cs;
char *tag, *id, *class, *pclass;
{
  cs->tag = tag;
  cs->id = id;
  cs->class = class;
  cs->pclass = pclass;
  return;
}

/*
 * CSSFindProperty
 */
char *
CSSFindProperty(css, selectors, name)
CSSContext css;
GList selectors;
char *name;
{
  CSSProperty p;
  GList slist, xlist;
  CSSSelector h;

  if ((h = GListGetHead(selectors)) != GListGetTail(selectors))
  {
    slist = GListCreate();
    GListAddHead(slist, h);
  }
  else slist = NULL;

  for (p = (CSSProperty)GListGetHead(css->props); p != NULL;
       p = (CSSProperty)GListGetNext(css->props))
  {
    if (scompare(p->name, name) == 0)
    {
      for (xlist = (GList)GListGetHead(p->cr->selectors); xlist != NULL;
	   xlist = (GList)GListGetNext(p->cr->selectors))
      {
	if ((slist != NULL && SelectorListMatch(xlist, slist)) ||
	    SelectorListMatch(xlist, selectors))
	{
	  if (slist != NULL) GListDestroy(slist);
	  return(p->value);
	}
      }
    }
  }

  if (slist != NULL) GListDestroy(slist);

  return(NULL);
}

/*
 * CSSPrintSelectorList
 */
void
CSSPrintSelectorList(slist)
GList slist;
{
  CSSSelector s;

  for (s = (CSSSelector)GListGetHead(slist); s != NULL;
       s = (CSSSelector)GListGetNext(slist))
  {
    if (s->tag != NULL) fprintf (stderr, "%s ", s->tag);
    if (s->id != NULL) fprintf (stderr, "%s ", s->id);
    if (s->class != NULL) fprintf (stderr, "%s ", s->class);
    if (s->pclass != NULL) fprintf (stderr, "%s ", s->pclass);
    fprintf (stderr, "\n");
  }

  return;
}

/*
 * CSSPrint
 */
void
CSSPrint(css)
CSSContext css;
{
  CSSRule cr;
  CSSSelector cs;
  GList slist;
  CSSProperty prop;

  for (cr = (CSSRule)GListGetHead(css->rules); cr != NULL;
       cr = (CSSRule)GListGetNext(css->rules))
  {
    for (slist = (GList)GListGetHead(cr->selectors); slist != NULL;
	 slist = (GList)GListGetNext(cr->selectors))
    {
      for (cs = (CSSSelector)GListGetHead(slist); cs != NULL;
	   cs = (CSSSelector)GListGetNext(slist))
      {
	printf ("%s ", cs->tag);
      }
      printf (", ");
    }

    printf ("{\n");
    for (prop = (CSSProperty)GListGetHead(cr->properties); prop != NULL;
	 prop = (CSSProperty)GListGetNext(cr->properties))
    {
      printf ("\t%s : %s;\n", prop->name, prop->value);
    }
    printf ("}\n");
  }
}

#ifdef CSSDEBUG

#include <sys/types.h>
#include <sys/stat.h>

main(argc, argv)
int argc;
char *argv[];
{
  CSSContext css;
  FILE *fp;
  char *b;
  char *name;
  struct stat st;
  GList slist;
  CSSSelector cs;
  MemPool mp;

  if (argc < 2) exit(1);

  if (stat(argv[1], &st) != 0) exit(1);

  if ((b = (char *)alloc_mem(st.st_size)) == NULL) exit(1);

  if ((fp = fopen(argv[1], "r")) == NULL) exit(1);
  fread(b, 1, st.st_size, fp);
  fclose(fp);

  css = CSSParseBuffer(NULL, b, st.st_size, NULL, NULL); 

  mp = MPCreate();
  slist = GListCreate();

  cs = CSSCreateSelector(mp);
  CSSSetSelector(cs, "H5", NULL, NULL, NULL);
  GListAddHead(slist, cs);

  name = "white-space";

  if ((b = CSSFindProperty(css, slist, name)) != NULL)
  {
    printf ("%s == %s\n", name, b);
  }
  else printf ("%s not found\n", name);

  CSSPrint(css);

  CSSDestroyContext(css);

  exit(0);
}

#endif
