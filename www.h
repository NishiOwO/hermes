/* www.h function declarations and general defines */

#define VERSION         "v 1.0a"

#define memmove memcpy  /* SunOS doesn't include memmove */

#define COMPACTDEFLISTS 1   /* comment out for old style */

/* You must change the following equates for the default configuration */

#define CACHE_SERVER    "ptsun00.cern.ch"
#define CACHE_PORT      2786

#define NEWS_SERVER     "news"
#define GATEWAY_HOST    "hplose.hpl.hp.com"         /* TCP/IP gateway to Internet */
#define HELP_URL   "http://hplose.hpl.hp.com:2784/nfs/hplose/permanent/dsr/www/help.html"
#define DEFAULT_URL "http://www-hplb.hpl.hp.com/"
#define PRINTER     "alljet"

/* HTML error codes displayed in Octal */

#define ERR_ANCHOR  1 << 0
#define ERR_EMPH    1 << 1
#define ERR_HEADER  1 << 2

#define ERR_UL      1 << 3
#define ERR_OL      1 << 4
#define ERR_DL      1 << 5

#define ERR_SELECT  1 << 6
#define ERR_BLOCK   1 << 7
#define ERR_PRE     1 << 8

#define ERR_LI      1 << 9
#define ERR_TABLE   1 << 10
#define ERR_SETUP   1 << 11

/* fonts and colors set in www.c GetResources(void) */

#define H1FONT      "-adobe-helvetica-bold-r-normal--18-180-75-75-p-103-iso8859-1"
#define H2FONT      "-adobe-helvetica-bold-r-normal--14-140-75-75-p-82-iso8859-1"
#define H3FONT      "-adobe-helvetica-bold-r-normal--12-120-75-75-p-70-iso8859-1"
#define H4FONT      "-adobe-helvetica-bold-r-normal--10-100-75-75-p-60-iso8859-1"
#define LABELFONT   "6x13"

#define NORMALFONT  "-adobe-times-medium-r-normal--14-140-75-75-p-74-iso8859-1"
#define ITALICFONT  "-adobe-times-medium-i-normal--14-140-75-75-p-73-iso8859-1"
#define BINORMFONT  "-adobe-times-bold-i-normal--14-140-75-75-p-77-iso8859-1"
#define BOLDFONT    "-adobe-times-bold-r-normal--14-140-75-75-p-77-iso8859-1"

#define IFIXEDFONT  "-adobe-courier-medium-o-normal--12-120-75-75-m-70-iso8859-1"
#define BFIXEDFONT  "-adobe-courier-bold-r-normal--12-120-75-75-m-70-iso8859-1"
#define BIFIXEDFONT "-adobe-courier-bold-o-normal--12-120-75-75-m-70-iso8859-1"
#define RFIXEDFONT  "-adobe-courier-medium-r-normal--12-120-75-75-m-70-iso8859-1"

#define SYMFONT     "-adobe-symbol-medium-r-normal--12-120-75-75-p-74-adobe-fontspecific"

#define H1FONTL      "-adobe-helvetica-bold-r-normal--34-240-100-100-p-182-iso8859-1"
#define H2FONTL      "-adobe-helvetica-bold-r-normal--25-180-100-100-p-138-iso8859-1"
#define H3FONTL      "-adobe-helvetica-bold-r-normal--20-140-100-100-p-105-iso8859-1"
#define H4FONTL      "-adobe-helvetica-bold-r-normal--14-100-100-100-p-82-iso8859-1"

#define NORMALFONTL  "-adobe-new century schoolbook-medium-r-normal--17-120-100-100-p-91-iso8859-1"
#define ITALICFONTL  "-adobe-new century schoolbook-medium-i-normal--17-120-100-100-p-92-iso8859-1"
#define BINORMFONTL  "-adobe-new century schoolbook-bold-i-normal--17-120-100-100-p-99-iso8859-1"
#define BOLDFONTL    "-adobe-new century schoolbook-bold-r-normal--17-120-100-100-p-99-iso8859-1"

#define IFIXEDFONTL  "-adobe-courier-medium-o-normal--17-120-100-100-m-100-iso8859-1"
#define BFIXEDFONTL  "-adobe-courier-bold-r-normal--17-120-100-100-m-100-iso8859-1"
#define BIFIXEDFONTL "-adobe-courier-bold-o-normal--17-120-100-100-m-100-iso8859-1"
#define RFIXEDFONTL  "-adobe-courier-medium-r-normal--17-120-100-100-m-100-iso8859-1"

#define SYMFONTL     "-adobe-symbol-medium-r-normal--17-120-100-100-p-95-adobe-fontspecific"

#define H1FONTG      "-adobe-helvetica-bold-r-normal--34-240-100-100-p-182-iso8859-1"
#define H2FONTG      "-adobe-helvetica-bold-r-normal--25-180-100-100-p-138-iso8859-1"
#define H3FONTG      "-adobe-helvetica-bold-r-normal--20-140-100-100-p-105-iso8859-1"
#define H4FONTG      "-adobe-helvetica-bold-r-normal--14-100-100-100-p-82-iso8859-1"

#define NORMALFONTG  "-adobe-new century schoolbook-medium-r-normal--20-140-100-100-p-103-iso8859-1"
#define ITALICFONTG  "-adobe-new century schoolbook-medium-i-normal--20-140-100-100-p-104-iso8859-1"
#define BINORMFONTG  "-adobe-new century schoolbook-bold-i-normal--20-140-100-100-p-111-iso8859-1"
#define BOLDFONTG    "-adobe-new century schoolbook-bold-r-normal--20-140-100-100-p-113-iso8859-1"

#define IFIXEDFONTG  "-adobe-courier-medium-o-normal--20-140-100-100-m-110-iso8859-1"
#define BFIXEDFONTG  "-adobe-courier-bold-r-normal--20-140-100-100-m-110-iso8859-1"
#define BIFIXEDFONTG "-adobe-courier-bold-o-normal--20-140-100-100-m-110-iso8859-1"
#define RFIXEDFONTG  "-adobe-courier-medium-r-normal--20-140-100-100-m-110-iso8859-1"

#define SYMFONTG     "-adobe-symbol-medium-r-normal--20-140-100-100-p-107-adobe-fontspecific"


/* fonts are held in an array to allow them to be identified by a 4 bit index */

#define FONTS          15

#define IDX_H1FONT              0
#define IDX_H2FONT              1
#define IDX_H3FONT              2
#define IDX_H4FONT              3
#define IDX_LABELFONT           4
#define IDX_NORMALFONT          5
#define IDX_INORMALFONT         6
#define IDX_BNORMALFONT         7
#define IDX_BINORMALFONT        8
#define IDX_TTNORMALFONT        9
#define IDX_FIXEDFONT          10
#define IDX_IFIXEDFONT         11
#define IDX_BFIXEDFONT         12
#define IDX_BIFIXEDFONT        13
#define IDX_SYMBOLFONT         14


/*
The paint stream consists of a sequence of overlapping frames:

    a) TextLines which contain elements appearing on a single line
       e.g. STRING, LITERAL, RULE, BULLET, IMAGE. The TextLine
       is large enough to contain all objects on the line. TextLines
       never overlap except when they occur in different frames.

    b) Frames which contain TextLines and other Frames
       (used for figures, tables and sidebars)

Frames are designed for efficient painting and scrolling,
regardless of the length of a document. The frames are strictly
ordered with respect to increasing pixel offset from the top of
the document. Objects with the same offset are sorted with
respect to increasing indent.

This property is vital to the scrolling and painting
mechanism. The ordering may be broken for elements within LINEs,
for which a left to right ordering following that in the HTML
source simplifies the Find menu action.

FRAMEs are broken into BEGIN and END frame objects. The END frame
objects are positioned in the sequence according the offset at
which the frames end. This is needed to safely terminate search
for objects which intersect the top of the window, when scrolling
back through the document.
*/

#define TEXTLINE    1   /* frame containing elements on a line */
#define BEGIN_FRAME 2   /* begining of a frame */
#define END_FRAME   3   /* end of a frame */

#define FRAMESTLEN  17  /* number of bytes in start of frame marker */
#define FRAMENDLEN  5   /* number of bytes in end of frame marker */
#define TXTLINLEN   11  /* number of bytes in TEXTLINE header */

/* parameters in frame-like objects:

Each object starts with an 8 bit tag and ends with a 2 byte
size field that permits moving back up the list of objects.
The size is set to the number of bytes from the tag up to
the size field itself.

    tag = BEGIN_FRAME
    offset (4 bytes)
    indent (2 bytes)
    width  (2 bytes)
    height (4 bytes)
    style  (1 byte)
    border (1 byte)
    length (2 bytes)
    zero or more elements
    size   (2 bytes)

The length parameter gives the number of bytes until
the end of the frame's data. It is used to skip quickly
to objects following this frame. This requires a simple
stack in DisplayHTML() to handle nested frames.

The pixel offset for the end of the frame is marked by
the END_FRAME object which specifies the number of bytes
back to the corresponding BEGIN_FRAME object.

    tag = END_FRAME
    start  (2 bytes)
    size   (2 bytes)

A frame for a line of text composed of multiple elements:

TEXTLINE:

    tag == TEXTLINE
    offset (4 bytes)
    baseline (two bytes)
    indent (two bytes)
    height (two bytes)
    zero or more elements
    1 byte marker (= 0)
    size (two bytes)
*/

typedef unsigned char Byte;

/*
When parsing, we need to defer printing the END_FRAME marker
until we come to print an object with the same or larger offset
and at the same level of frame nesting. We also need to examine
the widths and position of active frames to decide where to flow
text and graphics. In this phase the Frame struct only uses
the next, nesting, info and offset fields.

When displaying, we need a list of frames which intersect the
top of the window, plus a pointer to the background flow. Once
this list has been dealt with, display continues in the background
flow and subsequent frames are dealt with one at a time.

When scrolling towards the start of the document, the code needs
to take care in case a text line object intersects the top of
the window, otherwise the first fully visible line will do.
*/

struct frame_struct
    {
        struct frame_struct *next;  /* linked list of peer frames */
        struct frame_struct *child; /* linked list of nested frames */
        unsigned char *top;         /* where to start displaying from */
        long offset;                /* from start of document */
        unsigned int indent;        /* from left of document */
        long height;                /* documents can be very long */
        unsigned int width;
        unsigned int info;    /* to BEGIN_FRAME object in paint stream */
        unsigned int length;        /* byte count of frame's data */
        unsigned int lastrow;       /* used in parsing tables */
        unsigned char style;        /* frame's background style */
        unsigned char border;       /* frame's border style */
    };

typedef struct frame_struct Frame;

/* stack of nested frames starting after top of windo */

typedef struct sframe_struct
    {
        unsigned char *p_next;
        unsigned char *p_end;
    } StackFrame;

/* elements which can appear in a TEXTLINE & must be non-zero */

#define STRING      1       /* tag name for string values */
#define BULLET      2       /* tag name for bullet */
#define RULE        3       /* horizontal rule */
#define SEQTEXT     4       /* explicit text, e.g. "i)" */
#define IMAGE       5       /* a pixmap (character-like) */
#define INPUT       6       /* text input field */

#define RULEFLEN    5
#define BULLETFLEN  5
#define STRINGFLEN  12
#define IMAGEFLEN   17
#define INPUTFLEN   5
#define SEQTEXTFLEN(len) (4 + len)

#define BSIZE 6     /* size of bullet graphic */

#define CHWIDTH(font)    XTextWidth(Fonts[font], " ", 1)
#define SPACING(font)    (2 + font->max_bounds.ascent + font->max_bounds.descent)
#define BASELINE(font)   (1 + font->max_bounds.ascent)
#define STRIKELINE(font) (font->max_bounds.ascent - font->max_bounds.descent + 1)
#define ASCENT(font)     (1 + Fonts[font]->max_bounds.ascent)
#define DESCENT(font)    (1 + Fonts[font]->max_bounds.descent)
#define WIDTH(font, str, len) XTextWidth(Fonts[font], str, len)

#define EMPH(emph, font)    (emph | (font & 0xF))

#define EMPH_NORMAL      0
#define EMPH_ANCHOR     0x10
#define EMPH_UNDERLINE  0x20
#define EMPH_STRIKE     0x40
#define EMPH_HIGHLIGHT  0x80

/* note that PRE_TEXT is passed to display code with tag *not* emph */

#define ISMAP           0x80
#define PRE_TEXT        0x80     /* implies preformatted text */


#define IsWhite(c)  (c == ' ' || c == '\n' || c == '\t' || c == '\f' || c == '\r')

#define max(a, b)   ((a) > (b) ? (a) : (b))
#define min(a, b)   ((a) < (b) ? (a) : (b))

/* imaging capability */

typedef struct
    {
        unsigned char red;
        unsigned char green;
        unsigned char blue;
        unsigned char grey;
    } Color;

#define COLOR888    888     /* DirectColor 24 bit systems */
#define COLOR444    444     /* DirectColor 12 bit systems */
#define COLOR232    232     /* 128 shared colors from default visual */
#define GREY4        4      /* 16 shared colors from default visual */
#define MONO         1      /*  when all else fails */

#define BUTTONUP     1      /* used by WhichAnchor() */
#define BUTTONDOWN   2
#define MOVEUP       3      /* Mouse move in button up state */
#define MOVEDOWN     4      /* Mouse move in button down state */

/*--------------------------------------------------------------------------------------*/

/* document tags */

#define REMOTE  0
#define LOCAL   1

/* GATEWAY isn't a tag but is used with
   REMOTE in calls to GetAuthorization */

#define GATEWAY 5

/* document types */

#define TEXTDOCUMENT    0
#define HTMLDOCUMENT    1
#define XVDOCUMENT      2
#define DVIDOCUMENT     3
#define PSDOCUMENT      4
#define MPEGDOCUMENT    5
#define AUDIODOCUMENT   6
#define XWDDOCUMENT     7
#define MIMEDOCUMENT    8

/* document protocol  - see table of names in http.c */

#define MYFILE  0
#define HTTP    1
#define FTP     2
#define NEWS    3
#define GOPHER  4
#define TELNET  5
#define RLOGIN  6
#define WAIS    7
#define FINGER  8

#define NPROTOCOLS 9

/* useful macro for freeing heap strings */

#define Free(s)   if (s) free(s)

/* Block structure used to simulate file access */

typedef struct memblock_struct
        {
                char *buffer;
                size_t next;
                size_t size;
        } Block;

/* Doc structure used for new and current document */

typedef struct s_doc
{
    int where;      /* LOCAL or REMOTE - direct file access? */
    int type;       /* TEXTDOCUMENT, ... */
    int port;       /* 80 for HTTP by default */
    int protocol;   /* HTTP, ... WAIS */
    char *buffer;   /* document's contents (including header */
    int hdrlen;     /* number of bytes in MIME header */
    long length;    /* in bytes (including header) */
    long height;    /* of viewable portion in pixels */
    long offset;    /* pixel offset to current position */
    char *host;     /* internet host name */
    char *path;     /* including optional search string */
    char *anchor;   /* named anchor point in document */
    char *url;      /* absolute URL reference */
    char *cache;    /* file name in shared cache */
} Doc;

#define TABSIZE         8

/* codes returned by button down handlers to indicate
   interest in receiving the corresponding button up event */

#define VOID            0
#define WINDOW          1
#define SCROLLBAR       2
#define TOOLBAR         3
#define STATUS          4

typedef struct s_button
    {
        int x;
        int y;
        unsigned int w;
        unsigned int h;
        char *label;
    } Button;


/* www.c */

int CloneSelf(void);
void Redraw(int x, int y, int w, int h);
void SetBanner(char *title);
void PollEvents(int block);
void BackDoc();

/* file.c */

char *Uncompress(char *buf, long *len);
int HasXVSuffix(char *name);
int NewDocumentType(void);
char *ParentDirCh(char *dir);
char *GetFile(char *name);
char *CurrentDirectory(void);

/* display.c */

#define WinTop    ToolBarHeight
#define WinBottom (win_height - statusHeight - sbar_width)
#define WinWidth  (win_width - sbar_width)
#define WinHeight (WinBottom - WinTop)
#define WinLeft   0
#define WinRight (WinLeft + WinWidth)
#define Abs2Win(n) (WinTop + n - PixelOffset)
#define Win2Abs(n) ((long)n + PixelOffset - WinTop)

void SetDisplayWin(Window aWin);
void SetDisplayGC(GC aGC);
void SetDisplayFont(XFontStruct *pFont_info);
void SetFont(GC gc, int fontIndex);
void SetEmphFont(GC gc, XFontStruct *pFont, XFontStruct *pNormal);
void NewBuffer(char *buf);
void DisplaySizeChanged(int all);

char *TextLine(char *txt);
long CurrentHeight(char *buf);
long DocHeight(char *buf, int *width);
int LineLength(char *buf);

int DeltaTextPosition(long h);
void MoveVDisplay(long h);
void MoveHDisplay(int indent);
void MoveLeftLine();
void MoveRightLine();
void MoveLeftPage();
void MoveRightPage();
void MoveToLeft();
void MoveToRight();
void MoveUpLine();
void MoveDownLine();
void MoveUpPage();
void MoveDownPage();
void MoveToStart();
void MoveToEnd();
void SlideHDisplay(int slider, int scrollExtent);
void SlideVDisplay(int slider, int scrollExtent);
void DisplayDoc(int x, int y, unsigned int w, unsigned int h);
void FindString(char *pattern, char **next);
void ToggleView(void);

/* scrollbar.c */

void SetScrollBarHPosition(int indent, int buffer_width);
void SetScrollBarVPosition(long offset, long buf_height);
void SetScrollBarWidth(int buffer_width);
void SetScrollBarHeight(long buff_height);

void SetScrollBarWin(Window aWin);
void SetScrollBarGC(GC aGC);

int ScrollButtonDown(int x, int y);
void ScrollButtonUp(int x, int y);
void ScrollButtonDrag(int x, int y);

void MoveHSlider(int indent, int buffer_width);
void MoveVSlider(long offset, long buf_height);
void DisplaySlider();
void DisplayScrollBar();

int AtStart(long offset);
int AtEnd(long offset);

/* status.c */

void HideAuthorizeWidget();
void ShowAbortButton(int n);
int StatusButtonDown(int button, int x, int y);
void StatusButtonUp(int x, int y);
void Beep();
void Announce(char *args, ...);
void Warn(char *args, ...);
void SetStatusWin(Window aWin);
void SetStatusGC(GC aGC);
void SetStatusFont(XFontStruct *pf);
int StatusActive(void);

void DrawOutSet(GC gc, int x, int y, unsigned int w, unsigned int h);
void DrawInSet(GC gc, int x, int y, unsigned int w, unsigned int h);
void DrawOutSetCircle(GC gc, int x, int y, unsigned int w, unsigned int h);
void DrawInSetCircle(GC gc, int x, int y, unsigned int w, unsigned int h);
void DisplayStatusBar();
void SetStatusString(char *s);
void ClearStatus();
void RestoreStatusString(void);
void SaveStatusString(void);
int IsEditChar(char c);
void EditChar(char c);
void MoveStatusCursor(int key);
void GetAuthorization(int mode, char *host);
char *UserName(char *who);
char *PassStr(char *who);

/* toolbar.c */

void PaintVersion(int bad);
void SetToolBarWin(Window aWin);
void SetToolBarGC(GC aGC);
void SetToolBarFont(XFontStruct *pf);
void DisplayToolBar();
int ToolBarButtonDown(int x, int y);
void ToolBarButtonUp(int x, int y);
void PrintDoc();
void SaveDoc(char *file);
void DisplayExtDocument(char *buffer, long length, int type, char *path);

/* parsehtml.c */

#define MAXVAL  1024

/* alignment codes */
#define ALIGN_TOP       0
#define ALIGN_MIDDLE    1
#define ALIGN_BOTTOM    2
#define ALIGN_LEFT      3
#define ALIGN_CENTER    4
#define ALIGN_RIGHT     5
#define ALIGN_JUSTIFY   6

/* the html token codes */

#define ENTITY          -4
#define WHITESPACE      -3     /* the specific char */
#define PCDATA          -2     /* the specific char */
#define ENDDATA         -1

#define ENDTAG          (1<<7) /*  ORed with TAG code */
#define IsTag(tag)      (tag >= 0)

#define UNKNOWN         0
#define TAG_ANCHOR      1       /* EN_TEXT */
#define TAG_BOLD        2       /* EN_TEXT */
#define TAG_DL          3       /* EN_LIST */
#define TAG_DT          4       /* EN_DEFLIST */
#define TAG_DD          5       /* EN_DEFLIST */
#define TAG_H1          6       /* EN_HEADER */
#define TAG_H2          7       /* EN_HEADER */
#define TAG_H3          8       /* EN_HEADER */
#define TAG_H4          9       /* EN_HEADER */
#define TAG_H5          10      /* EN_HEADER */
#define TAG_H6          11      /* EN_HEADER */
#define TAG_ITALIC      12      /* EN_TEXT */
#define TAG_IMG         13      /* EN_TEXT */
#define TAG_LI          14      /* EN_LIST */
#define TAG_OL          15      /* EN_LIST */
#define TAG_P           16      /* EN_BLOCK */
#define TAG_TITLE       17      /* EN_SETUP */
#define TAG_UNDERLINE   18      /* EN_TEXT */
#define TAG_UL          19      /* EN_LIST */
#define TAG_HEAD        20      /* EN_SETUP */
#define TAG_BODY        21      /* EN_MAIN */
#define TAG_HR          22      /* EN_BLOCK */
#define TAG_ADDRESS     23      /* EN_BLOCK */
#define TAG_BR          24      /* EN_TEXT */
#define TAG_STRIKE      25      /* EN_TEXT */
#define TAG_PRE         26      /* EN_BLOCK */
#define TAG_CITE        27      /* EN_TEXT */
#define TAG_CODE        28      /* EN_TEXT */
#define TAG_TT          29      /* EN_TEXT */
#define TAG_EM          30      /* EN_TEXT */
#define TAG_STRONG      31      /* EN_TEXT */
#define TAG_KBD         32      /* EN_TEXT */
#define TAG_SAMP        33      /* EN_TEXT */
#define TAG_DFN         34      /* EN_TEXT */
#define TAG_Q           35      /* EN_TEXT */
#define TAG_QUOTE       36      /* EN_BLOCK */
#define TAG_ISINDEX     37      /* EN_SETUP */
#define TAG_FIG         38      /* EN_TEXT */
#define TAG_INPUT       39      /* EN_TEXT */
#define TAG_SELECT      40      /* EN_TEXT */
#define TAG_OPTION      41      /* EN_TEXT */
#define TAG_TEXTAREA    42      /* EN_TEXT */
#define TAG_TABLE       43      /* EN_BLOCK */
#define TAG_TR          44      /* EN_TABLE */
#define TAG_TH          45      /* EN_TABLE */
#define TAG_TD          46      /* EN_TABLE */
#define TAG_CAPTION     47      /* EN_BLOCK */
#define TAG_ADDED       48      /* EN_TEXT */
#define TAG_REMOVED     49      /* EN_TEXT */
#define TAG_MATH        50      /* EN_TEXT */
#define TAG_MARGIN      51      /* EN_TEXT */
#define TAG_ABSTRACT    52      /* EN_BLOCK */
#define TAG_BLOCKQUOTE  53      /* EN_BLOCK */
#define TAG_VAR         54      /* EN_TEXT */

/* entity classes */

#define EN_UNKNOWN         0
#define EN_TEXT         (1 << 0)
#define EN_BLOCK        (1 << 1)
#define EN_LIST         (1 << 2)
#define EL_DEFLIST      (1 << 3)
#define EN_HEADER       (1 << 4)
#define EN_SETUP        (1 << 5)
#define EN_MAIN         (1 << 6)
#define EN_TABLE        (1 << 7)

/* input field types */

#define TEXTFIELD       0x00
#define CHECKBOX        0x10
#define RADIOBUTTON     0x20
#define OPTIONLIST      0x40
#define SUBMITBUTTON    0x60
#define RESETBUTTON     0x70

#define CHECKED         0x80    /* flags field as 'active' */
#define IN_ERROR        0x40    /* field flagged as in error */
#define DISABLED        0x20    /* field is greyed out */
#define MULTIPLE        0x10    /* multiple selections are allowed */

/* window margin and indents */

#define MAXMARGIN       (win_width - sbar_width - 4)
#define MININDENT       4
#define NESTINDENT      5
#define GLINDENT        30
#define OLINDENT        3
#define LINDENT         8
#define GLGAP           4


typedef struct h_link
        {
            int x;
            int y;
            unsigned int w;
            unsigned int h;
            int continuation;
            char *href;
            int reflen;
        } HyperLink;

#define MAXLINKS        512

/* structure for table widths - index from 1
   upwards as zeroth entry used for book keeping */

typedef struct t_width
        {
            int left;
            int right;
            int min;   /* reused for row height in 2nd pass */
            int max;   /* we track min/max width of columns */
            int rows;  /* row span count for each column */
        } ColumnWidth;

/* store current/max number of columns in first element */

#define COLS(widths)    widths->min
#define MAXCOLS(widths) widths->max
#define NCOLS   15

int GetWord(char *p, int *len, int max);
void SwallowAttributes();
int GetToken(void);
char *TitleText(char *buf);
void ClipToWindow(void);
void DisplayHTML(int x, int y, unsigned int w, unsigned int h);
void ParseSGML(int mode, long *offset, char **data, long window, long bottom, char *target);
long DeltaHTMLPosition(long h);

void OpenDoc(char *name, char *who, int where);
void ReloadDoc(char *name, char *who);
int WindowButtonDown(int x, int y);
void WindowButtonUp(int shifted, int x, int y);
void SearchIndex(char *keywords);

/* html.c */

long ParseHTML(int *width);
char *TopStr(Frame *frame);

/* http.c */

#define HTTP_PORT       80
#define OLD_PORT        2784
#define GOPHERPORT      70

#define SCHSIZ 12
#define HOSTSIZ 48
#define PATHSIZ 512
#define ANCHORSIZ 64

char *safemalloc(int n);
int IsHTMLDoc(char *p, int len);
char *MyHostName(void);
void InitCurrent(char *path);
char *UnivRefLoc(Doc *doc);
char *ParseReference(char *s, int local);
int HeaderLength(char *buf, int *type);
char *GetDocument(char *href, char *who, int local);
char *SearchRef(char *keywords);

/* cache.c */

int CloneHistoryFile(void);
void SetCurrent();
int ViewStack(void);
int PushDoc(long offset);
char *PopDoc(long *where);
char *GetCachedDoc(void);
int StoreNamePW(char *who);
char *RetrieveNamePW(void);

/* ftp.c */

char *CloseFTP(void);
char *GetFileData(int socket);
char *GetFTPdocument(char *host, char *path, char *who);

/* nntp.c */

char *GetNewsDocument(char *host, char *path);

/* tcp.c */

#define GATEWAYPORT 2785

void Pause(int delay);
int XPSend(int skt, char *data, int len, int once);
int XPRecv(int skt, char *msg, int len);
int Connect(int s, char *host, int port, int *ViaGateway);
char *GetData(int socket, int *length);

/* entities.c */

int entity(char *name, int *len);
void InitEntities(void);

/* image.c */

typedef struct image_struct
        {
            struct image_struct *next;
            char *url;
            Pixmap pixmap;
            unsigned long *pixels;
            int npixels;           
            unsigned int width;
            unsigned int height;
        } Image;

int InitImaging(int ColorStyle);
unsigned long GreyColor(unsigned int grey);
unsigned long StandardColor(unsigned char red, unsigned char green, unsigned char blue);
unsigned char *CreateBackground(unsigned int width, unsigned int height, unsigned int depth);
Image *GetImage(char *href, int hreflen);
void FreeImages(int cloned);
void ReportStandardColorMaps(Atom which_map);
void ReportVisuals(void);
Visual *BestVisual(int class, int *depth);
void PaintFace(int happy);
void MakeFaces(unsigned int depth);

/* gif.c */

unsigned char *LoadGifImage(Image *image, Block *bp, unsigned int depth);

/* forms.c */

typedef struct option_struct
    {
        struct option_struct *next; /* linked list of options */
        unsigned char flags;        /* CHECKED, DISABLED */
        char *label;
        int j;                      /* option index number */
    } Option;

/* note font is held in lower 4 bits of flags */

typedef  struct field_struct
    {
        struct field_struct *next;  /* linked list of fields */
        struct form_struct  *form;  /* the parent form */
        Option *options;            /* linked list of options */
        unsigned char type;         /* RADIOBUTTON, CHECKBOX, ... */
        unsigned char flags;        /* CHECKED, IN_ERROR, DISABLED */
        char *name;                 /* field name attribute */
        char *value;                /* malloc'ed buffer */
        int i;                      /* field number in form */
        int j;                      /* option index */
        int bufsize;                /* current buffer size */
        int buflen;                 /* length of useful data */
        int width;                  /* in pixels */
        int height;                 /* in pixels */
        int x;                      /* from left of document */
        int x_indent;               /* for text fields */
        int y_indent;
        long baseline;              /* from start of document */
        int object;                 /* offset in paint stream */
    } Field;

typedef struct form_struct
    {
        struct form_struct *next;   /* linked list of forms */
        int i;                      /* track i'th field in form */
        char *action;               /* URL from ACTION attribute */
        Field *fields;              /* linked list of fields */
    } Form;

char *strdup2(char *s, int len);
void FreeForms(void);
void FreeFrames(Frame *frame);
Form *GetForm(char *url, int len);
Field *GetField(Form *form, int type, int x, char *name, int nlen,
               char *value, int vlen, int rows, int cols, int flags);
Option *AddOption(Field *field, int flags, char *label, int len);
int ClickedInField(GC gc, int baseline, Field *field, int x, int y, int event);
void PaintField(GC gc, int baseline, Field *field);
void PaintDropDown(GC gc, Field *field);
int ClickedInDropDown(GC gc, Field *field, int event, int x, int y);
Form *DefaultForm(void);
void PaintCross(GC gc, int x, int y, unsigned int w, unsigned int h);
void PaintTickMark(GC gc, int x, int y, unsigned int w, unsigned int h);
void ShowPaint(int npaint, int nobjs);
int RegisterDoc(char *buf);
int QueryCacheServer(char *command, char **response);


