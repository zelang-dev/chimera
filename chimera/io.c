/*
 * io.c
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
#include <time.h>
#include <errno.h>
#ifdef __QNX__
#include <ioctl.h>
#include <unix.h> /* for O_NDELAY/O_NONBLOCK, gethostname() */
#define FNDELAY O_NDELAY
#else
#ifdef __EMX__
#define FNDELAY O_NDELAY
#endif
#include <fcntl.h>
#endif
#if (defined(SYSV) || defined(SVR4)) && (defined(sun) || defined(hpux) || defined(_nec_ews_svr4))
#include <sys/file.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <X11/IntrinsicP.h>

#include "port_after.h"

#include "ChimeraP.h"

#include "ChimeraStream.h"

struct ChimeraStreamP
{
  bool                  destroyed;
  MemPool               mp;
  ChimeraResources      cres;
  int                   s;
  int                   as;
  unsigned long         addr;
  int                   port;
  bool                  bound;
  bool                  accepted;
  
  /* read callback */
  ChimeraStreamCallback rdfunc;
  void                  *rdclosure;
  byte                  *rdb;
  size_t                rdlen;
  XtInputId             rdid;

  /* write callback */
  ChimeraStreamCallback wrfunc;
  void                  *wrclosure;
  byte                  *wrb;
  size_t                wrmax;
  size_t                wri;
  XtInputId             wrid;
};

static void WriteStreamHandler _ArgProto((XtPointer, int *, XtInputId *));
static void ReadStreamHandler _ArgProto((XtPointer, int *, XtInputId *));

/*
 * WriteStreamHandler
 */
static void
WriteStreamHandler(cldata, netfd, xid)
XtPointer cldata;
int *netfd;
XtInputId *xid;
{
  ChimeraStream ps = (ChimeraStream)cldata;
  ssize_t wlen;
  int s;

  if (ps->bound) s = ps->as;
  else s = ps->s;

  wlen = write(s, ps->wrb + ps->wri, ps->wrmax - ps->wri);
  if (wlen < 0)
  {
    if (errno != EWOULDBLOCK)
    {
      XtRemoveInput(ps->wrid);
      ps->wrid = 0;
      CMethod(ps->wrfunc)(ps, -1, ps->wrclosure);
    }
  }
  else
  {
    ps->wri += wlen;
    if (ps->wri == ps->wrmax)
    {
      XtRemoveInput(ps->wrid);
      ps->wrid = 0;
      CMethod(ps->wrfunc)(ps, 0, ps->wrclosure);
    }
  }

  return;
}

/*
 * ReadStreamHandler
 */
static void
ReadStreamHandler(cldata, netfd, xid)
XtPointer cldata;
int *netfd;
XtInputId *xid;
{
  ChimeraStream ps = (ChimeraStream)cldata;
  ssize_t rlen;
  struct sockaddr addr;
  int s;
  int namlen;

  if (ps->bound)
  {
    if (!ps->accepted)
    {
      namlen = sizeof(addr);
      if ((ps->as = accept(ps->s, &addr, &namlen)) < 0)
      {
	XtRemoveInput(ps->rdid);
	ps->rdid = 0;
	CMethod(ps->rdfunc)(ps, -1, ps->rdclosure);
      }
      ps->accepted = true;
    }
    s = ps->as;
  }
  else s = ps->s;

  rlen = read(s, ps->rdb, ps->rdlen);
  if (rlen < 0)
  {
    if (errno != EWOULDBLOCK)
    {
      XtRemoveInput(ps->rdid);
      ps->rdid = 0;
      CMethod(ps->rdfunc)(ps, rlen, ps->rdclosure);
    }
  }
  else
  {
    XtRemoveInput(ps->rdid);
    ps->rdid = 0;
    CMethod(ps->rdfunc)(ps, rlen, ps->rdclosure);
  }

  return;
}

/*
 * StreamRead
 */
void
StreamRead(ps, b, blen, func, closure)
ChimeraStream ps;
byte *b;
size_t blen;
ChimeraStreamCallback func;
void *closure;
{
  int s;

  myassert(!ps->destroyed, "ChimeraStream destroyed");

  if (ps->bound && ps->accepted) s = ps->as;
  else s = ps->s;

  ps->rdb = b;
  ps->rdlen = blen;
  ps->rdfunc = func;
  ps->rdclosure = closure;
  ps->rdid = XtAppAddInput(ps->cres->appcon, s,
			   (XtPointer)XtInputReadMask,
			   ReadStreamHandler, (XtPointer)ps);

  return;
}

/*
 * StreamWrite
 */
void
StreamWrite(ps, b, blen, func, closure)
ChimeraStream ps;
byte *b;
size_t blen;
ChimeraStreamCallback func;
void *closure;
{
  myassert(!ps->destroyed, "ChimeraStream destroyed.");

  ps->wrb = b;
  ps->wri = 0;
  ps->wrmax = blen;
  ps->wrfunc = func;
  ps->wrclosure = closure;
  ps->wrid = XtAppAddInput(ps->cres->appcon,
			   ps->s,
			   (XtPointer)XtInputWriteMask,
			   WriteStreamHandler, (XtPointer)ps);

  return;
}

/*
 * StreamCreateINet
 */
ChimeraStream
StreamCreateINet(cres, host, port)
ChimeraResources cres;
char *host;
int port;
{
  ChimeraStream ps;
  int s;
  int rval;
  struct sockaddr_in addr;
  struct hostent *hp;
  MemPool mp;

  if (host == NULL) return(NULL);

  memset(&addr, 0, sizeof(addr));

  /* fix by Jim Rees so that numeric addresses are dealt with */
  if ((addr.sin_addr.s_addr = inet_addr(host)) == -1)
  {
    if ((hp = (struct hostent *)gethostbyname(host)) == NULL)
    {
      return(NULL);
    }
    memcpy(&(addr.sin_addr), hp->h_addr, hp->h_length);
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)port);

  s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s < 0) return(NULL);

#ifdef __QNX__
  ioctl(s, FNDELAY, 0);
#else
  fcntl(s, F_SETFL, FNDELAY);
#endif

  if ((rval = connect(s, (struct sockaddr *)&addr, sizeof(addr))) != 0)
  {
    if (errno != EINPROGRESS)
    {
      close(s);
      return(NULL);
    }
  }

  mp = MPCreate();
  ps = (ChimeraStream)MPCGet(mp, sizeof(struct ChimeraStreamP));
  ps->mp = mp;
  ps->cres = cres;
  ps->s = s;

  return((ChimeraStream)ps);
}

/*
 * StreamCreateINet2
 */
ChimeraStream
StreamCreateINet2(cres)
ChimeraResources cres;
{
  MemPool mp;
  ChimeraStream ps;
  int s;
  struct sockaddr_in addr;
  struct sockaddr_in xaddr;
  struct hostent *hp;
  int namlen;
  char host[BUFSIZ];

  s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s < 0) return(NULL);

  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_family = AF_INET;
  addr.sin_port = 0;

  if (bind(s, (struct sockaddr *) &addr, sizeof (addr)) < 0)
  {
    close(s);
    return(NULL);
  }

  if (listen(s, 1) < 0)
  {
    close(s);
    return(NULL);
  }

  if (gethostname(host, sizeof(host) - 1) == 0)
  {
    /* fix by Jim Rees so that numeric addresses are dealt with */
    if ((xaddr.sin_addr.s_addr = inet_addr(host)) == -1)
    {
      if ((hp = (struct hostent *)gethostbyname(host)) == NULL)
      {
	close(s);
	return(NULL);
      }
      memcpy(&(xaddr.sin_addr), hp->h_addr, hp->h_length);
    }
  }
  else
  {
    close(s);
    return(NULL);
  }

  namlen = sizeof(addr);
  if (getsockname(s, (struct sockaddr *)&addr, &namlen) < 0)
  {
    close(s);
    return(NULL);
  }

#ifdef __QNX__
  ioctl(s, FNDELAY, 0);
#else
  fcntl(s, F_SETFL, FNDELAY);
#endif

  mp = MPCreate();
  ps = (ChimeraStream)MPCGet(mp, sizeof(struct ChimeraStreamP));
  ps->mp = mp;
  ps->cres = cres;
  ps->s = s;
  ps->port = (int)addr.sin_port;
  ps->addr = xaddr.sin_addr.s_addr;
  ps->bound = true;

  return((ChimeraStream)ps);
}

/*
 * StreamDestroy
 */
void
StreamDestroy(ps)
ChimeraStream ps;
{
  myassert(!ps->destroyed, "ChimeraStream destroyed");

  ps->destroyed = true;
  if (ps->rdid != 0) XtRemoveInput(ps->rdid);
  if (ps->wrid != 0) XtRemoveInput(ps->wrid);
  close(ps->s);
  MPDestroy(ps->mp);

  return;
}

/*
 * StreamGetINetPort
 */
int
StreamGetINetPort(ps)
ChimeraStream ps;
{
  return(ps->port);
}

/*
 * StreamGetINetAddr
 */
unsigned long
StreamGetINetAddr(ps)
ChimeraStream ps;
{
  return(ps->addr);
}

/*
 * StreamCreate
 */
ChimeraStream
StreamCreate(cres, fd)
ChimeraResources cres;
int fd;
{
  MemPool mp;
  ChimeraStream ps;

  fcntl(fd, F_SETFL, FNDELAY);

  mp = MPCreate();
  ps = (ChimeraStream)MPCGet(mp, sizeof(struct ChimeraStreamP));
  ps->mp = mp;
  ps->cres = cres;
  ps->s = fd;

  return((ChimeraStream)ps);
}
