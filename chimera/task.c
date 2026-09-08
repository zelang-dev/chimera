/*
 * task.c
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

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>

#include "port_after.h"

#include "ChimeraP.h"

struct ChimeraTaskP
{
  ChimeraTaskProc func;
  void            *closure;
  int             line;
  char            *file;
  bool            removed;
};

struct ChimeraSchedulerP
{
  MemPool         mp;
  GList           oldtasks;
  GList           tasks;
  XtWorkProcId    wpid;
};

static Boolean ScheduleTaskProc _ArgProto((XtPointer));

/*
 * ScheduleTaskProc
 */
static Boolean
ScheduleTaskProc(closure)
XtPointer closure;
{
  ChimeraResources cres = (ChimeraResources)closure;
  ChimeraScheduler cs = cres->cs;
  ChimeraTask wt;

  cs->wpid = 0;

  while ((wt = (ChimeraTask)GListPop(cs->tasks)) != NULL)
  {
    if (!wt->removed)
    {
      if (cres->printTaskInfo)
      {
	fprintf (stderr, "Task Called %s, %d\n", wt->file, wt->line);
      }
      CMethod(wt->func)(wt->closure);
    }
    GListAddTail(cs->oldtasks, wt);
    memset(wt, 1, sizeof(struct ChimeraTaskP));
  }

  return(True);
}

/*
 * TaskSchedule
 */
ChimeraTask
TaskScheduleX(cres, func, closure, line, file)
ChimeraResources cres;
ChimeraTaskProc func;
void *closure;
int line;
char *file;
{
  ChimeraTask wt;
  char *detail;

  if ((wt = (ChimeraTask)GListPop(cres->cs->oldtasks)) == NULL)
  {
    detail = "New";
    wt = (ChimeraTask)MPGet(cres->mp, sizeof(struct ChimeraTaskP));
  }
  else detail = "Reused";

  wt->func = func;
  wt->closure = closure;
  wt->line = line;
  wt->file = file;
  wt->removed = false;

  GListAddTail(cres->cs->tasks, wt);

  if (cres->cs->wpid == 0)
  {
    cres->cs->wpid = XtAppAddWorkProc(cres->appcon, ScheduleTaskProc, cres);
    myassert(cres->cs->wpid != 0, "Bad assumption about XtAppAddWorkProc");
  }

  if (cres->printTaskInfo)
  {
    fprintf (stderr, "Task Created %s, %s, %d\n", detail, file, line);
  }

  return(wt);
}

/*
 * TaskRemove
 */
void
TaskRemove(cres, wt)
ChimeraResources cres;
ChimeraTask wt;
{
  ChimeraTask c;
  GList list;
  char *detail = "notfound";

  list = cres->cs->tasks;
  for (c = (ChimeraTask)GListGetHead(list); c != NULL;
       c = (ChimeraTask)GListGetNext(list))
  {
    if (c == wt)
    {
      c->removed = true;
      detail = "found";
      break;
    }
  }

  if (cres->printTaskInfo)
  {
    fprintf (stderr, "Task Remove %s, %s, %d\n", detail, wt->file, wt->line);
  }

  return;
}

/*
 * SchedulerCreate
 */
ChimeraScheduler
SchedulerCreate()
{
  ChimeraScheduler cs;
  MemPool mp;

  mp = MPCreate();
  cs = (ChimeraScheduler)MPCGet(mp, sizeof(struct ChimeraSchedulerP));
  cs->mp = mp;
  cs->oldtasks = GListCreateX(mp);
  cs->tasks = GListCreateX(mp);

  return(cs);
}
