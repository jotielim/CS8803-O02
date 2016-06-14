#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>

#define BUFSIZE 4096

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  transferclient [options]\n"                                                \
"options:\n"                                                                  \
"  -s                  Server (Default: localhost)\n"                         \
"  -p                  Port (Default: 8888)\n"                                \
"  -o                  Output file (Default foo.txt)\n"                       \
"  -h                  Show this help message\n"

// declare header for function recv_file
int recv_file(int sockfd, char *filename);

/* Main ========================================================= */
int main(int argc, char **argv) {
    int option_char = 0;
    char *hostname = "localhost";
    unsigned short portno = 8888;
    char *filename = "foo.txt";

    // Parse and set command line arguments
    while ((option_char = getopt(argc, argv, "s:p:o:h")) != -1) {
        switch (option_char) {
            case 's': // server
                hostname = optarg;
                break;
            case 'p': // listen-port
                portno = atoi(optarg);
                break;
            case 'o': // filename
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

    // 1. Create a socket with the socket() system call
    // 2. Connect the socket to the address of the server using the connect() system call
    // 3. Send and receive data.

    int sockfd;
    struct sockaddr_in serv_addr;
    struct addrinfo hints, *host;

    char buffer[BUFSIZE];
    char recvline[BUFSIZE];
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
        exit(EXIT_FAILURE);
    }

    // connect the client to the socket
    if (connect(sockfd, host->ai_addr, host->ai_addrlen)) {
        perror("Unable to connect to the server");
        close(sockfd);
        exit(EXIT_FAILURE); // error
    }

    // get bytes from the server
    recv_file(sockfd, filename);

    // close the socket before exiting
    close(sockfd);
    exit(EXIT_SUCCESS);
}

int recv_file(int sockfd, char *filename) {
    int f, recv_count, recv_size;
    ssize_t recv_bytes;
    char buffer[BUFSIZE];

    // create file to attempt to save received data
    if ((f = open(filename, O_WRONLY|O_CREAT, 0644)) < 0) {
        perror("error creating file");
        return -1;
    }

    // reset counters to 0
    recv_count = 0;
    recv_size = 0;

    // TODO: place a timeout to terminate the connection
    // receive file from server
    while ((recv_bytes = recv(sockfd, buffer, BUFSIZE, 0)) > 0) {
        recv_count++;
        recv_size += recv_bytes;

        // save the received buffer data to local file
        // terminate when error occurs
        if (write(f, buffer, (size_t)recv_bytes) < 0) {
            perror("error writing to file");
            return -1;
        }
    }
    close(f); // close file
    printf("Client received %d bytes in %d recv(s)\n", recv_size, recv_count);
    return recv_size;
}
