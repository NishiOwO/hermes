#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <ctype.h>

/* assumes X11/HPkeysym.h */

#include "www.h"
#include "www.bm"

#ifndef XK_DeleteChar
#define XK_DeleteChar XK_Delete
#endif

#define BITMAPDEPTH 1

/* values for window_size in main, is window big enough to be useful */

#define SMALL 1
#define OK 0

void exit(int);

extern int statusHeight;
extern int sbar_width;
extern int ToolBarHeight;
extern long PixelOffset;
extern long buf_height;
extern int PixelIndent;
extern int buf_width;
extern char *buffer;
extern int hdrlen;
extern int Authorize;
extern int OpenURL;
extern int IsIndex;
extern int SaveFile;
extern int FindStr;
extern char *FindStrVal, *FindNextStr;
extern Doc CurrentDoc, NewDoc;
extern int font;
extern Field *focus;
extern Frame background;
extern Byte *paint;

int debug = 0;        /* used to control reporting of errors */
int initialised = 0;  /* avoid X output until this is true! */
int busy = 0;         /* blocks hypertext links etc while receiving data */
int UseHTTP2 = 1;     /* use new HTRQ/HTTP protocol */
int OpenSubnet = 0;   /* if true host is on OpenSubnet */
int UsePaper = 1; 
int fontsize = 0;

/* Display and screen are used as arguments to nearly every Xlib
 * routine, so it simplifies routine calls to declare them global.
 * If there were additional source files, these variables would be
 * declared `extern' in them.
 */

Display *display;
Visual *visual;
Colormap colormap;
int screen;
int depth;
Window win;
int ExposeCount;   /* used to monitor GraphicsExpose events during scrolling */
char *prog;        /* name of invoking program */
int default_pixmap_width, default_pixmap_height;
Pixmap background_pixmap, default_pixmap;

Cursor hourglass;
int shape;

XFontStruct *h1_font, *h2_font, *h3_font, *h4_font, *normal_font, *italic_font,
         *bold_font, *bold_i_font, *fixed_i_font, *fixed_b_font,
         *fixed_bi_font, *fixed_font, *legend_font, *symbol_font;

unsigned long textColor, labelColor, windowColor, strikeColor, statusColor,
       transparent, windowTopShadow, windowBottomShadow, windowShadow;

int charWidth, charHeight, charAscent, lineHeight;
unsigned int win_width, win_height, tileWidth, tileHeight;

int document;
int gadget;
int gatewayport = 3000; /* GATEWAYPORT; */
char *gateway;      /* gateway if can't access server directly */
char *help;
char *printer;
char *startwith;    /* default initial document */
char *user;

int RepeatButtonDown = 0;

XFontStruct *Fonts[FONTS];  /* array of fonts */
int LineSpacing[FONTS];
int BaseLine[FONTS];
int StrikeLine[FONTS];

int ListIndent1, ListIndent2;

GC gc_fill;
static GC gc_scrollbar, gc_status, gc_text;
static char *display_name = NULL;
static int window_size = 0;    /* OK or SMALL to display contents */
static int button_x, button_y;
static XSizeHints size_hints;
static ColorStyle;

/* find best visual for specified visual class */
Visual *BestVisual(int class, int *depth)
{
    long visual_info_mask;
    int number_visuals, i, best_depth;
    XVisualInfo *visual_array, visual_info_template;
    Visual *best_visual;

    visual_info_template.class = class;
    visual_info_template.screen = DefaultScreen(display);
    visual_info_mask = VisualClassMask | VisualScreenMask;

    visual_array = XGetVisualInfo(display, visual_info_mask,
                        &visual_info_template,
                        &number_visuals);

    best_depth = *depth;
    *depth = 0;
    best_visual = 0;

    for (i = 0; i < number_visuals; ++i)
    {
        if (visual_array[i].depth > *depth)
        {
            best_visual = visual_array[i].visual;
            *depth = visual_array[i].depth;
        }

        if (*depth == best_depth)
            break;
    }

    XFree((void *)visual_array);
    return best_visual;
}

/* send myself an expose event to force redrawing of give rectangle */

void Redraw(int x, int y, int w, int h)
{
    XExposeEvent event;

    event.type = Expose;
    event.serial = 0;
    event.send_event = 1;
    event.display = display;
    event.window = win;
    event.x = x;
    event.y = y;
    event.width = w;
    event.height = h;
    event.count = 0;

    XSendEvent(display, win, 0, ExposureMask, (XEvent *)&event);
}

void SetBanner(char *title)
{
    char *p;
    XTextProperty textprop;

    XStoreName(display, win, title);

    if (strlen(title) > 8 && (p = strrchr(title, '/')))
    {
            if (p[1] != '\0')
                ++p;

            title = p;
    }

    textprop.value = (unsigned char *)title;
    textprop.encoding = XA_STRING;
    textprop.format = 8;
    textprop.nitems = strlen(title);

    XSetWMIconName(display, win, &textprop);
}

void LoadFont(XFontStruct **font_info, char *font_name, char *fall_back)
{
    int direction_hint, font_ascent, font_descent;
    XCharStruct overall;
    char *test = "Testing";

    if ((*font_info = XLoadQueryFont(display, font_name)) == NULL)
    {
        (void) fprintf(stderr, "Cannot open %s font\n", font_name);

        if ((*font_info = XLoadQueryFont(display, fall_back)) == NULL)
        {
            (void) fprintf(stderr, "Cannot open alternate font: %s\n", fall_back);
            exit(1);
        }

        fprintf(stderr, "Using alternate font: %s\n", fall_back);
    }

    XTextExtents(*font_info, test, strlen(test),
                    &direction_hint,
                    &font_ascent,
                    &font_descent,
                    &overall);

    charWidth = overall.width/strlen(test);
    charHeight = overall.ascent + overall.descent;
    charAscent = overall.ascent;
    lineHeight = charHeight + overall.ascent/4 + 2;
}

static int hex(char *s)
{
    int n, c;

    n = toupper(*(unsigned char *)*s++) - '0';

    if (n > 9)
        n += '0' - 'A' + 10;
    
    c = toupper(*(unsigned char *)*s) - '0';

    if (c > 9)
        c += '0' - 'A' + 10;

    return c + (n << 4);
}

int Pixel4Color(XColor *color, unsigned long *pixel)
{
    unsigned long r, g, b;

    r = color->red >> 8;
    g = color->green >> 8;
    b = color->blue >> 8;

    *pixel = (b | g << 8 | r << 16);
    return 1;
}

int GetNamedColor(char *name, unsigned long *pix)
{
    XColor color;

    if (depth == 24)
    {
        if (XParseColor(display, colormap, name, &color) == 0)
        {
            if (debug)
                fprintf(stderr, "www: can't allocate named color `%s'\n", name);

            return 0;
        }

        return Pixel4Color(&color, pix);
    }
    else if (XParseColor(display, colormap, name, &color) == 0 ||
                     XAllocColor(display, colormap, &color) == 0)
    {
        if (debug)
            fprintf(stderr, "www: can't allocate named color `%s'\n", name);

        return 0;
    }

    *pix = color.pixel;
    return 1;
}

int GetColor(int red, int green, int blue, unsigned long *pix)
{
    XColor color;
/*
    if (red == 255 && green == 255 && blue == 255)
    {
        *pix = WhitePixel(display, screen);
        return 1;
    }

    if (red == 0 && green == 0 && blue == 0)
    {
        *pix = BlackPixel(display, screen);
        return 1;
    }
*/
    color.red = red << 8;
    color.green = green << 8;
    color.blue = blue  << 8;

    if (depth == 24)
        return Pixel4Color(&color, pix);
    else if (XAllocColor(display, colormap, &color) == 0)
    {
        if (debug)
            fprintf(stderr, "www: can't allocate color %d:%d:%d\n", red, green, blue);

        return 0;
    }

    *pix = color.pixel;
    return 1;
}

char *DetermineValue(char *prog, char *param, char *def)
{
    char *value;

    if ((value = XGetDefault(display, prog, param)) == NULL)
        value = def;

    return value;
}

int DetermineColor(char *prog, char *param,
        int r, int g, int b,
        unsigned long *pix)
{
    char *colorname, *p1, *p2;

    colorname = XGetDefault(display, prog, param);

    if (colorname)  /* parse and GetColor() or GetNamedColor() */
    {
        /* "red:green:blue" e.g."205:184:157" or named color */

        p1 = strchr(colorname, ':');
        p2 = strrchr(colorname, ':');

        if (p1 && p2 && p1 != p2)
        {
            sscanf(colorname, "%d:", &r);
            sscanf(p1+1, "%d:", &g);
            sscanf(p2+1, "%d", &b);

            return GetColor(r, g, b, pix);
        }

        return GetNamedColor(colorname, pix);
    }

    return GetColor(r, g, b, pix);
}

void ButtonDown(unsigned int button, unsigned int state, int x, int y)
{
    if (y < WinTop)
        gadget = ToolBarButtonDown(x, y);
    else if (y >= win_height - statusHeight)
        gadget = StatusButtonDown(button, x, y);
    else if (x < WinRight && y < WinBottom)
        gadget = WindowButtonDown(x, y);
    else if (x >= WinRight && y < WinBottom ||
             x <= WinRight && y >= WinBottom)
    {
        button_x = x;
        button_y = y;
        gadget = ScrollButtonDown(x, y);
    }
    else
        gadget = VOID;
}

void ButtonUp(unsigned int button, unsigned int state, int x, int y)
{
    int shifted;

    RepeatButtonDown = 0;
    shifted = ShiftMask & state;

    if (gadget == WINDOW)
        WindowButtonUp(shifted, x, y);
    else if (gadget == SCROLLBAR)
        ScrollButtonUp(x, y);
    else if (gadget == TOOLBAR)
        ToolBarButtonUp(x, y);
    else if (gadget == STATUS)
        StatusButtonUp(x, y);
}

void BackDoc()
{
    long where;
    int tag;
    char * q;

    XDefineCursor(display, win, hourglass);
    XFlush(display);
    FindNextStr = 0;

    q = PopDoc(&where);

    if (q && *q)
    {
        /* note current status string */

        SaveStatusString();

        /* quit Open, SaveAs or Find if active */

        OpenURL = SaveFile = FindStr = 0;

        SetBanner("World Wide Web Browser");

        SetCurrent();        /* set current = new scheme etc. */
        NewBuffer(q);        /* sets buffer, document, hdrlen, etc */

        if (document == HTMLDOCUMENT)
        {
            DeltaHTMLPosition(where);
            SetScrollBarVPosition(where, buf_height);
            SetScrollBarHPosition(0, buf_width);
        }

        RestoreStatusString();
        DisplayDoc(WinLeft, WinTop, WinWidth, WinHeight);
        Announce(CurrentDoc.url);
        DisplayScrollBar();
    }
    else
        XBell(display, 0);

    XUndefineCursor(display, win);
    XFlush(display);
}


void PollEvents(int block)
{
    char keybuf[20];
    int keybufsize = 20;
    int count;
    KeySym key;
    XEvent event;
    XComposeStatus cs;

    busy = !block;

    while (block || XEventsQueued(display, QueuedAfterReading) != 0)
    {
        if (RepeatButtonDown && !ExposeCount &&
                XEventsQueued(display, QueuedAfterReading) == 0)
        {
            XFlush(display);
            ButtonDown(event.xbutton.button, event.xbutton.state, button_x, button_y);
            Pause(10);
            continue;
        }

        XNextEvent(display, &event);

        switch(event.type)
        {
            case Expose:
                /* get rid of all other Expose events on the queue */
                while (XCheckTypedEvent(display, Expose, &event));

                SetToolBarWin(win);
                SetToolBarGC(gc_status);
                DisplayToolBar();

                SetStatusWin(win);
                SetStatusGC(gc_status);
                DisplayStatusBar();

                SetScrollBarWin(win);
                SetScrollBarGC(gc_scrollbar);
                DisplayScrollBar();

                SetDisplayGC(gc_text);
                DisplayDoc(WinLeft, WinTop, WinWidth, WinHeight);
                break;

            case GraphicsExpose:
                SetToolBarWin(win);
                SetToolBarGC(gc_status);
                DisplayToolBar();

                SetStatusWin(win);
                SetStatusGC(gc_status);
                DisplayStatusBar();

                SetScrollBarWin(win);
                SetScrollBarGC(gc_scrollbar);
                DisplayScrollBar();

                SetDisplayGC(gc_text);
                DisplayDoc(event.xgraphicsexpose.x,
                            event.xgraphicsexpose.y,
                            event.xgraphicsexpose.width,
                            event.xgraphicsexpose.height);

                ExposeCount = event.xgraphicsexpose.count;
                break;

            case NoExpose:
                ExposeCount = 0;
                break;

            case ConfigureNotify:
                if (win_width == event.xconfigure.width &&
                        win_height == event.xconfigure.height)
                    break;

                win_width = event.xconfigure.width;
                win_height = event.xconfigure.height;

                if ((win_width < size_hints.min_width) ||
                        (win_height < size_hints.min_height))
                    window_size = SMALL;
                else
                    window_size = OK;

                if (focus && focus->flags & CHECKED)
                {
                    focus->flags &= ~CHECKED;
                    focus = NULL;
                }

                DisplaySizeChanged(1);
                break;

            case KeyPress:
                count = XLookupString((XKeyEvent*)&event, keybuf, keybufsize, &key, &cs);
                keybuf[count] = 0;

                if (key == XK_F8)
                {
                    XCloseDisplay(display);
                    exit(1);
                }
                else if (key == XK_F4)
                    ShowPaint(background.top-paint, 14);
                else if (key == XK_F6)
                    ReportVisuals();
                else if (key == XK_F7)
                {
                    ReportStandardColorMaps(XA_RGB_DEFAULT_MAP);
                    ReportStandardColorMaps(XA_RGB_BEST_MAP);
                }
                else if ((Authorize|OpenURL|IsIndex|SaveFile|FindStr) && IsEditChar(*keybuf))
                    EditChar(*keybuf);
                else if ((Authorize|OpenURL|IsIndex|SaveFile|FindStr) && (key == XK_Left || key == XK_Right))
                    MoveStatusCursor(key);
                else if ((Authorize|OpenURL|IsIndex|SaveFile|FindStr) && (key == XK_Delete || key == XK_DeleteChar))
                    EditChar(127);
#ifdef _HPUX_SOURCE
                else if ((Authorize|OpenURL|IsIndex|SaveFile|FindStr) && (key == XK_ClearLine || key == XK_DeleteLine))
                {
                    ClearStatus();
                    SetStatusString(0); /* force refresh */
                }
#endif
                else if (key == XK_Up && !ExposeCount)
                {
                    MoveUpLine();
                    XFlush(display);   /* to avoid scrollbar flashing */
                }
                else if (key == XK_Down && !ExposeCount)
                {
                    MoveDownLine();
                    XFlush(display);   /* to avoid scrollbar flashing */
                }
                else if (key == XK_Left && !ExposeCount)
                {
                    MoveLeftLine();
                    XFlush(display);   /* to avoid scrollbar flashing */
                }
                else if (key == XK_Right && !ExposeCount)
                {
                    MoveRightLine();
                    XFlush(display);   /* to avoid scrollbar flashing */
                }
                else if (key == XK_Prior && !ExposeCount)
                    MoveUpPage();
                else if (key == XK_Next && !ExposeCount)
                    MoveDownPage();
                else if ((key == XK_Begin || key == XK_Home) && !ExposeCount)
                {
                    if (event.xkey.state & ShiftMask)
                        MoveToEnd();
                    else
                        MoveToStart();
                }
                else if (key == XK_End && !ExposeCount)
                    MoveToEnd();
                else if (key == XK_Escape)
                {
                    if (Authorize)
                        HideAuthorizeWidget();
                    else if (busy)
                        Beep();
                    else if (OpenURL)
                    {
                        OpenURL = 0;
                        SetStatusString("");
                    }
                    else if (SaveFile)
                    {
                        SaveFile = 0;
                        SetStatusString("");
                    }
                    else if (FindStr)
                    {
                        FindStr = 0;
                        SetStatusString("");
                    }
                }
                else if (busy)
                    Beep();
                else if (key == XK_F2)
                    BackDoc();
                else if (key == XK_F3 && FindStrVal)
                    FindString(FindStrVal, &FindNextStr);
                break;

            case KeyRelease:
                count = XLookupString((XKeyEvent*)&event, keybuf, keybufsize, &key, &cs);
                keybuf[count] = 0;
                break;

            case ButtonRelease:
                ButtonUp(event.xbutton.button, event.xbutton.state, event.xbutton.x, event.xbutton.y);
                break;

            case ButtonPress:
                ButtonDown(event.xbutton.button, event.xbutton.state, event.xbutton.x, event.xbutton.y);
                break;

            case MotionNotify:  /* only sent when Button1 is down! */
              /* ignore event if still repairing holes from earlier copy operation*/
                if (ExposeCount)
                    break;

                /* get rid of all other MotionNotify events on the queue */
                while (XCheckTypedEvent(display, MotionNotify, &event));

                ScrollButtonDrag(event.xbutton.x, event.xbutton.y);
                break;

           /* the following is a failed attempt to exit tidily when the user
                forces an exit through the system menu - I need to do better ! */

            case DestroyNotify:
                fprintf(stderr, "DestroyNotify message\n");
                /* XCloseDisplay(display); */
                exit(1);

            case MapNotify:
                initialised = 1;   /* allow status display now its safe */
                break;

            case UnmapNotify:
                fprintf(stderr, "UnmapNotify\n");
                break;

            case ReparentNotify:
                break;

            default:  /* handle all other events */
                fprintf(stderr, "Unexpected Event: %ld\n", event.type);
                break;
        }
    }

    busy = 0;
}

/* get font/colour resources */

void GetResources(void)
{
    if (fontsize == 0)
    {
        LoadFont(&h1_font, DetermineValue(prog, "h1font", H1FONT), "vg-20");
        LoadFont(&h2_font, DetermineValue(prog, "h2font", H2FONT), "fg-16");
        LoadFont(&h3_font, DetermineValue(prog, "h3font", H3FONT), "variable");
        LoadFont(&h4_font, DetermineValue(prog, "h4font", H4FONT), "variable");
        LoadFont(&legend_font, DetermineValue(prog, "labelfont", LABELFONT), "fixed");
        LoadFont(&normal_font, DetermineValue(prog, "normalfont", NORMALFONT), "fixed");
        LoadFont(&italic_font, DetermineValue(prog, "italicfont", ITALICFONT), "fixed");
        LoadFont(&bold_font, DetermineValue(prog, "boldfont", BOLDFONT), "fixed");
        LoadFont(&bold_i_font, DetermineValue(prog, "bolditalicfont", BINORMFONT), "fixed");
        LoadFont(&fixed_i_font, DetermineValue(prog, "ifixedfont", IFIXEDFONT), "fixed");
        LoadFont(&fixed_b_font, DetermineValue(prog, "bfixedfont", BFIXEDFONT), "fixed");
        LoadFont(&fixed_bi_font, DetermineValue(prog, "bifixedfont", BIFIXEDFONT), "fixed");
        LoadFont(&fixed_font, DetermineValue(prog, "fixedfont", RFIXEDFONT), "fixed");
        LoadFont(&symbol_font, DetermineValue(prog, "symbolfont", SYMFONT), "sym-s25");
    }
    else if (fontsize == 1)
    {
        LoadFont(&h1_font, DetermineValue(prog, "h1font", H1FONTL), "vg-20");
        LoadFont(&h2_font, DetermineValue(prog, "h2font", H2FONTL), "fg-16");
        LoadFont(&h3_font, DetermineValue(prog, "h3font", H3FONTL), "variable");
        LoadFont(&h4_font, DetermineValue(prog, "h4font", H4FONTL), "variable");
        LoadFont(&legend_font, DetermineValue(prog, "labelfont", LABELFONT), "fixed");
        LoadFont(&normal_font, DetermineValue(prog, "normalfont", NORMALFONTL), "fixed");
        LoadFont(&italic_font, DetermineValue(prog, "italicfont", ITALICFONTL), "fixed");
        LoadFont(&bold_font, DetermineValue(prog, "boldfont", BOLDFONTL), "fixed");
        LoadFont(&bold_i_font, DetermineValue(prog, "bolditalicfont", BINORMFONTL), "fixed");
        LoadFont(&fixed_i_font, DetermineValue(prog, "ifixedfont", IFIXEDFONTL), "fixed");
        LoadFont(&fixed_b_font, DetermineValue(prog, "bfixedfont", BFIXEDFONTL), "fixed");
        LoadFont(&fixed_bi_font, DetermineValue(prog, "bifixedfont", BIFIXEDFONTL), "fixed");
        LoadFont(&fixed_font, DetermineValue(prog, "fixedfont", RFIXEDFONTL), "fixed");
        LoadFont(&symbol_font, DetermineValue(prog, "symbolfont", SYMFONTL), "sym-s25");
    }
    else if (fontsize == 2)
    {
        LoadFont(&h1_font, DetermineValue(prog, "h1font", H1FONTG), "vg-20");
        LoadFont(&h2_font, DetermineValue(prog, "h2font", H2FONTG), "fg-16");
        LoadFont(&h3_font, DetermineValue(prog, "h3font", H3FONTG), "variable");
        LoadFont(&h4_font, DetermineValue(prog, "h4font", H4FONTG), "variable");
        LoadFont(&legend_font, DetermineValue(prog, "labelfont", LABELFONT), "fixed");
        LoadFont(&normal_font, DetermineValue(prog, "normalfont", NORMALFONTG), "fixed");
        LoadFont(&italic_font, DetermineValue(prog, "italicfont", ITALICFONTG), "fixed");
        LoadFont(&bold_font, DetermineValue(prog, "boldfont", BOLDFONTG), "fixed");
        LoadFont(&bold_i_font, DetermineValue(prog, "bolditalicfont", BINORMFONTG), "fixed");
        LoadFont(&fixed_i_font, DetermineValue(prog, "ifixedfont", IFIXEDFONTG), "fixed");
        LoadFont(&fixed_b_font, DetermineValue(prog, "bfixedfont", BFIXEDFONTG), "fixed");
        LoadFont(&fixed_bi_font, DetermineValue(prog, "bifixedfont", BIFIXEDFONTG), "fixed");
        LoadFont(&fixed_font, DetermineValue(prog, "fixedfont", RFIXEDFONTG), "fixed");
        LoadFont(&symbol_font, DetermineValue(prog, "symbolfont", SYMFONTG), "sym-s25");
    }

    Fonts[IDX_H1FONT] = h1_font;
    Fonts[IDX_H2FONT] = h2_font;
    Fonts[IDX_H3FONT] = h3_font;
    Fonts[IDX_H4FONT] = h4_font;
    Fonts[IDX_LABELFONT] = legend_font;
    Fonts[IDX_NORMALFONT] = normal_font;
    Fonts[IDX_INORMALFONT] = italic_font;
    Fonts[IDX_BNORMALFONT] = bold_font;
    Fonts[IDX_BINORMALFONT] = bold_i_font;
    Fonts[IDX_TTNORMALFONT] = fixed_font;
    Fonts[IDX_FIXEDFONT] = fixed_font;
    Fonts[IDX_IFIXEDFONT] = fixed_i_font;
    Fonts[IDX_BFIXEDFONT] = fixed_b_font;
    Fonts[IDX_BIFIXEDFONT] = fixed_bi_font;
    Fonts[IDX_SYMBOLFONT] = symbol_font;

    LineSpacing[IDX_H1FONT] = SPACING(h1_font);
    LineSpacing[IDX_H2FONT] = SPACING(h2_font);
    LineSpacing[IDX_H3FONT] = SPACING(h3_font);
    LineSpacing[IDX_H4FONT] = SPACING(h4_font);
    LineSpacing[IDX_LABELFONT] = SPACING(legend_font);
    LineSpacing[IDX_NORMALFONT] = SPACING(normal_font);
    LineSpacing[IDX_INORMALFONT] = SPACING(normal_font);
    LineSpacing[IDX_BNORMALFONT] = SPACING(normal_font);
    LineSpacing[IDX_BINORMALFONT] = SPACING(normal_font);
    LineSpacing[IDX_TTNORMALFONT] = SPACING(normal_font);
    LineSpacing[IDX_FIXEDFONT] = SPACING(fixed_font);
    LineSpacing[IDX_IFIXEDFONT] = SPACING(fixed_font);
    LineSpacing[IDX_BFIXEDFONT] = SPACING(fixed_font);
    LineSpacing[IDX_BIFIXEDFONT] = SPACING(fixed_font);
    LineSpacing[IDX_SYMBOLFONT] = SPACING(normal_font);

    BaseLine[IDX_H1FONT] = BASELINE(h1_font);
    BaseLine[IDX_H2FONT] = BASELINE(h2_font);
    BaseLine[IDX_H3FONT] = BASELINE(h3_font);
    BaseLine[IDX_H4FONT] = BASELINE(h4_font);
    BaseLine[IDX_LABELFONT] = BASELINE(legend_font);
    BaseLine[IDX_NORMALFONT] = BASELINE(normal_font);
    BaseLine[IDX_INORMALFONT] = BASELINE(normal_font);
    BaseLine[IDX_BNORMALFONT] = BASELINE(normal_font);
    BaseLine[IDX_BINORMALFONT] = BASELINE(normal_font);
    BaseLine[IDX_TTNORMALFONT] = BASELINE(normal_font);
    BaseLine[IDX_FIXEDFONT] = BASELINE(fixed_font);
    BaseLine[IDX_IFIXEDFONT] = BASELINE(fixed_font);
    BaseLine[IDX_BFIXEDFONT] = BASELINE(fixed_font);
    BaseLine[IDX_BIFIXEDFONT] = BASELINE(fixed_font);
    BaseLine[IDX_SYMBOLFONT] = BASELINE(normal_font);

    StrikeLine[IDX_H1FONT] = STRIKELINE(h1_font);
    StrikeLine[IDX_H2FONT] = STRIKELINE(h2_font);
    StrikeLine[IDX_H3FONT] = STRIKELINE(h3_font);
    StrikeLine[IDX_H4FONT] = STRIKELINE(h4_font);
    StrikeLine[IDX_LABELFONT] = STRIKELINE(legend_font);
    StrikeLine[IDX_NORMALFONT] = STRIKELINE(normal_font);
    StrikeLine[IDX_INORMALFONT] = STRIKELINE(normal_font);
    StrikeLine[IDX_BNORMALFONT] = STRIKELINE(normal_font);
    StrikeLine[IDX_BINORMALFONT] = STRIKELINE(normal_font);
    StrikeLine[IDX_TTNORMALFONT] = STRIKELINE(normal_font);
    StrikeLine[IDX_FIXEDFONT] = STRIKELINE(fixed_font);
    StrikeLine[IDX_IFIXEDFONT] = STRIKELINE(fixed_font);
    StrikeLine[IDX_BFIXEDFONT] = STRIKELINE(fixed_font);
    StrikeLine[IDX_BIFIXEDFONT] = STRIKELINE(fixed_font);
    StrikeLine[IDX_SYMBOLFONT] = STRIKELINE(normal_font);

    /* list indents for ordered/unordered lists */

    ListIndent1 = XTextWidth(normal_font, "ABCabc", 6)/6;
    ListIndent2 = ListIndent1;
    ListIndent1 = 2 * ListIndent1;

    /* check for monchrome displays */

    if (DefaultDepth(display, screen) == 1)
    {
        strikeColor = labelColor = textColor = BlackPixel(display, screen);
        transparent = statusColor = windowColor = WhitePixel(display, screen);
        windowShadow = windowTopShadow = windowBottomShadow = textColor;
    }
    else   /* try for color but degrade as sensibly as possible */
    {
        if (!DetermineColor(prog, "windowColor", 220, 209, 186, &windowColor))
        {
            if (GetNamedColor("gray", &windowColor))
            {
                windowBottomShadow = BlackPixel(display, screen);

                if (!GetNamedColor("dim gray", &windowShadow))
                    windowShadow = windowColor;

                if (!GetNamedColor("light gray", &windowTopShadow))
                    windowTopShadow = WhitePixel(display, screen);

                strikeColor = textColor = labelColor = WhitePixel(display, screen);
            }
            else
            {
                labelColor = strikeColor = textColor = BlackPixel(display, screen);
                windowShadow = windowColor = WhitePixel(display, screen);
                windowShadow = windowTopShadow = windowBottomShadow = textColor;
            }
        }
        else
        {
            DetermineColor(prog, "textColor", 0, 0, 100, &textColor);
            DetermineColor(prog, "labelColor", 0, 100, 100, &labelColor);
            DetermineColor(prog, "strikeColor", 170, 0, 0, &strikeColor);

            DetermineColor(prog, "windowShadow", 200, 188, 169, &windowShadow);
            DetermineColor(prog, "windowTopShadow", 255, 242, 216, &windowTopShadow);
            DetermineColor(prog, "windowBottomShadow", 180, 170, 152, &windowBottomShadow);

            if (windowTopShadow == windowColor)
                windowTopShadow = WhitePixel(display, screen);

            if (windowBottomShadow == windowColor)
                windowBottomShadow = BlackPixel(display, screen);

            if (windowColor == textColor)
            {
                windowShadow = windowColor = BlackPixel(display, screen);
                strikeColor = windowTopShadow = windowBottomShadow = textColor;
            }
        }

        transparent = windowColor;
        statusColor = windowShadow;
    }
}

void MakePaper(int UsePaper)
{
    XGCValues values;
    unsigned int valuemask;
    Pixmap pixmap;
    XImage *image;
    GC drawGC;
    char *data;

    gc_fill = XCreateGC(display, win, 0, 0);
    XSetFunction(display, gc_fill, GXcopy);

    if (UsePaper && (depth == 8 || depth == 24))
    {
        tileWidth = tileHeight = 64;
        XQueryBestTile(display, win, tileWidth, tileHeight, &tileWidth, &tileHeight);

        if ((pixmap = XCreatePixmap(display, win, tileWidth, tileHeight, depth)) == 0)
        {
            fprintf(stderr, "Failed to create Pixmap for background!\n");
            exit(1);
        }

        data = (char *)CreateBackground(tileWidth, tileHeight, depth);

        if ((image = XCreateImage(display, DefaultVisual(display, screen),
             depth, ZPixmap, 0, data,
             tileWidth, tileHeight, (depth == 24 ? 32 : 8), 0)) == 0)
        {
            free(data);
            XFreePixmap(display, pixmap);
            fprintf(stderr, "Failed to create X Image for background!\n");
            exit(1);
        }

        drawGC = XCreateGC(display, pixmap, 0, 0);
        XSetFunction(display, drawGC, GXcopy);
        XPutImage(display, pixmap, drawGC, image, 0, 0, 0, 0, tileWidth, tileHeight);
        XFreeGC(display, drawGC);
        XDestroyImage(image); /* also free's image data */

        valuemask = GCTile|GCFillStyle;
        values.tile = pixmap;
        values.fill_style = FillTiled;
        XChangeGC(display, gc_fill, valuemask, &values);
    }
    else
    {
        UsePaper = 0;
        XSetForeground(display, gc_fill, windowColor);
    }

 /* create pixmaps for smiling/frowing faces */
    MakeFaces(depth);
}

/* create clone of self by forking and duplicating resources

    creates duplicate of history file and closes
    all open files, then duplicates stdio ...

    returns 0 for parent, and 1 for clone
*/

int CloneSelf(void)
{
    int childpid, tty;
    int x = 0, y = 0;               /* window position */
    unsigned int border_width = 4;  /* border four pixels wide */
    unsigned int display_width, display_height;
    char *window_name = "World Wide Web Browser";
    char *icon_name = "web";
    int i, fh, depth, tag;
    unsigned int class;
    Visual *visual;
    unsigned long valuemask, where;
    XSetWindowAttributes attributes;
    Pixmap icon_pixmap;

    /* close FTP channel if open */

    CloseFTP();

    if ((tty = open("/dev/tty", 2)) == -1 && (tty = open("/dev/null", 2)) == -1)
    {
        Warn("Can't open /dev/tty");
        return 0;
    }

    /* ensure that children won't become zombies */

    signal(SIGCHLD, SIG_IGN);

    if ((childpid = fork()) < 0)
    {
        close(tty);
        Warn("Can't fork new process");
        return 0;
    }

    if (childpid != 0)
    {
        close(tty);
        return 0;
    }

    /* ok child process - so dup stdio  - this leaves other files open! */

    close(0); dup(tty);
    close(1); dup(tty);
    close(2); dup(tty);
    close(tty);

    close(ConnectionNumber(display));  /* close TCP connection to X server */

    /* now clone history file and close original */

    initialised = 0;  /* avoid X output until this is true! */

/*    XCloseDisplay(display); /* close connection for parent display */

    /* connect to X server */

    if ( (display = XOpenDisplay(display_name)) == NULL )
    {
        (void) fprintf(stderr, "www: cannot connect to X server %s\n",
                        XDisplayName(display_name));
        exit(-1);
    }

    /* free memory but not pixmaps which we no longer own! */
    FreeImages(1);
    FreeForms();   /* is this right? */
                     
  /* try to allocate 128 fixed colors + 16 grey scales */
    if (InitImaging(ColorStyle) == MONO)
        UsePaper = 0;

    GetResources();  /* load font and colour resources */

    if (ColorStyle == MONO)
        statusColor = windowColor;

    /* get screen size from display structure macro */

    screen = DefaultScreen(display);

    depth = DisplayPlanes(display, screen);
    class = InputOutput;    /* window class*/
    visual = DefaultVisual(display, screen);
    valuemask = CWColormap | CWBorderPixel | CWBitGravity |
             CWBackingStore | CWBackingPlanes;
    attributes.colormap = colormap;
    attributes.bit_gravity = ForgetGravity;
    attributes.backing_planes = 0;
    attributes.backing_store = NotUseful;

    win = XCreateWindow(display, RootWindow(display, screen),
            x,y, win_width, win_height, border_width,
            depth, class, visual, valuemask, &attributes);

    XSetWindowBackground(display, win, windowColor);

    /* Create pixmap of depth 1 (bitmap) for icon */

    icon_pixmap = XCreateBitmapFromData(display, win, www_bits,
                                    www_width, www_height);

    /* Create default pixmap for use when we can't load images */

    default_pixmap = XCreatePixmapFromBitmapData(display, win, www_bits,
                      www_width, www_height, textColor, transparent, depth);

    default_pixmap_width = www_width;
    default_pixmap_height = www_height;

    /* initialize size hint property for window manager */

    size_hints.flags = PPosition | PSize | PMinSize;
    size_hints.x = x;
    size_hints.y = y;
    size_hints.width = win_width;
    size_hints.height = win_height;
    size_hints.min_width = 440;
    size_hints.min_height = 250;

    /* set properties for window manager (always before mapping) */

    XSetStandardProperties(display, win, window_name, icon_name,
        icon_pixmap, (char **)0, 0, &size_hints);

    /* select events wanted */

    XSelectInput(display, win, ExposureMask | KeyPressMask | KeyReleaseMask |
       Button1MotionMask | ButtonPressMask | ButtonReleaseMask | StructureNotifyMask);

    /* create hourglass cursor */

    hourglass = XCreateFontCursor(display, XC_watch);

    /* create GCs for text and drawing: gc_scrollbar, gc_status, gc_text */

    gc_scrollbar = XCreateGC(display, win, 0, 0);
    gc_status = XCreateGC(display, win, 0, 0);
    gc_text = XCreateGC(display, win, 0, 0);
    MakePaper(UsePaper);
    font = -1;

    if (document != HTMLDOCUMENT)
        SetBanner(CurrentDoc.url);

    /* refresh paint buffer to ensure that images resources are created */

    where = PixelOffset;
    PixelOffset = 0;
    ParseHTML(&i);

    if (where > 0)
        DeltaHTMLPosition(where);

    /* Map Display Window */

    XMapWindow(display, win);

    return 1;
}

void main(int argc, char **argv)
{
    int x = 0, y = 0;               /* window position */
    unsigned int border_width = 4;  /* border four pixels wide */
    unsigned int display_width, display_height;
    char *window_name = "World Wide Web Browser";
    char *icon_name = "web";
    int best_depth, tag, n;
    unsigned long *pixels;
    unsigned int class;
    unsigned long valuemask;
    XSetWindowAttributes attributes;
    Pixmap icon_pixmap;
    char *q;

    UseHTTP2 = 1;
    UsePaper = 1;
    ColorStyle = COLOR232;

    prog = argv[0];

    while (argc > 1)
    {

        if (strcmp(argv[1], "-debug") == 0)
        {
            debug = 1;
            --argc;
            ++argv;
        }
        else if (strcmp(argv[1], "-old") == 0)
        {
            UseHTTP2 = 0;
            --argc;
            ++argv;
        }
        else if (strcmp(argv[1], "-open") == 0)
        {
            OpenSubnet = 1;
            --argc;
            ++argv;
        }
        else if (strcmp(argv[1], "-large") == 0)
        {
            fontsize = 1;
            --argc;
            ++argv;
        }
        else if (strcmp(argv[1], "-giant") == 0)
        {
            fontsize = 2;
            --argc;
            ++argv;
        }
        else if (strcmp(argv[1], "-mono") == 0)
        {
            ColorStyle = MONO;
            --argc;
            ++argv;
        }
        else if (strcmp(argv[1], "-grey") == 0 ||
                     strcmp(argv[1], "-gray") == 0)
        {
            ColorStyle = GREY4;
            --argc;
            ++argv;
        }
        else if (strcmp(argv[1], "-color") == 0)
        {
            ColorStyle = COLOR888;
            --argc;
            ++argv;
        }
        else if (strcmp(argv[1], "-plain") == 0)
        {
            UsePaper = 0;
            --argc;
            ++argv;
        }
        else
            break;
    }

    /* initialise ISO character entity definitions */

    InitEntities();

    /* connect to X server */

    if ( (display=XOpenDisplay(display_name)) == NULL )
    {
        (void) fprintf(stderr, "www: cannot connect to X server %s\n",
                        XDisplayName(display_name));
        exit(-1);
    }

/* for debuging  - disable buffering of output queue */

    XSynchronize(display, 0);  /* 0 enable, 1 disable */

    gateway = DetermineValue(prog, "gateway", GATEWAY_HOST);
    help = DetermineValue(prog, "help", HELP_URL);

    printer = DetermineValue(prog, "printer", PRINTER);
    startwith = DetermineValue(prog, "startwith", DEFAULT_URL);

    /* get screen size from display structure macro */

    screen = DefaultScreen(display);
    depth = DisplayPlanes(display, screen);

    visual = BestVisual(DirectColor, &best_depth);

    if (ColorStyle != COLOR888  || best_depth <= depth)
    {
        colormap = DefaultColormap(display, screen);
        visual = DefaultVisual(display, screen);

        if ((ColorStyle = InitImaging(ColorStyle)) == MONO)
            UsePaper = 0;
    }
    else
    {
        depth = best_depth;
        colormap = XCreateColormap(display, RootWindow(display, screen),
                                                    visual, AllocNone);
    }

    GetResources();  /* load font and colour resources */

    if (ColorStyle == MONO)
        statusColor = windowColor;

    display_width = DisplayWidth(display, screen);
    display_height = DisplayHeight(display, screen);

    /* size widow with enough room for text */

    charWidth = XTextWidth(Fonts[IDX_FIXEDFONT], "ABCabc", 6)/6;
    charHeight = SPACING(Fonts[IDX_FIXEDFONT]);

    win_width = 83 * charWidth;
    win_height = 35 * charHeight + 7;
    class = InputOutput;    /* window class*/
    valuemask = CWColormap | CWBorderPixel | CWBitGravity |
             CWBackingStore | CWBackingPlanes;
    attributes.colormap = colormap;
    attributes.bit_gravity = ForgetGravity;
    attributes.backing_planes = 0;
    attributes.backing_store = NotUseful;

    /* create opaque window */

    win = XCreateWindow(display, RootWindow(display, screen),
            x,y, win_width, win_height, border_width,
            depth, class, visual, valuemask, &attributes);

    /* Create pixmap of depth 1 (bitmap) for icon */

    icon_pixmap = XCreateBitmapFromData(display, win, www_bits,
                                    www_width, www_height);

    /* Create default pixmap for use when we can't load images */

    default_pixmap = XCreatePixmapFromBitmapData(display, win, www_bits,
                      www_width, www_height, textColor, transparent, depth);

    default_pixmap_width = www_width;
    default_pixmap_height = www_height;

    /* initialize size hint property for window manager */

    size_hints.flags = PPosition | PSize | PMinSize;
    size_hints.x = x;
    size_hints.y = y;
    size_hints.width = win_width;
    size_hints.height = win_height;
    size_hints.min_width = 440;
    size_hints.min_height = 350;

    /* set properties for window manager (always before mapping) */

    XSetStandardProperties(display, win, window_name, icon_name,
        icon_pixmap, argv, argc, &size_hints);

    /* select events wanted */

    XSelectInput(display, win, ExposureMask | KeyPressMask | KeyReleaseMask |
       Button1MotionMask | ButtonPressMask | ButtonReleaseMask |
       StructureNotifyMask | SubstructureNotifyMask);

    /* create hourglass cursor */

    hourglass = XCreateFontCursor(display, XC_watch);

    /* create GCs for text and drawing: gc_scrollbar, gc_status, gc_text */

    gc_scrollbar = XCreateGC(display, win, 0, 0);
    gc_status = XCreateGC(display, win, 0, 0);
    gc_text = XCreateGC(display, win, 0, 0);

    /* find user's name */

     user = getenv("USER");     /* shell variable for user name */

    InitCurrent(CurrentDirectory());   /* set current host to self etc. */
    SetToolBarFont(legend_font);
    SetStatusFont(legend_font);
    SetDisplayGC(gc_text);

    MakePaper(UsePaper);  /* create gc_fill for textured paper */

    /* create pixmap for testing initial <img> implementation */

    /* Map Display Window */

    XMapWindow(display, win);

    /* get buffer with named file in it */

    hdrlen = 0;
    buffer = NULL;
    NewDoc.length = 0;

    if (argc == 1)
    {
        document = HTMLDOCUMENT;
        buffer = GetDocument("default.html", NULL, LOCAL);

        if (buffer == NULL)
            buffer = GetDocument(startwith, NULL, REMOTE);

        if (buffer)
        {
            SetBanner(NewDoc.url);

            if (NewDoc.type == HTMLDOCUMENT || NewDoc.type == TEXTDOCUMENT)
            {
                SetCurrent();
                NewBuffer(buffer);
            }
        }
        else if (Authorize)
            SetBanner(argv[1]);
        else
            exit(1);
    }
    else
    {
        if (strchr(argv[1], ':'))
            tag = REMOTE;
        else
            tag = LOCAL;

        buffer = GetDocument(argv[1], NULL, tag);

        if (buffer)
        {
            SetBanner(argv[1]);

            if (NewDoc.type == HTMLDOCUMENT || NewDoc.type == TEXTDOCUMENT)
            {
                SetCurrent();
                NewBuffer(buffer);
            }
        }
        else if (Authorize)
            SetBanner(argv[1]);
        else
            exit(1);
    }

    if (CurrentDoc.url == NULL && buffer &&
          NewDoc.type != TEXTDOCUMENT &&
          NewDoc.type != HTMLDOCUMENT)
    {
        DisplayExtDocument(buffer+NewDoc.hdrlen, NewDoc.length-NewDoc.hdrlen, NewDoc.type, NewDoc.path);
        free(buffer);
        NewDoc.buffer = buffer = 0;
        NewDoc.hdrlen = 0;
        NewDoc.length = 0;
        NewDoc.type = TEXTDOCUMENT;
        SetCurrent();
        NewBuffer(buffer);
    }

    /* get events, use first Expose to display text and graphics;
     * ConfigureNotify to indicate a resize; ButtonPress or KeyPress
     * to exit
     */

    for (;;)
        PollEvents(1);
}
