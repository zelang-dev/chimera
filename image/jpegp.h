/*
 * jpegp.h
 *
 * Copyright (C) 1995, Erik Corry (ehcorry@inet.uni-c.dk)
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

/*
 * This stuff is very fragile wrt. changes in the sizes of the
 * jpeg library structures.  It's a real good idea to be using libjpeg
 * version 6a or later; 6a added some defenses against structure mismatches.
 */

#ifndef JPEG_H_INCLUDED
#define JPEG_H_INCLUDED 1

#include <stdio.h>
#include <sys/types.h>		/* in case size_t is not defined by stdio.h */
#include <setjmp.h>

#include "image_format.h"

/*
 * Unfortunately libjpeg uses some of the same configuration symbols
 * as Chimera.  Undef'ing these symbols here is not essential, but it
 * prevents macro-redefinition warnings from gcc and like-minded compilers.
 */
#undef HAVE_PROTOTYPES
#undef HAVE_UNSIGNED_CHAR
#undef HAVE_UNSIGNED_SHORT
#undef HAVE_STDDEF_H
#undef HAVE_STDLIB_H

#include "jpeglib.h"
#include "jerror.h"

/*
 * Each of these structures begins with a jpeg library struct,
 * followed by chimera extension fields.  This lets us cast pointers
 * back and forth between the structs known to the library and the
 * full struct definitions.  Poor man's subclassing, if you like.
 */

struct jpeg_chimera_error_state
{
  struct jpeg_error_mgr pub; /* - this struct comes from jpeg lib */
  jmp_buf jump_buffer;       /* usually an array of sorts from setjmp.h */
};

struct jpeg_chimera_input_state
{
  struct jpeg_source_mgr pub;   /* - this struct comes from jpeg lib */
  int	bytes_consumed;		/* # bytes already consumed by jpeg lib */
  bool	at_eof;			/* indicates we've gotten the whole file */
  bool	faked_eoi;		/* indicates next_input_byte is phony */
  JOCTET fake_eoi[2];		/* workspace for making a fake EOI */
};

/*
 * jpegState
 */
typedef struct jpeg_state
{
  struct jpeg_decompress_struct cinfo;	/* - this struct comes from jpeg lib */

  struct jpeg_chimera_error_state error_state; /* subsidiary structures */
  struct jpeg_chimera_input_state input_state;

  bool	destroy_jpeg;		/* indicates we need to call jpeg_destroy */
  int	state;			/* state of JPEG reader */

  JDIMENSION   ypos;		/* current scanline number */

  FormatLineProc lineProc;	/* line callback */
  void	*lineClosure;		/* closure for callback */

  Image *image;

  Intensity cmap[256];		/* room for making a grayscale palette */
} jpegState;

/*
 * state of JPEG reader
 */
#define CH_JPEG_READ_HEADER 1
#define CH_JPEG_START_DECOMPRESS 2
#define CH_JPEG_START_OUTPUT 3
#define CH_JPEG_READ_IMAGE 4
#define CH_JPEG_FINISH_OUTPUT 5
#define CH_JPEG_FINISH_DECOMPRESS 6
#define CH_JPEG_FINISHED 0

/*
 * return values from jpeg processing fns
 * NOTE: CH_JPEG_FINISHED is also used as a return value!
 */
#define CH_JPEG_SUCCESS 1
#define CH_JPEG_NEED_DATA 2
#define CH_JPEG_ERROR 3

#endif
