#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

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

#define BUFSIZE 4096

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  transferserver [options]\n"                                                \
"options:\n"                                                                  \
"  -p                  Port (Default: 8888)\n"                                \
"  -f                  Filename (Default: bar.txt)\n"                         \
"  -h                  Show this help message\n"

// declare header for function send_file
int send_file(int connfd, char *filename);

int main(int argc, char **argv) {
    int option_char;
    int portno = 8888; /* port to listen on */
    char *filename = "bar.txt"; /* file to transfer */
    int maxnpending = 5;

    // Parse and set command line arguments
    while ((option_char = getopt(argc, argv, "p:f:h")) != -1){
        switch (option_char) {
            case 'p': // listen-port
                portno = atoi(optarg);
                break;
            case 'n': // server
                maxnpending = atoi(optarg);
                break;
            case 'f': // filename
                filename = optarg;
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

    int listenfd, connfd;
    struct sockaddr_in serv_addr;

    char buffer[BUFSIZE];

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

        // clean the buffer
        memset(&buffer, 0, BUFSIZE);

        // send the file
        if (send_file(connfd, filename) < 0) {
            perror("fail sending file");
            exit(EXIT_FAILURE);
        }

        // close the accepted connection
        close(connfd);
    }
}

int send_file(int connfd, char *filename) {
    int send_count;
    ssize_t read_bytes, send_bytes, send_size;
    char buffer[BUFSIZE];
    int f;

    // reset counters to 0
    send_count = 0;
    send_size = 0;

    // open the file as read only
    // if file not found, terminate the program
    if ((f = open(filename, O_RDONLY)) < 0) {
        perror(filename);
        return -1;
    }

    // loop until there are no more bytes left to read
    while ((read_bytes = read(f, buffer, BUFSIZE)) > 0) {
        // send bytes to the client
        // terminate when error occur
        if ((send_bytes = send(connfd, buffer, (size_t)read_bytes, 0)) < read_bytes) {
            perror("send error");
            return  -1;
        }
        send_count++;
        send_size += send_bytes;
    }
    close(f); // close file
    return send_count;
}
