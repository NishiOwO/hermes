/******************************************************************************

   World Wide Web distributed hypertext server
   ===========================================

   This program responds to requests sent by WWW clients using
   the HTTP 0.9 and 1.0 protocols. If the request names a
   directory, the "ls -al" command is used to create a browsable
   hypertext listing.   

   The server is invoked by inetd and requires the following:

    /etc/services needs the following entry:

        www     80/tcp          # official www port

    /etc/inetd.conf needs the following entry:

        www  stream tcp  nowait  root  /etc/wwwd wwwd

    There are two optional command line paramaters:

        wwwd [-world] [-root pathname]

    The "-world" parameter makes all world readable files and
    directories available without the need for further authorization.

    The "-root pathname" causes the server to chroot to the given
    pathname. This is useful for administrators wishing to restrict
    access to a closed part of the host system.

    Administrators can take advantage of these features by editing
    the /etc/inetd.conf file accordingly, e.g.

        www   stream tcp  nowait  root  /etc/wwwd wwd -world -root /public

    Access to files and directories is otherwise controlled via the
    standard UNIX access mechanisms:

        a)  user name & password
        b)  personal, group and world access bits set with chmod
        c)  /etc/hosts.equiv and .rhosts

    Like other current tools, e.g. ftp and rlogin, the user name
    and password are sent in clear. This doesn't pose undue risks
    due to HP's closed subnet denying access to outsiders. In future
    RSA's public key technology will avoid the need to send passwords.

    A future extension is planned to provide access to searchable
    indexes and databases via a wwwd.conf configuration file.

    Dave Raggett, HP Labs, Bristol UK.

    Copyright Hewlett Packard 1993, All rights reserved.
*******************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>

#define HOSTSIZE    128
#define MSGSIZE     1024
#define BUFSIZE     4098
#define THRESHOLD   1024

/* status codes and methods for ParseHTRQ */

#define ILLEGAL         -1
#define UNSUPPORTED     -2
#define UNAUTHORIZED    -3
#define NOTFOUND        -4
#define INTERNAL        -5

#define GET             0
#define HEAD            1
#define TEXTSEARCH      2
#define CHECKOUT        3
#define CHECKIN         4
#define PUT             5
#define POST            6
#define SHOWMETHOD      7

#define CLIENT  0   /* client socket is zero - inetd */

static int world_readable;          /* authorization parameter */
static int debug;                   /* if true prints debugging info to stdout */
static char *from;                  /* e.g. "Dave_Raggett <dsr@otter.hpl.hp.com>" */
static char *authorization;         /* e.g. "user fred:secret" */
static char *acceptformats;         /* format negotiation */
static char *acceptencoding;        /* format negotiation */
static char *uagent;                /* name of client program (user agent) */
static char *referee;               /* parent document - see HTTP 1.0 spec */
static char *path;                  /* file or directory name */
static char *find;                  /* search string */
static char *vers;                  /* NULL or "HTTP/1.0" */

static struct sockaddr_in client;   /* address info */
static struct sockaddr_in server;   /* address info */
static struct hostent *hp;          /* pointer to host info for remote host */

static char myhost[HOSTSIZE];
static struct stat stat_buf;


/* Send and Recv routines with 2 second timeout */

int XPSend(int skt, char *data, int len, int once)
{
    struct timeval timer;
    int k, n;
    fd_set writefds, exceptfds;

    for(k = 0; ;)
    {
        if (k == len)
            return len;

        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);
        FD_SET(skt, &writefds);
        FD_SET(skt, &exceptfds);
        timer.tv_sec = 2;
        timer.tv_usec = 0;

        n = select(NFDBITS, 0, (int*)&writefds, (int*)&exceptfds, &timer);

        if (n == 0 || FD_ISSET(skt, &exceptfds))  /* timeout or exception */
        {
            close(skt);
            return -1;
        }

        if (FD_ISSET(skt, &writefds))
        {
            n = send(skt, data + k, len - k, 0);

            if (n == -1)
            {
                close(skt);
                return -1;
            }

            k += n;

            if (once && k < len)
            {
                close(skt);
                return -1;
            }
        }
    }
}

/* similar receive operation where len is the maximum amount
   of data to receive, the amount received is generally less */

int XPRecv(int skt, char *msg, int len)
{
    int n;
    fd_set readfds, exceptfds;
    struct timeval timer;

    for (;;)
    {
        FD_ZERO(&readfds);
        FD_ZERO(&exceptfds);

        FD_SET(skt, &readfds);
        FD_SET(skt,&exceptfds);

        timer.tv_sec = 2;
        timer.tv_usec = 0;

        n = select(NFDBITS, (int*)&readfds, 0, (int*)&exceptfds, &timer);

        if (n == 0 || FD_ISSET(skt, &exceptfds))
        {
            close(skt);
            return -1;
        }

        if (FD_ISSET(skt, &readfds))
            return recv(skt, msg, len, 0);
    }
}


/* parse HTRQ msg and return method or status code */

int ParseRequest(char *buf)
{
    int c, method;
    char *p, *q;

    if (strncasecmp(buf, "get ", 4) == 0)
    {
        method = GET;
        path = buf + 4;
    }
    else if (strncasecmp(buf, "head ", 5) == 0)
    {
        method = HEAD;
        path = buf + 5;
    }
    else if (strncasecmp(buf, "textsearch ", 11) == 0)
    {
        method = TEXTSEARCH;
        path = buf + 11;
    }
    else if (strncasecmp(buf, "checkout ", 9) == 0)
    {
        method = CHECKOUT;
        path = buf + 9;
    }
    else if (strncasecmp(buf, "checkin ", 8) == 0)
    {
        method = CHECKIN;
        path = buf + 8;
    }
    else if (strncasecmp(buf, "put ", 4) == 0)
    {
        method = PUT;
        path = buf + 4;
    }
    else if (strncasecmp(buf, "post ", 5) == 0)
    {
        method = POST;
        path = buf + 5;
    }
    else if (strncasecmp(buf, "showmethod ", 11) == 0)
    {
        method = SHOWMETHOD;
        path = buf + 11;
    }
    else
        return ILLEGAL;

    if (method != GET && method != HEAD)
        return UNSUPPORTED;

    p = path;
    q = strchr(p, '\r');

    if (q)
        *q++ = '\0';

    find = NULL;
    vers = NULL;

    for (;;)
    {
        c = *p;

        if (!c)
            break;

        if (c == '?')
        {
            *p++ = '\0';
            find = p;
            continue;
        }

        if (c == ' ')
        {
            *p++ = '\0';
            vers = p;
            break;
        }

        ++p;
    }

    if (*q == '\n')
        ++q;

    p = q;
    q = strchr(p, '\r');

    if (q)
        *q++ = '\0';    

    from = NULL;
    authorization = NULL;
    acceptformats = NULL;
    acceptencoding = NULL;
    uagent = NULL;
    referee = NULL;

    while (*p)
    {
        if (strncasecmp(p, "from: ", 6) == 0)
            from = p+6;
        else if (strncasecmp(p, "authorization: ", 15) == 0)
            authorization = p+15;
        else if (strncasecmp(p, "accept: ", 8) == 0)
            acceptformats = p+8;
        else if (strncasecmp(p, "accept-encoding: ", 17) == 0)
            acceptencoding = p+17;
        else if (strncasecmp(p, "useragent: ", 11) == 0)
            uagent = p+11;
        else if (strncasecmp(p, "referee uri: ", 13) == 0)
            referee = p+13;

        /* else unknown header - ignore it */

        /* get ready for next line */

        if (*q == '\n')
        ++q;

        p = q;
        q = strchr(p, '\r');

        if (q)
            *q++ = '\0';
    }

    /* may need to examine the data pointed to by p ... */

    return method;
}

/* deduce MIME content type from pathname

   Oh dear, Oh dear, MIME content types are
   insufficiently well defined yet, e.g.

   what about all these images types?

   what about compression which is
     basically orthogonal to content type */

char *ContentType(char *path)
{
    char *p;

    p = strrchr(path, '.');

    
    if (strcasecmp(p, ".html") == 0)
        return "text/html";

    if (strcasecmp(p, ".ps") == 0)
        return "text/postscript";

    if (strcasecmp(p, ".ai") == 0)
        return "text/postscript";

    if (strcasecmp(p, ".dvi") == 0)
        return "text/dvi";

    if (strcasecmp(p, ".gif") == 0)
        return "image/gif";

    if (strcasecmp(p, ".jpeg") == 0)
        return "image/jpeg";

    if (strcasecmp(p, ".jpg") == 0)
        return "image/jpeg";;

    if (strcasecmp(p, ".tiff") == 0)
        return "image/tiff";

    if (strcasecmp(p, ".tif") == 0)
        return "image/tiff";

    if (strcasecmp(p, ".pbm") == 0)
        return "image/pbm";

    if (strcasecmp(p, ".pgm") == 0)
        return "image/pgm";

    if (strcasecmp(p, ".ppm") == 0)
        return "image/ppm";

    if (strcasecmp(p, ".xbm") == 0)
        return "image/xdm";

    if (strcasecmp(p, ".pm") == 0)
        return "image/pm";

    if (strcasecmp(p, ".ras") == 0)
        return "image/ras";

    if (strcasecmp(p, ".mpeg") == 0)
        return "video/mpeg";

    if (strcasecmp(p, ".xwd") == 0)
        return "image/xwd";

    if (strcasecmp(p, ".au") == 0)
        return "audio/au";

    return "text/plain";
}

/* write MIME header to buffer and set len to its length */

void MimeHeader(char *buf, int *len, int IsHtml, long size)
{
    int n, dn;

    sprintf(buf, "%nHTTP/1.0 200 Document follows\r\n", &n);
    sprintf(buf+n, "MIME-Version: 1.0\r\n%n", &dn);
    n += dn;

    if (IsHtml)
    {
        sprintf(buf+n, "Content-Type: text/html\r\n%n", &dn);
        n += dn;
    }
    else
    {
        sprintf(buf+n, "Content-Type: %s\r\n%n", ContentType(path), &dn);
        n += dn;
    }

    if (size > 0)
    {
        sprintf(buf+n, "Length: %ld\r\n%n", size, &dn);
        n += dn;
    }

    if (stat_buf.st_mode)
    {
        sprintf(buf+n, "Date: %24.24s\r\n%n", ctime(&(stat_buf.st_mtime)), &dn);
        n += dn;
    }

    /* lastly a blank line to separate off the document contents */

    strcpy(buf+n, "\r\n");
    *len = n+2;
}

char *errmsg(int err)
{
    if (err == ILLEGAL)
        return "400 Bad Request";

    if (err == UNAUTHORIZED)
        return "401 Unauthorized";

    if (err == NOTFOUND)
        return "404 Not found";

    if (err == UNSUPPORTED)
        return "501 Not Implemented";

    return "500 Internal Error";
}

/* if vers then include MIME header */

void ReportError(int error, char *comment)
{
    int n, dn;
    char msg[MSGSIZE];

    if(vers)
    {
        sprintf(msg, "HTTP/1.0 %s\r\n%n", errmsg(error), &n);
        sprintf(msg+n, "MIME-Version: 1.0\r\n%n", &dn);
        n += dn;
        sprintf(msg+n, "Content-Type: text/html\r\n%n", &dn);
        n += dn;

        sprintf(msg+n, "\r\n<title>WWW Server Error</title>\r\n%n", &dn);
        n += dn;
        sprintf(msg+n, "<H1>WWW Error</H1>\r\n%n", &dn);
        n += dn;
        sprintf(msg+n, "%s\r\n%n", errmsg(error), &dn);
        n += dn;

        if (errno)
	{
            sprintf(msg+n, "<P>%s - errno %d\r\n%n", comment, errno, &dn);
            n += dn;
        }
    }
    else
    {
        sprintf(msg, "<title>WWW Server Error</title>\r\n%n", &n);
        sprintf(msg+n, "<H1>WWW Error</H1>\r\n%n", &dn);
        n += dn;
        sprintf(msg+n, "%s\r\n%n", errmsg(error), &dn);
        n += dn;

        if (errno)
	{
            sprintf(msg+n, "<P>%s - errno %d\r\n%n", comment, errno, &dn);
            n += dn;
        }
    }

    XPSend(CLIENT, msg, n, 0);
}

char *ClientName(int skt)
{
    int len;
    struct hostent *hp;
    struct sockaddr_in client;

    len = sizeof(struct sockaddr_in);

    if (getpeername(skt, &client, &len) < 0)
        return "(unknown host)";

    hp = gethostbyaddr((char *)&client.sin_addr,
                            sizeof(struct in_addr),
                            client.sin_family);

    if (hp == NULL)  /* host info unavailable so format its address */
        return inet_ntoa(client.sin_addr);

   return hp->h_name;
}


/* check if client is authorised to see requested document

  either

    a)  document is world readable for server process owner

  or

    b)  user's name and host are given in /etc/hosts.equiv or .rhosts
        (see hosts.equiv [4]) and user has access to this document

  or

    c)  user name/password is ok
        and has access to this document

In case (c) the user name and password are supplied as arguments
to the Authorization: field in the HTRQ:

    Authorization: user fred:secret\r\n

In case (b) only the user name is required.

    Authorization: user dsr\r\n

The password check will fail if /.secure/etc/passwd
is in use and this process doesn't have read access

There isn't as yet a facility to give both a local and a remote
user name as defined by the ruserok() system call.

*/

int Authorized(char *path)
{
    int n;
    char *client, *user, *pass, salt[4];
    struct passwd *pw;

    /* ok if world readable */

    if (debug)
    {
        printf("checking if world readable %d\n", world_readable);
        printf("mode: %c%c%c%c\n",
            ((stat_buf.st_mode & S_IFDIR) ? 'd' : '-'),
            ((stat_buf.st_mode & S_IRUSR) ? 'r' : '-'),
            ((stat_buf.st_mode & S_IRGRP) ? 'r' : '-'),
            ((stat_buf.st_mode & S_IROTH) ? 'r' : '-'));
    }

    if (world_readable && (stat_buf.st_mode & S_IROTH))
    {
        if (debug)
            printf("world readable\n");

        return 1;
    }

    /* check if user name/password have been supplied */

    if (authorization && strncasecmp(authorization, "user ", 5) == 0)
    {
        user = authorization+5;
        pass = strchr(user, ':');

        if (pass)
            *pass++ = '\0';
        else
            pass = "";

        if ((pw = getpwnam((const char *)user)) == NULL)
        {
            errno = 13;  /* set errno to permission denied */
            ReportError(UNAUTHORIZED, "unknown user");
            return 0;
        }

        n = strlen(pw->pw_passwd);

        /* if a password is needed check hosts.equiv and .rhosts */

        if (n > 0 && (client = ClientName(CLIENT)) &&
            ruserok(client, 0, (const char *)user, (const char *)user) == 0)
        {
            goto useraccess;  /* trusted user */
        }

        if (n == 0)     /* no password needed */
            goto useraccess;

        if (strlen(pw->pw_passwd) != 13)
        {
            errno = 13;  /* set errno to permission denied */
            ReportError(UNAUTHORIZED, "bad password entry");
            return 0;
        }

        strncpy(salt, pw->pw_passwd, 2);

        /* if password doesn't match ignore .rhosts file and fail */

        if (strcmp(pw->pw_passwd, crypt(pass, salt)) != 0)
        {
            errno = 13;  /* set errno to permission denied */
            ReportError(UNAUTHORIZED, "missing/wrong password");
            return 0;
        }

        /* change owner of process to given user */

      useraccess:

        setgid(pw->pw_gid);
        setuid(pw->pw_uid);

        /* redo the stat - this time with given user's access rights */

        if (lstat(path, &stat_buf) == -1)
        {
            if (errno == EACCES)
                ReportError(UNAUTHORIZED, "lstat");
            else
                ReportError(NOTFOUND, "lstat");

            return 0;
        }

        if ((stat_buf.st_mode & S_IFLNK) && stat(path, &stat_buf) == -1)
        {
            if (errno == EACCES)
                ReportError(UNAUTHORIZED, "lstat");
            else
                ReportError(NOTFOUND, "lstat");

            return 0;
        }


        if ( stat_buf.st_mode & (S_IRUSR | S_IRGRP| S_IROTH) )
            return 1;
    }

    errno = 13;  /* permission denied */
    ReportError(UNAUTHORIZED, "no read access");
    return 0;
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

/* send file or hypertext directory listing for GET method */

int SendDocument(int method)
{
    int len, dlen, AddSlash, fd, n, c;
    char *p, *q, *r, lbuf[256], buf[BUFSIZE+1];
    long size;
    FILE *fp;

    /* first stat local file */

    if (debug)
        printf("stat on %s\n", path);

    if (lstat(path, &stat_buf) == -1)
    {
        if (errno == EACCES)
            ReportError(UNAUTHORIZED, "lstat");
        else
            ReportError(NOTFOUND, "lstat");

        return 0;
    }

    /* if symbolic link one need's to stat the linked file */

    if ((stat_buf.st_mode & S_IFLNK) && stat(path, &stat_buf) == -1)
    {
        if (errno == EACCES)
            ReportError(UNAUTHORIZED, "stat");
        else
            ReportError(NOTFOUND, "stat");

        return 0;
    }

    if (!Authorized(path))
        return 0;

    if (debug)
        printf("passed authorisation checks\n");

    errno = 0;
    size = stat_buf.st_size;

    if (chdir(path) == 0)
    {
        if (myhost[0] == '\0')
        {
            if (gethostname(myhost, HOSTSIZE) != 0)
            {
                ReportError(INTERNAL, "getting hostname");
                exit(1);
            }
        }

        if (vers)
            MimeHeader(buf, &len, 1, 0);

        /* don't send file data for method HEAD */

        if (method == HEAD)
        {
            if (XPSend(CLIENT, buf, len, 0) != len)
                return 0;

            return 1;
        }

        sprintf(buf+len, "<TITLE>%s at %s</TITLE>\n<H1>%s at %s</H1>\n<XMP>\n%n",
                               path, myhost, path, myhost, &dlen);
        len += dlen;

        /* check is a trailing slash is needed */

        AddSlash = (path[strlen(path)-1] != '/' ? 1 : 0);

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

                    if (strcmp(p, "..") == 0 && (r = ParentDirCh(path)))
                    {
                        if (strcmp(path, "/") == 0)
                             goto next_line;

                        c = *r;
                        *r = '\0';
                        sprintf(buf+len, "%s <A HREF=\"http://%s%s\">", lbuf, myhost, path);
                        *r = c;
                    }
                    else if (*lbuf == 'l')  /* link */
                    {
                        *(p - 4) = '\0';
                         r = strrchr(lbuf, ' ');
                        *r++ = '\0';

                        if (*p == '/')  /* absolute link */
                            sprintf(buf+len, "%s <A HREF=\"http://%s%s\">", lbuf, myhost, p);
                        else if (AddSlash)
                            sprintf(buf+len, "%s <A HREF=\"http://%s%s/%s\">", lbuf, myhost, path, p);
                        else
                            sprintf(buf+len, "%s <A HREF=\"http://%s%s%s\">", lbuf, myhost, path, p);
                    }
                    else if (AddSlash)
                        sprintf(buf+len, "%s <A HREF=\"http://%s%s/%s\">", lbuf, myhost, path, p);
                    else
                        sprintf(buf+len, "%s <A HREF=\"http://%s%s%s\">", lbuf, myhost, path, p);

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

                if (len >= BUFSIZE - THRESHOLD)
                {
                    if (XPSend(CLIENT, buf, len, 0) != len)
                        return 0;

                    len = 0;
                }
            }
        }

        pclose(fp);
        buf[len++] = '\0';

        if (XPSend(CLIENT, buf, len, 0) != len)
            return 0;

        return 1;
    }
    else if ((fd = open(path, O_RDONLY)) == -1)
    {
        if (debug)
            printf("can't open %s\n", path);

        ReportError(UNAUTHORIZED, path);
        return 0;
    }

    /* file size determined during authorization */

     len = 0;

     if (vers)
         MimeHeader(buf, &len, 0, size);

    /* don't send file data for method HEAD */

    if (method == HEAD)
    {
        close(fd);

        if (XPSend(CLIENT, buf, len, 0) != len)
            return 0;

        return 1;
    }

     /* read in large gulps and send to client,
        but first time round place read data after MIME header */

     while (size > 0)
     {
        errno = 0;
        n = read(fd, buf+len, BUFSIZE-len);

        if (n <= 0)
            return 0;

        len += n;

        if (XPSend(CLIENT, buf, len, 0) != len)
            return 0;

        size -= len;
        len = 0;
     }

    close(fd);

    return 1;
}

/* silently exit on specified interrupts */

void onintr(int sig)
{
    exit(1);
}

main(int argc, char **argv)
{
    char *request;
    int n, len, size, method;

    /* deal with command line arguments */

    n = 0;
    world_readable = 0;

    while (--argc > 0)
    {
        ++n;

        if (strcmp(argv[n], "-world") == 0)
            world_readable = 1;
        else if (strcmp(argv[n], "-root") == 0)
        {
            if (--argc > 0)
            {
                ++n;

                if (chroot( (const char*) (argv[n]) ) == -1)
                {
                    ReportError(INTERNAL, "Can't chroot");
                    exit(1);
                }
            }
        }
    }

    /* SIGPIPE event occurs when other end prematurely closes connection */

    if (signal(SIGPIPE, SIG_IGN) != SIG_IGN)
        signal(SIGPIPE, onintr);

    /* Get complete request as one or more packets

       for HTTP 0.9 (i.e. old clients) the request
       is always contained within one packet.

       Otherwise the HTRQ header is terminated by
       a blank line (a line with only CRLF or just LF).

       The header indicates if a message body follows,
       which itself will be terminated by a blank line.
       A body is only expected for the methods:

          PUT, POST, CHECKIN and TEXTSEARCH
    */
    

    size = BUFSIZE;
    len = 0;
    request = malloc(size);

    if (!request)
        exit(1);

    /* examine  first line for presence of version info */

    len = XPRecv(CLIENT, request, BUFSIZE-1);

    if (len == -1)
        exit(1);

    request[len] = '\0';

    if (debug)
        printf("received %d :\"%s\"\n", len, request);

    if (strstr(request, "HTRQ"))  /* true if version is present */
    {
        while (strcmp(request+len-4, "\r\n\r\n") != 0 && strcmp(request+len-2, "\n\n") != 0)
        {
            if (len >= size - THRESHOLD)
            {
                size *= 2;  /* attempt to double size */
                request = realloc(request, size);

                if (request == NULL)
                    exit(1);
            }

            n = XPRecv(CLIENT, request+len, BUFSIZE-len-1);

            if (n == -1)
                exit(1);

            len += n;
            request[len] = '\0';
        }
    }
    else  /* HTRQ/V0.9 so just look for a \n */
    {
        while ( *(request+len-1) != '\n' )
        {
            if (len >= size - THRESHOLD)
            {
                size *= 2;  /* attempt to double size */
                request = realloc(request, size);

                if (request == NULL)
                    exit(1);
            }

            n = XPRecv(CLIENT, request+len, BUFSIZE-len-1);

            if (n == -1)
                exit(1);

            len += n;
            request[len] = '\0';
        }
    }

    method = ParseRequest(request);

    if (method != GET && method != HEAD)
        ReportError(UNSUPPORTED, "parsing request");

    SendDocument(method);
    exit(0);
}

