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
#define METHOD_POST "POST"
#define METHOD_PUT "PUT"
#define METHOD_DELETE "DELETE"
#define HEADER_RESPONSE "GETFILE %s %d\r\n\r\n"
#define END_OF_REQUEST "\r\n\r\n"

#define true 1
#define false 0
typedef int bool;

struct request_t {
    bool is_valid_request;
    char *scheme;
    char *method;
    char *path;
};

struct gfserver_t {
    int listenfd;
    unsigned short port;
    int max_npending;
    ssize_t (*handler)(gfcontext_t *, char *, void *);
    void* args;
};

struct gfcontext_t {
    int connfd;
    struct sockaddr_in client_addr;
    struct request_t *request;
};

ssize_t gfs_sendheader(gfcontext_t *ctx, gfstatus_t status, size_t file_len){
    char header[BUFSIZ];

    switch (status) {
        case GF_OK:
            // "GETFILE OK %d \r\n\r\n"
            sprintf(header, HEADER_RESPONSE, "OK", (int)file_len);
            memcpy(header, header, strlen(header));
            break;
        case GF_FILE_NOT_FOUND:
            // "GETFILE FILE_NOT_FOUND 0 \r\n\r\n"
            sprintf(header, HEADER_RESPONSE, "FILE_NOT_FOUND", (int)file_len);
            memcpy(header, header, strlen(header));
            break;
        case GF_ERROR:
        default:
            // "GETFILE ERROR 0 \r\n\r\n"
            sprintf(header, HEADER_RESPONSE, "ERROR", (int)file_len);
            memcpy(header, header, strlen(header));
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

    if ((gfs = (gfserver_t *)malloc(sizeof(gfserver_t))) == NULL) {
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
        if ((request_size + n) > size) {
            n = size - request_size;
        }

        memcpy(buffer + request_size, temp, n); // TODO: buffer overflow?
        request_size += (int)n;
        buffer[request_size] = '\0';
        if (strstr(buffer, END_OF_REQUEST) != NULL) {
            break;
        }
    }

    return request_size;
}

bool check_valid_method (char *method) {
    // only accept GET method for the moment
    if (strcmp(method, METHOD_GET) == 0) {
        return true;
    } else if (strcmp(method, METHOD_POST) == 0) {
        return false;
    } else if (strcmp(method, METHOD_PUT) == 0) {
        return false;
    } else if (strcmp(method, METHOD_DELETE) == 0) {
        return false;
    }
    return false;
}

// NOTE: not sure why this doesn't work
// req->path is giving an error later when getting the content `content_get`
//static int parse_request(gfcontext_t **ctx, char *buffer) {
//    char temp_buffer[strlen(buffer)];
//    strcpy(temp_buffer, buffer);
//
//    int request_size;
//    char *scheme;
//    char *method;
//    char *path;
//
//    struct request_t *req = (struct request_t *)malloc(sizeof(struct request_t));
//
//    req->is_valid_request = false;
//    (*ctx)->request = req;
//
//    // get the scheme, return -1 if not a valid scheme
//    scheme = strtok(temp_buffer, " \t");
//    printf("scheme: '%s'\n", scheme);
//    if (strcmp(scheme, SCHEME) != 0) {
//        return -1;
//    }
//    // allocate so that we can reference outside of this function
//    req->scheme = malloc(sizeof(char) * strlen(scheme));
//    memcpy(req->scheme, scheme, strlen(scheme));
//
//    // get the method, return -1 if not a valid method
//    method = strtok(NULL, " \t");
//    printf("method: '%s'\n", method);
//    bool valid_method = check_valid_method(method);
//    printf("valid_method: '%d'\n", valid_method);
//    if (valid_method != true) {
//        return -1;
//    }
//    // allocate so that we can reference outside of this function
//    req->method = malloc(sizeof(char) * strlen(method));
//    memcpy(req->method, method, strlen(method));
//
//    // get the path, return -1 if path does not start with '/'
//    path = strtok(NULL, " \t");
//    printf("path: '%s'\n", path);
//    if (path[0] != '/') {
//        return -1;
//    }
//    // allocate so that we can reference outside of this function
//    req->path = malloc(sizeof(char) * strlen(path));
//    memcpy(req->path, path, strlen(path));
//
//    req->is_valid_request = true;
//    request_size = (int)strlen(buffer);
//    (*ctx)->request = req;
//
//    return request_size;
//}

void gfserver_serve(gfserver_t *gfs){
    struct sockaddr_in serv_addr;
    char buffer[BUFSIZ];

    // clean the buffer
    memset(&buffer, 0, BUFSIZ);

    printf("Starting server...\n");

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
        gfcontext_t *ctx = (gfcontext_t *)malloc(sizeof(gfcontext_t));
        socklen_t client_size = sizeof(ctx->client_addr);
//        struct request_t *req;
        int transfer_size;

        printf("Listening for incoming connection...\n");

        // accept connection from an incoming client
        if ((ctx->connfd = accept(gfs->listenfd, (struct sockaddr *)&(ctx->client_addr), &client_size)) < 0) {
            perror("Error accepting client");
            exit(EXIT_FAILURE);
        }

        printf("Incoming client connection was accepted\n");

        // get the request from the client
        if (get_request(ctx, buffer, BUFSIZ) < 0) {
            perror("Error receiving request");
            // Do not exit but just close the client's socket
        } else {
            printf("Request: '%s'\n", buffer);

//            // parse the request i.e. <schema> <method> <path> \r\n\r\n
//            parse_request(ctx, buffer);
//
//            req = ctx->request;
//
//            printf("\n**req**\n");
//            printf("scheme: '%s'\n", req->scheme);
//            printf("method: '%s'\n", req->method);
//            printf("path: '%s'\n", req->path);
//
//            if (req->is_valid_request != true) {
//                // error
//                perror("Invalid request format");
//                gfs_sendheader(ctx, GF_ERROR, 0);
//            } else {
//                printf("\n\n");
//                printf("Sending the file...\n");
//                printf("path: '%s'\n", req->path);
//                printf("hello world\n");
//                printf("len: %d\n", (int)strlen(req->path));
//
//                char path[strlen(req->path)];
//                strcpy(path, req->path);
//
//                printf("req->path: '%s'\n", req->path);
//                printf("path: '%s'\n", path);
//
//                // send the file
//                transfer_size = (int)gfs->handler(ctx, path, gfs->args);
//                printf("Transfer: %d bytes\n", transfer_size);
//            }

            // parse the request i.e. <schema> <method> <path> \r\n\r\n
            char temp_buffer[strlen(buffer)];
            strcpy(temp_buffer, buffer);
            char *scheme;
            char *method;
            char *path;
            bool is_valid_request = true;

            // get the scheme, set is_valid_request to false if not a valid scheme
            scheme = strtok(temp_buffer, " \t");
            if (strcmp(scheme, SCHEME) != 0) {
                is_valid_request = false;
            }

            if (is_valid_request) {
                // get the method, set is_valid_request to false if not a valid method
                method = strtok(NULL, " \t");
                bool valid_method = check_valid_method(method);
                if (valid_method != true) {
                    is_valid_request = false;
                }
            }

            if (is_valid_request) {
                // get the path, set is_valid_request to false if path does not start with '/'
                path = strtok(NULL, " \t\r\n");
                if (path[0] != '/') {
                    is_valid_request = false;
                }
            }

            if (is_valid_request != true) {
                // error
                fprintf(stderr, "buffer: '%s'\n", buffer);
                gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
            } else {
                printf("Sending the file...\n");
                printf("path: '%s'\n", path);

                // send the file
                transfer_size = (int)gfs->handler(ctx, path, gfs->args);
                printf("Transfer: %d bytes\n", transfer_size);
            }
        }

        // close the accepted connection
        close(ctx->connfd);
        // clean up and free malloc
//        free(ctx->request->scheme);
//        free(ctx->request->method);
//        free(ctx->request->path);
//        free(ctx->request);
        free(ctx);
    }
}

