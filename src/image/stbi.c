
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <tiffio.h>	// TIFF reading

#include "common.h"
#include "imagep.h"
#include "image_format.h"

#include "image_endian.h"
#include "stbi.h"
#include <athena.h>

/*
 * stbGetImage
 */
static Image *stbGetImage(void *pointer) {
	stbState *stb = (stbState *)pointer;
	return stb->image;
}

/*
 * stbDestroy
 */
static void stbDestroy(void *pointer) {
	stbState *stb = (stbState *)pointer;
	if (stb != NULL) {
		if (stb->image->data)
			free(stb->image->data);

		stb->image->data = NULL;
		if (stb->image)
			free(stb->image);

		stb->image = NULL;
		free(stb);
		stb = NULL;
	}
}

static Image *newImage(void *imagedata, unsigned int width, unsigned int height) {
	Image *image = (Image *)calloc(1, sizeof(Image));
	if (!image)
		return((Image *)0);

	image->type = ITRUE;
	image->width = width;
	image->height = height;
	image->transparent = -1;
	image->data = (byte *)imagedata;
	return(image);
}

static int lf_read_image(stbState *stb, byte *data, int len) {
	int x, y;
	unsigned char *image_data = NULL;

	if (!data)
		return STB_NEED_DATA;

	if ((image_data = memory_stbi(data, len, &x, &y))
		|| (image_data = memory_nsvg(data, len, &x, &y))
		|| (image_data = memory_tiff(data, len, &x, &y))) {
		stb->image = newImage(image_data, x, y);
		if (!stb->image) {
			fprintf(stderr, "Memory allocation failed\n");
			free(image_data);
			stb->state = STB_FAILED;
			return STB_ERROR;
		}

		if (stb->lineProc != NULL)
			(stb->lineProc)(stb->closure, stb->xpos, stb->ypos);

		stb->state = STB_FINISHED;
		return STB_SUCCESS;
	}

	fprintf(stderr, "Failed to load image: %s\n", stbi_failure_reason());
	stb->state = STB_FAILED;
	return STB_ERROR;
}

/*
 * stbAddData
 *
 * 0 success
 * 1 needs more data
 * -1 error
 *
 * Assumes data is the address of the beginning of the stb data and len
 * is the total length.
 */
static int stbAddData(void *pointer, byte *data, int len, bool data_ended) {
	stbState *stb = (stbState *)pointer;
	int rval;
	for (;;) {
		switch (stb->state) {
			case STB_READ_IMAGE:
				rval = lf_read_image(stb, data, len);
				break;
			case STB_FINISHED:
				return 0;
			case STB_FAILED:
				return -1;
		}

		if (rval == STB_NEED_DATA) {
			if (data_ended)
				return(-1);

			return(1);
		}
	}

	return (rval == STB_SUCCESS ? 0 : -1);
}

/*
 * stbInit
 *
 * Initialize STB reader state
 */
void stbInit(void (*lineProc)(), void *closure, struct ifs_vector *if_vector) {
	stbState *stb = (stbState *)malloc(sizeof(stbState));
	if (!stb) {
		fprintf(stderr, "`stbInit` allocation failed\n");
		return;
	}

	memset(stb, 0, sizeof(stbState));
	stb->state = STB_READ_IMAGE;
	stb->lineProc = lineProc;
	stb->closure = closure;

	if_vector->initProc = &stbInit;
	if_vector->destroyProc = &stbDestroy;
	if_vector->addDataProc = &stbAddData;
	if_vector->getImageProc = &stbGetImage;

	if_vector->image_format_closure = (void *)stb;
}
