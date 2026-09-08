/*
 * timeout.c
 *
 * Copyright (C) 1994-1997, John Kilburg <john@cs.unlv.edu>
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

#include "port_after.h"

#include "ChimeraP.h"

struct ChimeraTimeOutP
{
  ChimeraResources cres;
  XtIntervalId iid;
  ChimeraTimeOutProc func;
  void *closure;
};

static void TimeOutHandler _ArgProto((XtPointer, XtIntervalId *));

/*
 * TimeOutCreate
 */  
ChimeraTimeOut
TimeOutCreate(cres, interval, func, closure)
ChimeraResources cres;
unsigned int interval;
ChimeraTimeOutProc func;
void *closure;
{
  ChimeraTimeOut pto;

  if ((pto = (ChimeraTimeOut)GListPop(cres->oldtimeouts)) == NULL)
  {
    pto = (ChimeraTimeOut)MPCGet(cres->mp, sizeof(struct ChimeraTimeOutP));
  }

  pto->cres = cres;
  pto->closure = closure;
  pto->func = func;
  GListAddHead(cres->timeouts, pto);

  pto->iid = XtAppAddTimeOut(pto->cres->appcon,
			     (unsigned long)interval,
			     TimeOutHandler, cres);
  
  return(pto);
}

/*
 * TimeOutHandler
 */
static void
TimeOutHandler(closure, iid)
XtPointer closure;
XtIntervalId *iid;
{
  ChimeraResources cres = (ChimeraResources)closure;
  ChimeraTimeOut pto;

  for (pto = (ChimeraTimeOut)GListGetHead(cres->timeouts); pto != NULL;
       pto = (ChimeraTimeOut)GListGetNext(cres->timeouts))
  {
    if (pto->iid == *iid)
    {
      pto->iid = 0;
      (pto->func)(pto, pto->closure);
      return;
    }
  }

  myassert(false, "Jam!");

  return;
}

/*
 * TimeOutDestroy
 */
void
TimeOutDestroy(pto)
ChimeraTimeOut pto;
{
  GListRemoveItem(pto->cres->timeouts, pto);
  GListAddHead(pto->cres->oldtimeouts, pto);
  if (pto->iid != 0) XtRemoveTimeOut(pto->iid);
  memset(pto, 1, sizeof(struct ChimeraTimeOutP));
  return;
}
