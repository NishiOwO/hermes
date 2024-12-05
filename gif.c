/*    gif.c - Dave Raggett, 16th December 1994
            - revised 9th Feb 1994 to speed decoding & add gif89 support

   Derived from the pbmplus package, so we include David's copyright.

   The colors are preallocated by image.c with 4:8:4 R:G:B colors and
   16 grey scales. These are rendered using an ordered dither based on
   16x16 dither matrices for monochrome, greyscale and color images.

   This approach ensures that we won't run out of colors regardless of
   how many images are on screen or how many independent copies of www
   are running at any time. The scheme is compatible with HP's MPOWER
   tools. The code will be extended to take advantage of 24 bit direct
   color or secondary true color hardware colormaps as soon as practical.
*/

/* +-------------------------------------------------------------------+ */
/* | Copyright 1990, David Koblas.                                     | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */


#include <X11/Intrinsic.h>
#include <stdio.h>
#include <stdlib.h>
#include "www.h"

#define MAXCOLORMAPSIZE 256

#define TRUE    1
#define FALSE   0

#define CM_RED          0
#define CM_GREEN        1
#define CM_BLUE         2

#define MAX_LWZ_BITS    12

#define INTERLACE       0x40
#define LOCALCOLORMAP   0x80

#if 0 /* replaced by version reading from memory buffer */
#define ReadOK(file,buffer,len) (fread(buffer, len, 1, file) != 0)
#endif

#define BitSet(byte, bit) (((byte) & (bit)) == (bit))
#define LM_to_uint(a,b)   (((b)<<8)|(a))

extern Display *display;
extern int screen;
extern int depth;
extern Colormap colormap;
extern int imaging;  /* set to COLOR888, COLOR232, GREY4 or MONO */
extern unsigned long windowColor;
unsigned long stdcmap[128];  /* 2/3/2 color maps for gifs etc */
extern unsigned long greymap[16];

static int GetDataBlock(Block *bp, unsigned char *buf);

int Magic256[256] =    /* for halftoning */
{
    0, 223, 48, 207, 14, 237, 62, 221, 3, 226, 51, 210, 13, 236, 61, 220,
    175, 80, 128, 96, 189, 94, 141, 110, 178, 83, 130, 99, 188, 93, 140, 109,
    191, 32, 239, 16, 205, 46, 253, 30, 194, 35, 242, 19, 204, 45, 252, 29,
    112, 143, 64, 159, 126, 157, 78, 173, 115, 146, 67, 162, 125, 156, 77, 172,
    11, 234, 59, 218, 5, 228, 53, 212, 8, 231, 56, 215, 6, 229, 54, 213,
    186, 91, 138, 107, 180, 85, 132, 101, 183, 88, 135, 104, 181, 86, 133, 102,
    202, 43, 250, 27, 196, 37, 244, 21, 199, 40, 247, 24, 197, 38, 245, 22,
    123, 154, 75, 170, 117, 148, 69, 164, 120, 151, 72, 167, 118, 149, 70, 165,
    12, 235, 60, 219, 2, 225, 50, 209, 15, 238, 63, 222, 1, 224, 49, 208,
    187, 92, 139, 108, 177, 82, 129, 98, 190, 95, 142, 111, 176, 81, 128, 97,
    203, 44, 251, 28, 193, 34, 241, 18, 206, 47, 254, 31, 192, 33, 240, 17,
    124, 155, 76, 171, 114, 145, 66, 161, 127, 158, 79, 174, 113, 144, 65, 160,
    7, 230, 55, 214, 9, 232, 57, 216, 4, 227, 52, 211, 10, 233, 58, 217,
    182, 87, 134, 103, 184, 89, 136, 105, 179, 84, 131, 100, 185, 90, 137, 106,
    198, 39, 246, 23, 200, 41, 248, 25, 195, 36, 243, 20, 201, 42, 249, 26,
    119, 150, 71, 166, 121, 152, 73, 168, 116, 147, 68, 163, 122, 153, 74, 169

};

int Magic16[256] =    /* for 16 levels of gray */
{
    0, 13, 3, 12, 1, 14, 4, 13, 0, 13, 3, 12, 1, 14, 4, 13,
    10, 5, 8, 6, 11, 6, 8, 6, 10, 5, 8, 6, 11, 5, 8, 6,
    11, 2, 14, 1, 12, 3, 15, 2, 11, 2, 14, 1, 12, 3, 15, 2,
    7, 8, 4, 9, 7, 9, 5, 10, 7, 9, 4, 10, 7, 9, 5, 10,
    1, 14, 3, 13, 0, 13, 3, 12, 0, 14, 3, 13, 0, 13, 3, 13,
    11, 5, 8, 6, 11, 5, 8, 6, 11, 5, 8, 6, 11, 5, 8, 6,
    12, 3, 15, 2, 12, 2, 14, 1, 12, 2, 15, 1, 12, 2, 14, 1,
    7, 9, 4, 10, 7, 9, 4, 10, 7, 9, 4, 10, 7, 9, 4, 10,
    1, 14, 4, 13, 0, 13, 3, 12, 1, 14, 4, 13, 0, 13, 3, 12,
    11, 5, 8, 6, 10, 5, 8, 6, 11, 6, 8, 7, 10, 5, 8, 6,
    12, 3, 15, 2, 11, 2, 14, 1, 12, 3, 15, 2, 11, 2, 14, 1,
    7, 9, 4, 10, 7, 9, 4, 9, 7, 9, 5, 10, 7, 8, 4, 9,
    0, 14, 3, 13, 1, 14, 3, 13, 0, 13, 3, 12, 1, 14, 3, 13,
    11, 5, 8, 6, 11, 5, 8, 6, 11, 5, 8, 6, 11, 5, 8, 6,
    12, 2, 14, 1, 12, 2, 15, 1, 11, 2, 14, 1, 12, 2, 15, 2,
    7, 9, 4, 10, 7, 9, 4, 10, 7, 9, 4, 10, 7, 9, 4, 10

};

int Magic32[256] =    /* for 8 levels of green */
{
    0, 27, 6, 25, 2, 29, 8, 27, 0, 27, 6, 26, 2, 29, 7, 27,
    21, 10, 16, 12, 23, 11, 17, 13, 22, 10, 16, 12, 23, 11, 17, 13,
    23, 4, 29, 2, 25, 6, 31, 4, 24, 4, 29, 2, 25, 5, 31, 4,
    14, 17, 8, 19, 15, 19, 9, 21, 14, 18, 8, 20, 15, 19, 9, 21,
    1, 28, 7, 27, 1, 28, 6, 26, 1, 28, 7, 26, 1, 28, 7, 26,
    23, 11, 17, 13, 22, 10, 16, 12, 22, 11, 16, 13, 22, 10, 16, 12,
    25, 5, 30, 3, 24, 4, 30, 3, 24, 5, 30, 3, 24, 5, 30, 3,
    15, 19, 9, 21, 14, 18, 8, 20, 15, 18, 9, 20, 14, 18, 8, 20,
    1, 29, 7, 27, 0, 27, 6, 25, 2, 29, 8, 27, 0, 27, 6, 25,
    23, 11, 17, 13, 22, 10, 16, 12, 23, 12, 17, 13, 21, 10, 16, 12,
    25, 5, 31, 3, 23, 4, 29, 2, 25, 6, 31, 4, 23, 4, 29, 2,
    15, 19, 9, 21, 14, 18, 8, 20, 15, 19, 10, 21, 14, 18, 8, 19,
    1, 28, 7, 26, 1, 28, 7, 26, 0, 28, 6, 26, 1, 28, 7, 26,
    22, 11, 16, 12, 22, 11, 17, 13, 22, 10, 16, 12, 23, 11, 17, 13,
    24, 5, 30, 3, 24, 5, 30, 3, 24, 4, 30, 2, 24, 5, 30, 3,
    14, 18, 9, 20, 15, 19, 9, 20, 14, 18, 8, 20, 15, 19, 9, 21

};

int Magic64[256] =    /* for 4 levels of red and blue */
{
    0, 55, 12, 51, 3, 59, 15, 55, 1, 56, 13, 52, 3, 58, 15, 54,
    43, 20, 32, 24, 47, 23, 35, 27, 44, 20, 32, 24, 47, 23, 35, 27,
    47, 8, 59, 4, 51, 11, 63, 7, 48, 9, 60, 5, 50, 11, 62, 7,
    28, 35, 16, 39, 31, 39, 19, 43, 28, 36, 16, 40, 31, 39, 19, 43,
    3, 58, 15, 54, 1, 56, 13, 52, 2, 57, 14, 53, 1, 57, 13, 53,
    46, 22, 34, 26, 45, 21, 33, 25, 45, 22, 33, 26, 45, 21, 33, 25,
    50, 11, 62, 7, 48, 9, 60, 5, 49, 10, 61, 6, 49, 9, 61, 5,
    30, 38, 18, 42, 29, 37, 17, 41, 30, 37, 18, 41, 29, 37, 17, 41,
    3, 58, 15, 54, 0, 56, 12, 52, 4, 59, 16, 55, 0, 55, 12, 51,
    46, 23, 34, 27, 44, 20, 32, 24, 47, 23, 35, 27, 44, 20, 32, 24,
    50, 11, 62, 7, 48, 8, 60, 4, 51, 12, 63, 8, 47, 8, 59, 4,
    31, 38, 19, 42, 28, 36, 16, 40, 31, 39, 19, 43, 28, 36, 16, 40,
    2, 57, 14, 53, 2, 57, 14, 53, 1, 56, 13, 52, 2, 58, 14, 54,
    45, 21, 33, 25, 46, 22, 34, 26, 44, 21, 32, 25, 46, 22, 34, 26,
    49, 10, 61, 6, 49, 10, 61, 6, 48, 9, 60, 5, 50, 10, 62, 6,
    29, 37, 17, 41, 30, 38, 18, 42, 29, 36, 17, 40, 30, 38, 18, 42

};

struct
    {
        unsigned int    Width;
        unsigned int    Height;
        Color           colors[MAXCOLORMAPSIZE];
        unsigned int    BitPixel;
        unsigned int    ColorResolution;
        unsigned int    Background;
        unsigned int    AspectRatio;
        int             xGreyScale;
    } GifScreen;

struct
    {
        int     transparent;
        int     delayTime;
        int     inputFlag;
        int     disposal;
    } Gif89 = { -1, -1, -1, 0 };

int verbose = FALSE;
int showComment = FALSE;
int ZeroDataBlock = FALSE;

size_t ReadOK(Block *bp, unsigned char *buffer, int len)
{
    if (bp->size > bp->next)
    {
        if (bp->next + len > bp->size)
            len = bp->size - bp->next;

        memmove(buffer, bp->buffer + bp->next, len);
        bp->next += len;
        return len;
    }

    return 0;
}

/*
**  Pulled out of nextCode
*/
static  int             curbit, lastbit, get_done, last_byte;
static  int             return_clear;
/*
**  Out of nextLWZ
*/
static int      stack[(1<<(MAX_LWZ_BITS))*2], *sp;
static int      code_size, set_code_size;
static int      max_code, max_code_size;
static int      clear_code, end_code;

static void initLWZ(int input_code_size)
{
        static int      inited = FALSE;

        set_code_size = input_code_size;
        code_size     = set_code_size + 1;
        clear_code    = 1 << set_code_size ;
        end_code      = clear_code + 1;
        max_code_size = 2 * clear_code;
        max_code      = clear_code + 2;

        curbit = lastbit = 0;
        last_byte = 2;
        get_done = FALSE;

        return_clear = TRUE;

        sp = stack;
}

static int nextCode(Block *bp, int code_size)
{
        static unsigned char    buf[280];
        static int maskTbl[16] = {
                0x0000, 0x0001, 0x0003, 0x0007,
                0x000f, 0x001f, 0x003f, 0x007f,
                0x00ff, 0x01ff, 0x03ff, 0x07ff,
                0x0fff, 0x1fff, 0x3fff, 0x7fff,
        };
        int                     i, j, ret, end;

        if (return_clear) {
                return_clear = FALSE;
                return clear_code;
        }

        end = curbit + code_size;

        if (end >= lastbit) {
                int     count;

                if (get_done) {
                        if (curbit >= lastbit)
                        {
#if 0
                                ERROR("ran off the end of my bits" );
#endif
                        }
                        return -1;
                }
                buf[0] = buf[last_byte-2];
                buf[1] = buf[last_byte-1];

                if ((count = GetDataBlock(bp, &buf[2])) == 0)
                        get_done = TRUE;

                last_byte = 2 + count;
                curbit = (curbit - lastbit) + 16;
                lastbit = (2+count)*8 ;

                end = curbit + code_size;
        }

        j = end / 8;
        i = curbit / 8;

        if (i == j)
                ret = (int)buf[i];
        else if (i + 1 == j)
                ret = (int)buf[i] | ((int)buf[i+1] << 8);
        else
                ret = (int)buf[i] | ((int)buf[i+1] << 8) | ((int)buf[i+2] << 16);

        ret = (ret >> (curbit % 8)) & maskTbl[code_size];

        curbit += code_size;

        return ret;
}

#define readLWZ(bp) ((sp > stack) ? *--sp : nextLWZ(bp))

static int nextLWZ(Block *bp)
{
        static int       table[2][(1<< MAX_LWZ_BITS)];
        static int       firstcode, oldcode;
        int              code, incode;
        register int     i;

        while ((code = nextCode(bp, code_size)) >= 0) {
               if (code == clear_code) {

                        /* corrupt GIFs can make this happen */
                        if (clear_code >= (1<<MAX_LWZ_BITS))
                        {
                                return -2;
                        }

                       for (i = 0; i < clear_code; ++i) {
                               table[0][i] = 0;
                               table[1][i] = i;
                       }
                       for (; i < (1<<MAX_LWZ_BITS); ++i)
                               table[0][i] = table[1][i] = 0;
                       code_size = set_code_size+1;
                       max_code_size = 2*clear_code;
                       max_code = clear_code+2;
                       sp = stack;
                        do {
                               firstcode = oldcode = nextCode(bp, code_size);
                        } while (firstcode == clear_code);

                        return firstcode;
               }
               if (code == end_code) {
                       int             count;
                       unsigned char   buf[260];

                       if (ZeroDataBlock)
                               return -2;

                       while ((count = GetDataBlock(bp, buf)) > 0)
                               ;

                       if (count != 0)
                        {
#if 0
                               INFO_MSG(("missing EOD in data stream (common occurence)"));
#endif
                        }
                       return -2;
               }

               incode = code;

               if (code >= max_code) {
                       *sp++ = firstcode;
                       code = oldcode;
               }

               while (code >= clear_code) {
                       *sp++ = table[1][code];
                       if (code == table[0][code])
                        {
#if 0
                               ERROR("circular table entry BIG ERROR");
                               return(code);
#endif
                        }
                       code = table[0][code];
               }

               *sp++ = firstcode = table[1][code];

               if ((code = max_code) <(1<<MAX_LWZ_BITS)) {
                       table[0][code] = oldcode;
                       table[1][code] = firstcode;
                       ++max_code;
                       if ((max_code >= max_code_size) &&
                               (max_code_size < (1<<MAX_LWZ_BITS))) {
                               max_code_size *= 2;
                               ++code_size;
                       }
               }

               oldcode = incode;

               if (sp > stack)
                       return *--sp;
        }
        return code;
}


static int GetDataBlock(Block *bp, unsigned char *buf)
{
    unsigned char count;

    count = 0;

    if (! ReadOK(bp,&count,1))
    {
        fprintf(stderr, "error in getting DataBlock size\n");
        return -1;
    }

    ZeroDataBlock = count == 0;

    if ((count != 0) && (! ReadOK(bp, buf, count)))
    {
        fprintf(stderr, "error in reading DataBlock\n");
        return -1;
    }

    return (int)(count);
}

static unsigned char *ReadImage(Block *bp, int len, int height,
     Color colors[MAXCOLORMAPSIZE], int grey, int interlace, int ignore)
{
    unsigned char   c;      
    int             v;
    int             xpos = 0, ypos = 0, pass = 0;
    unsigned char   *data, *dp, *dy;
    int             cr, cg, cb, r, g, b, row, col;
    Color           color;

  /*
   *  Initialize the Compression routines
   */

    if (! ReadOK(bp,&c,1))
    {
        fprintf(stderr, "EOF / read error on image data\n");
        return(NULL);
    }

    initLWZ(c);

   /*
    *  If this is an "uninteresting picture" ignore it.
    */

    if (ignore)
    {
        if (verbose)
            fprintf(stderr, "skipping image...\n" );

        while (readLWZ(bp) >= 0);

        return NULL;
    }

    data = (unsigned char *)malloc(len * height);

    if (data == NULL)
    {
        fprintf(stderr, "Cannot allocate space for image data\n");
        return(NULL);
    }

    if (verbose)
        fprintf(stderr, "reading %d by %d%s GIF image\n",
                 len, height, interlace ? " interlaced" : "" );

    if (interlace)
    {
        int i;
        int pass = 0, step = 8;

        for (i = 0; i < height; ++i)
        {
            dp = &data[len*ypos];
            col = ypos & 15;

            for (xpos = 0; xpos < len; ++xpos)
            {
                if ((v = readLWZ(bp)) < 0)
                    goto fini;

                if (v == Gif89.transparent)
                {
                    *dp++ = windowColor;
                    continue;
                }

                color = colors[v];
                row = xpos & 15;

                if (!grey && imaging == COLOR232)
                {
                    if (color.grey > 0)
                        goto grey_color1;

                    cr = color.red;
                    cg = color.green;
                    cb = color.blue;

                    r = cr & 0xC0;
                    g = cg & 0xE0;
                    b = cb & 0xC0;

                    v = (row << 4) + col;

                    if (cr - r > Magic64[v])
                        r += 64;

                    if (cg - g > Magic32[v])
                        g += 32;

                    if (cb - b > Magic64[v])
                        b += 64;

                 /* clamp error to keep color in range 0 to 255 */

                    r = min(r, 255) & 0xC0;
                    g = min(g, 255) & 0xE0;
                    b = min(b, 255) & 0xC0;

                    *dp++ = stdcmap[(r >> 6) | (g >> 3) | (b >> 1)];
                    continue;
                }

                if (imaging == MONO)
                {
                    if (color.grey < Magic256[(row << 4) + col])
                        *dp++ = greymap[0];
                    else
                        *dp++ = greymap[15];
                    continue;
                }

           grey_color1:

                cg  = color.grey;
                g = cg & 0xF0;

                if (cg - g > Magic16[(row << 4) + col])
                    g += 16;

                g = min(g, 0xF0);
                *dp++ = greymap[g >> 4];
            }

            if ((ypos += step) >= height)
            {
                if (pass++ > 0)
                    step /= 2;

                ypos = step /2;
            }
        }
    }
    else
    {
        dp = data;

        for (ypos = 0; ypos < height; ++ypos)
        {
            col = ypos & 15;

            for (xpos = 0; xpos < len; ++xpos)
            {
                if ((v = readLWZ(bp)) < 0)
                    goto fini;

                if (v == Gif89.transparent)
                {
                    *dp++ = windowColor;
                    continue;
                }

                color = colors[v];
                row = xpos & 15;

                if (!grey && imaging == COLOR232)
                {
                    if (color.grey > 0)
                        goto grey_color2;

                    cr = color.red;
                    cg = color.green;
                    cb = color.blue;

                    r = cr & 0xC0;
                    g = cg & 0xE0;
                    b = cb & 0xC0;

                    v = (row << 4) + col;

                    if (cr - r > Magic64[v])
                        r += 64;

                    if (cg - g > Magic32[v])
                        g += 32;

                    if (cb - b > Magic64[v])
                        b += 64;

                 /* clamp error to keep color in range 0 to 255 */

                    r = min(r, 255) & 0xC0;
                    g = min(g, 255) & 0xE0;
                    b = min(b, 255) & 0xC0;

                    *dp++ = stdcmap[(r >> 6) | (g >> 3) | (b >> 1)];
                    continue;
                }

                if (imaging == MONO)
                {
                    if (color.grey < Magic256[(row << 4) + col])
                        *dp++ = greymap[0];
                    else
                        *dp++ = greymap[15];
                    continue;
                }

            grey_color2:

                cg  = color.grey;
                g = cg & 0xF0;

                if (cg - g > Magic16[(row << 4) + col])
                    g += 16;

                g = min(g, 0xF0);
                *dp++ = greymap[g >> 4];
            }
        }
    }

 fini:

    if (readLWZ(bp) >= 0)
        fprintf(stderr, "too much input data, ignoring extra...\n");

    return data;
}

static unsigned char *ReadImage24(Block *bp, int len, int height,
     Color colors[MAXCOLORMAPSIZE], int nColors, int interlace, int ignore)
{
    unsigned char   c;      
    int             v;
    int             xpos = 0, ypos = 0, pass = 0;
    unsigned char   *dp;
    unsigned long pixels[MAXCOLORMAPSIZE], *pp, *data24;

   /* setup pixel table for faster rendering */

    dp = (unsigned char *)&(pixels[0]);

    for (v = 0; v < nColors; ++v)
    {
        if (v == Gif89.transparent)
        {
            *dp++ = '\0';
            *dp++ = (windowColor >> 16) & 0xFF;
            *dp++ = (windowColor >>  8) & 0xFF;
            *dp++ = windowColor & 0xFF;
            continue;
        }

        *dp++ = '\0';
        *dp++ = colors[v].red;
        *dp++ = colors[v].green;
        *dp++ = colors[v].blue;
    }

  /*
   *  Initialize the Compression routines
   */

    if (! ReadOK(bp,&c,1))
    {
        fprintf(stderr, "EOF / read error on image data\n");
        return(NULL);
    }

    initLWZ(c);

   /*
    *  If this is an "uninteresting picture" ignore it.
    */

    if (ignore)
    {
        if (verbose)
            fprintf(stderr, "skipping image...\n" );

        while (readLWZ(bp) >= 0);

        return NULL;
    }

    data24 = (unsigned long *)malloc(len * height * sizeof(unsigned long));

    if (data24 == NULL)
    {
        fprintf(stderr, "Cannot allocate space for image data\n");
        return(NULL);
    }

    if (verbose)
        fprintf(stderr, "reading %d by %d%s GIF image\n",
                 len, height, interlace ? " interlaced" : "" );

    if (interlace)
    {
        int i;
        int pass = 0, step = 8;

        for (i = 0; i < height; ++i)
        {
            pp = &data24[len*ypos];

            for (xpos = 0; xpos < len; ++xpos)
            {
                if ((v = readLWZ(bp)) < 0)
                    goto fini;

                *pp++ = pixels[i];
            }

            if ((ypos += step) >= height)
            {
                if (pass++ > 0)
                    step /= 2;

                ypos = step /2;
            }
        }
    }
    else
    {
        pp = data24;

        for (ypos = 0; ypos < height; ++ypos)
        {
            for (xpos = 0; xpos < len; ++xpos)
            {
                if ((v = readLWZ(bp)) < 0)
                    goto fini;

                *pp++ = pixels[v];
            }
        }
    }

 fini:

    if (readLWZ(bp) >= 0)
        fprintf(stderr, "too much input data, ignoring extra...\n");

    return (unsigned char *)data24;
}

static int DoExtension(Block *bp, int label)
{
    static char buf[256];
    char *str;

    switch (label)
    {
        case 0x01:              /* Plain Text Extension */
            str = "Plain Text Extension";
            break;

        case 0xff:              /* Application Extension */
            str = "Application Extension";
            break;

        case 0xfe:              /* Comment Extension */
            str = "Comment Extension";
            while (GetDataBlock(bp, (unsigned char*) buf) != 0)
            {
                if (showComment)
                    fprintf(stderr, "gif comment: %s\n", buf);
            }
            return FALSE;

        case 0xf9:              /* Graphic Control Extension */
            str = "Graphic Control Extension";
            (void) GetDataBlock(bp, (unsigned char*) buf);
            Gif89.disposal    = (buf[0] >> 2) & 0x7;
            Gif89.inputFlag   = (buf[0] >> 1) & 0x1;
            Gif89.delayTime   = LM_to_uint(buf[1],buf[2]);

            if ((buf[0] & 0x1) != 0)
                Gif89.transparent = (int)((unsigned char)buf[3]);

            while (GetDataBlock(bp, (unsigned char*) buf) != 0)
                   ;
            return FALSE;

        default:
            str = buf;
            sprintf(buf, "UNKNOWN (0x%02x)", label);
            break;
    }

    /* fprintf(stderr, "got a '%s' extension\n", str); */

    while (GetDataBlock(bp, (unsigned char*) buf) != 0)
           ;

    return FALSE;
}

static int ReadColorMap(Block *bp, Image *image, int ncolors, Color *colors, int *grey)
{
    int i, flag, npixels, drb, dgb, dbr;
    unsigned char rgb[3];
    unsigned long pixel, *pixels;

    flag = 1;
    image->npixels = npixels = 0;

    for (i = 0; i < ncolors; ++i)
    {
        if (! ReadOK(bp, rgb, sizeof(rgb)))
        {
            fprintf(stderr, "bad colormap\n");
            return 0;
        }

        colors->red = rgb[0];
        colors->green = rgb[1];
        colors->blue = rgb[2];

        drb = abs(rgb[1] - rgb[0]);
        dgb = abs(rgb[2] - rgb[1]);
        dbr = abs(rgb[0] - rgb[2]);

        if (*grey || (drb < 40 && dgb < 40 && dbr < 40))
        {   
            flag &= 1;
            colors->grey = (5*rgb[0] + 6*rgb[1] + 4*rgb[2])/15;
        }
        else
        {
            if (!(*grey))
                flag = 0;

            colors->grey = 0;
        }

        ++colors;
    }

    *grey = flag;
    return 1;
}

unsigned char *LoadGifImage(Image *image, Block *bp, unsigned int depth)
{
    unsigned char   buf[16];
    unsigned char   *data = NULL;
    unsigned char   c;
    Color           *cmap, colors[MAXCOLORMAPSIZE];
    int             useGlobalColormap;
    int             bitPixel;
    int             imageCount = 0;
    char            version[4];
    int             imageNumber = 1;
    int             i;
    int             greyScale = 0;
    unsigned long pixel;
    unsigned int w, h;

    verbose = FALSE;
    showComment = FALSE;

 /* initialize GIF89 extensions */

    Gif89.transparent = -1;
    Gif89.delayTime = -1;
    Gif89.inputFlag = -1;
    Gif89.disposal = 0;

    if (! ReadOK(bp,buf,6))
    {
        fprintf(stderr, "error reading magic number\n");
        return(NULL);
    }

    if (strncmp((char *)buf,"GIF",3) != 0)
    {
        if (verbose)
            fprintf(stderr, "not a GIF file\n");

        return(NULL);
    }

    strncpy(version, (char *)buf + 3, 3);
    version[3] = '\0';

    if ((strcmp(version, "87a") != 0) && (strcmp(version, "89a") != 0))
    {
        fprintf(stderr, "bad version number, not '87a' or '89a'\n");
        return(NULL);
    }

    if (! ReadOK(bp,buf,7))
    {
        fprintf(stderr, "failed to read screen descriptor\n");
        return(NULL);
    }

    GifScreen.Width           = LM_to_uint(buf[0],buf[1]);
    GifScreen.Height          = LM_to_uint(buf[2],buf[3]);
    GifScreen.BitPixel        = 2<<(buf[4]&0x07);
    GifScreen.ColorResolution = (((buf[4]&0x70)>>3)+1);
    GifScreen.Background      = buf[5];
    GifScreen.AspectRatio     = buf[6];
    GifScreen.xGreyScale      = 0;

    if (BitSet(buf[4], LOCALCOLORMAP))
    {    /* Global Colormap */

        if (!ReadColorMap(bp, image, GifScreen.BitPixel, GifScreen.colors,
                                                     &GifScreen.xGreyScale))
        {
             fprintf(stderr, "error reading global colormap\n");
             return(NULL);
        }
    }

    if (GifScreen.AspectRatio != 0 && GifScreen.AspectRatio != 49)
        fprintf(stderr, "Warning:  non-square pixels!\n");

    while (data == NULL)
    {
        if (! ReadOK(bp,&c,1))
        {
            fprintf(stderr, "EOF / read error on image data\n");
            return(NULL);
        }

        if (c == ';')
        {         /* GIF terminator */

            if (imageCount < imageNumber)
            {
                fprintf(stderr, "No images found in file\n");
                return(NULL);
            }
            break;
        }

        if (c == '!')
        {         /* Extension */

            if (! ReadOK(bp,&c,1))
            {
                fprintf(stderr, "EOF / read error on extention function code\n");
                return(NULL);
            }

            DoExtension(bp, c);
            continue;
        }

        if (c != ',')
        {         /* Not a valid start character */
            fprintf(stderr, "bogus character 0x%02x, ignoring\n", (int)c);
            continue;
        }

        ++imageCount;

        if (! ReadOK(bp,buf,9))
        {
            fprintf(stderr,"couldn't read left/top/width/height\n");
            return(NULL);
        }

        useGlobalColormap = ! BitSet(buf[8], LOCALCOLORMAP);

        bitPixel = 1<<((buf[8]&0x07)+1);

     /* Just set width/height for the imageNumber we are requesting */

        if (imageCount == imageNumber)
        {
            image->width = LM_to_uint(buf[4],buf[5]);
            image->height = LM_to_uint(buf[6],buf[7]);
        }

        if (! useGlobalColormap)
        {
            if (!ReadColorMap(bp, image, bitPixel, colors, &greyScale))
            {
                fprintf(stderr, "error reading local colormap\n");
                return(NULL);
            }

            cmap = colors;
        }
        else
        {
            cmap = GifScreen.colors;
            bitPixel = GifScreen.BitPixel;
            greyScale = GifScreen.xGreyScale;
        }

        if (depth == 24 || depth == 12)
        {
            data = ReadImage24(bp, LM_to_uint(buf[4],buf[5]),
                             LM_to_uint(buf[6],buf[7]), cmap, bitPixel,
                 BitSet(buf[8], INTERLACE), imageCount != imageNumber);
        }
        else
        {
            data = ReadImage(bp, LM_to_uint(buf[4],buf[5]),
                             LM_to_uint(buf[6],buf[7]), cmap, greyScale,
                 BitSet(buf[8], INTERLACE), imageCount != imageNumber);
        }

        if (imageCount != imageNumber && data != NULL)
        {
            free(data);
            data = NULL;
        }
    }

    return(data);
}
