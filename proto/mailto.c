/*
 * mailto.c
 *
 * Copyright 1997, Dave Davey <daved@physiol.usyd.edu.au>
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

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>

#include <X11/Xaw/Form.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Label.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/param.h>

#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif

#include "port_after.h"

#include "../mxw/TextField.h"

#include "common.h"

#include "../mxw/MyDialog.h"

#include "Chimera.h"
#include "ChimeraSource.h"

#include "mime.h"


/*
 * hacked by john a little bit so you know where the blames goes.
 * status messages are ugly.
 */

typedef struct MailtoInfoP MailtoInfo;
typedef struct MailtoActionInfoP MailtoActionInfo;

char *URLUnescape(MemPool mp, char *url);

static char *sendmail _ArgProto((ChimeraResources, char *));

void InitModule_Mailto _ArgProto((ChimeraResources));
static void MailtoClassDestroy _ArgProto((void *));
static void *MailtoInit _ArgProto((ChimeraSource, ChimeraRequest *, void *));
static void MailtoCancel _ArgProto((void *));
static void MailtoDestroy _ArgProto((void *));
static void MailtoGetData _ArgProto((void *, byte **, size_t *, MIMEHeader *));
static void MakeMailtoForm _ArgProto((void *));

static void *MailtoActionInit _ArgProto((ChimeraSource,
					 ChimeraRequest *, void *));
static void MailtoActionCancel _ArgProto((void *));
static void MailtoActionDestroy _ArgProto((void *));
static void MailtoActionGetData _ArgProto((void *,
					   byte **, size_t *, MIMEHeader *));

typedef struct
{
  MemPool mp;
  char *header;
  char *trailer;
} MailtoClass;

struct MailtoInfoP
{
  MemPool mp;
  ChimeraSource ws;
  ChimeraResources cres;
  ChimeraRequest *wr;
  MailtoClass *mc;
  ChimeraTask wt;
  byte *data;
  size_t len;
  byte *address;
  MIMEHeader mh;
};

struct MailtoActionInfoP
{
  MemPool mp;
  ChimeraSource ws;
  ChimeraResources cres;
  ChimeraRequest *wr;
  ChimeraTask wt;
  byte *data;
  size_t len;
  MIMEHeader mh;
};

/*
 * InitModule_Mailto
 */
void
InitModule_Mailto(cres)
ChimeraResources cres;
{
  ChimeraSourceHooks ph;
  MailtoClass *mc;
  MemPool mp;

  mp = MPCreate();
  mc = (MailtoClass *)MPCGet(mp, sizeof(MailtoClass));
  mc->mp = mp;
  mc->header = ResourceGetString(cres, "mailto.dirheader");
  if (mc->header == NULL)
  {
    mc->header = "<html><body><h2>Mailto</h2><ul>";
  }

  mc->trailer = ResourceGetString(cres, "mailto.dirtrailer");
  if (mc->trailer == NULL) mc->trailer = "</body></html>";

  memset(&ph, 0, sizeof(ph));
  ph.class_closure = mc;
  ph.class_destroy = MailtoClassDestroy;
  ph.name = "mailto";
  ph.init = MailtoInit;
  ph.destroy = MailtoDestroy;
  ph.stop = MailtoCancel;
  ph.getdata = MailtoGetData;
  SourceAddHooks(cres, &ph);
  
  memset(&ph, 0, sizeof(ph));
  ph.name = "x-mailtoaction";
  ph.init = MailtoActionInit;
  ph.destroy = MailtoActionDestroy;
  ph.stop = MailtoActionCancel;
  ph.getdata = MailtoActionGetData;
  SourceAddHooks(cres, &ph);
  
  return;
}

/*
 * MailtoClassDestroy
 */
static void
MailtoClassDestroy(closure)
void *closure;
{
  MailtoClass *mc = (MailtoClass *)closure;
  MPDestroy(mc->mp);
  return;
}

/*
 * MailtoInit
 */
static void *
MailtoInit(ws, wr, class_closure)
ChimeraSource ws;
ChimeraRequest *wr;
void *class_closure;
{
	/*
	** get here when click on mailto link
	** wr->url points to url including "mailto"
	*/
  MailtoInfo *mi;
  MemPool mp;
  ChimeraTaskProc func;
  char *p;

  if((p = strchr(wr->url, ':')) == NULL)
	return(NULL);
  mp = MPCreate();
  mi = (MailtoInfo *)MPCGet(mp, sizeof(MailtoInfo));
  mi->address = p+1;
  mi->mp = mp;
  mi->ws = ws;
  mi->wr = wr;
  mi->cres = SourceToResources(ws);

  mi->mc = (MailtoClass *)class_closure;
  func = MakeMailtoForm;
  mi->wt = TaskSchedule(mi->cres, func, mi);
  mi->mh = MIMECreateHeader();
  MIMEAddField(mi->mh, "content-type", "text/html");
  MIMEAddField(mi->mh, "x-url", mi->wr->url);
  return(mi);
}

/*
 * MailtoCancel
 */
static void
MailtoCancel(closure)
void *closure;
{
  /* nothing to do here ? */
  /* what if sendmail "hangs" ? */
  return;
}

/*
 * MailtoDestroy
 */
static void
MailtoDestroy(closure)
void *closure;
{
  MailtoInfo *mi = (MailtoInfo *)closure;
  if (mi->wt != NULL) TaskRemove(mi->cres, mi->wt);
  MPDestroy(mi->mp);
  return;
}

static void
MailtoGetData(closure, data, len, mh)
void *closure;
byte **data;
size_t *len;
MIMEHeader *mh;
{
  MailtoInfo *mi = (MailtoInfo *)closure;

  *data = mi->data;
  *len = mi->len;
  *mh = mi->mh;
  
  return;
}

enum
{
	FROM,
	TO,
	CC,
	SUBJECT,
	MESSAGE
};
struct MailHeader
{
	char	*name;
	char	*contents;
	char	*input_type;
} MailHeaders[] =
{
	{ "From",	"",	"INPUT size=64"	},
	{ "To",		"",	"INPUT size=64"	},
	{ "Cc",		"",	"INPUT size=64"	},
	{ "Subject",	"",	"INPUT size=64"	},
	{ "Message",	"",	"TEXTAREA rows=20 cols=64" },
	{ NULL,		NULL	}
};
/*
 * MakeMailForm
 */
static void
MakeMailtoForm(closure)
void *closure;
{
  char *f;
  MailtoInfo *mi = (MailtoInfo *)closure;
  struct passwd *p;
#ifdef HAVE_UNAME
  struct utsname uts;
#endif
  char *sender;
  char *username;
  struct MailHeader *mh;

  if ((sender = ResourceGetString(mi->cres, "user.email")) == NULL)
  {
#ifdef HAVE_UNAME

    if((p = getpwuid(getuid())) == NULL) username = "nobody";
    else username = p->pw_name;

    uname(&uts);
    sender = (char *)MPCGet(mi->mp, strlen(username) +
                                    strlen(uts.nodename) + 2);
    strcpy(sender, username);
    strcat(sender, "@");
    strcat(sender, uts.nodename);
#else
    sender = "";
#endif
  }

  MailHeaders[FROM].contents = sender;
  MailHeaders[TO].contents = mi->address;
  MailHeaders[CC].contents = "";
  MailHeaders[SUBJECT].contents = mi->wr->parent_url;
  MailHeaders[MESSAGE].contents = "";

  f = (char *)MPCGet(mi->mp, BUFSIZ);
  strcpy(f, "<HTML>\n<HEAD>\n<TITLE>Chimera Mail</TITLE>\n</HEAD>\n");
  strcat(f, "<BODY>\n<H1>Chimera Mail</H1>\n<HR>\n");
  strcat(f, "<FORM action=\"x-mailtoaction:void\">\n<TABLE>\n");

  for(mh = MailHeaders; mh->name != NULL; mh++)
  {
	strcat(f, "<TR><TD>\n<P>");
	strcat(f, mh->name);
	strcat(f, ":</TD><TD>\n<");
	strcat(f, mh->input_type);
	strcat(f, " name=\"");
	strcat(f, mh->name);
	strcat(f, "\" type=\"text\" value=\"");
	if(mh->contents)
		strcat(f, mh->contents);
	strcat(f, "\">\n");
	strcat(f, "</TD></TR>\n");
  }

  strcat(f, "</TABLE>\n");

  strcat(f, "<INPUT type=submit name=SendMail value=\"Send mail\">\n");
  strcat(f, "<INPUT type=submit name=Cancel value=\"Cancel\">\n");
  strcat(f, "</FORM>\n</BODY>\n</HTML>\n");
  
  mi->data = (byte *)f;
  mi->len = strlen(f);
  SourceInit(mi->ws, false);
  SourceEnd(mi->ws);

  mi->wt = NULL;
  return;
}

static void
MailtoAction(closure)
void *closure;
{
  MailtoActionInfo *mi = (MailtoActionInfo *)closure;
  char *f;

  if (strstr(mi->wr->input_data, "Cancel") != NULL)
  {
    f = "mail cancelled by the user";
  }
  else if (strstr(mi->wr->input_data, "SendMail") != NULL)
  {
    f = sendmail(mi->cres, mi->wr->input_data);
  }
  else f = "unexpected input_data";

  mi->data = (byte *)f;
  mi->len = strlen(f);
  SourceInit(mi->ws, false);
  SourceEnd(mi->ws);

  mi->wt = NULL;

  return;
}

static char *
sendmail(cres, data)
ChimeraResources cres;
char *data;
{
  MemPool mp;
  FILE	*fp;
  char	*p;
  struct MailHeader *mh;
  char *mailer;
  char *f;

  if ((mailer = ResourceGetString(cres, "mailto.mailer")) == NULL)
  {
#ifdef __EMX__
    mailer = "sendmail -t";
#else
    mailer = "/usr/lib/sendmail -t";
#endif
  }

  if ((fp = popen(mailer, "w")) != NULL)
  {
    mp = MPCreate();
    mh = MailHeaders;
    
    for(mh = MailHeaders; mh->name != NULL; mh++)
    {
      p = strstr(data, mh->name) + strlen(mh->name) + 1;
      data = strchr(data, '&');
      *data++ = '\0';
      mh->contents = URLUnescape(mp, p);
      if(strcmp(mh->name, "Message") == 0)
	  fprintf(fp, "X-Mailer: Chimera\n\n");
      else
      {
	if(mh->contents == NULL || strlen(mh->contents) == 0)
	    continue;
	fprintf(fp,"%s: ", mh->name);
      }
      fprintf(fp, "%s\n", mh->contents);
    }
    /* signature ?? */
    pclose(fp);
    MPDestroy(mp);

    f = "mail sent";
  }
  else f = "popen() failed";

  return(f);
}

/*
 * MailtoActionInit
 */
static void *
MailtoActionInit(ws, wr, class_closure)
ChimeraSource ws;
ChimeraRequest *wr;
void *class_closure;
{
	/*
	** get here when click on mailto link
	** wr->url points to url including "x-mailtoaction"
	*/
  MailtoActionInfo *mi;
  MemPool mp;
  ChimeraTaskProc func;

  mp = MPCreate();
  mi = (MailtoActionInfo *)MPCGet(mp, sizeof(MailtoActionInfo));
  mi->mp = mp;
  mi->ws = ws;
  mi->wr = wr;
  mi->cres = SourceToResources(ws);

  func = MailtoAction;
  mi->wt = TaskSchedule(mi->cres, func, mi);
  mi->mh = MIMECreateHeader();
  MIMEAddField(mi->mh, "content-type", "text/plain");
  MIMEAddField(mi->mh, "x-url", mi->wr->url);

  return(mi);
}

/*
 * MailtoActionCancel
 */
static void
MailtoActionCancel(closure)
void *closure;
{
  /* nothing to do here ? */
  /* what if sendmail "hangs" ? */
  return;
}

/*
 * MailtoActionDestroy
 */
static void
MailtoActionDestroy(closure)
void *closure;
{
  MailtoActionInfo *mi = (MailtoActionInfo *)closure;
  if (mi->wt != NULL) TaskRemove(mi->cres, mi->wt);
  MPDestroy(mi->mp);
  return;
}

static void
MailtoActionGetData(closure, data, len, mh)
void *closure;
byte **data;
size_t *len;
MIMEHeader *mh;
{
  MailtoActionInfo *mi = (MailtoActionInfo *)closure;

  *data = mi->data;
  *len = mi->len;
  *mh = mi->mh;
  
  return;
}
