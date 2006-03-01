/* 
 * The Python Imaging Library
 * $Id: Chops.c 2134 2004-10-06 08:55:20Z fredrik $
 *
 * basic channel operations
 *
 * history:
 *	96-03-28 fl:	Created
 *	96-08-13 fl:	Added and/or/xor for "1" images
 *	96-12-14 fl:	Added add_modulo, sub_modulo
 *
 * Copyright (c) Fredrik Lundh 1996.
 * Copyright (c) Secret Labs AB 1997.
 *
 * See the README file for details on usage and redistribution.
 */


#include "Imaging.h"

#define	CHOP(operation, mode)\
    int x, y;\
    Imaging imOut;\
    imOut = create(imIn1, imIn2, mode);\
    if (!imOut)\
	return NULL;\
    for (y = 0; y < imOut->ysize; y++) {\
	UINT8* out = (UINT8*) imOut->image[y];\
	UINT8* in1 = (UINT8*) imIn1->image[y];\
	UINT8* in2 = (UINT8*) imIn2->image[y];\
	for (x = 0; x < imOut->linesize; x++) {\
	    int temp = operation;\
	    if (temp <= 0)\
		out[x] = 0;\
	    else if (temp >= 255)\
		out[x] = 255;\
	    else\
		out[x] = temp;\
	}\
    }\
    return imOut;

#define	CHOP2(operation, mode)\
    int x, y;\
    Imaging imOut;\
    imOut = create(imIn1, imIn2, mode);\
    if (!imOut)\
	return NULL;\
    for (y = 0; y < imOut->ysize; y++) {\
	UINT8* out = (UINT8*) imOut->image[y];\
	UINT8* in1 = (UINT8*) imIn1->image[y];\
	UINT8* in2 = (UINT8*) imIn2->image[y];\
	for (x = 0; x < imOut->linesize; x++) {\
	    out[x] = operation;\
	}\
    }\
    return imOut;

static Imaging
create(Imaging im1, Imaging im2, char* mode)
{
    int xsize, ysize;

    if (!im1 || !im2 || im1->type != IMAGING_TYPE_UINT8 ||
        (mode != NULL && (strcmp(im1->mode, "1") || strcmp(im2->mode, "1"))))
	return (Imaging) ImagingError_ModeError();
    if (im1->type  != im2->type  ||
        im1->bands != im2->bands)
	return (Imaging) ImagingError_Mismatch();

    xsize = (im1->xsize < im2->xsize) ? im1->xsize : im2->xsize;
    ysize = (im1->ysize < im2->ysize) ? im1->ysize : im2->ysize;

    return ImagingNew(im1->mode, xsize, ysize);
}

Imaging
ImagingChopLighter(Imaging imIn1, Imaging imIn2)
{
    CHOP((in1[x] > in2[x]) ? in1[x] : in2[x], NULL);
}

Imaging
ImagingChopDarker(Imaging imIn1, Imaging imIn2)
{
    CHOP((in1[x] < in2[x]) ? in1[x] : in2[x], NULL);
}

Imaging
ImagingChopDifference(Imaging imIn1, Imaging imIn2)
{
    CHOP(abs((int) in1[x] - (int) in2[x]), NULL);
}

Imaging
ImagingChopMultiply(Imaging imIn1, Imaging imIn2)
{
    CHOP((int) in1[x] * (int) in2[x] / 255, NULL);
}

Imaging
ImagingChopScreen(Imaging imIn1, Imaging imIn2)
{
    CHOP(255 - ((int) (255 - in1[x]) * (int) (255 - in2[x])) / 255, NULL);
}

Imaging
ImagingChopAdd(Imaging imIn1, Imaging imIn2, float scale, int offset)
{
    CHOP(((int) in1[x] + (int) in2[x]) / scale + offset, NULL);
}

Imaging
ImagingChopSubtract(Imaging imIn1, Imaging imIn2, float scale, int offset)
{
    CHOP(((int) in1[x] - (int) in2[x]) / scale + offset, NULL);
}

Imaging
ImagingChopAnd(Imaging imIn1, Imaging imIn2)
{
    CHOP2(in1[x] && in2[x], "1");
}

Imaging
ImagingChopOr(Imaging imIn1, Imaging imIn2)
{
    CHOP2(in1[x] | in2[x], "1");
}

Imaging
ImagingChopXor(Imaging imIn1, Imaging imIn2)
{
    CHOP2((in1[x] != 0) ^ (in2[x] != 0), "1");
}

Imaging
ImagingChopAddModulo(Imaging imIn1, Imaging imIn2)
{
    CHOP2(in1[x] + in2[x], NULL);
}

Imaging
ImagingChopSubtractModulo(Imaging imIn1, Imaging imIn2)
{
    CHOP2(in1[x] - in2[x], NULL);
}
