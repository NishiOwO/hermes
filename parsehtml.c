/* parsehtml.c - display code for html

ParseHTML() parses the HTML elements and generates a stream of commands for
displaying the document in the Paint buffer. The paint commands specify the
appearence of the document as a sequence of text, lines, and images. Each
command includes the position as a pixel offset from the start of the
document. This makes it easy to scroll efficiently in either direction.
The paint buffer must be freed and recreated if the window is resized.

The model needs to switch to relative offsets to enable the transition
to an wysiwyg editor for html+. Relative values for pixel offsets and
pointers to the html+ source would make it much easier to edit documents
as it would limit revisions to the paint stream to the region changed.

*/

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <string.h>
#include <ctype.h>
#include "www.h"

#define LBUFSIZE 1024

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
extern Frame background;

char *bufptr;  /* parse position in the HTML buffer */
char *lastbufptr;  /* keep track of last position to store delta's */

Byte *TopObject;  /* first visible object in window */
Byte *paint; /* holds the sequence of paint commands */
int paintbufsize;     /* size of buffer, not its contents */
int paintlen;         /* where to add next entry */

int paintStartLine;   /* where line starts in the paint stream */
int above;            /* above baseline */
int below;            /* below baseline */

int error;            /* set by parser */
int prepass;          /* true during table prepass */
int html_width;       /* tracks maximum width */
int min_width, max_width; /* table cell width */
int list_indent;

extern int preformatted;
extern int font;  /* index into Fonts[] array */

static int EndTag, TagLen;
static int TokenClass, TokenValue, Token;
static char *EntityValue;

unsigned int ui_n;
int baseline;       /* from top of line */
long TermTop, TermBottom;

long PixOffset;     /* current offset from start of document */
long PrevOffset;    /* keep track for saving delta's */
long LastLIoffset;  /* kludge for <LI><LI> line spacing */
long ViewOffset;    /* for toggling between HTML/TEXT views */

extern long IdOffset;      /* offset for targetId */
extern char *targetptr;    /* for toggling view between HTML/TEXT views */
extern char *targetId;     /* for locating named Id during ParseHTML() */

int Here;
int HTMLInit = 0;
Image *start_figure, *figure;
long figEnd;
Form *form;

char *LastBufPtr, *StartOfLine, *StartOfWord; /* in HTML document */
static int LineLen, LineWidth, WordStart, WordWidth;
static char LineBuf[LBUFSIZE]; /* line buffer */

static char *Ones[] = {"i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix"};
static char *Tens[] = {"x", "xx", "xxx", "xl", "l", "lx", "lxx", "lxxx", "xc"};
static char *Hundreds[] = {"c", "cc", "ccc", "cd", "d", "dc", "dcc", "dccc", "cm"};

/* push 16 bit value onto paint buffer */

#define PushValue(p, value) ui_n = (unsigned int)value; *p++ = ui_n & 0xFF; *p++ = (ui_n >> 8) & 0xFF

/* expand paint stream to fit len bytes */

Byte *MakeRoom(int len)
{
    Byte *p;

    if (paintlen > paintbufsize - len)
    {
        paintbufsize = paintbufsize << 1;
        paint = (Byte *)realloc(paint, paintbufsize);
    }

    p = paint + paintlen;
    paintlen += len;
    return p;
}

/* insert figure frame */

#if 0
void PrintFigure(int tag)
{
    unsigned int indent;
    Byte *p;

    figure = start_figure;
    start_figure = 0;
    indent = MININDENT;

    if (indent + figure->width > html_width)
        html_width = indent + figure->width;

    ++PixOffset;

    if (!prepass)
    {
        p = MakeRoom(17);
        *p++ = tag;
        PushValue(p, PixOffset & 0xFFFF);
        PushValue(p, (PixOffset >> 16) &0xFFFF);
        PushValue(p, indent);
        PushValue(p, (figure->width));
        PushValue(p, (figure->height));
        PushValue(p, ((unsigned long)(figure->pixmap) & 0xFFFF));
        PushValue(p, (((unsigned long)(figure->pixmap) >> 16) &0xFFFF));
        PushValue(p, 15);  /* framelen */
    }

    figEnd = PixOffset + figure->height + 1;
}

/* EndFigure is currently used to see if text
   lines need to be flowed around the figure */

int EndFigure(void)
{
    int indent;

    if (PixOffset > figEnd)
    {
        figure = 0;
        return 0;
    }

    return figure->width + 4;
}

#endif /* figure code left out during re-org */

void ShowFrame(int n)
{
    Byte *p, *obj;
    unsigned int tag, c1, c2, size, baseline, len,
             indent, width, length, style, border;
    long offset, height;

    obj = p = paint + n;

    if (*p++ == BEGIN_FRAME)
    {
        c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
        c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;
        c1 = *p++; c2 = *p++; indent = c1 | c2<<8;
        c1 = *p++; c2 = *p++; width = c1 | c2<<8;
        c1 = *p++; c2 = *p++; height = c1 | c2<<8;
        c1 = *p++; c2 = *p++; height |= (c1 | c2<<8) << 16;
        style = *p++; border = *p++;
        c1 = *p++; c2 = *p++; length = c1 | c2<<8;

        printf("[%d] Frame:\n", obj-paint);
        printf("  offset = %ld\n", offset);
        printf("  indent = %d\n", indent);
        printf("  width  = %d\n", width);
        printf("  height = %ld\n", height);
        printf("  length = %d\n", length);

        /* check size field is ok */
        p += length;     /* skip over frame contents */
        c1 = *p++; c2 = *p++; size = c1 | c2<<8;

        if (p - 2 - size != obj)
            printf("**** bad size field found %d when %d expected\n", size, p-2-obj);
    }
    else
        printf("Not start of frame %d\n", *obj);
}

void ShowPaint(int npaint, int nobjs)
{
    Byte *p, *obj;
    unsigned int tag, c1, c2, size, baseline, len,
             indent, width, length, style, border;
    long offset, height;
    int i;

    printf("\n");
    p = paint + npaint;

    while (nobjs-- > 0)
    {
        obj = p;

        switch(*p++)
        {
            case BEGIN_FRAME:
                c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
                c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;
                c1 = *p++; c2 = *p++; indent = c1 | c2<<8;
                c1 = *p++; c2 = *p++; width = c1 | c2<<8;
                c1 = *p++; c2 = *p++; height = c1 | c2<<8;
                c1 = *p++; c2 = *p++; height |= (c1 | c2<<8) << 16;
                style = *p++; border = *p++;
                c1 = *p++; c2 = *p++; length = c1 | c2<<8;

                printf("[%d] Frame:offset = %ld, indent = %d, height = %ld\n", obj-paint, offset, indent, height);

                /* check size field is ok */
                p += length;     /* skip over frame contents */
                c1 = *p++; c2 = *p++; size = c1 | c2<<8;
                if (p - 2 - size != obj)
                      printf("**** bad size field found %d when %d expected\n", size, p-2-obj);
                break;

            case END_FRAME:
                c1 = *p++; c2 = *p++; length = c1 | c2<<8;
                c1 = *p++; c2 = *p++; size = c1 | c2<<8;
                p = obj - length;

                if (*p++ != BEGIN_FRAME)
                {
                    fprintf(stderr, "Unexpected tag: %d when BEGIN_FRAME was expected\n", tag);
                    exit(1);
                }

                c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
                c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;
                c1 = *p++; c2 = *p++; indent = c1 | c2<<8;
                c1 = *p++; c2 = *p++; width = c1 | c2<<8;
                c1 = *p++; c2 = *p++; height = c1 | c2<<8;
                c1 = *p++; c2 = *p++; height |= (c1 | c2<<8) << 16;
                printf("[%d] EndFrame: %ld (%ld), indent %d\n", obj-paint, offset + height, offset, indent);
                p = obj + FRAMENDLEN;
                break;

            case TEXTLINE:
                c1 = *p++; c2 = *p++; offset = c1 | c2<<8;
                c1 = *p++; c2 = *p++; offset |= (c1 | c2<<8) << 16;
                c1 = *p++; c2 = *p++; baseline = c1 | c2<<8;
                c1 = *p++; c2 = *p++; indent = c1 | c2<<8;
                c1 = *p++; c2 = *p++; height = c1 | c2<<8;

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

                c1 = *p++; c2 = *p++; size = c1 | c2<<8;
                printf("[%d] TextLine offset = %ld, indent = %d, height = %ld\n", obj-paint, offset, indent, height);
#if 0
                printf("  offset = %ld\n", offset);
                printf("  indent = %d\n", indent);
                printf("  size = %d\n", size);
#endif
                break;
        }
    }
}

void PrintBeginFrame(Frame *frame)
{
    Byte *p;
    unsigned int len;
    long offset;

    if (!prepass)
    {
        p = MakeRoom(FRAMESTLEN);
        frame->info = (p - paint);
        offset = frame->offset;
        *p++ = BEGIN_FRAME;
        PushValue(p, offset & 0xFFFF);
        PushValue(p, (offset >> 16) & 0xFFFF);
        PushValue(p, frame->indent);
        PushValue(p, frame->width);
        PushValue(p, 0);        /* subsequently filled in with height(1) */
        PushValue(p, 0);        /* subsequently filled in with height(2) */
        *p++ = frame->style;    /* frame's background style */
        *p++ = frame->border;   /* frame's border style */
        PushValue(p, 0);        /* subsequently filled in with length */
    }
}

/* the size field after end of frame contents */
void PrintFrameLength(Frame *frame)
{
    Byte *p;
    unsigned int len;

    if (!prepass)
    {
        if (frame->info == 287)
            len = 1;

     /* write the length field in frame's header */
        p = paint + frame->info + 15;
        PushValue(p, frame->length);

     /* write the size field after frame's contents */
        p = MakeRoom(2);
        len = p - paint - frame->info;
        PushValue(p, len);
    }
}

/* marker for pixel offset to end of frame */
void PrintEndFrame(Frame *frame)
{
    Byte *p;
    unsigned int len;

    if (!prepass)
    {
        p = MakeRoom(FRAMENDLEN);
        len = p - (paint + frame->info);
        *p++ = END_FRAME;
        PushValue(p, len);
        PushValue(p, FRAMENDLEN-2);
    }
}

/* flush frames with offset <= PixOffset
 Note that nested frames need flushing at
 the end of the enclosing frame. This code
 needs to take into account nesting level. */

void FlushFrames(int all, Frame **frames)
{
    Frame *prev, *frame, *next;

    prev = NULL;
    frame = *frames;

    while (frame)
    {
        if (all || frame->offset <= PixOffset)
        {
            PrintEndFrame(frame);
            next = frame->next;

            if (prev)
                prev->next = next;
            else
                *frames = next;

            free(frame);
            frame = next;
            continue;
        }

        prev = frame;
        frame = frame->next;
    }
}

/* The frame is created here, the new frame is
   returned so that the parser can later call
   EndFrame at the end of the frame. Any frames
   which end before PixOffset are first flushed */

Frame *BeginFrame(Frame **frames, int style, int border, int left, int right)
{
    Frame *frame;

    FlushFrames(0, frames); /* flush pending end of frames */

    frame = (Frame *)malloc(sizeof(Frame));
    memset(frame, 0, sizeof(Frame));
    frame->next = NULL;
    frame->offset = PixOffset;
    frame->indent = left;
    frame->width = right - left;
    frame->style = style;
    frame->border = border;
    PrintBeginFrame(frame);
    return frame;
}

/* This writes the frame's height in the frame's header.
   The END_FRAME marker is output later on, here we need
   to insert the frame in the right place in the list
   according to the offset of the end of the frame */
void EndFrame(Frame **frames, Frame *frame)
{
    Byte *p;
    Frame *prev, *next;

 /* write height into paint struct for frame */
    p = paint + frame->info + 9;
    PushValue(p, frame->height & 0xFFFF);
    PushValue(p, (frame->height >> 16) & 0xFFFF);
    frame->offset = PixOffset;  /* changed to end of frame */

    if (*frames == NULL)
        *frames = frame;
    else if ((*frames)->offset > PixOffset)
    {
        frame->next = *frames;
        *frames = frame;
    }
    else
    {
        for (prev = *frames;;)
        {
            next = prev->next;

            if (next == NULL)
            {
                prev->next = frame;
                break;
            }

            if (next->offset > PixOffset)
            {
                frame->next = next;
                prev->next = frame;
                break;
            }

            prev = next;
        }
    }
}

int ListCells(Frame *cells)
{
    int n;

    for (n = 0; cells != NULL; ++n)
    {
        printf("address = %x, indent = %d, width = %d, height = %ld\n",
            cells, cells->indent, cells->width, cells->height);

        cells = cells ->next;
    }

    return n;
}

/*
 Insert cell at end of list of cells
*/
InsertCell(Frame **cells, Frame *cell)
{
    Frame *frame, *next;

    frame = *cells;
    cell->next = NULL;

    if (frame == NULL)
        *cells = cell;
    else
    {
        for (frame = *cells;;)
        {
            next = frame->next;

            if (next == NULL)
            {
                frame->next = cell;
                break;
            }

            frame = next;
        }
    }
}

/*
 This routine adjusts height of all table cells which end
 on this row and then calls EndFrame() to move them to
 the list of frames awaiting PrintEndFrame()
*/

void FlushCells(Frame **frames, int row, Frame **cells)
{
    Frame *prev, *frame, *next;

    prev = NULL;
    frame = *cells;

    while (frame)
    {
        if (frame->lastrow <= row)
        {
            next = frame->next;

            if (prev)
                prev->next = next;
            else
                *cells = next;

            frame->height = PixOffset - frame->offset;
            frame->next = NULL;
            EndFrame(frames, frame);
            frame = next;
            continue;
        }

        prev = frame;
        frame = frame->next;
    }
}

/* insert TEXTLINE frame */

void TextLineFrame(Frame **frames)
{
    Byte *p;

    if (!prepass)
    {
        FlushFrames(0, frames); /* flush pending end of frames */

        p = MakeRoom(TXTLINLEN);
        paintStartLine = p - paint;
        *p++ = TEXTLINE;
    
        PushValue(p, PixOffset & 0xFFFF);
        PushValue(p, (PixOffset >> 16) & 0xFFFF);

 /* baseline & indent set at end of line by EndOfLine() */

        *p++ = 0; *p++ = 0;
        *p++ = 0; *p++ = 0;
    }
}

/* set baseline, linespacing for current line
   and then push frame length */

void EndOfLine(int indent)
{
    unsigned int n, height;
    Byte *p;

    if (paintStartLine >= 0)
    {
        /* fill in baseline for current line */

        if (!prepass)
        {
            if (indent < 0)
                indent = 0;

            height = above + below;

            paint[paintStartLine + 5] = (above & 0xFF);
            paint[paintStartLine + 6] = (above >> 8) & 0xFF;
            paint[paintStartLine + 7] = (indent & 0xFF);
            paint[paintStartLine + 8] = (indent >> 8) & 0xFF;
            paint[paintStartLine + 9] = (height & 0xFF);
            paint[paintStartLine + 10] = (height >> 8) & 0xFF;

            p = MakeRoom(3);
            *p++ = '\0';  /* push end of elements marker */

            /* and write frame length */

            n = p - paint - paintStartLine;
            PushValue(p, n);
        }

        PixOffset += above + below;
        paintStartLine = -1;
        above = 0;
        below = 0;
#if 0
        EndFigure();
#endif
    }

#if 0
    if (start_figure)
        PrintFigure(BEGIN_FIG);
#endif
}

/* push horizontal rule onto paint stream */

void PrintRule(Frame **frames, int LeftMargin, int RightMargin)
{
    Byte *p;
    int indent;

    indent = 0; /* (figure ? EndFigure() : 0); */
    LeftMargin += indent;
    RightMargin += indent;

    if (RightMargin > MAXMARGIN)
        RightMargin = MAXMARGIN;

    if (paintStartLine < 0)
        TextLineFrame(frames);

    if (RightMargin > html_width)
        html_width = RightMargin;

    above = max(above, 3);
    below = max(below, 5);

    if (!prepass)
    {
        p = MakeRoom(RULEFLEN);
        *p++ = RULE;
        PushValue(p, LeftMargin);
        PushValue(p, RightMargin);
    }
}


/* push bullet onto paint stream */

void PrintBullet(Frame **frames, int depth, int font)
{
    Byte *p;
    int indent;

    indent = Here; /* + (figure ? EndFigure() : 0); */

    if (paintStartLine < 0)
        TextLineFrame(frames);

    if (indent + BSIZE> html_width)
        html_width = indent + BSIZE;

    above = max(above, ASCENT(font));
    below = max(below, DESCENT(font));

    if (!prepass)
    {
        p = MakeRoom(BULLETFLEN);
        *p++ = BULLET;
        PushValue(p, indent);
        PushValue(p, depth);
    }
}


/* push text input field */

void PrintInputField(Frame **frames, Field *field)
{
    Byte *p;

    if (paintStartLine < 0)
        TextLineFrame(frames);

    if (field->x + field->width > html_width)
        html_width = field->x + field->width;

    if (!prepass)
    {
        p = MakeRoom(INPUTFLEN);
        field->object = p - paint;
        *p++ = INPUT;
        PushValue(p, (long)field & 0xFFFF);
        PushValue(p, ((long)field >> 16) &0xFFFF);
    }
}

/* push normal or preformatted string */
/* emph contains font and emphasis */

void PrintString(Frame **frames, int emph, char *buf, int len, int width)
{
    Byte *p;
    int font, indent;

    indent = Here; /* + (figure ? EndFigure() : 0); */

    if (paintStartLine < 0)
        TextLineFrame(frames);

    if (!prepass)
        p = MakeRoom(STRINGFLEN);

    if (indent + width > html_width)
        html_width = indent + width;

    font = emph & 0xF;
    above = max(above, ASCENT(font));
    below = max(below, DESCENT(font));

    if (!prepass)
    {
        *p++ = (preformatted ? (STRING | PRE_TEXT) : STRING);
        *p++ = emph;
        PushValue(p, indent);
        PushValue(p, len);
        PushValue(p, width);
        PushValue(p, ((unsigned long)buf & 0xFFFF));
        PushValue(p, (((unsigned long)buf >> 16) &0xFFFF));
    }
}

/* push explicit text onto paint stream */

void PrintSeqText(Frame **frames, int font, char *s)
{
    Byte *p;
    int tag, len, indent;

    indent = Here; /* + (figure ? EndFigure() : 0); */

    if (paintStartLine < 0)
        TextLineFrame(frames);

    if (!prepass)
    {
        len = strlen(s);
        p = MakeRoom(SEQTEXTFLEN(len));

        above = max(above, ASCENT(font));

        tag = SEQTEXT |((font << 4) & 0xF0);

        *p++ = tag;
        PushValue(p, indent);
        *p++ = len;
        memcpy(p, s, len);
    }
}

/* buf points to start of element in html source iff ismap is present */

void PrintImage(Frame **frames, int delta, char *buf, Pixmap image, unsigned int width, unsigned int height)
{
    Byte *p;
    int indent;

    indent = Here; /* + (figure ? EndFigure() : 0); */

    if (paintStartLine < 0)
        TextLineFrame(frames);

    if (prepass)  /* just update min/max widths */
    {
        if (width > min_width)
            min_width = width;

        if (indent + width > html_width)
            html_width = indent + width;
    }
    else
    {
        p = MakeRoom(IMAGEFLEN);

        if (indent + width > html_width)
            html_width = indent + width;

        *p++ = IMAGE | (buf ? ISMAP : 0);
        PushValue(p, delta);
        PushValue(p, indent);
        PushValue(p, width);
        PushValue(p, height);
        PushValue(p, ((unsigned long)image & 0xFFFF));
        PushValue(p, (((unsigned long)image >> 16) &0xFFFF));
        PushValue(p, ((unsigned long)buf & 0xFFFF));
        PushValue(p, (((unsigned long)buf >> 16) &0xFFFF));
    }
}

#define NOBREAK 0
#define BREAK   1

/* check if current word forces word wrap and flush line as needed */
void WrapIfNeeded(Frame **frames, int align, int emph, int font, int WrapLeftMargin, int WrapRightMargin)
{
    int WordLen, space, delta, rightMargin;
    long line;

    rightMargin = WrapRightMargin - (figure ? figure->width : 0);

    LineBuf[LineLen] = '\0';  /* debug*/
    WordLen = LineLen - WordStart;
    WordWidth = XTextWidth(Fonts[font], LineBuf+WordStart, WordLen);
    space = XTextWidth(Fonts[font], " ", 1);    /* width of a space char */
    line = LineSpacing[font];                   /* height of a line */

    if (WordWidth > min_width)      /* for tables */
        min_width = WordWidth;

    if (WordStart == 0 && Here + WordWidth > rightMargin)
    {
        /* word wider than window */
        if (WordWidth > rightMargin - WrapLeftMargin)
        {
            if (emph & EMPH_ANCHOR)
                WordWidth += 2;

            PrintString(frames, EMPH(emph, font), StartOfLine, WordLen, WordWidth);
            LineWidth = LineLen = WordStart = 0;
            StartOfLine = bufptr;
        }
        else /* wrap to next line */
        {
            LineWidth = WordWidth;
            LineLen = WordLen;
            WordStart = LineLen;
            StartOfLine = StartOfWord;
        }

        if (align == ALIGN_LEFT)
            delta = 0;
        else if (align == ALIGN_CENTER)
            delta = (rightMargin - Here - LineWidth) / 2;
        else if (align == ALIGN_RIGHT)
            delta = rightMargin - Here - LineWidth;

        Here = WrapLeftMargin;
        EndOfLine(delta);
    }
    else if (WordStart > 0 && Here + LineWidth + space + WordWidth > rightMargin)
    {
        if (emph & EMPH_ANCHOR)
            LineWidth += 2;

        PrintString(frames, EMPH(emph, font), StartOfLine, WordStart-1, LineWidth);

        if (align == ALIGN_LEFT)
            delta = 0;
        else if (align == ALIGN_CENTER)
            delta = (rightMargin - Here - LineWidth) / 2;
        else if (align == ALIGN_RIGHT)
            delta = rightMargin - Here - LineWidth;

        Here = WrapLeftMargin;
        EndOfLine(delta);
        memcpy(LineBuf, LineBuf+WordStart, WordLen);
        LineWidth = WordWidth;
        LineLen = WordLen;
        WordStart = LineLen;
        StartOfLine = StartOfWord;
    }
    else /* word will fit on end of current line */
    {
        if (WordStart > 0)
            LineWidth += space;

        if (WordWidth > 0)
            LineWidth += WordWidth;

        WordStart = LineLen;
    }
}

/* flush text in line buffer, wrapping line as needed */

void FlushLine(int linebreak, Frame **frames, int align, int emph, int font, int WrapLeftMargin, int WrapRightMargin)
{
    int WordLen, delta, rightMargin;

    if (preformatted)
    {
        WordLen = LineLen - WordStart;
        LineWidth = XTextWidth(Fonts[font], LineBuf+WordStart, WordLen);
    }
    else if (LineLen > 0)
        WrapIfNeeded(frames, align, emph, font, WrapLeftMargin, WrapRightMargin);

    if (LineLen > 0)
    {
        if (emph & EMPH_ANCHOR)
            LineWidth += 2;

        /* watch out for single space as leading spaces
           are stripped by CopyLine */

        if (LineLen > 1 || LineBuf[0] != ' ')
            PrintString(frames, EMPH(emph, font), StartOfLine, LineLen, LineWidth);

        if (linebreak)
        {
            rightMargin = WrapRightMargin - (figure ? figure->width : 0);

            if (align == ALIGN_LEFT)
                delta = 0;
            else if (align == ALIGN_CENTER)
                delta = (rightMargin - Here - LineWidth) / 2;
            else if (align == ALIGN_RIGHT)
                delta = rightMargin - Here - LineWidth;

            Here = WrapLeftMargin;
            LineWidth = LineLen = WordStart = 0;
            EndOfLine(delta);
        }
        else
        {
            Here += LineWidth;
            LineWidth = LineLen = WordStart = 0;
        }
    }
    else if (linebreak)
    {
        /* watch out for empty preformatted lines */
        if (preformatted && paintStartLine < 0)
            PixOffset += ASCENT(font) + DESCENT(font);

        if (Here > WrapLeftMargin)
        {
            rightMargin = WrapRightMargin - (figure ? figure->width : 0);
            if (align == ALIGN_LEFT)
                delta = 0;
            else if (align == ALIGN_CENTER)
                delta = (rightMargin - Here - LineWidth) / 2;
            else if (align == ALIGN_RIGHT)
                delta = rightMargin - Here - LineWidth;

            Here = WrapLeftMargin;
            EndOfLine(delta);
        }
    }

    StartOfLine = StartOfWord = bufptr;
}

/* needs to cope with > in quoted text for ' and " */
void SwallowAttributes(void)
{
    int c;

    while ((c = *bufptr) && c != '>')
    {
        ++bufptr;
    }

    if (c == '>')
        ++bufptr;
}

/*
 char *tag points to start of tag string which is terminated
 by whitespace (including EOF) or a '>' character.

 return tag code or 0 if unknown.
*/

int RecogniseTag(void)
{
    int c, len;
    char *s;

    s = bufptr;

    if (*++s == '/')
    {
        EndTag = 1;
        ++s;
    }
    else
        EndTag = 0;

  /* find end of tag to allow use of strncasecmp */

    while (isalpha(*s) || isdigit(*s))
        ++s;

    TagLen = s - bufptr;        /* how far to next char after tag name */
    len = TagLen - EndTag - 1;  /* number of chars in tag name itself */
    s -= len;
    c = tolower(*s);

    if (isalpha(c))
    {
        if (c == 'a')
        {
            if (len == 1 && strncasecmp(s, "a", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_ANCHOR;
            }

            if (len == 5 && strncasecmp(s, "added", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_ADDED;
            }

            if (len == 7 && strncasecmp(s, "address", len) == 0)
            {
                TokenClass = EN_BLOCK;
                return TAG_ADDRESS;
            }

            if (len == 8 && strncasecmp(s, "abstract", len) == 0)
            {
                TokenClass = EN_BLOCK;
                return TAG_ABSTRACT;
            }
        }
        else if (c == 'b')
        {
            if (len == 1)
            {
                TokenClass = EN_TEXT;
                return TAG_BOLD;
            }

            if (len == 2 && strncasecmp(s, "br", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_BR;
            }

            if (len == 4 && strncasecmp(s, "body", len) == 0)
            {
                TokenClass = EN_MAIN;
                return TAG_BODY;
            }

            if (len == 10 && strncasecmp(s, "blockquote", len) == 0)
            {
                TokenClass = EN_BLOCK;
                return TAG_QUOTE;
            }
        }
        else if (c == 'c')
        {
            if (len == 4)
            {
                if (strncasecmp(s, "code", len) == 0)
                {
                    TokenClass = EN_TEXT;
                    return TAG_CODE;
                }

                if (strncasecmp(s, "cite", len) == 0)
                {
                    TokenClass = EN_TEXT;
                    return TAG_CITE;
                }
            }
        }
        else if (c == 'd')
        {
            if (len == 3 && strncasecmp(s, "dfn", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_DFN;
            }

            if (len != 2)
                return 0;

            if (strncasecmp(s, "dl", len) == 0)
            {
                TokenClass = EN_LIST;
                return TAG_DL;
            }

            if (strncasecmp(s, "dt", len) == 0)
            {
                TokenClass = EL_DEFLIST;
                return TAG_DT;
            }

            if (strncasecmp(s, "dd", len) == 0)
            {
                TokenClass = EL_DEFLIST;
                return TAG_DD;
            }
        }
        else if (c == 'e')
        {
            if (len == 2 && strncasecmp(s, "em", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_EM;
            }
        }
        else if (c == 'f')
        {
            if (len == 3 && strncasecmp(s, "fig", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_FIG;
            }
        }
        else if (c == 'h')
        {
            if (len == 4 && strncasecmp(s, "head", len) == 0)
            {
                TokenClass = EN_SETUP;
                return TAG_HEAD;
            }

            if (len != 2)
                return 0;

            TokenClass = EN_HEADER;
            c = tolower(s[1]);

            switch (c)
            {
                case '1':
                    return TAG_H1;
                case '2':
                    return TAG_H2;
                case '3':
                    return TAG_H3;
                case '4':
                    return TAG_H4;
                case '5':
                    return TAG_H5;
                case '6':
                    return TAG_H6;
                case 'r':
                    TokenClass = EN_BLOCK;
                    return TAG_HR;
            }
        }
        else if (c == 'i')
        {
            if (len == 1)
            {
                TokenClass = EN_TEXT;
                return TAG_ITALIC;
            }

            if (len == 3 && strncasecmp(s, "img", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_IMG;
            }

            if (len == 5 && strncasecmp(s, "input", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_INPUT;
            }

            if (len == 7 && strncasecmp(s, "isindex", len) == 0)
            {
                TokenClass = EN_SETUP;
                return TAG_ISINDEX;
            }
        }
        else if (c == 'k')
        {
            if (len == 3 && strncasecmp(s, "kbd", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_KBD;
            }
        }
        else if (c == 'l')
        {
            if (len == 2 && strncasecmp(s, "li", len) == 0)
            {
                TokenClass = EN_LIST;
                return TAG_LI;
            }
        }
        else if (c == 'm')
        {
            if (len == 4 && strncasecmp(s, "math", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_MATH;
            }

            if (len == 6 && strncasecmp(s, "margin", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_MARGIN;
            }
        }
        else if (c == 'o')
        {
            if (len == 2 && strncasecmp(s, "ol", len) == 0)
            {
                TokenClass = EN_LIST;
                return TAG_OL;
            }

            if (len == 6 && strncasecmp(s, "option", len) == 0)
            {
                TokenClass = EN_TEXT;  /* kludge for error recovery */
                return TAG_OPTION;
            }
        }
        else if (c == 'p')
        {
            if (len == 1)
            {
                TokenClass = EN_BLOCK;
                return TAG_P;
            }

            if (len == 3 && strncasecmp(s, "pre", len) == 0)
            {
                TokenClass = EN_BLOCK;
                return TAG_PRE;
            }
        }
        else if (c == 'q')
        {
            if (len == 1)
            {
                TokenClass = EN_TEXT;
                return TAG_Q;
            }

            if (len == 5 && strncasecmp(s, "quote", len) == 0)
            {
                TokenClass = EN_BLOCK;
                return TAG_QUOTE;
            }
        }
        else if (c == 'r')
        {
            if (len == 7 && strncasecmp(s, "removed", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_REMOVED;
            }
        }
        else if (c == 's')
        {
            if (len == 1)
            {
                TokenClass = EN_TEXT;
                return TAG_STRIKE;
            }

            if (len == 4 && strncasecmp(s, "samp", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_SAMP;
            }

            if (len == 6 && strncasecmp(s, "strong", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_STRONG;
            }

            if (len == 6 && strncasecmp(s, "select", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_SELECT;
            }
        }
        else if (c == 't')
        {
            if (len == 5 && strncasecmp(s, "title", len) == 0)
            {
                TokenClass = EN_SETUP;
                return TAG_TITLE;
            }

            if (len == 2 && strncasecmp(s, "tt", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_TT;
            }

            if (len == 2 && strncasecmp(s, "tr", len) == 0)
            {
                TokenClass = EN_TABLE;
                return TAG_TR;
            }

            if (len == 2 && strncasecmp(s, "th", len) == 0)
            {
                TokenClass = EN_TABLE;
                return TAG_TH;
            }

            if (len == 2 && strncasecmp(s, "td", len) == 0)
            {
                TokenClass = EN_TABLE;
                return TAG_TD;
            }

            if (len == 5 && strncasecmp(s, "table", len) == 0)
            {
                TokenClass = EN_BLOCK;
                return TAG_TABLE;
            }

            if (len == 8 && strncasecmp(s, "textarea", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_TEXTAREA;
            }
        }
        else if (c == 'u')
        {
            if (len == 1)
            {
                TokenClass = EN_TEXT;
                return TAG_UNDERLINE;
            }

            if (len == 2 && strncasecmp(s, "ul", len) == 0)
            {
                TokenClass = EN_LIST;
                return TAG_UL;
            }
        }
        else if (c == 'v')
        {
            if (len == 3 && strncasecmp(s, "var", len) == 0)
            {
                TokenClass = EN_TEXT;
                return TAG_VAR;
            }
        }
        else if (c == 'x')
        {
            if (len == 3 && strncasecmp(s, "xmp", len) == 0)
            {
                TokenClass = EN_BLOCK;
                return TAG_PRE;
            }
        }
    }

    TokenClass = EN_UNKNOWN;
    return UNKNOWN; /* unknown tag */
}


void UnGetToken(void)
{
    bufptr = LastBufPtr;
}

/*
   The token type is returned in the global token.
   Characters are returned in TokenValue while TokenClass
   is used to return a class value e.g. EN_SETUP or EN_BLOCK.
   Entity definitions are pointed to by EntityValue.

   The bufptr is moved past the token, except at the end
   of the buffer - as a safety precaution.
*/

int GetToken(void)
{
    int c, k, n;
    static char *NextBufPtr;

    LastBufPtr = bufptr;
    c = *bufptr;
    TokenValue = c;

    if (bufptr <= targetptr)
        ViewOffset = PixOffset;

    if (c == '<')
    {
        Token = RecogniseTag();
        bufptr += TagLen;   /* to first char after tag name */
        return Token;
    }

    TokenClass = EN_TEXT;
    EndTag = 0;

    if (c == '&' && isalpha(bufptr[1]))
    {
        n = entity(bufptr + 1, &k);

        if (n)
        {
            bufptr += k;
            TokenValue = n;
            Token = PCDATA;
            return Token;
        }
    }

    if (c <= ' ')
    {
        if (c == '\0')
        {
            Token = ENDDATA;
            TokenClass = EN_UNKNOWN;
            return Token;
        }
        
        ++bufptr;
        Token = WHITESPACE;
        return Token;
    }


    ++bufptr;
    Token = PCDATA;
    return Token;
}

void ParseTitle(int implied, Frame **frames)
{
    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        SwallowAttributes();

    /* skip leading white space - subsequently contigous
       white space is compressed to a single space */
    while (GetToken() == WHITESPACE);
    UnGetToken();

    LineLen = 0;

    for (;;)
    {
        GetToken();

        if (Token == TAG_TITLE && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == WHITESPACE)
        {
            while (GetToken() == WHITESPACE);
            UnGetToken();

            if (LineLen < LBUFSIZE - 1)
                LineBuf[LineLen++] = ' ';
            continue;
        }

        if (Token != PCDATA)
        {
            UnGetToken();
            break;
        }

        if (LineLen < LBUFSIZE - 1)
            LineBuf[LineLen++] = TokenValue;
    }    

    LineBuf[LineLen] = '\0';
    SetBanner(LineBuf);
}

void ParseSetUp(int implied, Frame **frames)
{
    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        SwallowAttributes();

    for (;;)
    {
        while (GetToken() == WHITESPACE);
        UnGetToken();

        if (Token == TAG_HEAD && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (Token == TAG_TITLE)
        {
            ParseTitle(0, frames);
            continue;
        }

        if (Token == TAG_ISINDEX)
        {
            SwallowAttributes();
            IsIndex = 1;
            ClearStatus();
            continue;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == PCDATA || Token == ENTITY)
        {
            UnGetToken();
            break;
        }

        if (Token == ENDDATA || TokenClass != EN_SETUP)
        {
            UnGetToken();
            break;
        }
    }
}

/* assumes bufptr points to start of attribute */
char *ParseAttribute(int *len)
{
    int c;
    char *attr;

    *len = 0;
    attr = bufptr;

    for (;;)
    {
        c = *bufptr;

        if (c == '>' || c == '\0')
            return attr;

        if (c == '=' || IsWhite(c))
            break;

        ++(*len);
        ++bufptr;
    }

    return attr;
}

/* values start with "=" or " = " etc. */
char *ParseValue(int *len)
{
    int c, delim;
    char *value;

    *len = 0;

    while (c = *bufptr, IsWhite(c))
        ++bufptr;

    if (c != '=')
        return 0;

    ++bufptr;   /* past the = sign */

    while (c = *bufptr, IsWhite(c))
        ++bufptr;

    if (c == '"' || c == '\'')
    {
        delim = c;
        ++bufptr;
    }
    else
        delim = 0;

    value = bufptr;

    for (;;)
    {
        c = *bufptr;

        if (c == '\0')
            return 0;

        if (delim)
        {
            if (c == delim)
            {
                ++bufptr;
                break;
            }
        }
        else if (c == '>' || IsWhite(c))
            break;

        ++(*len);
        ++bufptr;
    }

    return value;
}

void ParseAnchorAttrs(char **href, int *hreflen, char **name, int *namelen)
{
    int c, n, m;
    char *attr, *value;

    *href = 0;
    *name = 0;

    for (;;)
    {
        c = *bufptr++;

        if (c == '\0')
            break;

        if (c == '>')
            break;

        if (IsWhite(c))
            continue;

        --bufptr;
        attr = ParseAttribute(&n);
        value = ParseValue(&m);

        if (n == 4 && strncasecmp(attr, "href", n) == 0)
        {
            *href = value;
            *hreflen = m;
            continue;
        }

        if (n == 4 && strncasecmp(attr, "name", n) == 0)
        {
            *name = value;
            *namelen = m;
            continue;
        }
    }
}

void ParseImageAttrs(char **href, int *hreflen, int *align, int *ismap)
{
    int c, n, m;
    char *attr, *value;

    *href = 0;
    *align = ALIGN_BOTTOM;
    *ismap = 0;

    for (;;)
    {
        c = *bufptr++;

        if (c == '\0')
            break;

        if (c == '>')
            break;

        if (IsWhite(c))
            continue;

        --bufptr;
        attr = ParseAttribute(&n);
        value = ParseValue(&m);

        if (n == 3 && strncasecmp(attr, "src", n) == 0)
        {
            *href = value;
            *hreflen = m;
            continue;
        }

        if (n == 5 && strncasecmp(attr, "align", n) == 0)
        {
            if (m == 3 && strncasecmp(value, "top", m) == 0)
                *align = ALIGN_TOP;
            else if (m == 6 && strncasecmp(value, "middle", m) == 0)
                *align = ALIGN_MIDDLE;
            else if (m == 6 && strncasecmp(value, "bottom", m) == 0)
                *align = ALIGN_BOTTOM;

            continue;
        }

        if (n == 5 && strncasecmp(attr, "ismap", n) == 0)
            *ismap = 1;
    }
}

ParseTextAreaAttrs(int *type, char **name, int *nlen,
                char **value, int *vlen, int *rows, int *cols, int *flags)
{
    int c, n, m, checked;
    char *attr, *attrval;

    *type = TEXTFIELD;
    *rows = 4;
    *cols = 20;
    *flags = 0;
    *name = *value = "";
    *nlen = *vlen = 0;

    for (;;)
    {
        c = *bufptr++;

        if (c == '\0')
            break;

        if (c == '>')
            break;

        if (IsWhite(c))
            continue;

        --bufptr;
        attr = ParseAttribute(&n);
        attrval = ParseValue(&m);

        if (n == 4 && strncasecmp(attr, "type", n) == 0)
        {
            if (m == 4 && strncasecmp(attrval, "text", m) == 0)
                *type = TEXTFIELD;
            else if (m == 8 && strncasecmp(attrval, "checkbox", m) == 0)
                *type = CHECKBOX;
            else if (m == 5 && strncasecmp(attrval, "radio", m) == 0)
                *type = RADIOBUTTON;

            continue;
        }

        if (n == 4 && strncasecmp(attr, "name", n) == 0)
        {
            *name = attrval;
            *nlen = m;
            continue;
        }

        if (n == 5 && strncasecmp(attr, "value", n) == 0)
        {
            *value = attrval;
            *vlen = m;
            continue;
        }

        if (n == 4 && strncasecmp(attr, "rows", n) == 0)
        {
            sscanf(attrval, "%d", rows);
            continue;
        }

        if (n == 4 && strncasecmp(attr, "cols", n) == 0)
        {
            sscanf(attrval, "%d", cols);
            continue;
        }

        if (n == 5 && strncasecmp(attr, "error", n) == 0)
        {
            *flags |= IN_ERROR;
            continue;
        }

        if (n == 8 && strncasecmp(attr, "disabled", n) == 0)
        {
            *flags |= DISABLED;
            continue;
        }

        if (n == 7 && strncasecmp(attr, "checked", n) == 0)
            *flags |= CHECKED;
    }
}

ParseInputAttrs(int *type, char **name, int *nlen,
                char **value, int *vlen, int *size, int *flags)
{
    int c, n, m, checked;
    char *attr, *attrval;

    *type = TEXTFIELD;
    *size = 20;
    *flags = 0;
    *name = *value = "";
    *nlen = *vlen = 0;

    for (;;)
    {
        c = *bufptr++;

        if (c == '\0')
            break;

        if (c == '>')
            break;

        if (IsWhite(c))
            continue;

        --bufptr;
        attr = ParseAttribute(&n);
        attrval = ParseValue(&m);

        if (n == 4 && strncasecmp(attr, "type", n) == 0)
        {
            if (m == 4 && strncasecmp(attrval, "text", m) == 0)
                *type = TEXTFIELD;
            else if (m == 8 && strncasecmp(attrval, "checkbox", m) == 0)
                *type = CHECKBOX;
            else if (m == 5 && strncasecmp(attrval, "radio", m) == 0)
                *type = RADIOBUTTON;
            else if (m == 6 && strncasecmp(attrval, "submit", m) == 0)
                *type = SUBMITBUTTON;
            else if (m == 5 && strncasecmp(attrval, "reset", m) == 0)
                *type = RESETBUTTON;

            continue;
        }

        if (n == 4 && strncasecmp(attr, "name", n) == 0)
        {
            *name = attrval;
            *nlen = m;
            continue;
        }

        if (n == 5 && strncasecmp(attr, "value", n) == 0)
        {
            *value = attrval;
            *vlen = m;
            continue;
        }

        if (n == 4 && strncasecmp(attr, "size", n) == 0)
        {
            sscanf(attrval, "%d", size);
            continue;
        }

        if (n == 5 && strncasecmp(attr, "error", n) == 0)
        {
            *flags |= IN_ERROR;
            continue;
        }

        if (n == 8 && strncasecmp(attr, "disabled", n) == 0)
        {
            *flags |= DISABLED;
            continue;
        }

        if (n == 7 && strncasecmp(attr, "checked", n) == 0)
            *flags |= CHECKED;

        if (n == 8 && strncasecmp(attr, "multiple", n) == 0)
            *flags |= MULTIPLE;
    }
}

ParseCellAttrs(int *rowspan, int *colspan, int *align, int *nowrap)
{
    int c, n, m;
    char *attr, *attrval;

    *rowspan = *colspan = 1;
    *align = ALIGN_CENTER;
    *nowrap = 0;

    for (;;)
    {
        c = *bufptr++;

        if (c == '\0')
            break;

        if (c == '>')
            break;

        if (IsWhite(c))
            continue;

        --bufptr;
        attr = ParseAttribute(&n);
        attrval = ParseValue(&m);

        if (n == 7 && strncasecmp(attr, "rowspan", n) == 0)
        {
            sscanf(attrval, "%d", rowspan);
            continue;
        }

        if (n == 7 && strncasecmp(attr, "colspan", n) == 0)
        {
            sscanf(attrval, "%d", colspan);
            continue;
        }

        if (n == 5 && strncasecmp(attr, "align", n) == 0)
        {
            if (m == 6 && strncasecmp(attrval, "center", m) == 0)
                *align = ALIGN_CENTER;
            else if (m == 5 && strncasecmp(attrval, "right", m) == 0)
                *align = ALIGN_RIGHT;
            else if (m == 4 && strncasecmp(attrval, "left", m) == 0)
                *align = ALIGN_LEFT;

            continue;
        }

        if (n == 6 && strncasecmp(attr, "nowrap", n) == 0)
            *nowrap = 1;
    }
}

ParseTableAttrs(int *border)
{
    int c, n, m;
    char *attr, *attrval;

    *border = 0;

    for (;;)
    {
        c = *bufptr++;

        if (c == '\0')
            break;

        if (c == '>')
            break;

        if (IsWhite(c))
            continue;

        --bufptr;
        attr = ParseAttribute(&n);
        attrval = ParseValue(&m);

        if (n == 6 && strncasecmp(attr, "border", n) == 0)
        {
            *border = 1;
            continue;
        }
    }
}

ParseParaAttrs(int *align)
{
    int c, n, m;
    char *attr, *attrval;

    for (;;)
    {
        c = *bufptr++;

        if (c == '\0')
            break;

        if (c == '>')
            break;

        if (IsWhite(c))
            continue;

        --bufptr;
        attr = ParseAttribute(&n);
        attrval = ParseValue(&m);

        if (n == 5 && strncasecmp(attr, "align", n) == 0)
        {
            if (m == 6 && strncasecmp(attrval, "center", m) == 0)
                *align = ALIGN_CENTER;
            else if (m == 5 && strncasecmp(attrval, "right", m) == 0)
                *align = ALIGN_RIGHT;
            else if (m == 4 && strncasecmp(attrval, "left", m) == 0)
                *align = ALIGN_LEFT;
        }
    }
}

void ParseOption(int implied, Frame **frames, Field *field, int font)
{
    int width;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        SwallowAttributes();

    LineLen = 0;

    for (;;)
    {
        GetToken();

        if (Token == TAG_OPTION && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        /* condense whitespace */

        if (Token == WHITESPACE)
        {
            while (GetToken() == WHITESPACE);
            UnGetToken();

            if (LineLen < LBUFSIZE - 1)
                LineBuf[LineLen++] = ' ';

            continue;
        }

        if (Token == PCDATA)
        {
            if (LineLen < LBUFSIZE - 1)
                LineBuf[LineLen++] = TokenValue;

            continue;
        }

     /* unexpected tag so terminate element */

        UnGetToken();
        break;
    }

    LineBuf[LineLen] = '\0';

    if (LineLen > 0)
        AddOption(field, font, LineBuf, LineLen);
}

void ParseSelect(int implied, Frame **frames, int font)
{
    int type, nlen, vlen, size, flags;
    char *name, *value;
    Field *field;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
    {
        /* bodge parsing select's attributes as short term hack */
        ParseInputAttrs(&type, &name, &nlen, &value, &vlen, &size, &flags);
    }
    else
    {
        name = value = "";
        nlen = vlen = flags = 0;

    }

    if (form == NULL)
        form = DefaultForm();

    field = GetField(form, OPTIONLIST, Here, name, nlen, value, vlen, 1, 6, flags|font);
    PrintInputField(frames, field);

    for (;;)
    {
        while (GetToken() == WHITESPACE);

        if (Token == TAG_SELECT && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == TAG_OPTION)
        {
            ParseOption(0, frames, field, font);
            continue;
        }

        if (Token == PCDATA)
        {
            ParseOption(1, frames, field, font);
            continue;
        }

     /* unexpected tag so terminate element */

        error |= ERR_SELECT;
        UnGetToken();
        break;
    }    

    Here += field->width;
}

void ParseEmph(Frame **frames, int align, int emph, int font, int left, int right)
{
    int ThisToken, WordLen, hreflen, namelen, nlen, vlen, size,
        type, rows, cols, delta, ismap, width, height, flags, indent;
    char *href, *name, *value, *p;
    Image *image;
    Field *field;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    ThisToken = Token;

    switch (ThisToken)
    {
        case TAG_ANCHOR:
            ParseAnchorAttrs(&href, &hreflen, &name, &namelen);

            if (href)
                emph |= EMPH_ANCHOR;

            if (name)
            {
                if (strlen(targetId) == namelen &&
                    strncasecmp(name, targetId, namelen) == 0)
                {
                    IdOffset = PixOffset;
                }
            }

            if (preformatted)
                break;

           /* otherwise skip leading white space - subsequently contigous
              white space is compressed to a single space */

            while (GetToken() == WHITESPACE);
            UnGetToken();
            break;

        case TAG_MARGIN:
            SwallowAttributes();
            font = IDX_H2FONT;
            indent = XTextWidth(Fonts[IDX_NORMALFONT], "mmm", 3);
            left += indent;
            right -= indent;

            if (align == ALIGN_LEFT)
                delta = 0;
            else if (align == ALIGN_CENTER)
                delta = (right - Here - LineWidth) / 2;
            else if (align == ALIGN_RIGHT)
                delta = right - Here - LineWidth;

            Here = left;
            EndOfLine(delta);
            PixOffset += LineSpacing[font];
            break;

        case TAG_TT:
        case TAG_CODE:
        case TAG_SAMP:
        case TAG_KBD:
            SwallowAttributes();

            if (font == IDX_BNORMALFONT)
                font = IDX_BIFIXEDFONT;
            else
                font = IDX_TTNORMALFONT;
            break;

        case TAG_MATH:
            SwallowAttributes();
            font = IDX_SYMBOLFONT;
            break;

        case TAG_ITALIC:
        case TAG_EM:
        case TAG_DFN:
        case TAG_CITE:
        case TAG_VAR:
        case TAG_Q:
            SwallowAttributes();

            if (preformatted)
            {
                if (font == IDX_BFIXEDFONT)
                    font = IDX_BIFIXEDFONT;
                else
                    font = IDX_IFIXEDFONT;
            }
            else
            {
                if (font == IDX_BNORMALFONT)
                    font = IDX_BINORMALFONT;
                else
                    font = IDX_INORMALFONT;
            }
            break;

        case TAG_BOLD:
        case TAG_STRONG:
            SwallowAttributes();

            if (preformatted)
            {
                if (font == IDX_IFIXEDFONT)
                    font = IDX_BIFIXEDFONT;
                else
                    font = IDX_BFIXEDFONT;
            }
            else
            {
                if (font == IDX_INORMALFONT)
                    font = IDX_BINORMALFONT;
                else
                    font = IDX_BNORMALFONT;
            }
            break;

        case TAG_UNDERLINE:
            SwallowAttributes();
            emph |= EMPH_UNDERLINE;
            break;

        case TAG_STRIKE:
        case TAG_REMOVED:  /* <removed> doesn't work across <P> etc. */
            SwallowAttributes();
            emph |= EMPH_STRIKE;
            break;

        case TAG_ADDED:  /* doesn't work across <P> etc! */
            SwallowAttributes();
            emph |= EMPH_HIGHLIGHT;

            if (preformatted)
            {
                if (font == IDX_BFIXEDFONT)
                    font = IDX_BIFIXEDFONT;
                else
                    font = IDX_IFIXEDFONT;
            }
            else
            {
                if (font == IDX_BNORMALFONT)
                    font = IDX_BINORMALFONT;
                else
                    font = IDX_INORMALFONT;
            }
            break;

        case TAG_TEXTAREA:  /* a short term bodge */
            ParseTextAreaAttrs(&type, &name, &nlen, &value, &vlen, &rows, &cols, &flags);

            if (form == NULL)
                form = DefaultForm();

            field = GetField(form, type, Here+1, name, nlen, value, vlen, rows, cols, flags|font);
            PrintInputField(frames, field);
            Here += field->width + 2;
            StartOfLine = StartOfWord = bufptr;
            LineLen = LineWidth = WordStart = 0;
            return;

        case TAG_INPUT:
            ParseInputAttrs(&type, &name, &nlen, &value, &vlen, &size, &flags);

            if (form == NULL)
                form = DefaultForm();

            field = GetField(form, type, Here+1, name, nlen, value, vlen, 1, size, flags|font);
            PrintInputField(frames, field);
            Here += field->width+2;
            StartOfLine = StartOfWord = bufptr;
            LineLen = LineWidth = WordStart = 0;
            return;

        case TAG_SELECT:
            ParseSelect(0, frames, font);
            StartOfLine = StartOfWord = bufptr;
            LineLen = LineWidth = WordStart = 0;
            return;

        case TAG_OPTION:
            ParseSelect(1, frames, font);
            StartOfLine = StartOfWord = bufptr;
            LineLen = LineWidth = WordStart = 0;
            return;

        case TAG_BR:
            SwallowAttributes();

            if (align == ALIGN_LEFT)
                delta = 0;
            else if (align == ALIGN_CENTER)
                delta = (right - Here - LineWidth) / 2;
            else if (align == ALIGN_RIGHT)
                delta = right - Here - LineWidth;

            Here = left;
            EndOfLine(delta);
            StartOfLine = StartOfWord = bufptr;
            LineLen = LineWidth = WordStart = 0;
            return;

        case TAG_FIG:
            ParseImageAttrs(&href, &hreflen, &align, &ismap);
            start_figure = GetImage(href, hreflen);
            StartOfLine = StartOfWord = bufptr;
            LineLen = LineWidth = WordStart = 0;
            return;

        case TAG_IMG:
            p = bufptr - 4;
            ParseImageAttrs(&href, &hreflen, &align, &ismap);
            image = GetImage(href, hreflen);

            width = image->width;
            height = image->height;

            if (ismap | (emph & EMPH_ANCHOR))
            {
                width += 8;
                height += 8;
            }
            else
                p = NULL;

            if (image)
            {
                if (align == ALIGN_BOTTOM)
                {
                    delta = height - DESCENT(font);
                    PrintImage(frames, delta, p, image->pixmap, width, height);

                    delta += 2;

                    if (delta > above)
                        above = delta;
                }
                else if (align == ALIGN_MIDDLE)
                {
                    if (height >  ASCENT(font) + DESCENT(font))
                        delta = (height + ASCENT(font) - DESCENT(font))/2;
                    else
                        delta = (height + ASCENT(font))/2;

                    PrintImage(frames, delta, p, image->pixmap, width, height);

                    delta += 2;

                    if (delta > above)
                        above = delta;

                    delta = 2 + height - delta;

                    if (delta> below)
                        below = delta;
                }
                else  /* ALIGN_TOP */
                {
                    delta = ASCENT(font);
                    PrintImage(frames, delta, p, image->pixmap, width, height);

                    delta = 2 + height - delta;

                    if (delta > below)
                        below = delta;
                }

                Here += width;
            }

            StartOfLine = StartOfWord = bufptr;
            LineLen = LineWidth = WordStart = 0;
            return;

        default:
            SwallowAttributes();
            break;
    }

    StartOfLine = StartOfWord = bufptr;
    LineLen = LineWidth = WordStart = 0;

    if (ThisToken == TAG_Q)
    {
            PrintSeqText(frames, IDX_NORMALFONT, "\253");  /* open quote char  */
            Here += XTextWidth(Fonts[IDX_NORMALFONT], "\253", 1);
    }

    for (;;)
    {
        GetToken();

        if (Token == ThisToken && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (TokenClass != EN_TEXT)
        {
            error |= ERR_EMPH;
            UnGetToken();
            break;
        }

        if (Token == WHITESPACE)
        {
            if (preformatted)
            {
                if (TokenValue == '\n')
                    FlushLine(BREAK, frames, align, emph, font, left, right);
                else
                {
                    if (LineLen < LBUFSIZE - 1)
                        LineBuf[LineLen++] = ' ';
                }
                continue;
            }

            while (GetToken() == WHITESPACE);
            UnGetToken();

            if (Here == left && LineLen == 0)
            {
                StartOfLine = StartOfWord = bufptr;
                continue;
            }

            /* check that we have a word */

            if ((WordLen = LineLen - WordStart) > 0)
                WrapIfNeeded(frames, align, emph, font, left, right);

            if (LineLen < LBUFSIZE - 1)
                LineBuf[LineLen++] = ' ';

            WordStart = LineLen;
            StartOfWord = bufptr;
            continue;
        }

        if (IsTag(Token))
        {
            FlushLine(NOBREAK, frames, align, emph, font, left, right);
            ParseEmph(frames, align, emph, font, left, right);
            continue;
        }

        /* must be PCDATA */

        if (LineLen < LBUFSIZE - 1)
            LineBuf[LineLen++] = TokenValue;
    }    

    LineBuf[LineLen] = '\0';
    FlushLine(NOBREAK, frames, align, emph, font, left, right);

    if (ThisToken == TAG_Q)
    {
        PrintSeqText(frames, IDX_NORMALFONT, "\273");  /* close quote char  */
        Here += XTextWidth(Fonts[IDX_NORMALFONT], "\273", 1);
    }

    if (ThisToken == TAG_MARGIN)
    {
        if (align == ALIGN_LEFT)
            delta = 0;
        else if (align == ALIGN_CENTER)
            delta = (right - Here - LineWidth) / 2;
        else if (align == ALIGN_RIGHT)
            delta = right - Here - LineWidth;

        EndOfLine(delta);
        Here = left -  indent;
        PixOffset += LineSpacing[font];
        StartOfLine = StartOfWord = bufptr;
        LineLen = LineWidth = WordStart = 0;
    }
}

void ParseHeader(Frame **frames, int align, int left, int right)
{
    int HeaderTag, WordLen, emph, font;

    ParseParaAttrs(&align);

    if (EndTag)
        return;

    HeaderTag  = Token;
    Here = left;

    switch (HeaderTag)
    {
        case TAG_H1:
            font = IDX_H1FONT;
            break;

        case TAG_H2:
            font = IDX_H2FONT;
            break;

        case TAG_H3:
        case TAG_H4:
            font = IDX_H3FONT;
            break;

        default:
            font = IDX_H4FONT;
            break;
    }

    emph = EMPH_NORMAL;

    StartOfLine = StartOfWord = bufptr;
    LineLen = LineWidth = WordStart = 0;

    for (;;)
    {
        GetToken();

        if (Token == HeaderTag && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (TokenClass != EN_TEXT)
        {
            error |= ERR_HEADER;
            UnGetToken();
            break;
        }

        if (Token == WHITESPACE)
        {
            while (GetToken() == WHITESPACE);
            UnGetToken();

            if (Here == left && LineLen == 0)
            {
                StartOfLine = StartOfWord = bufptr;
                continue;
            }

            /* check that we have a word */

            if ((WordLen = LineLen - WordStart) > 0)
                WrapIfNeeded(frames, align, emph, font, left, right);

            if (LineLen < LBUFSIZE - 1)
                LineBuf[LineLen++] = ' ';

            WordStart = LineLen;
            StartOfWord = bufptr;
            continue;
        }

        if (IsTag(Token))
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            FlushLine(NOBREAK, frames, align, emph, font, left, right);
            ParseEmph(frames, align, emph, font, left, right);
            continue;
        }

        /* must be PCDATA */

        if (LineLen < LBUFSIZE - 1)
            LineBuf[LineLen++] = TokenValue;
    }    

    LineBuf[LineLen] = '\0';
    FlushLine(BREAK, frames, align, emph, font, left, right);
    PixOffset += LineSpacing[font]/2;
}

void ParsePara(int implied, Frame **frames, int align, int font, int left, int right)
{
    int WordLen, emph;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        ParseParaAttrs(&align);

    if (Here < left)
        Here = left;

    emph = EMPH_NORMAL;

    /* skip leading white space - subsequently contigous
       white space is compressed to a single space */
    while (GetToken() == WHITESPACE);
    UnGetToken();

    above = below = 0;
    StartOfLine = StartOfWord = bufptr;
    LineLen = LineWidth = WordStart = 0;

    for (;;)
    {
        GetToken();

        if (Token == TAG_P && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (TokenClass != EN_TEXT)
        {
            UnGetToken();
            break;
        }

        if (Token == WHITESPACE)
        {
            while (GetToken() == WHITESPACE);
            UnGetToken();

            if (Here == left && LineLen == 0)
            {
                StartOfLine = StartOfWord = bufptr;
                continue;
            }

            /* check that we have a word */

            if ((WordLen = LineLen - WordStart) > 0)
                WrapIfNeeded(frames, align, emph, font, left, right);

            if (LineLen < LBUFSIZE - 1)
                LineBuf[LineLen++] = ' ';

            WordStart = LineLen;
            StartOfWord = bufptr;
            continue;
        }

        if (IsTag(Token))
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            FlushLine(NOBREAK, frames, align, emph, font, left, right);
            ParseEmph(frames, align, emph, font, left, right);
            continue;
        }

        /* must be PCDATA */

        if (LineLen < LBUFSIZE - 1)
            LineBuf[LineLen++] = TokenValue;
    }    

    LineBuf[LineLen] = '\0';
    FlushLine(BREAK, frames, align, emph, font, left, right);
    PixOffset += LineSpacing[font]/3;
}

void ItemNumber(char *buf, int depth, int n)
{
    int w, ones, tens, hundreds, thousands;
    char *p, *q;

    w = depth % 3;

    if (w == 0)
        sprintf(buf, "%d.", n);
    else if (w == 1)
    {
        thousands = n/1000;
        n = n % 1000;
        hundreds = n/100;
        n = n % 100;
        tens = n/10;
        ones = n % 10;

        p = buf;

        while (thousands-- > 0)
               *p++ = 'm';

        if (hundreds)
        {
            q = Hundreds[hundreds-1];
            while (*p++ = *q++);
            --p;
        }

        if (tens)
        {
            q = Tens[tens-1];
            while (*p++ = *q++);
            --p;
        }

        if (ones)
        {
            q = Ones[ones-1];
            while (*p++ = *q++);
            --p;
        }

        *p++ = ')';
        *p = '\0';
    }
    else
        sprintf(buf, "%c)", 'a' + (n-1)%26);
}

/* advance declaration due for nested lists */
void ParseUL(int implied, Frame **frames, int depth, int align, int left, int right);
void ParseOL(int implied, Frame **frames, int depth, int align, int left, int right);
void ParseDL(int implied, Frame **frames, int left, int align, int right);

void ParseLI(int implied, Frame **frames, int depth, int seq, int align, int left, int right)
{
    int indent, w;
    long y;
    char buf[16];

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    indent = XTextWidth(Fonts[IDX_NORMALFONT], "mmm", 3);
    y = PixOffset;

    if (!implied)
        SwallowAttributes();

    Here = left + indent/3;

    if (seq > 0)
    {
        ItemNumber(buf, depth++, seq);
        PrintSeqText(frames, IDX_NORMALFONT, buf);

        w = XTextWidth(Fonts[IDX_NORMALFONT], buf, strlen(buf));

        if (w + indent/3 > indent - 4)
            indent = 4 + w + indent/3;
    }
    else
        PrintBullet(frames, depth++, IDX_NORMALFONT);

    if ((w = left + indent) > list_indent)    /* for tables */
        list_indent = w;

    for (;;)
    {
        while (GetToken() == WHITESPACE);

        if (Token == TAG_LI && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (Token == TAG_UL)
        {
            UnGetToken();

            if (EndTag)
                break;

            ParseUL(0, frames, depth, align, left + indent, right);
            continue;
        }

        if (Token == TAG_OL)
        {
            UnGetToken();

            if (EndTag)
                break;

            ParseOL(0, frames, depth, align, left + indent, right);
            continue;
        }

        if (Token == TAG_DL)
        {
            UnGetToken();

            if (EndTag)
                break;

            ParseDL(0, frames, align, left + indent, right);
            continue;
        }

        if (Token == TAG_DT || Token == TAG_DD)
        {
            UnGetToken();
            ParseDL(1, frames, align, left + indent, right);
            continue;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == TAG_P)
        {
            ParsePara(0, frames, align, IDX_NORMALFONT, left + indent, right);
            continue;
        }

        if (Token == TAG_HR)
        {
            SwallowAttributes();

            if (EndTag)
                continue;

            PrintRule(frames, left + indent, right);
            EndOfLine(0);
            continue;
        }

        if (TokenClass == EN_TEXT)
        {
            UnGetToken();
            ParsePara(1, frames, align, IDX_NORMALFONT, left + indent, right);
            continue;
        }

     /* unexpected tag so terminate element */

        UnGetToken();
        break;
    }    

    /* kludge to cope with an <LI> element with no content */

    if (y == PixOffset)
        PixOffset += (4 * LineSpacing[IDX_NORMALFONT])/3;
}

void ParseUL(int implied, Frame **frames, int depth, int align, int left, int right)
{
    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        SwallowAttributes();

    for (;;)
    {
        while (GetToken() == WHITESPACE);

        if (Token == TAG_UL && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == TAG_LI)
        {
            ParseLI(0, frames, depth, 0, align, left, right);
            continue;
        }

        if (Token == TAG_P || Token == TAG_HR || TokenClass == EN_TEXT)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            UnGetToken();
            ParseLI(1, frames, depth, 0, align, left, right);
            continue;
        }

        if (Token == TAG_UL || Token == TAG_OL ||
             Token == TAG_DL || Token == TAG_DT || Token == TAG_DD)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            UnGetToken();
            ParseLI(1, frames, depth, 0, align, left, right);
            continue;
        }

     /* unexpected tag so terminate element */

        error |= ERR_UL;
        UnGetToken();
        break;
    }    

    Here = left;
}

void ParseOL(int implied, Frame **frames, int depth, int align, int left, int right)
{
    int seq;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        SwallowAttributes();

    seq = 0;

    for (;;)
    {
        ++seq;

        while (GetToken() == WHITESPACE);

        if (Token == TAG_OL && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == TAG_LI)
        {
            ParseLI(0, frames, depth, seq, align, left, right);
            continue;
        }

        if (Token == TAG_P || Token == TAG_HR || TokenClass == EN_TEXT)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            UnGetToken();
            ParseLI(1, frames, depth, seq, align, left, right);
            continue;
        }

        if (Token == TAG_UL || Token == TAG_OL ||
             Token == TAG_DL || Token == TAG_DT || Token == TAG_DD)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            UnGetToken();
            ParseLI(1, frames, depth, seq, align, left, right);
            continue;
        }

     /* unexpected tag so terminate element */

        error |= ERR_OL;
        UnGetToken();
        break;
    }    

    Here = left;
}

void ParseDT(int implied, Frame **frames, int align, int left, int right)
{
    int WordLen, emph, font;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        SwallowAttributes();

    Here = left;

    emph = EMPH_NORMAL;
    font = IDX_BNORMALFONT;

    /* skip leading white space - subsequently contigous
       white space is compressed to a single space */
    while (GetToken() == WHITESPACE);
    UnGetToken();

    StartOfLine = StartOfWord = bufptr;
    LineLen = LineWidth = WordStart = 0;

    for (;;)
    {
        GetToken();

        if (Token == TAG_DT && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (TokenClass != EN_TEXT)
        {
            UnGetToken();
            break;
        }

        if (Token == WHITESPACE)
        {
            while (GetToken() == WHITESPACE);
            UnGetToken();

            /* check that we have a word */

            if ((WordLen = LineLen - WordStart) > 0)
                WrapIfNeeded(frames, align, emph, font, left, right);

            if (LineLen < LBUFSIZE - 1)
                LineBuf[LineLen++] = ' ';

            WordStart = LineLen;
            StartOfWord = bufptr;
            continue;
        }

        if (IsTag(Token))
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            FlushLine(NOBREAK, frames, align, emph, font, left, right);
            ParseEmph(frames, align, emph, font, left, right);
            continue;
        }

        /* must be PCDATA */

        if (LineLen < LBUFSIZE - 1)
            LineBuf[LineLen++] = TokenValue;
    }    

    LineBuf[LineLen] = '\0';
    FlushLine(NOBREAK, frames, align, emph, font, left, right);
    Here += 5;
}

void ParseDD(int implied, Frame **frames, int align, int left, int right)
{
    int delta;
    long y;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        SwallowAttributes();

    y = PixOffset;

    for (;;)
    {
        while (GetToken() == WHITESPACE);

        if (Token == TAG_DD && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (Token == TAG_DL && EndTag)
        {
            UnGetToken();
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == TAG_UL)
        {
            ParseUL(0, frames, 0, align, left, right);
            continue;
        }

        if (Token == TAG_OL)
        {
            ParseOL(0, frames, 0, align, left, right);
            continue;
        }

        if (Token == TAG_LI)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            UnGetToken();
            ParseUL(1, frames, 0, align, left, right);
            continue;
        }

        if (Token == TAG_DL)
        {
            if (PixOffset == y) /* force a line break */
            {
                if (align == ALIGN_LEFT)
                    delta = 0;
                else if (align == ALIGN_CENTER)
                    delta = (right - Here - LineWidth) / 2;
                else if (align == ALIGN_RIGHT)
                    delta = right - Here - LineWidth;

                Here = left;
                EndOfLine(delta);
            }

            ParseDL(0, frames, align, left, right);
            Here = left;
            continue;
        }

        if (Token == TAG_HR)
        {
            SwallowAttributes();

            if (EndTag)
                continue;

            PrintRule(frames, left, right);
            EndOfLine(0);
            continue;
        }

        if (Token == TAG_P)
        {
            ParsePara(0, frames, align, IDX_NORMALFONT, left, right);
            continue;
        }

        if (TokenClass == EN_TEXT || Token == ENTITY)
        {
            UnGetToken();
            ParsePara(1, frames, align, IDX_NORMALFONT, left, right);
            continue;
        }

     /* unexpected tag so terminate element */

        if (Token != TAG_DT)
            error |= ERR_DL;

        UnGetToken();
        break;
    }

    /* kludge to cope with an <DD> element with no content */

    if (y == PixOffset)
    {
        PixOffset += LineSpacing[IDX_NORMALFONT]/3;
        EndOfLine(0);
    }
}

void ParseDL(int implied, Frame **frames, int align, int left, int right)
{
    int indent, LastToken, delta;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        SwallowAttributes();

    LastToken = TAG_DL;
    indent = XTextWidth(Fonts[IDX_NORMALFONT], "mm", 2);

    for (;;)
    {
        while (GetToken() == WHITESPACE);

        if (Token == TAG_DL && EndTag)
        {
            SwallowAttributes();

            if (LastToken == TAG_DT)
            {
                if (align == ALIGN_LEFT)
                    delta = 0;
                else if (align == ALIGN_CENTER)
                    delta = (right - Here - LineWidth) / 2;
                else if (align == ALIGN_RIGHT)
                    delta = right - Here - LineWidth;

                EndOfLine(delta);
            }
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == TAG_DT )
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            if (LastToken == TAG_DT)
            {
                if (align == ALIGN_LEFT)
                    delta = 0;
                else if (align == ALIGN_CENTER)
                    delta = (right - Here - LineWidth) / 2;
                else if (align == ALIGN_RIGHT)
                    delta = right - Here - LineWidth;

                EndOfLine(delta);
            }
            ParseDT(0, frames, align, left, right);
            LastToken = TAG_DT;
            continue;
        }

        if (Token == TAG_DD )
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            if (LastToken != TAG_DT)
                error |= ERR_DL;

            ParseDD(0, frames, align, left + indent, right);
            LastToken = TAG_DD;
            continue;
        }

        if (Token == TAG_P || Token == TAG_HR || TokenClass == EN_TEXT ||
            Token == TAG_UL || Token == TAG_LI || Token == TAG_OL || Token == TAG_DL)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            error |= ERR_DL;
            UnGetToken();
            ParseDD(1, frames, align, left + indent, right);
            LastToken = TAG_DD;
            continue;
        }

     /* unexpected tag so terminate element */

        error |= ERR_DL;
        UnGetToken();
        break;
    }

    Here = left;    
}

void ParseBlock(int implied, Frame **frames, int tag, int align, int font, int left, int right)
{
    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        SwallowAttributes();

    Here = left;

    for (;;)
    {
        while (GetToken() == WHITESPACE);

        if (Token == tag && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == TAG_P)
        {
            ParsePara(0, frames, align, font, left, right);
            continue;
        }

        if (TokenClass == EN_TEXT)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            UnGetToken();
            ParsePara(1, frames, align, font, left, right);
            continue;
        }

     /* unexpected tag so terminate element */

        error |= ERR_BLOCK;
        UnGetToken();
        break;
    }    
}

void ParsePRE(int implied, Frame **frames, int left, int right)
{
    int WordLen, ch, n, emph, font;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        SwallowAttributes();

    Here = left;

    preformatted = 1;
    emph = EMPH_NORMAL;
    font = IDX_FIXEDFONT;
    ch = CHWIDTH(font);

    StartOfLine = StartOfWord = bufptr;
    LineLen = LineWidth = WordStart = 0;

    for (;;)
    {
        GetToken();

        if (Token == TAG_PRE && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (TokenClass != EN_TEXT)
        {
            error |= ERR_PRE;
            UnGetToken();
            break;
        }

        if (Token == WHITESPACE && TokenValue == '\n')
        {
            FlushLine(BREAK, frames, ALIGN_LEFT, emph, font, left, right);
            continue;
        }

        if (IsTag(Token))
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            FlushLine(NOBREAK, frames, ALIGN_LEFT, emph, font, left, right);
            ParseEmph(frames, ALIGN_LEFT, emph, font, left, right);
            continue;
        }

        /* must be PCDATA */

#if 0  /* CopyLine can't work out how many spaces to use! */
        if (TokenValue == '\t')
        {
            n = LineLen + (Here - left)/ch;
            n = 8 - n % 8;

            while (n-- > 0 && LineLen < LBUFSIZE - 1)
                LineBuf[LineLen++] = ' ';
            continue;
        }
#endif
        if (LineLen < LBUFSIZE - 1)
            LineBuf[LineLen++] = TokenValue;
    }    

    LineBuf[LineLen] = '\0';
    FlushLine(BREAK, frames, ALIGN_LEFT, emph, font, left, right);
    preformatted = 0;
}

/* tag is TAG_TH or TAG_TD, col is column number starting from 1 upwards, returns cell height */
long ParseTableCell(int implied, Frame **frames, int row, Frame **cells,
          int border, ColumnWidth *widths, int *pcol, int tag, int left, int right)
{
    int align, nowrap, col, rowspan, colspan, m, prev_width, font;
    long cellTop, cellHeight;
    Frame *frame, *newframes;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    newframes = NULL;
    col = *pcol;

    align = ALIGN_CENTER;
    nowrap = 0;
    prev_width = html_width;
    html_width = 0;
    list_indent = min_width = max_width = 0;
    rowspan = colspan = 1;

    if (!implied)
        ParseCellAttrs(&rowspan, &colspan, &align, &nowrap);

    *pcol += colspan;

    if (!prepass)
    {
        font = (tag == TAG_TH ? IDX_H3FONT : IDX_NORMALFONT);
        cellTop = PixOffset;
        left = widths[col].left;
        right = widths[col + colspan - 1].right;
        frame = BeginFrame(frames, 0, border, left-3, right+4);
        frame->lastrow = row + rowspan - 1;

        /* try to make TH and TD baselines match for first line */

        if (tag == TAG_TH)
            PixOffset += ASCENT(IDX_NORMALFONT) - ASCENT(IDX_H3FONT);

        PixOffset += LineSpacing[IDX_NORMALFONT]/3;
    }

    for (;;)
    {
        while (GetToken() == WHITESPACE);

        if (Token == TAG_DD && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (Token == TAG_DL && EndTag)
        {
            UnGetToken();
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == TAG_UL)
        {
            ParseUL(0, &newframes, 0, align, left, right);
            continue;
        }

        if (Token == TAG_OL)
        {
            ParseOL(0, &newframes, 0, align, left, right);
            continue;
        }

        if (Token == TAG_LI)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            UnGetToken();
            ParseUL(1, &newframes, 0, align, left, right);
            continue;
        }

        if (Token == TAG_DL)
        {
            ParseDL(0, &newframes, align, left, right);
            Here = left;
            continue;
        }

        if (Token == TAG_HR)
        {
            SwallowAttributes();

            if (EndTag)
                continue;

            PrintRule(&newframes, left, right);
            EndOfLine(0);
            continue;
        }

        if (Token == TAG_P)
        {
            ParsePara(0, &newframes, align, (tag == TAG_TH ? IDX_H3FONT : IDX_NORMALFONT), left, right);
            continue;
        }

        if (TokenClass == EN_HEADER)
        {
            UnGetToken();
            ParseHeader(&newframes, align, left, right);
            continue;
        }

        if (TokenClass == EN_TEXT || Token == ENTITY)
        {
            UnGetToken();
            ParsePara(1, &newframes, align, (tag == TAG_TH ? IDX_H3FONT : IDX_NORMALFONT), left, right);
            continue;
        }

     /* unexpected tag so terminate element */

        UnGetToken();
        break;
    }

    if (col > COLS(widths))   /* update table column count */
        COLS(widths) = col;

    m = MAXCOLS(widths);

    if (col + colspan - 1 > m)   /* double array size as needed */
    {
        while (col + colspan - 1> m)
           m = m << 1;

        widths = (ColumnWidth *)realloc(widths, (m + 1) * sizeof(ColumnWidth));
        MAXCOLS(widths) = m;

        while (m >= col)   /* zero widths for newly allocated elements */
        {
            widths[m].min = 0;
            widths[m].max = 0;
            widths[m].rows = 0;
            --m;
        }
    }

    if (html_width > left)
        max_width = html_width - left;

    if (prev_width > html_width)
        html_width = prev_width;

    min_width += list_indent;

    if (nowrap && max_width > min_width)
        min_width = max_width;

    if (colspan > 1)  /* apportion widths evenly */
    {
        min_width = min_width/colspan;
        max_width = max_width/colspan;

        for (m = 0; m < colspan; ++m)
        {
            if (min_width > widths[col + m].min)
                widths[col + m].min = min_width;

            if (max_width > widths[col + m].max)
                widths[col + m].max = max_width;
        }
    }
    else
    {
        if (min_width > widths[col].min)
            widths[col].min = min_width;

        if (max_width > widths[col].max)
            widths[col].max = max_width;
    }

    widths[col].rows = rowspan - 1;

    if (!prepass)
    {
        cellHeight = (PixOffset - cellTop)/rowspan;
        frame->height = cellHeight;  /* for debug only */
        widths[col].min = cellHeight;
        FlushFrames(1, &newframes);  /* any nested frames */
        frame->length = paintlen - frame->info - FRAMESTLEN;
        PrintFrameLength(frame);
        InsertCell(cells, frame);
        return cellHeight;
    }

    return 0;
}

void DummyCell(Frame **frames, int row, Frame **cells,
               int border, ColumnWidth *widths, int col)
{
    int left, right;
    Frame *frame;

    left = widths[col].left;
    right = widths[COLS(widths)].right;
    frame = BeginFrame(frames, 0, border, left-3, right+4);
    frame->lastrow = row;
    frame->length = paintlen - frame->info - FRAMESTLEN;
    PrintFrameLength(frame);
    InsertCell(cells, frame);
}

void ParseTableRow(int implied, Frame **frames, int row,
        Frame **cells, int border, ColumnWidth *widths, int left, int right)
{
    int cols = 1;
    long rowTop, rowHeight, cellHeight;
    char *row_bufptr;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        SwallowAttributes();

    Here = left;
    rowTop = PixOffset;
    rowHeight = 0;

    for (;;)
    {
        PixOffset = rowTop;

        /* if this cell spans more than one row */
        if (widths[cols].rows > 0)
        {
            widths[cols].rows -= 1;  /* decrement span count */

            if (!prepass) /* does spanned cell effect rowBottom? */
            {
                if (widths[cols].min > rowHeight)
                    rowHeight = widths[cols].min;
            }

            ++cols;
            continue;
        }

        while (GetToken() == WHITESPACE);

        if (Token == TAG_TR && EndTag)
        {
            if (EndTag)
            {
                SwallowAttributes();
                break;
            }

            UnGetToken();
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == TAG_TH)
        {
            cellHeight = ParseTableCell(0, frames, row, cells, border, widths, &cols, TAG_TH, left, right);

            if (cellHeight > rowHeight)
                rowHeight = cellHeight;
            continue;
        }

        if (Token == TAG_TD)
        {
            cellHeight = ParseTableCell(0, frames, row, cells, border, widths, &cols, TAG_TD, left, right);

            if (cellHeight > rowHeight)
                rowHeight = cellHeight;
            continue;
        }

        if (Token == TAG_TABLE)
        {
            UnGetToken();
            break;
        }

        if (TokenClass == EN_LIST || TokenClass == EN_TEXT ||
            TokenClass == EN_HEADER || TokenClass == EN_BLOCK)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            UnGetToken();
            cellHeight = ParseTableCell(1, frames, row, cells, border, widths, &cols, TAG_TD, left, right);

            if (cellHeight > rowHeight)
                rowHeight = cellHeight;
            continue;
        }

     /* unexpected tag so terminate element */

        UnGetToken();
        break;
    }    

    if (!prepass && cols <= COLS(widths))
        DummyCell(frames, row, cells, border, widths, cols);

    PixOffset = rowTop + rowHeight;

    if (!prepass)
        FlushCells(frames, row, cells);  /* calls EndFrame() for cells ending this row */
}

void ParseTable(int implied, Frame **frames, int left, int right)
{
    int row, cols, border, i, w, W, x, min, max, spare, prev_width;
    long table_offset;
    char *table_bufptr;
    Frame *cells;

    ColumnWidth *widths;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        ParseTableAttrs(&border);

    prev_width = html_width;
    Here = left;

    widths = (ColumnWidth *)malloc((NCOLS + 1) * sizeof(ColumnWidth));

    COLS(widths) = 0;               /* current number of columns */
    MAXCOLS(widths) = NCOLS;        /* space currently allocated */
    widths[0].rows = 0;

    for (i = NCOLS; i > 0; --i)     /* zero widths for allocated elements */
    {
        widths[i].min = 0;
        widths[i].max = 0;
        widths[i].rows = 0;
    }

    prepass = 1;
    table_bufptr = bufptr;          /* note parse position for second pass */
    table_offset = PixOffset;

 draw_table:

    row = 0;
    cells = NULL;

    for (;;)
    {
        while (GetToken() == WHITESPACE);

        if (Token == TAG_TABLE && EndTag)
        {
            if (EndTag)
            {
                SwallowAttributes();
                break;
            }

            UnGetToken();
            break;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == TAG_TR)
        {
            ++row;
            ParseTableRow(0, frames, row, &cells, border, widths, left, right);
            continue;
        }

        if (TokenClass == EN_LIST || TokenClass == EN_TEXT || TokenClass == EN_TABLE ||
            TokenClass == EN_HEADER || TokenClass == EN_BLOCK)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            UnGetToken();
            ++row;
            ParseTableRow(1, frames, row, &cells, border, widths, left, right);
            continue;
        }

     /* unexpected tag so terminate element */

        UnGetToken();
        break;
    }    

    if (prepass)   /* assign actual column widths */
    {
        for (i = 1, min = 3, max = 3, W = 0; i <= COLS(widths); ++i)
        {
            min += 7 + widths[i].min;
            max += 7 + widths[i].max;
            W += widths[i].max - widths[i].min;
        }

        /* if max fits in window then use max widths */

        if (max <= right - left)
        {
            x = left + (right - left - max)/2;

            for (i = 1; i <= COLS(widths); ++i)
            {
                widths[i].left = x + 3;
                x += 7 + widths[i].max;
                widths[i].right = x - 4;
            }
        }
        else if (min < right - left)
        {
            x = left;
            spare = right - left - min;

            for (i = 1; i <= COLS(widths); ++i)
            {
                w = widths[i].max - widths[i].min;
                widths[i].left = x + 3;
                x += 7 + widths[i].min + (spare * w)/W;
                widths[i].right = x - 4;
            }
        }
        else /* assign minimum column widths */
        {
            x = left;

            for (i = 1; i <= COLS(widths); ++i)
            {
                widths[i].left = x + 3;
                x += 7 + widths[i].min;
                widths[i].right = x - 4;
            }
        }

        /* and do second pass to draw table */

        bufptr = table_bufptr;
        PixOffset = table_offset;
        prepass = 0;
        goto draw_table;
    }

    free(widths);   /* free column widths */
    Here = left;
    PixOffset += LineSpacing[IDX_H1FONT]/2;

 /* restore previous value of html_width as needed */
    w = left + max;
    html_width = (w < prev_width ? prev_width : w);
}

void ParseBody(int implied, Frame **frames, int left, int right)
{
    int indent, margin;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    indent = margin = XTextWidth(Fonts[IDX_NORMALFONT], "mmm", 2);

    if (!implied)
        SwallowAttributes();

    for (;;)
    {
        while (GetToken() == WHITESPACE);

        if (Token == TAG_BODY && EndTag)
        {
            SwallowAttributes();
            break;
        }

        if (IsTag(Token) && TokenClass == EN_HEADER)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            if (Token == TAG_H1 || Token == TAG_H2)
                ParseHeader(frames, ALIGN_LEFT, left, right);
            else
                ParseHeader(frames, ALIGN_LEFT, left+(margin/2), right);
            continue;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == TAG_UL)
        {
            ParseUL(0, frames, 0, ALIGN_LEFT, left+margin, right);
            continue;
        }

        if (Token == TAG_OL)
        {
            ParseOL(0, frames, 0, ALIGN_LEFT, left+margin, right);
            continue;
        }

        if (Token == TAG_LI)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            error |= ERR_LI;
            UnGetToken();
            ParseUL(1, frames, 0, ALIGN_LEFT, left+margin, right);
            continue;
        }

        if (Token == TAG_DL)
        {
            ParseDL(0, frames, ALIGN_LEFT, left+margin, right);
            continue;
        }

        if (TokenClass == EN_TABLE)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            error |= ERR_TABLE;
            UnGetToken();
            ParseTable(1, frames, left, right);
            continue;
        }

        if (Token == TAG_TABLE)
        {
            ParseTable(0, frames, left, right);
            continue;
        }

        if (Token == TAG_DT || Token == TAG_DD)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            error = ERR_DL;
            UnGetToken();
            ParseDL(1, frames, ALIGN_LEFT, left+margin, right);
            continue;
        }

        if (Token == TAG_HR)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            SwallowAttributes();
            PrintRule(frames, left, right);
            EndOfLine(0);
            continue;
        }

        if (Token == TAG_PRE)
        {
            ParsePRE(0, frames, left+margin, right);
            continue;
        }

        if (Token == TAG_P)
        {
            ParsePara(0, frames, ALIGN_LEFT, IDX_NORMALFONT, left+margin, right);
            continue;
        }

        if (TokenClass == EN_TEXT || Token == ENTITY)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            UnGetToken();
            ParsePara(1, frames, ALIGN_LEFT, IDX_NORMALFONT, left+margin, right);
            continue;
        }

        if (Token == TAG_ADDRESS)
        {
            ParseBlock(0, frames, Token, ALIGN_RIGHT, IDX_H3FONT, left+margin+indent, right-indent);
            Here = left;
            continue;
        }

        if (Token == TAG_ABSTRACT)
        {
            ParseBlock(0, frames, Token, ALIGN_LEFT, IDX_BINORMALFONT, left+margin+indent, right-indent);
            Here = left;
            continue;
        }

        if (Token == TAG_QUOTE)
        {
            ParseBlock(0, frames, Token, ALIGN_LEFT, IDX_INORMALFONT, left+indent+indent, right-indent);
            Here = left;
            continue;
        }

        if (Token == ENDDATA)
        {
            UnGetToken();
            break;
        }

        if (Token == TAG_BODY || TokenClass == EN_SETUP)
        {
            error |= ERR_SETUP;
            SwallowAttributes();
            continue;
        }
    }
}

long ParseHTML(int *width)
{
    int c, WordLen;
    Byte *p;
    Frame *frames;

    PixOffset = 0;
    LastBufPtr = bufptr = buffer+hdrlen;
    error = prepass = preformatted = 0;
    paintlen = 0;
    IsIndex = 0;
    start_figure = figure = 0;
    html_width = WinWidth;
    font = paintStartLine = -1;
    frames = NULL;
    form = NULL;

    if (paintbufsize == 0)
    {
        paintbufsize = 8192;
        paint = (Byte *)malloc(paintbufsize);
    }

 /* Reserve space for background's begin frame object
    which is needed to simply display and scrolling routines */

    MakeRoom(FRAMESTLEN);

    for (;;)
    {    
        while (GetToken() == WHITESPACE);

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        break;
    }

    if (Token == TAG_HEAD)
        ParseSetUp(0, &frames);
    else
    {
        UnGetToken();
        ParseSetUp(1, &frames);
    }

    while (GetToken() == WHITESPACE);

    if (Token == TAG_BODY)
        ParseBody(0, &frames, MININDENT, MAXMARGIN);
    else
    {
        UnGetToken();
        ParseBody(1, &frames, MININDENT, MAXMARGIN);
    }

    FlushFrames(1, &frames); /* flush remaining end of frames */
    *width = html_width;

 /* initialise background frame */

    background.next = NULL;
    background.child = NULL;
    background.top = paint + FRAMESTLEN;
    background.offset = 0;
    background.indent = 0;
    background.height = PixOffset;
    background.width = html_width;
    background.info = 0;
    background.length = paintlen - FRAMESTLEN;
    background.style = 0;
    background.border = 0;

 /* and fill in begin frame object at start of paint buffer */

    p = paint;
    *p++ = BEGIN_FRAME;
    PushValue(p, background.offset & 0xFFFF);
    PushValue(p, (background.offset >> 16) & 0xFFFF);
    PushValue(p, background.indent);
    PushValue(p, background.width);
    PushValue(p, background.height);
    *p++ = background.style;
    *p++ = background.border;
    PushValue(p, background.length);

    TopObject = paint;   /* obsolete */

    return PixOffset;
}
