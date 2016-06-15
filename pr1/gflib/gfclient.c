#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

#include "gfclient.h"

#define BUFFER_SIZE 4096
#define SCHEME "GETFILE"
#define STATUS_OK "OK"
#define STATUS_FILE_NOT_FOUND "FILE_NOT_FOUND"
#define STATUS_ERROR "ERROR"
#define HEADER_REQUEST "GETFILE %s %s\r\n\r\n"
#define END_OF_RESPONSE "\r\n\r\n"

#define true 1
#define false 0
typedef int bool;

struct addrinfo *addr_hints;

struct response_t {
    bool is_valid_response;
    char *scheme;
    char *status;
    int filelen;
    char *eor;
};

struct gfcrequest_t {
    int sockfd;
    char *server;
    char *path;
    unsigned short port;
    void (*headerfunc)(void*, size_t, void *);
    void *headerarg;
    void (*writefunc)(void*, size_t, void *);
    void *writearg;
    gfstatus_t status;
    size_t filelen;
    size_t bytesrecv;
    struct response_t *response;
};

gfcrequest_t *gfc_create(){
    gfcrequest_t *gfr;

    if ((gfr = (gfcrequest_t *)malloc(sizeof(gfcrequest_t))) == NULL) {
        perror("Unable to allocate memory");
        exit(EXIT_FAILURE);
    }
    gfr->status = GF_INVALID;
    gfr->filelen = 0;
    gfr->bytesrecv = 0;
    gfr->headerfunc = NULL;
    gfr->writefunc = NULL;

    return gfr;
}

void gfc_set_server(gfcrequest_t *gfr, char* server){
    gfr->server = server;
}

void gfc_set_path(gfcrequest_t *gfr, char* path){
    gfr->path = path;
}

void gfc_set_port(gfcrequest_t *gfr, unsigned short port){
    gfr->port = port;
}

void gfc_set_headerfunc(gfcrequest_t *gfr, void (*headerfunc)(void*, size_t, void *)){
    gfr->headerfunc = headerfunc;
}

void gfc_set_headerarg(gfcrequest_t *gfr, void *headerarg){
    gfr->headerarg = headerarg;
}

void gfc_set_writefunc(gfcrequest_t *gfr, void (*writefunc)(void*, size_t, void *)){
    gfr->writefunc = writefunc;
}

void gfc_set_writearg(gfcrequest_t *gfr, void *writearg){
    gfr->writearg = writearg;
}

// Method to get the status in gfstatus_t type given a string
static gfstatus_t get_status(char *status) {
    if (strcmp(status, STATUS_OK) == 0) {
        return GF_OK;
    } else if (strcmp(status, STATUS_FILE_NOT_FOUND) == 0) {
        return GF_FILE_NOT_FOUND;
    } else if (strcmp(status, STATUS_ERROR) == 0) {
        return GF_ERROR;
    }
    return GF_INVALID;
}

static int parse_response(gfcrequest_t *gfr, char *buffer) {
    char temp_buffer[strlen(buffer)];
    strcpy(temp_buffer, buffer);

    char *status;
    int header_size;

    struct response_t *res = (struct response_t *)malloc(sizeof(struct response_t));
    res->is_valid_response = false;
    gfr->response = res;

    strtok(temp_buffer, "\r\n\r\n");
    header_size = (int)strlen(temp_buffer) + 4; // +4 for \r\n\r\n

    res->scheme = strtok(temp_buffer, " \t");
    if (strcmp(res->scheme, SCHEME) != 0) {
        return -1;
    }

    status = strtok(NULL, " \t");
    if ((gfr->status = get_status(status)) == GF_INVALID) {
        return -1;
    }

    char *tmp = strtok(NULL, " \t\r\n");
    printf("tmp: '%s'\n", tmp);
    if (tmp != NULL) {
        res->filelen = atoi(tmp);
        gfr->filelen = (size_t)atoi(tmp);
    }

    res->is_valid_response = true;
    gfr->response = res;

    return header_size;
}

int gfc_perform(gfcrequest_t *gfr){
    char req[BUFSIZ];
    char buffer[BUFFER_SIZE];
    struct addrinfo *host;
    int sockfd;
    char portno[6];
    int header_size = -1;
    ssize_t recv_size;

    // convert port to string
    sprintf(portno, "%d", gfr->port);
    printf("Port Number: %s\n", portno);

    getaddrinfo(gfr->server, portno, addr_hints, &host);

    if ((sockfd = socket(host->ai_family, host->ai_socktype, host->ai_protocol)) < 0) {
        perror("Unable to create the socket");
        return -1;
    }
    gfr->sockfd = sockfd;

    printf("Socket is created\n");

    if (connect(sockfd, host->ai_addr, host->ai_addrlen) < 0) {
        freeaddrinfo(host);
        gfc_cleanup(gfr);
        gfc_global_cleanup();
        perror("Error connecting");
        exit(EXIT_FAILURE);
    }

    printf("Connected to the socket\n");

    // make sure to clean up
    freeaddrinfo(host);

    sprintf(req, HEADER_REQUEST, "GET", gfr->path);
    printf("Request: '%s'\n", req);

    // send request to server
    if (send(sockfd, req, strlen(req), 0) < 0) {
        perror("Error sending request");
        return -1;
    }

    printf("Successfully sending request to the server\n");

    // TODO: change to while loop
    // read the header
    if ((recv_size = recv(sockfd, buffer, BUFFER_SIZE, 0)) > 0) {
        printf("response: '%s'\n", buffer);
        // if not NULL, \r\n\r\n exists in buffer. "end of request"
        if (strstr(buffer, END_OF_RESPONSE) != NULL) {
            // parse the request
            header_size = parse_response(gfr, buffer);
        } else {
            // could not find end of request in the buffer
            // terminate on timeout
            header_size = -1;
        }
    }

    // if header is not in the right format or invalid, return error and exit
    if (header_size == -1 || gfc_get_status(gfr) == GF_INVALID) {
        perror("Header error");
        return -1;
    }

    // header callback
    if (gfr->headerfunc) {
        gfr->headerfunc(buffer, (size_t)header_size, gfr->headerarg);
    }

    // get remainder of the buffer as the content to write
    if (gfr->writefunc) {
        // move past the header_size and adjust the recv_size by header_size
        gfr->writefunc(buffer + header_size, recv_size - (size_t)header_size, gfr->writearg);
        gfr->bytesrecv = recv_size - (size_t)header_size;
    }

    // read the response until all bytes are received
    while (gfr->bytesrecv < gfr->filelen) {
        if ((recv_size = recv(sockfd, buffer, BUFFER_SIZE, 0)) == 0) {
            printf("error occur\n");
            close(sockfd);
            return -1;
        }
        gfr->writefunc(buffer, (size_t)recv_size, gfr->writearg);
        gfr->bytesrecv += recv_size;
    }

    close(sockfd);

    return 0;
}

gfstatus_t gfc_get_status(gfcrequest_t *gfr){
    return gfr->status;
}

char* gfc_strstatus(gfstatus_t status){
    char *status_string;

    switch (status) {
        case GF_OK:
            status_string = "OK";
            break;
        case GF_FILE_NOT_FOUND:
            status_string = "FILE_NOT_FOUND";
            break;
        case GF_ERROR:
            status_string = "ERROR";
            break;
        case GF_INVALID:
        default:
            status_string = "INVALID";
            break;
    }

    return status_string;
}

size_t gfc_get_filelen(gfcrequest_t *gfr){
    return gfr->filelen;
}

size_t gfc_get_bytesreceived(gfcrequest_t *gfr){
    return gfr->bytesrecv;
}

void gfc_cleanup(gfcrequest_t *gfr){
    close(gfr->sockfd);
    free(gfr->response);
    free(gfr);
}

void gfc_global_init(){
    addr_hints = (struct addrinfo *)malloc(sizeof(struct addrinfo));
    memset(addr_hints, 0, sizeof(struct addrinfo));
    addr_hints->ai_family = AF_INET;
    addr_hints->ai_socktype = SOCK_STREAM;
    addr_hints->ai_protocol = 0;
}

void gfc_global_cleanup(){
    free(addr_hints);
}
