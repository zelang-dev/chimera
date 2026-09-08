/*
 * ChimeraGUI.h
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
#ifndef __CHIMERAGUI_H__
#define __CHIMERAGUI_H__ 1

typedef struct ChimeraGUIP *ChimeraGUI;
typedef struct ChimeraGUIScrollPosP *ChimeraGUIScrollPos;

typedef void (*GUISizeCallback) _ArgProto((ChimeraGUI, void *,
					   unsigned int, unsigned int));

ChimeraGUI GUICreate _ArgProto((ChimeraContext, ChimeraGUI,
				GUISizeCallback, void *));
void GUIDestroy _ArgProto((ChimeraGUI));

void GUIMap _ArgProto((ChimeraGUI, int, int));
void GUIUnmap _ArgProto((ChimeraGUI));

void GUISetInitialDimensions _ArgProto((ChimeraGUI,
					unsigned int, unsigned int));

void GUIGetOnScreenDimensions _ArgProto((ChimeraGUI,
					 int *, int *,
					 unsigned int *, unsigned int *));

int GUIGetDimensions _ArgProto((ChimeraGUI,
				unsigned int *, unsigned int *));

void GUISetDimensions _ArgProto((ChimeraGUI,
				 unsigned int, unsigned int));

Display *GUIToDisplay _ArgProto((ChimeraGUI));
Window GUIToWindow _ArgProto((ChimeraGUI));
Pixel GUIBackgroundPixel _ArgProto((ChimeraGUI));
Widget GUIToWidget _ArgProto((ChimeraGUI));

void GUISetScrollBar _ArgProto((ChimeraGUI, bool));
void GUIGetScrollPosition _ArgProto((ChimeraGUI, int *, int *));
void GUISetScrollPosition _ArgProto((ChimeraGUI, int, int));

int GUIGetNamedColor _ArgProto((ChimeraGUI, char *, Pixel *));

#endif
