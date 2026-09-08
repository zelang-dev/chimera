/*
 * ftp.c
 *
 * Copyright 1993-1997, John Kilburg (john@cs.unlv.edu)
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

#include "Chimera.h"
#include "ChimeraStream.h"
#include "ChimeraSource.h"
#include "ChimeraAuth.h"
#include "mime.h"

#define PRINT_RATE 10

#define FM_OPEN 0
#define FM_RU 1
#define FM_RK 2
#define FM_DONE 3
#define FM_SEND 4
#define FM_WAITING 5

#define MSGLEN 1024
#define REQLEN 1024

static struct ftp_message
{
  char *name;
  char *def;
} ftp_messages[] =
{
  { "ftp.open", "Connecting to " },
  { "ftp.read_unknown", " bytes read so far." },
  { "ftp.read_known", " bytes remaining." },
  { "ftp.done", "FTP read finished." },
  { "ftp.send", "Sending request..." },
  { "ftp.waiting", "Waiting for a response from " },
  { NULL, NULL },
};

typedef struct 
{
  char *username;
  char *password;
  char *hostname;
  int port;
  bool good;
} FTPPassword;

typedef struct
{
  MemPool mp;
  char *header;
  char *trailer;
  GList passwords;
} FTPClass;

typedef struct FTPInfoP FTPInfo;
typedef void (*FTPProc) _ArgProto((FTPInfo *));
static void FTPDestroy _ArgProto((void *));
static void FTPData _ArgProto((ChimeraStream, ssize_t, void *));
static void FTPDirData _ArgProto((ChimeraStream, ssize_t, void *));
static void FTPSimpleRead _ArgProto((ChimeraStream, ssize_t, void *));
static void FTPDummy _ArgProto((ChimeraStream, ssize_t, void *));
static void FTPDirRead _ArgProto((FTPInfo *));
static void FTPNlst _ArgProto((FTPInfo *));
static void FTPCwd _ArgProto((FTPInfo *));
static void FTPRetrieve _ArgProto((FTPInfo *));
static void FTPPassive _ArgProto((FTPInfo *));
static void FTPSize _ArgProto((FTPInfo *));
static void FTPType _ArgProto((FTPInfo *));
static void FTPUser _ArgProto((FTPInfo *));
static void FTPPass _ArgProto((FTPInfo *));
static void *FTPInit _ArgProto((ChimeraSource, ChimeraRequest *, void *));
static void FTPClassDestroy _ArgProto((void *));
static void FTPCancel _ArgProto((void *));
static void FTPGetData _ArgProto((void *, byte **, size_t *, MIMEHeader *));
static int ftp_strcmp _ArgProto((const void *, const void *));
static void FTPAuthCallback _ArgProto((void *, char *, char *));
void InitModule_FTP _ArgProto((ChimeraResources));

struct FTPInfoP
{
  MemPool mp;
  ChimeraSource ws;
  ChimeraResources cres;
  ChimeraRequest *wr;
  FTPClass *fc;
  FTPProc rfunc;
  char msgbuf[MSGLEN];
  char request[REQLEN];
  char *msg[sizeof(ftp_messages) / sizeof(ftp_messages[0])];
  int rcount;
  FTPPassword *fp;
  ChimeraAuth wa;

  /* control stream */
  ChimeraStream cs;
  byte *cb;                /* control buffer */
  size_t cblen;            /* control buffer content length */
  size_t cbsize;           /* control buffer size */
  bool ignore_err;         /* recognition of control errors */

  /* data stream */
  ChimeraStream ds;
  byte *db;                /* data buffer */
  size_t dblen;            /* data buffer content length */
  size_t dbsize;           /* data buffer size */
  size_t dbmax;            /* data buffer maximum */
  MIMEHeader mh;
};

static char *FTPDirToHTML _ArgProto((FTPInfo *));
static void FTPWrite _ArgProto((ChimeraStream, ssize_t, void *));
static void FTPDestroyStream _ArgProto((FTPInfo *));
static void FTPReadData _ArgProto((FTPInfo *, ChimeraStreamCallback));
static void FTPReadControl _ArgProto((FTPInfo *, ChimeraStreamCallback));
static int FTPParseResponse _ArgProto((FTPInfo *));
static void FTPDummy _ArgProto((ChimeraStream, ssize_t, void *));
static void FTPFailure _ArgProto((FTPInfo *));
static void FTPSimple _ArgProto((FTPInfo *, FTPProc, bool));
static void FTPAddPassword _ArgProto((FTPInfo *, char *, char *));
static FTPPassword *FTPFindPassword _ArgProto((FTPInfo *, char *));

/*
 * FTPFindPassword
 */
static FTPPassword *
FTPFindPassword(fi, username)
FTPInfo *fi;
char *username;
{
  FTPPassword *fp;
  char *hostname;
  int port;

  hostname = fi->wr->up->hostname;
  port = fi->wr->up->port == 0 ? 21:fi->wr->up->port;

  for (fp = (FTPPassword *)GListGetHead(fi->fc->passwords); fp != NULL;
       fp = (FTPPassword *)GListGetNext(fi->fc->passwords))
  {
    if (strcmp(username, fp->username) == 0 &&
	strcasecmp(hostname, fp->hostname) == 0 &&
	port == fp->port &&
	fp->good)
    {
      return(fp);
    }
  }

  return(NULL);
}

/*
 * FTPAddPassword
 */
static void
FTPAddPassword(fi, username, password)
FTPInfo *fi;
char *username;
char *password;
{
  FTPPassword *fp;
  MemPool mp;

  if (username == NULL) return;
  if (fi->wr->up->hostname == NULL) return;

  mp = fi->fc->mp;

  fp = (FTPPassword *)MPCGet(mp, sizeof(FTPPassword));
  fp->username = MPStrDup(mp, username);
  if (password != NULL) fp->password = MPStrDup(mp, password);
  else fp->password = MPStrDup(mp, "");
  fp->hostname = MPStrDup(mp, fi->wr->up->hostname);
  fp->port = fi->wr->up->port == 0 ? 21:fi->wr->up->port;
  GListAddHead(fi->fc->passwords, fp);

  fi->fp = fp;

  return;
}

/*
 * FTPFailure
 */
static void
FTPFailure(fi)
FTPInfo *fi;
{
  SourceStop(fi->ws, "ftp failure");
  FTPDestroyStream(fi);
  return;
}

/*
 * FTPDestroyStream
 */
static void
FTPDestroyStream(fi)
FTPInfo *fi;
{
  if (fi->ds != NULL)
  {
    StreamDestroy(fi->ds);
    fi->ds = NULL;
  }
  if (fi->cs != NULL)
  {
    StreamDestroy(fi->cs);
    fi->cs = NULL;
  }
  return;
}

/*
 * FTPDestroy
 */
static void
FTPDestroy(closure)
void *closure;
{
  FTPInfo *fi = (FTPInfo *)closure;
  FTPDestroyStream(fi);
  if (fi->db != NULL) free_mem(fi->db);
  if (fi->cb != NULL) free_mem(fi->cb);
  if (fi->mh != NULL) MIMEDestroyHeader(fi->mh);
  MPDestroy(fi->mp);
  return;
}

/*
 * FTPReadData
 */
static void
FTPReadData(fi, func)
FTPInfo *fi;
ChimeraStreamCallback func;
{
  size_t len;

  if (fi->dbmax > 0)
  {
    len = fi->dbmax - fi->dblen;
    if (len > BUFSIZ) len = BUFSIZ;
  }
  else len = BUFSIZ;

  if (len > 0)
  {
    if (fi->db == NULL) fi->db = (byte *)alloc_mem(len);
    else fi->db = (byte *)realloc_mem(fi->db, fi->dbsize + len);
    fi->dbsize += len;
  }

  StreamRead(fi->ds, fi->db + fi->dblen, len, func, fi);

  return;
}

/*
 * FTPReadControl
 */
static void
FTPReadControl(fi, func)
FTPInfo *fi;
ChimeraStreamCallback func;
{
  if (fi->cblen + BUFSIZ > fi->cbsize)
  {
    if (fi->cb == NULL) fi->cb = (byte *)alloc_mem(BUFSIZ);
    else fi->cb = (byte *)realloc_mem(fi->cb, fi->cbsize + BUFSIZ);
    fi->cbsize += BUFSIZ;
  }
  StreamRead(fi->cs, fi->cb + fi->cblen, BUFSIZ, func, fi);

  return;
}

/*
 * FTPData
 */
static void
FTPData(ios, len, closure)
ChimeraStream ios;
ssize_t len;
void *closure;
{  
  FTPInfo *fi = (FTPInfo *)closure;

  if (len < 0)
  {
    FTPFailure(fi);
    return;
  }

  if (len == 0)
  {
    SourceSendMessage(fi->ws, fi->msg[FM_DONE]);
    FTPCancel(fi);
    SourceEnd(fi->ws);
  }
  else
  {
    fi->dblen += len;
    if (fi->dbmax > 0)
    {
      if (fi->rcount++ % PRINT_RATE == 0)
      {
	snprintf (fi->msgbuf, sizeof(fi->msgbuf),
		  "%ld %s", (long)(fi->dbmax - fi->dblen), fi->msg[FM_RK]);
	SourceSendMessage(fi->ws, fi->msgbuf);
      }
      SourceAdd(fi->ws);
    }
    else
    {
      if (fi->rcount++ % PRINT_RATE == 0)
      {
	snprintf (fi->msgbuf, sizeof(fi->msgbuf),
		  "%ld %s", (long)fi->dblen, fi->msg[FM_RU]);
	SourceSendMessage(fi->ws, fi->msgbuf);
      }
    }
    FTPReadData(fi, FTPData);
  }

  return;
}

/*
 * ftp_dirdata
 */
static void
FTPDirData(ios, len, closure)
ChimeraStream ios;
ssize_t len;
void *closure;
{  
  FTPInfo *fi = (FTPInfo *)closure;

  if (len > 0)
  {
    fi->dblen += len;
    FTPReadData(fi, FTPDirData);
    return;
  }
  else if (len < 0)
  {
    FTPFailure(fi);
    return;
  }

  MIMEAddField(fi->mh, "content-type", "text/html");
  MIMEAddField(fi->mh, "x-url", fi->wr->url);
  fi->db = FTPDirToHTML(fi);
  fi->dblen = strlen(fi->db);

  FTPDestroyStream(fi);

  SourceInit(fi->ws, fi->fp == NULL);
  SourceEnd(fi->ws);

  return;
}

/*
 * FTPParseResponse
 */
static int
FTPParseResponse(fi)
FTPInfo *fi;
{
  char *cp;
  char *b = (char *)fi->cb;
  size_t len = fi->cblen;

  if (b[len - 1] == '\n')
  {
    if (*(b + 3) == ' ') return(atoi(b));
    for (cp = b + len - 2; cp >= b; cp--)
    {
      if (*cp == '\n' && *(cp + 4) == ' ') return(atoi(cp + 1));
    }
  }

  return(-1);
}

/*
 * FTPSimpleRead
 */
static void
FTPSimpleRead(ios, len, closure)
ChimeraStream ios;
ssize_t len;
void *closure;
{
  int ecode;
  FTPInfo *fi = (FTPInfo *)closure;

  if (len < 0)
  {
    FTPFailure(fi);
    return;
  }

  fi->cblen += len;

  if ((ecode = FTPParseResponse(fi)) == -1)
  {
    FTPReadControl(fi, FTPSimpleRead);
    return;
  }
  if (ecode < 400 || fi->ignore_err)
  {
    (fi->rfunc)(fi);
    fi->cblen = 0;
  }
  else FTPFailure(fi);

  return;
}

/*
 * FTPWrite
 */
static void
FTPWrite(ios, len, closure)
ChimeraStream ios;
ssize_t len;
void *closure;
{
  FTPReadControl((FTPInfo *)closure, FTPSimpleRead);
  return;
}

/*
 * FTPSimple
 */
static void
FTPSimple(fi, rfunc, ignore_err)
FTPInfo *fi;
FTPProc rfunc;
bool ignore_err;
{
  fi->rfunc = rfunc;
  fi->ignore_err = ignore_err;
  SourceSendMessage(fi->ws, fi->msg[FM_SEND]);
  StreamWrite(fi->cs, (byte *)fi->request, strlen(fi->request),
	      FTPWrite, fi);
  return;
}

/*
 * FTPDirRead
 */
static void
FTPDirRead(fi)
FTPInfo *fi;
{
  FTPReadData(fi, FTPDirData);
  return;
}

/*
 * FTPNlst
 */
static void
FTPNlst(fi)
FTPInfo *fi;
{
  snprintf (fi->request, sizeof(fi->request), "NLST\r\n");
  FTPSimple(fi, FTPDirRead, false);
  return;
}

/*
 * FTPDummy
 */
static void
FTPDummy(ios, len, closure)
ChimeraStream ios;
ssize_t len;
void *closure;
{
  return;
}

/*
 * FTPCwd
 */
static void
FTPCwd(fi)
FTPInfo *fi;
{
  int ecode;
  char *filename;

  filename = fi->wr->up->filename;

  sscanf((char*)fi->cb, "%d", &ecode);
  if (ecode < 400)
  {
    char *content;

    if ((content = ChimeraExt2Content(fi->cres, filename)) == NULL)
    {
      content = "text/plain";
    }
    MIMEAddField(fi->mh, "content-type", content);
    MIMEAddField(fi->mh, "x-url", fi->wr->url);

    SourceInit(fi->ws, fi->fp == NULL);

    FTPReadData(fi, FTPData);
    FTPReadControl(fi, FTPDummy);
  }
  else
  {
    snprintf (fi->request, sizeof(fi->request), "CWD %s\r\n", filename);
    FTPSimple(fi, FTPNlst, false);
  }

  return;
}

/*
 * FTPRetrieve
 */
static void
FTPRetrieve(fi)
FTPInfo *fi;
{
  int h0, h1, h2, h3, p0, p1, reply, n;
  const char *format = "RETR %s\r\n";
  char dhost[BUFSIZ];
  int dport;

  n = sscanf((char *)fi->cb, "%d %*[^(] (%d,%d,%d,%d,%d,%d)",
	     &reply, &h0, &h1, &h2, &h3, &p0, &p1);
  if (n != 7 || reply != 227)
  {
    /* error */
    return;
  }
  
  snprintf (dhost, sizeof(dhost), "%d.%d.%d.%d", h0, h1, h2, h3);
  dport = (p0 << 8) + p1;
  
  /*
   * Check for error here.
   */
  if ((fi->ds = StreamCreateINet(fi->cres, dhost, dport)) == NULL)
  {
    return;
  }

  snprintf (fi->request, sizeof(fi->request), format, fi->wr->up->filename);
  FTPSimple(fi, FTPCwd, true);

  return;
}

/*
 * FTPPassive
 */
static void
FTPPassive(fi)
FTPInfo *fi;
{
  int ecode;
  long size;

  sscanf((char *)fi->cb, "%d %ld", &ecode, &size);

  fi->dbmax = (size_t)size;
  if (ecode >= 400) fi->dbmax = 0;
  else
  {
    fi->db = (byte *)realloc_mem(fi->db, fi->dbmax);
    fi->dbsize = fi->dbmax;
  }

  snprintf (fi->request, sizeof(fi->request), "PASV\r\n");
  FTPSimple(fi, FTPRetrieve, true);

  return;
}

/*
 * FTPSize
 */
static void
FTPSize(fi)
FTPInfo *fi;
{
  snprintf (fi->request, sizeof(fi->request),
	    "SIZE %s\r\n", fi->wr->up->filename);
  FTPSimple(fi, FTPPassive, true);
  return;
}

/*
 * FTPType
 */
static void
FTPType(fi)
FTPInfo *fi;
{
  /* Now we know the password succeeded so check it as OK */
  if (fi->fp != NULL) fi->fp->good = true;
  snprintf (fi->request, sizeof(fi->request), "TYPE I\r\n");
  FTPSimple(fi, FTPSize, false);
  return;
}

/*
 * FTPPass
 */
static void
FTPPass(fi)
FTPInfo *fi;
{
  char *uname;
  const char *pformat = "PASS %s\r\n";

  if (fi->fp != NULL)
  {
    snprintf (fi->request, sizeof(fi->request), pformat, fi->fp->password);
  }
  else if (fi->wr->up->password != NULL)
  {
    snprintf (fi->request, sizeof(fi->request), pformat, fi->wr->up->password);
  }
  else if ((uname = getenv("EMAIL")) != NULL)
  {
    snprintf (fi->request, sizeof(fi->request), pformat, uname);
  }
  else
  {
    snprintf (fi->request, sizeof(fi->request),
	      "PASS -nobody@nowhere.org\r\n");
  } 
  FTPSimple(fi, FTPType, false);

  return;
}

/*
 * FTPAuthCallback
 */
static void
FTPAuthCallback(closure, username, password)
void *closure;
char *username;
char *password;
{
  FTPInfo *fi = (FTPInfo *)closure;

  if (username == NULL || password == NULL)
  {
    FTPCancel(fi);
    return;
  }

  FTPAddPassword(fi, username, password);

  AuthDestroy(fi->wa);
  fi->wa = NULL;

  FTPUser(fi);

  return;
}

/*
 * FTPUser
 */
static void
FTPUser(fi)
FTPInfo *fi;
{
  const char *uformat = "USER %s\r\n";

  if (fi->fp != NULL)
  {
    snprintf (fi->request, sizeof(fi->request), uformat, fi->fp->username);
  }
  else if (fi->wr->up->username != NULL)
  {
    if (fi->wr->up->password == NULL)
    {
      if ((fi->fp = FTPFindPassword(fi, fi->wr->up->username)) == NULL)
      {
	fi->wa = AuthCreate(fi->cres, "Enter password", fi->wr->up->username,
			    FTPAuthCallback, fi);
	if (fi->wa != NULL) return;
      }
    }

    snprintf (fi->request, sizeof(fi->request), uformat, fi->wr->up->username);
  }
  else
  {
    snprintf (fi->request, sizeof(fi->request), "USER anonymous\r\n");
  }
  
  FTPSimple(fi, FTPPass, false);

  return;
}

/*
 * FTPInit
 */
static void *
FTPInit(ws, wr, class_closure)
ChimeraSource ws;
ChimeraRequest *wr;
void *class_closure;
{
  FTPInfo *fi;
  MemPool mp;
  size_t mlen, tlen;
  int i;

  mp = MPCreate();
  fi = (FTPInfo *)MPCGet(mp, sizeof(FTPInfo));
  fi->mp = mp;
  fi->ws = ws;
  fi->wr = wr;
  fi->cres = SourceToResources(ws);
  fi->fc = (FTPClass *)class_closure;
  fi->mh = MIMECreateHeader();

  /* get the status messages and allocate a message work buffer */
  mlen = 0;
  for (i = 0; ftp_messages[i].name != NULL; i++)
  {
    if ((fi->msg[i] = ResourceGetString(fi->cres,
					ftp_messages[0].name)) == NULL)
    {
      fi->msg[i] = ftp_messages[i].def;
    }
    if ((tlen = strlen(fi->msg[i])) > mlen) mlen = tlen;    
  }

  snprintf (fi->msgbuf, sizeof(fi->msgbuf),
	    "%s %s", fi->msg[FM_OPEN], wr->up->hostname);
  SourceSendMessage(fi->ws, fi->msgbuf);

  fi->cs = StreamCreateINet(fi->cres,
			    wr->up->hostname,
			    wr->up->port == 0 ? 21:wr->up->port);
  if (fi->cs == NULL)
  {
    FTPDestroy(fi);
    return(NULL);
  }

  fi->rfunc = FTPUser;
  FTPReadControl(fi, FTPSimpleRead);

  return(fi);
}

/*
 * FTPClassDestroy
 */
static void
FTPClassDestroy(closure)
void *closure;
{
  FTPClass *fc = (FTPClass *)closure;
  MPDestroy(fc->mp);
  return;
}

/*
 * FTPCancel
 */
static void
FTPCancel(closure)
void *closure;
{
  FTPInfo *fi = (FTPInfo *)closure;

  if (fi->wa != NULL)
  {
    AuthDestroy(fi->wa);
    fi->wa = NULL;
  }
  FTPDestroyStream(fi);

  return;
}

static void
FTPGetData(closure, data, len, mh)
void *closure;
byte **data;
size_t *len;
MIMEHeader *mh;
{
  FTPInfo *fi = (FTPInfo *)closure;

  *data = fi->db;
  *len = fi->dblen;
  *mh = fi->mh;
  
  return;
}

/*
 * InitModule_FTP
 */
void
InitModule_FTP(cres)
ChimeraResources cres;
{
  ChimeraSourceHooks ph;
  FTPClass *fc;
  MemPool mp;

  mp = MPCreate();
  fc = (FTPClass *)MPCGet(mp, sizeof(FTPClass));
  fc->mp = mp;
  fc->passwords = GListCreateX(mp);
  fc->header = ResourceGetString(cres, "ftp.dirheader");
  if (fc->header == NULL)
  {
    fc->header = "<html><body><h2>FTP Directory</h2><ul>";
  }

  fc->trailer = ResourceGetString(cres, "ftp.dirtrailer");
  if (fc->trailer == NULL) fc->trailer = "</ul></body></html>";

  memset(&ph, 0, sizeof(ph));
  ph.class_closure = fc;
  ph.class_destroy = FTPClassDestroy;
  ph.name = "ftp";
  ph.init = FTPInit;
  ph.destroy = FTPDestroy;
  ph.stop = FTPCancel;
  ph.getdata = FTPGetData;
  SourceAddHooks(cres, &ph);
  
  return;
}

static int
ftp_strcmp(a, b)
const void *a, *b;
{
  return(strcmp(*((char **)a), *((char **)b)));
}

/*
 * FTPDirToHTML
 *
 * Convert directory list to HTML
 */
static char *
FTPDirToHTML(fi)
FTPInfo *fi;
{
  char *f;
  int i;
  char *hostname;
  char *filename;
  const char *entry = "<li><a href=ftp://%s:%d%s/%s>%s</a>\n";
  byte *cp, *lastcp, *dname;
  int sacount;
  char **sa;
  int olen, hlen, flen, elen;
  FTPClass *fc = fi->fc;

  if (fi->wr->up->username != NULL)
  {
    size_t t;

    t = strlen(fi->wr->up->username);
    t += strlen("@");
    t += strlen(fi->wr->up->hostname);

    hostname = MPGet(fi->mp, t + 1);
    strcpy(hostname, fi->wr->up->username);
    strcat(hostname, "@");
    strcat(hostname, fi->wr->up->hostname);
  }
  else
  {
    hostname = fi->wr->up->hostname;
  }
  
  filename = fi->wr->up->filename;
  hlen = strlen(hostname);
  flen = strlen(filename);
  elen = strlen(entry);
  
  sacount = 0;
  for (cp = fi->db, lastcp = cp + fi->dblen; cp < lastcp; cp++)
  {
    if (*cp == '\n') sacount++;
  }
  sa = (char **)MPGet(fi->mp, sizeof(char *) * sacount);
  dname = fi->db;
  olen = 0;
  for (i = 0, cp = fi->db, lastcp = cp + fi->dblen; cp < lastcp; cp++)
  {
    if (*cp == '\n')
    {
      sa[i] = (char *)dname;
      *cp = '\0';
      dname = (byte *)cp + 1;
      olen += hlen + 20 + flen + elen + strlen(sa[i]) * 2;
      i++;
    }
  }
  qsort(sa, sacount, sizeof(char *), ftp_strcmp);

  olen += strlen(fc->header) + 2 * flen + hlen + strlen(fc->trailer) + 1;
  f = (char *)alloc_mem(olen);
  snprintf (f, olen, fc->header, filename, hostname, filename);

  if (filename[0] != '\0' && filename[1] == '\0') filename = "";

  for (i = 0; i < sacount; i++)
  {
    snprintf(f + strlen(f), olen - strlen(f), entry,
	     hostname,
	     fi->wr->up->port == 0 ? 21:fi->wr->up->port,
	     filename,
	     sa[i], sa[i]);
  }

  strcat(f, fc->trailer);

  return(f);
}

