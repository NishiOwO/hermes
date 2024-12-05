/*
 Filter to convert html to latex for printing on A or A4 pages

 (c) 1993 Dave Raggett, Hewlett Packard Laboratories
     All rights reserved ...

 You invoke the filter as follows:

        html2latex [-udi UDI] file > file.tex

 The UDI (e.g. 'http://host/path') is printed below the title at the
 start of the document. This is useful when you want to have a fresh
 look at the original file online.

 In practice we use this inconjunction with the print button on our browser,
 which saves the current document as a temporary file and then invokes the
 filter via a simple script which then runs latex, prints the dvi file and
 deletes the temporary files produced as a result:

    #! /bin/sh
    # Dave Raggett - script for printing html document
    # use as:  prhtml -udi 'http://info.cern.ch/fred.html' -dfir foo.html
    # note: all 4 arguments are mandatory

    html2latex $1 $2 $4 > $4.tex
    latex $4.tex
    latex $4.tex
    lp $3 -odvi $4.dvi
    rm $4 $4.tex $4.aux $4.dvi $4.log

 Note that the filter ignores unrecognised tags, and doesn't yet cope with
 some of the more recent emphasis tags, or the complete set of entity
 references for Latin-1. The references for Hypertext links are noted
 as footnotes.

 The filter requires ANSI C and compiles on HP kit with:

    cc -Aa -g -D_HPUX_SOURCE -o html2latex html2latex.c -lm
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#define TABSIZE         8
#define MAXVAL  1024

#define END     0
#define TEXT    1
#define TAG     2
#define ENDTAG  3

/* the html tag codes */

#define UNKNOWN         1
#define WORD            2
#define BEGINTITLE      3
#define ENDTITLE        4
#define BEGINHEADER     5
#define ENDHEADER       6
#define BEGINANCHOR     7
#define ENDANCHOR       8
#define PARAGRAPH       9
#define NEXTID          10
#define BEGINDL         11
#define ENDDL           12
#define DTTAG           13
#define DDTAG           14
#define BEGINXMP        15
#define ENDXMP          16
#define BEGINADDRESS    17
#define ENDADDRESS      18
#define BEGINUL         19
#define ENDUL           20
#define BEGINOL         21
#define ENDOL           22
#define LITAG           23
#define NEWLINE         24
#define PLAINTEXT       25
#define LISTING         26
#define BEGINPRE        27
#define ENDPRE          28
#define HEADTAG         29
#define BODYTAG         30
#define NEWPAGE         31   /* only recognised for printing */

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

#define HTTP_PORT       80
#define OLD_PORT        2784
#define GOPHERPORT      70

#define SCHSIZ 12
#define HOSTSIZ 48
#define PATHSIZ 512
#define ANCHORSIZ 64



int debug = 0;
long NewLength;
int NewType;
char *udi;

char *MyHostName(void)
{
    static char myhost[HOSTSIZ];

    if (gethostname(myhost, HOSTSIZ) != 0)
    {
        perror("can't get gethostname");
        return "";
    }

    return (char *)myhost;
}

/* return pointer to '/' terminating parent dir or just after 1st */

char *ParentDirCh(char *dir)
{
    char *p;

    p = dir + strlen(dir) - 2;  /* dir has optional trailing '/' */

    while (p > dir && *p != '/')
        --p;

    return (p > dir ? p : dir+1);
}

/* read specified file, and return pointer to string buffer */

#define BUFSIZE     4096
#define THRESHOLD   512

char *GetFile(char *name)
{
    int fd, c, AddSlash, len;
    unsigned int size;
    FILE *fp;
    char *buf, *p, *q, *r, *s, *me, lbuf[1024];

    udi = name;

    if (chdir(name) == 0)  /* its a directory */
    {
        size = 2048;
        buf = malloc(size);


        if (buf == 0)
        {
            fprintf(stderr, "can't allocate buffer size %d\n", size);
            return 0;
        }

        me = MyHostName();

        sprintf(buf, "<TITLE>%s at %s</TITLE>\n<H1>%s at %s</H1>\n<XMP>\n",
                name, me, name, me);

        len = strlen(buf);

        /* check is a trailing slash is needed */

        AddSlash = (name[strlen(name)-1] != '/' ? 1 : 0);

        /* invoke ls command to list directory to FILE *fp */

        fp = popen("ls -al", "r");

        q = lbuf;

        while ( (c = getc(fp)) != EOF )
        {
            *q++ = c;

            if (c == '\n')
            {
                *--q = '\0';

                /* Unix dir list has total line */

                p = strrchr(lbuf, ' ');
            
                if (p && strncasecmp(lbuf, "total", 5) != 0)
                {
                    *p++ = '\0';

                    if (strcmp(p, ".") == 0)
                        goto next_line;
                    if (strcmp(p, "..") == 0 && (r = ParentDirCh(name)))
                    {
                        if (strcmp(name, "/") == 0)
                            goto next_line;

                        c = *r;
                        *r = '\0';
                        sprintf(buf+len, "%s <A HREF=\"http://%s%s\">", lbuf, me, name);
                        *r = c;
                    }
                    else if (*lbuf == 'l')
                    {
                        *(p - 4) = '\0';
                        r = strrchr(lbuf, ' ');
                        *r++ = '\0';

                        if (*p == '/')  /* absolute link */
                             sprintf(buf+len, "%s <A HREF=\"http://%s%s\">", lbuf, me, p);
                        else if (AddSlash)
                             sprintf(buf+len, "%s <A HREF=\"http://%s%s/%s\">", lbuf, me, name, p);   
                        else
                             sprintf(buf+len, "%s <A HREF=\"http://%s%s%s\">", lbuf, me, name, p);
                    }
                    else if (AddSlash)
                        sprintf(buf+len, "%s <A HREF=\"http://%s%s/%s\">", lbuf, me, name, p);
                    else
                        sprintf(buf+len, "%s <A HREF=\"http://%s%s%s\">", lbuf, me, name, p);

                    len += strlen(buf+len);

                    if (lbuf[0] == 'd')
                        sprintf(buf+len, "%s/</A>\n", p);
                    else if (lbuf[0] == 'l')
                        sprintf(buf+len, "%s@</A>\n", r);
                    else if (lbuf[3] == 'x')
                        sprintf(buf+len, "%s*</A>\n", p);
                    else
                        sprintf(buf+len, "%s</A>\n", p);

                    len += strlen(buf+len);
                }
                else
                {
                     memcpy(buf+len, lbuf, q-lbuf);
                     len += q-lbuf;
                     buf[len++] = '\n';
                }

             next_line:

                q = lbuf;

                if (len >= size - THRESHOLD)
                {
                    size *= 2;  /* attempt to double size */

                    p = realloc(buf, size);

                    if (p == NULL)
                    {
                        fprintf(stderr, "can't realloc buffer to size %ld\n", size);
                        buf[len] = 0;
                        return buf;
                    }

                    buf = p;
                }
            }
        }

        pclose(fp);
        buf[len++] = '\0';
        NewLength = len;
        return buf;
    }
    else if (errno != ENOTDIR)
    {
        fprintf(stderr, "%s: errno %d\n", name, errno);
        return NULL;
    }
    else if ((fd = open(name, O_RDONLY)) == -1)
    {
        if (debug)
            fprintf(stderr, "Can't open file: \"%s\" errno %d\n", name, errno);

        return NULL;
    }

    size = lseek(fd, 0L, SEEK_END);

    buf = malloc(1 + size);
    lseek(fd, 0L, SEEK_SET);

    /* the next line assumes that read won't return -1 look at man page! */
    buf[read(fd, (void *)buf, size)] = '\0';

    close(fd);
    NewLength = size;
    return buf;
}

/*
   SGML state flags - these are used by the parser to note when
   the current point is within <tag> ..........</tag> and to track
   the effects of some other special tags such as <PLAINTEXT>

   The flags are operated on by inc/dec actions. This allows the
   parser to deal with nested tags when appropriate.
*/

static int TitleFlag, HeaderFlag, AnchorFlag, GlossaryFlag, XmpFlag,
   PreFlag, AddressFlag, UListFlag, OListFlag, LeftMargin, RightMargin;

static int InList;

/* The following variable is for convenience in passing the current
   buffer position to GetToken() and GetWord(). It should be set
   appropriately before calling GetToken(). It might be better to
   pass it as a procedural argument instead ...  */

static char *pStr;     /* points to current position in text buffer */

static char *anchor;     /* points to last seen anchor string */

/* a buffer for storing visible hypertext links */

int hButtonPressed = 0;
int nlinks;            /* the number of visible links */
HyperLink hyperlinks[MAXLINKS];

#define IsWhiteSpace(c)  (c == ' ' || c == '\n' || c == '\t' || c == '\r')
#define IsPunctuation(c) (c == '.' || c == ',' || c == ';' || c == '?' || c == '!')

/* returns pointer to extracted HREF field from Anchor string
   needs to expand entity references and strip surrounding "
   may need extension to match HREF independent of case       */

char *HRef(char *s)
{
    int i, c;
    char *p;
    static char buf[256];

    while (strncasecmp(s, "href", 4) != NULL)
    {
        if (*s == '\0')
            return "";

        ++s;
    }

    p = s+5; /* past "HREF=" */

    while (IsWhiteSpace(*p))
        ++p;

    if (*p == '"')
        ++p;

    s = buf;   /* reuse s as local var */
    i = 255;

    while ((c = *s = *p++))
    {
        if (IsWhiteSpace(c) || c == '"' || i == 0)
        {
            *s = '\0';
            break;
        }

        ++s;
    }

    return buf;
}

/* swallow chars upto and including '>' char which ends each sgml tag */

void SwallowAttributes()
{
    while (*pStr && *pStr++ != '>');
}

/* coerce string to upper case */
void MakeUpper(char *s)
{
    for (; *s; ++s)
        *s = toupper(*s);
}

/* Entity References

   &lt;     <
   &gt;     >
   &amp;    &
   &quot;   "
   &apos;   '

These are recognised in attribute data and normal text

Note character references are different:

    &#45;
    &#Atilde;

These give character codes or names. A large set is defined in ISO 8879 D.4
*/

/*
  Returns replacement char if *s starts with  a known HTML entity reference

    o   slen returns the number of chars in s that are replaced
*/

int EntityReference(char *s, int *slen)
{
    char buf[64];
    int i, c;

    if (*s++ != '&')
        return 0;

    for (i = 0; i < 63; ++i)
    {
        if ((c = *s++) == ';')
            break;

        if (c == '\0')
            break;

        buf[i] = c;
    }

    buf[i] = '\0';
    *slen = i+1;

    /* don't recognize named character references for now! */

    if (buf[0] == '#')
    {
        sscanf(buf+1, "%d", &c);
        return c;
    }

    MakeUpper(buf);
    if (strcmp(buf, "LT") == 0)
        return '<';

    if (strcmp(buf, "GT") == 0)
        return '>';

    if (strcmp(buf, "AMP") == 0)
        return '&';

    if (strcmp(buf, "QUOT") == 0)
        return '"';

    if (strcmp(buf, "APOS") == 0)
        return '\'';

    return 0;
}

/*
   Get a word from pStr, terminated by white space or '>',
   and leave pStr pointing to terminating char.

   It is assumed that the buffer is large enough for
   max chars PLUS a terminating '\0'

   Sets *len to the number of chars read into the
   buffer, excluding the terminating '\0'.

   Returns TEXT, TAG, ENDTAG
*/
int GetWord(char *p, int *len, int max)
{
    int c, i, type, rlen;
    char *s;

    /* skip leading white space */

    while ((c = *pStr) && IsWhiteSpace(c))
        ++pStr;

    if (c == '\f')
    {
        len = 0;
        return NEWPAGE;
    }

    if (c == '\0')
    {
        len = 0;
        return END;
    }

    if (c == '<')
    {
        c = *++pStr;

        if (c  == '/')
        {
            type = ENDTAG;
            c = *++pStr;
        }
        else
            type = TAG;
    }
    else
        type = TEXT;

    switch(type)
    {
        case TAG:
            for (i = 0 ; c && i < max; )
            {
                if (c == '>')
                    break;

                *p++ = c;
                ++i;
                c = *++pStr;

                if (IsWhiteSpace(c))
                    break;
            }
            break;

        case ENDTAG:
            for (i = 0 ; c && i < max; )
            {
                if (c == '>')
                    break;

                *p++ = c;
                ++i;
                c = *++pStr;

                if (IsWhiteSpace(c))
                    break;
            }
            break;

        case TEXT:
            for (i = 0 ; c && i < max; )
            {
                if (c == '<')
                    break;

                if (c == '&')
                {
                    if (c = EntityReference(pStr, &rlen))
                        pStr += rlen;
                    else
                        c = '&';
                }

                /* escape latex chars */

                if (c == '%' || c == '$' || c == '{' || c == '}' ||
                        c == '#' || c == '_' || c == '&')
                {
                    *p++ = '\\';
                    ++i;
                }

                if (c == '[' && InList)
                {
                    *p++ = '{';
                    ++i;
                }
                else if (c == ']' && InList)
                {
                    *p++ = c;
                    ++i;
                    c = '}';
                }

                if (c == '<')
                {
                    s = "$<$";
                    strcpy(p, s);
                    p += strlen(s);
                    i += strlen(s);
                }
                else if (c == '>')
                {
                    s = "$>$";
                    strcpy(p, s);
                    p += strlen(s);
                    i += strlen(s);
                }
                else if (c == '\\')
                {
                    s = "$\\backslash$";
                    strcpy(p, s);
                    p += strlen(s);
                    i += strlen(s);
                }
                else
                {
                    *p++ = c;
                    ++i;
                }

                c = *++pStr;

                if (IsWhiteSpace(c))
                    break;
            }
            break;
    }

    *p = '\0';
    *len = i;

    if (i > max)
    {
        fprintf(stderr, "Overflowed token buffer!\n");
        i = max;
    }

    return type;
}

/* recognize pStr == "/XMP...." */

int IsXMPTag(char *s)
{
    int closing; 

    closing = 0;

    if (*s == '/')
    {
        ++s;
        closing = 1;
    }

    if (toupper(*s++) != 'X')
        return 0;

    if (toupper(*s++) != 'M')
        return 0;

    if (toupper(*s) != 'P')
        return 0;

    if (closing)
        return ENDXMP;

    return BEGINXMP;
}

int IsPRETag(char *s)
{
    int closing; 

    closing = 0;

    if (*s == '/')
    {
        ++s;
        closing = 1;
    }

    if (toupper(*s++) != 'P')
        return 0;

    if (toupper(*s++) != 'R')
        return 0;

    if (toupper(*s) != 'E')
        return 0;

    if (closing)
        return ENDPRE;

    return BEGINPRE;
}

/* recognize pStr == "/A...." */

int IsAnchorTag(char *s)
{
    int closing; 

    closing = 0;

    if (*s == '/')
    {
        ++s;
        closing = 1;
    }

    if (toupper(*s++) != 'A')
        return 0;

    if (!IsWhiteSpace(*s) && *s != '>')
        return 0;

    if (closing)
        return ENDANCHOR;

    return BEGINANCHOR;
}

/* recognize pStr == "P...." */

int IsParaTag(char *s)
{
    int closing; 

    closing = 0;

    if (toupper(*s++) != 'P')
        return 0;

    if (*s != '>' && *s != ' ')
        return 0;

    return PARAGRAPH;
}

/* this routine copies the tag for GetLiteral to support error messages */
void CopyTag(char *s, char *p)
{
    int c;

    while ((c = *p++) != '>' && c != ' ')
        *s++ = c;

    *s = '\0';
}

int GetLiteral(char *s, int *n, int max)
{
    int c, i, rlen;
    char *p;

    if (*pStr == '\n')
    {
        ++pStr;
        *s = '\0';
        return NEWLINE;
    }

    if (*pStr == '\f')
    {
        ++pStr;
        *s = '\0';
        return NEWPAGE;
    }

    if ( *pStr == '<')
    {
        if ( (c = IsXMPTag(pStr+1)) || (c = IsPRETag(pStr+1)) || (c = IsParaTag(pStr+1)) )
        {
            CopyTag(s, pStr);
            *n = 0;
            SwallowAttributes();
            return c;
        }
        else if (c = IsAnchorTag(pStr+1))
        {
            /* note pointer to anchor's attributes */

            if (c == BEGINANCHOR)
                anchor = pStr;

            CopyTag(s, pStr);
            SwallowAttributes(); /* move pStr past attributes */
            return c;
        }
    }

    if (*pStr == '\0')
    {
        *s = '\0';
        *n = i;
        return END;
    }

    for (i = 0; i < max;)
    {
        c = *pStr;

        ++pStr;

        if (c == '_' && *pStr == '\b')
        {
            ++pStr;
            c = *pStr++;
        }

        if (c == '\r')
            continue;

        if (c == '\n' || c == '\0')
        {
            --pStr;
            break;
        }

        if (c == '\t')
        {
            do
                *s++ = ' ';
            while (++i % TABSIZE);

            continue;
        }

        if (c == '<' && (IsXMPTag(pStr) || IsPRETag(pStr) || IsParaTag(pStr) || IsAnchorTag(pStr)) )
        {
            --pStr;
            break;
        }

        if (c == '&' && (c = EntityReference(pStr-1, &rlen)) )
        {
             pStr += rlen;
        }

        *s++ = c;
        ++i;
    }

    *s = '\0';
    *n = i;

    return WORD;
}

/*

GetToken examines the document buffer using the global pointer pStr
to parse sgml tags or whitespace deliminated words. pStr is incremented
to point to the char following the terminator (or to the null char after
the end of the buffer).

The routine is passed the address of a string pointer giving the place to
store the token's displayable value, i.e. the text string for words or the
title text for a title tag.

A more efficient approach to matching sgml tags is needed, perhaps based
upon hash indexing. The tags are all known in advance, so it ought to be
possible to avoid clashes while keeping to a small table size.

Note that tag names seem to be case-insensitive. See Goldfarb's book
for the full horror that is SGML ...

*/

int GetToken(char *s, int *n, int max)
{
    int type, endtag;

    if (XmpFlag || PreFlag)
        return GetLiteral(s, n, max);

    type = GetWord(s, n, max);

    if (type == END)
        return END;

    endtag = (type == ENDTAG ? 1 : 0);

    /* check for known sgml tags, unrecognised ones are treated as text */

    if (type == TAG || type == ENDTAG)
    {
        if (*s == '!' || *s == '?')
        {
            SwallowAttributes(); /* ignore attributes for now! */
            return UNKNOWN;
        }

        MakeUpper(s);   /* coerce tag to upper case */

        if (strcmp(s, "P") == 0)
        {
            SwallowAttributes(); /* ignore attributes for now! */
            return PARAGRAPH;
        }

        /* <A NAME=XXX HREF=YYY>anchor text</A>

           Note that there are three optional tags which can
                occur in any order:

                NAME -- allows anchor to be destination of a link
                HREF -- hypertext link given by W3 address syntax
                TYPE -- modifies the meaning of hypertext links
         */

        if (strcmp(s, "A") == 0)
        {
            /* note pointer to anchor's attributes */

            if (!endtag)
                anchor = pStr;

            SwallowAttributes(); /* move pStr past attributes */
            return (endtag ? ENDANCHOR : BEGINANCHOR);
        }

        /* <TITLE>title text</TITLE> */

        if (strcmp(s, "TITLE") == 0)
        {
            SwallowAttributes();
            return (endtag ? ENDTITLE : BEGINTITLE);
        }

        /* <H1>header text</H1> etc. for H1, H2, ... H6 */

        if (s[0] == 'H' && '1' <= s[1] && s[1] <= '6')
        {
            SwallowAttributes();
            return (endtag ? ENDHEADER : BEGINHEADER);
        }

        /* <NEXTID 27> next document wide numeric identifier */

        if (strcmp(s, "NEXTID") == 0)
        {
            SwallowAttributes();
            return NEXTID;
        }

        /* <DL> .... </DL>  glossaries */

        /* kludge for CERN phone book - map <DLC> to <DL> */
        if (strcmp(s, "DL") == 0 || strcmp(s, "DLC") == 0)
        {
            SwallowAttributes();
            return (endtag ? ENDDL : BEGINDL);
        }

        /* <DT> short title <DD> paragraph */

        if (strcmp(s, "DT") == 0)
        {
            SwallowAttributes();
            return DTTAG;
        }

        if (strcmp(s, "DD") == 0)
        {
            SwallowAttributes();
            return DDTAG;
        }

        /* unnumbered list <UL> <LI>... <LI> ...</UL> */

        if (strcmp(s, "UL") == 0)
        {
            SwallowAttributes();
            return (endtag ? ENDUL : BEGINUL);
        }

        /* treat DIR, MENU for now as being the same as UL */

        if (strcmp(s, "DIR") == 0 || strcmp(s, "MENU") == 0)
        {
            SwallowAttributes();
            return (endtag ? ENDUL : BEGINUL);
        }

        if (strcmp(s, "OL") == 0)
        {
            SwallowAttributes();
            return (endtag ? ENDOL : BEGINOL);
        }

        if (strcmp(s, "XMP") == 0)
        {
            SwallowAttributes();
            return (endtag ? ENDXMP : BEGINXMP);
        }

        if (strcmp(s, "PRE") == 0)
        {
            SwallowAttributes();
            return (endtag ? ENDPRE : BEGINPRE);
        }

        /* treat LISTING as if it were XMP */

        if (strcmp(s, "LISTING") == 0)
        {
            SwallowAttributes();
            return (endtag ? ENDXMP : BEGINXMP);
        }

        /* treat <PLAINTEXT> as <XMP> */

        if (strcmp(s, "PLAINTEXT") == 0)
        {
            SwallowAttributes();
            return BEGINXMP;
        }

        if (strcmp(s, "ADDRESS") == 0)
        {
            SwallowAttributes();
            return (endtag ? ENDADDRESS : BEGINADDRESS);
        }

         if (strcmp(s, "LI") == 0)
        {
            SwallowAttributes();
            return LITAG;
        }

        /* fudge IsIndex */

        if (strcmp(s, "ISINDEX") == 0)
        {
            SwallowAttributes();
            return UNKNOWN;;
        }

        /* unknown tag - so copy until closing '>' */

        SwallowAttributes();

        if (debug)
            fprintf(stderr, "unknown sgml tag: %s\n", s);

        return UNKNOWN;
    }

    return WORD;
}

/* routines for incrementing/decementing state variables */

void IncTag(int *pTag, char *tagname)
{
    if (*pTag > 0 && debug)
        fprintf(stderr, "Nested tag: %s>\n", tagname);

    *pTag += 1;
}


void DecTag(int *pTag, char *tagname)
{
    *pTag -= 1;

    if (*pTag < 0 && debug)
    {
        fprintf(stderr, "Unexpected closing tag: %s>\n", tagname);
        *pTag = 0;
    }
}

void DoTitle(char *s, char *udi)
{
    char *p, c;

    p = s;

    while (*p && *p != '<')
        ++p;

    c = *p;
    *p = '\0';
    printf("\\title{%s}\n", s);
    *p = c;    

    if (udi)
        printf("\\author{%s}\n", udi);
    else
        printf("\\author{}\n");

    printf("\\date{}\n");

    printf("\\maketitle\n");
}

char *AnchorString(char *s)
{
    char *p, *q;
    int i, c;
    static char buf[256];

    while (strncasecmp(s, "href=", 5) != 0)
    {
        if (*s == '>')
            return NULL;

        ++s;
    }

    
    i = 0;
    s += 5;
    p = buf;

    while (i < 256 && *s && *s != '>' && *s != ' ')
    {
        c = *s++;

        /* escape latex chars */

        if (c == '%' || c == '$' || c == '{' || c == '}' ||
            c == '#' || c == '_' || c == '&')
        {
            *p++ = '\\';
            ++i;
        }

        if (c == '.')
        {
            *p++ = c;
            ++i;
            *p++ = '\\';
            ++i;
            c = ' ';
        }
        else if (c == '[' && InList)
        {
            *p++ = '{';
            ++i;
        }
        else if (c == ']' && InList)
        {
            *p++ = c;
             ++i;
            c = '}';
        }

        if (c == '<')
        {
            q = "$<$";
            strcpy(p, q);
            p += strlen(q);
            i += strlen(q);
        }
        else if (c == '>')
        {
            q = "$>$";
            strcpy(p, q);
            p += strlen(q);
            i += strlen(q);
        }
        else if (c == '\\')
        {
            q = "$\\backslash$";
            strcpy(p, q);
            p += strlen(q);
            i += strlen(q);
        }
        else
        {
            *p++ = c;
            ++i;
        }
    }

    *p = '\0';
    return buf;
}


int IsAlphaNumeric(char c)
{
    if ('A' <= c && c <= 'Z')
        return 1;

    if ('a' <= c && c <= 'z')
        return 1;

    if ('0' <= c && c <= '9')
        return 1;

    return 0;
}

void ConvertHTML2Latex(char *buffer, char *udi)
{
    int n, token, last_token, len;
    char *s, *p, *title, buf[MAXVAL+1];

    /* note current values for sgml state variables */

    title = NULL;
    pStr = buffer;
    s = buf;
    len = 0;
    token = 0;

    printf("\\documentstyle{article}\n");

  /* Stefek's Latex magic to decrease default margin sizes to something reasonable */

    printf("\\newlength{\\xxxxThingThing} \\setlength{\\xxxxThingThing}{18 true cm}\n");
    printf("\\textwidth=\\xxxxThingThing\n");
    printf("\\setlength{\\oddsidemargin}{15 true mm}\n");
    printf("\\addtolength{\\oddsidemargin}{-1 true in}\n");  /* the Lamport adjustment */
    printf("\\setlength{\\evensidemargin}{15 true mm}\n");
    printf("\\addtolength{\\evensidemargin}{-1 true in}\n"); /* the Lamport adjustment */
    printf("\\pagestyle{plain}\n");
    printf("\\addtolength{\\topmargin}{-35 true mm}\n");

  /* Redefine text height, suitable for filling US paper (and so leaving
        an extra 3/4" gap at the bottom of A4) */

    printf("\\setlength{\\textheight}{11 true in}\n");       /* full length, now decrement... */
    printf("\\addtolength{\\textheight}{-1\\topmargin}\n");  /* Make room for the header */
    printf("\\addtolength{\\textheight}{-1 true in}\n");     /* Lamport's top-margin excess... */
    printf("\\addtolength{\\textheight}{-1\\headsep}\n");    /* (and its separator) */
    printf("\\addtolength{\\textheight}{-1\\headheight}\n"); /* Make room for the header */
    printf("\\addtolength{\\textheight}{-1\\footheight}\n"); /* Make room for the footer */
    printf("\\addtolength{\\textheight}{-1\\footskip}\n");   /*  (and its separator) */

    /* and now pull the footer up a bit: */

    printf("\\addtolength{\\textheight}{-20 true mm}\n");

    printf("\\begin{document}\n");

    /* initialise status flags */

    nlinks = TitleFlag = HeaderFlag = AnchorFlag = GlossaryFlag = 0;
    InList = PreFlag = XmpFlag = AddressFlag = UListFlag = OListFlag = 0;

    for (;;)
    {
        last_token = token;
        token = GetToken(s, &n, MAXVAL-len);

        switch(token)
        {
            case BEGINTITLE:
                title = pStr;
                IncTag(&TitleFlag, s);
                continue;

            case ENDTITLE:
                if (title)
                    DoTitle(title, udi);

                DecTag(&TitleFlag, s);
                continue;

            case BEGINHEADER:
                switch(s[1])
                {
                    case '1':
                        printf("\n\n\\section*{");
                        len = 10;
                        break;

                    case '2':
                        printf("\n\n\\subsection*{");
                        len = 13;
                        break;

                    default:
                        printf("\n\n\\subsubsection*{");
                        len = 16;
                        break;
                }

                IncTag(&HeaderFlag, s);
                continue;

            case ENDHEADER:
                printf("}\n");
                len = 0;
                DecTag(&HeaderFlag, s);
                continue;


            case BEGINANCHOR:
                IncTag(&AnchorFlag, s);
                continue;

            case ENDANCHOR:
                DecTag(&AnchorFlag, s);
                p = AnchorString(anchor);

                if (!p)
                    continue;

                /* ignore footnotes in verbatim mode since
                   they would throw a newline or two */

                if (XmpFlag || PreFlag)
                    continue;

                printf("\\footnote{ %s}", p);
                len = 11 + strlen(p);

                if (IsAlphaNumeric(*pStr))
                {
                    printf(" ");
                    ++len;
                }
                continue;

            case BEGINDL:
                printf("\n\\begin{description}\n");
                len = 0;
                IncTag(&GlossaryFlag, s);
                break;

            case ENDDL:
                printf("\n\\end{description}\n");
                len = 0;
                DecTag(&GlossaryFlag, s);
                break;

            case DTTAG:
                InList = 1;
                printf("\n\\item[");
                len = 6;
                break;

            case DDTAG:
                printf("]\n");
                InList = 0;
                break;

            case BEGINXMP:
                printf("\n\\begin{verbatim}");
                IncTag(&XmpFlag, s);
                break;

            case ENDXMP:
                printf(" \\end{verbatim}\n");
                len = 0;
                DecTag(&XmpFlag, s);
                break;

            case BEGINPRE:
                printf("\n\\begin{verbatim}");
                IncTag(&PreFlag, s);
                break;

            case ENDPRE:
                printf(" \\end{verbatim}\n");
                len = 0;
                DecTag(&PreFlag, s);
                break;

            case BEGINADDRESS:
                IncTag(&AddressFlag, s);
                break;

            case ENDADDRESS:
                DecTag(&AddressFlag, s);
                break;

            case BEGINUL:
                printf("\n\\begin{itemize}");
                len = 15;
                IncTag(&UListFlag, s);
                break;

            case BEGINOL:
                printf("\n\\begin{enumerate}");
                len = 17;
                IncTag(&OListFlag, s);
                break;

            case LITAG:
                printf("\n\\item ");
                len = 6;
                continue;

            case ENDUL:
                printf("\n\\end{itemize}\n");
                len = 0;
                DecTag(&UListFlag, s);
                break;

            case ENDOL:
                printf("\n\\end{enumerate}\n");
                len = 0;
                DecTag(&OListFlag, s);
                break;

            case PARAGRAPH:
                printf("\n\\par\n");
                len = 0;
                break;

            case UNKNOWN:
                break;

            case WORD:
                if (TitleFlag)
                     continue;

                if (XmpFlag || PreFlag)
                {
                     if (last_token == WORD || last_token == BEGINANCHOR)
                        printf(" %s", s);
                     else
                        printf("%s", s);

                     continue;
                }


                /* is wordwrap needed */

                if (len + n > 72)
                {
                    printf("\n");
                    len = 0;
                }

                if (len > 0 && last_token == WORD || last_token == BEGINANCHOR)
                {
                    printf(" %s", s);
                    len += n + 1;
                }
                else
                {
                    printf("%s", s);
                    len += n;
                }

                continue;

            case NEWLINE:
                len = 0;
                printf("\n");
                continue;

            case NEWPAGE:
                len = 0;

                if (XmpFlag || PreFlag)
                {
                    printf("\\end{verbatim}\n\\newpage\n\\begin{verbatim}");
                    continue;
                }

                printf("\\newpage\n");
                continue;

            case END:
                break;

            default:
                continue;
        }

        if (token == END)
            break;
    }

    if (XmpFlag || PreFlag)
        printf(" \\end{verbatim}\n");

    printf("\n\\end{document}\n");
}

main(int argc, char **argv)
{
    char *buffer, *udi;

    udi = NULL;

    if (argc > 1 && strcasecmp(argv[1], "-debug") == 0)
    {
        debug = 1;
        --argc;
        ++argv;
    }

    if (argc > 1 && strcasecmp(argv[1], "-udi") == 0)
    {
        udi = argv[2];
        --argc;
        --argc;
        ++argv;
        ++argv;
    }

    if (argc > 1)
    {
        buffer = GetFile(argv[1]);
        ConvertHTML2Latex(buffer, udi);
    }
}


