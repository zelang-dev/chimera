/*
 * jpeg.c:
 *
 * (c) 1995 Erik Corry ehcorry@inet.uni-c.dk
 *
 * modelled on xbm.c.
 *
 * If you want to understand this read the following files from libjpeg:
 *
 *   libjpeg.doc
 *   jdatasrc.c
 *   jpeglib.h
 *   example.c
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
#include "port_before.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port_after.h"

#include "common.h"
#include "imagep.h"

#include "jpegp.h"

static void jpegDestroy _ArgProto((void *));

/*
 * jpegGetImage - called through vector
 */

static Image *
jpegGetImage(void *pointer)
{
  jpegState *jpeg = (jpegState *)pointer;
  return jpeg->image;
}

/*
 * jpegDestroy - called through vector
 */
static void
jpegDestroy(pointer)
void *pointer;
{
  jpegState *jpeg = (jpegState *)pointer;
  if (jpeg) {
    if(setjmp(jpeg->error_state.jump_buffer))
      {
	/*
	 * Oh shit, an error while clearing up...
	 */
	fprintf(stderr, "jpegDestroy: failed\n");
	return;
      }
    if (jpeg->image)
      freeImage(jpeg->image);
    jpeg->image = 0;
    if (jpeg->destroy_jpeg)
      jpeg_destroy_decompress(&jpeg->cinfo);
    jpeg->destroy_jpeg = false;
    free_mem(jpeg);
  }
}

/*
 * these routines form a suspending "data source manager" for libjpeg.
 */

static void
lf_init_source_callback(
  struct jpeg_decompress_struct *cinfo)
{
}

static boolean
lf_fill_input_buffer_callback(
  struct jpeg_decompress_struct *cinfo)
{
  struct jpeg_chimera_input_state *input_state =
    (struct jpeg_chimera_input_state *)(cinfo->src);

  if (! input_state->at_eof)
    return FALSE;		/* force suspension until more data arrives */

  /* If libjpeg demands more data beyond the EOF,
   * pacify it by supplying EOI marker(s).
   * This lets us produce some kind of image from a trucated or corrupted file.
   */

  WARNMS(cinfo, JWRN_JPEG_EOF);

  input_state->fake_eoi[0] = (JOCTET) 0xFF;
  input_state->fake_eoi[1] = (JOCTET) JPEG_EOI;
  input_state->pub.next_input_byte = input_state->fake_eoi;
  input_state->pub.bytes_in_buffer = 2;
  input_state->faked_eoi = true;

  return TRUE;
}

static void
lf_skip_input_data_callback(
  struct jpeg_decompress_struct *cinfo,
  long num_bytes)
{
  struct jpeg_chimera_input_state *input_state =
    (struct jpeg_chimera_input_state *)(cinfo->src);

  if (num_bytes <= 0) return;

  if (num_bytes > (long) input_state->pub.bytes_in_buffer)
  {
    /* Tricky code here: we advance next_input_byte beyond the end of the
     * buffer.  libjpeg will suspend because we set bytes_in_buffer to 0,
     * and then jpegAddData's bookkeeping will do the right thing.
     * This could fail on sufficiently weird architectures, because
     * pointers aren't guaranteed by the C standard to be able to point
     * past the end of allocated memory; but in practice it should be fine.
     */
    input_state->pub.next_input_byte += num_bytes;
    input_state->pub.bytes_in_buffer = 0;
  }
  else
  {
    input_state->pub.next_input_byte += num_bytes;
    input_state->pub.bytes_in_buffer -= num_bytes;
  }
}

static void
lf_term_source_callback(
  struct jpeg_decompress_struct *cinfo)
{
}

/*
 * this routine overrides libjpeg's default fatal-error handler.
 */

static void
lf_error_exit_callback(
  j_common_ptr cinfo)
{
  struct jpeg_chimera_error_state *error_state =
    (struct jpeg_chimera_error_state *)(cinfo->err);
  /*
   * Display message. At the moment I use the builtin stderr handler,
   * but we could install something smarter, or just keep quiet about it.
   * Note that if we did want to keep quiet, we'd have to override
   * output_message as well as error_exit, so as to suppress warning msgs.
   */
  (error_state->pub.output_message)(cinfo);

  /* Return control to the setjmp point */
  longjmp(error_state->jump_buffer, 1);
}

/*
 * These routines are the elements of a state machine for reading JPEGs.
 * We use a distinct state for each point at which we may have to suspend
 * processing.
 */

static int
lf_read_header(jpegState *jpeg)
{
  int status = jpeg_read_header(&jpeg->cinfo, TRUE);

  if (status == JPEG_SUSPENDED) return CH_JPEG_NEED_DATA;

  /*
   * Done reading header; set decompression parameters.
   * Probably ought to have some user-settable preferences here.
   */

  /* These are needed for progressive rendering */
  if (jpeg_has_multiple_scans(&jpeg->cinfo))
    jpeg->cinfo.buffered_image = TRUE;
  jpeg->cinfo.do_block_smoothing = TRUE;
  /* These are optional tradeoffs of quality for speed */
  /* if (force-grayscale) jpeg->cinfo.out_color_space = JCS_GRAYSCALE; */
  jpeg->cinfo.dct_method = JDCT_FASTEST;
  jpeg->cinfo.do_fancy_upsampling = FALSE;

  /* Advance to next state */
  jpeg->state = CH_JPEG_START_DECOMPRESS;
  return CH_JPEG_SUCCESS;
}

static int
lf_start_decompress(jpegState *jpeg)
{
  int status = jpeg_start_decompress(&jpeg->cinfo);

  if (status == FALSE) return CH_JPEG_NEED_DATA;

  /*
   * start_decompress done; get image parameters
   */

  if (jpeg->cinfo.out_color_space == JCS_GRAYSCALE &&
      jpeg->cinfo.output_components == 1)
  {
    /* we represent grayscale as RGBI with a gray-ramp palette */
    int i;
    jpeg->image = newRGBImage(jpeg->cinfo.output_width,
			      jpeg->cinfo.output_height, 8);
    if (!jpeg->image)
      return CH_JPEG_ERROR;

    jpeg->image->type = IGRAY;
    jpeg->image->rgb.red = jpeg->cmap;
    jpeg->image->rgb.green = jpeg->cmap;
    jpeg->image->rgb.blue = jpeg->cmap;
    jpeg->image->rgb.used = 256;
    for (i = 0; i < 256; i++)
      jpeg->cmap[i] = i | (i << 8);
  }
  else if (jpeg->cinfo.out_color_space == JCS_RGB &&
	   jpeg->cinfo.output_components == 3)
  {
    jpeg->image = newTrueImage(jpeg->cinfo.output_width,
			       jpeg->cinfo.output_height);
    if (!jpeg->image)
      return CH_JPEG_ERROR;
  }
  else
  {
    fprintf(stderr, "Unsupported JPEG colorspace\n");
    return CH_JPEG_ERROR;
  }

  /* Advance to next state */
  jpeg->state = CH_JPEG_START_OUTPUT;
  return CH_JPEG_SUCCESS;
}

static int
lf_start_output(jpegState *jpeg)
{
  if (jpeg->cinfo.buffered_image)
  {
    /* Eat all the available data before issuing start_output.
     * This ensures we have an up-to-date target scan number
     * and prevents executing unnecessary output passes.
     * Once we get a SUSPENDED return (or possibly an EOI),
     * we can proceed with display.
     */
    int status;
    for (;;) {
      status = jpeg_consume_input(&jpeg->cinfo);
      if (status == JPEG_SUSPENDED || status == JPEG_REACHED_EOI)
	break;
    }
    status = jpeg_start_output(&jpeg->cinfo, jpeg->cinfo.input_scan_number);
    if (status == FALSE) return CH_JPEG_NEED_DATA;
    /* printf("scan %d\n", jpeg->cinfo.input_scan_number); */
  }

  jpeg->ypos = 0;

  /* Advance to next state */
  jpeg->state = CH_JPEG_READ_IMAGE;
  return CH_JPEG_SUCCESS;
}

static int
lf_read_image(jpegState *jpeg)
{
  int scanline_count, max_scanlines;
  int i;
  unsigned char *scanline_pointers[4];
  unsigned char *output_addr;

  if (jpeg->cinfo.buffered_image)
  {
    /* In progressive mode, consume all available input right away.
     * NOTE: maybe ought to be less greedy if whole file is available???
     */
    for (;;) {
      int status = jpeg_consume_input(&jpeg->cinfo);
      if (status == JPEG_SUSPENDED || status == JPEG_REACHED_EOI)
	break;
    }
  }

  while (jpeg->ypos < jpeg->cinfo.output_height)
  {
    /* Somewhat optimistically, we assume that libjpeg may be able to
     * give us as many as four scanlines per call.
     */
    max_scanlines = MIN(4, jpeg->cinfo.output_height - jpeg->ypos);
    output_addr =
      jpeg->image->data + jpeg->image->bytes_per_line * jpeg->ypos;
    for (i = 0; i < max_scanlines; i++)
    {
      scanline_pointers[i] = output_addr;
      output_addr += jpeg->image->bytes_per_line;
    }
  
    scanline_count =
      jpeg_read_scanlines(&jpeg->cinfo, scanline_pointers, max_scanlines);

    if (scanline_count == 0) return CH_JPEG_NEED_DATA;
  
    if (jpeg->lineProc != NULL)
      (jpeg->lineProc)(jpeg->lineClosure, jpeg->ypos,
                       jpeg->ypos + scanline_count - 1);
  
    jpeg->ypos += scanline_count;
  }

  /* Advance to next state */
  jpeg->state = CH_JPEG_FINISH_OUTPUT;
  return CH_JPEG_SUCCESS;
}

static int
lf_finish_output(jpegState *jpeg)
{
  if (jpeg->cinfo.buffered_image)
  {
    int status = jpeg_finish_output(&jpeg->cinfo);
    if (status == FALSE) return CH_JPEG_NEED_DATA;

    /* We need another output scan unless the input file is all read
     * and the just-completed output scan was started after the final
     * input scan began arriving.
     */
    if (! jpeg_input_complete(&jpeg->cinfo) ||
	jpeg->cinfo.output_scan_number < jpeg->cinfo.input_scan_number)
    {
      jpeg->state = CH_JPEG_START_OUTPUT;
      return CH_JPEG_SUCCESS;
    }
  }

  /* Advance to next state */
  jpeg->state = CH_JPEG_FINISH_DECOMPRESS;
  return CH_JPEG_SUCCESS;
}

static int
lf_finish_decompress(jpegState *jpeg)
{
  int status = jpeg_finish_decompress(&jpeg->cinfo);

  if (status == FALSE) return CH_JPEG_NEED_DATA;

  /* Advance to next state */
  jpeg->state = CH_JPEG_FINISHED;
  return CH_JPEG_SUCCESS;
}

/*
 * jpegAddData - called through vector
 *
 * Returns:
 * 0 success
 * 1 need more data
 * -1 error
 *
 * Assumes data is the address of the beginning of the jpeg data and len
 * is the total length.
 */
static int
jpegAddData(pointer, data, len, data_ended)
void *pointer;
byte *data;
int len;
bool data_ended;
{
  jpegState *jpeg = (jpegState *)pointer;
  int rval;

/*
  fprintf(stderr, "jpegAddData %p: buf %p %d %d\n",
	  &jpeg->cinfo, data, len, data_ended);
*/

  if (setjmp(jpeg->error_state.jump_buffer))
  {
    /* Failed.  Assume we will get a jpegDestroy call */
    return -1;
  }

  /*
   * Can't progress unless more data is available (or EOI is reached).
   * Note that len < bytes_consumed is entirely valid, if libjpeg has
   * commanded a skip beyond the data so far received.
   */
  if (len <= jpeg->input_state.bytes_consumed)
  {
    if (! data_ended) return 1;	/* suspend */
    /* If reached EOF, terminate any incomplete skip. */
    jpeg->input_state.bytes_consumed = len;
  }

  /* Update libjpeg's input pointers */
  jpeg->input_state.pub.next_input_byte =
    ((JOCTET*) data) + jpeg->input_state.bytes_consumed;
  jpeg->input_state.pub.bytes_in_buffer = 
    len - jpeg->input_state.bytes_consumed;
  jpeg->input_state.faked_eoi = false; /* next_input_byte is valid */
  if (data_ended)
    jpeg->input_state.at_eof = true;

  /*
   * Don't bother to parse less than a few hundred bytes at a time;
   * this prevents excess cycling in libjpeg.  (Normal TCP connections
   * will deliver a packet at a time, so this test will probably never
   * trigger...)
   */
  if (!data_ended &&
      jpeg->input_state.pub.bytes_in_buffer < 200)
    return 1;

  /*
   * Cycle the state machine until done or forced to suspend by lack of input.
   * Note that we are able to progress through multiple states per call
   * as long as the input is there.
   */

  for ( ; ; )
  {
    switch (jpeg->state)
    {
      case CH_JPEG_READ_HEADER:
        rval = lf_read_header(jpeg);
        break;
      case CH_JPEG_START_DECOMPRESS:
        rval = lf_start_decompress(jpeg);
	break;
      case CH_JPEG_START_OUTPUT:
        rval = lf_start_output(jpeg);
	break;
      case CH_JPEG_READ_IMAGE:
        rval = lf_read_image(jpeg);
	break;
      case CH_JPEG_FINISH_OUTPUT:
        rval = lf_finish_output(jpeg);
	break;
      case CH_JPEG_FINISH_DECOMPRESS:
        rval = lf_finish_decompress(jpeg);
	break;
      case CH_JPEG_FINISHED:
	rval = CH_JPEG_FINISHED;
	break;
      default:
	fprintf(stderr, "jpegAddData: bogus state %d\n", jpeg->state);
	rval = CH_JPEG_ERROR;
	break;
    }
    /* exit loop for FINISHED, NEED_DATA, or ERROR returns */
    if (rval != CH_JPEG_SUCCESS)
      break;
  }

  /*
   * Record how much data libjpeg consumed.
   */
  if (jpeg->input_state.faked_eoi)
  {
    /* next_input_bytes is invalid, just say we ate it all. */
    jpeg->input_state.bytes_consumed = len;
  }
  else
  {
    jpeg->input_state.bytes_consumed =
      jpeg->input_state.pub.next_input_byte - ((JOCTET*) data);
  }

  if (rval == CH_JPEG_NEED_DATA)
    return 1;
  if (rval == CH_JPEG_FINISHED)
    return 0;
  return(-1);
}

/*
 * jpegInit
 *
 * Initialize JPEG reader state
 */
void
jpegInit(lineProc, lineClosure, if_vector)
void (*lineProc)();
void *lineClosure;
struct ifs_vector *if_vector;
{
  jpegState *jpeg;

  /*
   * Initialise vector for methods Chimera needs
   */

  if_vector->initProc = &jpegInit;
  if_vector->destroyProc = &jpegDestroy;
  if_vector->addDataProc = &jpegAddData;
  if_vector->getImageProc = &jpegGetImage;

  /* allocate my workspace */
  jpeg = (jpegState *)alloc_mem(sizeof(jpegState));
  if_vector->image_format_closure = (void *)jpeg;
  if (jpeg == NULL)
    return;

  memset(jpeg, 0, sizeof(jpegState));
  jpeg->state = CH_JPEG_READ_HEADER;
  jpeg->lineProc = lineProc;
  jpeg->lineClosure = lineClosure;

  /*
   * Initialise error handler.
   * We set up the normal JPEG error routines, then override error_exit.
   */
  jpeg->cinfo.err = jpeg_std_error(&jpeg->error_state.pub);
  jpeg->error_state.pub.error_exit = &lf_error_exit_callback;
  if (setjmp(jpeg->error_state.jump_buffer))
  {
    /*
     * We get here after a failure in initialisation. Lets just assume that
     * people are careful about the order in which things are done, and that
     * a destroy will work here.
     */
    jpegDestroy((void *)jpeg);
    /*
     * It would be nice if init could indicate failure...
     */
    return;
  }

  /*
   * Initialise jpeg library
   */
  jpeg->destroy_jpeg = true;	/* jpeg_destroy is OK even if create fails */
  jpeg_create_decompress(&jpeg->cinfo);

  /*
   * Initialise our input reader.
   */
  jpeg->cinfo.src = &jpeg->input_state.pub;
  jpeg->input_state.pub.init_source = &lf_init_source_callback;
  jpeg->input_state.pub.fill_input_buffer = &lf_fill_input_buffer_callback;
  jpeg->input_state.pub.skip_input_data = &lf_skip_input_data_callback;
  jpeg->input_state.pub.resync_to_restart = &jpeg_resync_to_restart;
  jpeg->input_state.pub.term_source = &lf_term_source_callback;
  jpeg->input_state.pub.bytes_in_buffer = 0;
  jpeg->input_state.pub.next_input_byte = NULL;

  jpeg->input_state.bytes_consumed = 0;
  jpeg->input_state.at_eof = false;
}
