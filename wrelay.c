/****************************************************************************** 
    TCP/IP Relay for HP access to the World Wide Web, Gopher etc.

    test program for relayd

    it listens on port 3000 for connections and the forks
    and execs relayd passing it the client socket on file descriptor 0

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

#define PORT        3000    /* port for tcp relay service */
struct sockaddr_in server;  /* address info */
struct sockaddr_in client;  /* address info */
struct sockaddr_in myself;  /* address info */
struct hostent *hp;         /* pointer to host info for remote host */

#define HP_SUBNET   15      /* HP has a level 0 IP address */

main(int argc, char **argv)
{
    struct timeval timeout;
    fd_set readfds, writefds, exceptfds;
    int i, s, cs, ls, len, addrlen, childpid;
    char *p, sec[20], usec[20], sock[10];
    struct timeval tp;
    struct timezone tzp;

    /* ensure that children won't become zombies */

    signal(SIGCLD, SIG_IGN);

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

            gettimeofday(&tp, &tzp);
            sprintf(sec, "%lu", tp.tv_sec);
            sprintf(usec, "%ld", tp.tv_usec);
            sprintf(sock, "%d", cs);

            childpid = fork();

            if (childpid < 0)
                perror("fork failed");
            else if (childpid == 0)
            {
                close(0);
          /*      close(1);
                close(2);   */
                dup2(cs, 0);
           /*     dup2(cs, 1);
                dup2(cs, 2);  */
                close(cs);

                execl("/nfs/hplose/permanent/dsr/www/relayd", "relayd", sock, sec, usec, (char *)0);
                perror("exec failed");
                exit(1);
            }

            close(cs);
        }
    }
}
