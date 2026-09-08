/*
 * ChimeraAuth.h
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
#ifndef __CHIMERAAUTH_H__
#define __CHIMERAAUTH_H__ 1

typedef struct ChimeraAuthP *ChimeraAuth;

typedef void (*ChimeraAuthCallback) _ArgProto((void *, char *, char *));

ChimeraAuth AuthCreate _ArgProto((ChimeraResources, char *, char *,
				  ChimeraAuthCallback, void *));
void AuthDestroy _ArgProto((ChimeraAuth));
					       
#endif
