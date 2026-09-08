/*
 * file.c
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
#include <ctype.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#if defined(SYSV) || defined(SVR4) || defined(__arm) || defined(__QNX__)
#include <dirent.h>
#define DIRSTUFF struct dirent
#else
#include <sys/dir.h>
#define DIRSTUFF struct direct
#endif

#include "port_after.h"

#include "Chimera.h"
#include "ChimeraSource.h"
#include "ChimeraAuth.h"
#include "mime.h"

#define PRINT_RATE 10
#define MSGLEN 4096          /* this needs to be big */

/* Jim Rees fix */
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISBLK
#define S_ISBLK(mode) (((mode) & S_IFMT) == S_IFBLK)
#endif

#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISFIFO
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#endif

#ifndef S_ISCHR
#define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
#endif

/* not POSIX but what the heck */
#ifndef S_ISLNK
#define S_ISLNK(mode) (((mode) & S_IFMT) == S_IFLNK)
#endif

typedef struct
{
  FILE *fp;
  off_t size;
} RegInfo;

typedef struct
{
  DIR *dp;
  int size;
  int used;
  char *dirname;
  char **sa;
  char *direntry;
} DirInfo;

typedef struct
{
  MemPool mp;
  bool directory;
  bool stopped;
  ChimeraSource ws;
  ChimeraResources cres;
  ChimeraTask wt;
  ChimeraRequest *wr;

  char *filename;

  DirInfo di;
  RegInfo ri;
  bool init_done;
  char mbuf[MSGLEN];
  char *rstr;
  int pcount;

  byte *data;
  size_t len;
  MIMEHeader mh;
} FileInfo;

static void DirRead _ArgProto((void *));
static void FileRead _ArgProto((void *));
static void FileGetData _ArgProto((void *, byte **, size_t *, MIMEHeader *));
static int local_strcmp _ArgProto((const void *, const void *));
static void DirEOF _ArgProto((FileInfo *));
static void FileCancel _ArgProto((void *));
static void FileDestroy _ArgProto((void *));
static void *FileInit _ArgProto((ChimeraSource, ChimeraRequest *, void *));
void InitModule_File _ArgProto((ChimeraResources));

static int
local_strcmp(a, b)
const void *a, *b;
{
  return(strcmp(*((char **)a), *((char **)b)));
}

/*
 * DirEOF
 */
static void
DirEOF(fi)
FileInfo *fi;
{
  int i;
  char *f;
  char **sa = fi->di.sa;
  char *cp;
  int flen;
  static char *header1 = "";
  static char *header2 = "<ul>\n";
  static char *trailer1 = "\n</ul>";
  static char *format = "<li><a href=file:%s%s>%s</a>%s\n";

  if (fi->di.used > 0) qsort(sa, fi->di.used, sizeof(char *), local_strcmp);

  for (i = 0, flen = 0; i < fi->di.used; i++)
  {
    flen += 2 * strlen(sa[i]) + strlen(format) + strlen(fi->di.dirname);
  }
  flen += strlen(header1) + strlen(header2) + strlen(trailer1) + sizeof(char);

  f = (char *)alloc_mem(flen);
  strcpy(f, header1);
  strcat(f, header2);
  for (i = 0; i < fi->di.used; i++)
  {
    for (cp = sa[i]; *cp != '\0'; cp++)
    {
      if (isspace8(*cp)) break;
    }
    *cp++ = '\0';
    snprintf(f + strlen(f), flen - strlen(f),
	     format, fi->di.dirname, sa[i], sa[i], cp); 
  }
  strcat(f, trailer1);

  fi->data = (byte *)f;
  fi->len = strlen(f);
  fi->init_done = true;
  SourceInit(fi->ws, false);
  if (!fi->stopped) SourceEnd(fi->ws);

  return;
}

/*
 * DirRead
 */
static void
DirRead(closure)
void *closure;
{
  FileInfo *fi = (FileInfo *)closure;
  DIRSTUFF *de;
  int salen;
  struct stat fs;

  if ((de = readdir(fi->di.dp)) == NULL)
  {
    closedir(fi->di.dp);
    fi->di.dp = NULL;
    DirEOF(fi);
    fi->wt = NULL;
    return;
  }

  strcpy(fi->mbuf, fi->di.dirname);
  strcat(fi->mbuf, de->d_name);
  if (stat(fi->mbuf, &fs) != -1)
  {
    if (S_ISREG(fs.st_mode))
    {
      snprintf(fi->mbuf, sizeof(fi->mbuf), " (%ld bytes)", (long)fs.st_size);
    }
    else if (S_ISDIR(fs.st_mode)) strcpy(fi->mbuf, "/");
#ifndef __EMX__
    else if (S_ISLNK(fs.st_mode)) strcpy(fi->mbuf, " &lt;link&gt;");
    else if (S_ISFIFO(fs.st_mode)) strcpy(fi->mbuf, " &lt;pipe&gt;");
    else if (S_ISBLK(fs.st_mode)) strcpy(fi->mbuf, " &lt;block device&gt;");
#endif
    else if (S_ISCHR(fs.st_mode)) strcpy(fi->mbuf, " &lt;char device&gt;");
    else strcpy(fi->mbuf, " &lt;unknown&gt;");
  }
  else strcpy(fi->mbuf, " ");
  
  /*
   * Resize the file entry table if needed.
   */
  if (fi->di.used >= fi->di.size)
  {
    char **nsa;
    nsa = (char **)MPGet(fi->mp, fi->di.size * 2 * sizeof(char *));
    memcpy(nsa, fi->di.sa, fi->di.size * sizeof(char *));
    fi->di.size *= 2;
    fi->di.sa = nsa;
  }
  
  salen = strlen(de->d_name) + strlen(fi->mbuf) + 2 * sizeof(char);
  fi->di.sa[fi->di.used] = (char *)MPGet(fi->mp, salen);
  strcpy(fi->di.sa[fi->di.used], de->d_name);
  strcat(fi->di.sa[fi->di.used], " ");
  strcat(fi->di.sa[fi->di.used], fi->mbuf);
  fi->di.used++;

  if (fi->pcount++ % PRINT_RATE == 0)
  {
    snprintf (fi->mbuf, sizeof(fi->mbuf),
	      fi->rstr, fi->di.used, fi->filename);
    SourceSendMessage(fi->ws, fi->mbuf);
  }

  fi->wt = TaskSchedule(fi->cres, DirRead, fi);

  return;
}

/*
 * FileRead
 */
static void
FileRead(closure)
void *closure;
{
  int readlen;
  FileInfo *fi = (FileInfo *)closure;

  readlen = fi->ri.size - fi->len;
  if (readlen > BUFSIZ) readlen = BUFSIZ;

  if ((readlen = fread(fi->data + fi->len, 1, readlen, fi->ri.fp)) == 0)
  {
    fclose(fi->ri.fp);
    fi->ri.fp = NULL;
    if (fi->init_done) SourceEnd(fi->ws);

    fi->wt = NULL;
  }
  else
  {
    if (fi->len == 0)
    {
      fi->init_done = true;
      SourceInit(fi->ws, false);
      if (fi->stopped) return;
    }
    fi->len += readlen;
    SourceAdd(fi->ws);

    if (fi->pcount++ % PRINT_RATE == 0)
    {
      snprintf (fi->mbuf, sizeof(fi->mbuf),
		fi->rstr, fi->len, fi->filename);
      SourceSendMessage(fi->ws, fi->mbuf);
    }

    fi->wt = TaskSchedule(fi->cres, FileRead, fi);
  }

  return;
}

/*
 * FileCancel
 */
static void
FileCancel(closure)
void *closure;
{
  FileInfo *fi = (FileInfo *)closure;

  fi->stopped = true;
  if (fi->wt != NULL)
  {
    TaskRemove(fi->cres, fi->wt);
    fi->wt = NULL;
  }
  if (fi->di.dp != NULL)
  {
    closedir(fi->di.dp);
    fi->di.dp = NULL;
  }
  if (fi->ri.fp != NULL)
  {
    fclose(fi->ri.fp);
    fi->ri.fp = NULL;
  }

  return;
}

/*
 * FileDestroy
 */
static void
FileDestroy(closure)
void *closure;
{
  FileInfo *fi = (FileInfo *)closure;

  FileCancel(fi);
  if (fi->data != NULL) free_mem(fi->data);
  if (fi->mh != NULL) MIMEDestroyHeader(fi->mh);
  MPDestroy(fi->mp);

  return;
}

/*
 * FileInit
 */
static void *
FileInit(ws, wr, class_closure)
ChimeraSource ws;
ChimeraRequest *wr;
void *class_closure;
{
  FileInfo *fi;
  struct stat s, as, *rs;
  ChimeraTaskProc func;
  char *rname, *drname;
  char *content;
  MemPool mp;
  char *autofile, *autopath, *tpath;
  size_t len;
  char *filename;

  if (wr->up->filename == NULL) filename = "/";
  else filename = wr->up->filename;

  if (stat(filename, &s) == -1) return(NULL);
  rs = &s;
  tpath = filename;

  mp = MPCreate();
  fi = (FileInfo *)MPCGet(mp, sizeof(FileInfo));
  fi->mp = mp;
  fi->ws = ws;
  fi->cres = SourceToResources(ws);
  fi->wr = wr;
  fi->mh = MIMECreateHeader();
  fi->filename = MPStrDup(mp, filename);

  if (S_ISDIR(s.st_mode) &&
      (autofile = ResourceGetString(fi->cres, "file.autoLoad")) != NULL)
  {
    len = strlen(filename);
    autopath = (char *)MPGet(mp, len + strlen(autofile) + strlen("/") + 1);
    strcpy(autopath, fi->filename);
    if (fi->filename[len - 1] != '/') strcat(autopath, "/");
    strcat(autopath, autofile);

    if (stat(autopath, &as) == 0)
    {
      tpath = autopath;
      rs = &as;
    }
  }

  if (S_ISDIR(rs->st_mode))
  {
    fi->directory = true;
    if ((fi->di.dp = opendir(fi->filename)) == NULL)
    {
      FileCancel(fi);
      return(NULL);
    }

    fi->di.dirname = (char *)MPGet(fi->mp, strlen(fi->filename) + 
				   strlen("/") + sizeof(char));
    strcpy(fi->di.dirname, fi->filename);
    if (fi->filename[strlen(fi->filename) - 1] != '/')
    {
      strcat(fi->di.dirname, "/");
    }

    fi->di.size = 512;
    fi->di.sa = (char **)MPGet(fi->mp, sizeof(char **) * fi->di.size);

    rname = "file.readdir";
    drname = "Read %d entries from %s";
    func = DirRead;
    content = "text/html";
  }
  else
  {
    fi->directory = false;
    if ((fi->ri.fp = fopen(tpath, "r")) == NULL)
    {
      FileCancel(fi);
      return(NULL);
    }

    fi->data = (byte *)alloc_mem((size_t)rs->st_size);
    fi->ri.size = rs->st_size;

    rname = "file.readfile";
    drname = "Read %d bytes from %s";
    func = FileRead;
    if ((content = ChimeraExt2Content(fi->cres, tpath)) == NULL)
    {
      content = "text/plain";
    }
  }

  MIMEAddField(fi->mh, "content-type", content);
  MIMEAddField(fi->mh, "x-url", fi->wr->url);

  if ((fi->rstr = ResourceGetString(fi->cres, rname)) == NULL)
  {
    fi->rstr = drname;
  }

  fi->wt = TaskSchedule(fi->cres, func, fi);

  return(fi);
}

static void
FileGetData(closure, data, len, mh)
void *closure;
byte **data;
size_t *len;
MIMEHeader *mh;
{
  FileInfo *fi = (FileInfo *)closure;

  *data = fi->data;
  *len = fi->len;
  *mh = fi->mh;

  return;
}

/*
 * InitModule_File
 */
void
InitModule_File(cres)
ChimeraResources cres;
{
  ChimeraSourceHooks ph;

  memset(&ph, 0, sizeof(ph));
  ph.name = "file";
  ph.init = FileInit;
  ph.destroy = FileDestroy;
  ph.stop = FileCancel;
  ph.getdata = FileGetData;
  SourceAddHooks(cres, &ph);

  return;
}
