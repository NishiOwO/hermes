/* display the file in the window */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/keysym.h>
#include "www.h"

#define IsWhiteSpace(c)  (c == ' ' || c == '\n' || c == '\t' || c == '\r')

extern Display *display;
extern int screen;
extern Window win;
extern int ExposeCount;  /* used to monitor GraphicsExpose events during scrolling */
extern int error;        /* set by HTML parser */
extern int sbar_width;
extern int statusHeight;
extern int ToolBarHeight;
extern unsigned long textColor, windowColor;
extern unsigned int win_width, win_height;
extern int document;  /* HTMLDOCUMENT or TEXTDOCUMENT */
extern int IsIndex;   /* HTML searchable flag */
extern int UsePaper;
extern int new_form;
extern int FindStr;   /* search this document for string */
extern XFontStruct *fixed_font, *normal_font;
extern char *FindStrVal;
extern Doc CurrentDoc;
extern GC gc_fill;
extern unsigned int tileHeight;
extern char *targetptr;    /* for toggling view between HTML and TEXT */
extern char *targetId;     /* named ID (or anchor NAME) */
extern long ViewOffset;    /* for toggling between HTML/TEXT views */
extern long IdOffset;      /* offset of named ID */
extern Frame background;
extern XFontStruct *Fonts[FONTS];
/* 
    The current top line is displayed at the top of the window,the pixel
    offset is the number of pixels from the start of the document.
*/

GC disp_gc;

char *buffer;            /* the start of the document buffer */
char *StartOfLine;       /* the start of the current top line */
int hdrlen;              /* offset to start of data from top of buffer*/
long PixelOffset;        /* the pixel offset to this line */
long buf_height;
long lineHeight;
long chDescent;
int PixelIndent;
int buf_width;
int chStrike;
int chWidth;            /* width of average char */
int spWidth;            /* width of space char */
int font = -1;               /* index into Fonts[] */

XFontStruct *pFontInfo;

void SetDisplayWin(Window aWin)
{
    win = aWin;
}

void SetDisplayGC(GC aGC)
{
    disp_gc = aGC;
}

void SetDisplayFont(XFontStruct *pf)
{
    pFontInfo = pf;
    XSetFont(display, disp_gc, pFontInfo->fid);
    XSetForeground(display, disp_gc, textColor);
    XSetBackground(display, disp_gc, windowColor);
}

void SetFont(GC gc, int fontIndex)
{
    font = fontIndex;
    pFontInfo = Fonts[fontIndex];
    XSetFont(display, gc, pFontInfo->fid);
    XSetForeground(display, gc, textColor);
    XSetBackground(display, gc, windowColor);
    lineHeight = 2 + pFontInfo->max_bounds.ascent + pFontInfo->max_bounds.descent;
    chDescent = pFontInfo->max_bounds.descent;
    chStrike = lineHeight - 2 - (pFontInfo->max_bounds.ascent + chDescent)/2;
    spWidth = XTextWidth(pFontInfo, " ", 1);
    chWidth = XTextWidth(pFontInfo, "ABCabc", 6)/6;
}

void SetEmphFont(GC gc, XFontStruct *pFont, XFontStruct *pNormal)
{
    pFontInfo = pFont;
    XSetFont(display, gc, pFont->fid);
    XSetForeground(display, gc, textColor);
    XSetBackground(display, gc, windowColor);
    lineHeight = 2 + pNormal->max_bounds.ascent + pNormal->max_bounds.descent;
    chDescent = pNormal->max_bounds.descent;
    chStrike = lineHeight - 2 - (pFontInfo->max_bounds.ascent + chDescent)/2;
    spWidth = XTextWidth(pFontInfo, " ", 1);
    chWidth = XTextWidth(pFontInfo, "ABCabc", 6)/6;
}

/*
    When a new file buffer is created, the first thing to do is to measure
    the length of the buffer in pixels and set up the scrollbar appropriately.
    The reference point should be set to the beginning of the buffer.

    Assumes that pFontInfo has been set up in advance,
                 and that buffer and hdrlen are ok.
*/

void NewBuffer(char *buf)
{
    long target;

    if (buffer && buffer != buf)
        free(buffer);

    buffer = buf;
    hdrlen = CurrentDoc.hdrlen;
    document = CurrentDoc.type;
    StartOfLine = buffer+hdrlen;
    PixelOffset = 0;
    PixelIndent = 0;

    if (document == HTMLDOCUMENT)
    {
        SetFont(disp_gc, IDX_NORMALFONT);
        targetptr = 0;
        targetId = CurrentDoc.anchor;
    }
    else
        SetDisplayFont(fixed_font);

    FreeImages(0); /* free image resources */
    FreeForms();   /* free form data structures */
    FreeFrames(background.child);  /* free tree of frames */
    background.child = NULL;
    IsIndex = 0;  /* clear the HTML searchable flag */
    lineHeight = 2 + pFontInfo->max_bounds.ascent + pFontInfo->max_bounds.descent;
    chDescent = pFontInfo->max_bounds.descent;
    chWidth = XTextWidth(pFontInfo, " ", 1);
    buf_height = DocHeight(buffer+hdrlen, &buf_width);

    if (document == HTMLDOCUMENT && IdOffset > 0)
    {
        target = IdOffset;

        if (target > buf_height - WinHeight)
        {
            target = buf_height - WinHeight;

            if (target < 0)
                target == 0;
        }

        DeltaHTMLPosition(target);
    }

    SetScrollBarWidth(buf_width);
    SetScrollBarHeight(buf_height);
    SetScrollBarHPosition(PixelIndent, buf_width);
    SetScrollBarVPosition(PixelOffset, buf_height);
}

void DisplaySizeChanged(int all)
{
    int max_indent;
    long h, target;

    if (document == HTMLDOCUMENT)
    {
        new_form = 0;
        targetptr = TopStr(&background);
        PixelOffset = 0;
        buf_height = ParseHTML(&buf_width);

        if (ViewOffset > 0)
        {
            target = ViewOffset;
            h = buf_height - WinHeight;

            if (h <= 0)
                target = 0;
            else if (target > h)
                target = h;

            DeltaHTMLPosition(target);
        }
    }
    else if (all || buf_height == 0)
    {
        PixelOffset = CurrentHeight(buffer);
        buf_height = DocHeight(buffer, &buf_width);
    }

    max_indent = (buf_width > WinWidth ? buf_width - WinWidth : 0);

    if (max_indent < PixelIndent)
        PixelIndent = max_indent;

    SetScrollBarWidth(buf_width);
    SetScrollBarHeight(buf_height);
    SetScrollBarHPosition(PixelIndent, buf_width);
    SetScrollBarVPosition(PixelOffset, buf_height);
}

int LineLength(char *buf)
{
    char *s;
    int len, c;

    s = buf;
    len = 0;

    while ((c = *s++) && c != '\n')
        ++len;

    if (buf[len-1] == '\r')
        --len;

    return len;
}

/* DEBUG: return pointer to null terminated line of text from string s */
char *TextLine(char *s)
{
    static char buf[128];
    int i, c;

    for (i = 0; i < 127; ++i)
    {
        c = *s;

        if (c == '\0' || c == '\r' || c == '\n')
            break;

        buf[i] = c;
        ++s;
    }

    buf[i] = '\0';
    return buf;
}


/* work out how far window has moved relative to document */

int DeltaTextPosition(long h)
{
    long d1;
    char *p1;
    int delta;
    int nClipped;            /* the number of pixels hidden for this line */

    nClipped = PixelOffset % lineHeight;

    /* find the text line which intersects/starts from top of window */
    /* d1 is pixel offset to new top line, p1 points to its text */
    /* PixelOffset is the pixel offset to the previous top line */

    if (h > PixelOffset)
    {     /* search forward */
        d1 = PixelOffset-nClipped;
        p1 = StartOfLine;

        while (d1 + lineHeight <= h)
        {
            while (*p1)
            {
                if (*p1++ != '\n')
                     continue;

                d1 += lineHeight;
                break;
            }

            if (*p1 == '\0')
                break;         /* this should be unnecessary */
        }
    }
    else  /* search backward */
    {
        d1 = PixelOffset-nClipped;
        p1 = StartOfLine;

        while (d1 > h)
        {
            if (p1 == buffer+hdrlen)
                 break;         /* this should be unnecessary */

            /* now move back to start of previous line*/

            --p1;  /* to first point to \n at end of previous line */

            for (;;)   /* and find start of that line */
            {
                if (p1 == buffer+hdrlen)
                        break;
                else if (*--p1 == '\n')
                {
                        ++p1;
                        break;
                }
            }

            /* finally adjust pixel offset to start of that line */
            d1 -= lineHeight;
        }
    }
    
    /* delta is required movement of window in pixels */

    delta = h - PixelOffset;

    StartOfLine = p1;
    PixelOffset = h;
    return delta;
}

/* move display to place the left of the window at indent pixels from left hand edge */

void MoveHDisplay(int indent)
{
    XRectangle rect;
    int delta;

    /* see if change in pixel offset from start of document is
       small enough to justify a scroll of window contents     */

    delta = indent - PixelIndent;
    PixelIndent += delta;


    if (delta > 0 && delta < (2 * WinWidth)/3)
    {
        /* document moves left by delta pixels thru window */

        rect.x = WinLeft;
        rect.y = WinTop;
        rect.width = WinWidth;
        rect.height = WinHeight;
        XSetClipRectangles(display, disp_gc, 0, 0, &rect, 1, Unsorted);

        XCopyArea(display, win, win, disp_gc,
                    WinLeft + delta, WinTop,
                    WinWidth - delta, WinHeight,
                    WinLeft, WinTop);

        /* we must note that a copy request has been issued, and avoid further
           such requests until all resulting GraphicsExpose events are handled
           as these will repair any holes caused by windows above this one */

        ExposeCount = 1;

        DisplayDoc(WinRight - delta, WinTop, delta, WinHeight);
    }
    else if (delta < 0 && -delta < (2 * WinWidth)/3)
    {
        /* document moves right by -delta pixels thru window */

        rect.x = WinLeft;
        rect.y = WinTop;
        rect.width = WinWidth;
        rect.height = WinHeight;
        XSetClipRectangles(display, disp_gc, 0, 0, &rect, 1, Unsorted);

        XCopyArea(display, win, win, disp_gc,
                    WinLeft, WinTop,
                    WinWidth + delta, WinHeight,
                    WinLeft - delta, WinTop);

        DisplayDoc(WinLeft, WinTop, - delta, WinHeight);        
    }
    else if (delta != 0)
        DisplayDoc(WinLeft, WinTop, WinWidth, WinHeight);
}

/* move display to make h the top of the window in pixels from start of documenr */

void MoveVDisplay(long h)
{
    XRectangle rect;
    int delta;

    /* see if change in pixel offset from start of document is
       small enough to justify a scroll of window contents     */

    if (document == HTMLDOCUMENT)
        delta = DeltaHTMLPosition(h);
    else
        delta = DeltaTextPosition(h);

    if (delta > 0 && delta < (2 * WinHeight)/3)
    {
        /* document moves up by delta pixels thru window */

        rect.x = WinLeft;
        rect.y = WinTop;
        rect.width = WinWidth;
        rect.height = WinHeight;
        XSetClipRectangles(display, disp_gc, 0, 0, &rect, 1, Unsorted);

        XCopyArea(display, win, win, disp_gc,
                    WinLeft, WinTop + delta,
                    WinWidth, WinHeight - delta,
                    WinLeft, WinTop);

        /* we must note that a copy request has been issued, and avoid further
           such requests until all resulting GraphicsExpose events are handled
           as these will repair any holes caused by windows above this one */

        ExposeCount = 1;

        DisplayDoc(WinLeft, WinBottom - delta, WinWidth, delta);
    }
    else if (delta < 0 && -delta < (2 * WinHeight)/3)
    {
        /* document moves down by delta pixels thru window */

        rect.x = WinLeft;
        rect.y = WinTop;
        rect.width = WinWidth;
        rect.height = WinHeight;
        XSetClipRectangles(display, disp_gc, 0, 0, &rect, 1, Unsorted);

        XCopyArea(display, win, win, disp_gc,
                    WinLeft, WinTop,
                    WinWidth, WinHeight + delta,
                    WinLeft, WinTop - delta);

        DisplayDoc(WinLeft, WinTop, WinWidth, - delta);        
    }
    else if (delta != 0)
        DisplayDoc(WinLeft, WinTop, WinWidth, WinHeight);
}


/* Should these adjust window offset to ensure that nClipped == 0 ? */
/*   (i.e. so that top line is never clipped after MoveUpLine)     */

void MoveLeftLine()
{
    int offset;

    offset = PixelIndent - lineHeight;

    if (offset < 0)
        offset = 0;

    if (!AtLeft(offset))
    {
        MoveHDisplay(offset);
        MoveHSlider(offset, buf_width);
    }
    else
        MoveToLeft();
}

void MoveUpLine()
{
    long offset;

    offset = PixelOffset - lineHeight;

    if (offset < 0)
        offset = 0;

    if (!AtStart(offset))
    {
        MoveVDisplay(offset);
        MoveVSlider(offset, buf_height);
    }
    else
        MoveToStart();
}

void MoveLeftPage()
{
    int offset;

    offset = WinWidth - lineHeight * 2;

    offset = PixelIndent - offset;

    if (offset < 0)
        offset = 0;

    if (!AtStart(offset))
    {
        MoveHDisplay(offset);
        MoveHSlider(offset, buf_width);
    }
    else
        MoveToLeft();
}

void MoveUpPage()
{
    long offset;

    offset = WinHeight - lineHeight * 2;

    offset = PixelOffset - offset;

    if (offset < 0)
        offset = 0;

    if (!AtStart(offset))
    {
        MoveVDisplay(offset);
        MoveVSlider(offset, buf_height);
    }
    else
        MoveToStart();
}

void MoveRightLine()
{
    int offset;

    offset = PixelIndent + lineHeight;

    if (!AtRight(offset))
    {
        MoveHDisplay(offset);
        MoveHSlider(offset, buf_width);
    }
    else
        MoveToRight();
}

void MoveDownLine()
{
    long offset;

    offset = PixelOffset + lineHeight;

    if (!AtEnd(offset))
    {
        MoveVDisplay(offset);
        MoveVSlider(offset, buf_height);
    }
    else
        MoveToEnd();
}

void MoveRightPage()
{
    int offset;

    offset = WinWidth - lineHeight * 2;
    offset = PixelIndent + offset;

    if (!AtRight(offset))
    {
        MoveHDisplay(offset);
        MoveHSlider(offset, buf_width);
    }
    else
        MoveToRight();
}

void MoveDownPage()
{
    long offset;

    offset = WinHeight - lineHeight * 2;
    offset = PixelOffset + offset;

    if (!AtEnd(offset))
    {
        MoveVDisplay(offset);
        MoveVSlider(offset, buf_height);
    }
    else
        MoveToEnd();
}

void MoveToLeft()
{
    int offset;

    if (PixelIndent > 0)
    {
        offset = 0;
        MoveHDisplay(offset);
        MoveHSlider(offset, buf_width);
    }

}

void MoveToStart()
{
    long offset;

    if (PixelOffset > 0)
    {
        offset = 0;
        MoveVDisplay(offset);
        MoveVSlider(offset, buf_height);
    }

}

void MoveToRight()
{
    int offset;

    offset = buf_width - WinWidth;

    if (PixelIndent != offset)
    {
        MoveHDisplay(offset);
        MoveHSlider(offset, buf_width);
    }

}

void MoveToEnd()
{
    long offset;

    offset = buf_height - WinHeight;

    if (offset > 0 && PixelOffset != offset)
    {
        MoveVDisplay(offset);
        MoveVSlider(offset, buf_height);
    }
}

void SlideHDisplay(int slider, int scrollExtent)
{
    double dh;

    /* compute the new pixel offset to top of window */

    dh = ((double)slider * buf_width) / scrollExtent;
    MoveHDisplay((long)(dh+0.5));
}


void SlideVDisplay(int slider, int scrollExtent)
{
    double dh;

    /* compute the new pixel offset to top of window */

    dh = ((double)slider * buf_height) / scrollExtent;
    MoveVDisplay((long)(dh+0.5));
}


/*

  Display the text in the buffer appearing in the view defined
  by the rectangle with upper left origin (x, y) and extent (w, h)

  The first line of text is pointed to by (char *)StartOfLine
  and is PixelOffset-nClipped pixels from the start of the document.
  There are nClipped pixels of this line hidden above the top
  of the window, so that the window starts at PixelOffset pixels
  from the start of the document itself.

*/

void DisplayDoc(int x, int y, unsigned int w, unsigned int h)
{
    int line_number, c, len, x1, y1;
    char *p, *r, lbuf[512];
    XRectangle rect;
    int nClipped;            /* the number of pixels hidden for this line */
    extern int sliding;

    if (document == HTMLDOCUMENT)
    {
        DisplayHTML(x, y, w, h);
        PaintVersion(error);
        return;
    }

    error = 0;
    PaintVersion(error);

    nClipped = PixelOffset % lineHeight;
    SetFont(disp_gc, IDX_FIXEDFONT);

    /* make absolutely certain we don't overwrite the scrollbar */

    if (w > WinWidth)
        w = WinWidth;

    /* make absolutely certain we don't overwrite the status bar */

    if (h > WinHeight)
        h = WinHeight;

    /* the text must be clipped to avoid running over into adjacent
       regions, i.e. the scrollbar at the rhs */

    rect.x = x;
    rect.y = y;
    rect.width = w;
    rect.height = h;
    XSetClipRectangles(display, disp_gc, 0, 0, &rect, 1, Unsorted);
    XSetClipRectangles(display, gc_fill, 0, 0, &rect, 1, Unsorted);

    x1 = WinLeft;
    y1 = WinTop-nClipped;
    line_number = 0;
    p = StartOfLine;

    if (UsePaper)
        XSetTSOrigin(display, gc_fill,
           -PixelIndent % tileHeight, -(int)(PixelOffset % tileHeight));

    XFillRectangle(display, win, gc_fill, x, y, w, h);

    XSetForeground(display, disp_gc, textColor);

    while ( (y1 < y+(int)h) && *p)
    {
        y1 += lineHeight;

        r = lbuf;
        len = 0;

        while ((c = *p) && c != '\n')
        {
            ++p;

            if (len > 512 - TABSIZE)
                continue;

            if (c == '\t')
            {
                do
                    *r++ = ' ';
                while (++len % TABSIZE);

                continue;
            }

            if (c == '\r')
                continue;

            if (c == '\b')
            {
                if (len > 0)
                {
                    --len;
                    --r;
                }

                continue;
            }

            ++len;
            *r++ = c;
        }

        if (y1 > y)
            XDrawString(display, win, disp_gc, x1+4-PixelIndent, y1 - chDescent, lbuf, len);


        if (*p == '\n')
            ++p;
    }
}

/* what is the offset from the start of the file to the current line? */

long CurrentHeight(char *buf)
{
    long height;
    extern int debug;

    if (!buf)
        return 0;

    if (document == HTMLDOCUMENT)
    {
        height = 0;
        /* ParseSGML(HEIGHT, &height, &buf, 0, 0, StartOfLine); */
    }
    else
        height = (*buf ? PixelOffset : 0);

    return height;
}

/* how long (in pixels) is the file ? */
long DocHeight(char *buf, int *width)
{
    char *p;
    int w;
    long height;
    extern int debug;

    *width = WinWidth;

    if (!buf)
        return lineHeight;

    if (document == HTMLDOCUMENT)
    {
        height = ParseHTML(width);
    }
    else
    {
        height = (*buf ? lineHeight : 0);
        p = buf;

        while (*buf)
        {
            if (*buf++ == '\n')
            {
                height += lineHeight;
                w = chWidth * (buf - p);
                p = buf;

                if (w > *width)
                    *width = w;
            }
        }

        w = chWidth * (buf - p - 1);

        if (w > *width)
            *width = w;
    }

    return height;
}

/* setup skip table for searching str forwards thru document */
void ForwardSkipTable(unsigned char *skip, int len, char *str)
{
    int i;
    unsigned char c;

    for (i = 0; i < 256; ++i)
        skip[i] = len;

    for (i = 1; c = *(unsigned char *)str++; ++i)
        skip[c] = len - i;
}

/* setup skip table for searching str backwards thru document */
void BackwardSkipTable(unsigned char *skip, int len, char *str)
{
    int i;
    unsigned char c;

    for (i = 0; i < 256; ++i)
        skip[i] = len;

    str += len;

    for (i = len - 1; i >= 0; --i)
        skip[(unsigned char) *--str] = i;
}


void FindString(char *str, char **next)
{
    char *p;
    int i, j, c1, c2, patlen, patlen1;
    long len, h, offset;
    unsigned char skip[256];
    static char *np;

    FindStr = 0;
    DisplayStatusBar();

    patlen = strlen(str);
    patlen1 = patlen - 1;
    ForwardSkipTable(skip, patlen, str);
    len = CurrentDoc.length;

    p = *next;

    if (!p ||  p <  buffer || p > buffer + len - 2)
        p = StartOfLine;

    i = p - buffer + patlen1;
    j = patlen1;

    while (j >= 0 && i < len && i >= 0)
    {
        c1 = buffer[i];
        c1 = tolower(c1);

        c2 = str[j];
        c2 = tolower(c2);

        if (IsWhiteSpace(c1))
            c1 = ' ';

        if (c1 == c2)
        {
            --i;
            --j;
            continue;
        }

    retry1:

        i += patlen - j;    /* to next New char */
        j = patlen1;

        if (i >= len)
            break;

        c1 = buffer[i];
        c1 = tolower(c1);

        if (IsWhiteSpace(c1))
            c1 = ' ';

        i += skip[c1];
    }

    if (++j > 0)
    {
        *next = 0;
        Warn("Can't find \"%s\"", str);
    }
    else
    {
        /* move to start of current line */

        *next = buffer+i+patlen1;

        while (i > 0)
        {
            if (buffer[i] != '\n')
            {
                --i;
                continue;
            }

            ++i;
            break;
        }

        /* and display accordingly */

        len = 0;

/*        if (document == HTMLDOCUMENT)
        {
            offset = 0;
            p = StartOfLine;
            ParseSGML(HEIGHT, &offset, &p, 0, 0, buffer+i);
            DeltaHTMLPosition(PixelOffset+offset);

            if (AtLastPage())
            {
                offset = buf_height - WinHeight + lineHeight;

                if (offset > buf_height)
                    offset = buf_height - lineHeight;;

                if (offset < 0)
                    offset = 0;

                DeltaHTMLPosition(offset);
            }
        }
        else   */
        {
            p = buffer;
            StartOfLine = buffer+i;
            PixelOffset = (*p ? lineHeight : 0);

            while (*p && p < StartOfLine)
            {
                if (*p++ == '\n')
                    PixelOffset += lineHeight;
            }

            if (PixelOffset + WinHeight > buf_height)
            {
                len = buf_height - WinHeight;

                if (len < 0)
                    len = 0;

                MoveVDisplay(len);
            }
        }



        Announce("Found \"%s\", press F3 for next match", FindStrVal);
        SetScrollBarHPosition(PixelIndent, buf_width);
        SetScrollBarVPosition(PixelOffset, buf_height);
        DisplayScrollBar();
        DisplayDoc(WinLeft, WinTop, WinWidth, WinHeight);
    }
}

/* toggle view between HTML and PLAIN */
void ToggleView(void)
{
    char *p, *q, *start;
    long offset, maxOffset, target;

    if (CurrentDoc.type == HTMLDOCUMENT)
    {
        if (document == HTMLDOCUMENT)
        {
            document = TEXTDOCUMENT;
            SetFont(disp_gc, IDX_FIXEDFONT);
            p = buffer+hdrlen;
            start = TopStr(&background);
            offset = 0;   /* (*p ? lineHeight : 0); */

            while (*p && p < start)
            {
                if (*p++ == '\n')
                    offset += lineHeight;
            }

            buf_height = DocHeight(buffer+hdrlen, &buf_width);
            maxOffset = buf_height - WinHeight;

            if (offset > maxOffset)
                offset = maxOffset;

            PixelOffset = 0;
            StartOfLine = buffer;
            DeltaTextPosition(offset);
        }
        else if (document == TEXTDOCUMENT)
        {
            document = HTMLDOCUMENT;
            SetFont(disp_gc, IDX_NORMALFONT);
            targetptr = StartOfLine;
            PixelOffset = 0;
            buf_height = ParseHTML(&buf_width);

            if (ViewOffset > 0)
            {
                target = ViewOffset;

                if (target > buf_height - WinHeight)
                {
                    target = buf_height - WinHeight;

                    if (target < 0)
                        target == 0;
                }

                maxOffset = buf_height - WinHeight;

                if (target > maxOffset)
                    target = maxOffset;

                DeltaHTMLPosition(target);
            }
        }

        SetScrollBarWidth(buf_width);
        SetScrollBarHeight(buf_height);
        SetScrollBarHPosition(PixelIndent, buf_width);
        SetScrollBarVPosition(PixelOffset, buf_height);
        DisplayScrollBar();
        DisplayDoc(WinLeft, WinTop, WinWidth, WinHeight);
    }
    else
        Warn("Use View button to toggle view between HTML and plain text");
}
