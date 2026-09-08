/*
 * image_format.h
 *
 * Copyright (c) 1995 Erik Corry ehcorry@inet.uni-c.dk
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

#ifndef IMAGE_FORMAT_H_INCLUDED
#define IMAGE_FORMAT_H_INCLUDED

typedef struct ifs_vector *ift_vector;

typedef void (*FormatLineProc) _ArgProto((void *, int, int));

typedef void (InitProcDecl) _ArgProto((FormatLineProc line_proc,
                                        void *line_proc_closure,
                                        ift_vector vector));
typedef InitProcDecl *InitProc;
typedef void (*DestroyProc) _ArgProto((void *image_format_closure));
typedef int (*AddDataProc) _ArgProto((void *image_format_closure,
                                     byte *data,
                                     int len,
                                     bool data_ended));

typedef Image *(*GetImageProc) _ArgProto((void *image_format_closure));

InitProcDecl gifInit;
InitProcDecl xbmInit;
InitProcDecl pnmInit;
InitProcDecl jpegInit;
InitProcDecl pngInit;

struct ifs_vector
{
  void *image_format_closure;
  int image_format;
  InitProc initProc;
  DestroyProc destroyProc;
  AddDataProc addDataProc;
  GetImageProc getImageProc;
};

#endif
