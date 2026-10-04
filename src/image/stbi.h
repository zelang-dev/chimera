
#ifndef __STB_H_INCLUDED
#define __STB_H_INCLUDED 1

#include "image_format.h"

/*
 * stbState
 */
typedef struct stb_State
{
	int state;				/* state of `stb/tiff/nanosvg` reader */
  FormatLineProc lineProc; 	/* line callback */
  void *closure;			/* closure for callback */

  Image *image;

  int   ypos;              /* current line */
  int   xpos;              /* current pixel */
} stbState;

/*
 * state of `stb/tiff/nanosvg` reader
 */
#define STB_FINISHED 0
#define STB_READ_IMAGE 1
#define STB_FAILED 2

/*
 * return values from `stb/tiff/nanosvg` processing fns
 */
#define STB_ERROR 0
#define STB_SUCCESS 1
#define STB_NEED_DATA 2

#endif
