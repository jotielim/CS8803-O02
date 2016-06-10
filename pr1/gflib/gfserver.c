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

#include "gfserver.h"

/* 
 * Modify this file to implement the interface specified in
 * gfserver.h.
 */

#define SCHEME "GETFILE"
#define METHOD_GET "GET"
#define HEADER_RESPONSE "GETFILE %s %d \r\n\r\n"

struct request_t {
    char *scheme;
    char *method;
    char *path;
    char *eor;
};

struct gfserver_t {
    int listenfd;
    unsigned short port;
    int max_npending;
    ssize_t (*handler)(gfcontext_t *, char *, void*);
    void* args;
};

struct gfcontext_t {
    int connfd;
    struct sockaddr_in client_addr;
    struct request_t request;
};

ssize_t gfs_sendheader(gfcontext_t *ctx, gfstatus_t status, size_t file_len){
    char header[file_len];

    switch (status) {
        case GF_OK:
            // "GETFILE OK %d \r\n\r\n"
            sprintf(header, HEADER_RESPONSE, "OK", (int)file_len);
            break;
        case GF_FILE_NOT_FOUND:
            // "GETFILE FILE_NOT_FOUND 0 \r\n\r\n"
            sprintf(header, HEADER_RESPONSE, "FILE_NOT_FOUND", (int)file_len);
            break;
        case GF_ERROR:
        default:
            // "GETFILE ERROR 0 \r\n\r\n"
            sprintf(header, HEADER_RESPONSE, "ERROR", (int)file_len);
            break;
    }

    return send(ctx->connfd, header, strlen(header), 0);
}

ssize_t gfs_send(gfcontext_t *ctx, void *data, size_t len){
    return send(ctx->connfd, data, len, 0);
}

void gfs_abort(gfcontext_t *ctx){
    close(ctx->connfd);
}

gfserver_t* gfserver_create(){
    gfserver_t *gfs;
    int listenfd;

    if ((gfs = malloc(sizeof(*gfs))) != NULL) {
        perror("Unable to allocate memory");
        exit(EXIT_FAILURE);
    }

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to create the socket");
        exit(EXIT_FAILURE);
    }

    gfs->listenfd = listenfd;

    return gfs;
}

void gfserver_set_port(gfserver_t *gfs, unsigned short port){
    gfs->port = port;
}
void gfserver_set_maxpending(gfserver_t *gfs, int max_npending){
    gfs->max_npending = max_npending;
}

void gfserver_set_handler(gfserver_t *gfs, ssize_t (*handler)(gfcontext_t *, char *, void*)){
    gfs->handler = handler;
}

void gfserver_set_handlerarg(gfserver_t *gfs, void* arg){
    gfs->args = arg;
}

static int get_request(gfcontext_t *ctx, char *buffer, int size) {
    int request_size = 0;
    ssize_t n;
    char temp[size];

    while ((n = recv(ctx->connfd, temp, (size_t)size, 0)) > 0) {
        memcpy(buffer+request_size, temp, n); // TODO: buffer overflow?
        request_size += n;
        buffer[request_size] = '\0';
        if (strstr(buffer, "\r\n\r\n") != NULL) {
            break;
        }
    }

    return request_size;
}

static struct request_t* parse_request(gfcontext_t *ctx, char *buffer) {
    struct request_t *req = malloc(sizeof(struct request_t));

    req->scheme = strtok(buffer, " \t");
    req->method = strtok(NULL, " \t");
    req->path = strtok(NULL, " \t");
    req->eor = strtok(NULL, " \t");

    ctx->request = *req;

    return req;
}

void gfserver_serve(gfserver_t *gfs){
    struct sockaddr_in serv_addr;
    char *buffer;

    // prepare the sockaddr_in structure
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htons(INADDR_ANY);
    serv_addr.sin_port = htons(gfs->port);

    // bind the socket to the address
    if (bind(gfs->listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Unable to bind the socket");
        exit(EXIT_FAILURE);
    }

    // listen to maximum pending connections
    if (listen(gfs->listenfd, gfs->max_npending) < 0) {
        perror("Error listening to maximum pending connections");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", gfs->port);

    // infinite loop, continuously listening
    for ( ; ; ) {
        gfcontext_t *ctx = malloc(sizeof(gfcontext_t));
        socklen_t client_size = sizeof(ctx->client_addr);
        struct request_t *req;
        int transfer_size;

        // accept connection from an incoming client
        if ((ctx->connfd = accept(gfs->listenfd, (struct sockaddr *)&(ctx->client_addr), &client_size)) < 0) {
            perror("Error accepting client");
            exit(EXIT_FAILURE);
        }

        // get the request from the client
        if (get_request(ctx, buffer, BUFSIZ) < 0) {
            perror("Error receiving request");
            // Do not exit but just close the client's socket
        } else {
            printf("Request: %s", buffer);

            // parse the request i.e. <schema> <method> <path> \r\n\r\n
            req = parse_request(ctx, buffer);

            if (strcmp(req->scheme, SCHEME) != 0 ||
                    strcmp(req->method, METHOD_GET) != 0 ||
                    req->path[0] != '/' ||
                    strcmp(req->eor, "\r\n\r\n") != 0) {
                // error
                perror("Invalid request format");
                gfs_sendheader(ctx, GF_ERROR, 0);
            } else {
                // send the file
                transfer_size = (int)gfs->handler(ctx, req->path, gfs->args);
                printf("Transfer: %d bytes\n", transfer_size);
            }
        }

        // close the accepted connection
        close(ctx->connfd);
    }
}

