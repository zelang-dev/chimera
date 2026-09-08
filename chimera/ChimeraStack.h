/*
 * ChimeraStack.h
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
#ifndef __CHIMERASTACK_H__
#define __CHIMERASTACK_H__

typedef struct ChimeraStackP *ChimeraStack;

ChimeraStack StackCreate _ArgProto((ChimeraStack, int, int,
				    unsigned int, unsigned int,
				    ChimeraRenderActionProc, void *));
char *StackGetCurrentURL _ArgProto((ChimeraStack));
void StackBack _ArgProto((ChimeraStack));
void StackForward _ArgProto((ChimeraStack));
void StackHome _ArgProto((ChimeraStack));
void StackOpen _ArgProto((ChimeraStack, ChimeraRequest *));
void StackReload _ArgProto((ChimeraStack));
void StackCancel _ArgProto((ChimeraStack));
void StackRedraw _ArgProto((ChimeraStack));
void StackDestroy _ArgProto((ChimeraStack));
ChimeraStack StackFromGUI _ArgProto((ChimeraContext, ChimeraGUI));
ChimeraStack StackGetParent _ArgProto((ChimeraStack));
void StackAction _ArgProto((ChimeraStack, ChimeraRequest *, char *));

#endif
