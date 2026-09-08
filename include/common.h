/*
 * common.h
 *
 * Copyright (c) 1996-1997, John Kilburg <john@cs.unlv.edu>
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

#ifndef __COMMON_H__
#define __COMMON_H__ 1

#include <sys/types.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>

#ifdef __QNX__
#define strcasecmp(s1, s2) stricmp(s1, s2)
#define strncasecmp(s1, s2, n) strnicmp(s1, s2, n)
#endif

#ifdef __EMX__
#define strcasecmp(s1, s2) stricmp(s1, s2)
#define strncasecmp(s1, s2, n) strnicmp(s1, s2, n)
#endif


/*
 * ANSI-C stuff.  Snarfed from xloadimage.
 */

#ifdef __STDC__
#if !defined(_ArgProto)
#define _ArgProto(ARGS) ARGS
#endif
#else /* !__STDC__ */
/* "const" is an ANSI thing */
#if !defined(const)
#define const
#endif

#if !defined(_ArgProto)
#define _ArgProto(ARGS) ()
#endif

#endif /* __STDC__ */

/*
 * If you have an old version of C++ without bool, you'll have to add
 * the ifdef here
 */
#ifndef __cplusplus

#if !defined(bool_DEFINED) && !defined(true_DEFINED) && !defined(false_DEFINED)
#define bool_DEFINED 1
#define true_DEFINED 1
#define false_DEFINED 1
typedef enum {
	false = 0,
	true = 1
} bool;
#endif

#ifndef bool_DEFINED
#define bool_DEFINED 1
typedef int bool;
#endif

#ifndef true_DEFINED
#define true_DEFINED 1
#define true 1
#endif

#ifndef false_DEFINED
#define false_DEFINED 1
#define false 0
#endif

#endif /* __cplusplus */

#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

/*
 * should be in limits.h
 */
#ifndef CHAR_BITS
#define CHAR_BITS 8
#endif

#ifndef byte_DEFINED
#define byte_DEFINED 1
typedef unsigned char byte;
#endif

#ifndef word_DEFINED
#define word_DEFINED 1
typedef unsigned long word;
#endif

/*
 * FILENAME_MAX apparently doesn't cut it.  I don't see a decent
 * alternative.  Invent one.  If it clashes or isn't long enough then
 * try again.  Repeat as necessary.
 */
#define MAXFILENAMELEN 1024

#define isspace8(a) ((a) < 33 && (a) >= 0)

#ifndef HAVE_SNPRINTF
#ifdef HAVE_STDARG_H
int snprintf _ArgProto((char *, size_t, const char *, ...));
#endif
#endif

/* mempool.c */
typedef struct MemPoolP *MemPool;

#define MPCreate() MPCreateTrack(__LINE__, __FILE__)
MemPool MPCreateTrack _ArgProto((int, char *));
void *MPGet _ArgProto((MemPool, size_t));
void *MPCGet _ArgProto((MemPool, size_t));
char *MPStrDup _ArgProto((MemPool, const char *));
void MPDestroy _ArgProto((MemPool));
void MPPrintStatus _ArgProto((void));

/* util.c */
char *FixPath _ArgProto((MemPool, char *));
char *mystrtok _ArgProto((char *, size_t, char **));
char *GetBaseFilename _ArgProto((char *));
char *mytmpnam _ArgProto((MemPool));
#ifdef NDEBUG
#define myassert(a, b)
#else
#define myassert(a, b) xmyassert(a, b, __FILE__, __LINE__)
void xmyassert _ArgProto((bool, char *, char *, int));
#endif

/* dir.c */
char *compress_path _ArgProto((MemPool, char *, char *));
char *whack_filename _ArgProto((MemPool, char *));

/* list.c */
typedef struct GListP *GList;

#define GListCreate() GListCreateTrack(__LINE__, __FILE__)
GList GListCreateTrack _ArgProto((int, char *));
GList GListCreateX _ArgProto((MemPool));
void GListAddHead _ArgProto((GList, void *));
#define GListPush(a, b) GListAddHead(a, b)
void GListAddTail _ArgProto((GList, void *));
void GListDestroy _ArgProto((GList));
void *GListGetHead _ArgProto((GList));
void *GListGetTail _ArgProto((GList));
void *GListGetNext _ArgProto((GList));
void *GListGetPrev _ArgProto((GList));
void GListRemoveItem _ArgProto((GList, void *));
bool GListEmpty _ArgProto((GList));
void *GListPop _ArgProto((GList));
void *GListGetCurrent _ArgProto((GList));
void GListPrintStatus _ArgProto((void));
void GListClear _ArgProto((GList));
void *GListCurrent _ArgProto((GList));

/* uproc.c */
void StartReaper _ArgProto((void));
int PipeCommand _ArgProto((char *, int *));

#endif /* __COMMON_H__ */
