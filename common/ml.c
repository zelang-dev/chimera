/*
 * ml.c
 *
 * Copyright (c) 1995-1997, John Kilburg <john@cs.unlv.edu>
 *
 * Bogus parser for things that kind of maybe look like HTML/SGML.
 * Or something.
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
#include "ml.h"

typedef enum
{
  MLS_TAG,                    /* inside tag */
  MLS_AVS,                    /* inside attribute value (single quote) */
  MLS_AVD,                    /* inside attribute value (double quote) */
  MLS_PSCOMMENT,              /* possible start comment */
  MLS_PECOMMENT,              /* possible end comment */
  MLS_COMMENT,                /* inside comment */
  MLS_DATA,                   /* between tags */
  MLS_START                   /* start state */
} MLStateID;

#define IN_STATE(a, b) ((a)->plist->state == (b))

struct MLAttributeP
{
  char *name;
  char *value;
  struct MLAttributeP *next;
};

struct MLElementP
{
  MLElementType type;    
  char *ptext;                 /* processed text */
  size_t plen;                 /* processed text length */
  MLAttribute ta;              /* attribute list */
};

struct PState
{
  MLStateID state;
  size_t off;
  struct PState *next;
};

struct MLStateP
{
  MemPool tmp;                 /* temporary memory pool */
  MemPool mp;                  /* memory pool descriptor */
  char *data;                  /* address of HTML buffer */
  size_t len;                  /* length of HTML buffer */
  size_t off;                  /* offset of parse */
  MLElementHandler func;       /* called when part created */
  void *closure;               /* state info for callback */
  MLElement head, tail;        /* element list */
  MLElement curr;              /* next unprocessed element */
  struct PState *plist;        /* parser state stack */
  int count;                   /* call count */
};

static MLAttribute hs_parse_tag _ArgProto((MLState, char *, size_t));
static void hs_add_element _ArgProto((MLState, MLElement));
static MLElement hs_create_element _ArgProto((MLState, char *, size_t));
static MLElement hs_create_text _ArgProto((MLState, char *, size_t));
static MLElement hs_create_tag _ArgProto((MLState, char *, size_t));
static char *hs_condense _ArgProto((MLState, char *, size_t, size_t *));
static MLAttribute hs_create_attribute _ArgProto((MLState, char *, size_t));
static void hs_push _ArgProto((MLState, int));
static size_t hs_pop _ArgProto((MLState));
static void hs_handle_data _ArgProto((MLState, char *, size_t, bool));

/*
 * hs_push
 */
static void
hs_push(hs, state)
MLState hs;
int state;
{
  struct PState *ps;
  ps = (struct PState *)MPCGet(hs->tmp, sizeof(struct PState));
  ps->off = hs->off;
  ps->state = state;
  ps->next = hs->plist;
  hs->plist = ps;
  return;
}

/*
 * hs_pop
 */
static size_t
hs_pop(hs)
MLState hs;
{
  size_t off;
  if (hs->plist == NULL) abort();
  off = hs->plist->off;
  hs->plist = hs->plist->next;
  return(off);
}

/*
 * hs_create_attribute
 */
static MLAttribute
hs_create_attribute(hs, text, len)
MLState hs;
char *text;
size_t len;
{
  MLAttribute ta;

  ta = (MLAttribute)MPCGet(hs->mp, sizeof(struct MLAttributeP));

  ta->name = (char *)MPGet(hs->mp, len + 1);
  strncpy(ta->name, text, len);
  ta->name[len] = '\0';

  return(ta);
}

struct entity
{
  char *text;
  int len;
  unsigned char c;
} clist[] =
{
  { "amp", 3, '&' },
  { "lt", 2, '<' },
  { "gt", 2, '>' },
  { "copy", 4, 169 },
  { "AElig", 5, 198 },
  { "Aacute", 6, 193 },
  { "Acirc", 5, 194 },
  { "Agrave", 6, 192 },
  { "Aring", 5, 197 },
  { "Atilde", 6, 195 },
  { "Auml", 4, 196 },
  { "Ccedil", 6, 199 },
  { "ETH", 3, 208 },
  { "Eacute", 6, 201 },
  { "Ecirc", 5, 202 },
  { "Egrave", 6, 200 },
  { "Euml", 4, 203 },
  { "Iacute", 6, 205 },
  { "Icirc", 5, 206 },
  { "Igrave", 6, 204 },
  { "Iuml", 4, 207 },
  { "Ntilde", 6, 209 },
  { "Oacute", 6, 211 },
  { "Ocirc", 5, 212 },
  { "Ograve", 6, 210 },
  { "Oslash", 6, 216 },
  { "Otilde", 6, 213 },
  { "Ouml", 4, 214 },
  { "THORN", 5, 222 },
  { "Uacute", 6, 218 },
  { "Ucirc", 5, 219 },
  { "Ugrave", 6, 217 },
  { "Uuml", 4, 220 },
  { "Yacute", 6, 221 },
  { "aacute", 6, 225 },
  { "acirc", 5, 226 },
  { "aelig", 5, 230 },
  { "agrave", 6, 224 },
  { "aring", 5, 229 },
  { "atilde", 6, 227 },
  { "auml", 4, 228 },
  { "ccedil", 6, 231 },
  { "eacute", 6, 233 },
  { "ecirc", 5, 234 },
  { "egrave", 6, 232 },
  { "eth", 3, 240 },
  { "euml", 4, 235 },
  { "iacute", 6, 237 },
  { "icirc", 5, 238 },
  { "igrave", 6, 236 },
  { "iuml", 4, 239 },
  { "ntilde", 6, 241 },
  { "oacute", 6, 243 },
  { "ocirc", 5, 244 },
  { "ograve", 6, 242 },
  { "oslash", 6, 248 },
  { "otilde", 6, 245 },
  { "ouml", 4, 246 },
  { "szlig", 5, 223 },
  { "thorn", 5, 254 },
  { "uacute", 6, 250 },
  { "ucirc", 5, 251 },
  { "ugrave", 6, 249 },
  { "uuml", 4, 252 },
  { "yacute", 6, 253 },
  { "yuml", 4, 255 },
  { "reg", 3, 174 },
  { "comma", 5, 44 },
  { "colon", 5, 58 },
  { "quot", 4, '\"' },
  { "nbsp", 4, ' ' },
  { NULL, 0, '\0' },
};

/*
 * hs_condense
 */
static char *
hs_condense(hs, text, len, newlen)
MLState hs;
char *text;
size_t len;
size_t *newlen;
{
  char *cp, *lastcp;
  size_t i;
  char *b, *bcp;
  char *x;

  /*
   * allocate a buffer for the new text.  the new text will be smaller than
   * the incoming text
   */
  b = (char *)MPGet(hs->mp, len + 1);
  bcp = b;

  for (cp = text, lastcp = text + len; cp < lastcp; cp++)
  {
    /*
     * Found entity and there is at least one more character?
     */
    if (*cp == '&')
    {
      for (x = cp; x < lastcp && *x != ';' && !isspace8(*x); x++)
	  ;
      if (x < lastcp)                              /* found end of entity ? */
      {
	if (*x == '\r') x++;                       /* this may die */
	if (*(cp + 1) == '#')                      /* number given ? */
	{
	  *bcp = (char)atoi(cp + 2);               /* get the character */
	  bcp++;
	  cp = x;
	}
	else
	{
	  char *crap;

	  /* Search in the table of entity names */
	  for (i = 0, crap = cp + 1; clist[i].text != NULL; i++)
	  {
	    if (strncmp(clist[i].text, crap, clist[i].len) == 0)
	    {
	      *bcp = clist[i].c;
	      bcp++;
	      cp = x;
	      break;
	    }
	  }
	}
      }
      if (cp < x)
      {
	*bcp = *cp;                 
	bcp++;
      }
    }
    else
    {
      *bcp = *cp;                                 /* not part of entity */
      bcp++;
    }
  }

  *newlen = bcp - b;
  *bcp = '\0';

  return(b);
}

/*
 * hs_parse_tag
 */
MLAttribute
hs_parse_tag(hs, text, len)
MLState hs;
char *text;
size_t len;
{
  char *cp, *start, *lastcp;
  MLAttribute tahead, tatail, ta;
  size_t newlen;

  tahead = NULL;
  tatail = NULL;
  lastcp = text + len;
  for (cp = text; cp < lastcp; )
  {
    /* Eat leading spaces */
    for (; cp < lastcp && isspace8(*cp); cp++)
	;
    if (cp == lastcp) break;

    /* Look for value */
    for (start = cp; cp < lastcp && !isspace8(*cp) && *cp != '='; cp++)
	;

    ta = hs_create_attribute(hs, start, cp - start);

    if (isspace8(*cp))
    {
      for (; cp < lastcp && isspace8(*cp); cp++)
	  ;
    }

    /* Found value */
    if (cp < lastcp && *cp == '=')
    {
      cp++; /* past '=' */

      /* Eat leading spaces */
      for (; cp < lastcp && isspace8(*cp); cp++)
	  ;
      if (cp < lastcp)
      {
	/* Quoted value */
	if (*cp == '"')
	{
	  cp++; /* past '"' */

	  for (start = cp; cp < lastcp && *cp != '"'; cp++)
	      ;
	}
	else if (*cp == '\'') /* single quoted value */
	{
	  cp++; /* past ' */

	  for (start = cp; cp < lastcp && *cp != '\''; cp++)
	      ;
	}
	else /* Unquoted value */
	{
	  for (start = cp; cp < lastcp && !isspace8(*cp); cp++)
	      ;
	}

	ta->value = hs_condense(hs, start, cp - start, &newlen);
      }
    }

    if (tatail == NULL) tahead = ta;
    else tatail->next = ta;
    tatail = ta;
  }

  return(tahead);
}

/* 
 * hs_add_element
 */
static void
hs_add_element(hs, p)
MLState hs;
MLElement p;
{
  (hs->func)(hs->closure, p);

  return;
}

/*
 * hs_create_element
 */
static MLElement
hs_create_element(hs, ptext, plen)
MLState hs;
char *ptext;
size_t plen;
{
  MLElement p;

  p = (MLElement)MPCGet(hs->mp, sizeof(struct MLElementP));
  p->ptext = ptext;
  p->plen = plen;

  return(p);
}

/*
 * hs_create_text
 */
static MLElement
hs_create_text(hs, text, len)
MLState hs;
char *text;
size_t len;
{
  char *cp;
  size_t newlen;
  size_t i;
  MLElement p;

  /* Check for an entity */
  for (cp = text, i = 0; i < len && *cp != '&'; cp++, i++)
      ;

  if (i == len) p = hs_create_element(hs, text, len);
  else
  {
    /* found an entity...condense */
    cp = hs_condense(hs, text, len, &newlen);
    p = hs_create_element(hs, cp, newlen);
  }

  p->type = ML_DATA;

  return(p);
}

/*
 * hs_create_tag
 */
static MLElement
hs_create_tag(hs, text, len)
MLState hs;
char *text;
size_t len;
{
  MLElement p;

  p = hs_create_element(hs, text, len);
  p->ta = hs_parse_tag(hs, text + 1, len - 2);

  if (p->ta != NULL && p->ta->name != NULL && p->ta->name[0] == '/')
  {
    p->ta->name++;    /* this should be OK because it is not free'd */
    p->type = ML_ENDTAG;
  }
  else p->type = ML_BEGINTAG;

  return(p);
}

/*
 * MLInit
 */
MLState
MLInit(func, closure)
MLElementHandler func;
void *closure;
{
  MLState hs;
  MemPool mp;

  mp = MPCreate();
  hs = (MLState)MPCGet(mp, sizeof(struct MLStateP));
  hs->mp = mp;
  hs->func = func;
  hs->closure = closure;

  hs->tmp = MPCreate();

  hs_push(hs, MLS_START);
  hs_push(hs, MLS_DATA);
  
  return(hs);
}

/*
 * MLDestroy
 */
void
MLDestroy(hs)
MLState hs;
{
  MPDestroy(hs->mp);
  return;
}

/*
 * hs_handle_data
 */
void
hs_handle_data(hs, data, len, dend)
MLState hs;
char *data;
size_t len;
bool dend;
{
  char *cp;
  size_t poff;
  MLElement n;

  if (data == NULL || len == 0) return;

  hs->data = data;
  hs->len = len;

  while (hs->off < len)
  {
    cp = data + hs->off;

    if (IN_STATE(hs, MLS_DATA))
    {
      if (*cp == '<')
      {
	poff = hs_pop(hs);
	if (hs->off - poff > 0)
	{
	  n = hs_create_text(hs, hs->data + poff, hs->off - poff);
	  hs_add_element(hs, n);
	}
	hs_push(hs, MLS_TAG);
      }
      hs->off++;
    }
    else if (IN_STATE(hs, MLS_TAG))
    {
      if (*cp == '<') hs->off++;
      else if (*cp == '>')
      {
        poff = hs_pop(hs);
	n = hs_create_tag(hs, hs->data + poff, hs->off - poff + 1);
        hs_add_element(hs, n);
        hs->off++;
        hs_push(hs, MLS_DATA);
      }
      else if (*cp == '-')
      {
	hs_push(hs, MLS_PSCOMMENT);
	hs->off++;
      }
      else if (*cp == '"')
      {
	hs_push(hs, MLS_AVD);
	hs->off++;
      }
      else if (*cp == '\'')
      {
	hs_push(hs, MLS_AVS);
	hs->off++;
      }
      else hs->off++;
    }
    else if (IN_STATE(hs, MLS_PSCOMMENT))
    {
      hs_pop(hs);
      if (*cp == '-')
      {
	hs_push(hs, MLS_COMMENT);
	hs->off++;
      }
      else if (*cp == '>')
      {
        poff = hs_pop(hs);
	n = hs_create_tag(hs, hs->data + poff, hs->off - poff + 1);
        hs_add_element(hs, n);
        hs->off++;
        hs_push(hs, MLS_DATA);
      }
      else hs->off++;
    }
    else if (IN_STATE(hs, MLS_AVS))
    {
      if (*cp == '\'')
      {
	hs_pop(hs);
	hs->off++;
      }
      else if (*cp == '>')  hs_pop(hs);
      else hs->off++;
    }
    else if (IN_STATE(hs, MLS_AVD))
    {
      if (*cp == '"')
      {
	hs_pop(hs);
	hs->off++;
      }
      else if (*cp == '>')  hs_pop(hs);
      else hs->off++;
    }
    else if (IN_STATE(hs, MLS_COMMENT))
    {
      if (*cp == '-') hs_push(hs, MLS_PECOMMENT);
      hs->off++;
    }
    else if (IN_STATE(hs, MLS_PECOMMENT))
    {
      /* Depends on only COMMENT state pushing PECOMMENT state */
      hs_pop(hs);
      if (*cp == '-')
      {
	hs->off++;
	hs_pop(hs);
      }
      else if (*cp == '>')
      {
        poff = hs_pop(hs);
	n = hs_create_tag(hs, hs->data + poff, hs->off - poff + 1);
        hs_add_element(hs, n);
        hs->off++;
        hs_push(hs, MLS_DATA);
      }
      else hs->off++;
    }
    else
    {
      fprintf (stderr, "Invalid markup parser state.\n");
      hs->off++;
    }
  }

  return;
}

/*
 * MLAddData
 */
void
MLAddData(hs, data, len)
MLState hs;
char *data;
size_t len;
{
  hs_handle_data(hs, data, len, false);
  return;
}

/*
 * MLEndData
 */
void
MLEndData(hs, data, len)
MLState hs;
char *data;
size_t len;
{
  MLElement p;
  size_t poff;

  if (data == NULL || len == 0 || len < hs->off) return;

  hs_handle_data(hs, data, len, true);

  poff = hs_pop(hs);
  if (len - poff > 0)
  {
    if (IN_STATE(hs, MLS_DATA))
    {
      p = hs_create_text(hs, hs->data + poff, len - poff);
    }
    else
    {
      p = hs_create_tag(hs, hs->data + poff, len - poff);
    }
    hs_add_element(hs, p);
  }

  p = (MLElement)MPCGet(hs->mp, sizeof(struct MLElementP));
  p->type = ML_EOF;
  hs_add_element(hs, p);

  MPDestroy(hs->tmp);
  hs->tmp = NULL;

  return;
}

/*
 * MLFindAttribute
 */
char *
MLFindAttribute(p, name)
MLElement p;
char *name;
{
  MLAttribute ta;

  for (ta = p->ta; ta != NULL; ta = ta->next)
  {
    if (strcasecmp(ta->name, name) == 0)
    {
      return(ta->value == NULL ? "":ta->value);
    }
  }

  return(NULL);
}

/*
 * MLGetText
 */
void
MLGetText(p, text, len)
MLElement p;
char **text;
size_t *len;
{
  *text = p->ptext;
  *len = p->plen;

  return;
}

/*
 * MLGetType
 */
MLElementType
MLGetType(p)
MLElement p;
{
  return(p->type);
}

/*
 * MLTagName
 */
char *
MLTagName(p)
MLElement p;
{
  if ((p->type == ML_BEGINTAG || p->type == ML_ENDTAG) && p->ta != NULL)
  {
    return(p->ta->name);
  }
  return(NULL);
}

/*
 * MLCreateTag
 */
MLElement
MLCreateTag(hs, text, len)
MLState hs;
char *text;
size_t len;
{
  return(hs_create_tag(hs, text, len));
}

/*
 * MLAttributeToInt
 */
int
MLAttributeToInt(p, name)
MLElement p;
char *name;
{
  char *value;

  if ((value = MLFindAttribute(p, name)) == NULL) return(-1);

  return(atoi(value));
}

