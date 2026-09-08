/*
 * mime.c
 *
 * Copyright (c) 1997, John Kilburg <john@cs.unlv.edu>
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "port_after.h"

#include "common.h"
#include "mime.h"

typedef struct MIMEFieldP *MIMEField;
typedef struct MIMEParamP *MIMEParam;

/*
 * MIMEish parsing thing.
 *
 * Tries to deal with stuff of the form:
 *
 * field-name: field-value; param[=value]; param[=value]; ...
 */

struct MIMEHeaderP
{
  GList list;       /* list of MIMEField */
  MemPool mp;
  size_t i;
  size_t len;
  bool data_found;
};

struct MIMEFieldP
{
  char *name;
  char *value;
  GList list;       /* list of param */
};

struct MIMEParamP
{
  char *name;
  char *value;
};

static MIMEParam parse_param _ArgProto((char *));
static MIMEField parse_line _ArgProto((MemPool, char *));

MIMEHeader
MIMECreateHeader()
{
  MemPool mp;
  MIMEHeader mh;

  mp = MPCreate();
  mh = MPCGet(mp, sizeof(struct MIMEHeaderP));
  mh->mp = mp;
  mh->list = GListCreateX(mp);
  mh->data_found = false;

  return(mh);
}

static MIMEParam
parse_param(line)
char *line;
{
  MIMEParam p = NULL;

  return(p);
}

static MIMEField
parse_line(mp, line)
MemPool mp;
char *line;
{
  MIMEField mf = NULL;
  MIMEParam param;
  char *cp;
  char *ps = NULL;
  bool inquote = false;
  char *t;

  for (cp = line; *cp != '\0'; cp++)
  {
    if (mf != NULL)
    {
      if (inquote)
      {
	if (*cp == '"') inquote = false;
      }
      else
      {
	if (*cp == '"') inquote = true;
	else if (*cp == ';' || *cp == '\0')
	{
	  while (isspace8(*ps) && ps < cp) ps++;
	  if (ps < cp)
	  {
	    t = (char *)MPGet(mp, cp - ps + 1);
	    strncpy(t, ps, cp - ps);
	    t[cp - ps] = '\0';
	  
	    if (mf->value == NULL) mf->value = t;
	    else
	    {
	      if (mf->list == NULL) mf->list = GListCreateX(mp);
	      if ((param = parse_param(t)) != NULL)
	      {
		GListAddTail(mf->list, param);
	      }
	    }
	  }
	  ps = cp + 1;
	}
      }
    }
    else if (*cp == ':')
    {
      mf = (MIMEField)MPCGet(mp, sizeof(struct MIMEFieldP));
      mf->name = (char *)MPGet(mp, cp - line + 1);
      strncpy(mf->name, line, cp - line);
      mf->name[cp - line] = '\0';
      ps = cp + 1;
    }
  }

  /*
   * Catch the value or parameter that is terminated with \0
   */
  if (mf != NULL && ps < cp)
  {
    while (isspace8(*ps) && ps < cp) ps++;
    if (ps < cp)
    {
      t = (char *)MPGet(mp, cp - ps + 1);
      strncpy(t, ps, cp - ps);
      t[cp - ps] = '\0';

      if (mf->value == NULL) mf->value = t;
      else
      {
	if (mf->list == NULL) mf->list = GListCreateX(mp);
	if ((param = parse_param(t)) != NULL)
	{
	  GListAddTail(mf->list, param);
	}
      }
    }
  }

  return(mf);
}

/*
 * MIMEParseBuffer
 *
 * This is the entry point for code to parse a buffer that contains
 * a MIME-like thing chunk of chars.
 */
int
MIMEParseBuffer(mh, buf, len)
MIMEHeader mh;
char *buf;
size_t len;
{
  MIMEField mf;
  size_t j;
  char *cp, *pcp, *ls, *le;
  char *line;
  size_t usedlen;
  size_t linelen;

  myassert(mh->data_found, "Must call MIMEFindData before MIMEParseBuffer");

  usedlen = 0;
  linelen = BUFSIZ;
  line = (char *)alloc_mem(linelen);

  le = NULL;
  ls = cp = buf;
  pcp = "";
  for (j = 0; j < len; j++, pcp = cp, cp++)
  {
    if (*cp != '\n') continue;

    /*
     * If line ends with \r\n then shift EOL back one character so that
     * \r doesn't get included in the line.
     */
    if (*pcp == '\r') le = pcp;
    else le = cp;
    
    /*
     * If the line start is the same as the line end then it must be
     * a blank line and its time to bail out.
     */
    if (ls == le) break;

    /*
     * If the line doesn't begin with a space character then flush the
     * previous line (if there was a previous) as a complete field.
     * Otherwise remove the blank space from the continuation line.
     */
    if (!isspace8(*ls))
    {
      if (usedlen > 0)
      {
	if ((mf = parse_line(mh->mp, line)) != NULL)
	{
	  GListAddTail(mh->list, mf);
	}
	usedlen = 0;
      }
    }
    else
    {
      while(isspace8(*ls) && ls < le) ls++;
    }

    /*
     * If there is something besides whitespace then stick it in the line
     * buffer.
     */
    if (ls < le)
    {
      if (linelen < usedlen + (le - ls))
      {
	linelen += ((le - ls) / BUFSIZ + 1) * BUFSIZ;
	line = (char *)realloc(line, linelen);
      }
      strncpy(line + usedlen, ls, le - ls);
      usedlen += le - ls;
      line[usedlen] = '\0';
    }
    
    /*
     * Start the next line after the \n
     */
    ls = cp + 1;
  }

  if (usedlen > 0)
  {
    if ((mf = parse_line(mh->mp, line)) != NULL)
    {
      GListAddTail(mh->list, mf);
    }
    usedlen = 0;
  }

  free_mem(line);

  return(0);
}

/*
 * MIMEDestroyHeader
 */
void
MIMEDestroyHeader(mh)
MIMEHeader mh;
{
  MPDestroy(mh->mp);
  return;
}

/*
 * MIMEFindData
 *
 * Searches for \n\n and \r\n\r\n in a string.  Returns the offset after
 * the pattern;
 */
int
MIMEFindData(mh, data, len, doff)
MIMEHeader mh;
char *data;
size_t len;
size_t *doff;
{
  char *cp;
  char n[4];
  size_t j;

  myassert(mh->i < len, "MIMEFindData: Inconsistent buffer sizes");

  memset(n, 0, sizeof(n));
  j = 0;
  for (cp = data + mh->i; mh->i < len; mh->i++, j++, cp++)
  {
    n[j % 4] = *cp;

    /*
     * Mmmm fun.  If there are two or more characters and the current
     * and previous character are '\n' (i.e. blank line) then the end
     * of the header has been reached.  If there are 4 or more characters
     * and the sequence of characters (in reverse order) is \n\r\n\r then
     * the end of header has been reached.
     */
    if (j >= 2 && n[j % 4] == '\n' &&
	(n[(j - 1) % 4] == '\n' ||
	(j >= 4 && n[(j - 1) % 4] == '\r' && n[(j - 2) % 4] == '\n' &&
	 n[(j - 3) % 4] == '\r')))
    {
      mh->data_found = true;
      *doff = mh->i + 1;
      return(0);
    }
  }

  return(-1);
}

/*
 * MIMEGetField
 */
int
MIMEGetField(mh, name, value)
MIMEHeader mh;
char *name;
char **value;
{
  MIMEField f;

  for (f = (MIMEField)GListGetHead(mh->list); f != NULL;
       f = (MIMEField)GListGetNext(mh->list))
  {
    if (strlen(f->name) == strlen(name) && strcasecmp(f->name, name) == 0)
    {
      *value = f->value;
      return(0);
    }
  }

  return(-1);
}

/*
 * MIMEAddLine
 *
 * Adds a field to a header.  Parses out parameters into structures.
 */
void
MIMEAddLine(mh, line)
MIMEHeader mh;
char *line;
{
  MIMEField f;
  
  if ((f = parse_line(mh->mp, line)) != NULL)
  {
    GListAddTail(mh->list, f);
  }
    
  return;
}

/*
 * MIMEAddField
 */
void
MIMEAddField(mh, name, value)
MIMEHeader mh;
char *name;
char *value;
{
  MIMEField mf;
  mf = (MIMEField)MPCGet(mh->mp, sizeof(struct MIMEFieldP));
  mf->name = MPStrDup(mh->mp, name);
  mf->value = MPStrDup(mh->mp, value);
  GListAddTail(mh->list, mf);
  return;
}

/*
 * MIMEWriteHeader
 *
 * This doesn't break long lines.
 */
int
MIMEWriteHeader(mh, fp)
MIMEHeader mh;
FILE *fp;
{
  MIMEField mf;
  MIMEParam param;

  for (mf = (MIMEField)GListGetHead(mh->list); mf != NULL;
       mf = (MIMEField)GListGetNext(mh->list))
  {
    fprintf (fp, "%s: %s", mf->name, mf->value);
    if (mf->list != NULL)
    {
      for (param = (MIMEParam)GListGetHead(mf->list); param != NULL;
	   param = (MIMEParam)GListGetNext(mf->list))
      {
	fprintf (fp, "; %s", param->name);
	if (param->value != NULL) fprintf (fp, "=%s", param->value);
      }
    }
    fprintf (fp, "\n");
  }
  fprintf (fp, "\n");

  return(0);
}
