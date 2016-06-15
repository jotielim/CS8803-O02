#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "gfserver.h"
#include "content.h"
#include "steque.h"

#define BUFFER_SIZE 4096

typedef struct request_item_t {
    gfcontext_t *ctx;
    char path[BUFSIZ];
} request_item_t;

/**
 * global variables
 */
static int g_num_threads;
static steque_t *g_queue;
static pthread_t *g_workers;
static pthread_mutex_t g_mutex_queue = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond_remove = PTHREAD_COND_INITIALIZER;

/**
 * function to add item to the bottom of the queue
 */
static void add_item (request_item_t *req) {
    pthread_mutex_lock(&g_mutex_queue);
    steque_enqueue(g_queue, (steque_item)req);
    pthread_mutex_unlock(&g_mutex_queue);

    pthread_cond_signal(&g_cond_remove);
}

/**
 * function to remove item from the top of the queue and return the removed item
 */
static request_item_t* remove_item () {
    steque_item item;

    pthread_mutex_lock(&g_mutex_queue);

    // wait until there is content to remove
    while (steque_isempty(g_queue)) {
		pthread_cond_wait(&g_cond_remove, &g_mutex_queue);
	}

    item = steque_pop(g_queue);
    pthread_mutex_unlock(&g_mutex_queue);

    return (request_item_t*) item;
}

/**
 * function to be executed when a thread is available
 */
static ssize_t execute_thread (gfcontext_t *ctx, char *path){
    int fildes;
    ssize_t file_len, bytes_transferred;
    ssize_t read_len, write_len;
    char buffer[BUFFER_SIZE];

    if( 0 > (fildes = content_get(path)))
        return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);

    /* Calculating the file size */
    file_len = lseek(fildes, 0, SEEK_END);

    gfs_sendheader(ctx, GF_OK, (size_t)file_len);

    /* Sending the file contents chunk by chunk. */
    bytes_transferred = 0;
    while(bytes_transferred < file_len){
        read_len = pread(fildes, buffer, BUFFER_SIZE, bytes_transferred);
        if (read_len <= 0){
            fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
            gfs_abort(ctx);
            return -1;
        }
        write_len = gfs_send(ctx, buffer, (size_t)read_len);
        if (write_len != read_len){
            fprintf(stderr, "handle_with_file write error");
            gfs_abort(ctx);
            return -1;
        }
        bytes_transferred += write_len;
    }

    return bytes_transferred;
}

static void *worker_thread (void *arg) {
    // endless loop to process work in the queue
    for ( ; ; ) {
        request_item_t *item = remove_item();
        execute_thread(item->ctx, item->path);
        free(item);
    }

    pthread_exit(NULL);
}

/**
 * function to initialize the worker thread pool
 */
void worker_threads_init (int nthreads) {
    int i;

    g_num_threads = nthreads;
    g_workers = (pthread_t*) malloc(g_num_threads * sizeof(pthread_t));

    // initialize
    // g_queue is used to pass work items to the worker thread
    g_queue = (steque_t*) malloc(sizeof(steque_t));
    steque_init(g_queue);

    // create the worker thread pool
    for (i = 0; i < g_num_threads; i++) {
        if (pthread_create(&g_workers[i], NULL, worker_thread, NULL) != 0) {
            fprintf(stderr, "Error creating thread");
            exit(1);
        }
    }
}

/**
 * boss thread
 */
ssize_t handler_get (gfcontext_t *ctx, char *path, void* arg){
    request_item_t* req;
    req = malloc(sizeof(request_item_t));

    req->ctx = ctx;
    memcpy(req->path, path, strlen(path));

    add_item(req);

    return 0;
}
