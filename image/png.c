/*
 * Portable Network Graphics (PNG) image format
 */

#include "port_before.h"

#include <png.h>

#include "port_after.h"

#include "common.h"
#include "imagep.h"
#include "image_format.h"
#include "image_endian.h"
#include <assert.h>

#define PM_SCALE(a, b, c) (long)((a) * (c))/(b)

enum {
    image_success = 0, image_need_data = 1, image_error = -1
};

typedef struct {
    png_struct *state;
    png_info *info;
    Image *image;
    Intensity cmap[3][256];
    void (*lineProc)(void *, int, int);
    void *closure;
    int lenSoFar;
    int done;
} pngState;


static Image *
pngGetImage(void *pointer)
{
    return ((pngState *) pointer)->image;
}


static void lc_reverse_byte(byte *row, int n)
{
    int i;
    byte c;

    for (i = 0; i < n; ++i) {
        c = row[i];
        c = ((c << 4) & 0xf0) | ((c >> 4) & 0x0f);
        c = ((c << 2) & 0xcc) | ((c >> 2) & 0x33);
        c = ((c << 1) & 0xaa) | ((c >> 1) & 0x55);
        row[i] = c;
    }
}


static void
lf_info_callback(png_struct *state, png_info *info)
{
    int orig_depth = 0;
    pngState *png = (pngState *) png_get_progressive_ptr(state);

    if (info->bit_depth < 8 && (PNG_COLOR_TYPE_RGB == info->color_type ||
            PNG_COLOR_TYPE_RGB_ALPHA == info->color_type))
        png_set_expand(state);

    /* I wish the frame's background colour was available here */
    if (info->color_type & PNG_COLOR_MASK_ALPHA) {
        png_color_16 bg;
        int gflag = PNG_BACKGROUND_GAMMA_SCREEN;
        double gval = 1.0;
        int expand = 0;

        bg.red = bg.green = bg.blue = bg.gray = 0;
        if (PNG_COLOR_TYPE_PALETTE == info->color_type)
            png_set_expand(state);

        png_set_background(state, &bg, gflag, expand, gval);
    }

    if (info->bit_depth < 8 && (info->bit_depth > 1 ||
            PNG_COLOR_TYPE_GRAY != info->color_type)) {
        if (PNG_COLOR_TYPE_GRAY == info->color_type)
            orig_depth = info->bit_depth;
        png_set_packing(state);
    }
 
    /* tell libpng to strip 16 bit depth files down to 8 bits */
    if (info->bit_depth > 8)
        png_set_strip_16(state);

    png_set_interlace_handling(state);

    /* update palette with transformations, update the info structure */
    png_read_update_info(state, info);

    /* allocate the memory to hold the image using the fields of png_info. */
    if (PNG_COLOR_TYPE_GRAY == info->color_type && 1 == info->bit_depth) {
        png->image = newBitImage(info->width, info->height);
       if (!png->image) {
           png->done = image_error;
           return;
       }

        png_set_invert_mono(state);
    } else if (PNG_COLOR_TYPE_PALETTE == info->color_type) {
        int i;

        png->image = newRGBImage(info->width, info->height, info->bit_depth);
       if (!png->image) {
           png->done = image_error;
           return;
       }

        png->image->rgb.red = png->cmap[0];
        png->image->rgb.green = png->cmap[1];
        png->image->rgb.blue = png->cmap[2];
        for (i = 0; i < info->num_palette; ++i) {
            png->image->rgb.red[i] = info->palette[i].red << 8;
            png->image->rgb.green[i] = info->palette[i].green << 8;
            png->image->rgb.blue[i] = info->palette[i].blue << 8;
        }
        png->image->rgb.used = info->num_palette;
        if (info->valid & PNG_INFO_tRNS) {
            int val, i;

            val = 0;
            for (i = 0; i < info->num_trans; ++i) {
                if (info->trans[i] < info->trans[val])
                    val = i;
            }
            png->image->transparent = val;
        }
    } else if (PNG_COLOR_TYPE_GRAY == info->color_type) {
        int i;
        int depth = orig_depth ? orig_depth : info->bit_depth;
        int maxval = (1 << depth) - 1;

        png->image = newRGBImage(info->width, info->height, depth);
       if (!png->image) {
           png->done = image_error;
           return;
       }

        /* png->image->type = IGRAY; */
        png->image->rgb.red = png->cmap[0];
        png->image->rgb.green = png->cmap[1];
        png->image->rgb.blue = png->cmap[2];
        for (i = 0; i <= maxval; i++) {
            png->image->rgb.red[i] = PM_SCALE(i, maxval, 0xffff);
            png->image->rgb.green[i] = PM_SCALE(i, maxval, 0xffff);
            png->image->rgb.blue[i] = PM_SCALE(i, maxval, 0xffff);
        }
        png->image->rgb.used = maxval + 1;

        if (info->valid & PNG_INFO_tRNS)
            png->image->transparent = info->trans_values.gray;
    } else {
        png->image = newTrueImage(info->width, info->height);
       if (!png->image) {
           png->done = image_error;
           return;
       }

    }

    if (info->valid & PNG_INFO_gAMA && png->image->type != IBITMAP)
        png->image->gamma = 1.0 / info->gamma;

    assert((png->image->width * png->image->pixlen + 7) / 8 == info->rowbytes);
}


static void
lf_row_callback(png_struct *state, png_byte *new_row, png_uint_32 row_num,
    int pass)
{
    pngState *png;
    byte *old_row;

    if (!new_row)
        return;

    png = (pngState *) png_get_progressive_ptr(state);
    if (!png->image)
       return;

    old_row = png->image->data + png->image->bytes_per_line * row_num;

    png_progressive_combine_row(state, old_row, new_row);
    if (png->lineProc) {
        /* I can't say I'm too fond of this endian business. */
#ifdef CHIMERA_LITTLE_ENDIAN
        if (IBITMAP == png->image->type)
           lc_reverse_byte(old_row, png->info->rowbytes);
#endif
        (png->lineProc)(png->closure, row_num, row_num);
#ifdef CHIMERA_LITTLE_ENDIAN
        if (IBITMAP == png->image->type)
           lc_reverse_byte(old_row,png->info->rowbytes);
#endif
    }
}


static void
lf_end_callback(png_struct *state, png_info *info)
{
    pngState *png= (pngState *) png_get_progressive_ptr(state);
    png->done = image_success;
}


static void
pngDestroy(void *pointer)
{
    pngState *png = (pngState *) pointer;

    if (!png)
        return;

    if (setjmp(png->state->jmpbuf))
        return;

    if (png->state) {
        png_read_destroy(png->state, png->info, (png_info *) 0);
        free_mem(png->state);
        png->state = 0;
    }

    if (png->info) {
        free_mem(png->info);
        png->info = 0;
    }

    if (png->image) {
        freeImage(png->image);
        png->image = 0;
    }

    free_mem(png);
}

static int
pngAddData(void *pointer, byte *data, int len, bool data_ended)
{
    pngState *png = (pngState *) pointer;

    if (setjmp(png->state->jmpbuf))
        return image_error;

    if (len > png->lenSoFar) {
        png_process_data(png->state, png->info, data + png->lenSoFar,
            len - png->lenSoFar);
        png->lenSoFar = len;
    }

    if (image_need_data == png->done && data_ended)
        return image_error;

    return png->done;
}


void
pngInit(void (*lineProc)(void *, int, int), void *closure, struct ifs_vector *ifsv)
{
    pngState *png;

    ifsv->image_format_closure = 0;
    png = (pngState *) alloc_mem(sizeof(pngState));
    if (!png)
         return;

    memset(png, 0, sizeof(pngState));
    png->lineProc = lineProc;
    png->closure = closure;
    png->state = (png_struct *) alloc_mem(sizeof(png_struct));
    if (!png->state)
        return;

    png->info = (png_info *) alloc_mem(sizeof(png_info));
    if (!png->info) {
        free_mem(png->state);
        return;
    }

    if (setjmp(png->state->jmpbuf)) {
        png_read_destroy(png->state, png->info, (png_info *) 0);
        free_mem(png->state);
        free_mem(png->info);
        png->state = 0;
        png->info = 0;
        return;
    }

    png_info_init(png->info);
    png_read_init(png->state);

    png_set_progressive_read_fn(png->state, (void *) png, lf_info_callback,
        lf_row_callback, lf_end_callback);
    png->done = image_need_data;
    png->lenSoFar = 0;

    ifsv->initProc = &pngInit;
    ifsv->destroyProc = &pngDestroy;
    ifsv->addDataProc = &pngAddData;
    ifsv->getImageProc = &pngGetImage;
    ifsv->image_format_closure = (void *) png;
}
