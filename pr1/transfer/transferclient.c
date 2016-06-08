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
    struct hostent *he;

    char buffer[BUFSIZE];
    char recvline[BUFSIZE];

    // clean the buffer
    memset(&buffer, 0, BUFSIZE);

    // create the socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to create the socket");
        exit(EXIT_FAILURE);
    }

    // resolve hostname
    if ((he = gethostbyname(hostname)) == NULL) {
        perror("Hostname not found");
        close(sockfd);
        exit(EXIT_FAILURE); // exit when error or hostname is not found
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    // copy the network address to sockaddr_in structure
    memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);

    // connect the client to the socket
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) {
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
