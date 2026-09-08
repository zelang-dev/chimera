/*
 * Chimera.h
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
#ifndef __CHIMERA_H__
#define __CHIMERA_H__ 1

#include "common.h"
#include "url.h"

typedef struct ChimeraResourcesP   *ChimeraResources;
typedef struct ChimeraContextP     *ChimeraContext;

typedef struct ChimeraTaskP      *ChimeraTask;
typedef void (*ChimeraTaskProc) _ArgProto((void *));

#define TaskSchedule(a, b, c)    TaskScheduleX(a, b, c, __LINE__, __FILE__);

ChimeraTask TaskScheduleX _ArgProto((ChimeraResources,
				     ChimeraTaskProc, void *, int, char *));
void TaskRemove _ArgProto((ChimeraResources, ChimeraTask));

typedef struct ChimeraTimeOutP *ChimeraTimeOut;
typedef void (*ChimeraTimeOutProc) _ArgProto((ChimeraTimeOut, void *));

ChimeraTimeOut TimeOutCreate _ArgProto((ChimeraResources, unsigned int,
					ChimeraTimeOutProc, void *));
void TimeOutDestroy _ArgProto((ChimeraTimeOut));

char *ResourceGetString _ArgProto((ChimeraResources, char *));
int ResourceAddFile _ArgProto((ChimeraResources, char *));
int ResourceAddString _ArgProto((ChimeraResources, char *));
char *ResourceGetBool _ArgProto((ChimeraResources, char *, bool *));
char *ResourceGetInt _ArgProto((ChimeraResources, char *, int *));
char *ResourceGetUInt _ArgProto((ChimeraResources, char *, unsigned int *));
char *ResourceGetFilename _ArgProto((ChimeraResources, MemPool, char *));

void CMethodVoidDoom();
void *CMethodVoidPtrDoom();
int CMethodIntDoom();
char CMethodCharDoom();
char *CMethodCharPtrDoom();
byte *CMethodBytePtrDoom();
bool CMethodBoolDoom();

#define CMethod(x) ((x) != NULL ? (x):CMethodVoidDoom)
#define CMethodChar(x) ((x) != NULL ? (x):CMethodCharDoom)
#define CMethodCharPtr(x) ((x) != NULL ? (x):CMethodCharPtrDoom)
#define CMethodBytePtr(x) ((x) != NULL ? (x):CMethodBytePtrDoom)
#define CMethodBool(x) ((x) != NULL ? (x):CMethodBoolDoom)
#define CMethodInt(x) ((x) != NULL ? (x):CMethodIntDoom)
#define CMethodVoid(x) ((x) != NULL ? (x):CMethodVoidDoom)
#define CMethodVoidPtr(x) ((x) != NULL ? (x):CMethodVoidPtrDoom)

#endif
