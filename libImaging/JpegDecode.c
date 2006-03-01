/*
 * The Python Imaging Library.
 * $Id$
 *
 * decoder for JPEG image data.
 *
 * history:
 * 96-05-02 fl	Created
 * 96-05-05 fl	Handle small JPEG files correctly
 * 96-05-28 fl	Added "draft mode" support
 * 97-01-25 fl	Added colour conversion override
 * 98-01-31 fl	Adapted to libjpeg 6a
 * 98-07-12 fl	Extended YCbCr support
 * 98-12-29 fl	Added new state to handle suspension in multipass modes
 *
 * Copyright (c) Secret Labs AB 1998.
 * Copyright (c) Fredrik Lundh 1996-97.
 *
 * See the README file for details on usage and redistribution.
 */


#include "Imaging.h"

#ifdef	HAVE_LIBJPEG

#undef HAVE_PROTOTYPES 
#undef HAVE_STDLIB_H 
#undef UINT8
#undef UINT16
#undef UINT32
#undef INT16
#undef INT32

#include "Jpeg.h"


/* -------------------------------------------------------------------- */
/* Suspending input handler						*/
/* -------------------------------------------------------------------- */

METHODDEF(void)
stub(j_decompress_ptr cinfo)
{
    /* empty */
}

METHODDEF(boolean)
fill_input_buffer(j_decompress_ptr cinfo)
{
    /* Suspension */
    return FALSE;
}

METHODDEF(void)
skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    JPEGSOURCE* source = (JPEGSOURCE*) cinfo->src;

    if (num_bytes > (long) source->pub.bytes_in_buffer) {
	/* We need to skip more data than we have in the buffer.
	   This will force the JPEG library to suspend decoding. */
	source->skip = num_bytes - source->pub.bytes_in_buffer;
	source->pub.next_input_byte += source->pub.bytes_in_buffer;
	source->pub.bytes_in_buffer = 0;
    } else {
	/* Skip portion of the buffer */
	source->pub.bytes_in_buffer -= num_bytes;
	source->pub.next_input_byte += num_bytes;
	source->skip = 0;
    }
}


GLOBAL(void)
jpeg_buffer_src(j_decompress_ptr cinfo, JPEGSOURCE* source)
{
  cinfo->src = (void*) source;

  /* Prepare for suspending reader */
  source->pub.init_source = stub;
  source->pub.fill_input_buffer = fill_input_buffer;
  source->pub.skip_input_data = skip_input_data;
  source->pub.resync_to_restart = jpeg_resync_to_restart;
  source->pub.term_source = stub;
  source->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */

  source->skip = 0;
}


/* -------------------------------------------------------------------- */
/* Error handler							*/
/* -------------------------------------------------------------------- */

METHODDEF(void)
error(j_common_ptr cinfo)
{
  JPEGERROR* error;
  error = (JPEGERROR*) cinfo->err;
#if 0
  /* DEBUG */
  cinfo->err->output_message(cinfo);
#endif
  longjmp(error->setjmp_buffer, 1);
}


/* -------------------------------------------------------------------- */
/* Decoder								*/
/* -------------------------------------------------------------------- */

int
ImagingJpegDecode(Imaging im, ImagingCodecState state, UINT8* buf, int bytes)
{
    JPEGSTATE* context = (JPEGSTATE*) state->context;
    int ok;

    if (setjmp(context->error.setjmp_buffer)) {
	/* JPEG error handler */
	jpeg_destroy_decompress(&context->cinfo);
	state->errcode = IMAGING_CODEC_BROKEN;
	return -1;
    }

    if (!state->state) {

	/* Setup decompression context */
	context->cinfo.err = jpeg_std_error(&context->error.pub);
	context->error.pub.error_exit = error;
	jpeg_create_decompress(&context->cinfo);
	jpeg_buffer_src(&context->cinfo, &context->source);

	/* Ready to decode */
	state->state = 1;

    }

    /* Load the source buffer */
    context->source.pub.next_input_byte = buf;
    context->source.pub.bytes_in_buffer = bytes;

    if (context->source.skip > 0) {
	/* There's more data to skip */
	skip_input_data(&context->cinfo, context->source.skip);
	if (context->source.skip > 0)
	    return context->source.pub.next_input_byte - buf; 
    }

    switch (state->state) {

    case 1:

	/* Read JPEG header, until we find an image body. */
	do {
	    
	    /* Note that we cannot return unless we have decoded
	       as much data as possible. */
	    ok = jpeg_read_header(&context->cinfo, FALSE);

	} while (ok == JPEG_HEADER_TABLES_ONLY);

	if (ok == JPEG_SUSPENDED)
	    break;

	/* Decoder settings */

	/* jpegmode indicates whats in the file; if not set, we'll
	   trust the decoder */
	if (strcmp(context->jpegmode, "L") == 0)
	    context->cinfo.jpeg_color_space = JCS_GRAYSCALE;
	else if (strcmp(context->jpegmode, "RGB") == 0)
	    context->cinfo.jpeg_color_space = JCS_RGB;
	else if (strcmp(context->jpegmode, "YCbCr") == 0)
	    context->cinfo.jpeg_color_space = JCS_YCbCr;
	else if (strcmp(context->jpegmode, "CMYK") == 0)
	    context->cinfo.jpeg_color_space = JCS_CMYK;
	else if (strcmp(context->jpegmode, "YCbCrK") == 0) {
	    context->cinfo.jpeg_color_space = JCS_YCCK;
	}

	/* rawmode indicates what we want from the decoder.  if not
	   set, conversions are disabled */
	if (strcmp(context->rawmode, "L") == 0)
	    context->cinfo.out_color_space = JCS_GRAYSCALE;
	else if (strcmp(context->rawmode, "RGB") == 0)
	    context->cinfo.out_color_space = JCS_RGB;
	else if (strcmp(context->rawmode, "CMYK") == 0)
	    context->cinfo.out_color_space = JCS_CMYK;
	else if (strcmp(context->rawmode, "YCbCr") == 0)
	    context->cinfo.out_color_space = JCS_YCbCr;
	else if (strcmp(context->rawmode, "YCbCrK") == 0)
	    context->cinfo.out_color_space = JCS_YCCK;
	else {
	    /* Disable decoder conversions */
	    context->cinfo.jpeg_color_space = JCS_UNKNOWN;
	    context->cinfo.out_color_space = JCS_UNKNOWN;
	}

	if (context->scale > 1) {
	    context->cinfo.scale_num = 1;
	    context->cinfo.scale_denom = context->scale;
	}
	if (context->draft) {
	    context->cinfo.do_fancy_upsampling = FALSE;
	    context->cinfo.dct_method = JDCT_FASTEST;
	}

        state->state++;
	/* fall through */

    case 2:

        /* Set things up for decompression (this processes the entire
           file if necessary to return data line by line) */
	if (!jpeg_start_decompress(&context->cinfo))
            break;
        
	state->state++;
	/* fall through */

    case 3:

	/* Decompress a single line of data */
	ok = 1;
	while (state->y < state->ysize) {
	    ok = jpeg_read_scanlines(&context->cinfo, &state->buffer, 1);
	    if (ok != 1)
		break;
	    state->shuffle((UINT8*) im->image[state->y + state->yoff] +
			   state->xoff * im->pixelsize, state->buffer,
			   state->xsize);
	    state->y++;
	}
	if (ok != 1)
	    break;
	state->state++;
	/* fall through */

    case 4:

	/* Finish decompression */
	if (!jpeg_finish_decompress(&context->cinfo))
	    break;

	/* Clean up */
	jpeg_destroy_decompress(&context->cinfo);
	/* if (jerr.pub.num_warnings) return BROKEN; */
	return -1;

    }

    /* Return number of bytes consumed */
    return context->source.pub.next_input_byte - buf; 

}

#endif

