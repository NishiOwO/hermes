/* tidy.c - reformats HTML files to match DTD

On an HP-UX system this compiles with:

    cc -Aa -g -D_HPUX_SOURCE  -o tidy tidy.c

The program reformats html files to remove faults and make them
consistent with current practice. Please email your comments and
suggestions for improvements to dsr@hplb.hpl.hp.com

Useage:     tidy file file file ...

    (c) April 1994 Dave Raggett
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#define VERSION         "1.0a"

#define IsWhite(c)  (c == ' ' || c == '\n' || c == '\t' || c == '\f' || c == '\r')

#define max(a, b)   ((a) > (b) ? (a) : (b))
#define min(a, b)   ((a) < (b) ? (a) : (b))

/* alignment codes */
#define ALIGN_TOP       0
#define ALIGN_MIDDLE    1
#define ALIGN_BOTTOM    2
#define ALIGN_LEFT      3
#define ALIGN_CENTER    4
#define ALIGN_RIGHT     5
#define ALIGN_JUSTIFY   6

/* the html token codes */

#define COMMENT         -5     /* <-- SGML Comments --> */
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
#define TAG_FORM        48      /* EN_BLOCK */

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
#define EN_COMMENT      (1 << 8)

/* input field types */

#define TEXTFIELD       0x00
#define CHECKBOX        0x10
#define RADIOBUTTON     0x20
#define OPTIONLIST      0x40

#define CHECKED         0x80    /* flags field as 'active' */
#define IN_ERROR        0x40    /* field flagged as in error */
#define DISABLED        0x20    /* field is greyed out */
#define SUBMIT          0x10    /* submit button */

/* window margin and indents */

#define MAXMARGIN       78
#define MININDENT       4
#define NESTINDENT      5
#define GLINDENT        30
#define OLINDENT        3
#define LINDENT         8
#define GLGAP           4


#define LBUFSIZE 1024

int comment = 0;
int debug = 0;

/*
    The current top line is displayed at the top of the window,the pixel
    offset is the number of pixels from the start of the document.
*/

FILE *fp;      /* global file pointer for writing cleaned version */
char *bufptr;  /* parse position in the HTML buffer */
char *lastbufptr;  /* keep track of last position to store delta's */

int overwrite;
int preformatted = 0;

static int EndTag, TagLen;
static int TokenClass, TokenValue, Token;
static char *EntityValue;

int baseline;       /* from top of line */
long TermTop, TermBottom;

long LastOffset;
long PixOffset;     /* current offset from start of document */
long PrevOffset;    /* keep track for saving delta's */
long LastLIoffset;  /* kludge for <LI><LI> line spacing */
long ViewOffset;    /* for toggling between HTML/TEXT views */

int Here;
int HTMLInit = 0;
long figEnd;

char *LastBufPtr, *StartOfLine, *StartOfWord; /* in HTML document */
static int LineLen, LineWidth, WordStart, WordWidth;
static char LineBuf[LBUFSIZE]; /* line buffer */

int namelen, hreflen;
char *name, *href;

#define NOBREAK 0
#define BREAK   1

char *TokenName[] =
{
    "",
    "A",
    "B",
    "DL",
    "DT",
    "DD",
    "H1",
    "H2",
    "H3",
    "H4",
    "H5",
    "H6",
    "I",
    "IMG",
    "LI",
    "OL",
    "P",
    "TITLE",
    "U",
    "UL",
    "HEAD",
    "BODY",
    "HR",
    "ADDRESS",
    "BR",
    "S",
    "PRE",
    "CITE",
    "CODE",
    "TT",
    "EM",
    "STRONG",
    "KBD",
    "SAMP",
    "DFN",
    "Q",
    "BLOCKQUOTE",
    "ISINDEX",
    "INPUT",
    "SELECT",
    "OPTION",
    "TEXTAREA",
    "FORM"
};

#if 0
void Warn(char *args, ...)
{
    va_list ap;
    char buf[256];

    if (debug)
    {
        va_start(ap, args);
        vsprintf(buf, args, ap);
        va_end(ap);
        fprintf(stderr, "%s\n", buf);
    }
}
#endif

void PrintEntity(void)
{
    int c;

    for (;;)
    {
        c = *bufptr++;

        if (c == '\0' || c == ';')
            break;

        LineBuf[LineLen++] = c;
    }

    LineBuf[LineLen++] = ';';
}

void PrintString(int len)
{
    fwrite((char *)LineBuf, 1, len, fp);
}

void EndOfLine(void)
{
    putc('\n', fp);
}

void PrintStartTag(int token)
{
    fprintf(fp, "<%s>", TokenName[token]);
}

void PrintEndTag(int token)
{
    fprintf(fp, "</%s>", TokenName[token]);
}

/* check if current word forces word wrap and flush line as needed */
void WrapIfNeeded(int WrapLeftMargin, int WrapRightMargin)
{
    int WordLen, space, rightMargin;
    long line;

    rightMargin = WrapRightMargin;

    LineBuf[LineLen] = '\0';  /* debug*/
    WordLen = LineLen - WordStart;
    WordWidth = WordLen;
    space = 1;
    line = 1;

    if (WordStart == 0 && Here + WordWidth > rightMargin)
    {
        /* word wider than window */
        if (WordWidth > rightMargin - WrapLeftMargin)
        {
            PrintString(WordLen);
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

        Here = WrapLeftMargin;
        EndOfLine();
    }
    else if (WordStart > 0 && Here + LineWidth + space + WordWidth > rightMargin)
    {
        PrintString(WordStart-1);
        Here = WrapLeftMargin;
        EndOfLine();
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

void FlushLine(int linebreak, int WrapLeftMargin, int WrapRightMargin)
{
    int WordLen;

    if (preformatted)
    {
        WordLen = LineLen - WordStart;
        LineWidth = WordLen;
    }
    else if (LineLen > 0)
        WrapIfNeeded(WrapLeftMargin, WrapRightMargin);

    if (LineLen > 0)
    {
        /* watch out for single space as leading spaces
           are stripped by CopyLine */

        if (LineLen > 1 || LineBuf[0] != ' ')
            PrintString(LineLen);

        if (linebreak)
        {
            Here = WrapLeftMargin;
            LineWidth = LineLen = WordStart = 0;
            EndOfLine();
        }
        else
        {
            Here += LineWidth;
            LineWidth = LineLen = WordStart = 0;
        }
    }
    else if (linebreak && Here > WrapLeftMargin)
    {
        Here = WrapLeftMargin;
        EndOfLine();
    }

    StartOfLine = StartOfWord = bufptr;
}

/* needs to cope with > in quoted text for ' and " */
void SwallowAttributes(void)
{
    int c, dash;

    dash = 0;

    while ((c = *bufptr) && c != '>')
    {
        if (comment)
        {
            if (c == '-')
            {
                if (dash)
                    comment = dash = 0;
                else
                    dash = 1;
            }
            else
                dash = 0;
        }
        else if (c == '-')
        {
            if (dash)
            {
                comment = 1;
                dash = 0;
            }
            else
                dash = 1;
        }
        else if (c == '>')
            break;

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

            if (len == 7 && strncasecmp(s, "address", len) == 0)
            {
                TokenClass = EN_BLOCK;
                return TAG_ADDRESS;
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
            if (len == 2 && strncasecmp(s, "code", len) == 0)
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

            if (len == 4 && strncasecmp(s, "form", len) == 0)
            {
                TokenClass = EN_BLOCK;
                return TAG_FORM;
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
                TokenClass = EN_LIST;
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

            if (len == 5 && strncasecmp(s, "strong", len) == 0)
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

  next_char: /* hack for entity names */
    c = *bufptr;
    TokenValue = c;

    if (c == '<')
    {
        if (bufptr[1] == '-' && bufptr[2] == '-')
        {
            Token = UNKNOWN;
            TokenClass = EN_COMMENT;
            comment = 1;
            bufptr += 3;
            return Token;
        }
        else if (isalpha(bufptr[1]))
        {
            Token = RecogniseTag();
            bufptr += TagLen;   /* to first char after tag name */
            return Token;
        }
        else if ((bufptr[1] == '/' || bufptr[1] == '!') && isalpha(bufptr[2]))
        {
            Token = RecogniseTag();
            bufptr += TagLen;   /* to first char after tag name */
            return Token;
        }
    }

    TokenClass = EN_TEXT;
    EndTag = 0;

    /* app needs to advance bufptr past entity: &fred; */
    if (c == '&' && isalpha(bufptr[1]))
    {
        Token = ENTITY;
        return Token;
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

    fprintf(fp, " ");
    fwrite(attr, 1, *len, fp);
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

    if (!delim)
        delim = '"';

    if (*len > 0)
    {
        fprintf(fp, "=%c", delim);
        fwrite(value, 1, *len, fp);
        fprintf(fp, "%c", delim);
    }
    return value;
}

void ParseAttributes(int tag)
{
    int c, n, m;
    char *attr, *value;

    fprintf(fp, "<%s", TokenName[tag]);

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
    }

    fprintf(fp, ">");
}

void ParseTitle(int implied)
{
    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        SwallowAttributes();

    fprintf(fp, "<TITLE>");

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

        if (Token == ENTITY)
        {
            PrintEntity();
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
    fprintf(fp, "%s</TITLE>\n", LineBuf);
}

void ParseSetUp(int implied)
{
    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    fprintf(fp, "<HEAD>\n");

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
            ParseTitle(0);
            continue;
        }

        if (Token == TAG_ISINDEX)
        {
            SwallowAttributes();
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

    fprintf(fp, "</HEAD>\n\n");
}

void ParseOption(int implied)
{
    int width;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        SwallowAttributes();

    fprintf(fp, "<OPTION>");

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
}

void ParseSelect(int implied)
{
    int type, nlen, vlen, size, flags;
    char *name, *value;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        ParseAttributes(TAG_SELECT);
    else
    {
        name = value = "";
        nlen = vlen = flags = 0;        nlen = vlen = flags = 0; 
    }

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
            ParseOption(0);
            continue;
        }

        if (Token == PCDATA)
        {
            ParseOption(1);
            continue;
        }

     /* unexpected tag so terminate element */

        UnGetToken();
        break;
    }    
}

/* check for anchors which enclose headers as the parser
   will treat these as an empty anchor followed by a header */
int AnchorEnclosesHeader()
{
    int n;
    char *lastptr, *p, *q, *r, buf[1024];

    lastptr = bufptr;

    SwallowAttributes();
    while (GetToken() == WHITESPACE);
    UnGetToken();

    q = bufptr;

    /* if <A> is followed by something that isn't
       allowed inside an <A>..</A> then swap them over */

    if (TokenClass != EN_TEXT && !EndTag && Token != TAG_ANCHOR)
    {
        /* bufptr -> <H1> ... etc */

        for (r = lastptr; *--r != '<';);

        for (p = buf; (*p++ = *bufptr++) != '>';);
        *p = '\0'; /* for debugging */
        n = p - buf;

        memmove(r + (bufptr-q), r, q - r);
        p = buf;
        bufptr = r;

        while(n-- > 0)
            *r++ = *p++;
        
        return 1;
    }

    bufptr = lastptr;
    return 0;
}

void ParseEmph(int left, int right)
{
    int ThisToken, WordLen, hreflen, namelen, nlen, vlen, size,
        type, align, rows, cols, delta, ismap, width, height, flags;
    char *href, *name, *value, *p;

    if (EndTag)
    {
        SwallowAttributes();
        PrintEndTag(Token);
        return;
    }

    ThisToken = Token;

    switch (ThisToken)
    {
        case TAG_ANCHOR:
            if (AnchorEnclosesHeader())
                return;

            ParseAttributes(ThisToken);
            break;

        case TAG_TT:
        case TAG_CODE:
        case TAG_SAMP:
        case TAG_KBD:
            PrintStartTag(ThisToken);
            SwallowAttributes();
            break;

        case TAG_ITALIC:
        case TAG_EM:
        case TAG_DFN:
        case TAG_CITE:
        case TAG_Q:
            PrintStartTag(ThisToken);
            SwallowAttributes();
            break;

        case TAG_BOLD:
        case TAG_STRONG:
            PrintStartTag(ThisToken);
            SwallowAttributes();
            break;

        case TAG_UNDERLINE:
            PrintStartTag(ThisToken);
            SwallowAttributes();
            break;

        case TAG_STRIKE:
            PrintStartTag(ThisToken);
            SwallowAttributes();
            break;

        case TAG_TEXTAREA:  /* a short term bodge */
            ParseAttributes(ThisToken);
            StartOfLine = StartOfWord = bufptr;
            LineLen = LineWidth = WordStart = 0;
            return;

        case TAG_INPUT:
            ParseAttributes(ThisToken);
            StartOfLine = StartOfWord = bufptr;
            LineLen = LineWidth = WordStart = 0;
            return;

        case TAG_SELECT:
            ParseSelect(0);
            StartOfLine = StartOfWord = bufptr;
            LineLen = LineWidth = WordStart = 0;
            return;

        case TAG_OPTION:
            ParseSelect(1);
            StartOfLine = StartOfWord = bufptr;
            LineLen = LineWidth = WordStart = 0;
            return;

        case TAG_BR:
            PrintStartTag(ThisToken);
            SwallowAttributes();
            Here = left;
            EndOfLine();
            StartOfLine = StartOfWord = bufptr;
            LineLen = LineWidth = WordStart = 0;
            return;

        case TAG_IMG:
            ParseAttributes(ThisToken);
            StartOfLine = StartOfWord = bufptr;
            LineLen = LineWidth = WordStart = 0;
            return;

        default:
            PrintStartTag(ThisToken);
            SwallowAttributes();
            break;
    }

    StartOfLine = StartOfWord = bufptr;
    LineLen = LineWidth = WordStart = 0;

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

        if (Token == ENTITY)
        {
            PrintEntity();
            continue;
        }

        if (TokenClass != EN_TEXT)
        {
            UnGetToken();
            break;
        }

        if (Token == WHITESPACE)
        {
            if (preformatted)
            {
                if (TokenValue == '\n')
                    FlushLine(BREAK, left, right);
                else
                {
                    if (LineLen < LBUFSIZE - 1)
                        LineBuf[LineLen++] = ' ';
                }
                continue;
            }

            while (GetToken() == WHITESPACE);
            UnGetToken();

            /* check that we have a word */

            if ((WordLen = LineLen - WordStart) > 0)
                WrapIfNeeded(left, right);

            if (LineLen < LBUFSIZE - 1)
                LineBuf[LineLen++] = ' ';

            WordStart = LineLen;
            StartOfWord = bufptr;
            continue;
        }

        if (IsTag(Token))
        {
            FlushLine(NOBREAK, left, right);
            ParseEmph(left, right);
            continue;
        }

        /* must be PCDATA */

        if (LineLen < LBUFSIZE - 1)
            LineBuf[LineLen++] = TokenValue;
    }    

    LineBuf[LineLen] = '\0';
    FlushLine(NOBREAK, left, right);
    PrintEndTag(ThisToken);
}

void ParseHeader(int left, int right)
{
    int HeaderTag, WordLen;

    SwallowAttributes();

    if (EndTag)
        return;

    fprintf(fp, "\n<H%d>", Token - TAG_H1 + 1);

    HeaderTag  = Token;
    Here = left;
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

        if (Token == ENTITY)
        {
            PrintEntity();
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
                WrapIfNeeded(left, right);

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

            FlushLine(NOBREAK, left, right);
            ParseEmph(left, right);
            continue;
        }

        /* must be PCDATA */

        if (LineLen < LBUFSIZE - 1)
            LineBuf[LineLen++] = TokenValue;
    }    

    LineBuf[LineLen] = '\0';
    FlushLine(NOBREAK, left, right);
    fprintf(fp, "</H%d>\n\n", HeaderTag - TAG_H1 + 1);
    PixOffset += 1;
}

void ParsePara(int implied, int left, int right)
{
    int WordLen, LastToken;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }

    if (!implied)
        SwallowAttributes();

    if (Here < left)
        Here = left;

    /* skip leading white space - subsequently contigous
       white space is compressed to a single space */
    while (GetToken() == WHITESPACE);
    UnGetToken();

    StartOfLine = StartOfWord = bufptr;
    LineLen = LineWidth = WordStart = 0;

    for (;;)
    {
        LastToken = Token;
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

        if (Token == TAG_P)
        {
            SwallowAttributes();
            FlushLine(NOBREAK, left, right);
            Here = left; LineLen = LineWidth = WordStart = 0;
            fprintf(fp, "\n<P>");
            continue;;
        }

        if (Token == ENTITY)
        {
            PrintEntity();
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
                WrapIfNeeded(left, right);

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

            FlushLine(NOBREAK, left, right);
            ParseEmph(left, right);
            continue;
        }

        /* must be PCDATA */

        if (LineLen < LBUFSIZE - 1)
            LineBuf[LineLen++] = TokenValue;
    }    

    LineBuf[LineLen] = '\0';


    FlushLine((LastToken != TAG_P ? BREAK : NOBREAK), left, right);
    PixOffset += 1;
}

/* advance declaration due for nested lists */
void ParseUL(int implied, int depth, int left, int right);
void ParseOL(int implied, int depth, int left, int right);
void ParseDL(int implied, int left, int right);

void ParseLI(int implied, int depth, int seq, int left, int right)
{
    int indent, w;
    long y;
    char buf[16];

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }
    else
        fprintf(fp, "<LI>");

    indent = 3;
    y = PixOffset;

    if (!implied)
        SwallowAttributes();

    Here = left + indent/3;

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

            ParseUL(0, depth, left + indent, right);
            continue;
        }

        if (Token == TAG_OL)
        {
            UnGetToken();

            if (EndTag)
                break;

            ParseOL(0, depth, left + indent, right);
            continue;
        }

        if (Token == TAG_DL)
        {
            UnGetToken();

            if (EndTag)
                break;

            ParseDL(0, left + indent, right);
            continue;
        }

        if (Token == TAG_DT || Token == TAG_DD)
        {
            UnGetToken();
            ParseDL(1, left + indent, right);
            continue;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == TAG_P)
        {
            ParsePara(0, left + indent, right);
            continue;
        }

        if (Token == TAG_HR)
        {
            SwallowAttributes();

            if (EndTag)
                continue;

            EndOfLine();
            continue;
        }

        if (TokenClass == EN_TEXT)
        {
            UnGetToken();
            ParsePara(0, left + indent, right);
            continue;
        }

     /* unexpected tag so terminate element */

        UnGetToken();
        break;
    }    

    /* kludge to cope with an <LI> element with no content */

    if (y == PixOffset)
        PixOffset += 1;
    EndOfLine();
}

void ParseUL(int implied, int depth, int left, int right)
{
    if (EndTag)
    {
        SwallowAttributes();
        return;
    }
    else
        fprintf(fp, "\n<UL>\n\n");

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
            ParseLI(0, depth, 0, left, right);
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
            ParseLI(1, depth, 0, left, right);
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
            ParseLI(1, depth, 0, left, right);
            continue;
        }

     /* unexpected tag so terminate element */

        UnGetToken();
        break;
    }    

    fprintf(fp, "</UL>\n\n");
    Here = left;
}

void ParseOL(int implied, int depth, int left, int right)
{
    int seq;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }
    else
        fprintf(fp, "\n<OL>\n\n");

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
            ParseLI(0, depth, seq, left, right);
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
            ParseLI(1, depth, seq, left, right);
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
            ParseLI(1, depth, seq, left, right);
            continue;
        }

     /* unexpected tag so terminate element */

        UnGetToken();
        break;
    }    

    fprintf(fp, "</OL>\n\n");
    Here = left;
}

void ParseDT(int implied, int left, int right)
{
    int WordLen;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }
    else
        fprintf(fp, "<DT>");

    if (!implied)
        SwallowAttributes();

    Here = left;

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
                WrapIfNeeded(left, right);

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

            FlushLine(NOBREAK, left, right);
            ParseEmph(left, right);
            continue;
        }

        if (Token == ENTITY)
        {
            PrintEntity();
            continue;
        }

        /* must be PCDATA */

        if (LineLen < LBUFSIZE - 1)
            LineBuf[LineLen++] = TokenValue;
    }    

    LineBuf[LineLen] = '\0';
    FlushLine(NOBREAK, left, right);
    EndOfLine();
    Here += 5;
}

void ParseDD(int implied, int left, int right)
{
    long y;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }
    else
        fprintf(fp, "<DD>");

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
            ParseUL(0, 0, left, right);
            continue;
        }

        if (Token == TAG_OL)
        {
            ParseOL(0, 0, left, right);
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
            ParseUL(1, 0, left, right);
            continue;
        }

        if (Token == TAG_DL)
        {
            if (PixOffset == y) /* force a line break */
            {
                Here = left;
                EndOfLine();
            }

            ParseDL(0, left, right);
            Here = left;
            continue;
        }

        if (Token == TAG_HR)
        {
            SwallowAttributes();

            if (EndTag)
                continue;

            EndOfLine();
            continue;
        }

        if (Token == TAG_P)
        {
            ParsePara(0, left, right);
            continue;
        }

        if (TokenClass == EN_TEXT || Token == ENTITY)
        {
            UnGetToken();
            ParsePara(1, left, right);
            continue;
        }

     /* unexpected tag so terminate element */

        UnGetToken();
        break;
    }

    /* kludge to cope with an <DD> element with no content */

    if (y == PixOffset)
        PixOffset += 1;

    EndOfLine();
}

void ParseDL(int implied, int left, int right)
{
    int indent, LastToken;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }
    else
        fprintf(fp, "\n<DL>\n\n");

    if (!implied)
        SwallowAttributes();

    LastToken = TAG_DL;
    indent = 2;

    for (;;)
    {
        while (GetToken() == WHITESPACE);

        if (Token == TAG_DL && EndTag)
        {
            SwallowAttributes();

            if (LastToken == TAG_DT)
                EndOfLine();

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
                EndOfLine();

            ParseDT(0, left, right);
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

            ParseDD(0, left + indent, right);
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

            UnGetToken();
            ParseDD(1, left + indent, right);
            LastToken = TAG_DD;
            continue;
        }

     /* unexpected tag so terminate element */

        UnGetToken();
        break;
    }

    fprintf(fp, "</DL>\n\n");
    Here = left;    
}

void ParseAddress(int implied, int left, int right)
{
    if (EndTag)
    {
        SwallowAttributes();
        return;
    }
    else
        fprintf(fp, "<ADDRESS>\n");

    if (!implied)
        SwallowAttributes();

    Here = left;

    for (;;)
    {
        while (GetToken() == WHITESPACE);

        if (Token == TAG_ADDRESS && EndTag)
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
            ParsePara(0, left, right);
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
            ParsePara(1, left, right);
            continue;
        }

     /* unexpected tag so terminate element */

        UnGetToken();
        break;
    }    

    fprintf(fp, "</ADDRESS>\n");
}

void ParsePRE(int implied, int left, int right)
{
    int WordLen;

    if (EndTag)
    {
        SwallowAttributes();
        return;
    }
    else
        fprintf(fp, "<PRE>\n");

    if (!implied)
        SwallowAttributes();

    Here = left;
    preformatted = 1;

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

        if (Token == ENTITY)
        {
            PrintEntity();
            continue;
        }

        if (TokenClass != EN_TEXT)
        {
            UnGetToken();
            break;
        }

        if (Token == WHITESPACE && TokenValue == '\n')
        {
            FlushLine(BREAK, left, right);
            continue;
        }

        if (IsTag(Token))
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            FlushLine(NOBREAK, left, right);
            ParseEmph(left, right);
            continue;
        }

        /* must be PCDATA */

        if (LineLen < LBUFSIZE - 1)
            LineBuf[LineLen++] = TokenValue;
    }    

    LineBuf[LineLen] = '\0';
    FlushLine(BREAK, left, right);
    fprintf(fp, "</PRE>\n");
    preformatted = 0;
}

void ParseBody(int implied, int left, int right)
{
    if (EndTag)
    {
        SwallowAttributes();
        return;
    }
    else
        fprintf(fp, "<BODY>\n");

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

            ParseHeader(left, right);
            continue;
        }

        if (Token == UNKNOWN)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == TAG_FORM)
        {
            SwallowAttributes();
            continue;
        }

        if (Token == TAG_UL)
        {
            ParseUL(0, 0, left, right);
            continue;
        }

        if (Token == TAG_OL)
        {
            ParseOL(0, 0, left, right);
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
            ParseUL(1, 0, left, right);
            continue;
        }

        if (Token == TAG_DL)
        {
            ParseDL(0, left, right);
            continue;
        }

        if (Token == TAG_DT || Token == TAG_DD)
        {
            if (EndTag)
            {
                SwallowAttributes();
                continue;
            }

            UnGetToken();
            ParseDL(1, left, right);
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
            EndOfLine();
            continue;
        }

        if (Token == TAG_PRE)
        {
            ParsePRE(0, left, right);
            continue;
        }

        if (Token == TAG_P)
        {
            ParsePara(0, left, right);
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
            ParsePara(1, left, right);
            continue;
        }

        if (Token == TAG_ADDRESS)
        {
            ParseAddress(0, left, right);
            continue;
        }

        if (Token == ENDDATA)
        {
            UnGetToken();
            break;
        }
    }

    fprintf(fp, "</BODY>\n");
}

long ParseHTML(char *buffer)
{
    int c, WordLen;

    PixOffset = LastOffset = 0;
    LastBufPtr = bufptr = buffer;
    comment = preformatted = 0;

    fprintf(fp, "<!DOCTYPE HTML SYSTEM \"HTML.DTD\">\n");

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
        ParseSetUp(0);
    else
    {
        UnGetToken();
        ParseSetUp(1);
    }

    while (GetToken() == WHITESPACE);

    if (Token == TAG_BODY)
        ParseBody(0, MININDENT, MAXMARGIN);
    else
    {
        UnGetToken();
        ParseBody(1, MININDENT, MAXMARGIN);
    }

    return PixOffset;
}


char *ReadFile(char *file)
{
    int fd, err;
    size_t size, len;
    char *buf;

    if ((fd = open(file, O_RDONLY)) == -1)
    {
        fprintf(stderr, "Can't open: %s\n", file);
        return NULL;
    }

    size = lseek(fd, 0L, SEEK_END);

    buf = malloc(1 + size);
    lseek(fd, 0L, SEEK_SET);
    len = read(fd, (void *)buf, size);
    err = errno;
    close(fd);

    if (len < 0)
    {
        fprintf(stderr, "Can't read data for %s - errno %d\n", file, err);
        free(buf);
        return NULL;
    }

    buf[len] = '\0';
    return buf;
}

void Clean(char *file)
{
    char *buf;

    if (!overwrite)
    {
        if ((buf = ReadFile(file)) != NULL)
        {
            fp = stdout;
            ParseHTML(buf);
            free(buf);
        }
    }
    else if ((buf = ReadFile(file)) != NULL &&
        (fp = fopen(file, "w")) != NULL)
    {
        ParseHTML(buf);
        fclose(fp);
        free(buf);
    }
    else
        fprintf(stderr, "\n\n*** Can't open file: %s\n\n", file);
}

Help()
{
    printf("\nTidy version %s,  (c) Dave Raggett April 1994\n\n", VERSION);
    printf("This is a program for tidying up unruly HTML documents. Tidy\n");
    printf("puts in all the bits people tend to leave out such as the DOCTYPE\n");
    printf("declaration and the HEAD and BODY elements. Tidy works hard to\n");
    printf("spot missing start and end tags, for example missing list tags\n");
    printf("like <UL> or <DL>. Unknown tags are stripped out. Tidy lays out\n");
    printf("the document to fit within an 80 column display for convenience\n");
    printf("in further editing. If you have any suggestions for improving\n");
    printf("\"tidy\" please email your comments to me at dsr@hplb.hpl.com\n\n");
}

main(int argc, char **argv)
{
    overwrite = 0;

    if (argc == 1)
    {
        fprintf(stderr, "\nTidies HTML files to match the DTD and current practice.\n");
        fprintf(stderr, "You are recommended to back up your files before tidying them.\n");
        fprintf(stderr, "Version %s,  (c) Dave Raggett April 1994\n", VERSION);
        fprintf(stderr, "\nUseage:\n\t%s [-help] [-w] file file file ...\n\n", argv[0]);
        fprintf(stderr, "where -help gives futher information on what tidy does, and -w\n");
        fprintf(stderr, "overwrites existing files rather than sending corrections to stdout\n\n");
    }

    debug = 0;

    while (argc > 1)
    {
        if (strcasecmp(argv[1], "-help") == 0)
        {
            Help();
            break;
        }
        else if (strcasecmp(argv[1], "-w") == 0)
            overwrite = 1;
        else if (argv[1][0] == '-')
            fprintf(stderr, "Unknown flag: %s\n", argv[1]);
        else
            Clean(argv[1]);

        --argc;
        ++argv;
    }
}
