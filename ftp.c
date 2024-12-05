/* simplified http client - I intend to migrate to WWW Library in due course! */

#include <X11/Xlib.h>
#include "www.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>

#define TEXTDOCUMENT    0
#define HTMLDOCUMENT    1

#define PWDSIZE     256
#define CTRLSIZE    1024
#define BUFSIZE     8192
#define THRESHOLD   1024

#define FTP_PORT    21        /* control port */
#define FTP_DATA    20

char *inet_ntoa();

extern int debug;
extern int Authorize;
extern char *gateway;
extern char *user;
extern Doc NewDoc, CurrentDoc;
extern long NewLength;

static struct sockaddr_in server;       /* address info */
static struct sockaddr_in dataserver;   /* address info */
static struct hostent *hp;              /* other host info */

int FtpCtrlSkt = -1;   /* for caching ftp control socket */
int FtpDataSkt = -1;   /* for caching ftp data socket */

/* keep track of current values for expanding abbreviated references */

char ctrlbuf[CTRLSIZE];
char recvbuf[BUFSIZE];
char pwd[PWDSIZE];

char *GetFileData(int socket)
{
    int len;
    char *buf;

    buf = GetData(socket, &len);
    NewDoc.length = len;

    if (buf)
    {
      /* Kludge for HTML docs with .txt or .doc suffix */
      /* if first non-space char is '<' assume as HTML doc */

         if (IsHTMLDoc(buf, len))
              NewDoc.type = HTMLDOCUMENT;
    }

    return buf;
}


char *GetDirList(int socket)
{
    int n, c, len, k, size, AddSlash;
    char *buffer, *buf, *p, *q, *r, *s;
    char *host, *path;

    host = NewDoc.host;
    path = NewDoc.path;

    if (pwd[0])
        path = pwd;

    AddSlash = 0;

    if (path[strlen(path)-1] != '/')
        AddSlash = 1;

    buf = GetData(socket, &k);

    if (buf == NULL)
        return NULL;

    NewDoc.type = HTMLDOCUMENT;
    size = BUFSIZE;
    buffer = malloc(size);

    if (buffer == 0)
    {
        Warn("couldn't create receive buffer");
        free(buf);
        return NULL;
    }

    sprintf(buffer, "<TITLE>%s at %s</TITLE>\n<H1>%s at %s</H1>\n<PRE>\n",
                path, host, path, host);

    len = strlen(buffer);
    p = buf;

    for(p = buf; *p != '\0'; p = q)
    {
        /* Each line should end with '\n'

           if the \n is missing this must be a partial line
           at the end of the received data, so move it to
           the start and read some more data
        */

        for (q = p; *q && *q != '\n'; ++q);

        *q++ = '\0';  /* q now points to next line */

        /* now ensure the html buffer is large enough */

        if (size - len < THRESHOLD)  /* need to grow buffer */
        {
            size *= 2;  /* attempt to double size */
            r = realloc(buffer, size);

            if (r == NULL)
            {
                Warn("Couldn't realloc buffer to size %ld", size);
                buffer[len] =  '\0';
                free(buf);
                return buffer;
            }

            buffer = r;
        }

        /* Unix dir list has total line */

        if (strncasecmp(p, "total", 5) == 0)
        {
            sprintf(buffer+len, "  %s\n", p);
            len += strlen(buffer+len);
            continue;
        }

        /* last space char occurs before file name */

        r = strrchr(p, ' ');

        if (r)
        {
            *r++ = '\0';

            if (*p == 'l')
            {
                *(r - 4) = '\0';
                s = strrchr(p, ' ');
                *s++ = '\0';

                if (*r == '/')  /* absolute link */
                    sprintf(buffer+len, "  %s <A HREF=\"ftp://%s%s\">", p, host, r);
                else if (AddSlash)
                    sprintf(buffer+len, "  %s <A HREF=\"ftp://%s%s/%s\">", p, host, path, r);
                else
                    sprintf(buffer+len, "  %s <A HREF=\"ftp://%s%s%s\">", p, host, path, r);
            }
            else if (AddSlash)
                 sprintf(buffer+len, "  %s <A HREF=\"ftp://%s%s/%s\">", p, host, path, r);
            else
                 sprintf(buffer+len, "  %s <A HREF=\"ftp://%s%s%s\">", p, host, path, r);

            len += strlen(buffer+len);

            if (*p == 'd')
                 sprintf(buffer+len, "%s/</A>\n", r);
            else if (*p == 'l')
                 sprintf(buffer+len, "%s@</A>\n", s); 
            else if (p[3] == 'x')
                 sprintf(buffer+len, "%s*</A>\n", r);
            else
                 sprintf(buffer+len, "%s</A>\n", r);

            len += strlen(buffer+len);
        }
        else
        {
            if (AddSlash)
                 sprintf(buffer+len, "  <A HREF=\"ftp://%s%s/%s\">%s</A>\n", host, path, p, p);
            else
                 sprintf(buffer+len, "  <A HREF=\"ftp://%s%s%s\">%s</A>\n", host, path, p, p);

            len += strlen(buffer+len);
        }
    }

    free(buf);
    strcpy(buffer+len, "</PRE>\n");
    NewDoc.length = len+7;
    return buffer;
}


#if 0

/* Older more efficient version has problems with tcp/ip
   as the next call to recv() sometimes drops the first
   few bytes from the beginning of the buffer */

char *GetDirList(int socket)
{
    int n, m, c, len, left, size, AddSlash;
    char *buffer, *p, *q, *r, *s;
    char *host, *path;

    host = NewDoc.host;
    path = NewDoc.path;
    AddSlash = 0;

    if (path[strlen(path)-1] != '/')
        AddSlash = 1;

    NewDoc.type = HTMLDOCUMENT;
    size = BUFSIZE;
    buffer = malloc(size);

    if (buffer == 0)
    {
        Warn("Couldn't create receive buffer");
        return NULL;
    }

    sprintf(buffer, "<TITLE>%s at %s</TITLE>\n<H1>%s at %s</H1>\n<PRE>\n",
                path, host, path, host);

    len = strlen(buffer);
    left = 0;
    m = 0;

    for(;;)
    {
        n = recv(socket, recvbuf+left, BUFSIZE-1-left, 0);

        if (n < 0)
        {
            Warn"Failed to read directory listing: errno %d\n", errno);
            free(buffer)
            return NULL;
        }

        if (n == 0)
                break;

        recvbuf[n] = '\0';

        m += n;
        Announce("received %d bytes\n", m);

        if (debug)
             fprintf(stderr, "received %d bytes making a total of %d\n", n, m);
        
        /* process each line in turn */

        for (p = recvbuf; *p; p = q)
        {
            /* Each line should end with '\n'

               if the \n is missing this must be a partial line
               at the end of the received data, so move it to
               the start and read some more data
            */

            for (q = p; *q && *q != '\n'; ++q);

            if (*q == '\0')  /* need to read the rest of the line */
            {
                n = q - p;
                memcpy(recvbuf, p, n+1);
                p = recvbuf;
                left = n;
                break;
            }

            /* check for \r directly before the \n */

            --q;

            if (*q == '\r')
                *q = '\0';

            ++q;

            *q++ = '\0';  /* q now points to next line */

            /* now ensure the html buffer is large enough */

            if (size - len < THRESHOLD)  /* need to grow buffer */
            {
                size *= 2;  /* attempt to double size */
                r = realloc(buffer, size);

                if (r == NULL)
                {
                    Warn("http.c couldn't realloc buffer to size %ld\n", size);
                    buffer[len] =  '\0';
                    return buffer;
                }

                buffer = r;
            }

            /* Unix dir list has total line */

            if (strncasecmp(p, "total", 5) == 0)
            {
                sprintf(buffer+len, "  %s\n", p);
                len += strlen(buffer+len);
                continue;
            }

            /* last space char occurs before file name */

            r = strrchr(p, ' ');

            if (r)
            {
                *r++ = '\0';

                if (*p == 'l')
                {
                    if (strcmp(r, "bsd-386") == 0)
                        r = r;

                    *(r - 4) = '\0';
                    s = strrchr(p, ' ');
                    *s++ = '\0';
                }

                if (AddSlash)
                    sprintf(buffer+len, "  %s <A HREF=\"ftp://%s%s/%s\">", p, host, path, r);
                else
                    sprintf(buffer+len, "  %s <A HREF=\"ftp://%s%s%s\">", p, host, path, r);

                len += strlen(buffer+len);

                if (*p == 'd')
                    sprintf(buffer+len, "%s/</A>\n", r);
                else if (*p == 'l')
                    sprintf(buffer+len, "%s@</A>\n", s); 
                else if (p[3] == 'x')
                    sprintf(buffer+len, "%s*</A>\n", r);
                else
                    sprintf(buffer+len, "%s</A>\n", r);

                len += strlen(buffer+len);
            }
            else
            {
                if (AddSlash)
                    sprintf(buffer+len, "  <A HREF=\"ftp://%s%s/%s\">%s</A>\n", host, path, p, p);
                else
                    sprintf(buffer+len, "  <A HREF=\"ftp://%s%s%s\">%s</A>\n", host, path, p, p);

                len += strlen(buffer+len);
            }
        }
    }

    /* left is non-zero only if a screw up occurred! */

    if (left > 0)
    {
        recvbuf[left] = '\n';
        n = left + 1;
        memcpy(buffer+len, recvbuf, n);
        len += n;
    }

    strcpy(buffer+len, "</XMP>\n");
    NewDoc.length = len+7;
    return buffer;
}

#endif

/* return true if p starts with "NNN " where N is any digit */

#define IsDigit(c) ('0' <= c && c <= '9')

int StatusCode(char *p)
{
    if (!IsDigit(*p))
        return 0;

    ++p;

    if (!IsDigit(*p))
        return 0;

    ++p;


    if (!IsDigit(*p))
        return 0;

    if (*++p != ' ')
        return 0;

    return 1;
}


char *GetResponse(int socket, char *request)
{
    static char *p = NULL;
    char *s;
    int status, c, len;

    if (request)
    {
        if (debug)
            fprintf(stderr, "sending: %s", request);

        p = NULL;  /* discard previous unread message */
        status = XPSend(socket, request, strlen(request), 0);

        if (status == -1)
        {
            Warn("Couldn't send request: %s", request);
            close(socket);      /* close the socket */
            return NULL;
        }
    }

    /* check if there is a currently unread message in the buffer */

    if (p && *p)
        goto parse_message;

    status = XPRecv(socket, recvbuf, BUFSIZE-1);

    if (status < 5)
    {
        Warn("Couldn't get response to %s", request);
        close(socket);      /* close the socket */
        return NULL;
    }

    len = status;
    recvbuf[status] = '\0';

    if (debug)
        fprintf(stderr, "%s", recvbuf);

    p = recvbuf;

  parse_message:

    /* skip contined lines which usually look like 230-blah blah blah
       to find the real status code to return. Some servers screw up
       the protocol by sending time info in place of the normal NNN-
       at the start of the line! */

    for (;;)
    {
        s = p;  /* note start of this line */

        /* make sure we have received the entire line */

        while ((c = *p++) != '\n')
        {
            /* check if we haven't got all the message */

            if (c == '\0')
            {
                strcpy(recvbuf, s);
                len = p - s - 1;
                p = recvbuf+len;
                s = recvbuf;
                status = XPRecv(socket, recvbuf+len, BUFSIZE-1-len);

                if (status < 1)
                {
                    Warn("Couldn't get rest of message");
                    close(socket);
                    return NULL;
                }

                recvbuf[len+status] = '\0';

                if (debug)
                    fprintf(stderr, "%s", recvbuf+len);

                len += status;
            }
        }

        if (StatusCode(s))
            return s;

        /* else continue with next line */
    }
}

/* tidy way of closing FTP connection */

char *CloseFTP(void)
{
    char *s;

    if (FtpCtrlSkt != -1)
    {
        if (FtpDataSkt != -1)
            close(FtpDataSkt);

        s = GetResponse(FtpCtrlSkt, "QUIT\r\n");

        close(FtpCtrlSkt);
        FtpCtrlSkt = -1;
        FtpDataSkt = -1;

        if (s && debug)
            fprintf(stderr, "%s", s);

        return s;
    }

    return NULL;
}

char *GetFTPdocument(char *host, char *path, char *who)
{
    int vg, status, n, a1, a2, a3, a4, p1, p2;
    char *r, *sc, *buffer;

    signal(SIGPIPE, SIG_IGN); /* don't barf on stream errors */

    NewDoc.length = 0;

    if (FtpCtrlSkt != -1)   /* use currently open connection to server */
    {
        status = 1;

        if (strcmp(host, CurrentDoc.host) == 0)
            goto reopen_channel;

        /* else close socket and login to new host */

        status = 0;
        CloseFTP();
    }

  login:   /* come back here after timeout */

    /* create a socket */

    FtpCtrlSkt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (FtpCtrlSkt == -1)
    {
        ShowAbortButton(0);
        Warn("Couldn't create a socket for %s!", host);
        return NULL;
    }

    Announce("Connecting to %s", host);

    vg = 0;

    if (!Connect(FtpCtrlSkt, host, FTP_PORT, &vg))
    {
        if (!Authorize)
            ShowAbortButton(0);

        return NULL;
    }

    sc = GetResponse(FtpCtrlSkt, NULL);  /* get response to connecting */

    if (sc == NULL)
    {
        close(FtpCtrlSkt);
        FtpCtrlSkt = -1;
        ShowAbortButton(0);
        Warn("Failed to get connection message");
        return NULL;
    }

    Announce(sc);

    if (who)
        sprintf(ctrlbuf, "USER %s\r\n", UserName(who));
    else
        strcpy(ctrlbuf, "USER anonymous\r\n");

    sc = GetResponse(FtpCtrlSkt, ctrlbuf);

    if (sc == NULL)
    {
        close(FtpCtrlSkt);
        FtpCtrlSkt = -1;
        ShowAbortButton(0);
        Warn("Couldn't send USER command");
        return NULL;
    }

    /* is a name/password needed? */
    if (strncmp(sc, "530", 3) == 0)
    {
        Beep();
        close(FtpCtrlSkt);
        FtpCtrlSkt = -1;
        GetAuthorization(REMOTE, NewDoc.url);
        return NULL;
    }

    if (*sc == '1' || *sc == '4' || *sc == '5')
    {
        close(FtpCtrlSkt);
        FtpCtrlSkt = -1;
        ShowAbortButton(0);
        Warn("Couldn't login on: %s", sc);
        return NULL;
    }

    /* need to send password */

    if (*sc == '3')
    {
        Announce(sc);

        if (who)
            sprintf(ctrlbuf, "PASS %s\r\n", PassStr(who));
        else /* send email address as password: me@myhostname */
        {
            sprintf(ctrlbuf, "PASS ");
            r = user;     /* shell variable for user name */

            if (r == NULL)
                r = "www";

            strcat(ctrlbuf, r);
            strcat(ctrlbuf, "@");

            /* own host name for send part */

            if (gethostname(ctrlbuf+50, CTRLSIZE-51) != 0)
            {
                Warn("Couldn't find own hostname - errno %d", errno);
            }

            strcat(ctrlbuf, ctrlbuf+50);
            strcat(ctrlbuf, "\r\n");
        }

        sc = GetResponse(FtpCtrlSkt, ctrlbuf);

        if (sc == NULL)
        {
            ShowAbortButton(0);
            Warn("Couldn't send PASS command");
            close(FtpCtrlSkt);
            FtpCtrlSkt = -1;
            return NULL;
        }

        /* "530 Login incorrect" - a name/password needed */
        if (strncmp(sc, "530", 3) == 0)
        {
            Beep();
            close(FtpCtrlSkt);
            FtpCtrlSkt = -1;
            GetAuthorization(REMOTE, NewDoc.url);
            return NULL;
        }

        if (*sc != '2')
        {
            close(FtpCtrlSkt);
            FtpCtrlSkt = -1;
            ShowAbortButton(0);
            Warn(sc);
            return NULL;
        }

    }
    else if (*sc != '2')  /* guest login not permitted */
    {
        close(FtpCtrlSkt);
        FtpCtrlSkt = -1;
        ShowAbortButton(0);
        Warn(sc);
        return NULL;
    }

    Announce(sc);

    /* request server to enter binary (image) mode */

    sc = GetResponse(FtpCtrlSkt, "TYPE I\r\n");

    if (sc == NULL)
    {
        ShowAbortButton(0);
        Warn("Couldn't send TYPE command");
        close(FtpCtrlSkt);
        FtpCtrlSkt = -1;
        return NULL;
    }

    if (*sc != '2')
    {
        Warn("Couldn't enter binary (image) mode");
        CloseFTP();
        ShowAbortButton(0);
        Warn("Couldn't enter binary (image) mode: %s", sc);
        return NULL;;
    }

    Announce(sc);

  reopen_channel:

    /* request server to enter passive mode */

    sc = GetResponse(FtpCtrlSkt, "PASV\r\n");

    if (status == 0 && sc == NULL)
    {
        ShowAbortButton(0);
        Warn("Couldn't send PASV command");
        close(FtpCtrlSkt);
        FtpCtrlSkt = -1;
        return NULL;
    }

    if ((status && sc == NULL) || strncmp(sc, "421", 3) == 0)  /* timeout */
    {
        if (sc)
            Announce(sc);
        else
            Announce("reconnecting after timeout ...");

        close(FtpCtrlSkt);
        FtpCtrlSkt = -1;
        goto login;
    }

    if (*sc != '2')
    {
        CloseFTP();
        ShowAbortButton(0);
        Warn("Couldn't enter passive mode: %s", sc);
        return NULL;
    }

    Announce(sc);

    dataserver.sin_family = AF_INET;    /* or AF_DECnet */    

    r = sc;
    while (*r && *r != ',')
        ++r;

    while (--r > recvbuf && '0' <= *r && *r <= '9');

    sscanf(r+1, "%d,%d,%d,%d,%d,%d", &a1, &a2, &a3, &a4, &p1, &p2);

    dataserver.sin_port = p2 + (p1 << 8);
    dataserver.sin_addr.s_addr = a4 + (a3 << 8) + (a2 << 16) + (a1 << 24);

    /* create a socket */

    FtpDataSkt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    /* connect to server using the address and port it has just given us */

    if(!Connect(FtpDataSkt, inet_ntoa(dataserver.sin_addr), dataserver.sin_port, &vg))
    {
        close(FtpDataSkt);
        close(FtpCtrlSkt);
        FtpDataSkt = -1;
        FtpCtrlSkt = -1;

        if (Authorize)
            return 0;

        ShowAbortButton(0);
        Warn("FTP: couldn't connect to data port %d on %s",
                    dataserver.sin_port, inet_ntoa(dataserver.sin_addr));
        return NULL;
    }

    /* if root dir then skip CWD command */

    if (path[0] == '/' && path[1] == '\0')
        goto list_dir;

    /* if Windows NT for which path starts like "/d:/ftpsvc"
       then trim off leading "/" which would confuse host */

    if (isalpha(path[1]) && path[2] == ':')
        ++path;

    sprintf(ctrlbuf, "RETR %s\r\n", path);

    sc = GetResponse(FtpCtrlSkt, ctrlbuf);

    if (sc == NULL)
    {
        CloseFTP();
        ShowAbortButton(0);
        Warn("FTP: couldn't get %s", path);
        return NULL;
    }

    Announce(sc);

    if (*sc == '1')
        goto getfile;

    sscanf(sc, "%d", &status);

    if (status == 550)
    {
        sprintf(ctrlbuf, "CWD %s\r\n", path);
        sc = GetResponse(FtpCtrlSkt, ctrlbuf);

        if (*sc != '2')
        {
            if (strncmp(sc, "550", 3) == 0)
            {
                Warn(sc);
                CloseFTP();
                GetAuthorization(REMOTE, NewDoc.url);
                return NULL;
            }

            Warn(sc);
            CloseFTP();

            ShowAbortButton(0);
            return NULL;
        }

        Announce(sc);

    list_dir:

        pwd[0] = '\0';

        /* the initial dir may not be "/" e.g. with Windows NT "d:/ftpsvc"
           so do pwd to insert into dir listing */

        sprintf(ctrlbuf, "PWD\r\n");
        sc = GetResponse(FtpCtrlSkt, ctrlbuf);

        if (*sc != '2')
        {
            Warn(sc);
            CloseFTP();

            ShowAbortButton(0);
            return NULL;
        }

        Announce(sc);

        r = strchr(sc+5, '"');

        if (r)
        {
            *r = '\0';
            pwd[0] = '/';

            strncpy((sc[5] == '/' ? pwd : pwd+1), sc+5, PWDSIZE-1);
            pwd[PWDSIZE-1] = '\0';
        }

        sprintf(ctrlbuf, "LIST\r\n");
/*        sprintf(ctrlbuf, "NLST\r\n");  */

        n = strlen(ctrlbuf);
        Announce("sending LIST command ...");

        if (XPSend(FtpCtrlSkt, ctrlbuf, n, 1) == n)
        {
            sc = GetResponse(FtpCtrlSkt, NULL); /* try and get 150 Opening BINARY mode msg */

            buffer = GetDirList(FtpDataSkt);
            sc = GetResponse(FtpCtrlSkt, NULL);

            close(FtpDataSkt);
            FtpDataSkt = -1;

            if (sc == NULL)
            {
                close(FtpCtrlSkt);
                FtpCtrlSkt = -1;
            }

            sscanf(sc, "%d", &status);

            if (status == 150)  /* Opening BINARY mode data connection ... */
            {
                sc = GetResponse(FtpCtrlSkt, NULL);

                if (sc == NULL)
                {
                    close(FtpCtrlSkt);
                    FtpCtrlSkt = -1;
                }
            }

            ShowAbortButton(0);
            Announce(NewDoc.url);
            NewDoc.buffer = buffer;
            NewDocumentType();
            return NewDoc.buffer;
        }

        CloseFTP();

        if (debug)
            fprintf(stderr, "Couldn't retrieve: %s from %s\n", path, host);

        ctrlbuf[strlen(ctrlbuf)-2] = '\0';  /* zap \r\n at end */
        ShowAbortButton(0);
        Warn("Couldn't send: %s", ctrlbuf);
        return NULL;
    }

    if (status != 150 && *sc != '2')
    {
        CloseFTP();

        if (debug)
            fprintf(stderr, "Couldn't retrieve: %s from %s\n", path, host);

        ShowAbortButton(0);
        Warn(sc);
        return NULL;
    }

  getfile:

    buffer = GetFileData(FtpDataSkt);
    close(FtpDataSkt);
    FtpDataSkt = -1;

    sc = GetResponse(FtpCtrlSkt, NULL);

    if (sc == NULL)
    {
        close(FtpCtrlSkt);
        FtpCtrlSkt = -1;
    }

    ShowAbortButton(0);

  /* if data needs decompression, buffer will change address */

    NewDoc.buffer = buffer;
    NewDocumentType();

    Announce(NewDoc.url);

    return NewDoc.buffer;
}

