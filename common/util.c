/*
 * util.c
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
#include <pwd.h>
#include <errno.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <time.h>

#include "port_after.h"

#include "common.h"

/*
 * FixPath
 *
 * The only thing this does right now is to handle the '~' stuff.
 */
char *
FixPath(mp, filename)
MemPool mp;
char *filename;
{
  struct passwd *p;
  char *cp, *cp2;
  char username[BUFSIZ];
  char *fname;
  char *home;
  char *newfilename;

  if (filename[0] == '~') fname = filename;
  else if (filename[0] == '/' && filename[1] == '~') fname = filename + 1;
  else fname = NULL;

  if (fname != NULL)
  {
    if (fname[1] == '/')
    {
      if ((home = getenv("HOME")) == NULL) return(NULL);
      cp = fname + 1;
    }
    else
    {
      for (cp = fname + 1, cp2 = username; *cp && *cp != '/'; cp++, cp2++)
      {
	*cp2 = *cp;
      }
      *cp2 = '\0';

      p = getpwnam(username);
      if (p == NULL) return(NULL);
      home = p->pw_dir;
    }

    newfilename = (char *)MPGet(mp, strlen(home) + strlen(cp) + 1);
    strcpy(newfilename, home);
    strcat(newfilename, cp);
  }
  else
  {
    newfilename = MPStrDup(mp, filename);
  }

  return(newfilename);
}

/*
 * mystrtok
 *
 */
char *
mystrtok(s, c, cdr)
char *s;
size_t c;
char **cdr;
{
  char *cp, *cp2;
  static char str[BUFSIZ];

  if (s == NULL) return(NULL);

  for (cp = s, cp2 = str; ; cp++)
  {
    if (*cp == '\0') break;
    else if (*cp == c)
    {
      for (cp++; *cp == c; )
      {
	cp++;
      }
      break;
    }
    else *cp2++ = *cp;
    if (cp2 == str + BUFSIZ - 1) break;
  }

  *cp2 = '\0';

  if (*cp == '\0') *cdr = NULL;
  else *cdr = cp;

  return(str);
}

/*
 * GetBaseFilename
 */
char *
GetBaseFilename(path)
char *path;
{
  char *cp;

  if (path == NULL) return(NULL);
  for (cp = path + strlen(path) - 1; cp >= path; cp--)
  {
    if (*cp == '/') break;
  }
  cp++;
  if (*cp == '\0') return(NULL);
  return(cp);
}

/*
 * xmyassert
 */
void
xmyassert(isok, msg, file, line)
bool isok;
char *msg;
char *file;
int line;
{
  if (!isok)
  {
    fprintf (stderr, "%s: %d\n", file, line);
    fprintf (stderr, "%s\n", msg);
    fflush(stderr);
    abort();
  }
  return;
}

#ifndef P_tmpdir
#define P_tmpdir "/tmp"
#endif

#ifndef L_tmpnam
#define L_tmpnam 256
#endif

/*
 * mytmpnam
 */
char *
mytmpnam(mp)
MemPool mp;
{
  char *n;

  n = (char *)MPGet(mp, L_tmpnam + 1);
  snprintf (n, L_tmpnam, "%s/%d%ld",
	    P_tmpdir, getpid(), (long)time(NULL));
  return(n);
}
