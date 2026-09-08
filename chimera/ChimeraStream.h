/*
 * ChimeraStream.h
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
#ifndef __CHIMERASTREAM_H__
#define __CHIMERASTREAM_H__ 1

typedef struct ChimeraStreamP    *ChimeraStream;

/*
 * io.c
 */
typedef void (*ChimeraStreamCallback) _ArgProto((ChimeraStream,
						 ssize_t, void *));

ChimeraStream StreamCreateINet _ArgProto((ChimeraResources, char *, int));
ChimeraStream StreamCreateINet2 _ArgProto((ChimeraResources));
ChimeraStream StreamCreateFD _ArgProto((ChimeraResources, int));
void StreamDestroy _ArgProto((ChimeraStream));
void StreamWrite _ArgProto((ChimeraStream, byte *, size_t,
			    ChimeraStreamCallback, void *));
void StreamRead _ArgProto((ChimeraStream, byte *, size_t,
			   ChimeraStreamCallback, void *));
int StreamGetINetPort _ArgProto((ChimeraStream));
unsigned long StreamGetINetAddr _ArgProto((ChimeraStream));

#endif
