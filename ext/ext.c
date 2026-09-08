/*
 * ext.c
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <signal.h>
#include <errno.h>

#include "port_after.h"

#include "Chimera.h"

typedef struct
{
  char *use;
  char *incontent;
  char *outcontent;
  char *command;
} ConvertEntry;

typedef struct
{
  MemPool mp;
  ChimeraSink wp;
  ChimeraRender wn;
  ChimeraGUI wd;
  ChimeraRender own;
  ChimeraRenderHooks *orh;
  ConvertEntry *c;
  int fd[2];
  size_t i;
} ExtInfo;

typedef struct
{
  MemPool mp;
  GList list;
} ExtModuleInfo;

static void ExtDestroy _ArgProto((void *));
static void *ExtInit _ArgProto((ChimeraRender, void *));
static void ExtAdd _ArgProto((void *));
static void ExtEnd _ArgProto((void *));
static void ExtCancel _ArgProto((void *));
static GList ReadConvertFiles _ArgProto((MemPool, char *));

/*
 * ExtDestroy
 */
static void
ExtDestroy(closure)
void *closure;
{
  ExtInfo *ei = (ExtInfo *)closure;
  MPDestroy(ei->mp);
  return;
}

/*
 * ExtAdd
 */
static void
ExtAdd(closure)
void *closure;
{
  ExtInfo *ei = (ExtInfo *)closure;
  byte *data;
  size_t len;
  GList mimelist;
  ssize_t rval;

  SinkGetData(ei->wp, &data, &len, &mimelist);

  if (len <= ei->i) return;

  if ((rval = write(ei->fd[0], data + ei->i, len - ei->i)) <= 0)
  {
    perror("ext write add");
    return;
  }

  ei->i += rval;

  return;
}

/*
 * ExtEnd
 */
static void
ExtEnd(closure)
void *closure;
{
  ExtInfo *ei = (ExtInfo *)closure;
  byte *data;
  size_t len;
  GList mimelist;
  ssize_t rval;
  char buffer[BUFSIZ];

  SinkGetData(ei->wp, &data, &len, &mimelist);

  while (len > ei->i)
  {
    if ((rval = write(ei->fd[0], data + ei->i, len - ei->i)) <= 0)
    {
      perror("ext write end");
      return;
    }
    ei->i += rval;
  }

  close(ei->fd[0]);

/*
  while (read(ei->fd[1], buffer, sizeof(buffer)) > 0)
      ;
*/

  close(ei->fd[1]);

  return;
}

/*
 * ExtCancel
 */
static void
ExtCancel(closure)
void *closure;
{
  return;
}

/*
 * ExtInit
 */
static void *
ExtInit(wn, closure)
ChimeraRender wn;
void *closure;
{
  SinkData wp;
  char *content;
  ConvertEntry *c;
  ExtModuleInfo *emi = (ExtModuleInfo *)closure;
  ExtInfo *ei;
  MemPool mp;
  ChimeraRenderHooks *orh;

  wp = RenderToSink(wn);
  content = SinkGetInfo(wp, "content-type");

  for (c = (ConvertEntry *)GListGetHead(emi->list); c != NULL;
       c = (ConvertEntry *)GListGetNext(emi->list))
  {
    if (strcasecmp(content, c->incontent) == 0) break;
  }

  if (c == NULL) return(NULL);

/*
  orh = WWWGetRenderHooks(WWWGetRenderContext(wn), c->outcontent);
  if (orh == NULL) return(NULL);
*/

  mp = MPCreate();
  ei = (ExtInfo *)MPCGet(mp, sizeof(ExtInfo));
  ei->mp = mp;
  ei->c = c;
  ei->wd = RenderToGUI(wn);
  ei->wp = wp;
  ei->wn = wn;
  ei->orh = orh;

  if (PipeCommand(c->command, ei->fd) == -1)
  {
    MPDestroy(mp);
    return(NULL);
  }

  return(ei);
}

/*
 * ReadConvertFiles
 *
 * Reads in the convert entries in a list of files separated by colons.
 * For example,
 *
 * ~/.chimera_convert:~john/lib/convert:/local/infosys/lib/convert
 */
GList
ReadConvertFiles(mp, filelist)
MemPool mp;
char *filelist;
{
  ConvertEntry *c;
  char *f;
  char *filename;
  char buffer[BUFSIZ];
  char use[BUFSIZ];
  char incontent[BUFSIZ];
  char outcontent[BUFSIZ];
  char command[BUFSIZ];
  FILE *fp;
  GList list;

  list = GListCreateX(mp);

  f = filelist;
  while ((filename = mystrtok(f, ':', &f)) != NULL)
  {
    filename = FixPath(mp, filename);
    if (filename == NULL) continue;

    fp = fopen(filename, "r");
    if (fp == NULL) continue;

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
      if (buffer[0] == '#' || buffer[0] == '\n') continue;

      if (sscanf(buffer, "%s %s %s %[^\n]",
                 use, incontent, outcontent, command) == 4)
      {
        c = (ConvertEntry *)MPCGet(mp, sizeof(ConvertEntry));
        c->use = MPStrDup(mp, use);
        c->incontent = MPStrDup(mp, incontent);
        c->outcontent = MPStrDup(mp, outcontent);
        c->command = strcasecmp(command, "none") == 0 ?
            NULL:MPStrDup(mp, command);

	GListAddTail(list, c);
      }
    }

    fclose(fp);
  }

  return(list);
}


void
InitModule_Ext(cres)
ChimeraResources cres;
{
  ChimeraRenderHooks rh;
  char *clist;
  ExtModuleInfo *emi;
  GList list;
  MemPool mp;
  ConvertEntry *c;

  if ((clist = ResourceGetString(cres, "convert.convertFiles")) == NULL)
  {
    clist = "~/.chimera/convert";
  }

  mp = MPCreate();

  list = ReadConvertFiles(mp, clist);
  if (GListEmpty(list))
  {
    MPDestroy(mp);
    return;
  }

  emi = (ExtModuleInfo *)MPCGet(mp, sizeof(ExtModuleInfo));
  emi->mp = mp;
  emi->list = list;

  for (c = (ConvertEntry *)GListGetHead(list); c != NULL;
       c = (ConvertEntry *)GListGetNext(list))
  {
    memset(&rh, 0, sizeof(ChimeraRenderHooks));
    rh.content = c->incontent;
    rh.class_context = emi;
    rh.class_destroy = ExtDestroy;
    rh.init = ExtInit;
    rh.add = ExtAdd;
    rh.end = ExtEnd;
    rh.destroy = ExtDestroy;
    rh.cancel = ExtCancel;
    RenderAddHooks(cres, &rh);
  }

  return;
}
