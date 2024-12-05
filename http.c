/* simplified http client - I intend to migrate to WWW Library in due course!

   Note supports HTRQ/V1.0 and is backward compatible with earlier servers.
   Well known file extensions are recognised explicitly.

   Otherwise HTML and Postscript documents are recognised by the presence
   of key strings at/near the start of their data contents.

   For older servers which send a <PLAINTEXT>/r/n tag at the start of non-html
   files, this is treated as part of the header and not-passed to the display code.
*/

#include <X11/Xlib.h>
#include "www.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

extern int debug;
extern int UseHTTP2;
extern int AbortFlag;
extern int Authorize;
extern char *gateway;
extern char *user;

#define TEXTDOCUMENT    0
#define HTMLDOCUMENT    1

#define BUFSIZE     8192
#define THRESHOLD   1024

char *scheme[9] =
{
    "file",
    "http",
    "ftp",
    "news",
    "gopher",
    "telnet",
    "rlogin",
    "wais",
    "finger"
};


Doc NewDoc, CurrentDoc;
char *url;

/* return name of this host */

char *MyHostName(void)
{
    static char myhost[HOSTSIZ];

    if (*myhost)
        return myhost;

    if (gethostname(myhost, HOSTSIZ) != 0)
    {
        perror("can't get gethostname");
        return "";
    }

    return (char *)myhost;
}

/* check if the host is me */
int IsMyName(char *host)
{
    char *p, *me;

    me = MyHostName();

    /* full match on name, e.g. dragget.hpl.hp.com */

    if (strcasecmp(host, me) == 0)
        return 1;

    p = strchr(host, '.');

    /* match abbreviated host name: dragget with dragget.hpl.hp.com */

    if (!p && strncasecmp(host, me, strlen(host)) == 0)
        return 1;

    return 0;
}

/* safe version of malloc() */

char *safemalloc(int n)
{
    char *p;

    p = malloc(n);

    if (!p)
    {
        Beep();
        fprintf(stderr, "ERROR: malloc failed, memory exhausted!\n");
        exit(1);
    }

    return p;
}


/* set current host, scheme etc to self  */

void InitCurrent(char *path)
{
    char *p;

    p = safemalloc(HOSTSIZ+1);

    if (gethostname(p, HOSTSIZ) != 0)
    {
        perror("gethostname");
        exit(1);
    }

    CurrentDoc.buffer = NULL;
    CurrentDoc.hdrlen = 0;
    CurrentDoc.length = 0;
    CurrentDoc.host = p;
    CurrentDoc.port = OLD_PORT;
    CurrentDoc.protocol = MYFILE;
    CurrentDoc.path = strdup(path);
    CurrentDoc.anchor = 0;
}


/* if host name contains "." check that it ends with ".hp.com" */

int not_hp_domain(char *name)
{
    char *p;

    p = strrchr(name, '.');

    if (p)
    {
        /* check for HP host address as 15.127.78.3 */

        if (strncmp(name, "15.", 3) == 0)
            return 0;

        /* otherwise check for host name ending with ".hp.com" */

        p -= 3;

        if (p >= name)
            return (strcasecmp(p, ".hp.com") == 0 ? 0 : 1);

        return 1;
    }

    return 0; /* no domain fields - probably a local hp node */
}

char *ProtcolName(int protocol)
{
    if (0 <= protocol && protocol < NPROTOCOLS)
        return scheme[protocol];

    return "unknown";
}

char *UnivRefLoc(Doc *doc)
{
    int n;
    char buf[512];

    if (doc->protocol == NEWS)
        sprintf(buf, "news:%s%n", doc->path, &n);
    else if (doc->protocol == FTP)
        sprintf(buf, "ftp://%s%s%n", doc->host, doc->path, &n);
    else if (doc->protocol == MYFILE)
    {
        sprintf(buf, "file:%s", doc->path);
        n = strlen(buf);
        
    }
    else if (doc->host)
        sprintf(buf, "%s://%s:%d%s%n", scheme[doc->protocol], doc->host, doc->port, doc->path, &n);

    if (doc->anchor && *doc->anchor != '\0')
        sprintf(buf+n, "#%s", doc->anchor);

    if (doc->url)
    {
        free(doc->url);
        doc->url = 0;
    }

    return strdup(buf);
}

/* how many chars to copy from current (absolute) path
   to get the required directory name

    n = 1       ./
    n = 2       ../
    n = 3       ../../

    etc.

   The number returned DOES include the final '/'
*/

int ParentDirCnt(char *path, int n)
{
    char *p;

    p = path + strlen(path);

    while (n > 0)
    {
        if (p == path)
            break;

        if (*--p == '/')
            --n;
    }

    return (1 + p - path);
}

/* parse document reference, setting up global params
   and returning pointer to expanded reference */

char *ParseReference(char *s, int where)
{
    char *p, *r, *anchor;
    int c, i, m, n, k, has_domain;

    FreeDoc(&NewDoc);  /* plug memory leaks */

    /* first look for a scheme: the text which occurs
       before a ":" and which must occur before any "/" char */

    if (where == LOCAL)
    {
        NewDoc.where = LOCAL;
        NewDoc.protocol = MYFILE;
    }
    else
    {
        NewDoc.where = CurrentDoc.where;
        NewDoc.protocol = CurrentDoc.protocol;
    }

    p = s;

    while (c = *p)
    {
        if (c == '/')
        {
            c = *s;
            goto parsehost;
        }

        if (c == ':')
        {
            NewDoc.where = REMOTE;
            n = p - s;
            *p = '\0';

            for (i = 0; i < NPROTOCOLS; ++i)
            {
                if (strcasecmp(s, scheme[i]) == 0)
                {
                    NewDoc.protocol = i;
                    break;
                }
            }

            *p = ':';

            if (i == TELNET)
            {
                Warn("telnet not yet supported: %s", s);
                return NULL;
            }

            if (i == NPROTOCOLS)
            {
                if (debug)
                    fprintf(stderr, "Unknown protocol in %s\n", s);

                Warn("Unknown protocol in %s", s);
                return NULL;
            }

            s = p+1;  /* s points to just after the ':' */

            /* check for local file reference "file:/tmp/fred.html"
                or misuse of file: in place of ftp: */

            if (NewDoc.protocol == MYFILE)
            {
                /* local file ref ? */

                if ( !(p[1] == '/' && p[2] == '/') )
                {
                    NewDoc.port = 0;
                    NewDoc.host = strdup(MyHostName());
                    NewDoc.where = LOCAL;   /* could cause problems */
                    goto parse_path;
                }

                NewDoc.protocol = FTP;
            }

            /* check for news: scheme */

            if (NewDoc.protocol == NEWS && strncmp(s, "//", 2) != 0)
            {
                n = strlen(s);
                NewDoc.path = malloc(n+1);
                strcpy(NewDoc.path, s);
                NewDoc.port = 0;
                NewDoc.host = strdup(NEWS_SERVER);
                NewDoc.anchor = NULL;
                NewDoc.url = UnivRefLoc(&NewDoc);

                return NewDoc.url;
            }

            c = *s;
            goto parsehost;
        }

        ++p;
    }

    NewDoc.protocol = CurrentDoc.protocol;

  parsehost:

    /* the chars following "//" and preceding a ":" or a "/" */

    if (c == '/' && s[1] == '/')
    {
        p = s + 2;
        has_domain = 0;

        for (i = 0; (c = *p) && c != '/' && c != ':'; ++i)
        {
            ++p;

            if (c == '.')
                has_domain = 1;
        }

        /* check if we need to add local domain */

        if (has_domain)
        {
            NewDoc.host = safemalloc(i+1);
            memcpy(NewDoc.host, s+2, i);
            NewDoc.host[i] = '\0';
        }
        else
        {
            r = strchr(MyHostName(), '.');

            if (r)
            {
                n = strlen(r);
                NewDoc.host = safemalloc(i+n+1);
                memcpy(NewDoc.host, s+2, i);
                strcpy(NewDoc.host+i, r);
            }
            else /* my host doesn't contain domain name */
            {
                fprintf(stderr, "ERROR - local hostname needs full internet domain\n");
                NewDoc.host = strndup(s+2, i);
            }
        }

        s = p;
    }
    else if (NewDoc.protocol == MYFILE || NewDoc.protocol == HTTP)
        NewDoc.host = strdup(CurrentDoc.host);

    /* at this stage c == *s and s points to ':' of port name or '/' of path */

    if (c == ':')  /* then parse port */
    {
        sscanf(s+1, "%d", &NewDoc.port);

        /* and skip to next '/' char */

        while ((c = *s), c != '/')
            ++s;
    }
    else if (NewDoc.protocol == GOPHER)
        NewDoc.port = GOPHERPORT;
    else if (strcasecmp(NewDoc.host, CurrentDoc.host) == 0)
        NewDoc.port = CurrentDoc.port;
    else
        NewDoc.port = HTTP_PORT;

  parse_path:

    anchor = strchr(s, '#');

    if (anchor)
    {
        k = anchor - s;
        NewDoc.anchor = strdup(anchor+1);
    }
    else
    {
        k = strlen(s);
        NewDoc.anchor = 0;
    }

    /* the path is absolute if scheme and host were present

       if missing, the path may start with "./" or perhaps
       "../" or even "../../../" etc.

       If the current path is absolute then use it to
       convert the new path to its absolute form */


    if ( s[0] == '/' || NewDoc.protocol != CurrentDoc.protocol ||
             (NewDoc.protocol != HTTP &&
              NewDoc.protocol != MYFILE && strcmp(NewDoc.path, CurrentDoc.path) != 0) )
    {   /* then absolute path */
        NewDoc.path = safemalloc(1+k);
        memcpy(NewDoc.path, s, k);
        NewDoc.path[k] = '\0';
    }
    else if (s[0] == '.' && s[1] == '/')  /* current directory */
    {
        if (CurrentDoc.path[0] == '/')  /* absolute path */
        {
            n = ParentDirCnt(CurrentDoc.path, 1);
            NewDoc.path = safemalloc(n + k - 2 + 1);
            memcpy(NewDoc.path, CurrentDoc.path, n);
            memcpy(NewDoc.path+n, s+2, k - 2);
            NewDoc.path[n + k - 2] = '\0';
        }
        else
        {
            NewDoc.path = safemalloc(1+k);
            memcpy(NewDoc.path, s, k);
            NewDoc.path[k] = '\0';
        }
    }
    else if (s[0] == '.' && s[1] == '.' && s[2] == '/') /* parent directory */
    {
        m = 1;
        p = s;

        do
        {
            ++m;
            p += 3;
        }
        while (p[0] == '.' && p[1] == '.' && p[2] == '/');

        if (CurrentDoc.path[0] == '/')  /* absolute path */
        {
            n = ParentDirCnt(CurrentDoc.path, m);
            NewDoc.path = safemalloc(n + k - (p - s) + 1);
            memcpy(NewDoc.path, CurrentDoc.path, n);
            memcpy(NewDoc.path+n, p, k - (p - s));
            NewDoc.path[n + k - (p - s)] = '\0';
        }
        else
        {
            NewDoc.path = safemalloc(1+k);
            memcpy(NewDoc.path, s, k);
            NewDoc.path[k] = '\0';
        }
    }
    else if (NewDoc.protocol == GOPHER)
        NewDoc.path = strdup("/");
    else /* must be in current directory */
    {
        if (CurrentDoc.path[0] == '/')  /* absolute path */
        {
            n = ParentDirCnt(CurrentDoc.path, 1);
            NewDoc.path = safemalloc(n + k + 1);
            memcpy(NewDoc.path, CurrentDoc.path, n);
            memcpy(NewDoc.path+n, s, k);
            NewDoc.path[n + k] = '\0';
        }
        else
        {
            NewDoc.path = safemalloc(1+k);
            memcpy(NewDoc.path, s, k);
            NewDoc.path[k] = '\0';
        }
    }

    NewDoc.url = UnivRefLoc(&NewDoc);

    return NewDoc.url;
}

#define ToHex(c)    (c > 9 ? c + 'A' - 10 : c + '0')

#define IsSearchSym(c)  (c == '$' || c == '_' || c == '@' || c == '!' || c == '%' \
     || c == '^' || c == '&' || c == '*' || c == '(' || c == ')' || c == '.')

/* SearchRef - uses current params plus keywords to build search reference */
char *SearchRef(char *keywords)
{
    int c, d;
    char *p;
    static char buf[256];

    sprintf(buf, "http://%s:%d%s?", CurrentDoc.host, CurrentDoc.port, CurrentDoc.path);

    c = strlen(buf);

    /* avoid double question mark for case when base document ends with "?" */

    if (buf[c-2] == '?')
        --c;

    p = buf + c;

    /* skip leading white space */

    while ((c = *keywords) && (c == ' '))
        ++keywords;

    while ((c = *keywords++))
    {
        if (p - buf > 254)
            break;

        /* comma or space char separates keywords */

        if (c == ',' || c == ' ')
        {
            while ((c = *keywords++) && (c == ' ' || c == ','));

            if (c == '\0')
                break;

            *p++ = '+';
        }

        if (isalpha(c) || isdigit(c) || IsSearchSym(c))
        {
            *p++ = c;
            continue;
        }

        d = c & 0xF;
        c = (c >> 4) & 0xF;

        *p++ = '%';
        *p++ = ToHex(c);
        *p++ = ToHex(d);
    }

    *p = '\0';

    return buf;
}

char *HTRQrequestString(Doc *doc, int *len, char *who)
{
    int c, n, dn;
    static char buf[512];

    strcpy(buf, "GET ");
    n = 4;
/*
    strcpy(buf, "HEAD ");
    n = 5;
*/
    sprintf(buf+n, "%s%n", doc->path, &dn);
    n += dn;

    /* Gopher servers don't understand HTRQ/V1.0 */

    if (!UseHTTP2 || doc->port == 70 || doc->protocol == GOPHER)
    {
        sprintf(buf+n, "\r\n%n", &dn);
        n += dn;
    }
    else
    {
        sprintf(buf+n, " HTTP/1.1\r\n%n", &dn);
        n += dn;

        if (who)
	{
           // sprintf(buf+n, "Authorization: user %s\r\n\r\n%n", who, &dn);
            //n += dn;
        }
        else if (user)
	{
            //sprintf(buf+n, "Authorization: user %s\r\n\r\n%n", user, &dn);
            //n += dn;
        }
        else
	{
            sprintf(buf+n, "\r\n\r\n%n", &dn);
            n += dn;
        }
    }

    *len = n;
    return buf;
}

/* search for <H1> or <TITLE> tags in first 1K of text */

int IsHTMLDoc(char *p, int len)
{
    int i;

    i = 1024;

    for (;;)
    {
        if (len-- <= 0)
            return 0;

        if (i-- <= 0)
            return 0;

        if (*p != '<')
        {
            ++p;
            continue;
        }

        if (strncasecmp(p, "</title>", 7) == 0)
            return 1;

        if (strncasecmp(p, "<h1>", 4) == 0)
            return 1;

        if (strncasecmp(p, "<li>", 4) == 0)
            return 1;

        if (strncasecmp(p, "<xmp>", 5) == 0)
            return 1;

        if (strncasecmp(p, "<pre>", 5) == 0)
            return 1;

        if (strncasecmp(p, "<isindex>", 9) == 0)
            return 1;

        ++p;
    }
}

int RecogniseMimeType(char *p)
{
    if (strncmp(p, "text/html", 9) == 0)
        return HTMLDOCUMENT;

    if (strncmp(p, "image/gif", 9) == 0)
        return XVDOCUMENT;
}

int HeaderLength(char *buf, int *type)
{
    int c;
    char *p;

    if (strncmp(buf, "HTTP", 4) != 0)
        return 0;

    p = buf;

    while (p)
    {
        p = strchr(p, '\n');

        ++p;

        if (strncasecmp(p, "content-type: ", 14) == 0)
            *type = RecogniseMimeType(p+14);

        if (*p == '\n')
            return 1 + p - buf;

        if (*p == '\r' && *++p == '\n')
            return 1 + p - buf;

    }

    return 0;
}

/* Get specified href according to where parameter:

        a)  LOCAL defaults to direct file access
        b)  REMOTE defaults to CurrentDoc.where

    who is NULL or a usename:password pair for authorization
    where specifies if href is to be interpreted as a local file name.

    The global NewDoc is setup for all fields except for height.
    The global url is set to the normalised href.
*/

char *GetDocument(char *href, char *who, int where)
{
    int s, n, hlen, len, vg;
    long cp;
    char *p, *request, *buf;

    NewDoc.length = 0;

    /* parse href to unpack fields and and create absolute URL */

    if (!ParseReference(href, where))   /* itself responsible for warnings */
        return NULL;

    if (NewDoc.where)
        return GetFile(href);

    /* manage name:password */

    if (who)
        StoreNamePW(who);
    else
        who = RetrieveNamePW();

    /* next check if document is in cache - if not
       if sets NewDoc.cache to suitable filename
       to store retrieved data in shared cache */

    if (NewDoc.where == REMOTE && (buf = GetCachedDoc()))
        return buf;

    ShowAbortButton(1);

    if (NewDoc.protocol == NEWS)
    {
        buf = GetNewsDocument(NewDoc.host, NewDoc.path);
        RegisterDoc(buf);
    }

    if (NewDoc.protocol == FTP)
    {
        buf = GetFTPdocument(NewDoc.host, NewDoc.path, who);
        RegisterDoc(buf);
    }

    /* check if we could get the document directly - note kludge for own system */

    if (NewDoc.protocol == MYFILE || ((NewDoc.protocol == HTTP || NewDoc.protocol == FTP) && IsMyName(NewDoc.host)))
    {
        ShowAbortButton(0);
        return GetFile(NewDoc.path);
    }

    /* create a socket */

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (s == -1)
    {
        ShowAbortButton(0);
        Warn("Couldn't create a socket for %s!\n", NewDoc.host);
        return NULL;
    }

    Announce("Connecting to %s on port %d", NewDoc.host, NewDoc.port);

    vg = 0;

    if (!Connect(s, NewDoc.host, NewDoc.port, &vg))
    {
        if (NewDoc.port != OLD_PORT)
        {
            if (!Authorize)
                ShowAbortButton(0);

            return NULL;
        }

        Announce("Retrying %s on port %d", NewDoc.host, HTTP_PORT);

        s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (s == -1)
        {
            ShowAbortButton(0);
            Warn("Couldn't create a socket for %s!\n", NewDoc.host);
            return NULL;
        }

        if (!Connect(s, NewDoc.host, HTTP_PORT, &vg))
        {
            if (!Authorize)
                ShowAbortButton(0);

            return NULL;
        }

        NewDoc.port = HTTP_PORT;
        NewDoc.url = UnivRefLoc(&NewDoc);
    }

    /* set up request string: "GET http://fred.hp.com/hypertext/pub/junk.html" */

    request = HTRQrequestString(&NewDoc, &len, who);

    Announce(TextLine(request));

    /* and send it to server */

    if (XPSend(s, request, len, 0) != len)
    {
        ShowAbortButton(0);
        Warn("Couldn't make connection with %s on port %d!\n", NewDoc.host, NewDoc.port);
        return NULL;
    }

    NewDoc.buffer = GetData(s, &len);

    if (debug)
        printf("received a total of %d bytes\n", len);

    if (!NewDoc.buffer)
    {
        ShowAbortButton(0);
        Beep();
        SetStatusString(NULL);
        return NULL;
    }

    if (len == 0)
    {
        ShowAbortButton(0);
        Warn("No data available for %s", NewDoc.url);
        FreeDoc(&NewDoc);
        return NULL;
    }

    /* check for HTTP status code of 401 i.e. unauthorized access */

    if ((strncmp(NewDoc.buffer, "HTTP/", 5) == 0) && (p = strchr(NewDoc.buffer, ' ')))
    {
        sscanf(p+1, "%d", &n);

        if (n == 401)
        {
            Beep();
            GetAuthorization(REMOTE, NewDoc.url);
            return NULL;
        }
        else if (n != 200)
            Beep();

        NewDoc.hdrlen = HeaderLength(NewDoc.buffer, &NewDoc.type);
    }
    else if (strncasecmp(NewDoc.buffer, "<plaintext>", 11) == 0)
    {  /* strip <PLAINTEXT>/r/n tag at start of text files */
        n = 11;

        if (NewDoc.buffer[n] == '\r')
            ++n;

        if (NewDoc.buffer[n] == '\n')
            ++n;

        NewDoc.hdrlen = n;

        if (NewDoc.type == HTMLDOCUMENT)
            NewDoc.type = TEXTDOCUMENT;
    }

    ShowAbortButton(0);

  /* buffer and length will alter if file is decompressed */

    NewDoc.length = len;
    NewDocumentType();  /* determine document type */

    RegisterDoc(NewDoc.buffer);
    Announce(NewDoc.url);
    return NewDoc.buffer;
}

