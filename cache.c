/* cache.c - manage a cache of documents */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "www.h"

/* these need to be preserved by push/pop operation */

extern char *buffer;
extern int hdrlen;
extern int cacheIndex;
extern int document;
extern int debug;
extern Doc NewDoc, CurrentDoc;

extern int FtpCtrlSkt;

/* currently restricted to a push down stack of documents
   saved as a single temporary file which is deleted upon exit */

struct authlist
{
    int protocol;
    char *hostname;
    char *namepw;
    struct authlist *next;
};

FILE *fpHist = NULL;

struct hlist
{
    int where
;
    int type;
    long length;
    long offset;        /* current position in document */
    int hdrlen;         /* length of MIME header */
    int protocol;       /* if FILE then local so don't consult cache server */
    int port;
    char *host;
    char *path;
    char *anchor;
    char *url;          /* absolute URL in standard format */
    char *cache;        /* document file name in shared cache */
};

/* use an expandable array */

#define HISTMAX     100

int histMax = HISTMAX;
int histSize = 0;
struct hlist *history = NULL;
struct authlist *AuthList = NULL;

void FreeDoc(Doc *doc)
{
    Free(doc->buffer);
    Free(doc->host);
    Free(doc->path);
    Free(doc->anchor);
    Free(doc->url);
    Free(doc->cache);

    doc->port = 0;
    doc->protocol = 0;
    doc->buffer = 0;
    doc->hdrlen = 0;
    doc->length = 0;
    doc->height = 0;
    doc->offset = 0;
    doc->host = 0;
    doc->path = 0;
    doc->anchor = 0;
    doc->url = 0;
    doc->cache = 0;
}

void SetCurrent()
{
    FreeDoc(&CurrentDoc);

  /* copy all fields from NewDoc to CurrentDoc */

    CurrentDoc = NewDoc;

  /* and clear all fields in NewDoc to avoid confusion */

    NewDoc.port = 0;
    NewDoc.protocol = 0;
    NewDoc.buffer = 0;
    NewDoc.hdrlen = 0;
    NewDoc.length = 0;
    NewDoc.height = 0;
    NewDoc.offset = 0;
    NewDoc.host = 0;
    NewDoc.path = 0;
    NewDoc.anchor = 0;
    NewDoc.url = 0;
    NewDoc.cache = 0;
}

/*
   Push details of current doc onto history stack,
   we can then backtrack to it and retrieve its
   contents from the shared cache or a local file.
*/

int PushDoc(long offset)
{
    struct hlist *hp;

    if (history == NULL)
    {
        history = (struct hlist *)malloc(sizeof(hp) * histMax);

        if (history == NULL)
        {
            Warn("Can't allocate history array\n");
            return 0;
        }
    }

    /* expand history array as needed */

    if (histSize == histMax)
    {
        histMax = 2 * histMax;
        hp = (struct hlist *)realloc(history, sizeof(hp) * histMax);

        if (hp == NULL)
        {
            Warn("Can't grow history array\n");
            return 0;
        }

        history = hp;
    }

    hp = history+histSize;

    if (hp != NULL)
    {
        hp->where = CurrentDoc.where;
        hp->type = CurrentDoc.type;
        hp->offset = offset;
        hp->hdrlen = CurrentDoc.hdrlen;
        hp->port = CurrentDoc.port;
        hp->protocol = CurrentDoc.protocol;
        hp->host = strdup(CurrentDoc.host);
        hp->path = strdup(CurrentDoc.path);
        hp->url = strdup(CurrentDoc.url);
        hp->cache = strdup(CurrentDoc.cache);

        if (CurrentDoc.anchor)
            hp->anchor = strdup(CurrentDoc.anchor);
        else
            hp->anchor = NULL;

        histSize++;
    }

    if (debug)
        printf("pushed (%d) %s\n", histSize, CurrentDoc.path);

    return 1;
}

/* pop history stack and retrieve doc from cache or local file
   as appropriate to protocol first used to retrieve doc */

char *PopDoc(long *offset)
{
    long len;
    int n;
    char *s, *doc;
    struct hlist *hp;

    if (history == NULL || histSize == 0)
        return NULL;

    FreeDoc(&NewDoc);
    hp = history + histSize - 1;    


    /*  If current doc is still the right one not much to do */

    if (hp->type == CurrentDoc.type &&
             hp->protocol == CurrentDoc.protocol &&
             hp->port == CurrentDoc.port &&
             (strcmp(hp->host, CurrentDoc.host) == 0) &&
             (strcmp(hp->path, CurrentDoc.path) == 0) )
    {
        NewDoc.where = CurrentDoc.where;
        NewDoc.type = CurrentDoc.type;
        NewDoc.buffer = CurrentDoc.buffer;
        CurrentDoc.buffer = 0; /* stop SetCurrent from freeing document buffer */
        NewDoc.length = CurrentDoc.length;
        NewDoc.hdrlen = CurrentDoc.hdrlen;
        NewDoc.port = CurrentDoc.port;
        NewDoc.protocol = CurrentDoc.protocol;
        NewDoc.host = strdup(CurrentDoc.host);
        NewDoc.path = strdup(CurrentDoc.path);
        *offset = NewDoc.offset = hp->offset;

        if (hp->anchor)
            NewDoc.anchor = hp->anchor;

        NewDoc.url = UnivRefLoc(&NewDoc);
        NewDoc.cache = strdup(CurrentDoc.cache);

        Free(hp->host);
        Free(hp->path);
        Free(hp->url);
        Free(hp->cache);

        --histSize;
        return NewDoc.buffer;
    }

    *offset = NewDoc.offset = hp->offset;
    NewDoc.type = hp->type;
    NewDoc.where = hp->where;

    GetDocument(hp->url, NULL, hp->where);

#if 0
    if ((fpHist = fopen(hp->cache, "r")) == NULL)
    {
        Warn("can't open cache file: %s", hp->cache);
        return NULL;
    }

    NewDoc.type = hp->type;
    NewDoc.length = len = hp->length;
    NewDoc.hdrlen = hp->hdrlen;
    s = (char *)malloc(len+1);
    doc = s;

    /* copy document from file including terminating '\0' */

    errno = 0;
    n = fread(s, len, 1, fpHist);
    s[len] = '\0';

    if (errno || n < 1)
    {
        Warn("error reading historyfile - errno %d", errno);
        free(doc);
        return NULL;
    }

    NewDoc.buffer = s;
#endif
    /* check if we need to close current FTP connection */

    if (FtpCtrlSkt != -1 && ((hp->protocol != FTP)
                             || strcmp(CurrentDoc.host, hp->host) != 0))
    {
        CloseFTP();
        FtpCtrlSkt = -1;
    }

#if 0
    NewDoc.port = hp->port;
    NewDoc.protocol = hp->protocol;
    NewDoc.host = strdup(hp->host);
    NewDoc.path = strdup(hp->path);

    if (hp->anchor)
        NewDoc.anchor = strdup(hp->anchor);
#endif

    Free(hp->host);
    Free(hp->path);
    Free(hp->anchor);

#if 0
    NewDoc.url = UnivRefLoc(&NewDoc);
#endif
    --histSize;

    if (debug)
        printf("popped (%d) %s\n", histSize, NewDoc.path);

    return NewDoc.buffer;
}

/* read file from shared cache */
char *GetCachedDoc(void)
{
    char command[256], *response, *file, *q, *buf;
    int len, fd;
    unsigned int size;
    FILE *fp;

    sprintf(command, "FIND %s", NewDoc.url);

    /* error message issued by QueryCacheServer() */
    if ((len = QueryCacheServer(command, &response)) <= 0)
        return 0;

    /* now parse response to extract status code and file name */

    response[len] = '\0';

    /* 404 Not found - then note suggested file name for
       where to save data after retrieving it in normal way */

    NewDoc.cache = NULL;

    if (strncmp(response, "404", 3) == 0)
    {
        file = strchr(response, ':');
        file += 2;  /* to start of file name */
        for (q = file; *q && *q != '\n' && *q != '\r'; ++q);
            *q = '\0';

        NewDoc.cache = strdup(file);
        return NULL;
    }

    /* otherwise should be "200 OK\nfile\n\n" */

    if (strncmp(response, "200", 3) != 0)
        return 0;

    file = strchr(response, '\n');

    while ( *file == '\n' || *file== '\r')
        ++file;

    for (q = file; *q && *q != '\n' && *q != '\r'; ++q);
    *q = '\0';

    if ((fd = open(file, O_RDONLY)) == -1)
    {
        Warn("%s: errno %d", file, errno);
        return NULL;
    }

    size = lseek(fd, 0L, SEEK_END);

    buf = malloc(1 + size);
    lseek(fd, 0L, SEEK_SET);

    /* the next line assumes that read won't return -1 look at man page! */
    buf[read(fd, (void *)buf, size)] = '\0';

    close(fd);
    NewDoc.buffer = buf;
    NewDoc.length = size;

    if (strncmp(NewDoc.buffer, "HTTP/", 5) == 0)
        NewDoc.hdrlen = HeaderLength(NewDoc.buffer, &NewDoc.type);
    else
        NewDoc.hdrlen = 0;

    NewDoc.cache = strdup(file);
    NewDocumentType();
    return NewDoc.buffer;
}

/* save NewDoc in shared cache and register with server */

int RegisterDoc(char *buf)
{
    char *response, cmd[512];
    int len;
    FILE *fp;

    Announce("Saving data in shared cache ...");

    if ((fp = fopen(NewDoc.cache, "w")) == NULL)
    {
        Warn("Can't create cache file: %s", NewDoc.cache);
        return 0;
    }

    fwrite(NewDoc.buffer, NewDoc.length, 1, fp);
    fclose(fp);

    sprintf(cmd, "REGISTER %s %s", NewDoc.url, NewDoc.cache);

    /* error message issued by QueryCacheServer() */
    if ((len = QueryCacheServer(cmd, &response)) <= 0)
        return 0;

    return 1;
}

/* manage record of authorization for protocol, host

  using the structure:

        struct authlist
        {
            int protocol;
            char *hostname;
            char *namepw;
            struct authlist *next;
        };
*/

/* domain matching for host names */

int SameHosts(char *host1, char *host2)
{
    char *p, *q, *r;

    if (strcasecmp(host1, host2) == 0)
        return 1;

    p = strchr(host1, '.');
    q = strchr(host2, '.');

    if (p)
    {
        if (q)
            return 0;
        else
        {
            r = strchr(MyHostName(), '.');

            if (strcasecmp(p, r) != 0)
                return 0;

            if (strncasecmp(host1, host2, p-host1) == 0)
                return 1;

            return 0;
        }
    }
    else
    {
        if (q)
        {
            r = strchr(MyHostName(), '.');

            if (strcasecmp(q, r) != 0)
                return 0;

            if (strncasecmp(host1, host2, q-host2) == 0)
                return 1;

            return 0;
        }

        return 0;
    }
}

int StoreNamePW(char *who)
{
    char *s;
    int i, protocol;
    struct authlist *pl, *butlast, *prev, *new;

    /* first check for an existing entry */

    i = 0;
    prev = pl = AuthList;
    protocol = NewDoc.protocol; /* ProtoColNum(NewScheme); */

    while (pl != NULL)
    {
        ++i;
        butlast = prev;

        if (pl->protocol == protocol &&
            strcmp(pl->hostname, NewDoc.host) == 0)
        {
            break;
        }

        prev = pl;
        pl = pl->next;
    }

    if (pl)
    {
        if (strcmp(pl->namepw, who) == 0)
            return 1;

        s= strdup(who);

        if (s)
        {
            free(pl->namepw);
            pl->namepw = s;
            return 1;
        }

        return 0;
    }

    /* else create new node */

    /* if list is very long then destroy last entry */

    if (i > 30)
    {
        free(butlast->next->hostname);
        free(butlast->next->namepw);
        free(butlast->next);
        butlast->next = NULL;
    }

    new = malloc(sizeof(struct authlist));

    if (!new)
        return 0;

    new->protocol = protocol;

    s = strdup(NewDoc.host);

    if (!s)
    {
        free(new);
        return 0;
    }

    new->hostname = s;

    s = strdup(who);

    if (!s)
    {
        free(new->hostname);
        free(new);
        return 0;
    }

    new->namepw = s;
    new->next = AuthList;
    AuthList = new;
    return 1;
}

/* find name:password for host/protocol, moving entry to front of list */

char *RetrieveNamePW(void)
{
    int protocol;
    struct authlist *pl, *prev;

    prev = pl = AuthList;
    protocol = NewDoc.protocol;  /* ProtoColNum(NewScheme); */

    while (pl != NULL)
    {
        if (pl->protocol == protocol &&
            strcmp(pl->hostname, NewDoc.host) == 0)
        {
            /* move to front of list */

            if (prev != pl)
            {
                prev->next = pl->next;
                pl->next = AuthList;
            }

            return pl->namepw;
        }

        prev = pl;
        pl = pl->next;
    }

    return NULL;
}
