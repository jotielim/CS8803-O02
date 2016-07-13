#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>

#include "gfserver.h"
#include "shm_channel.h"

//Replace with an implementation of handle_with_cache and any other
//functions you may need.

extern ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg);
extern shm_data_t *g_shm_array; // from shm_channel.c

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg) {
    int shmid, segsize;
    size_t buffsize, bytes_read, filelen, readlen;

    shmid = pop_shm();

    cache_request_t request;
    strcpy(request.key, path);
    get_shmname(shmid, request.shmname, sizeof(request.shmname));
    segsize = get_segsize();
    request.segsize = segsize;

    if (send_cache_request(&request) == -1) {
        fprintf(stderr, "Unable to send message %s\n", path);
        return gfs_sendheader(ctx, GF_ERROR, 0);
    }

    // calculate the size of data we can read
    buffsize = segsize - (2 * sizeof(sem_t)) - sizeof(int);

    // wait until there is a response from cache
    sem_wait(&(g_shm_array[shmid]->full));
    filelen = g_shm_array[shmid]->filesize;
    sem_post(&(g_shm_array[shmid]->empty));

    if (filelen == FILE_NOT_FOUND) {
        fprintf(stderr, "File not found in cache: %s\n", path);

        size_t curl_response = handle_with_curl(ctx, path, arg);

        add_shm(shmid);
        if (curl_response == SERVER_FAILURE) {
            return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
        }

        return curl_response;
    }

    gfs_sendheader(ctx, GF_OK, filelen);

    bytes_read = 0;

    while (bytes_read < filelen) {
        sem_wait(&(g_shm_array[shmid]->full));

        // read the data
        if ((filelen - bytes_read) > buffsize) {
            readlen = buffsize;
        } else {
            readlen = filelen - bytes_read;
        }

        gfs_send(ctx, &(g_shm_array[shmid]->data), readlen);
        bytes_read += readlen;

        sem_post(&(g_shm_array[shmid]->empty));
    }

    // put it back on the free queue
    add_shm(shmid);

    return bytes_read;
}
