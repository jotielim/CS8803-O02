#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <getopt.h>

/* Be prepared accept a response of this length */
//#define BUFSIZE 4096
#define BUFSIZE 16
#define MAXSTRING (BUFSIZE-1)

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  echoclient [options]\n"                                                    \
"options:\n"                                                                  \
"  -s                  Server (Default: localhost)\n"                         \
"  -p                  Port (Default: 8888)\n"                                \
"  -m                  Message to send to server (Default: \"Hello World!\"\n"\
"  -h                  Show this help message\n"

/* Main ========================================================= */
int main(int argc, char **argv) {
    int option_char = 0;
    char *hostname = "localhost";
    unsigned short portno = 8888;
    char *message = "Hello World!";

    // Parse and set command line arguments
    while ((option_char = getopt(argc, argv, "s:p:m:h")) != -1) {
        switch (option_char) {
            case 's': // server
                hostname = optarg;
                break;
            case 'p': // listen-port
                portno = atoi(optarg);
                break;
            case 'm': // server
                message = optarg;
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

    // 1. Create a socket with the socket() system call
    // 2. Connect the socket to the address of the server using the connect() system call
    // 3. Send and receive data.

    int sockfd;
    struct addrinfo hints, *host;
    ssize_t n;

    char buffer[BUFSIZE];
    char recvline[MAXSTRING];
    char port_str[6];

    // convert port to string
    sprintf(port_str, "%d", portno);

    // clean the buffer
    memset(&buffer, 0, BUFSIZE);

    // use getaddrinfo to get the host
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;

    getaddrinfo(hostname, port_str, &hints, &host);

    // create the socket
    sockfd = socket(host->ai_family, host->ai_socktype, host->ai_protocol);
    if (sockfd < 0) {
        perror("Unable to create the socket");
        exit(1);
    }

    // connect the client to the socket
    if (connect(sockfd, host->ai_addr, host->ai_addrlen)) {
        perror("Unable to connect to the server");
        close(sockfd);
        exit(3); // error
    }

    // send message to the server
    if (send(sockfd, message, strlen(message), 0) < 0) {
        perror("Unable to send message to the server");
        close(sockfd);
        exit(4);
    }

    // receive message from the server
    n = recv(sockfd, recvline, MAXSTRING, 0);
    recvline[n] = '\0';
    fputs(recvline, stdout);

    // close the socket before exiting
    close(sockfd);
    exit(0);
}