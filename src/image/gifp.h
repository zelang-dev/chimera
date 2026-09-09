/*
 * gifp.h
 *
 * Created from parts of old gifin.h
 * John Kilburg (john@cs.unlv.edu)
 *
 * gifin.h
 * kirk johnson
 * november 1989
 * external interface to gifin.c
 *
 * Copyright 1989 Kirk L. Johnson
 * See the file COPYRIGHT for details
 */

/*
 * the old gif.h file
 *
 * Copyright (C) 1995, John Kilburg (john@isri.unlv.edu)
 */

#define STAB_SIZE  4096         /* string table size */
#define PSTK_SIZE  4096         /* pixel stack size */


/*
 * gifState
 */
typedef struct _gif_state
{
  int state;               /* state of GIF reader */
  FormatLineProc lineProc; /* line callback */
  void *closure;           /* closure for callback */

  Image *image;

  int  root_size;          /* root code size */
  int  clr_code;           /* clear code */
  int  eoi_code;           /* end of information code */
  int  code_size;          /* current code size */
  int  code_mask;          /* current code mask */
  int  prev_code;          /* previous code */
  int  first;              /* */

  unsigned work_data;      /* working bit buffer */
  int  work_bits;          /* working bit count */

  byte *buf;               /* byte buffer */
  int  buf_cnt;            /* byte count */
  int  buf_idx;            /* buffer index */

  int table_size;          /* string table size */
  int prefix[STAB_SIZE];   /* string table : prefixes */
  int extnsn[STAB_SIZE];   /* string table : extensions */

  byte pstk[PSTK_SIZE];    /* pixel stack */
  int  pstk_idx;           /* pixel stack pointer */

  int  pos;                 /* current read position */
  byte *data;               /* pointer to data */
  int  datalen;             /* length of data */
  int  rast_width;          /* raster width */
  int  rast_height;         /* raster height */
  byte g_cmap_flag;         /* global colormap flag */
  int  g_pixel_bits;        /* bits per pixel, global colormap */
  int  g_ncolors;           /* number of colors, global colormap */
  int  g_ncolors_pos;       /* number of colors processed, global colormap */
  Intensity g_cmap[3][256]; /* global colormap */
  int  bg_color;            /* background color index */
  int  color_bits;          /* bits of color resolution */
  int  transparent;         /* transparent color index */

  int  img_left;            /* image position on raster */
  int  img_top;             /* image position on raster */
  int  img_width;           /* image width */
  int  img_height;          /* image height */
  byte have_dimensions;     /* 1 when dimensions known */
  byte l_cmap_flag;         /* local colormap flag */
  int  l_pixel_bits;        /* bits per pixel, local colormap */
  int  l_ncolors;           /* number of colors, local colormap */
  int  l_ncolors_pos;       /* number of colors processed, local colormap */
  Intensity l_cmap[3][256]; /* local colormap */
  byte interlace_flag;      /* interlace image format flag */
} gifState;

/*
 * end of old gif.h file
 */

/*
 * gifin return codes
 */
#define GS_SUCCESS       0      /* success */
#define GS_DONE          1      /* no more images */
#define GS_NEED_DATA     2      /* needs more data */
#define GS_NEED_DATA_NOR 3      /* needs more data, no rewind */

#define GS_ERR_BAD_SD   -1      /* bad screen descriptor */
#define GS_ERR_BAD_SEP  -2      /* bad image separator */
#define GS_ERR_BAD_SIG  -3      /* bad signature */
#define GS_ERR_EOD      -4      /* unexpected end of raster data */
#define GS_ERR_EOF      -5      /* unexpected end of input stream */
#define GS_ERR_FAO      -6      /* file already open */
#define GS_ERR_IAO      -7      /* image already open */
#define GS_ERR_NFO      -8      /* no file open */
#define GS_ERR_NIO      -9      /* no image open */
#define GS_ERR_BAD_STATE -10    /* bad state */
#define GS_ERR_NOMEM   -11     /* allocation failure */

/*
 * colormap indices 
 */
#define GIF_RED  0
#define GIF_GRN  1
#define GIF_BLU  2

/*
 * #defines, typedefs, and such
 */
#define GIF_SIG      "GIF87a"
#define GIF_SIG_89   "GIF89a"
#define GIF_SIG_LEN  6          /* GIF signature length */
#define GIF_SD_SIZE  7          /* GIF screen descriptor size */
#define GIF_ID_SIZE  9          /* GIF image descriptor size */

#define GIF_SEPARATOR   ','     /* GIF image separator */
#define GIF_EXTENSION   '!'     /* GIF extension block marker */
#define GIF_TERMINATOR  ';'     /* GIF terminator */

#define NULL_CODE  -1           /* string table null code */

/*
 * GIF states
 */
#define GS_OPEN_FILE 0
#define GS_OPEN_IMAGE 1
#define GS_LOAD_G_CMAP 2
#define GS_LOAD_L_CMAP 4
#define GS_DECODE_DATA 5
#define GS_MAKE_IMAGE 6
#define GS_INIT_DECODER 8
#define GS_READ_SIG 9
#define GS_READ_IMAGE_HEADER 10


