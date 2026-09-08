/*
 * http.c
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "port_after.h"

#include "Chimera.h"
#include "ChimeraStream.h"
#include "ChimeraAuth.h"
#include "ChimeraSource.h"

#include "mime.h"

/*
 * OK, this is a little bit weird.  The context that is passed is
 * actually the address of the address of the context.  This is
 * because once a context is returned by HTTPInit it can't be
 * changed (at least the way this circus works).  Its convienent
 * to be able to destroy the context when a new connection is made
 * because of authorization stuff or a redirect occurs.
 */

/*
 * Size of chunks to allocate while reading.
 */
#define CHUNKSIZE 16384

#define PRINT_RATE 10
#define MSGLEN 1024

#define HM_OPEN 0
#define HM_RU 1
#define HM_RK 2
#define HM_DONE 3
#define HM_SEND 4
#define HM_WAITING 5

static struct http_message
{
  char *name;
  char *def;
} http_messages[] =
{
  { "http.open", "Connecting to " },
  { "http.read_unknown", " bytes read so far." },
  { "http.read_known", " bytes remaining." },
  { "http.done", "HTTP read finished." },
  { "http.send", "Sending request..." },
  { "http.waiting", "Waiting for a response from " },
  { NULL, NULL },
};

typedef struct 
{
  char *username;
  char *password;
  char *hostname;
  char *realm;
  char *type;
  int port;
} HTTPPassword;

typedef struct
{
  MemPool mp;
  char *msg[sizeof(http_messages) / sizeof(http_messages[0])];
  size_t mlen;
  GList passwords;       /* username/password/realm cache */
} HTTPClass;

typedef struct
{
  MemPool mp, rmp;
  ChimeraSource ws;
  ChimeraResources cres;

  /* addressing stuff */
  ChimeraRequest *wr;          /* request addresses */
  char *url;                   /* URL to use for request */
  URLParts *up;                /* URLParts to use for request */
  URLParts *pup;               /* proxy server */

  /* other stuff */
  HTTPClass *hc;
  char msgbuf[MSGLEN];
  int nline;
  int rcount;

  /* Information from HTTP status line */
  int status;
  int major;
  int minor;

  /* authorization stuff */
  ChimeraAuth wa;
  HTTPPassword *hp;
  char *auth_type;    /* used for the Auth callback */
  char *realm;        /* used for the Auth callback */

  ChimeraStream ios;

  /* Information about the data received. */
  byte *b;
  size_t blen;
  size_t bsize;
  size_t bmax;
  size_t doff;       /* offset to data beyond header */
  MIMEHeader mh;  
} HTTPInfo;

static int uuencode _ArgProto((unsigned char *, unsigned int, char *));
static byte *HTTPBuildRequest _ArgProto((HTTPInfo *, ssize_t *));
static int HTTPCreateInfo _ArgProto((HTTPInfo **,
				     ChimeraSource, ChimeraRequest *,
				     HTTPClass *,
				     URLParts *, URLParts *,
				     HTTPPassword *));
static void HTTPCancel _ArgProto((void *));
static void *HTTPInit _ArgProto((ChimeraSource, ChimeraRequest *, void *));
static void HTTPDestroy _ArgProto((void *));
static void HTTPDestroyInfo _ArgProto((HTTPInfo *));
static void HTTPGetData _ArgProto((void *, byte **, size_t *, MIMEHeader *));
static byte *HTTPRequest_Auth _ArgProto((HTTPInfo *, ssize_t *));
static byte *HTTPRequest_UserAgent _ArgProto((HTTPInfo *, ssize_t *));
static byte *HTTPRequest_AcceptLang _ArgProto((HTTPInfo *, ssize_t *));
static byte *HTTPRequest_Accept _ArgProto((HTTPInfo *, ssize_t *));
static byte *HTTPRequest_Pragma _ArgProto((HTTPInfo *, ssize_t *));
static byte *HTTPRequest_Method _ArgProto((HTTPInfo *, ssize_t *));
static byte *HTTPRequest_Host _ArgProto((HTTPInfo *, ssize_t *));
static byte *HTTPRequest_Data1 _ArgProto((HTTPInfo *, ssize_t *));
static byte *HTTPRequest_Data2 _ArgProto((HTTPInfo *, ssize_t *));
static void HTTPFailure _ArgProto((HTTPInfo *));
static void HTTPRead _ArgProto((HTTPInfo **, ChimeraStreamCallback));
void HTTPReadHeader _ArgProto((ChimeraStream, ssize_t, void *));
static void HTTPReadData _ArgProto((ChimeraStream, ssize_t, void *));
static void HTTPReadUnknown _ArgProto((ChimeraStream, ssize_t, void *));
static void HTTPRequestDone _ArgProto((ChimeraStream, ssize_t, void *));
static void HTTPClassDestroy _ArgProto((void *));
void InitModule_HTTP _ArgProto((ChimeraResources));
static char *HTTPGetFilename _ArgProto((HTTPInfo *));
static int HTTPCheckHeader _ArgProto((HTTPInfo **));
HTTPPassword *HTTPFindPassword _ArgProto((HTTPInfo *, char *,
						 char *, int));
HTTPPassword *HTTPAddPassword _ArgProto((HTTPInfo *, char *, char *,
						char *, char *, char *, int));
static void HTTPAuthCallback _ArgProto((void *, char *, char *));

/*
 * crap for uuencode
 */
static char six2pr[64] =
{
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','+','/'
};

/*
 * uuencode
 *
 * I snarfed this code from some version of libwww.
 *
 * ACKNOWLEDGEMENT:
 *      This code is taken from rpem distribution, and was originally
 *      written by Mark Riordan.
 *
 * AUTHORS:
 *      MR      Mark Riordan    riordanmr@clvax1.cl.msu.edu
 *      AL      Ari Luotonen    luotonen@dxcern.cern.ch
 *
 */
static int
uuencode(bufin, nbytes, bufcoded)
unsigned char *bufin;
unsigned int nbytes;
char *bufcoded;
{
  /* ENC is the basic 1 character encoding function to make a char printing */
#define ENC(c) six2pr[c]
  
  register char *outptr = bufcoded;
  unsigned int i;
  
  for (i=0; i<nbytes; i += 3) 
  {
    *(outptr++) = ENC(*bufin >> 2);            /* c1 */
    *(outptr++) = ENC(((*bufin << 4) & 060) | ((bufin[1] >> 4) & 017)); /*c2*/
    *(outptr++) = ENC(((bufin[1] << 2) & 074) | ((bufin[2] >> 6) & 03));/*c3*/
    *(outptr++) = ENC(bufin[2] & 077);         /* c4 */
    
    bufin += 3;
  }
  
  /* If nbytes was not a multiple of 3, then we have encoded too
   * many characters.  Adjust appropriately.
   */
  if (i == nbytes + 1)
  {
    /* There were only 2 bytes in that last group */
    outptr[-1] = '=';
  }
  else if (i == nbytes + 2)
  {
    /* There was only 1 byte in that last group */
    outptr[-1] = '=';
    outptr[-2] = '=';
  }
  *outptr = '\0';

  return(outptr - bufcoded);
}

/*
 * HTTPAuthCallback
 */
static void
HTTPAuthCallback(closure, username, password)
void *closure;
char *username;
char *password;
{
  HTTPInfo **hip = (HTTPInfo **)closure;
  HTTPInfo *hi = *hip;
  HTTPPassword *hp;

  if (username == NULL || password == NULL)
  {
    HTTPCancel(hip);
    return;
  }

  hp = HTTPAddPassword(hi, username, password, hi->realm, hi->auth_type,
		       hi->up->hostname, hi->up->port);

  if (HTTPCreateInfo(hip, hi->ws, hi->wr, hi->hc,
		     hi->up, hi->pup, hp) == -1)
  {
    HTTPFailure(hi);
  }
  HTTPDestroyInfo(hi);
    
  return;
}

/*
 * HTTPFindPassword
 */
HTTPPassword *
HTTPFindPassword(hi, realm, hostname, port)
HTTPInfo *hi;
char *realm;
char *hostname;
int port;
{
  HTTPPassword *hp;

  port = port == 0 ? 80:port;

  for (hp = (HTTPPassword *)GListGetHead(hi->hc->passwords); hp != NULL;
       hp = (HTTPPassword *)GListGetNext(hi->hc->passwords))
  {
    if (strlen(realm) == strlen(hp->realm) &&
	strcmp(realm, hp->realm) == 0 &&
	strlen(hostname) == strlen(hp->hostname) &&
	strcasecmp(hostname, hp->hostname) == 0 &&
	port == hp->port)
    {
      return(hp);
    }
  }

  return(NULL);
}

/*
 * HTTPAddPassword
 */
HTTPPassword *
HTTPAddPassword(hi, username, password, realm, type, hostname, port)
HTTPInfo *hi;
char *username;
char *password;
char *realm;
char *type;
char *hostname;
int port;
{
  HTTPPassword *hp;
  MemPool mp;

  if (username == NULL) return(NULL);
  if (hostname == NULL) return(NULL);

  mp = hi->hc->mp;

  hp = (HTTPPassword *)MPCGet(mp, sizeof(HTTPPassword));
  hp->username = MPStrDup(mp, username);
  if (password != NULL) hp->password = MPStrDup(mp, password);
  else hp->password = MPStrDup(mp, "");

  /*
   * Probably shouldn't have defaults here.  Should just error out.
   */
  if (realm != NULL) hp->realm = MPStrDup(mp, realm);
  else hp->realm = MPStrDup(mp, "");
  if (type != NULL) hp->type = MPStrDup(mp, type);
  else hp->type = MPStrDup(mp, "basic");

  hp->hostname = MPStrDup(mp, hostname);
  hp->port = port == 0 ? 80:port;
  GListAddHead(hi->hc->passwords, hp);

  return(hp);
}

/*
 * HTTPRequest_Auth
 */
static byte *
HTTPRequest_Auth(hi, len)
HTTPInfo *hi;
ssize_t *len;
{
  char *t, *line = NULL;
  const char *authfield = "Authorization: ";
  char *username;
  char *password;
  URLParts *up;

  if (hi->hp == NULL) return(NULL);
  if (hi->hp->type == NULL || strcasecmp(hi->hp->type, "basic") != 0)
  {
    return(NULL);
  }

  username = hi->hp->username;
  password = hi->hp->password;
  up = hi->up;

  line = (char *)MPGet(hi->rmp,
                       strlen(authfield) +
                       2 * strlen(username) + 2 * strlen(":") +
                       2 * strlen(password != NULL ? password:"") +
		       2 * strlen(hi->hp->type) + 
                       strlen("\r\n") + 1);
  strcpy(line, username);
  if (password != NULL)
  {
    strcat(line, ":");
    strcat(line, password);
  }

  t = (char *)MPGet(hi->rmp, strlen(line) * 2);
  uuencode(line, strlen(line), t);

  strcpy(line, authfield);
  strcat(line, hi->hp->type);
  strcat(line, " ");
  strcat(line, t);
  strcat(line, "\r\n");

  if (line != NULL) *len = strlen(line);

  return((byte *)line);
}

/*
 * HTTPRequest_UserAgent
 */
static byte *
HTTPRequest_UserAgent(hi, len)
HTTPInfo *hi;
ssize_t *len;
{
  char *ua;
  char *line = NULL;
  const char *uaformat = "User-agent: %s\r\n";
  size_t linelen;

  ua = ResourceGetString(hi->cres, "http.userAgent");
  if (ua == NULL) return(NULL);

  linelen = strlen(uaformat) + strlen(ua) + 1;
  line = (char *)MPGet(hi->rmp, linelen);
  snprintf (line, linelen, uaformat, ua);

  if (line != NULL) *len = strlen(line);  

  return((byte *)line);
}

/*
 * HTTPRequest_AcceptLang
 */
static byte *
HTTPRequest_AcceptLang(hi, len)
HTTPInfo *hi;
ssize_t *len;
{
  char *line;
  char *acc;
  const char *accformat = "Accept-Language: %s\r\n";
  size_t linelen;

  if ((acc = ResourceGetString(hi->cres, "http.acceptLanguage")) != NULL)
  {
    linelen = strlen(accformat) + strlen(acc) + 1;
    line = (char *)MPGet(hi->rmp, linelen);
    snprintf (line, linelen, accformat, acc);
    *len = strlen(line);
    return((byte *)line);
  }
  return(NULL);
}

/*
 * HTTPRequest_Accept
 */
static byte *
HTTPRequest_Accept(hi, len)
HTTPInfo *hi;
ssize_t *len;
{
  GList list;
  char *nr;
  size_t rlen;
  size_t delimlen;
  char *line = NULL;
  const char *accfield = "Accept: ";
  const char *delim = ",";

  if (hi->wr->contents == NULL) line = "Accept: */*\r\n";
  else
  {
    list = hi->wr->contents;
    delimlen = strlen(delim);
    rlen = 0;
    for (nr = (char *)GListGetHead(list); nr != NULL;
	 nr = (char *)GListGetNext(list))
    {
      rlen += strlen(nr) + delimlen;
    }
    
    line = (char *)MPGet(hi->rmp, strlen(accfield) +
			 rlen + strlen("\r\n") + 1);
    strcpy(line, accfield);
    for (nr = (char *)GListGetHead(list); nr != NULL; )
    {
      strcat(line, nr);
      if ((nr = GListGetNext(list)) != NULL) strcat(line, delim);
    }
    strcat(line, "\r\n");
  }

  if (line != NULL) *len = strlen(line);

  return((byte *)line);
}

/*
 * HTTPRequest_Pragma
 */
static byte *
HTTPRequest_Pragma(hi, len)
HTTPInfo *hi;
ssize_t *len;
{
  const char *pragma = "Pragma: no-cache\r\n";

  if (hi->wr->reload)
  {
    *len = strlen(pragma);
    return((byte *)MPStrDup(hi->rmp, pragma));
  }
  return(NULL);
}

/*
 * HTTPRequest_Method
 */
static byte *
HTTPRequest_Method(hi, len)
HTTPInfo *hi;
ssize_t *len;
{
  char *filename;
  char *line = NULL;
  ChimeraRequest *wr;
  const char *getformat1 = "GET %s HTTP/1.0\r\n";
  const char *getformat2 = "GET %s%s%s HTTP/1.0\r\n";
  const char *postformat = "POST %s HTTP/1.0\r\n";
  size_t linelen;

  filename = HTTPGetFilename(hi);
  wr = hi->wr;

  if (wr->input_method == NULL || strcasecmp(wr->input_method, "GET") == 0)
  {
    if (wr->input_data != NULL && wr->input_len > 0)
    {
      linelen = strlen(getformat2) + wr->input_len + strlen(filename) +
	  strlen("?") + 1;
	 
      line = (char *)MPGet(hi->rmp, linelen);
      snprintf (line, linelen, getformat2, filename, "?", wr->input_data);
    }
    else
    {
      linelen = strlen(getformat1) + strlen(filename) + 1;
      line = (char *)MPGet(hi->rmp, linelen);
      snprintf (line, linelen, getformat1, filename);
    }
  }
  else if (strcasecmp(wr->input_method, "POST") == 0)
  {
    linelen = strlen(postformat) + strlen(filename) + 1;
    line = (char *)MPGet(hi->rmp, linelen);
    snprintf (line, linelen, postformat, filename);
  }
  else
  {
    *len = -1;
    return(NULL);
  }

  if (line != NULL) *len = strlen(line);

  return((byte *)line);
}

/*
 * HTTPRequest_Host
 */
static byte *
HTTPRequest_Host(hi, len)
HTTPInfo *hi;
ssize_t *len;
{
  char *host;
  int port;
  char *line;
  size_t linelen;

  host = hi->up->hostname;
  port = hi->up->port;

  linelen = strlen(host) + strlen("Host:") + 50;
  line = (char *)MPCGet(hi->rmp, linelen);
  if (port == 0) snprintf (line, linelen, "Host: %s\r\n", host);
  else snprintf (line, linelen, "Host: %s:%d\r\n", host, port);

  if (line != NULL) *len = strlen(line);

  return((byte *)line);
}

/*
 * HTTPRequest_Data1
 */
static byte *
HTTPRequest_Data1(hi, len)
HTTPInfo *hi;
ssize_t *len;
{
  char *line = NULL;
  const char *format = "Content-type: %s\r\nContent-length: %d\r\n";
  ChimeraRequest *wr = hi->wr;
  size_t linelen;

  if (wr->input_method == NULL ||
      strcasecmp(wr->input_method, "POST") != 0)
  {
    return(NULL);
  }

  if (wr->input_data != NULL && wr->input_len > 0 &&
      wr->input_type != NULL)
  {
    linelen = strlen(format) + strlen(wr->input_type) + 101;
    line = (char *)MPGet(hi->rmp, linelen);
    snprintf (line, linelen, format, wr->input_type, wr->input_len);
  }

  if (line != NULL) *len = strlen(line);

  return((byte *)line);
}

/*
 * HTTPRequest_Data2
 */
static byte *
HTTPRequest_Data2(hi, len)
HTTPInfo *hi;
ssize_t *len;
{
  ChimeraRequest *wr = hi->wr;

  if (wr->input_method == NULL ||
      strcasecmp(wr->input_method, "POST") != 0)
  {
    return(NULL);
  }

  if (wr->input_data != NULL && wr->input_len > 0 &&
      wr->input_type != NULL)
  {
    *len = wr->input_len;
    return(wr->input_data);
  }

  return(NULL);
}

/* 
 * HTTPBuildRequest
 *
 * This became less efficient but easier to hack, I think.
 */
static byte *
HTTPBuildRequest(hi, len)
HTTPInfo *hi;
ssize_t *len;
{
  byte *line = NULL;

  do
  {
    *len = 0;
    if (hi->nline == 0) line = HTTPRequest_Method(hi, len);
    else if (hi->nline == 1) line = HTTPRequest_Host(hi, len);
    else if (hi->nline == 2) line = HTTPRequest_UserAgent(hi, len);
    else if (hi->nline == 3) line = HTTPRequest_Accept(hi, len);
    else if (hi->nline == 4) line = HTTPRequest_AcceptLang(hi, len);
    else if (hi->nline == 5) line = HTTPRequest_Pragma(hi, len);
    else if (hi->nline == 6) line = HTTPRequest_Auth(hi, len);
    /* this must be last */
    else if (hi->nline == 7) line = HTTPRequest_Data1(hi, len);
    else if (hi->nline == 8)
    {
      *len = strlen("\r\n");
      line = (byte *)MPStrDup(hi->rmp, "\r\n");
    }
    else if (hi->nline == 9) line = HTTPRequest_Data2(hi, len);
    else break;
    hi->nline++;
  } while (line == NULL && *len != -1);

  return(line);
}

/*
 * HTTPFailure
 */
static void
HTTPFailure(hi)
HTTPInfo *hi;
{
  HTTPCancel(&hi);
  SourceStop(hi->ws, "Read error during HTTP transfer");
  return;
}

/*
 * HTTPGetFilename
 */
static char *
HTTPGetFilename(hi)
HTTPInfo *hi;
{
  char *filename;

  /*
   * If there is a proxy URL supplied then get the entire URL and not just
   * just the filename part.  If there is no proxy then just use the
   * filename part.
   */
  if (hi->pup != NULL) filename = URLMakeString(hi->mp, hi->up, false);
  else filename = hi->up->filename;
  if (filename == NULL) filename = "/";

  return(filename);
}

/*
 * HTTPRead
 */
static void
HTTPRead(hip, func)
HTTPInfo **hip;
ChimeraStreamCallback func;
{
  size_t rsize;
  HTTPInfo *hi = *hip;

  if (hi->bmax > 0)
  {
    rsize = hi->bmax - hi->blen;
    if (rsize > CHUNKSIZE) rsize = CHUNKSIZE;
  }
  else rsize = CHUNKSIZE;

  if (hi->bmax == 0 || ((hi->bsize - hi->blen) < rsize))
  {
    if (hi->b == NULL) hi->b = (byte *)alloc_mem(rsize);
    else hi->b = (byte *)realloc_mem(hi->b, hi->bsize + rsize);
    hi->bsize += rsize;
  }
  StreamRead(hi->ios, hi->b + hi->blen, rsize, func, hip);
  return;
}

/*
 * HTTPReadData
 */
static void
HTTPReadData(ios, len, closure)
ChimeraStream ios;
ssize_t len;
void *closure;
{
  HTTPInfo **hip = (HTTPInfo **)closure;
  HTTPInfo *hi = *hip;
  long rnum;
  char *rmsg;
  
  if (len < 0)
  {
    HTTPFailure(hi);
    return;
  }
  
  if (len > 0) hi->blen += len;
  
  if (len == 0 || ((hi->blen - hi->doff) == hi->bmax && hi->bmax > 0))
  {
    SourceSendMessage(hi->ws, hi->hc->msg[HM_DONE]);
    SourceEnd(hi->ws);
  }
  else
  {
    /*
     * Don't want to return data until we know the final size...realloc()
     * causes trouble.
     */
    if (hi->bmax > 0)
    {
      if (hi->blen - hi->doff > 0) SourceAdd(hi->ws);
      rmsg = hi->hc->msg[HM_RK];
      rnum = (long)(hi->bmax - hi->blen - hi->doff);
    }
    else
    {
      rmsg = hi->hc->msg[HM_RU];
      rnum = (long)(hi->blen - hi->doff);
    }
    
    if (hi->rcount++ % PRINT_RATE == 0)
    {
      snprintf (hi->msgbuf, sizeof(hi->msgbuf), "%ld %s", rnum, rmsg);
      SourceSendMessage(hi->ws, hi->msgbuf);
    }
    
    HTTPRead(hip, HTTPReadData);
  }
  
  return;
}

/*
 * HTTPCheckHeader
 *
 * Scrounges around in the header fields to see if there is any
 * interesting information.
 */
static int
HTTPCheckHeader(hip)
HTTPInfo **hip;
{
  char *value;
  size_t clen;
  char *option;
  char *list;
  URLParts *up, *pup;
  char *url;
  char *username;
  char *cp;
  bool cache;
  HTTPInfo *hi = *hip;

  if (hi->status >= 300) cache = false;
  else cache = true;

  /*
   * Take action on MIME fields.
   */
  if ((MIMEGetField(hi->mh, "location", &value) == 0 && value != NULL) &&
      hi->status >= 300 && hi->status < 400)
  {
    up = URLParse(hi->mp, value);
    pup = NULL;

    /* Just blindly follow the URL unless it should be used as a proxy */
    if (hi->status != 305) up = URLResolve(hi->mp, up, hi->up);
    else
    {
      pup = up;
      up = hi->up;
    }
 
    if (HTTPCreateInfo(hip, hi->ws, hi->wr, hi->hc,
		       up, pup, hi->hp) == -1)
    {
      HTTPFailure(hi);
    }
    HTTPDestroyInfo(hi);
    
    return(-1);
  }
  if (MIMEGetField(hi->mh, "content-length", &value) == 0 && value != NULL)
  {
    clen = (size_t)atoi(value) + hi->doff;
    if (clen > 0 && clen > hi->blen && clen > hi->bsize)
    {
      hi->b = (byte *)realloc_mem(hi->b, clen);
      hi->bsize = clen;
      hi->bmax = clen;
    }
    else hi->bmax = 0;
  }
  if (MIMEGetField(hi->mh, "pragma", &value) == 0 && value != NULL)
  {
    list = value;
    while ((option = mystrtok(list, ' ', &list)) != NULL)
    {
      if (strcasecmp(option, "no-cache") == 0) cache = false;
    }
  }
  if (MIMEGetField(hi->mh, "www-authenticate", &value) == 0 &&
      value != NULL && hi->hp == NULL)
  {
    cache = false;
    
    /*
     * Look for the authorization type and realm
     */
    list = value;
    while ((option = mystrtok(list, ' ', &list)) != NULL)
    {
      if (strcasecmp(option, "basic") == 0)
      {
	hi->auth_type = MPStrDup(hi->mp, option);
      }
      else if (strncasecmp(option, "realm=", 6) == 0)
      {
	for (cp = option; *cp != '\0'; cp++)
	{
	  if (*cp == '=')
	  {
	    hi->realm = MPStrDup(hi->mp, cp + 1);
	    break;
	  }
	}
      }
    }
    
    /*
     * If there is an authorization type and realm then try to
     * see if the password is already known.  If not then ask the user
     * for a username and password.
     */
    if (hi->auth_type != NULL && hi->realm != NULL)
    {
      if ((hi->hp = HTTPFindPassword(hi, hi->realm,
				     hi->up->hostname,
				     hi->up->port)) != NULL)
      {
	if (HTTPCreateInfo(hip, hi->ws, hi->wr, hi->hc,
			   hi->up, hi->pup, hi->hp) == -1)
	{
	  HTTPFailure(hi);
	}
	HTTPDestroyInfo(hi);
	return(-1);
      }
      else
      {
	if (hi->up->username != NULL)
	{
	  if (hi->up->password != NULL)
	  {
	    HTTPAuthCallback(hip, hi->up->username,
			     hi->up->password);
	  }
	  else username = hi->up->username;
	}
	else username = "";
	
	hi->wa = AuthCreate(hi->cres, "Enter password", username,
			    HTTPAuthCallback, hip);

	/*
	 * If the authorization context is created then return -1 to
	 * indicate that is the end of the transaction.  If the context
	 * is not created then pass through so the auth message will
	 * appear.
	 */
	if (hi->wa != NULL) return(-1);
      }
    }
  }
  
  if (MIMEGetField(hi->mh, "content-type", &value) != 0 || value == NULL)
  {
    if ((value = ChimeraExt2Content(hi->cres, HTTPGetFilename(hi))) == NULL)
    {
      value = "text/html";
    }
    MIMEAddField(hi->mh, "content-type", value);
  }
  
  if (hi->url != NULL) url = hi->url;
  else url = "unknown:/";

  MIMEAddField(hi->mh, "x-url", url);
  
  /* Do not call this before dealing with "Location:" */
  SourceInit(hi->ws, cache && hi->hp == NULL);
  
  return(0);
}

/*
 * HTTPReadHeader
 */
void
HTTPReadHeader(ios, len, closure)
ChimeraStream ios;
ssize_t len;
void *closure;
{
  HTTPInfo **hip = (HTTPInfo **)closure;
  HTTPInfo *hi = *hip;
  char *cp;
  ssize_t i;
  size_t moff = 0;

  if (len <= 0)
  {
    HTTPFailure(hi);
    return;
  }

  if (MIMEFindData(hi->mh, hi->b, hi->blen + len, &(hi->doff)) == 0)
  {
    /*
     * Look for the end of the first line.
     */
    for (i = 0, cp = (char *)hi->b; i < hi->doff; i++, cp++)
    {
      if (*cp == '\n')
      {
	moff = i + 1;
	break;
      }
    }

    myassert(i < hi->doff, "MIMEFindData must be broken");

    if (sscanf((char *)hi->b, "HTTP/%d.%d %d",
	       &hi->major, &hi->minor, &hi->status) != 3)
    {
      HTTPFailure(hi);
      return;
    }

    MIMEParseBuffer(hi->mh, hi->b + moff, hi->doff - moff);

    if (HTTPCheckHeader(hip) == -1) return;

    HTTPReadData(ios, len, closure);
  }
  else
  {
    snprintf (hi->msgbuf, sizeof(hi->msgbuf),
	      "%ld %s", (long)hi->blen, hi->hc->msg[HM_RU]);
    SourceSendMessage(hi->ws, hi->msgbuf);

    hi->blen += len;

    HTTPRead(hip, HTTPReadHeader);
  }

  return;
}

/*
 * HTTPReadUnknown
 */
static void
HTTPReadUnknown(ios, len, closure)
ChimeraStream ios;
ssize_t len;
void *closure;
{
  HTTPInfo **hip = (HTTPInfo **)closure;
  HTTPInfo *hi = *hip;
  char *content;
  size_t nblen;

  if (len < 0 || (len == 0 && hi->blen == 0))
  {
    HTTPFailure(hi);
    return;
  }

  nblen = hi->blen + len;

  if (nblen < 5 && len > 0)
  {
    hi->blen = nblen;

    /* Not enough information to know the HTTP version */
    HTTPRead(hip, HTTPReadUnknown);
  }
  else if (nblen >= 5 && strncmp("HTTP/", (char *)hi->b, 5) == 0)
  {
    HTTPReadHeader(ios, len, hip);
  }
  else
  {
    /* Must be HTTP/0.9 */
    if ((content = ChimeraExt2Content(hi->cres, HTTPGetFilename(hi))) == NULL)
    {
      content = "text/html";
    }

    MIMEAddField(hi->mh, "content-type", content);
    MIMEAddField(hi->mh, "x-url", hi->url);

    SourceInit(hi->ws, true);

    HTTPReadData(ios, len, hip);
  }

  snprintf (hi->msgbuf, sizeof(hi->msgbuf),
	    "%ld %s", (long)hi->blen, hi->hc->msg[HM_RU]);
  SourceSendMessage(hi->ws, hi->msgbuf);

  return;
}

/*
 * HTTPRequestDone
 */
static void
HTTPRequestDone(ios, rval, closure)
ChimeraStream ios;
ssize_t rval;
void *closure;
{
  byte *rdata;
  ssize_t rlen;
  HTTPInfo **hip = (HTTPInfo **)closure;
  HTTPInfo *hi = *hip;

  if (rval == -1)
  {
    HTTPFailure(hi);
    return;
  }

  if ((rdata = HTTPBuildRequest(hi, &rlen)) == NULL)
  {
    if (rlen == -1) HTTPFailure(hi);
    else HTTPRead(hip, HTTPReadUnknown);
  }
  else
  {
    StreamWrite(hi->ios, rdata, rlen, HTTPRequestDone, hip);
  }

  return;
}

/*
 * HTTPDestroy
 */
static void
HTTPDestroy(closure)
void *closure;
{
  HTTPInfo **hip = (HTTPInfo **)closure;

  HTTPDestroyInfo(*hip);
  free_mem(hip);

  return;
}

/*
 * HTTPDestroyInfo
 */
static void
HTTPDestroyInfo(hi)
HTTPInfo *hi;
{
  HTTPCancel(&hi);
  if (hi->rmp != NULL) MPDestroy(hi->rmp);
  if (hi->b != NULL) free_mem(hi->b);
  if (hi->mh != NULL) MIMEDestroyHeader(hi->mh);
  MPDestroy(hi->mp);

  return;
}

/*
 * HTTPCreateInfo
 */
static int
HTTPCreateInfo(hip, ws, wr, hc, up, pup, hp)
HTTPInfo **hip;
ChimeraSource ws;
ChimeraRequest *wr;
HTTPClass *hc;
URLParts *up, *pup;
HTTPPassword *hp;
{
  char *hostname;
  int port;
  HTTPInfo *hi;
  MemPool mp;

  mp = MPCreate();
  *hip = hi = (HTTPInfo *)MPCGet(mp, sizeof(HTTPInfo));
  hi->mp = mp;
  hi->hp = hp;
  hi->rmp = MPCreate();
  hi->ws = ws;
  hi->wr = wr;
  hi->cres = SourceToResources(ws);
  hi->hc = hc;
  hi->mh = MIMECreateHeader();
  hi->up = URLDup(mp, up);
  hi->url = URLMakeString(hi->mp, hi->up, true);
  if (pup != NULL) hi->pup = URLDup(mp, pup);
  else hi->pup = NULL;

  if (hi->pup != NULL)
  {
    hostname = pup->hostname;
    port = pup->port;
  }
  else
  {
    hostname = up->hostname;
    port = up->port;
  }

  snprintf (hi->msgbuf, sizeof(hi->msgbuf),
	    "%s %s", hc->msg[HM_OPEN], hostname);
  SourceSendMessage(hi->ws, hi->msgbuf);

  hi->ios = StreamCreateINet(hi->cres, hostname, port == 0 ? 80:port);
  if (hi->ios == NULL)
  {
    HTTPDestroyInfo(hi);
    return(-1);
  }

  HTTPRequestDone(hi->ios, 0, (void *)hip);

  return(0);
}

/*
 * HTTPInit
 */
static void *
HTTPInit(ws, wr, class_closure)
ChimeraSource ws;
ChimeraRequest *wr;
void *class_closure;
{
  HTTPClass *hc = (HTTPClass *)class_closure;
  HTTPInfo **hip;

  hip = (HTTPInfo **)alloc_mem(sizeof(HTTPInfo **));

  if (HTTPCreateInfo(hip, ws, wr, hc, wr->up, wr->pup, NULL) == -1)
  {
    free_mem(hip);
    return(NULL);
  }

  return(hip);
}

/*
 * HTTPCancel
 *
 * Used to obliterate a connection.  Use this when an error occurs which
 * could cause a connection to get messed up or at least a pain to
 * figure out.
 */
void
HTTPCancel(closure)
void *closure;
{
  HTTPInfo *hi = *((HTTPInfo **)closure);

  if (hi->wa != NULL)
  {
    AuthDestroy(hi->wa);
    hi->wa = NULL;
  }

  if (hi->ios != NULL)
  {
    StreamDestroy(hi->ios);
    hi->ios = NULL;
  }
  
  return;
}

/*
 * HTTPClassDestroy
 */
static void
HTTPClassDestroy(closure)
void *closure;
{
  HTTPClass *hc = (HTTPClass *)closure;
  MPDestroy(hc->mp);
  return;
}

/*
 * HTTPGetData
 */
static void
HTTPGetData(closure, data, len, mh)
void *closure;
byte **data;
size_t *len;
MIMEHeader *mh;
{
  HTTPInfo *hi = *((HTTPInfo **)closure);

  *data = hi->b + hi->doff;
  *len = hi->blen - hi->doff;
  *mh = hi->mh;

  return;
}


/*
 * InitModule_HTTP
 */
void
InitModule_HTTP(cres)
ChimeraResources cres;
{
  ChimeraSourceHooks ph;
  HTTPClass *hc;
  MemPool mp;
  size_t tlen;
  int i;

  mp = MPCreate();
  hc = (HTTPClass *)MPCGet(mp, sizeof(HTTPClass));
  hc->mp = mp;
  hc->passwords = GListCreateX(mp);

  /* get the status messages and allocate a message work buffer */
  for (i = 0; http_messages[i].name != NULL; i++)
  {
    if ((hc->msg[i] = ResourceGetString(cres, http_messages[0].name)) == NULL)
    {
      hc->msg[i] = http_messages[i].def;
    }
    if ((tlen = strlen(hc->msg[i])) > hc->mlen) hc->mlen = tlen;    
  }

  memset(&ph, 0, sizeof(ph));
  ph.class_closure = hc;
  ph.name = "http";
  ph.init = HTTPInit;
  ph.destroy = HTTPDestroy;
  ph.stop = HTTPCancel;
  ph.getdata = HTTPGetData;
  ph.class_destroy = HTTPClassDestroy;
  SourceAddHooks(cres, &ph);

  return;
}
