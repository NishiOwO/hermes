/* html.c - display code for html

This file contains the code for displaying HTML documents, scrolling them
and dealing with hypertext jumps by recognising which buttons are clicked.

*/

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <string.h>
#include <ctype.h>
#include "www.h"

extern Display *display;
extern int screen;
extern Window win;
extern GC disp_gc, gc_fill;
extern Cursor hourglass;
extern int UsePaper;

extern int debug;  /* controls display of errors */
extern int document;  /* HTMLDOCUMENT or TEXTDOCUMENT */
extern int busy;
extern int OpenURL;
extern int IsIndex;
extern int FindStr;
extern char *FindNextStr;
extern int SaveFile;
extern int sbar_width;
extern int statusHeight;
extern int ToolBarHeight;
extern unsigned long windowColor;
extern unsigned int win_width, win_height, tileWidth, tileHeight;
extern XFontStruct *h1_font, *h2_font, *h3_font,
        *normal_font, *italic_font, *bold_font, *fixed_i_font,
        *fixed_b_font, *fixed_font, *legend_font;

extern unsigned long textColor, labelColor, windowTopShadow,
                     strikeColor, windowBottomShadow, windowShadow, windowColor;

/*
    The current top line is displayed at the top of the window,the pixel
    offset is the number of pixels from the start of the document.
*/

extern char *buffer;            /* the start of the document buffer */
extern long PixelOffset;        /* the pixel offset to top of window */
extern int hdrlen;              /* MIME header length at start of buffer */
extern long buf_height;
extern long lineHeight;
extern long chDescent;
extern int buf_width;
extern int PixelIndent;
extern int chStrike;
extern int spWidth;             /* width of space char */
extern int chWidth;             /* width of average char */
extern Doc NewDoc, CurrentDoc;
extern XFontStruct *pFontInfo;
extern XFontStruct *Fonts[FONTS];
extern int LineSpacing[FONTS], BaseLine[FONTS], StrikeLine[FONTS];
extern int ListIndent1, ListIndent2;
extern char *LastBufPtr, *StartOfLine, *StartOfWord; /* in HTML document */

extern char *bufptr;  /* parse position in the HTML buffer */
extern Byte *TopObject;  /* first visible object in window */
extern Byte *paint; /* holds the sequence of paint commands */
extern int paintbufsize;     /* size of buffer, not its contents */
extern int paintlen;         /* where to add next entry */

extern Field *focus;
extern int font;  /* index into Fonts[] array */
int preformatted;
XRectangle displayRect; /* clipping limits for painting html */

long IdOffset;      /* offset for targetId */
char *targetptr;    /* for toggling view between HTML/TEXT views */
char *targetId;     /* for locating named Id during ParseHTML() */

/* globals associated with detecting which object is under the mouse */

char *anchor_start, *anchor_end;
Byte *clicked_element;
int img_dx, img_dy;

/* the background frame structure specifies:

    a) where to start painting in this frame

    b) where this frame ends in paint buffer

    c) tree of nested frames which all
       intersect the top of the window
*/
Frame background;

/* copy line from str to LineBuf, skipping initial spaces and SGML <tags> */
char *CopyLine(char *str, int len)
{
    int c, n, k;
    char *p;
    static char buf[1024];

    if (len == 0)
        return "";

    p = buf;

    while (len-- > 0)
    {
        c = *str;

        if (c == '\0')
            break;

        if (c == '<' && ((n = str[1]) == '!' || n == '/' || isalpha(n)))
        {                
            while (*str++ != '>');
            c = *str;
        }

        if (c == '&' && isalpha(str[1]))
        {
            n = entity(str + 1, &k);

            if (n)
            {
                /* TokenValue = n; */
                str += k;
                *p++  = n;
                continue;
            }
        }

        if (preformatted)
        {
            if (c == '\t')
            {
                n = 8 + (p - buf)%8;

                while (n-- > 0)
                    *p++ = ' ';
            }
            else
                *p++ = c;

            ++str;
            continue;
        }

        if (IsWhite(c))
        {
            do
                c = *++str;
            while (IsWhite(c));

            *p++ = c = ' ';
            continue;
        }

        *p++ = c;
        ++str;
    }

    *p = '\0';
    return buf;
}


void OpenDoc(char *name, char *who, int where)
{
    char *p, *q;
    long wh, target;

    OpenURL = 0;
    FindNextStr = 0;

    XDefineCursor(display, win, hourglass);
    XFlush(display);

    if (name)  /* attempt to get new document */
    {
        /* note current status string */

        SaveStatusString();

        /* quit Open, SaveAs or Find if active */

        OpenURL = SaveFile = FindStr = 0;

        /* kludge to cope with href="#id" */

        if (*name == '#' && document == HTMLDOCUMENT)
        {
            SetStatusString(name);
            StartOfLine = buffer+hdrlen;
            CurrentDoc.offset = PixelOffset; 
            PushDoc(CurrentDoc.offset);

            if (CurrentDoc.anchor)
                free(CurrentDoc.anchor);

            CurrentDoc.anchor = strdup(name+1);

            if (CurrentDoc.url)
                free(CurrentDoc.url);

            CurrentDoc.url = UnivRefLoc(&CurrentDoc);
            RestoreStatusString();

            PixelOffset = 0;
            targetId = name+1;
            buf_height = ParseHTML(&buf_width);

            if (IdOffset > 0)
            {
                target = IdOffset;

                if (target > buf_height - WinHeight)
                {
                    target = buf_height - WinHeight;

                    if (target < 0)
                        target == 0;
                }

                DeltaHTMLPosition(target);
                SetScrollBarVPosition(PixelOffset, buf_height);
            }

            DisplayScrollBar();
            DisplayDoc(WinLeft, WinTop, WinWidth, WinHeight);

            if (!IsIndex)
                Announce(CurrentDoc.url);
        }
        else if ((q = GetDocument(name, who, where)) && *q)
        {
            if (NewDoc.type == TEXTDOCUMENT || NewDoc.type == HTMLDOCUMENT)
            {
                CurrentDoc.offset = PixelOffset;
                PushDoc(CurrentDoc.offset);

                SetBanner(CurrentDoc.url);

                SetCurrent();        /* set current = new scheme etc. */
                NewBuffer(q);

                if (IsIndex)
                    ClearStatus();
            }
            else
            {
                DisplayExtDocument(q+NewDoc.hdrlen, NewDoc.length-NewDoc.hdrlen, NewDoc.type, NewDoc.path);
                free(q);
            }

            RestoreStatusString();
            SetStatusString(NULL);  /* to refresh status display */
            DisplayScrollBar();
            DisplayDoc(WinLeft, WinTop, WinWidth, WinHeight);
        }
        else
            SetStatusString(NULL);  /* to refresh status display */
    }

    XUndefineCursor(display, win);
    XFlush(display);
}

void ReloadDoc(char *name, char *who)
{
    char *p, *q;
    long wh, target;

    OpenURL = 0;
    FindNextStr = 0;

    XDefineCursor(display, win, hourglass);
    XFlush(display);

    if (name)  /* attempt to get new document */
    {
        /* note current status string */

        SaveStatusString();

        /* quit Open, SaveAs or Find if active */

        OpenURL = SaveFile = FindStr = 0;

        /* kludge to cope with href="#id" */

        if (*name == '#' && document == HTMLDOCUMENT)
        {
            SetStatusString(name);
            StartOfLine = buffer+hdrlen;
            CurrentDoc.offset = PixelOffset; 

            if (CurrentDoc.anchor)
                free(CurrentDoc.anchor);

            CurrentDoc.anchor = strdup(name+1);

            if (CurrentDoc.url)
                free(CurrentDoc.url);

            CurrentDoc.url = UnivRefLoc(&CurrentDoc);
            RestoreStatusString();

            PixelOffset = 0;
            targetId = name+1;
            buf_height = ParseHTML(&buf_width);

            if (IdOffset > 0)
            {
                target = IdOffset;

                if (target > buf_height - WinHeight)
                {
                    target = buf_height - WinHeight;

                    if (target < 0)
                        target == 0;
                }

                DeltaHTMLPosition(target);
                SetScrollBarVPosition(PixelOffset, buf_height);
            }

            DisplayScrollBar();
            DisplayDoc(WinLeft, WinTop, WinWidth, WinHeight);

            if (!IsIndex)
                Announce(CurrentDoc.url);
        }
        else if ((q = GetDocument(name, who, REMOTE)) && *q)
        {
            if (NewDoc.type == TEXTDOCUMENT || NewDoc.type == HTMLDOCUMENT)
            {
                CurrentDoc.offset = PixelOffset;
                SetBanner(CurrentDoc.url);

                SetCurrent();        /* set current = new scheme etc. */
                NewBuffer(q);

                if (IsIndex)
                    ClearStatus();
            }
            else
            {
                DisplayExtDocument(q+NewDoc.hdrlen, NewDoc.length-NewDoc.hdrlen, NewDoc.type, NewDoc.path);
                free(q);
            }

            RestoreStatusString();
            SetStatusString(NULL);  /* to refresh status display */
            DisplayScrollBar();
            DisplayDoc(WinLeft, WinTop, WinWidth, WinHeight);
        }
        else
            SetStatusString(NULL);  /* to refresh status display */
    }

    XUndefineCursor(display, win);
    XFlush(display);
}

/* find title text as defined by <TITLE>title text</TITLE> */

#define MAXTITLE 128

char *TitleText(char *buf)
{
    return "Dummy Title";
}

/*
    The global unsigned (char *)TopObject points to
    the paint stream and is adjusted to the first object
    that appears at the top of the window for an offset
    of h pixels from the start of the document.

    This involves a search thru the paint stream, and
    relies on the objects being ordered wrt increasing
    pixel offset from the start of the document.

    The paint stream is organised as a sequence of nested
    frames, intermingled with text lines.

    The procedure returns the pixel difference between
    the desired position and the current position.
*/

Frame *FrameForward(Frame *frame, long top);
Frame *FrameBackward(Frame *frame, long top);

long DeltaHTMLPosition(long h)
{
    long offset, TopOffset, delta;
    Byte *p, *q;
    int tag, c1, c2, k;
    Frame *frame;

    if (h > PixelOffset)  /* search forwards */
        FrameForward(&background, h);
    else if (h == 0) /* shortcut to start */
    {
        FreeFrames(background.child);
        background.child = NULL;
        background.top = paint + FRAMESTLEN;
    }
    else  /* search backwards */
        FrameBackward(&background, h);

    delta = h - PixelOffset;
    PixelOffset = h;

    return delta;
}

/*
  Move forwards thru frame, adjusting frame->top to last text line
  before top. The routine iterates thru peer frames and recurses
  thru decendent frames. The frame (and its descendents) are removed
  from the peer list if it (and therefore they) finish before top.
  This is implemented by returning the frame if it is needed otherwise
  returning NULL

  Question: what happens when top is the bottom of the buffer?
            can this ever occur in practice, e.g. with a null file?
*/

Frame *FrameForward(Frame *frame, long top)
{
    long offset, height;
    unsigned int c1, c2, width, length, len;
    int tag, indent, style, border;
    unsigned char *p, *p2, *obj;
    Frame *peer, *child, *last;

    if (!frame)
        return NULL;

 /* move down each of peer frames */
    peer = FrameForward(frame->next, top);

 /* Does this frame and its descendants end before top ? */
    if (frame->offset + frame->height <= top)
    {
        if (frame == &background) /* should never occur in practice! */
        {
            FreeFrames(background.child);
            background.child = NULL;
            background.top = paint + paintlen;
            return &background; /* never has peers ! */
        }

     /* remove self and any descendents BUT not our peers! */
        frame->next = NULL;  /* unlink from list before freeing */
        FreeFrames(frame);

        return peer;
    }

    frame->next = peer;
    frame->child = FrameForward(frame->child, top);

 /* find last child in list to avoid inserting
    new children in reverse order */

    for (last = frame->child; last; last = last->next)
    {
        if (last->next == NULL)
            break;
    }
    
 /* now move frame->top down until we reach top and insert
    any new children at end of frame->child list */

    p = frame->top;
    p2 = paint + frame->info + FRAMESTLEN + frame->length;

    while (p < p2)
    {
        obj = p;

        tag = *p++;

     /* if frame intersects top then create frame structure
        and find paint position within it (and its children)
        then insert it in front of frame->child list */

        if (tag == BEGIN_FRAME)
        {
            c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
            c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;

            if (offset > top)
            {
                frame->top = obj;
                break;
            }

         /* otherwise pickup frame header params */

            c1 = *p++; c2 = *p++; indent = c1 | c2<<8;
            c1 = *p++; c2 = *p++; width = c1 | c2<<8;
            c1 = *p++; c2 = *p++; height = c1 | c2<<8;
            c1 = *p++; c2 = *p++; height |= (c1 | c2<<8) << 16;
            style = *p++; border = *p++;
            c1 = *p++; c2 = *p++; length = c1 | c2<<8;

            if (offset + height > top)
            {
                child = (Frame *)malloc(sizeof(Frame));
                child->next = NULL;
                child->child = NULL;
                child->offset = offset;
                child->indent = indent;
                child->width = width;
                child->height = height;
                child->info = obj - paint;
                child->top = p;  /* == obj + FRAMESTLEN */
                child->length = length;
                child->style = style;
                child->border = border;
                FrameForward(child, top);

             /* and insert new child in same order
                as it appears in paint buffer */

                if (last)
                    last->next = child;
                else
                    frame->child = child;

                last = child;
            }

            p += length+2; /* to skip over frame's contents */
            continue;
        }

     /* the END_FRAME is only used when scrolling backwards */

        if (tag == END_FRAME)
        {
            p += 4;
            continue;
        }

     /* safety net for garbled paint structure */

        if (tag != TEXTLINE)
        {
            fprintf(stderr, "Unexpected tag: %d\n", tag);
            exit(1);
        }

      /* stop if textline overlaps top or starts after it */

        c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
        c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;

        if (offset >= top)
        {
            frame->top = obj;
            break;
        }

        p += 4; /* skip over baseline, indent */
        c1 = *p++; c2 = *p++; height = c1 | c2<<8;

        if (offset + height > top)
        {
            frame->top = obj;
            break;
        }

        /* skip elements in text line to reach next object */

        while ((tag = *p++) != '\0')
        {
            switch (tag & 0xF)
            {
                case RULE:
                    p += RULEFLEN - 1;
                    break;

                case BULLET:
                    p += BULLETFLEN - 1;
                    break;

                case STRING:
                    p += STRINGFLEN - 1;
                    break;

                case SEQTEXT:
                    ++p; ++p;  /* skip over x position */
                    len = *p++;
                    p += len;
                    break;

                case IMAGE:
                    p += IMAGEFLEN - 1;
                    break;

                case INPUT:
                    p += INPUTFLEN - 1;
                    break;

                default:
                    fprintf(stderr, "Unexpected tag: %d\n", tag);
                    exit(1);
            }
        }

        ++p; ++p;  /* skip over textline size param */
    }

    return frame;
}

/*
  Move backwards thru frame, adjusting frame->top to last text line
  before top. The routine iterates thru peer frames and recurses
  thru decendent frames. The frame (and its descendents) are removed
  from the peer list if it (and therefore they) start at or after top.
  This is implemented by returning the frame if it is needed otherwise
  returning NULL
*/

Frame *FrameBackward(Frame *frame, long top)
{
    long offset, height;
    unsigned int c1, c2, width, length, len;
    int tag, indent, style, border, k;
    unsigned char *p, *p2, *obj;
    Frame *peer, *child;

    if (!frame)
        return NULL;

 /* if this frame starts after top then remove from peer list */

    if (frame->offset >= top)
    {
        if (frame == &background)
        {
            FreeFrames(background.child);
            background.child = NULL;
            background.top = paint + paintlen;
            return &background; /* never has peers ! */
        }

        peer = frame->next;
        frame->next = NULL; /* unlink from list before freeing */
        FreeFrames(frame);
        return FrameBackward(peer, top);
    }

 /* move backwards through peer frames */
    frame->next = FrameBackward(frame->next, top);

 /* move backwards through current children */

    frame->child = FrameBackward(frame->child, top);

 /* now move frame->top back until we reach top and insert
    any new children in front of frame->child list */

    p = TopObject = frame->top;
    p2 = paint + frame->info + FRAMESTLEN;

    while (p > p2)
    {
        /* pop field size into k */
        c2 = *--p; c1 = *--p; k = c1 | c2<<8;

        p -= k;   /* p points to start of previous object */

        obj = p;
        tag = *p++;

        if (tag == BEGIN_FRAME)
        {
            --p;
            continue;
        }

     /* if frame intersects top then create frame structure
        and find paint position within it (and its children)
        then insert it in front of frame->child list */

        if (tag == END_FRAME)
        {
            c1 = *p++; c2 = *p++; length = c1 | c2<<8;
            p = obj - length;

         /* p now points to BEGIN_FRAME tag */

            if (*p++ != BEGIN_FRAME)
            {
                fprintf(stderr, "Unexpected tag: %d when BEGIN_FRAME was expected\n", tag);
                exit(1);
            }

            c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
            c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;

            if (offset >= top)
            {
                p = obj;
                continue;
            }

         /* otherwise pickup frame header params */

            c1 = *p++; c2 = *p++; indent = c1 | c2<<8;
            c1 = *p++; c2 = *p++; width = c1 | c2<<8;
            c1 = *p++; c2 = *p++; height = c1 | c2<<8;
            c1 = *p++; c2 = *p++; height |= (c1 | c2<<8) << 16;
            style = *p++; border = *p++;
            c1 = *p++; c2 = *p++; length = c1 | c2<<8;
            ++p; ++p; /* skip size param */

            if (offset + height > top)
            {
                child = (Frame *)malloc(sizeof(Frame));
                child->next = NULL;
                child->child = NULL;
                child->offset = offset;
                child->indent = indent;
                child->width = width;
                child->height = height;
                child->info = obj - paint;
                child->top = p + length;
                child->length = length;
                child->style = style;
                child->border = border;
                child = FrameBackward(child, top);

             /* and insert new child in front of current children */

                child->child = frame->child;
                frame->child = child;
            }

            p = obj;
            continue;
        }

     /* safety net for garbled paint structure */

        if (tag != TEXTLINE)
        {
            fprintf(stderr, "Unexpected tag: %d when TEXTLINE was expected\n", tag);
            exit(1);
        }

      /* stop if textline overlaps top or starts before it */

        c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
        c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;

        p = obj;

        if (offset < top)
            break;
    }

    frame->top = p;
    return frame;
}

void DrawHButtonUp(int x, int y, int w, int h)
{
    --x; ++y; ++w;  /* adjust drawing position */

    XSetForeground(display, disp_gc, windowTopShadow);
    XFillRectangle(display, win, disp_gc, x, y, w, 1);
    XFillRectangle(display, win, disp_gc, x, y, 1, h-1);

    XSetForeground(display, disp_gc, windowBottomShadow);
    XFillRectangle(display, win, disp_gc, x+1, y+h-1, w-1, 1);
    XFillRectangle(display, win, disp_gc, x+w-1, y+1, 1, h-2);
    XSetForeground(display, disp_gc, textColor);
}

void DrawHButtonDown(int x, int y, int w, int h)
{
    --x; ++y; ++w;  /* adjust drawing position */

    XSetForeground(display, disp_gc, windowBottomShadow);
    XFillRectangle(display, win, disp_gc, x, y, w, 1);
    XFillRectangle(display, win, disp_gc, x, y, 1, h-1);

    XSetForeground(display, disp_gc, windowTopShadow);
    XFillRectangle(display, win, disp_gc, x+1, y+h-1, w-1, 1);
    XFillRectangle(display, win, disp_gc, x+w-1, y+1, 1, h-2);
    XSetForeground(display, disp_gc, textColor);
}

/* Find which object if any is under the mouse at screen coords px, py
  event is one of BUTTONUP, BUTTONDOWN, MOVEUP, MOVEDOWN */

Byte *WhichFrameObject(int event, int x, int y, Byte *p1, Byte *p2,
               int *type, char **start, char **end, int *dx, int *dy)
{
    char *s;
    Byte *p, *q;
    unsigned int tag, len, emph, c1, c2;
    int action, active, fnt, x1, y1, xi, yb, y2, width, height;
    int style, border, length;
    long offset, str;

    x += PixelIndent;

    for (p = p1; p < p2;)
    {
        tag = *p++;

        if (tag == BEGIN_FRAME)
        {
            c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
            c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;

         /* we are done if frame starts after bottom of window */

            y1 = WinTop + (offset - PixelOffset);

            if (y1 >= WinBottom)
                break;

         /* otherwise pickup frame header params */

            c1 = *p++; c2 = *p++; x1 = c1 | c2<<8;
            c1 = *p++; c2 = *p++; width = c1 | c2<<8;
            c1 = *p++; c2 = *p++; height = c1 | c2<<8;
            c1 = *p++; c2 = *p++; height |= (c1 | c2<<8) << 16;
            style = *p++; border = *p++;
            c1 = *p++; c2 = *p++; length = c1 | c2<<8;

         /* call self to find target object in this frame */

            q = WhichFrameObject(event, x, y, p, p + length,
                                    type, start, end, dx, dy);

            if (q)
                return q;

            p += length+2; /* to skip over frame's contents */
            continue;
        }

        if (tag == END_FRAME)
        {
            p += 4;
            continue;
        }

        if (tag != TEXTLINE)
        {
            fprintf(stderr, "Unexpected tag: %d\n", tag);
            exit(1);
        }

        c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
        c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;

        /* y1 points to top of line */
        y1 = WinTop + (offset - PixelOffset);

        if (y1 > (int) WinBottom)
            break;

        c1 = *p++; c2 = *p++; yb = y1 + (c1 | c2<<8);
        c1 = *p++; c2 = *p++; xi = (c1 | c2<<8);
        c1 = *p++; c2 = *p++; height = (c1 | c2<<8);

        while ((tag = *p++) != '\0')
        {
            switch (tag & 0xF)
            {
                case RULE:
                    p += RULEFLEN - 1;
                    break;

                case BULLET:
                    p += BULLETFLEN - 1;
                    break;

                case STRING:
                    emph = *p++;
                    c1 = *p++; c2 = *p++; x1 = xi + (c1 | c2<<8);  /* here */
                    c1 = *p++; c2 = *p++; len = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; width = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; str = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; str |= (c1 | c2<<8) << 16;
                    fnt = emph & 0xF;
                    height = LineSpacing[fnt];
                    y2 = yb - ASCENT(fnt) - 1;

                    if ((emph & EMPH_ANCHOR) && x1 <= x && y2 <= y &&
                        x < x1 + width && y < y2 + height)
                    {
                        s = (char *)str;

                        while (s > buffer)
                        {
                            --s;

                            if (s[0] == '<' && tolower(s[1]) == 'a' && s[2] <= ' ')
                                break;
                        }

                        *start = s;    /* used in next stage */
                        s = (char *)str;

                        while (strncasecmp(s, "</a>", 4) != 0)
                        {
                            if (*s == '\0')
                                break;

                            ++s;
                        }

                        *end = s;    /* used in next stage */
                        *type = TAG_ANCHOR;

                        /* return pointer to start of anchor object */
                        return p - STRINGFLEN;
                    }
                    break;

                case SEQTEXT:
                    ++p; ++p;  /* skip over x position */
                    len = *p++;
                    p += len;
                    break;

                case IMAGE:
                    c1 = *p++; c2 = *p++; y2 = yb - (c1 | c2<<8);
                    c1 = *p++; c2 = *p++; x1 = xi + (c1 | c2<<8);  /* here */
                    c1 = *p++; c2 = *p++; width = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; height = c1 | c2<<8;
                    
                    if ((tag & (ISMAP|EMPH_ANCHOR)) && x1 <= x && y2 <= y &&
                        x < x1 + width && y < y2 + height)
                    {
                        p += 4;  /* past pixmap */
                        c1 = *p++; c2 = *p++; str = c1 | c2<<8;
                        c1 = *p++; c2 = *p++; str |= (c1 | c2<<8) << 16;

                        *dx = x - x1;
                        *dy = y - y2;
                        *start = (char *)str;
                        *end = (char *)str;
                        *type = TAG_IMG;
                        return p - IMAGEFLEN;
                    }
                    else
                        p += 8;  /* past pixmap and buf pointer */
                    break;

                case INPUT:
                    c1 = *p++; c2 = *p++; str = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; str |= (c1 | c2<<8) << 16;

                    if (ClickedInField(disp_gc, yb, (Field *)str, x, y, event))
                    {
                        *type = TAG_INPUT;
                        return (p - INPUTFLEN);
                    }
                    break;

                default:
                    fprintf(stderr, "Unexpected tag: %d\n", tag);
                    exit(1);
            }
        }

        ++p; ++p;  /* skip final frame length field */
    }

    return NULL;
}

/* find which hypertext button contains given point */
Byte *WhichObj(Frame *frame, int event, int x, int y,
               int *type, char **start, char **end, int *dx, int *dy)
{
    Byte *p, *q;
    Frame *peer;

    if (frame->child)
    {
        p = WhichObj(frame->child, event, x, y, type, start, end, dx, dy);

        if (p)
            return p;
    }

    for (peer = frame->next; peer; peer = peer->next)
    {
        p = WhichObj(peer, event, x, y, type, start, end, dx, dy);

        if (p)
            return p;
    }

    p = frame->top;
    q = paint + frame->info + FRAMESTLEN + frame->length;
    return WhichFrameObject(event, x, y, p, q, type, start, end, dx, dy);
}

/* find which hypertext button contains given point */
Byte *WhichObject(int event, int x, int y,
               int *type, char **start, char **end, int *dx, int *dy)
{
    if (focus && focus->type == OPTIONLIST &&
                focus->flags & CHECKED &&
                ClickedInDropDown(disp_gc, focus, event, x, y))
    {
        *type = TAG_SELECT;
        *start = *end = NULL;
        return paint + focus->object;
    }

    return WhichObj(&background, event, x, y, type, start, end, dx, dy);
}

/* drawn anchors in designated state */
void DrawFrameAnchor(int up, Byte *start, Byte *end)
{
    char *s;
    Byte *p;
    unsigned int tag, len, emph, c1, c2;
    int action, fnt, x1, y1, xi, y2, yb, width, height;
    int style, border, length;
    long offset, str;

    displayRect.x = WinLeft;
    displayRect.y = WinTop;
    displayRect.width = WinWidth;
    displayRect.height = WinHeight;
    XSetClipRectangles(display, disp_gc, 0, 0, &displayRect, 1, Unsorted);

    for (p = start; p < end;)
    {
        tag = *p++;

        if (tag == BEGIN_FRAME)
        {
           c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
            c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;

         /* we are done if frame starts after bottom of window */

            y1 = WinTop + (offset - PixelOffset);

            if (y1 >= WinBottom)
                break;

         /* otherwise pickup frame header params */

            c1 = *p++; c2 = *p++; x1 = c1 | c2<<8;
            c1 = *p++; c2 = *p++; width = c1 | c2<<8;
            c1 = *p++; c2 = *p++; height = c1 | c2<<8;
            c1 = *p++; c2 = *p++; height |= (c1 | c2<<8) << 16;
            style = *p++; border = *p++;
            c1 = *p++; c2 = *p++; length = c1 | c2<<8;

         /* call self to draw anchors in this frame */

            DrawFrameAnchor(up, p, p + length);
            p += length+2; /* to skip over frame's contents */
            continue;
        }

        if (tag == END_FRAME)
        {
            p += 4;
            continue;
        }

        if (tag != TEXTLINE)
        {
            fprintf(stderr, "Unexpected tag: %d\n", tag);
            exit(1);
        }

        c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
        c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;

        /* y1 points to top of line */
        y1 = WinTop + (offset - PixelOffset);

        if (y1 > (int) WinBottom)
            break;
        c1 = *p++; c2 = *p++; yb = y1 + (c1 | c2<<8);
        c1 = *p++; c2 = *p++; xi = (c1 | c2<<8) - PixelIndent;
        c1 = *p++; c2 = *p++; height = (c1 | c2<<8);

        while ((tag = *p++) != '\0')
        {
            switch (tag & 0xF)
            {
                case RULE:
                    p += RULEFLEN - 1;
                    break;

                case BULLET:
                    p += BULLETFLEN - 1;
                    break;

                case STRING:
                    emph = *p++;
                    c1 = *p++; c2 = *p++; x1 = xi + (c1 | c2<<8);  /* here */
                    c1 = *p++; c2 = *p++; len = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; width = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; str = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; str |= (c1 | c2<<8) << 16;
                    fnt = emph & 0xF;

                    if (font != fnt)
                    {
                        font = fnt;
                        SetFont(disp_gc, font);
                    }

                    height = LineSpacing[font];
                    y2 = yb - ASCENT(font) - 1;

                    if (emph & EMPH_ANCHOR) 
                    {
                        s = (char *)str;

                        if (anchor_start <= s && s <= anchor_end)
                        {
                            if (up)
                                DrawHButtonUp(x1, y2, width, height);
                            else
                                DrawHButtonDown(x1, y2, width, height);
                        }
                    }
                    break;

                case SEQTEXT:
                    ++p; ++p;  /* skip over x position */
                    len = *p++;
                    p += len;
                    break;

                case IMAGE:
                    c1 = *p++; c2 = *p++; y2 = yb - (c1 | c2<<8);
                    c1 = *p++; c2 = *p++; x1 = xi + (c1 | c2<<8);  /* here */
                    c1 = *p++; c2 = *p++; width = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; height = c1 | c2<<8;
                    p += 4;  /* past pixmap id */
                    c1 = *p++; c2 = *p++; str = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; str |= (c1 | c2<<8) << 16;
                    s = (char *)str;

                    if (tag & ISMAP && anchor_start == s)
                    {
                        x1 -= PixelIndent;

                        if (up)
                        {
                            DrawOutSet(disp_gc, x1, y2, width, height);
                            XSetForeground(display, disp_gc, textColor);
                            DrawHButtonDown(x1+4, y2+2, width-7, height-6);
                        }
                        else
                        {
                            DrawInSet(disp_gc, x1, y2, width, height);
                            XSetForeground(display, disp_gc, textColor);
                            DrawHButtonUp(x1+4, y2+2, width-7, height-6);
                        }

                        width -= 8;
                        height -= 8;
                        x1 += 4;
                        y2 += 4;
                    }
                    break;

                case INPUT:
                    p += INPUTFLEN - 1;
                    break;

                default:
                    fprintf(stderr, "Unexpected tag: %d\n", tag);
                    exit(1);
            }
        }

        ++p; ++p;  /* skip final frame length field */
    }

    XFlush(display);
}

void DrawAnchor(Frame *frame, int state)
{
    Byte *start, *end;
    Frame *peer;

    if (frame->child)
        DrawAnchor(frame->child, state);

    for (peer = frame->next; peer; peer = peer->next)
        DrawAnchor(peer, state);

    start = frame->top;
    end = paint + frame->info + FRAMESTLEN + frame->length;
    DrawFrameAnchor(state, start, end);
}

/* find topmost object's pointer to the html buffer */
char *TopStrSelf(Byte *start, Byte *end)
{
    Byte *p;
    char *s;
    unsigned int tag, len, emph, c1, c2;
    int style, border, length, action, x1, y1, xi, yb, y2, width, height;
    long offset, str;

    for (p = start; p < end;)
    {
        tag = *p++;

        if (tag == BEGIN_FRAME)
        {
            c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
            c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;
            c1 = *p++; c2 = *p++; x1 = c1 | c2<<8;
            c1 = *p++; c2 = *p++; width = c1 | c2<<8;
            c1 = *p++; c2 = *p++; height = c1 | c2<<8;
            c1 = *p++; c2 = *p++; height |= (c1 | c2<<8) << 16;
            style = *p++; border = *p++;
            c1 = *p++; c2 = *p++; length = c1 | c2<<8;

            if ((s = TopStrSelf(p, p + length)))
                return s;

            p += length+2; /* to skip over frame's contents */
            continue;
        }

        if (tag == END_FRAME)
        {
            p += 4;
            continue;
        }

        if (tag != TEXTLINE)
        {
            fprintf(stderr, "Unexpected tag: %d\n", tag);
            exit(1);
        }

        c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
        c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;

        /* y1 points to top of line */
        y1 = WinTop + (offset - PixelOffset);

        if (y1 > (int) WinBottom)
            break;
        c1 = *p++; c2 = *p++; yb = y1 + (c1 | c2<<8);
        c1 = *p++; c2 = *p++; xi = (c1 | c2<<8);
        c1 = *p++; c2 = *p++; height = (c1 | c2<<8);

        while ((tag = *p++) != '\0')
        {
            switch (tag & 0xF)
            {
                case RULE:
                    p += RULEFLEN - 1;
                    break;

                case BULLET:
                    p += BULLETFLEN - 1;
                    break;

                case STRING:
                    emph = *p++;
                    c1 = *p++; c2 = *p++; x1 = xi + (c1 | c2<<8);  /* here */
                    c1 = *p++; c2 = *p++; len = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; width = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; str = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; str |= (c1 | c2<<8) << 16;
                    return (char *)str;

                case SEQTEXT:
                    ++p; ++p;  /* skip over x position */
                    len = *p++;
                    p += len;
                    break;

                case IMAGE:
                    p += IMAGEFLEN - 1;
                    break;

                case INPUT:
                    p += INPUTFLEN - 1;
                    break;

                default:
                    fprintf(stderr, "Unexpected tag: %d\n", tag);
                    exit(1);
            }
        }

        ++p; ++p;  /* skip final frame length field */
    }

    return NULL;
}

char *TopStr(Frame *frame)
{
    Frame *peer;
    char *s;
    Byte *start, *end;

    if (frame->child && (s = TopStr(frame->child)))
        return s;

    for (peer = frame->next; peer; peer = peer->next)
        if ((s = TopStr(peer)))
            return s;

    start = frame->top;
    end = paint + frame->info + FRAMESTLEN + frame->length;
    return TopStrSelf(start, end);
}

void ClipToWindow(void)
{
    displayRect.x = WinLeft;
    displayRect.y = WinTop;
    displayRect.width = WinWidth;
    displayRect.height = WinHeight;
    XSetClipRectangles(display, disp_gc, 0, 0, &displayRect, 1, Unsorted);
}

void DrawBorder(int border, int x, int y, unsigned int w, unsigned int h)
{
    XFillRectangle(display, win, disp_gc, x, y, w, 1);
    XFillRectangle(display, win, disp_gc, x+w, y, 1, h);
    XFillRectangle(display, win, disp_gc, x, y+h, w, 1);
    XFillRectangle(display, win, disp_gc, x, y, 1, h);
}

/* free frame structures except for background frame */
void FreeFrames(Frame *frame)
{
    Frame *peer;

    if (frame)
    {
        FreeFrames(frame->child);

        for (peer = frame->next; peer; peer = peer->next)
             FreeFrames(peer);

        free(frame);
    }
}

void PaintSelf(Frame *frame, int y, unsigned int h);
void PaintPeers(Frame *frame, int y, unsigned int h);
void PaintFrame(unsigned char *p, unsigned char *p_end, int y, unsigned int h);

void DisplayHTML(int x, int y, unsigned int w, unsigned int h)
{
    if (UsePaper)
       XSetTSOrigin(display, gc_fill,
            -PixelIndent % tileHeight, -(int)(PixelOffset % tileHeight));

 /* make absolutely certain we don't overwrite the scrollbar */

    if (w > WinWidth)
        w = WinWidth;

    if (y < WinTop)
    {
        h -= (WinTop - y);
        y = WinTop;

        if (h <= 0)
            return;
    }

 /* make absolutely certain we don't overwrite the status bar */

    if (y + h > WinBottom)
    {
        h = WinBottom - y;

        if (h <= 0)
            return;
    }

 /* the text must be clipped to avoid running over into adjacent
    regions, i.e. the scrollbar at the rhs */

    displayRect.x = x;
    displayRect.y = y;
    displayRect.width = w;
    displayRect.height = h;
    XSetClipRectangles(display, disp_gc, 0, 0, &displayRect, 1, Unsorted);
    XSetClipRectangles(display, gc_fill, 0, 0, &displayRect, 1, Unsorted);

 /* fill background with texture */
    XFillRectangle(display, win, gc_fill, x, y, w, h);

 /* and paint all frames intersecting top of window */

    PaintSelf(&background, y, h);

    if (focus && focus->type == OPTIONLIST && focus->flags & CHECKED)
        PaintDropDown(disp_gc, focus);
}

/* paint children then self - first called for background frame */
void PaintSelf(Frame *frame, int y, unsigned int h)
{
    long y1;
    unsigned char *p1, *p2;

    /* Test that this frame is at least partly visible */

    y1 = PixelOffset + y - WinTop;   /* pixel offset for screen coord y */

    if (frame->offset < y1 + h && frame->offset + frame->height > y1)
    {
        if (frame->child)
            PaintPeers(frame->child, y, h);

        if (frame->border)
        {
            DrawBorder(frame->border, frame->indent-PixelIndent,
                               WinTop + (frame->offset - PixelOffset),
                               frame->width, frame->height);
        }

        p1 = frame->top;
        p2 = paint + frame->info + FRAMESTLEN + frame->length;
        PaintFrame(p1, p2, y, h);
    }
}

/* paint list of peer frames */
void PaintPeers(Frame *frame, int y, unsigned int h)
{
    while (frame)
    {
        PaintSelf(frame, y, h);
        frame = frame->next;
    }
}

/*
    p, p_end point to paint buffer while x,y,w,h define region to paint

    This routine recursively calls itself to paint nested frames when
    it comes across the BEGIN_FRAME tag. Note that the border for the
    current frame is already drawn - and saves having to pass the
    relevant params to this routine.
*/
void PaintFrame(unsigned char *p, unsigned char *p_end, int y, unsigned int h)
{
    char *s;
    unsigned int tag, len, c1, c2, width;
    int action, active, fnt, x1, y1, x2, y2, emph, xi, yb, depth;
    int style, border, length;
    long offset, height, str;

    while (p < p_end)
    {
        if (p >= paint + paintlen)
        {
            fprintf(stderr, "Panic: ran off end of paint buffer!\n");
            exit(1);
        }

        tag = *p++;

        if (tag == BEGIN_FRAME)
        {
            c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
            c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;

         /* we are done if frame starts after bottom of window */

            y1 = WinTop + (offset - PixelOffset);
        
            if (y1 >= y + (int)h)
                break;

         /* otherwise pickup frame header params */

            c1 = *p++; c2 = *p++; x1 = c1 | c2<<8;
            c1 = *p++; c2 = *p++; width = c1 | c2<<8;
            c1 = *p++; c2 = *p++; height = c1 | c2<<8;
            c1 = *p++; c2 = *p++; height |= (c1 | c2<<8) << 16;
            style = *p++; border = *p++;
            c1 = *p++; c2 = *p++; length = c1 | c2<<8;

            if (border)
                DrawBorder(border, x1-PixelIndent, y1, width, height);

         /* call self to paint this frame */

            PaintFrame(p, p + length, y, h);
            p += length+2; /* to skip over frame's contents */
            continue;
        }

        /* skip end of frame marker */
        if (tag == END_FRAME)
        {
            p += 4;  /* skip start/size params */
            continue;
        }

        if (tag != TEXTLINE)
        {
            fprintf(stderr, "Unexpected tag: %d\n", tag);
            exit(1);
        }

        c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
        c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;

        /* we are done if TextLine starts after bottom of window */

        y1 = WinTop + (offset - PixelOffset);

        if (y1 >= y + (int)h)
            break;

        c1 = *p++; c2 = *p++; yb = y1 + (c1 | c2<<8);
        c1 = *p++; c2 = *p++; xi = (c1 | c2<<8);
        c1 = *p++; c2 = *p++; height = (c1 | c2<<8);

        while ((tag = *p++) != '\0')
        {
            switch (tag & 0xF)
            {
                case RULE:
                    c1 = *p++; c2 = *p++; x1 = xi + (c1 | c2<<8);
                    c1 = *p++; c2 = *p++; x2 = c1 | c2<<8;
                    XSetForeground(display, disp_gc, windowBottomShadow);
                    XFillRectangle(display, win, disp_gc, x1-PixelIndent, yb, x2-x1, 1);
                    XSetForeground(display, disp_gc, windowTopShadow);
                    XFillRectangle(display, win, disp_gc, x1-PixelIndent, yb+1, x2-x1, 1);
                    XSetForeground(display, disp_gc, textColor);
                    break;

                case BULLET:
                    c1 = *p++; c2 = *p++; x1 = xi + (c1 | c2<<8);
                    c1 = *p++; c2 = *p++; depth = (c1 | c2<<8);
                    fnt = (tag & 0xF0) >> 4;

                    if (font != fnt)
                    {
                        font = fnt;
                        SetFont(disp_gc, font);
                    }

                    if (depth > 0)
                        XFillRectangle(display, win, disp_gc, x1-PixelIndent, yb-BSIZE, BSIZE, 2);
                    else
                        XFillRectangle(display, win, disp_gc, x1-PixelIndent, yb-BSIZE, BSIZE, BSIZE);
                    break;

                case STRING:
                    emph = *p++;
                    c1 = *p++; c2 = *p++; x1 = xi + (c1 | c2<<8);  /* here */
                    c1 = *p++; c2 = *p++; len = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; width = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; str = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; str |= (c1 | c2<<8) << 16;

                    preformatted = (tag & PRE_TEXT);
                    s = CopyLine((char *)str, len);
                    fnt = emph & 0xF;

                    if (font != fnt)
                    {
                        font = fnt;
                        SetFont(disp_gc, font);
                    }

                    if (emph & EMPH_HIGHLIGHT)
                    {
                        XSetForeground(display,disp_gc, labelColor);
                        XDrawString(display, win, disp_gc, x1-PixelIndent, yb, s, len);
                        XSetForeground(display,disp_gc, textColor);
                    }
                    else
                        XDrawString(display, win, disp_gc, x1-PixelIndent, yb, s, len);

                    if (emph & EMPH_ANCHOR)
                    {
                        y2 = yb - ASCENT(font) - 1;
                        DrawHButtonUp(x1-PixelIndent, y2, width, LineSpacing[fnt]);
                    }

                    if (emph & EMPH_UNDERLINE)
                    {
                        y2 = yb + DESCENT(font);
                        XFillRectangle(display, win, disp_gc, x1+1-PixelIndent, y2, width-2, 1);
                    }

                    if (emph & EMPH_STRIKE)
                    {
                        y2 = yb - ASCENT(font)/3;
                        XSetForeground(display,disp_gc, strikeColor);
                        XFillRectangle(display, win, disp_gc, x1-PixelIndent, y2, width, 1);
                        XSetForeground(display,disp_gc, textColor);
                    }
                    break;

                case SEQTEXT:
                    c1 = *p++; c2 = *p++; x1 = xi + (c1 | c2<<8);  /* here */
                    len = *p++;
                    XDrawString(display, win, disp_gc, x1-PixelIndent, yb, (char *)p, len);
                    p += len;
                    break;

                case IMAGE:
                    c1 = *p++; c2 = *p++; y2 = yb - (c1 | c2<<8);
                    c1 = *p++; c2 = *p++; x1 = xi + (c1 | c2<<8);  /* here */
                    c1 = *p++; c2 = *p++; width = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; height = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; str = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; str |= (c1 | c2<<8) << 16;
                    p += 4;  /* skip past buffer pointer */

                    if (y2 > y + (int)h)
                        break;

                    if (y2 + height < y)
                        continue;

                    x1 -= PixelIndent;

                    if (tag & ISMAP)
                    {
                        DrawOutSet(disp_gc, x1, y2, width, height);
                        XSetForeground(display, disp_gc, textColor);
                        DrawHButtonDown(x1+4, y2+2, width-7, height-6);
                        width -= 8;
                        height -= 8;
                        x1 += 4;
                        y2 += 4;
                    }

                    XCopyArea(display, (Pixmap)str, win, disp_gc,
                        0, 0, width, height, x1, y2);
                    break;

                case INPUT:
                    c1 = *p++; c2 = *p++; str = c1 | c2<<8;
                    c1 = *p++; c2 = *p++; str |= (c1 | c2<<8) << 16;
                    PaintField(disp_gc, yb, (Field *)str);
                    break;
            }
        }

        ++p; ++p;  /* skip size param to start of next object */
    }
}

/* search for anchor point in document */
/* search index for given keywords */

void SearchIndex(char *keywords)
{
    char *p, *q;
    int where;

    p = SearchRef(keywords);

    if (*p)  /* attempt to get new document */
    {
        XDefineCursor(display, win, hourglass);
        XFlush(display);

        q = GetDocument(p, NULL, REMOTE);

        if (q && *q)
        {
            CurrentDoc.offset = PixelOffset;
            PushDoc(CurrentDoc.offset);
            SetBanner("World Wide Web Browser");

            SetCurrent();
            NewBuffer(q);

            if (IsIndex)
                ClearStatus();

            SetStatusString(NULL);  /* to refresh screen */
        }

        DisplayScrollBar();
        DisplayDoc(WinLeft, WinTop, WinWidth, WinHeight);
        XUndefineCursor(display, win);
        XFlush(display);
    }
}

int WindowButtonDown(int x, int y)
{
    int tag, dx, dy;

    anchor_start = anchor_end = 0;
    tag = 0;

    clicked_element = WhichObject(BUTTONDOWN, x, y, &tag, &anchor_start, &anchor_end, &dx, &dy);

    if (tag == TAG_ANCHOR || tag == TAG_IMG)
    {
        DrawAnchor(&background, 0);
        return WINDOW;
    }

    if (tag == TAG_INPUT || TAG_SELECT)
        return WINDOW;

    Beep();
    return VOID;
}

void WindowButtonUp(int shifted, int px, int py)
{
    Byte *object;
    char *start, *end, *href, *name, *link, buf[16];
    int tag, hreflen, namelen, align, ismap, dx, dy;

    if (anchor_start && anchor_end)
        DrawAnchor(&background, 1);

    object = WhichObject(BUTTONUP, px, py, &tag, &start, &end, &dx, &dy);

    if ((tag == TAG_ANCHOR || tag == TAG_IMG) &&
                start == anchor_start && end == anchor_end)
    {
        if (tag == TAG_IMG)
        {
            bufptr = anchor_start+5;
            ParseImageAttrs(&href, &hreflen, &align, &ismap);
            sprintf(buf, "?x=%d,y=%d", dx, dy);
            link = (char *)malloc(hreflen+strlen(buf)+1);
            memcpy(link, href, hreflen);
            link[hreflen] = '\0';
            strcat(link, buf);
        }
        else /* tag == TAG_ANCHOR */
        {
            bufptr = anchor_start+3;
            ParseAnchorAttrs(&href, &hreflen, &name, &namelen);
            link = (char *)malloc(hreflen+1);
            memcpy(link, href, hreflen);
            link[hreflen] = '\0';
        }

        if (shifted)
        {
            if (CloneSelf())
                OpenDoc(link, NULL, REMOTE);
        }
        else
            OpenDoc(link, NULL, REMOTE);

        free(link);
    }

    XFlush(display);
}
