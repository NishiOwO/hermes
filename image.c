/* image.c - creates textured background and other pixmap stuff */

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <string.h>
#include <ctype.h>
#include "www.h"

extern Display *display;
extern int screen;
extern Window win;
extern unsigned long labelColor, textColor, statusColor, strikeColor,
            transparent, windowColor, windowBottomShadow, windowShadow;
extern int depth;
extern int IsIndex;
extern Doc NewDoc, CurrentDoc;

extern Pixmap default_pixmap;
extern int default_pixmap_width, default_pixmap_height;
extern Colormap colormap;
extern int Magic256[256];
extern int Magic16[256];
extern int Magic32[256];
extern int Magic64[256];

extern GC disp_gc;
extern unsigned int win_width, win_height;

Pixmap smile, frown;
int imaging; /* set to COLOR888, COLOR232, GREY4 or MONO */
Image *images;  /* linked list of images */
unsigned long stdcmap[128];  /* 2/3/2 color maps for gifs etc */
unsigned long greymap[16];  /* for mixing with unsaturated colors */

#define smile_xbm_width 15
#define smile_xbm_height 15
static char smile_xbm_bits[] = {
   0x1f, 0x7c, 0xe7, 0x73, 0xfb, 0x6f, 0xfd, 0x5f, 0xfd, 0x5f, 0xce, 0x39,
   0xce, 0x39, 0xfe, 0x3f, 0xfe, 0x3f, 0xee, 0x3b, 0xdd, 0x5d, 0x3d, 0x5e,
   0xfb, 0x6f, 0xe7, 0x73, 0x1f, 0x7c};

#define frown_xbm_width 15
#define frown_xbm_height 15
static char frown_xbm_bits[] = {
   0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f,
   0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f,
   0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f};

void MakeFaces(unsigned int depth)
{
    smile = XCreateBitmapFromData(display, win, smile_xbm_bits,
                    smile_xbm_width, smile_xbm_height);

    frown = XCreateBitmapFromData(display, win, frown_xbm_bits,
                    frown_xbm_width, frown_xbm_height);
}

void PaintFace(int sad)
{
    int x1, y1, w, h;
    Pixmap face;
    XRectangle displayRect;

    face = (sad ? frown : smile);
    x1 = win_width - smile_xbm_width - 4;
    y1 = win_height - smile_xbm_height - 6;
    w = smile_xbm_width;
    h = smile_xbm_height;

    displayRect.x = 0;
    displayRect.y = 0;
    displayRect.width = win_width;
    displayRect.height = win_height;
    XSetClipRectangles(display, disp_gc, 0, 0, &displayRect, 1, Unsorted);

    XSetForeground(display, disp_gc, strikeColor);
    XSetBackground(display, disp_gc, windowColor);

    XCopyPlane(display, face, win, disp_gc, 0, 0, w, h, x1, y1, 0x01);

    XSetForeground(display, disp_gc, textColor);
}

int AllocStandardColors(void)
{
    unsigned long r, g, b, i;
    XColor color;

    stdcmap[0] = BlackPixel(display, screen);
    stdcmap[127] = WhitePixel(display, screen);

    for (i = 1; i < 127; ++i)
    {
        color.red = (i & 0x3) * 65535/3;
        color.green = ((i >> 2) & 0x7) * 65535/7;
        color.blue = ((i >> 5) & 0x3) * 65535/3;

        if (XAllocColor(display, colormap, &color) == 0)
        {
            fprintf(stderr, "Can't allocate standard color palette\n");

            while (i > 1)
                XFreeColors(display, colormap, &(stdcmap[--i]), 1, 0);

            return 0;
        }

        stdcmap[i] = color.pixel;
    }

    return 1;
}

int AllocGreyScale(void)
{
    unsigned long g;
    XColor color;

    greymap[0] = BlackPixel(display, screen);
    greymap[15] = WhitePixel(display, screen);

    for (g = 1; g < 15; ++g)
    {
        color.red = color.green = color.blue = (255*256/15) * g;

        if (XAllocColor(display, colormap, &color) == 0)
        {
            fprintf(stderr, "Can't allocate standard grey palette\n");

            while (g > 1)
                XFreeColors(display, colormap, &(greymap[--g]), 1, 0);

            return 0;
        }
        greymap[g] = color.pixel;
    }

    return 1;
}

int InitImaging(int ColorStyle)
{
    imaging = MONO;

    greymap[0] = BlackPixel(display, screen);
    greymap[15] = WhitePixel(display, screen);

    if (ColorStyle == MONO || ColorStyle == COLOR888)
    {
        imaging = ColorStyle;
        return imaging;
    }

    if (AllocGreyScale())
    {
        imaging = GREY4;

        if (ColorStyle == GREY4)
            return imaging;

        if (AllocStandardColors())
            imaging = COLOR232;
    }

    return imaging;
}

void ReportVisuals(void)
{
    long visual_info_mask;
    int number_visuals, i;
    XVisualInfo *visual_array, visual_info_template;

    visual_info_template.screen = DefaultScreen(display);

    visual_info_mask = VisualClassMask | VisualScreenMask;

    printf("TrueColor:\n");

    visual_info_template.class = TrueColor;
    visual_array = XGetVisualInfo(display, visual_info_mask,
                        &visual_info_template,
                        &number_visuals);

    for (i = 0; i < number_visuals; ++i)
    {
        printf("  visual Id 0x%x\n", visual_array[i].visualid);
        printf("  depth = %d, bits per rgb = %d, size = %d\n", visual_array[i].depth,
                    visual_array[i].bits_per_rgb, visual_array[i].colormap_size);
        printf("   rgb masks %lx, %lx, %lx\n", visual_array[i].red_mask,
                    visual_array[i].green_mask, visual_array[i].blue_mask);
    }

    XFree((void *)visual_array);

    printf("DirectColor:\n");

    visual_info_template.class = DirectColor;
    visual_array = XGetVisualInfo(display, visual_info_mask,
                        &visual_info_template,
                        &number_visuals);

    for (i = 0; i < number_visuals; ++i)
    {
        printf("  visual Id 0x%x\n", visual_array[i].visualid);
        printf("  depth = %d, bits per rgb = %d, size = %d\n", visual_array[i].depth,
                    visual_array[i].bits_per_rgb, visual_array[i].colormap_size);
        printf("   rgb masks %lx, %lx, %lx\n", visual_array[i].red_mask,
                    visual_array[i].green_mask, visual_array[i].blue_mask);
    }

    XFree((void *)visual_array);

    printf("PseudoColor:\n");

    visual_info_template.class = PseudoColor;
    visual_array = XGetVisualInfo(display, visual_info_mask,
                        &visual_info_template,
                        &number_visuals);

    for (i = 0; i < number_visuals; ++i)
    {
        printf("  visual Id 0x%x\n", visual_array[i].visualid);
        printf("  depth = %d, bits per rgb = %d, size = %d\n", visual_array[i].depth,
                    visual_array[i].bits_per_rgb, visual_array[i].colormap_size);
        printf("   rgb masks %lx, %lx, %lx\n", visual_array[i].red_mask,
                    visual_array[i].green_mask, visual_array[i].blue_mask);
    }

    XFree((void *)visual_array);
}

void ReportStandardColorMaps(Atom which_map)
{
    XStandardColormap *std_colormaps;
    int i, number_colormaps;
    char *atom_name;

    if (XGetRGBColormaps(display, RootWindow(display, screen),
            &std_colormaps, &number_colormaps, which_map) != 0)
    {
        atom_name = XGetAtomName(display, which_map);
        printf("\nPrinting %d standard colormaps for %s\n",
                number_colormaps, atom_name);
        XFree(atom_name);

        for  (i = 0; i < number_colormaps; ++i)
        {
            printf("\tColormap: 0x%x\n", std_colormaps[i].colormap);
            printf("\tMax cells (rgb): %d, %d, %d\n",
                std_colormaps[i].red_max,
                std_colormaps[i].green_max,
                std_colormaps[i].blue_max);
            printf("\tMultipliers: %d, %d, %d\n",
                std_colormaps[i].red_mult,
                std_colormaps[i].green_mult,
                std_colormaps[i].blue_mult);
            printf("\tBase pixel: %d\n", std_colormaps[i].base_pixel);
            printf("\tVisual Id 0x%x, Kill Id 0x%x\n",
                std_colormaps[i].visualid,
                std_colormaps[i].killid);
        }

        XFree((void *)std_colormaps);
    }
}

/* create a textured background as paper */

unsigned char *CreateBackground(unsigned int width, unsigned int height, unsigned int depth)
{
    unsigned char *data, *p;
    int size, i, j, n, max, m1, m2, m3;
    unsigned long c, c1, c2, c3;  /* the colors */


    if (depth == 8)
    {
        GetColor(230, 218, 194, &c1);
        GetColor(220, 209, 186, &c2);
        GetColor(210, 199, 177, &c3);
        size = width * height;
    }
    else if (depth == 24)
        size = width * height * 4;
    else
        return NULL;

    p = data = (unsigned char *)malloc(size);

    if (data == NULL)
        return NULL;

    srand(0x6000);

    if (depth == 8)
    {
        for (i = 0; i < height; ++i)
            for (j = 0; j < width; ++j)
            {
                n = rand();

                if (n > 0x5000)
                    c = c1;
                else if (n > 0x3000)
                    c = c2;
                else
                    c = c3;

                *p++ = (c & 0xFF);
            }
    }
    else
    {
        for (i = 0; i < height; ++i)
            for (j = 0; j < width; ++j)
            {
                *p++ = '\0';
                n = rand();

                if (n > 0x5000)
                {
                    *p++ = 230;
                    *p++ = 218;
                    *p++ = 194;
                }
                else if (n > 0x3000)
                {
                    *p++ = 220;
                    *p++ = 209;
                    *p++ = 186;
                }
                else
                {
                    *p++ = 210;
                    *p++ = 199;
                    *p++ = 177;
                }
            }
    }

    return data;
}

#if 0  /* used to allow for nested comments */
/* XPM */
/********************************************************/
/**   (c) Copyright Hewlett-Packard Company, 1992.     **/
/********************************************************/
static char ** arizona.l.px  = {
/* width height ncolors cpp [x_hot y_hot] */
"28 38 13 1",
/* colors */
"   s iconColor2    m white c white",
".  s iconGray2     m white c #c8c8c8c8c8c8",
"X  s iconColor1    m black c black",
"o  s iconGray6     m black c #646464646464",
"O  s iconGray3     m white c #afafafafafaf",
"+  s iconColor3    m black c red",
"@  s iconColor8    m white c magenta",
"#  s iconGray4     m white c #969696969696",
"$  s iconGray5     m black c #7d7d7d7d7d7d",
"%  s iconColor6    m white c yellow",
"&  s iconGray1     m white c #e1e1e1e1e1e1",
"*  s iconColor4    m black c green",
"=  s bottomShadowColor     m black c #646464646464",
/* pixels */
"                            ",
" ..........................X",
" ..............oo..........X",
" .........OOOoo+@@oooOOOOOOX",
" .....OOOoooo@####@+ooooo..X",
" ..ooo#oo+@###$$$$#####OOO.X",
" ..OOOOOO###$$....$$#@+ooo.X",
" .......+@#$.%%%%%%.$###OOOX",
" ..o.ooo..$.%%%%%%%%%$#@+ooX",
and so on, ending with:
" XXXXXXXXXXXXXXXXXXXXXXXXXXX"};
#endif

static void SkipToChar(FILE *fp, int ch)
{
    int c;

    while ((c = getc(fp)) != ch && c != EOF);
}

/* *c to first char and return last word */

char *ReadColor(FILE *fp, int *ch)
{
    char *p;
    int c;
    static char line[256];

    SkipToChar(fp, '"');
    *ch = (unsigned char)getc(fp);
    p = line;

    while ((c = getc(fp)) != '"' && c != EOF)
        *p++ = c;

    *p = '\0';
    p = strrchr(line, ' ');

    if (p)
        ++p;

    return p;
}

/* load data from an XPM file and allocate colors */

char *LoadXpmImage(Image *image, unsigned int depth)
{
    int c, i, j, cr, cg, cb, r, g, b, ncolors, size, map[256];
    unsigned int width, height;
    unsigned long pixel, *pixdata;
    unsigned char *data, *p;
    char *name;
    Color *colors, color;
    FILE *fp;
    XColor xcolor;

    if ((fp = fopen(image->url, "r")) == NULL)
    {
        printf("Can't load image: %s", image->url);
        return NULL;
    }

    SkipToChar(fp, '"');
    fscanf(fp, "%d %d %d", &width, &height, &ncolors);
    SkipToChar(fp, '\n');

    size = width * height;
    image->width = width;
    image->height = height;

    if (size == 0 || ncolors == 0)
        return NULL;

    if (depth != 8 && depth != 24)
    {
        printf("Display depth %d unsupported", depth);
        fclose(fp);
        return NULL;
    }

    image->npixels = 0;
    image->pixels = 0; /*(unsigned long *)malloc(ncolors * sizeof(unsigned long)); */

    colors = (Color *)malloc(ncolors * sizeof(Color));

    for (i = 0; i < ncolors; ++i)
    {
        name = ReadColor(fp, &c);

        if (XParseColor(display, colormap, name, &xcolor) == 0)
        {
            map[c] = -1;
            continue;
        }

        map[c] = i;
        r = xcolor.red >> 8;
        g = xcolor.green >> 8;
        b = xcolor.blue >> 8;

        colors[i].red = r;
        colors[i].green = g;
        colors[i].blue = b;
        colors[i].grey = (3*r + 6*g + b)/10;
    }


    if (depth == 8)
    {
        p = data = malloc(size);

        for (i = 0; i < height; ++i)
        {
            SkipToChar(fp, '"');

            for (j = 0; j < width; ++j)
            {
                if ((c = getc(fp)) < 0)
                    c = 0;

                c = map[c];

                if (c < 0)
                {
                    *p++ = transparent;
                    continue;
                }

                color = colors[c];
                c = ((i % 16) << 4) + (j % 16);

                if (imaging == COLOR232)
                {
                    cr = color.red;
                    cg = color.green;
                    cb = color.blue;

                    if (cr == cg  && cg == cb)
                    {
                        cg  = color.grey;
                        g = cg & 0xF0;

                        if (cg - g > Magic16[c])
                            g += 16;

                        g = min(g, 0xF0);
                        *p++ = greymap[g >> 4];
                    }
                    else
                    {
                        r = cr & 0xC0;
                        g = cg & 0xE0;
                        b = cb & 0xC0;

                        if (cr - r > Magic64[c])
                            r += 64;

                        if (cg - g > Magic32[c])
                            g += 32;

                        if (cb - b > Magic64[c])
                            b += 64;

                        r = min(r, 255) & 0xC0;
                        g = min(g, 255) & 0xE0;
                        b = min(b, 255) & 0xC0;

                        *p++ = stdcmap[(r >> 6) | (g >> 3) | (b >> 1)];
                    }
                }
                else if (imaging == GREY4)
                {
                    cg  = color.grey;
                    g = cg & 0xF0;

                    if (cg - g > Magic16[c])
                        g += 16;

                    g = min(g, 0xF0);
                    *p++ = greymap[g >> 4];
                }
                else /* MONO */
                {
                    if (color.grey < Magic256[c])
                        *p++ = greymap[0];
                    else
                        *p++ = greymap[15];
                }
            }

            SkipToChar(fp, '\n');
        }
    }
    else  /* depth == 24 */
    {
        p = data = malloc(size * 4);

        for (i = 0; i < height; ++i)
        {
            SkipToChar(fp, '"');

            for (j = 0; j < width; ++j)
            {
                if ((c = map[getc(fp)]) < 0)
                {
                    *p++ = '\0';
                    *p++ = (windowColor >> 16) & 0xFF;
                    *p++ = (windowColor >>  8) & 0xFF;
                    *p++ = windowColor & 0xFF;
                    continue;
                };

                color = colors[c];
                *p++ = '\0';
                *p++ = color.red;
                *p++ = color.green;
                *p++ = color.blue;
            }

            SkipToChar(fp, '\n');
        }
    }

    fclose(fp);

    free(colors);
    return (char *)data;
}

char *ReadLine(FILE *fp)
{
    int i, c;
    static char buf[256];

    i = 0;

    while (i < 255)
    {
        c = getc(fp);

        if (c =='\n')
            break;

        buf[i++] = c;
    }

    buf[i] = '\0';
    return buf;
}

/* load data from an XBM file */

Pixmap LoadXbmImage(Image *image, unsigned int depth)
{
    int c, i, size;
    unsigned int width, height;
    unsigned char *data, *p;
    char *r, *s;
    char buf[256];
    FILE *fp;
    Pixmap pixmap;

    if ((fp = fopen(image->url, "r")) == NULL)
    {
        printf("Can't load image: %s", image->url);
        return NULL;
    }

    s = ReadLine(fp);

    if ((r = strrchr(s, ' ')) == NULL)
    {
        printf("Can't load image: %s", image->url);
        return NULL;
    }

    sscanf(r, "%d", &width);

    s = ReadLine(fp);

    if ((r = strrchr(s, ' ')) == NULL)
    {
        printf("Can't load image: %s", image->url);
        return NULL;
    }

    sscanf(r, "%d", &height);

    SkipToChar(fp, '\n');

    size = width * height;
    image->width = width;
    image->height = height;

    if (size == 0)
        return NULL;

    if (depth == 24)
        size = size << 2;
    else if (depth != 8)
    {
        printf("Display depth %d unsupported", depth);
        fclose(fp);
        return NULL;
    }

    image->npixels = 0;
    image->pixels = 0;

    data = malloc(size);
    ReadLine(fp);

    for (i = 0; i < size; ++i)
    {
        fscanf(fp, "%d%c", data+i);
    }

    fclose(fp);

    pixmap = XCreatePixmapFromBitmapData(display, win, (char *)data,
                width, height, windowShadow, windowColor, depth);

    free(data);
    return pixmap;
}

Image *DefaultImage(Image *image)
{
    if (image->npixels > 0)
    {
        XFreeColors(display, colormap, image->pixels, image->npixels, 0);
        free(image->pixels);
        image->pixels = NULL;
        image->npixels = 0;
    }

    image->pixmap = default_pixmap;
    image->width = default_pixmap_width;
    image->height = default_pixmap_height;
    image->next = images;
    images = image;

    return image;
}

Image *GetImage(char *href, int hreflen)
{
    XGCValues values;
    unsigned int width, height;
    Pixmap pixmap;
    XImage *ximage;
    GC drawGC;
    char *data;
    Image *image;
    Block block;
    int tag;

    /* check if designated image is already loaded */

    for (image = images; image != NULL; image = image->next)
    {
        if (strlen(image->url) == hreflen && strncmp(href, image->url, hreflen) == 0)
            return image;
    }

    image = (Image *)malloc(sizeof(Image));
    image->url = (char *)malloc(hreflen+1);
    memcpy(image->url, href, hreflen);
    image->url[hreflen] = '\0';
    image->npixels = 0;

    /* otherwise we need to load image from cache or remote server */

    block.buffer = GetDocument(image->url, NULL, REMOTE);

    if (block.buffer == NULL)
    {
        Warn("Failed to load image data: %s", image->url);
        return DefaultImage(image);
    }

    block.next = NewDoc.hdrlen;
    block.size = NewDoc.length;
    NewDoc.buffer = NULL;
    FreeDoc(&NewDoc);

    Announce("Processing image %s...", image->url);

    if (strncasecmp(image->url + hreflen - 4, ".gif", 4) == 0)
    {
        if ((data = (char *)LoadGifImage(image, &block, depth)) == NULL)
        {
            Warn("Failed to load GIF image: %s", image->url);
            free(block.buffer);
            return DefaultImage(image);
        }
    }
    else if ((data = LoadXpmImage(image, depth)) == NULL)
    {
        Warn("Failed to load XPM image: %s", image->url);
        free(block.buffer);
        return DefaultImage(image);
    }

    free(block.buffer);
    width = image->width;
    height = image->height;

    if ((ximage = XCreateImage(display, DefaultVisual(display, screen),
             depth, ZPixmap, 0, data,
             width, height, (depth == 24 ? 32 : 8), 0)) == 0)
    {
        Warn("Failed to create XImage: %s", image->url);
        free(data);
        return DefaultImage(image);
    }

    if ((pixmap = XCreatePixmap(display, win, width, height, depth)) == 0)
    {
        Warn("Failed to create Pixmap: %s", image->url);
        XDestroyImage(ximage); /* also free's image data */
        return DefaultImage(image);
    }

    drawGC = XCreateGC(display, pixmap, 0, 0);
    XSetFunction(display, drawGC, GXcopy);
    XPutImage(display, pixmap, drawGC, ximage, 0, 0, 0, 0, width, height);
    XFreeGC(display, drawGC);
    XDestroyImage(ximage);  /* also free's image data */

    image->pixmap = pixmap;
    image->width = width;
    image->height = height;
    image->next = images;
    images = image;

    if (!IsIndex)
        Announce(CurrentDoc.url);

    return image;
}

void FreeImages(int cloned)
{
    Image *im;

    while (images)
    {
        /* deallocate colors */

        if (!cloned && images->npixels > 0)
            XFreeColors(display, colormap, images->pixels, images->npixels, 0);

        /* free pixmap and image structure */

        if (!cloned && images->pixmap != default_pixmap)
            XFreePixmap(display, images->pixmap);

        im = images;
        images = im->next;
        free(im->url);

        if (im->npixels > 0)
            free(im->pixels);

        free(im);
    }
}
