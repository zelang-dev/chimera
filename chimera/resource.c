/*
 * resource.c
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>

#include "port_after.h"

#include "ChimeraP.h"

int
ResourceAddString(cres, str)
ChimeraResources cres;
char *str;
{
  XrmDatabase db;

  db = XrmGetStringDatabase(str);
  if (cres->db != NULL) XrmMergeDatabases(db, &(cres->db));
  else cres->db = db;

  if (cres->db == NULL) return(-1);

  return(0);
}

int
ResourceAddFile(cres, file)
ChimeraResources cres;
char *file;
{
  char *dbfile;
  MemPool mp;
  XrmDatabase db;

  if (file == NULL) return(-1);

  mp = MPCreate();
  if ((dbfile = FixPath(mp, file)) == NULL)
  {
    MPDestroy(mp);
    return(-1);
  }

  db = XrmGetFileDatabase(dbfile);
  if (cres->db != NULL) XrmMergeDatabases(db, &(cres->db));
  else cres->db = db;

  MPDestroy(mp);

  if (cres->db == NULL) return(-1);

  return(0);
}

/*
 * ResourceGetString
 */
char *
ResourceGetString(cres, name)
ChimeraResources cres;
char *name;
{
  char *type;
  XrmValue v;

  if (cres->db == NULL) return(NULL);
  if (!XrmGetResource(cres->db, name, XtNstring, &type, &v)) return(NULL);
  return(v.addr);
}

/*
 * ResourceGetFilename
 */
char *
ResourceGetFilename(cres, mp, name)
ChimeraResources cres;
MemPool mp;
char *name;
{
  char *value;

  if ((value = ResourceGetString(cres, name)) == NULL) return(NULL);
  return(FixPath(mp, value));
}

/*
 * ResourceGetBool
 */
char *
ResourceGetBool(cres, name, value)
ChimeraResources cres;
char *name;
bool *value;
{
  char *str;

  if ((str = ResourceGetString(cres, name)) == NULL) return(NULL);

  *value = false;
  if (strncasecmp("true", str, 4) == 0) *value = true;
  if (atoi(str) > 0) *value = true;

  return(str);
}

/*
 * ResourceGetInt
 */
char *
ResourceGetInt(cres, name, value)
ChimeraResources cres;
char *name;
int *value;
{
  char *str;

  if ((str = ResourceGetString(cres, name)) == NULL) return(NULL);

  *value = atoi(str);

  return(str);
}

/*
 * ResourceGetUInt
 */
char *
ResourceGetUInt(cres, name, value)
ChimeraResources cres;
char *name;
unsigned int *value;
{
  char *str;
  int bla;

  if ((str = ResourceGetString(cres, name)) == NULL) return(NULL);

  bla = atoi(str);
  if (bla < 0) bla = 0;
  *value = bla;

  return(str);
}
