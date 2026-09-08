
#ifndef __STB_H_INCLUDED
#define __STB_H_INCLUDED 1

#include "image_format.h"

// Structure to hold streamed data for stb_image
typedef struct {
	unsigned char *data;
	size_t size;
	size_t offset;
} MemoryStream;

/*
 * stbState
 */
typedef struct stb_State
{
  int state;               /* state of STB reader */
  FormatLineProc lineProc; /* line callback */
  void *closure;           /* closure for callback */

  Image *image;

  int   pos;               /* current read position */
  int   ypos;              /* current line */
  int   xpos;              /* current pixel */
  byte *imagepos;          /* current posn. in output */
} stbState;

/*
 * state of stb reader
 */
#define STB_FINISHED 0
#define STB_READ_IMAGE 1
#define STB_FAILED 2

/*
 * return values from stb processing fns
 */
#define STB_ERROR 0
#define STB_SUCCESS 1
#define STB_NEED_DATA 2

#endif
