/* w3cache.c 

This is a UDP server for managing a shared cache of documents
based on their URLs and HTTP headers. It uses the gdbm library
for a disk based index of URL-> file name, and runs as a background
process which processes requests one at a time. It opens the database
as a writer and hence only one such server can be run at a time
per cache database. If a URL is unknown the server returns a suggested
file name which is guaranteed to be unique and is based on a counter
incremented for each such request.

Clients are responsible for retrieving documents and saving them into
the suggested file name (typically a shared directory). The document
must then be registered with the cache database. Note that entries
in the database may point to files which have already been deleted.
Clients should test for this and retrieve the document normally if
this situation occurs.

The garbage collector is run as a separate program and continuously
scans the files in the cache directory and purges them and the entry
in the database once certain criteria are met. Requests from the
garbage collector and www clients are serialised by the cache server
and hence can coexist peacebly. 

The FIRST and NEXT methods are used to purge database entries which
no longer point to files. The garbage collector also scans the
files directly, and uses the stat info to determine the time of last
access or modification. For http files (as given by the URL) the
header is scanned for an Expires: field. http files are also tested
for consistency with the remote server using the HEAD and GET methods.
A recent proposal involves sending the Date: field as part of a GET
request. HTTP servers then returns error 304 (?) if the client's copy
is upto date otherwise it returns the document as normal.

I have tried to use the same error codes as HTTP.

The request starts with a method and optionally followed by a URL
and then other parameters depending on the method type.

    Methods:

        VERSION                   -- return version of cache server --

        FIND url                  -- return http header or suggested filename

        REGISTER url filename     -- add/replace entry for url

        PURGE url                 -- remove entry for url

        FORGET filename           -- remove entry for filename

        SHUTDOWN                  -- kill server nicely!

        FIRST                     -- first url in cache

        NEXT url                  -- next url following "url"

Dave Ragggett,  Wed  2-Mar-94
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include "gdbm.h"

#define VERSION     "1.0a"

#define DEBUG  1      /* delete this line for final code */
#define PORT    2786  /* service port number */

#define BUFSIZE 1024  /* max size of receive packets */

typedef void Sigfunc(int);  /* simplifies event handling */

#if 0
extern int gdbm_errno;
#endif
extern gdbm_error gdbm_errno;

int s;      /* socket descriptor */

int buflen;             /* number of bytes read */
char buffer[BUFSIZE];   /* receive buffer */

int rsize = 0;          /* size of response buffer */
char *response;         /* malloc'ed response buffer */

struct hostent *hp;   /* pointer to host info for requested host */
struct servent *sp;   /* pointer to service info */

struct sockaddr_in myaddr_in;       /* for local socket address */
struct sockaddr_in clientaddr_in;   /* client's socket address */

int NoReader;       /* set bu SIGPIPE */
GDBM_FILE dbf = NULL;      /* handle to gdbm database */
char *cache_directory;     /* cache directory passed via argv */

/* most kill signals are caught to ensure a tidy exit */

void tidyexit(int sig)
{
    if (dbf)
        gdbm_close(dbf);

    exit(0);
}

void trapsignal(int sig)
{
    if (signal(sig, SIG_IGN) != SIG_IGN)
        signal(sig, tidyexit);
}

void fatal_func(char *error)
{
#ifdef DEBUG
    fprintf(stderr, "gdbm fatal error: %s\n", error);
#endif
}


/* Generate a unique file name for use by client */
datum Gensym(void)
{
    datum key, content;

    key.dptr = "Gensym key";
    key.dsize = strlen(key.dptr);

    content = gdbm_fetch(dbf, key);

    /* initialise counter if first time */

    if (content.dptr == NULL)
    {
        char *s = "1";

        content.dsize = 1 + strlen(s);
        content.dptr = (char *)malloc(content.dsize);
        strcpy(content.dptr, s);
    }
    else /* increment counter */
    {
        int len;
        unsigned long counter;
        char buf[32];

        sscanf(content.dptr, "%lu", &counter);
        sprintf(buf, "%lu", ++counter);
        len = 1 + strlen(buf);
        content.dptr = realloc(content.dptr, len);
        memcpy(content.dptr, buf, len);
    }

    /* increment counter and free old data/alloc new */

    gdbm_store(dbf, key, content, GDBM_REPLACE);
    return content;
}

int SuitableFileName(char *access, char **msg, int *msglen)
{
    int n;
    char *p;
    datum content;

    content = Gensym();
    n = 1024;

    if (rsize < n)
    {
        response = realloc(response, n);

        if (response == NULL)
        {
            free(content.dptr);
            *msg = "500 internal error - can't realloc buffer\n\n";
            *msglen = 1 + strlen(*msg);
            return 0;
        }

        rsize = n;
    }

    sprintf(response, "404 Not found\nUseFileName: %s/%s_%s\n\n",
         cache_directory, access, content.dptr);

    free(content.dptr);
    *msg = response;
    *msglen = 1 + strlen(response);

    return 1;
}


int Version(char **msg, int *msglen)
{
    int n, m;
    char *s;

    s = "200 OK\nVersion ";
    n = strlen(s) + strlen(VERSION) + 1;

    if (rsize < n+1)
    {
        response = realloc(response, n+1);

        if (response == NULL)
        {
            *msg = "500 internal error - can't realloc buffer\n\n";
            *msglen = 1 + strlen(*msg);
            return 0;
        }

        rsize = n+1;
    }

    sprintf(response, "%s%s\n", s, VERSION);
    *msg = response;
    *msglen = n;

    return 1;
}

int FindURL(char **msg, int *msglen)
{
    int n, m;
    char *url, *access, *p, *s;
    datum key, content;

    p = buffer;
    while (*p  && *p != ' ') ++p;
    while (*p == ' ') ++p;

    url = p;

    for (n = 0; (*p && *p != ' ' && *p != '\n') ;)
    {
        ++p;
        ++n;
    }

    if (n == 0)
    {
        *msg = "400 Bad request - missing url\n\n";
        *msglen = 1 + strlen(*msg);
        return 0;
    }

    key.dptr = url;
    key.dsize = n;

    content = gdbm_fetch (dbf, key);

    /* if not found then return suitable file name */

    if (content.dptr == NULL)
    {
        if (strncasecmp(url, "http:", 5) == 0)
            access = "http";
        else if (strncasecmp(url, "gopher:", 5) == 0)
            access = "gopher";
        else if (strncasecmp(url, "ftp:", 5) == 0)
            access = "ftp";
        else
            access = "www";

        return SuitableFileName(access, msg, msglen);
    }

    s = "200 OK\n";
    m = strlen(s);
    n = content.dsize + m + 3;

    if (rsize < n)
    {
        response = realloc(response, n);

        if (response == NULL)
        {
            free(content.dptr);
            *msg = "500 internal error - can't realloc buffer\n\n";
            *msglen = 1 + strlen(*msg);
            return 0;
        }

        rsize = n;
    }

    p = response;
    strcpy(p, s);
    p += m;
    
    memcpy(p, content.dptr, content.dsize);
    free(content.dptr);
    p += content.dsize;
    strcpy(p, "\n\n");  /* 3 bytes including terminator */

    *msg = response;
    *msglen = n;

    return 1;
}

int RegisterURL(char **msg, int *msglen)
{
    int n;
    char *url, *filename, *p, *s;
    datum key, content;

    p = buffer;
    while (*p  && *p != ' ') ++p;
    while (*p == ' ') ++p;

    url = p;

    for (n = 0; (*p && *p != ' ' && *p != '\n') ;)
    {
        ++p;
        ++n;
    }

    if (n == 0)
    {
        *msg = "400 Bad request - missing url\n\n";
        *msglen = 1 + strlen(*msg);
        return 0;
    }

    key.dptr = url;
    key.dsize = n;

    while (*p == ' ') ++p;
    filename = p;

    for (n = 0; (*p && *p != ' ' && *p != '\n') ;)
    {
        ++p;
        ++n;
    }

    if (n == 0)
    {
        *msg = "400 Bad request - missing filename\n\n";
        *msglen = 1 + strlen(*msg);
        return 0;
    }

    content.dptr = filename;
    content.dsize = n;

    if (gdbm_store (dbf, key, content, GDBM_REPLACE) != 0)
    {
        *msg = "500 internal error - can't register URL\n\n";
        *msglen = 1 + strlen(*msg);
    }
    else
    {
        *msg = "200 Registered OK\n";
        *msglen = 1 + strlen(*msg);
    }
}

int PurgeURL(char **msg, int *msglen)
{
    int  n;
    char *url, *p;
    datum key;

    p = buffer;
    while (*p  && *p != ' ') ++p;
    while (*p == ' ') ++p;

    url = p;

    for (n = 0; (*p && *p != ' ' && *p != '\n') ;)
    {
        ++p;
        ++n;
    }

    if (n == 0)
    {
        *msg = "400 Bad request - missing url\n\n";
        *msglen = 1 + strlen(*msg);
        return 0;
    }

    key.dptr = url;
    key.dsize = n;

    if (gdbm_delete (dbf, key) != 0)
    {
        *msg = "404 Not found\n\n";
        *msglen = 1 + strlen(*msg);
        return 0;
    }

    *msg = "200 Deleted OK\n\n";
    *msglen = 1 + strlen(*msg);

    return 1;
}

/* search for matching entry and remove from database */

int ForgetFileName(char **msg, int *msglen)
{
    int  n;
    char *filename, *p;
    datum key, next;

    p = buffer;
    while (*p  && *p != ' ') ++p;
    while (*p == ' ') ++p;

    filename = p;

    for (n = 0; (*p && *p != ' ' && *p != '\n') ;)
    {
        ++p;
        ++n;
    }

    if (n == 0)
    {
        *msg = "400 Bad request - missing file name\n\n";
        *msglen = 1 + strlen(*msg);
        return 0;
    }

    /* iterate through database for matching key */

    key = gdbm_firstkey (dbf);

    for (;;)
    {
        if (key.dptr == NULL)
        {
            *msg = "404 Not found\n\n";
            *msglen = 1 + strlen(*msg);
            return 0;
        }

        if (n == key.dsize && strncmp(filename, key.dptr, key.dsize) == 0)
            break;

        next = gdbm_nextkey (dbf, key);

        free(key.dptr);
        key = next;
    }

    if (gdbm_delete (dbf, key) != 0)
    {
        *msg = "404 Not found\n\n";
        *msglen = 1 + strlen(*msg);
        return 0;
    }

    *msg = "200 Deleted OK\n\n";
    *msglen = 1 + strlen(*msg);

    return 1;
}

int FirstKey(char **msg, int *msglen)
{
    int n, m;
    char *url, *p, *s;
    datum key;

    key = gdbm_firstkey ( dbf );

    if (key.dptr == NULL)
    {
        *msg = "404 Not found - empty cache\n\n";
        *msglen = 1 + strlen(*msg);
        return 0;
    }

    s = "200 OK\n";
    m = strlen(s);
    n = key.dsize + m + 3;

    if (rsize < n)
    {
        response = realloc(response, n);

        if (response == NULL)
        {
            free(key.dptr);
            *msg = "500 internal error - can't realloc buffer\n\n";
            *msglen = 1 + strlen(*msg);
            return 0;
        }

        rsize = n;
    }

    p = response;
    strcpy(p, s);
    p += m;
    
    memcpy(p, key.dptr, key.dsize);
    free(key.dptr);
    p += key.dsize;
    strcpy(p, "\n\n");  /* 3 bytes including terminator */

    *msg = response;
    *msglen = n;

    return 1;
}

int NextKey(char **msg, int *msglen)
{
    int n, m;
    char *url, *p, *s;
    datum key, nextkey;

    p = buffer;
    while (*p  && *p != ' ') ++p;
    while (*p == ' ') ++p;

    url = p;

    if (*p == '\0')
    {
        *msg = "400 Bad request - missing url\n\n";
        *msglen = 1 + strlen(*msg);
        return 0;
    }

    for (n = 0; (*p && *p != ' ' && *p != '\n') ;)
    {
        ++p;
        ++n;
    }

    key.dptr = url;
    key.dsize = n;

    nextkey = gdbm_nextkey ( dbf, key );

    if (nextkey.dptr == NULL)
    {
        *msg = "404 Not found - no more entries\n\n";
        *msglen = 1 + strlen(*msg);
        return 0;
    }

    s = "200 OK\n";
    m = strlen(s);
    n = nextkey.dsize + m + 3;

    if (rsize < n)
    {
        response = realloc(response, n);

        if (response == NULL)
        {
            free(nextkey.dptr);
            *msg = "500 internal error - can't realloc buffer\n\n";
            *msglen = 1 + strlen(*msg);
            return 0;
        }

        rsize = n;
    }

    p = response;
    strcpy(p, s);
    p += m;
    
    memcpy(p, nextkey.dptr, nextkey.dsize);
    free(nextkey.dptr);
    p += nextkey.dsize;
    strcpy(p, "\n\n");  /* 3 bytes including terminator */

    *msg = response;
    *msglen = n;

    return 1;
}

/*
   Request is null terminated and in buffer with length buflen bytes.
   Process it and return with *msg pointing to replay msg and *msglen
   set to its length in bytes.
*/

void ProcessRequest(char **msg, int *msglen)
{
    int len;
    char *p;

    /* parse request */

    for (p = buffer; *p && *p != ' ' && *p != '\n'; ++p);
    len = p - buffer;

    if (len == 7 && strncasecmp(buffer, "version", len) == 0)
        Version(msg, msglen);
    else if (len == 4 && strncasecmp(buffer, "find", len) == 0)
        FindURL(msg, msglen);
    else if (len == 8 && strncasecmp(buffer, "register", len) == 0)
        RegisterURL(msg, msglen);
    else if (len == 5 && strncasecmp(buffer, "purge", len) == 0)
        PurgeURL(msg, msglen);
    else if (len == 6 && strncasecmp(buffer, "forget", len) == 0)
        ForgetFileName(msg, msglen);
    else if (len == 5 && strncasecmp(buffer, "first", len) == 0)
        FirstKey(msg, msglen);
    else if (len == 4 && strncasecmp(buffer, "next", len) == 0)
        NextKey(msg, msglen);
    else if (len == 8 && strncasecmp(buffer, "shutdown", len) == 0)
    {
        gdbm_close(dbf);
        dbf = NULL;
        *msg = "202 Shutting down server\n\n";
        *msglen = 1 + strlen(*msg);
    }
    else
    {
#ifdef DEBUG
        fprintf(stderr, "Unknown method:\n %s\n", buffer);
#endif
        *msg = "400 Unknown method\n\n";
        *msglen = 1 + strlen(*msg);
    }
}

void nameserver(void)
{
    int addrlen, msglen, len;
    char *msg, database[64];

 /* initialise response buffer */

    if (rsize == 0)
    {
        rsize = 1024;
        response = malloc(rsize);

        if (response == NULL)
        {
            if (DEBUG)
                fprintf(stderr, "Can't alloc response buffer!\n");

            exit(1);
        }
    }

 /* ignore writes on pipe with no reader */

    signal(SIGPIPE, SIG_IGN);

 /* trap kill signals to avoid problems with lock mechanism */

    trapsignal(SIGALRM);
    trapsignal(SIGEMT);
    trapsignal(SIGFPE);
    trapsignal(SIGHUP);
    trapsignal(SIGILL);
    trapsignal(SIGINT);
    trapsignal(SIGIOT);
    trapsignal(SIGKILL);
    trapsignal(SIGPROF);
#if 0
    trapsignal(SIGPWR);
#endif
    trapsignal(SIGQUIT);
    trapsignal(SIGSEGV);
    trapsignal(SIGSTOP);
    trapsignal(SIGSYS);
    trapsignal(SIGTERM);
    trapsignal(SIGTRAP);
    trapsignal(SIGTSTP);
    trapsignal(SIGTTIN);
    trapsignal(SIGTTOU);
    trapsignal(SIGUSR1);
    trapsignal(SIGUSR2);
    trapsignal(SIGVTALRM);

    while (dbf != NULL)
    {
        addrlen = sizeof(struct sockaddr_in);

        /* block until client sends us a datagram
           with address of client and buffer length */

        buflen = recvfrom(s, buffer, BUFSIZE-1, 0, &clientaddr_in, &addrlen);

        if (buflen == -1)
            exit(1);

        buffer[buflen] = '\0';  /* add null terminator to data */

        ProcessRequest(&msg, &msglen);
        sendto(s, msg, msglen, 0, &clientaddr_in, addrlen);
    }

    exit(0);
}


/*
  start server and fork process leaving child to do all the work
  so that it doesn't have to be run in the background. It sets up
  a socket and for each incoming request returns an answer

  w3cache DIRECTORYNAME

*/

main(int argc, char **argv)
{
    int len;
    char database[64];

    if (argc != 2)
    {
        fprintf(stderr, "Useage: %s DirectoryName\n", argv[0]);
        fprintf(stderr, "where DirectoryName is the name of a\n");
        fprintf(stderr, "shared directory for placing cache files.\n");
        fprintf(stderr, "Avoid using kill -9 to shutdown server\n");
        fprintf(stderr, "as this screws lock mechanism\n");
        exit(1);
    }

    cache_directory = argv[1];
    myaddr_in.sin_port = PORT;

    s = socket(AF_INET, SOCK_DGRAM, 0);  /* create socket */

    if (s == -1)
    {
        perror(argv[0]);
        printf("%s: unable to create socket for port %d\n", PORT);
        exit(1);
    }

    if (bind(s, &myaddr_in, sizeof(struct sockaddr_in)) == -1)
    {
        perror(argv[0]);
        printf("%s: unable to bind socket for port %d\n", PORT);
        exit(1);
    }

 /* open and create (if needed) the database */

    len = strlen(cache_directory);

    if (cache_directory[len-1] == '/')
        cache_directory[len-1] = '\0';

    sprintf(database, "%s/cache.gdbm", cache_directory);
    dbf = gdbm_open (database, 0, GDBM_WRCREAT, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH), fatal_func);

    if (dbf == NULL)
    {
        fprintf(stderr, "Can't open gdbm database %s errno %d\n", database, gdbm_errno);
        exit(1);
    }

    /* Do setpgrp() so that daemon won't be associated with user's
       control terminal. This is done before the fork, so that the
       child will not become a process group leader. */

#ifndef DEBUG
    setpgrp();
#endif

#ifndef DEBUG
    switch(fork())
    {
        case -1:    /* unable to fork for some reason */
            perror(argv[0]);
            printf("%s: unable to fork daemon\n");
            exit(1);

        case 0:     /* child process (daemon) */
            setsid();       /* create new session */
            umask(0);       /* clear our file creation mask */
            fclose(stdin);  /* no more error messages */
            fclose(stdout);
            fclose(stderr);
#endif
            nameserver();    /* doesn't return */
#ifndef DEBUG

        default:    /* parent process */
            exit(0);
    }
#endif
}
