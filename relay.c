/****************************************************************************** 
    TCP/IP Relay for HP access to the World Wide Web, FTP, Gopher etc.

    This relays tcp connections transparently so that clients can
    connect to servers outside the HP closed subnet. This program
    is invoked by inetd and subject to the inetd.sec security
    screen.

    /etc/services needs the following entry:

        wrelay    2785/tcp              # closed subnet tcp relay

    /etc/inetd.conf needs the following entry:

        wrelay    stream tcp  nowait  root  /etc/wrelayd  wrelayd

    /usr/adm/inetd.sec needs the following entry:

        wrelay    allow HP-Internet   # tcp relay for HP clients only
        

    inetd thus allows connections to be made by HP clients (net 15) only.
    Connections can't be made from external hosts to HP machines.

    This program makes further security checks:

        a) the client is an HPLB machine

                (client address & 255.255.248.0) == 15.8.56.0
    Or

        b) the client supplies a valid user name and password
           for an account on this machine, i.e.hplose.hpl.hp.com

    This program is invoked by inetd with the connection to
    the client on file descriptor zero.

    Connections are requested on port 2785 with the command:

        COMMAND ::= HOST\r\n | HOST USER\r\n
        HOST    ::= Address:Port | Hostname:Port
        USER    ::= Username:Password
   e.g.     
        "15.8.59.2:21\r\n"
        "15.8.59.2:21 fred:secret\r\n"
        "info.cern.ch:80\r\n"
        "info.cern.ch:80 fred:secret\r\n"

    The user name and password should refer to a valid account on
    the machine running this program e.g. hplose.hpl.hp.com.
    If the security check fails the relay sends the following
    error message back to the client and closes the connection:

        o   "Unauthorized!\r\n"

    Otherwise it looks up the hostname and connects to the
    specified host address and port number. It then writes
    an acknowledgement back to the client:

        o   "Connected!\r\n"     - if successful
        o   "Unknown!\r\n"       - if unknown hostname

    If a 5 minute timeout occurs before the connection is
    established then the connection to the client is simply
    closed without an acknowledgement. The client then
    sees a recv() with zero length.

    Once the acknowledgement is received, the client treats
    the connection as if it were directly connected to the
    remote host. The host sees the relay machine as its
    client, which may cause problems in some cases when the host
    uses this address as part of an authentication procedure.

    Connections to clients and servers are closed if no
    activity occurs within a specified interval (5 minutes).
    This means that FTP and TELNET connections which are kept
    open for extended periods will be closed down.

    Dave Raggett, HP Labs, Bristol UK.

    Copyright Hewlett Packard 1993, All rights reserved.
*******************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <pwd.h>
#include <signal.h>
#include <time.h>

#define PORT            2785                /* port for tcp relay service */

#define ADDRESS_MASK    "255.255.248.0"     /* ANDed with client address */
#define ADDRESS_BASE    "15.8.56.0"         /* which should yield this */

#define TIMEOUT         60                  /* 60 * 5 seconds = 5 minutes */

/* status codes for sockets */

#define UNUSED     -1
#define ADDRESS     1
#define UNKNOWN     2
#define CLIENT      3
#define CONNECTING  4
#define SERVER      5

struct sockaddr_in client;   /* address info */
struct sockaddr_in server;   /* address info */
struct hostent *hp;          /* pointer to host info for remote host */

int BrokenPipe = 0;

int client_socket;
int server_socket;

int client_status;      /* ADDRESS, UNKNOWN or CLIENT */
int server_status;      /* UNUSED, CONNECTING or SERVER */

int client_pending;     /* how many bytes to write to client */
int server_pending;     /* how many bytes to write to server */

int client_timer;       /* counts up from 0 to TIMEOUT */
int server_timer;

#define BUFSIZE       4096
char client_buf[BUFSIZE+1];     /* buffers data being relayed */ 
char server_buf[BUFSIZE+1];     /* valid data given by client_pending etc. */



/* convert "15.8.61.39" etc to 32 bit number */

u_long inet_aton(char *name)
{
    int a1, a2, a3, a4;

    sscanf(name, "%d.%d.%d.%d", &a1, &a2, &a3, &a4);
    return a4 + (a3 << 8) + (a2 << 16) + (a1 << 24);
}

int GoodAddress(int socket)
{
    int len;
    static u_long base, mask;

    /* initialise address masks */

    if (!base)
    {
        base = inet_aton(ADDRESS_BASE);
        mask = inet_aton(ADDRESS_MASK);
    }

    /* fetch client's internet address */

    len = sizeof(struct sockaddr_in);

    if (getpeername(socket, &client, &len) < 0)
        exit(1);

    /* and perform test with masks */

    if ((client.sin_addr.s_addr & mask) != base)
        return 0;

    return 1;
}

/*
   Read username and password if present and
   check if matches /etc/password

   this will fail if /.secure/etc/passwd is
   in use and this process doesn't have read access

   buf is in form "info.cern.ch:80 dsr:secret\r\n"
   this routine corrupts the buffer contents
*/

int KnownUser(int skt, char *buf)
{
    int n;
    char *user, *pass, salt[4];
    struct passwd *pw;

    /* find preceding ' ' char */

    user = strchr(buf, ' ');

    /* if no user name then treat as unauthorised */

    if (!user)
        return 0;

    ++user;

    /* zero out trailing '\r' */

    pass = strchr(user, '\r');

    if (!pass)
        return 0;

    *pass = '\0';

    /* find preceding ':' char */

    pass = strchr(user, ':');

    if (!pass)
        return 0;

    /* zero it out and adjust position */

    *pass++ = '\0';

    /* now check with /etc/passwd */

    if ((pw = getpwnam((const char *)user)) == NULL)
        return 0;

    n = strlen(pw->pw_passwd);

    if (n == 0)     /* no password */
        return 1;

    if (strlen(pw->pw_passwd) != 13)
        return 0;

    strncpy(salt, pw->pw_passwd, 2);

    if (!strcmp(pw->pw_passwd, crypt(pass, salt)))
        return 1;

    return 0;
}

/* note when an attempt has been made to write
   to a socket for which there is no reader */

void sigpipe(int sig)
{
    BrokenPipe = 1;
}

/* prepare for select call - taking care to wait only on sockets that 
   aren't blocked in other ways, e.g. with no data to write */

void init_select(fd_set *readfds, fd_set *writefds, fd_set *exceptfds)
{
    FD_ZERO(readfds);
    FD_ZERO(writefds);
    FD_ZERO(exceptfds);

    FD_SET(client_socket, exceptfds);

    if (client_status == CLIENT)
    {
        /* block reads while we have data to write to server */

        if (server_pending == 0)
            FD_SET(client_socket, readfds);

        /* block writes until there is data to write to server */

        if (client_pending > 0)
            FD_SET(client_socket, writefds);
    }
    else
    {
        FD_SET(client_socket, readfds);
        FD_SET(client_socket, writefds);
    }

    if (server_status == SERVER)
    {
        FD_SET(server_socket, exceptfds);

        /* block reads while we have data to write to client */

        if (client_pending == 0)
            FD_SET(server_socket, readfds);

        /* block writes until there is data to write to client */

        if (server_pending > 0)
            FD_SET(server_socket, writefds);
    }
    else if (server_status != UNUSED)
    {
        FD_SET(server_socket, exceptfds);
        FD_SET(server_socket, readfds);
        FD_SET(server_socket, writefds);
    }
}

main()
{
    struct timeval timeout;
    fd_set readfds, writefds, exceptfds;
    int i, len, a1, a2, a3, a4, port;
    char *p;

    /* trap writes to pipe with no one to read it */

    BrokenPipe = 0;
    signal(SIGPIPE, sigpipe);

    client_status = UNUSED;
    server_status = UNUSED;

    client_pending = 0;
    server_pending = 0;

    client_socket = 0;      /* inetd passes connection to client on socket 0 */
    server_socket = UNUSED;

    client_timer = TIMEOUT; /* time out quickly if client doesn't send host details */
    server_timer = 0;

    timeout.tv_sec = 5;     /* giving a 5 second timer resolution */
    timeout.tv_usec = 0;

    /* set client socket to be non-blocking */

    i = 1;
    ioctl(client_socket, FIOSNBIO, &i);

    client_status = ADDRESS;  /* prepare to read server address/port */

    /* enter indefinite loop waiting for requests from clients */

    for (;;)
    {
        init_select(&readfds, &writefds, &exceptfds);

        if ((i = select(NFDBITS, (int*)(&readfds), (int*)(&writefds), (int*)(&exceptfds), &timeout)) == -1)
            break;
        else if (i == 0)  /* 5 second timing check */
        {
            if (++client_timer >= TIMEOUT)
                exit(1);

            if (server_status == UNUSED)
                continue;

            if (++server_timer >= TIMEOUT)
                exit(1);

            continue;
        }
    
        if (FD_ISSET(client_socket, &exceptfds))
            exit(1);

        if (server_status != UNUSED && FD_ISSET(server_socket, &exceptfds))
            exit(1);

        /* client has sent address info or a data packet ? */

        if (FD_ISSET(client_socket, &readfds))
        {
            client_timer = 0;

            if (client_status == ADDRESS)  /* read server's address/port */
            {
                /* client is sending us address/port of server */

                len = recv(client_socket, server_buf, BUFSIZE, 0);

                if (len <= 0)
                    exit(1);

                server_buf[len] = '\0';

                /* Has the client given us a host name or ip address? */

                if ('0' <= server_buf[0] && server_buf[0] <= '9')
                {
                    /* IP address & port as: 15.8.59.2:21 */
                    sscanf(server_buf, "%d.%d.%d.%d:%d", &a1, &a2, &a3, &a4, &port);
                    server.sin_port = port;
                    server.sin_addr.s_addr = a4 + (a3 << 8) + (a2 << 16) + (a1 << 24);
                }
                else
                {
                    p = strchr(server_buf, ':');
                    sscanf(p+1, "%d", &port);
                    server.sin_port = port;
                    *p = '\0';
                    hp = gethostbyname(server_buf);
                    *p = ':';

                    if (hp == NULL)
                    {
                        client_status = UNKNOWN;
                        strcpy(client_buf, "Unknown!\r\n");
                        client_pending = strlen(client_buf);
                        continue;
                    }

                    server.sin_addr.s_addr = ((struct in_addr *) (hp->h_addr))->s_addr;
                }

                /* if !GoodAddress then check user name & password */

                if (!GoodAddress(client_socket) && !KnownUser(client_socket, server_buf))
                {
                    strcpy(client_buf, "Unauthorized!\r\n");
                    client_pending = strlen(client_buf);
                    client_status = UNKNOWN;
                    continue;
                }

                /* create a non-blocking socket */

                server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

                if (server_socket == -1)
                    exit(1);

                i = 1;
                ioctl(server_socket, FIOSNBIO, &i);      /* set to non-blocking */

                server.sin_family = AF_INET;  /* Internet */

                /* and initiate non-blocking connection to server */
                /* when connected report to client */

                i = connect(server_socket, (struct sockaddr *)&server, sizeof(struct sockaddr_in));

                client_status = CLIENT;
                server_status = CONNECTING;
                client_pending = 0;
                server_pending = 0;
                server_timer = 0;

                continue;
            }
            else if (client_status == CLIENT)  /* read data for server */
            {
                if (server_pending == 0)
                {
                    len = recv(client_socket, server_buf, BUFSIZE, 0);

                    if (len <= 0)
                        exit(1);  /* client has closed */

                    server_buf[len] = '\0';
                    server_pending = len;
                    server_timer = 0;
                }
            }
        }

        /* read to write data to client ? */

        if (FD_ISSET(client_socket, &writefds))
        {
            client_timer = 0;

            if ((len = client_pending) > 0)
            {
                i = send(client_socket, client_buf, len, 0);

                if (i <= 0)
                    exit(1);

                if (i < len)
                {
                    memmove(client_buf, client_buf+i, len-i);
                    client_pending -= i;
                }
                else
                    client_pending = 0;

                /* if unknown server or unauthorised client quit now */

                if (client_status == UNKNOWN)
                    exit(1);
            }
        }

        /* ready to read packet from server ? */
        if (FD_ISSET(server_socket, &readfds))
        {
            server_timer = 0;

            if (server_status == SERVER)  /* read data from server */
            {
                if (client_pending == 0)
                {
                    len = recv(server_socket, client_buf, BUFSIZE, 0);

                    if (len <= 0)
                        exit(1);  /* server has closed */

                    client_buf[len] = '\0';
                    client_pending = len;
                    client_timer = 0;
                }
            }
        }

        /* connected to server or ready to write to server ? */

        if (FD_ISSET(server_socket, &writefds))
        {
            server_timer = 0;

            if (server_status == CONNECTING)
            {
                /* check if connection failed */
                i = connect(server_socket, (struct sockaddr *)&server, sizeof(struct sockaddr_in));

                if (i == -1 && errno != EISCONN)
                    exit(1);

                server_status = SERVER;
                strcpy(client_buf, "Connected!\r\n");

                client_pending = strlen(client_buf);
                client_timer = 0;
                continue;
            }
            else if (server_status == SERVER && (len = server_pending) > 0)
            {
                i = send(server_socket, server_buf, len, 0);

                if (i <= 0)
                    exit(1);

                if (i < len)
                {
                    memmove(server_buf, server_buf+i, len-i);
                    server_pending -= i;
                }
                else
                    server_pending = 0;
            }
        }
    }

    exit(0);
}

