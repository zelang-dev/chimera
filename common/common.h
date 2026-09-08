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

/* mempool.c */
typedef struct MemPoolP *MemPool;

#define MPCreate() MPCreateTrack(__LINE__, __FILE__)
MemPool MPCreateTrack _ArgProto((int, char *));
void *MPGet _ArgProto((MemPool, size_t));
void *MPCGet _ArgProto((MemPool, size_t));
char *MPStrDup _ArgProto((MemPool, const char *));
void MPDestroy _ArgProto((MemPool));
void MPPrintStatus _ArgProto((void));

/* dmem.c */
void free_mem _ArgProto((void *));
void *alloc_mem _ArgProto((size_t));
void *calloc_mem _ArgProto((size_t, size_t));
void *realloc_mem _ArgProto((void *, size_t));
char *alloc_string _ArgProto((char *));

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
