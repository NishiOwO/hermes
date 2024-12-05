/* simplified nntp client */

#include <X11/Xlib.h>
#include "www.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>

#define CTRLSIZE    8192
#define BUFSIZE     8192
#define THRESHOLD   1024

#define NNTP_PORT   119        /* IANA allocated port */

/* the NNTP commands as given by HELP command

    100 This server accepts the following commands:

    ARTICLE     BODY         GROUP
    HEAD        LAST         LIST
    NEXT        POST         QUIT
    STAT        NEWGROUPS    HELP
    IHAVE       NEWNEWS      SLAVE

    Additionally, the following extentions are supported:

    XHDR        Retrieve a single header line from a range of articles.
    XTHREAD     Retrieve trn thread file for the current group.
    XUSER       Log a clients username to nntp logfile.
    XINDEX      Retrieve a tin group index file.
    XMOTD       Retrieve the NNTP/news motd.
    XOVER       Return news overview data
*/

extern char *gateway;
extern int debug;
extern int Authorize;
extern Doc NewDoc;

char *XoverView;        /* XOVER list for current news group */
char *CurrentGroup;     /* the current news group */

static struct sockaddr_in server;       /* address info */
static struct sockaddr_in dataserver;   /* address info */
static struct hostent *hp;              /* other host info */

int GatewayPort = 2785;

/* keep track of current values for expanding abbreviated references */

int GetNewsResponse(int socket, char *request, char *buffer, int len)
{
    char *p;
    int status;

    if (request)
    {
        if (debug)
            fprintf(stderr, "sending: %s", request);

        status = XPSend(socket, request, strlen(request), 1);

        if (status == -1)
        {
            Warn("Couldn't send request: %s", request);
            close(socket);      /* close the socket */
            return -1;
        }
    }

    status = XPRecv(socket, buffer, len);

    if (status == -1)
    {
        Warn("Couldn't get response to %s", request);
        close(socket);      /* close the socket */
        return -1;
    }

    buffer[status] = '\0';

    if (debug)
        fprintf(stderr, "%s", TextLine(buffer));

    return *buffer - '0';
}

/* read the overview data for the current group,
   closes socket and issues warning message on problems */

char *GetXoverData(int skt, char *group)
{
    int size, len, count, eol;
    char *p, *q, *buffer;

    /* purge buffer as needed */

    if (XoverView)
    {
        free(XoverView);
        free(group);
    }

    size = 4096;
    len = 0;
    buffer = malloc(size);

    if (buffer == NULL)
    {
        close(skt);      /* close the socket */
        ShowAbortButton(0);
        Warn("Couldn't alloc buffer sized %d", size);
        return NULL;
    }

    sprintf(buffer, "GROUP %s\r\n", group);
    count = XPSend(skt, buffer, strlen(buffer), 1);

    if (count == -1)
    {
        close(skt);
        ShowAbortButton(0);
        Warn("Can't list group: %s", group);
        free(buffer);
        return NULL;
    }

    count = XPRecv(skt, buffer, CTRLSIZE-1);

    if (count == -1)
    {
        close(skt);
        ShowAbortButton(0);
        Warn("Can't list group: %s", group);
        free(buffer);
        return NULL;
    }

    buffer[count] = '\0';

    if (*buffer != '2')
    {
        close(skt);
        ShowAbortButton(0);
        Warn(buffer);
        free(buffer);
        return NULL;
    }

    Announce(buffer);

    len = XPSend(skt, "XOVER\r\n", 7, 1);

    if (len == -1)
    {
        close(skt);      /* close the socket */
        ShowAbortButton(0);
        Warn("Couldn't send XOVER command");
        free(buffer);
        return NULL;
    }

    len = XPRecv(skt, buffer, size-1);

    if (len < 1)
    {
        close(skt);      /* close the socket */
        ShowAbortButton(0);
        Warn("Couldn't get response to XOVER command");
        free(buffer);
        return NULL;
    }

    buffer[len] = '\0';

    if (*buffer != '2')
    {
        close(skt);      /* close the socket */
        ShowAbortButton(0);
        Warn(buffer);
        free(buffer);
        return NULL;
    }

    /* search for pattern: "\n." */

    p = buffer;
    eol = 0;

    for(;;)
    {
        /* do we need to read some more ? */

        if (*p == '\0')
        {
            /* do we need to expand buffer? */

            if (size - len < 513)  /* need to grow buffer */
            {
                count = p - buffer;
                size *= 2;  /* attempt to double size */
                q = realloc(buffer, size);

                if (q == NULL)
                {
                    buffer[len] =  '\0';
                    close(skt);      /* close the socket */
                    ShowAbortButton(0);
                    Warn("Couldn't realloc buffer size to %d", size);
                    NewDoc.length = len;
                    NewDoc.buffer = buffer;
                    return buffer;
                }

                buffer = q;
                p = buffer + count;
            }

            /* now read some more data */

            count = XPRecv(skt, buffer+len, size-len-1);

            if (count < 1)
            {
                close(skt);      /* close the socket */
                ShowAbortButton(0);
                Warn("Couldn't get data for XOVER command");
                free(buffer);
                return NULL;
            }

            len += count;
            buffer[len] = '\0';
        }

        if (eol && *p == '.')
            break;

        eol = (*p++ == '\n' ? 1 : 0);
    }

    XoverView = buffer;
    CurrentGroup = strdup(group);

    return buffer;
}

char *GetNewsDocument(char *host, char *path)
{
    int s, c, count, n, vg, len, first, last, line;
    long size;
    char *p, *q, *t, *r, *from, *date, *subject, *groups,
         *messageId, *refs, *buffer, recvbuf[CTRLSIZE];

    signal(SIGPIPE, SIG_IGN); /* don't barf on stream errors */

    /* if UDI was news://host/ref, then trim leading '/' from path */

    if (*path == '/')
        ++path;

    NewDoc.type = HTMLDOCUMENT;
    NewDoc.length = 0;

    /* create a socket */

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (s == -1)
    {
        ShowAbortButton(0);
        Warn("Can't create socket for %s", host);
        return NULL;
    }

    /* request a connection */

    Announce("Connecting to %s", host);

    vg = 0;

    if (!Connect(s, host, NNTP_PORT, &vg))
    {
        if (!Authorize)
            ShowAbortButton(0);

        return NULL;
    }

    size = BUFSIZE;
    len = 0;
    buffer = malloc(size);

    if (buffer == 0)
    {
        close(s);
        ShowAbortButton(0);
        Warn("Can't allocate read buffer for nntp");
        free(buffer);
        return NULL;
    }

    n = GetNewsResponse(s, NULL, buffer, size);  /* get response to connecting */

    if (n == -1)
    {
        ShowAbortButton(0);
        Warn("Can't connect to news server %s", host);
        free(buffer);
        return NULL;
    }

    if (n != 2)
    {
        close(s);
        ShowAbortButton(0);
        Announce("");
        NewDoc.length = strlen(buffer);
        NewDoc.buffer = buffer;
        return buffer;
    }

    Announce(buffer);  /* show signon */

    /* if path has form 3456@host or <3456@host> it denotes an article */

    p = strchr(path, '@');

    if (p)
    {
        NewDoc.type = HTMLDOCUMENT;

        if (*path == '<')
            sprintf(recvbuf, "ARTICLE %s\r\n", path);
        else
            sprintf(recvbuf, "ARTICLE <%s>\r\n", path);

        n = XPSend(s, recvbuf, strlen(recvbuf), 1);

        if (n == -1)
        {
            close(s);
            ShowAbortButton(0);
            Warn("Couldn't retrieve article <%s>", path);
            free(buffer);
            return NULL;
        }

        count = XPRecv(s, recvbuf, CTRLSIZE-1);

        if (count == -1)
        {
            close(s);
            ShowAbortButton(0);
            Warn("can't retrieve article <%s>", path);
            free(buffer);
            return NULL;
        }

        recvbuf[count] = '\0';

        if (*recvbuf != '2')
        {
            close(s);
            sprintf(buffer, "<PRE>\n");
            q = buffer+6;

            for (p = recvbuf; *p;)
            {
                if (*p == '<')
                {
                    *q++ = '&';
                    *q++ = 'l';
                    *q++ = 't';
                    *q++ = ';';
                }
                else if (*p == '>')
                {
                    *q++ = '&';
                    *q++ = 'g';
                    *q++ = 't';
                    *q++ = ';';
                }
                else if (*p == '&')
                {
                    *q++ = '&';
                    *q++ = 'a';
                    *q++ = 'm';
                    *q++ = 'p';
                    *q++ = ';';
                }
                else
                    *q++ = *p;

                ++p;
            }

            NewDoc.length = strlen(buffer);
            NewDoc.buffer = buffer;
            return buffer;
        }

        from = subject = date = refs = groups = "";
        p = recvbuf;

        for (;;)
        {
            if (p[0] == '\r' || p[0] == '\n')
                break;

            for (q = p; *q && *q != '\n'; ++q)
            {
                if (*q == '\r')
                    *q = '\0';
            }

            *q++ = '\0';

            if (strncmp(p, "From: ", 6) == 0)
                from = p+6;
            else if (strncmp(p, "Subject: ", 9) == 0)
                subject = p+9;
            else if (strncmp(p, "Newsgroups: ", 12) == 0)
                groups = p+12;
            else if (strncmp(p, "Date: ", 6) == 0)
            {
                date = p+6;
                r = strchr(date, ':');
              /*
                if (r)
                    *(r-3) = '\0';

                r = p+6;

                if (*r == ' ')
                    ++r;

                if (*r > '9')
                    while (!('0' <= *r && *r <= '9'))
                        ++r;

                date = r;

                if (date[1] == ' ')
                {
                    --date;
                    *date = ' ';
                }

                r = strrchr(date, ' ');

                if (r)
                {
                    ++r;
                    r[0] = r[2];
                    r[1] = r[3];
                    r[2] = '\0';
                }  */
            }
            else if (strncmp(p, "Message-ID: ", 12) == 0)
            {
                messageId = p+12;

                if (*messageId == '<')
                    ++messageId;

                r = strchr(messageId, '>');

                if (r)
                    *r = '\0';
            }
            else if (strncmp(p, "References: ", 12) == 0)
                refs = p+12;

            p = q;
        }

        sprintf(buffer, "<TITLE>%s</TITLE>\n<H1>%s</H1>\n<PRE>\n", subject, subject);
        len = strlen(buffer);

        sprintf(buffer+len, "Date:       %s\nFrom:       %s\nMessageId:  <%s>\n", date, from, messageId);
        len += strlen(buffer+len);

        /* translate news groups into buttons */

        n = 0; /* reference number */

        for (;;)
        {
            while (*groups == ' ')
                ++groups;

            r = groups;

            while (*r && *r != ',')
                ++r;

            /* set q to next group */

            if (*r == ',')  /* another group follows */
            {
                *r = '\0';
                q = r+1;
            }
            else            /* *r == '\0' and no more groups */
                q = r;

            if (++n == 1)
            {
                sprintf(buffer+len, "Newsgroups: ");
                len += 12;
            }
            else if (n%3 == 1)
            {
              sprintf(buffer+len, "\n            ");
              len += 13;
            }

            sprintf(buffer+len, "<A HREF=\"news:%s\">%s</A> ", groups, groups);
            len += strlen(buffer+len);

            if (*q == '\0')
                break;

            groups = q;
        }

        if (n > 0)
            buffer[len++] = '\n';

        /* translate references into buttons */

        n = 0; /* reference number */

        for (;;)
        {
            t = strchr(refs, '<');
            q = strchr(refs, '>');
            r = strchr(refs, '@');

            if ( !(p && q && r) )
                break;

            refs = q+1;
            ++t;
            *q = '\0';

            if (++n == 1)
            {
                sprintf(buffer+len, "References: ");
                len += 12;
            }
            else if (n%3 == 1)
            {
              sprintf(buffer+len, "\n            ");
              len += 13;
            }

            sprintf(buffer+len, "<A HREF=\"news:%s\">%s</A> ", t, r+1);
            len += strlen(buffer+len);
        }

        if (n > 0)
            buffer[len++] = '\n';

        buffer[len++] = '\n';

        for(;;)
        {
            /* come here at beginning of line */

            if (p[0] == '.')
            {
                if (p[1] == '\r' || p[1] == '\n')
                    break;

                if (p[1] == '.')
                    ++p;
            }

            /* set q to end of line or end of data */

            for (q = p; *q != '\n'; ++q)
            {
                if (*q == '\0')  /* need to read some more */
                {
                    /* first shift first part of line to start of buffer */
                    n = q - p;
                    memcpy(recvbuf, p, n);

                    /* and read the next part of the line */
                    count = XPRecv(s, recvbuf+n, CTRLSIZE-n-1);

                    if (count < 1)
                    {
                        buffer[len] = '\0';
                        NewDoc.length = len;
                        NewDoc.buffer = buffer;
                        close(s);
                        ShowAbortButton(0);
                        Announce(NewDoc.url);
                        return buffer;
                    }

                    recvbuf[n+count] = '\0';
                    p = recvbuf;
                    q = p + n - 1; /* -1 compensates for q++ in loop */
                }
            }

            if (size - len < THRESHOLD)  /* need to grow buffer */
            {
                size *= 2;  /* attempt to double size */
                r = realloc(buffer, size);

                if (r == NULL)
                {
                    buffer[len] =  '\0';
                    close(s);      /* close the socket */
                    ShowAbortButton(0);
                    Warn("Couldn't realloc buffer to size %ld", size);
                    NewDoc.length = len;
                    NewDoc.buffer = buffer;
                    return buffer;
                }

                buffer = r;
            }

            n = q - p;

            if (p[n-1] == '\r')
                --n;

            while (n > 0)
            {
#if 0  /* this code replaces message ids in message body with hypertext links */
                if (*p == '<')
                {
                    t = "";
                    r = p+1;

                    while (*r && *r != '>')
                    {
                        if (*r == '@')
                            t = r;

                        ++r;
                    }

                    if (*r == '>' && *t == '@')
                    {
                        *r++ = '\0';
                        sprintf(buffer+len, "<A HREF=\"news:%s\">%s</A> ", ++p, ++t);
                        len += strlen(buffer+len);
                        p = r;
                        n -= 1+r-p;
                        continue;
                    }
                }
#endif
                buffer[len++] = *p++;
                --n;
            }

            buffer[len++] = '\n';

            p = ++q;
        }

        close(s);
        buffer[len] = '\0';
        NewDoc.length = len;
        NewDoc.buffer = buffer;
        ShowAbortButton(0);
        Announce(NewDoc.url);
        return buffer;
    }

    /* if the path has the form "alt.*" it denotes a list of group names */

    n = strlen(path);

    if (path[n-1] == '*')
    {
        if (*path == '*')
            strcpy(recvbuf, "LIST\r\n");
        else
        {
            if (path[n - 2] == '.')
                --n;

            c = path[n];
            path[n-1] = '\0';

            /* use NEWGROUPS for Jan 1st 1970 with path as distribution */

            sprintf(recvbuf, "NEWGROUPS 700101 0000 <%s>\r\n", path);

            path[n] = c;
        }

        count = XPSend(s, recvbuf, strlen(recvbuf), 1);

        if (count == -1)
        {
            close(s);
            ShowAbortButton(0);
            Warn("lost connection with news server %s", host);
            free(buffer);
            return NULL;
        }

        sprintf(buffer, "<TITLE>News Groups</TITLE>\n<H1>News Groups</H1>\nNote the number of of news articles in each group is an upper bound\n");

        count = XPRecv(s, recvbuf, CTRLSIZE -1);

        if (count > 0 && *recvbuf == '2')
        {
            strcat(buffer, "<UL>\n");
            len = strlen(buffer);
            recvbuf[count] = '\0';

            line = 0;
            p = recvbuf;

            for (;;)
            {
                if (p[0] == '.' && (p[1] == '\r' || p[1] == '\n'))
                    break;

                if (p[0] == '\r' || p[1] == '\n')
                    break;

                /* ensure we have the entire line in the recvbuf */

                r = p;   /* p -> start of line */

                for(;;)
                {
                    c = *r;

                    if (c == '\0')  /* need to read some more */
                    {
                        /* copy existing part to start of buffer */

                        if (p > recvbuf)
                            strcpy(recvbuf, p);

                        n = r - p;  /* the number of chars in this line we currently hold */
                        p = recvbuf;
                        r = recvbuf+n;
                        count = XPRecv(s, recvbuf+n, CTRLSIZE - 1 - n);

                        if (count < 1)
                            break;

                        recvbuf[n+count] = '\0';
                        --r;
                    }

                    ++r;

                    if (c == '\n')
                        break;
                }

                /* r -> start of next line */

                if (line++ == 0)  /* skip first line with NNTP status msg */
                {
                    p = r;
                    continue;
                }

                if (size - len < THRESHOLD)  /* need to grow buffer */
                {
                    size *= 2;  /* attempt to double size */
                    q = realloc(buffer, size);

                    if (q == NULL)
                    {
                        buffer[len] =  '\0';
                        close(s);      /* close the socket */
                        NewDoc.length = len;
                        NewDoc.buffer = buffer;
                        ShowAbortButton(0);
                        Warn("Couldn't realloc buffer to size %d", size);
                        return buffer;
                    }

                    buffer = q;
                }

                for (q = p; *q != ' '; ++q);
                *q++ = '\0';
                sscanf(q, "%d %d", &last, &first);

                sprintf(buffer+len, " <LI><A HREF=\"news:%s\">%s</A> %d\n", p, p, last-first+1);
                len += strlen(buffer+len); /*+= strlen(buffer+len);*/

                p = r;
            }

            strcat(buffer+len, "</UL>\n");
            len += 6;
        }
        else
        {
            if (count > 0)
                strcat(buffer, recvbuf);
            else
                strcat(buffer, "couldn't list groups");

            len = strlen(buffer);
        }

        close(s);
        NewDoc.type = HTMLDOCUMENT;
        buffer[len] =  '\0';
        NewDoc.length = len;
        NewDoc.buffer = buffer;
        ShowAbortButton(0);
        Announce(NewDoc.url);
        return buffer;
    }

    /* other wise get list of articles in specified group */

    p = GetXoverData(s, path);

    if (!p)
        return NULL;   /* error message handled by GetXover() */

    sprintf(buffer, "<TITLE>news:%s</TITLE>\n<H1>%s</H1>\n<PRE>\n", path, path);
    len = strlen(buffer);

    /* skip to first line of data in overview */

    p = strchr(p, '\n');
    ++p;

    line = 0;
    NewDoc.type = HTMLDOCUMENT;

    for (;;)
    {
        if (p[0] == '.' && (p[1] == '\r' || p[1] == '\n'))
            break;

        subject = strchr(p, '\t');
        ++subject;

        from = strchr(subject, '\t');
        *from++ = '\0';

        date = strchr(from, '\t');
        *date++ = '\0';

        /* from like "dsr@hplb.hpl.com (Dave Raggett)" */

        q = strchr(from, '(');

        if (q)
        {
            from = q+1;
            q = strchr(from, ')');

            if (q)
                *q = '\0';
        }
        else /* from like "Dave Raggett <dsr@hplb.hpl.hp.com>" */
        {
            q = strchr(from, '<');

            if (q && q > from+1)
                *(q-1) = '\0';
        }

        messageId = strchr(date, '\t');
        *messageId++ = '\0';

        q = strchr(messageId, '\t');
        *q++ = '\0';

        /* and move p to start of next line */

        p = strchr(q, '\n');
        ++p;

        /* strip < brackets > from messageId */

        if (*messageId == '<')
        {
            ++messageId;

            if ((q = strchr(messageId, '>')))
                *q++ = '\0';
        }

        /* strip of time of day */

        r = strchr(date, ':');

        if (r)
            *(r-3) = '\0';

        r = date;

        /* adjust year */

        if (*r == ' ')
            ++r;

        if (*r > '9')
             while (!('0' <= *r && *r <= '9'))
                    ++r;

        date = r;

        if (date[1] == ' ')
        {
            --date;
            *date = ' ';
        }

        r = strrchr(date, ' ');

        if (r)
        {
            ++r;
            r[0] = r[2];
            r[1] = r[3];
            r[2] = '\0';
        }

        if (size - len < THRESHOLD)  /* need to grow buffer */
        {
            size *= 2;  /* attempt to double size */
            q = realloc(buffer, size);

            if (q == NULL)
            {
                buffer[len] =  '\0';
                close(s);      /* close the socket */
                ShowAbortButton(0);
                Announce("Couldn't realloc buffer size to %d", size);
                NewDoc.length = len;
                NewDoc.buffer = buffer;
                return buffer;
            }

            buffer = q;
        }

        sprintf(buffer+len, "%-10.10s<A HREF=\"news:%s\"> %-42.42s</A> %s\n", date, messageId, subject, from);
        len += strlen(buffer+len);
        ++line;
    }

    sprintf(buffer+len, "</PRE>\n");
    len += 7;

    send (s, "QUIT\r\n", 6, 0);
    close(s);
    NewDoc.length = len;
    NewDoc.buffer = buffer;
    ShowAbortButton(0);
    Announce(NewDoc.url);
    return buffer;
}

