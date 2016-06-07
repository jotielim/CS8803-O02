#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#if 0
/*
 * Structs exported from netinet/in.h (for easy reference)
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr;
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif

//#define BUFSIZE 4096
#define BUFSIZE 16
#define MAXSTRING (BUFSIZE-1)

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  echoserver [options]\n"                                                    \
"options:\n"                                                                  \
"  -p                  Port (Default: 8888)\n"                                \
"  -n                  Maximum pending connections\n"                         \
"  -h                  Show this help message\n"

int main(int argc, char **argv) {
    int option_char;
    int portno = 8888; /* port to listen on */
    int maxnpending = 5;

    // Parse and set command line arguments
    while ((option_char = getopt(argc, argv, "p:n:h")) != -1){
        switch (option_char) {
            case 'p': // listen-port
                portno = atoi(optarg);
                break;
            case 'n': // server
                maxnpending = atoi(optarg);
                break;
            case 'h': // help
                fprintf(stdout, "%s", USAGE);
                exit(0);
                break;
            default:
                fprintf(stderr, "%s", USAGE);
                exit(1);
        }
    }

    /* Socket Code Here */

    // 1. Create a socket with the socket() system call.
    // 2. Bind the socket to an address using the bind() system call. For a server socket on the Internet,
    //    an address consists of a port number on the host machine.
    // 3. Listen for connections with the listen() system call.
    // 4. Accept a connection with the accept() system call. This call typically blocks until a client connects
    //    with the server.
    // 5. Send and receive data.

    int listenfd, connfd, n;
    struct sockaddr_in serv_addr;

    char buffer[BUFSIZE];

    // clean the buffer
    memset(&buffer, 0, BUFSIZE);

    // create socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    // prepare the sockaddr_in structure
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htons(INADDR_ANY);
    serv_addr.sin_port = htons(portno);

    // bind the socket to the address
    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    // listen to maximum pending connections
    listen(listenfd, maxnpending);

    // infinite loop, continuously listening
    for ( ; ; ) {
        // accept connection from an incoming client
        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);

        if ((n = recv(connfd, buffer, MAXSTRING, 0)) > 0) {
            // echo back to the client
            send(connfd, buffer, n, 0);
        }

        if (n < 0) {
            perror("Read error");
            exit(1);
        }

        // close the accepted connection
        close(connfd);
    }

    // close listening socket
    close(listenfd);
}