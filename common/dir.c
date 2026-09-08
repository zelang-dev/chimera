/*
 * dir.c
 *
 * Copyright (c) 1996-1997, John Kilburg <john@cs.unlv.edu>
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "port_after.h"

#include "common.h"

/*
 * whack_filename
 */
char *
whack_filename(mp, p)
MemPool mp;
char *p;
{
  char *n = MPStrDup(mp, p);
  size_t last;

  last = strlen(n) - 1;
  if (*(n + last) != '/')
  {
    for (p = n + last; p >= n; p--)
    {
      if (*p == '/')
      {
        *(p + 1) = '\0';
        break;
      }
    }
  }

  return(n);
}

/*
 * compress_path
 *
 * This assumes sizeof(char) == 1 which is probably a really bad idea.
 */
char *
compress_path(mp, c, cwd)
MemPool mp;
char *c;
char *cwd;
{
  char *r;
  char *p, *p1, *p2, *p3;
  char *ps;

  if (*c != '/')
  {
    if (*cwd != '/') return(NULL);
    r = (char *)MPGet(mp, strlen("/") + strlen(c) + strlen(cwd) + 1);
    strcpy(r, cwd);
    strcat(r, "/");
    strcat(r, c);
  }
  else r = MPStrDup(mp, c);
  
  for (p = r; *p != '\0'; )
  {
    if (*p == '/')
    {
      p1 = p + 1;
      if (*p1 == '/')
      {
        memmove(p, p1, strlen(p1) + 1);
        continue;
      }
      else if (*p1 == '.')
      {
        p2 = p1 + 1;
        if (*p2 == '/')
        {
          memmove(p, p2, strlen(p2) + 1);
          continue;
        }
        else if (*p2 == '\0')
        {
          *p1 = '\0';
          return(r);
        }
        else if (*p2 == '.')
        {
          p3 = p2 + 1;
          if (*p3 == '/' || *p3 == '\0')
          {
            for (ps = p - 1; ps >= r; ps--)
            {
              if (*ps == '/') break;
            }
            if (ps < r) ps = r;
            if (*p3 == '/') memmove(ps, p3, strlen(p3) + 1);
            else
            {
              *(ps + 1) = '\0';
              return(r);
            }
            p = ps;
            continue;
          }
        }
      }
    }
    p++;
  }

  return(r);
}
