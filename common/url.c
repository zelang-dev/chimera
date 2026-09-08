/*
 * url.c
 *
 * Copyright (C) 1993-1997, John Kilburg <john@cs.unlv.edu>
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

#include "url.h"

#define URLDELIMS ":/?#"

#ifndef NullString
#define NullString(s)	(s == NULL || *s == '\0')
#endif

static URLParts *URLCreate _ArgProto((MemPool));
static char *resolve_filename _ArgProto((MemPool, char *, char *));

/*
 * URLcmp
 *
 * Return 0 if equal.
 */
int
URLcmp(u1, u2)
URLParts *u1, *u2;
{
  if (strcasecmp(u1->scheme, u2->scheme) == 0 &&
      strcasecmp(u1->hostname, u2->hostname) == 0 &&
      u1->port == u2->port &&
      strcasecmp(u1->filename, u2->filename) == 0)
  {
    return(0);
  }
  return(-1);
}

/*
 * URLEscape
 *
 * Puts escape codes in URLs.  NOT complete.
 */
char *
URLEscape(mp, url, s2p)
MemPool mp;
char *url;
bool s2p;
{
  char *cp;
  char *n, *s;
  static char *hex = "0123456789ABCDEF";

  /*
   * use a bit of memory so i don't have to mess around here
   */
  s = n = (char *)MPGet(mp, strlen(url) * 3 + 2);

  for (cp = url; *cp; cp++, n++)
  {
    if (*cp == ' ' && s2p)
    {
      *n = '+';
    }
    else if (*cp == '+' && s2p)
    {
      *n = '%';
      n++;
      *n = hex[*cp / 16];
      n++;
      *n = hex[*cp % 16];
    }
#ifdef ORIGINAL_CODE
/*
    else if (isalnum(*cp) || strchr("$-_.'(),+!*", *cp))
*/
    else if (strchr("<>\"#{}|\\^~[]`",*cp))
#else
/* My CGI scripts and the Apache daemon say it's closer the first way - djhjr */
    else if (isalnum(*cp) || strchr("$-_.'(),+!*", *cp))
#endif
    {
      *n = *cp;
    }
    else
    {
      *n = '%';
      n++;
      *n = hex[*cp / 16];
      n++;
      *n = hex[*cp % 16];
    }
  }

  *n = '\0';

  return(s);
}

/*
 * UnescapeURL
 *
 * Converts the escape codes (%xx) into actual characters.  NOT complete.
 * Could do everthing in place I guess.
 */
char *
URLUnescape(mp, url)
MemPool mp;
char *url;
{
  char *cp, *n, *s;
  char hex[3];

  s = n = (char *)MPGet(mp, strlen(url) + 2);
  for (cp = url; *cp; cp++, n++)
  {
    if (*cp == '%')
    {
      cp++;
      if (*cp == '%')
      {
	*n = *cp;
      }
      else
      {
	hex[0] = *cp;
	cp++;
	hex[1] = *cp;
	hex[2] = '\0';
	*n = (char)strtol(hex, NULL, 16);
      }
    }
    else if (*cp == '+') *n = ' ';
    else
    {
      *n = *cp;
    }
  }

  *n = '\0';

  return(s);
}

/*
 * URLMakeString
 */
char *
URLMakeString(mp, up, addfrag)
MemPool mp;
URLParts *up;
bool addfrag;
{
  size_t len;
  char *u;
  char *delim;
  char *delim2;
  char *filename;
  char *hostname;
  char *scheme;
  char *delim3;
  char *fragment;

  if (NullString(up->scheme)) scheme = "file";
  else scheme = up->scheme;
  
  if (NullString(up->hostname))
  {
    delim = "";
    hostname = "";
  }
  else
  {
    delim = "//";
    hostname = up->hostname;
  }

  if (NullString(up->filename)) filename = "/";
  else filename = up->filename;

  delim2 = "";

  if (up->fragment != NULL && addfrag)
  {
    fragment = up->fragment;
    delim3 = "#";
  }
  else
  {
    fragment = "";
    delim3 = "";
  }

  len = strlen(scheme) + strlen(hostname) + strlen(filename) +
        strlen(delim) + strlen(fragment) + 11;
  u = (char *)MPGet(mp, len + 1);
  if (up->port == 0)
  {
    snprintf (u, len, "%s:%s%s%s%s%s%s", scheme, delim, hostname, delim2,
	      filename, delim3, fragment);
  }
  else
  {
    snprintf (u, len, "%s:%s%s:%d%s%s%s%s", scheme, delim, hostname, up->port,
	      delim2, filename, delim3, fragment);
  }

  return(u);
}

/*
 * URLCreate
 *
 * Allocate URLParts and initialize to NULLs
 */
static URLParts *
URLCreate(mp)
MemPool mp;
{
  URLParts *up;

  up = (URLParts *)MPCGet(mp, sizeof(URLParts));

  return(up);
}

/*
 * resolve_filename
 *
 * I'm not sure this is much better than the original.
 */
static char *
resolve_filename(mp, c, p)
MemPool mp;
char *c, *p;
{
  char *r;
  char *t;
  MemPool tmp;

  /*
   * If current is an absolute path then use it otherwise
   * build an absolute path using the parent as a reference.
   */
  if (c == NULL || c[0] == '/') r = MPStrDup(mp, c);
  else if (c[0] == '~')
  {
    r = MPGet(mp, strlen(c) + 2);
    r[0] = '/';
    strcpy(r + 1, c);
  }
  else
  {
    tmp = MPCreate();
    if (p == NULL || p[0] != '/') p = "/";
    else p = whack_filename(tmp, p);
    t = compress_path(tmp, c, p);
    if (t == NULL) r = MPStrDup(mp, "/");
    else r = MPStrDup(mp, t);
    MPDestroy(tmp);
  }

  return(r);
}

/*
 * URLResolve
 *
 * c - current
 * p - parent
 * r - result
 */
URLParts *
URLResolve(mp, c, p)
MemPool mp;
URLParts *c, *p;
{
  URLParts *r;

  /*
   * If the protocols are different then just return the original with
   * some empty fields filled in.
   */
  if (c->scheme != NULL && p->scheme != NULL &&
      strcasecmp(c->scheme, p->scheme) != 0)
  {
    r = URLDup(mp, c);
    if (r->hostname == NULL) r->hostname = MPStrDup(mp, "localhost");
    r->filename = resolve_filename(mp, c->filename, p->filename);
    return(r);
  }

  r = URLCreate(mp);

  /*
   * If current has a protocol then use it, otherwise
   * use the parent's protocol.  If the parent doesn't have a protocol for
   * some reason then use "file".
   */
  if (c->scheme == NULL)
  {
    if (p->scheme != NULL) r->scheme = MPStrDup(mp, p->scheme);
    else r->scheme = MPStrDup(mp, "file");
  }
  else r->scheme = MPStrDup(mp, c->scheme);

  /*
   * If current has a hostname then use it, otherwise
   * use the parent's hostname.  If neither has a hostname then
   * fallback to "localhost".
   */
  if (c->hostname == NULL)
  {
    if (p->hostname != NULL)
    {
      r->hostname = MPStrDup(mp, p->hostname);
      r->port = p->port;
    }
    else
    {
      r->hostname = MPStrDup(mp, "localhost"); /* fallback */
      r->port = 0;
    }
  }
  else
  {
    r->hostname = MPStrDup(mp, c->hostname);
    r->port = c->port;
  }

  r->filename = resolve_filename(mp, c->filename, p->filename);

  /*
   * Copy misc. fields.
   */
  r->username = MPStrDup(mp, c->username);
  r->password = MPStrDup(mp, c->password);
  r->fragment = MPStrDup(mp, c->fragment);

  return(r);
}

URLParts *
URLDup(mp, up)
MemPool mp;
URLParts *up;
{
  URLParts *dp;

  dp = URLCreate(mp);
  dp->scheme = MPStrDup(mp, up->scheme);
  dp->hostname = MPStrDup(mp, up->hostname);
  dp->port = up->port;

  dp->filename = up->filename != NULL ?
      MPStrDup(mp, up->filename):MPStrDup(mp, "/");

  dp->fragment = MPStrDup(mp, up->fragment);

  dp->username = MPStrDup(mp, up->username);
  dp->password = MPStrDup(mp, up->password);

  return(dp);
}

/*
 * URLParse
 *
 * Turns a URL into a URLParts structure
 *
 * The good stuff was written by Rob May <robert.may@rd.eng.bbc.co.uk>
 * and heavily mangled/modified by john to suite his own weird style.
 */
URLParts *
URLParse(mp, url)
MemPool mp;
char *url;
{
  URLParts *up;
  char *start;
  char *colon, *slash, *fslash;
  char *pound; /* link pound (#) sign */
  char *at; /* username/password @ */
  char *ucolon; /* username colon */
  char *pcolon; /* port number colon */

  up = URLCreate(mp);

  /* skip leading white-space (if any)*/
  for (start = url; isspace8(*start); start++)
      ;

  /*
   * Look for indication of a scheme.
   */
  colon = strchr(start, ':');

  /*
   * Search for characters that indicate the beginning of the
   * path/params/query/fragment part.
   */
  slash = strchr(start, '/');
  if (slash == NULL) slash = strchr(start, ';');
  if (slash == NULL) slash = strchr(start, '?');
  if (slash == NULL) slash = strchr(start, '#');

  /*
   * Check to see if there is a scheme.  There is a scheme only if
   * all other separators appear after the colon.
   */
  if (colon != NULL && (slash == NULL || colon < slash))
  {
    up->scheme = MPGet(mp, colon - start + 1);
    strncpy(up->scheme, start, colon - start);
    up->scheme[colon - start] = '\0';
  }

  /*
   * If there is a slash then sort out the hostname and filename.
   * If there is no slash then there is no hostname but there is a
   * filename.
   */
  if (slash != NULL)
  {
    /*
     * Check for leading //. If its there then there is a host string.
     */
    if ((*(slash + 1) == '/') && ((colon == NULL && slash == start) ||
	(colon != NULL && slash == colon + 1)))
    {
      /*
       * Check for filename at end of host string.
       */
      slash += 2;
      if ((fslash = strchr(slash, '/')) != NULL)
      {
	up->hostname = MPGet(mp, fslash - slash + 1);
	strncpy(up->hostname, slash, fslash - slash);
	up->hostname[fslash - slash] = '\0';
	up->filename = MPStrDup(mp, fslash);
      }
      else
      { /* there is no filename */
	up->hostname = MPStrDup(mp, slash);
      }
    }
    else
    {
      /*
       * the rest is a filename because there is no // or it appears
       * after other characters
       */
      if (colon != NULL && colon < slash) 
      {
	up->filename = MPStrDup(mp, colon + 1);
      }
      else up->filename = MPStrDup(mp, start);
    }
  }
  else
  {
    /*
     * No slashes at all so the rest must be a filename.
     */
    if (colon == NULL) up->filename = MPStrDup(mp, start);
    else up->filename = MPStrDup(mp, colon + 1);
  }

  /*
   * If there is a host string then divide it into
   * username:password@hostname:port as needed.
   */
  if (up->hostname != NULL)
  {
    /*
     * Look for username:password.
     */
    if ((at = strchr(up->hostname, '@')) != NULL)
    {
      char *mumble;

      up->username = MPGet(mp, at - up->hostname + 1);
      strncpy(up->username, up->hostname, at - up->hostname);
      up->username[at - up->hostname] = '\0';

      mumble = MPStrDup(mp, at + 1);
      up->hostname = mumble;

      if ((ucolon = strchr(up->username, ':')) != NULL)
      {
	up->password = MPStrDup(mp, ucolon + 1);
	*ucolon = '\0';
      }
    }

    /*
     * Grab the port.
     */
    if ((pcolon = strchr(up->hostname, ':')) != NULL)
    {
      up->port = atoi(pcolon + 1);
      *pcolon = '\0';
    }
  }

  /*
   * Check the filename for a '#foo' string.
   */
  if (up->filename != NULL)
  {
    if ((pound = strchr(up->filename, '#')) != NULL)
    {
      *pound = '\0';
      up->fragment = MPStrDup(mp, pound + 1);

      if (strlen(up->filename) == 0) up->filename = NULL;
    }
  }

  return(up);
}


/*
 * URLIsAbsolute
 */
bool
URLIsAbsolute(up)
URLParts *up;
{
  if (up->scheme == NULL) return(false);
  return(true);
}

/*
 * URLBaseFilename
 */
char *
URLBaseFilename(mp, up)
MemPool mp;
URLParts *up;
{
  char *cp;

  if (up->filename == NULL) return(NULL);

  for (cp = up->filename + strlen(up->filename) - 1;
       cp >= up->filename; cp--)
  {
    if (*cp == '/') break;
  }
  cp++;
  if (*cp == '\0') return(NULL);

  return(MPStrDup(mp, cp));
}

/*
 * URLGetScheme
 */
char *
URLGetScheme(mp, url)
MemPool mp;
char *url;
{
  char *cp, *dp;
  char *r;

  for (cp = url; *cp != '\0'; cp++)
  {
    for (dp = URLDELIMS; *dp != '\0'; dp++)
    {
      if (*cp == *dp)
      {
	if (*cp == ':')
	{
	  r = (char *)MPCGet(mp, cp - url + 1);
	  strncpy(r, url, cp - url);
	  return(r);
	}
	return(NULL);
      }
    }
  }

  return(NULL);
}
