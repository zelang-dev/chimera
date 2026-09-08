/*
 * url.h
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
#ifndef __URL_H__
#define __URL_H__ 1

#include "common.h"

/*
 * <scheme>://<net_loc>/<path>;<params>?<query>#<fragment>
 *
 * net_loc = username:password@hostname:port
 */
typedef struct
{
  char *scheme;
  char *username;
  char *password;
  char *hostname;
  int port;
  char *filename;
  char *params;
  char *query;
  char *fragment;
} URLParts;

int URLcmp _ArgProto((URLParts *, URLParts *));
char *URLMakeString _ArgProto((MemPool, URLParts *, bool));
URLParts *URLResolve _ArgProto((MemPool, URLParts *, URLParts *));
URLParts *URLParse _ArgProto((MemPool, char *));
char *URLUnescape _ArgProto((MemPool, char *));
char *URLEscape _ArgProto((MemPool, char *, bool));
URLParts *URLDup _ArgProto((MemPool, URLParts *));
bool URLIsAbsolute _ArgProto((URLParts *));
char *URLBaseFilename _ArgProto((MemPool, URLParts *));
char *URLGetScheme _ArgProto((MemPool, char *));

/*
 * url_translate.c
 */
int URLReadTranslations _ArgProto((GList, char *));
char *URLTranslate _ArgProto((MemPool, GList, char *));

#endif

