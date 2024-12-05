/* tcp.c - this module provides a layer over the basic tcp connections
   with support for timeouts, user abort, and polling X events */

#include <X11/Xlib.h>
#include "www.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h> /* @@@ */

#define TEXTDOCUMENT    0
#define HTMLDOCUMENT    1

#define CTRLSIZE    1024
#define BUFSIZE     8192
#define THRESHOLD   1024

char *inet_ntoa();

extern int debug;
extern int AbortFlag;

extern int OpenSubnet;
extern char *gateway;
extern int gatewayport;
extern Doc NewDoc;

/**** globals for UDP ****/

#define ADDRNOTFOUND    0xffffffff  /* value returned for an unknown host */
#define RETRIES 5   /* number of times to retry before giving up */

#define RBUFSIZE 1024  /* max size of receive packets */

int cache_s = -1;      /* socket descriptor */

static int buflen;             /* number of bytes read */
static char buffer[RBUFSIZE];   /* receive buffer */

static struct sockaddr_in myaddr_in;     /* for local socket address */
static struct sockaddr_in servaddr_in;   /* server's socket address */

static int NoReader;       /* set by SIGPIPE */

/**** globals for TCP ****/

static struct sockaddr_in server;       /* address info */
static struct hostent *hp;              /* other host info */

int BrokenPipe;
char *gatewayUser;               /* username:password for gateway */

#define TIMEOUT 3000            /* 5 minutes */
#define MASK(f)     (1 << f)

/* pause for delay milliseconds */

void Pause(int delay)
{
    struct timeval timer;

    timer.tv_sec = 0;
    timer.tv_usec = delay * 1000;

    select(NFDBITS, 0, 0, 0, &timer);
}

static void handler()
{
    signal(SIGALRM, handler);
}

int QueryCacheServer(char *command, char **response)
{
    int retry;
    struct hostent *hp;  /* other host info */

    if (cache_s == -1)  /* need to initialise connection */
    {
        /* clear out address structures */
        memset((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
        memset((char *)&servaddr_in, 0, sizeof(struct sockaddr_in));

        /* set up server address */

        servaddr_in.sin_family = AF_INET;

        /* get the host info for server's hostname (global define) */
        hp = gethostbyname(CACHE_SERVER);

        if (hp == NULL)
        {
            Warn("%s Not found in /etc/hosts", CACHE_SERVER);
            return -1;
        }

        servaddr_in.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;

        /* would be better in long term to use getservbyname() */
        servaddr_in.sin_port = CACHE_PORT;

        /* create the socket */

        cache_s = socket(AF_INET, SOCK_DGRAM, 0);

        if (cache_s == -1)
        {
            Warn("Unable to create socket for cache server");
            return -1;
        }

        /* Bind socket to some local address so that the server can
           send the reply back. A port number of zero will be used
           so that the system will assign any available port number.
           An address of INADDR_ANY will be used so that we won't
           have to look up the Internet address of the local host */

        myaddr_in.sin_family = AF_INET;
        myaddr_in.sin_port = 0;
        myaddr_in.sin_addr.s_addr = INADDR_ANY;

        if (bind(cache_s, &myaddr_in, sizeof(struct sockaddr_in)) == -1)
        {
            Warn("Unable to bind socket for cache server");
            return -1;
        }

        /* setup alarm signal handler */

        signal(SIGALRM, handler);
    }

    /* send the request to the server */

    retry = RETRIES;

  again:

    if (sendto(cache_s, command, strlen(command), 0,
                &servaddr_in, sizeof(struct sockaddr_in)) == -1)
    {
        Warn("Unable to send request to cache server");
        return -1;
    }


    /* Set up a timeout so that client doesn't hang in case the
       packet gets lost. After all, UDP does not guarantee delivery */

    alarm(1);

    if ((buflen = recv(cache_s, buffer, RBUFSIZE-1, 0)) == -1)
    {
        if (errno == EINTR) /* alarm went off and aborted receive */
        {
            if (retry--)
            {
                PollEvents(0);  /* poll X events to refresh screen etc */
                goto again;
            }

            Warn("Unable to get response from %s : %d after %d attempts.",
                    CACHE_SERVER, CACHE_PORT, RETRIES);

            return -1;
        }

        Warn("Unable to receive response from cache server");
        return -1;
    }

    alarm(0);
    buffer[buflen] = '\0';
    printf("Response: %s\n", buffer);
    *response = buffer;
    return buflen;
}

/* Connect, Send and Recv routines that poll X events */

int XPConnect(int skt, struct sockaddr *server)
{
    struct timeval timer;
    int n, ntime, writefds, exceptfds;

    AbortFlag = 0;

    /* set socket to non-blocking i/o */
    n = 1;
    ioctl(skt, FIONBIO, &n);

    n = connect(skt, server, sizeof(struct sockaddr_in));

    if (n == -1 && errno != EINPROGRESS)
         return -1;

    for (ntime = 0; ntime < TIMEOUT && !AbortFlag;)
    {
        errno = 0;
        writefds = exceptfds = MASK(skt);
        timer.tv_sec = 0;
        timer.tv_usec = 100000;

        n = select(NFDBITS, 0, &writefds, &exceptfds, &timer);

        if (n == 0)  /* 100 mS time out */
        {
            ++ntime;
            PollEvents(0);
            continue;
        }

        if (exceptfds)
        {
            perror("XPConnect - aborted via signal");
            close(skt);
            return -1;
        }

        /* writefds implies the connect activity has now completed
           it may have failed, so call connect again to check
           (EISCONN implies socket is already connected, ie success) */

        if (writefds)
        {
            n = connect(skt, server, sizeof(struct sockaddr_in));

            if (n == -1 && errno != EISCONN)
            {
                /* EINVAL implies the socket was already shutdown
                   but results in the confusing msg: Invalid argument */

                if (errno == EINVAL)
                    errno = ECONNREFUSED;

                close(skt);
                return -1;
            }


            return 0;
        }
    }

    close(skt);
    Warn("aborted/timed-out during connect");
    errno = 0;
    return -1;
}

/* note when an attempt has been made to write
   to a socket for which there is no reader */

void sigpipe(int sig)
{
    BrokenPipe = 1;
}

int XPSend(int skt, char *data, int len, int once)
{
    struct timeval timer;
    int k, ntime, n, writefds, exceptfds;

    AbortFlag = 0;
    k = 0;

    /* trap writes to pipe with no one to read it */

    BrokenPipe = 0;
    signal(SIGPIPE, sigpipe);

    for(ntime = 0; ntime < TIMEOUT && !AbortFlag;)
    {
        if (k == len)
            return len;

        writefds = exceptfds = MASK(skt);
        timer.tv_sec = 0;
        timer.tv_usec = 100000;

        n = select(NFDBITS, 0, &writefds, &exceptfds, &timer);

        if (n == 0)  /* 100 mS time out */
        {
            ++ntime;
            PollEvents(0);
            continue;
        }

        if (exceptfds)
        {
            close(skt);
            return -1;
        }

        if (writefds)
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

    Warn("Aborted/timed-out during send");
    close(skt);
    errno = 0;
    return -1;
}

/* similar receive operation where len is the maximum amount
   of data to receive, the amount received is generally less */

int XPRecv(int skt, char *msg, int len)
{
    int n, ntime, readfds, exceptfds;
    struct timeval timer;

    /* I maybe should call recv repeatedly until I have got
       all of the message as indicated by the message contents */

    AbortFlag = 0;
    errno = 0;

    for (ntime = 0; ntime < TIMEOUT && !AbortFlag;)
    {
        readfds = exceptfds = MASK(skt);
        timer.tv_sec = 0;
        timer.tv_usec = 10000;

        n = select(NFDBITS, &readfds, 0, &exceptfds, &timer);

        if (n == 0)
        {
            ++ntime;
            PollEvents(0);
            continue;
        }
    
        if (exceptfds & MASK(skt))
        {
            perror("XPRecv - aborted via signal");
            Warn("Aborted during receive");
            close(skt);
            return -1;
        }

        if (readfds)
            return recv(skt, msg, len, 0);
    }

    fprintf(stderr, "aborted/timed-out during receive");

    if (AbortFlag)
        Warn("Aborted during receive");
    else
        Warn("Timed-out during receive");

    close(skt);
    errno = 0;
    return -1;
}



/* this code relies on C initialising static memory to zeroes
   as you can't otherwise initialise fields in a static structure */

int Connect(int s, char *host, int port, int *ViaGateway)
{
    int n;
    char buf[80];
    static struct in_addr gateway_address;

    server.sin_family = AF_INET;

    if (OpenSubnet)
    {
        hp = gethostbyname(host);
        *ViaGateway = 0;
    }
    else if (not_hp_domain(host))
        *ViaGateway = 1;
    else if (!*ViaGateway)
        hp = gethostbyname(host);

    if (*ViaGateway || hp == NULL)
    {
        *ViaGateway = 1;

        server.sin_port = gatewayport;

        if (gateway_address.s_addr == NULL)
        {
            hp = gethostbyname(gateway);

            if (hp == NULL)
            {
                fprintf(stderr, "unknown gateway: %s\n", gateway);
                Warn("Unknown gateway: %s", gateway);
                return 0;
            }

            gateway_address.s_addr = ((struct in_addr *) (hp->h_addr))->s_addr;
        }

        server.sin_addr.s_addr = gateway_address.s_addr;

        n =  XPConnect(s, (struct sockaddr *)&server);

        if (n == -1)
        {
            Warn("Can't connect to gateway at %s on port %d - errno %d", gateway, gatewayport, errno);
            return 0;
        }

        if (gatewayUser)
            sprintf(buf, "%s:%d %s\r\n", host, port, gatewayUser);
        else
            sprintf(buf, "%s:%d\r\n", host, port);

        n = XPSend(s, buf, strlen(buf), 0);

        if (n == -1)
        {
            Warn("Can't send host/port to gateway - errno %d", errno);
            return 0;
        }

        n = XPRecv(s, buf, 15);  /* kludge - just ask for the "Connected!\r\n" msg */

        if (n <= 0)
        {
            close(s);
            Warn("Can't connect to %s on port %d - errno %d", host, port, errno);
            return 0;
        }

        buf[n] = '\0';

        if (strncmp(buf, "Unauthorized!\r\n", 15) == 0)
        {
            Warn("Your are unauthorised for using %s", host);
            GetAuthorization(GATEWAY, gateway);
            return 0;
        }

        if (strncmp(buf, "Unknown!\r\n", 10) == 0)
        {
            Warn("Unknown hostname %s", host);
            return 0;
        }

        if (strncmp(buf, "Connected!\r\n", 12) != 0)
        {
            Warn("Can't connect to %s on port %d!", host, port);
            return 0;
        }

        Announce("Connected to %s:%d", host, port);
        return 1;
    }
    else
    {
        server.sin_port = port;
        server.sin_addr.s_addr = ((struct in_addr *) (hp->h_addr))->s_addr;

        n = XPConnect(s, (struct sockaddr *)&server);

        if (n == -1)
        {
            Warn("Can't connect to %s:%d - errno %d", host, port, errno);
            return 0;
        }

        Announce("Connected to %s:%d", host, port);
        return 1;
    }
}

char *GetData(int socket, int *length)
{
    char *p, *buffer;
    int count, m, len, size, nfound, readfds, exceptfds;
    char *host, *path;

    host = NewDoc.host;
    path = NewDoc.path;

    *length = len = 0;
    size = BUFSIZE;
    buffer = malloc(size);

    if (buffer == 0)
    {
        Warn("Couldn't alloc buffer size %d", size);
        return NULL;
    }

    for (m = 0;;)
    {
        count = XPRecv(socket, buffer+len, size - len);

        if (count == -1)
        {
            if (errno)
                 Warn("Couldn't get data: errno %d", errno);

            free(buffer);
            return NULL;
        }

        /* zero count indicates the circuit has closed
           due to end of data, timeout or a premature
           close by the gateway or remote host */

        if (count == 0)
        {
            buffer[len] = '\0';

            *length = len;
            return buffer;
        }

        m += count;
        len += count;
        Announce("received %d bytes", m);

        if (size - len < THRESHOLD)  /* need to grow buffer */
        {
            size *= 2;  /* attempt to double size */
            p = realloc(buffer, size);

            if (p == NULL)
            {
                buffer[len] =  '\0';
                NewDoc.length = len;
                Warn("Couldn't realloc buffer to size %d", size);
                return buffer;
            }


            buffer = p;
        }
    }
}

