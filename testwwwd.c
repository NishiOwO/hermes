/****************************************************************************** 
    tests wwwd - an inetd based World Wide Web server

    it listens on port 2784 for connections and the forks
    and execs wwwd passing it the client socket on file descriptor 0

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
#include <string.h>         /* for string processing functions */
#include <sys/ioctl.h>      /* for setting listen socket to be non-blocking */
#include <signal.h>         /* for trapping signals e.g. SIGPIPE */
#include <time.h>           /* for timeouts with select() */

#define PORT        2784    /* port for tcp relay service */
struct sockaddr_in server;  /* address info */
struct sockaddr_in client;  /* address info */
struct sockaddr_in myself;  /* address info */
struct hostent *hp;         /* pointer to host info for remote host */

#define HP_SUBNET   15      /* HP has a level 0 IP address */

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

main(int argc, char **argv)
{
    struct timeval timeout;
    fd_set readfds, writefds, exceptfds;
    int i, s, cs, ls, len, addrlen, childpid, world_readable;
    char *p, sec[20], usec[20], sock[10];
    struct timeval tp;
    struct timezone tzp;

    /* ensure that children won't become zombies */

    signal(SIGCLD, SIG_IGN);

    world_readable = 0;

    if (argc > 1 && strcmp(argv[1], "-world") == 0)
        world_readable = 1;

    /* set up address structure for receiving requests from clients */

    myself.sin_family = AF_INET;
    myself.sin_addr.s_addr = INADDR_ANY;  /* listen on wildcard address */
    myself.sin_port = PORT;

    /* create the listen socket */

    ls = socket(AF_INET, SOCK_STREAM, 0);

    if (ls == -1)
    {
        printf("can't create socket: %m\n");
        exit(1);
    }

    /* bind the listen address to the socket */

    if (bind(ls, &myself, sizeof(myself)) == -1)
    {
        printf("can't bind address: %m\n");
        exit(1);
    }

    /* Initiate listen on the socket so remote users can connect,
       setting listen backlog to 5 (largest currently supported) */

    if (listen(ls, 5) == -1)
    {
        printf("unable to listen on socket\n: %m");
        exit(1);
    }

    timeout.tv_sec = 5;     /* giving a 5 second timer resolution */
    timeout.tv_usec = 0;

    /* set listen socket to be non-blocking */

    i = 1;
    ioctl(ls, FIOSNBIO, &i);

    /* enter indefinite loop waiting for requests from clients */

    for (;;)
    {
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);

        FD_SET(ls, &readfds);


        if ((i = select(NFDBITS, (int*)(&readfds), (int*)(&writefds), (int*)(&exceptfds), 0)) == -1)
        {
            printf("select failed");
            break;
        }
        else if (i == 0)
        {
          /*  printf("timechk\n");  */
            continue;
        }
    
        if (FD_ISSET(ls, &readfds)) /* read from socket skt */
        {
            addrlen = sizeof(struct sockaddr_in);
            cs = accept(ls, &client, &addrlen);

            if (cs == -1)
            {
                perror("can't accept new client");
                continue;
            }

            i = client.sin_addr.s_addr;

            if (i >> 24 != HP_SUBNET)
            {
                printf("unauthorised access from %s\n", inet_ntoa(client.sin_addr));
                close(cs);
                continue;
            }

            printf("hello from %s\n", ClientName(cs));

            childpid = fork();

            if (childpid < 0)
            {
                perror("fork failed");
                close(cs);
                continue;
            }

            if (childpid == 0)
            {
                close(0);
            /*    close(1);
                close(2);  */
                dup2(cs, 0);
            /*    dup2(cs, 1);
                dup2(cs, 2);  */
                close(cs);

                if (world_readable)
                    execl("/nfs/hplose/permanent/dsr/www/wwwd", "wwwd", "-world", (char *)0);
                else
                    execl("/nfs/hplose/permanent/dsr/www/wwwd", "wwwd", (char *)0);
                perror("exec failed");
                exit(1);
            }

            close(cs);
        }
    }
}
