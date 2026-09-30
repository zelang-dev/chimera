
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <tiffio.h>	// TIFF reading

#include "common.h"
#include "imagep.h"
#include "image_format.h"

#include "image_endian.h"
#include "stbi.h"

#define NANOSVG_ALL_COLOR_KEYWORDS	// Include full list of color keywords.
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"	// SVG parsing

#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"	// SVG rasterization

#define STB_IMAGE_IMPLEMENTATION
#define STBI_SUPPORT_ZLIB
#include "stb_image.h"	// PNG/JPG/GIF/TGA/BMP/PIC/PNM/PSD/HDR reading

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

/* Custom read function for TIFFClientOpen */
tsize_t mem_read(thandle_t handle, tdata_t buf, tsize_t size) {
	stbState *mem = (stbState *)handle;
	if (mem->image->offset + size > mem->image->size)
		size = mem->image->size - mem->image->offset;
	memcpy(buf, mem->image->data + mem->image->offset, size);
	mem->image->offset += size;
	return size;
}

/* Dummy write function (read-only) */
tsize_t mem_write(thandle_t handle, tdata_t buf, tsize_t size) {
	return 0; // not supported
}

/* Seek function */
toff_t mem_seek(thandle_t handle, toff_t offset, int whence) {
	stbState *mem = (stbState *)handle;
	size_t new_offset;
	switch (whence) {
		case SEEK_SET: new_offset = offset; break;
		case SEEK_CUR: new_offset = mem->image->offset + offset; break;
		case SEEK_END: new_offset = mem->image->size + offset; break;
		default: return (toff_t)-1;
	}
	if (new_offset > mem->image->size) return (toff_t)-1;
	mem->image->offset = new_offset;
	return mem->image->offset;
}

/* Close function */
int mem_close(thandle_t handle) {
	return 0; // nothing to free here
}

/* Size function */
toff_t mem_size(thandle_t handle) {
	stbState *mem = (stbState *)handle;
	return mem->image->size;
}

/* Map/unmap functions (optional, not used here) */
int mem_map(thandle_t handle, tdata_t *data, toff_t *size) { return 0; }
void mem_unmap(thandle_t handle, tdata_t data, toff_t size) {}

static int lf_read_image(stbState *stb, byte *data, int len) {
	int x, y, comp, req_comp = 4;
	NSVGimage *shapes = NULL;
	NSVGrasterizer *rast = NULL;
	stbi_uc *image_data = NULL;
	TIFF *tif = NULL;
	if (!data)
		return STB_NEED_DATA;

	if (stbi_info_from_memory((const stbi_uc *)data, len, &x, &y, &comp)
		&& (image_data = stbi_load_from_memory(data, len, &x, &y, &comp, req_comp))) {
		stb->image = newImage(image_data, x, y);
		if (!stb->image) {
			fprintf(stderr, "Memory allocation failed to load image: %s\n", stbi_failure_reason());
			free(image_data);
			stb->state = STB_FAILED;
			return STB_ERROR;
		}

		if (stb->lineProc != NULL)
			(stb->lineProc)(stb->closure, stb->xpos, stb->ypos);

		stb->state = STB_FINISHED;
		return STB_SUCCESS;
	} else if ((shapes = nsvgParse(data, "px", 96.0f))) {
		x = (int)shapes->width;
		y = (int)shapes->height;
		rast = nsvgCreateRasterizer();
		if (!rast) {
			fprintf(stderr, "Raster allocation failed\n");
			nsvgDelete(shapes);
			stb->state = STB_FAILED;
			return STB_ERROR;
		}

		stb->image = newImage((malloc(x * y * 4)), x, y);
		if (!stb->image || !stb->image->data) {
			fprintf(stderr, "Memory allocation failed\n");
			nsvgDeleteRasterizer(rast);
			nsvgDelete(shapes);
			stb->state = STB_FAILED;
			return STB_ERROR;
		}

		nsvgRasterize(rast, shapes, 0, 0, 1.0f, stb->image->data, x, y, x * 4);
		nsvgDeleteRasterizer(rast);
		nsvgDelete(shapes);
		if (stb->lineProc != NULL)
			(stb->lineProc)(stb->closure, stb->xpos, stb->ypos);

		stb->state = STB_FINISHED;
		return STB_SUCCESS;
	} else if ((tif = TIFFClientOpen("MemTIFF", "r", (thandle_t)&data, mem_read,
		mem_write, mem_seek, mem_close,	mem_size, mem_map, mem_unmap))) {
		uint32_t width, height;
		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

		stb->image = newImage(_TIFFmalloc(width * height * sizeof(uint32_t)), width, height);
		if (!stb->image || !stb->image->data) {
			fprintf(stderr, "Raster/memory allocation failed\n");
			TIFFClose(tif);
			stb->state = STB_FAILED;
			return STB_ERROR;
		}

		/* Read RGBA image */
		if (!TIFFReadRGBAImageOriented(tif, width, height,
			(uint32_t *)stb->image->data, ORIENTATION_TOPLEFT, 0)) {
			fprintf(stderr, "TIFFReadRGBAImage failed\n");
			_TIFFfree(stb->image->data);
			stb->image->data = NULL;
			TIFFClose(tif);
			stb->state = STB_FAILED;
			return STB_ERROR;
		}

		if (stb->lineProc != NULL)
			(stb->lineProc)(stb->closure, stb->xpos, stb->ypos);

		stb->state = STB_FINISHED;
		return STB_SUCCESS;
	} else {
		fprintf(stderr, "Failed to load image: %s\n", stbi_failure_reason());
		stb->state = STB_FAILED;
	}

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
