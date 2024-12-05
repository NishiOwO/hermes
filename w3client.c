/* w3client.c

This is a program for testing the shared cache mechanism, and will
eventually evolve into the client code for www browsers and the
cache clean up utility for cleaning the cache. The latter is expected
to run in the early morning to remove little used entries and restore
cache consistency.

Usage:
        w3client FIND url
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

#define DEBUG  1      /* delete this line for final code */
#define SERVER  "dragget.hpl.hp.com"
#define PORT    2786  /* service port number */
#define ADDRNOTFOUND    0xffffffff  /* value returned for an unknown host */
#define RETRIES 5   /* number of times to retry before giving up */

#define BUFSIZE 1024  /* max size of receive packets */

int s;      /* socket descriptor */

int buflen;             /* number of bytes read */
char buffer[BUFSIZE];   /* receive buffer */

int rsize = 0;          /* size of response buffer */
char *response;         /* malloc'ed response buffer */

struct hostent *hp;   /* pointer to host info for requested host */
struct servent *sp;   /* pointer to service info */

struct sockaddr_in myaddr_in;       /* for local socket address */
struct sockaddr_in servaddr_in;   /* server's socket address */

int NoReader;       /* set bu SIGPIPE */

/* handler()

    This routine is the signal handler for the alarm signal.
    It simply re-installs itself as the handler and returns.
*/

void handler()
{
    signal(SIGALRM, handler);
}

main(int argc, char **argv)
{
    int retry = RETRIES;

    if (argc != 2)
    {
        fprintf(stderr, "Tests w3cache server\n");
        fprintf(stderr, "Useage: %s \"METHOD args\"\n", argv[0]);
        exit(1);
    }

    /* clear out address structures */
    memset((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
    memset((char *)&servaddr_in, 0, sizeof(struct sockaddr_in));

    /* set up server address */

    servaddr_in.sin_family = AF_INET;

    /* get the host info for server's hostname (global define) */
    hp = gethostbyname(SERVER);

    if (hp == NULL)
    {
        fprintf(stderr, "%s: %s not found in /etc/hosts\n", argv[0], SERVER);
        exit(1);
    }

    servaddr_in.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;

    /* would be better in long term to use getservbyname() */
    servaddr_in.sin_port = PORT;

    /* create the socket */

    s = socket(AF_INET, SOCK_DGRAM, 0);

    if (s == -1)
    {
        perror(argv[0]);
        fprintf(stderr, "%s: unable to create socket\n", argv[0]);
        exit(1);
    }

    /* Bind socket to some local address so that the server can
       send the reply back. A port number of zero will be used
       so that the system will assign any available port number.
       An address of INADDR_ANY will be used so that we won't
       have to look up the Internet address of the local host */

    myaddr_in.sin_family = AF_INET;
    myaddr_in.sin_port = 0;
    myaddr_in.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, &myaddr_in, sizeof(struct sockaddr_in)) == -1)
    {
        perror(argv[0]);
        fprintf(stderr, "%s: unable to bind socket\n", argv[0]);
        exit(1);
    }

    /* setup alarm signal handler */

    signal(SIGALRM, handler);

    /* send the request to the server */

  again:

    if (sendto(s, argv[1], strlen(argv[1]), 0, &servaddr_in, sizeof(struct sockaddr_in)) == -1)
    {
        perror(argv[0]);
        fprintf(stderr, "%s: unable to send request\n", argv[0]);
        exit(1);
    }


    /* Set up a timeout so that client doesn't hang in case the
       packet gets lost. After all, UDP does not guarantee delivery */

    alarm(5);

    if ((buflen = recv(s, buffer, BUFSIZE-1, 0)) == -1)
    {
        if (errno == EINTR) /* alarm went off and aborted receive */
        {
            if (retry--)
                goto again;

            printf("Unable to get response from %s : %d after %d attempts.\n",
                    SERVER, PORT, RETRIES);

            exit(1);
        }

        perror(argv[0]);
        fprintf(stderr, "%s: unable to receive response\n", argv[0]);
        exit(1);
    }

    alarm(0);

    /* print out response */

    buffer[buflen] = '\0';
    printf("Reply: %s\n", buffer);
}
