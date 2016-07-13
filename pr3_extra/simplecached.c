#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <semaphore.h>

#include "steque.h"
#include "shm_channel.h"
#include "simplecache.h"

#if !defined(CACHE_FAILURE)
#define CACHE_FAILURE (-1)
#endif // CACHE_FAILURE

#define THREAD_FAILURE -1
#define MAX_CACHE_REQUEST_LEN 256

static void _sig_handler(int signo){
    if (signo == SIGINT || signo == SIGTERM){
        /* Unlink IPC mechanisms here*/
        cache_mqueue_destroy();
        simplecache_destroy();

        exit(signo);
    }
}

// global variables
static pthread_t *g_workers;
static pthread_mutex_t mutex_queue = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_job = PTHREAD_COND_INITIALIZER;
static steque_t *g_queue;

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"      \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
        {"nthreads",           required_argument,      NULL,           't'},
        {"cachedir",           required_argument,      NULL,           'c'},
        {"help",               no_argument,            NULL,           'h'},
        {NULL,                 0,                      NULL,             0}
};

// initialize worker thread using boss worker pattern
void workerthread_init(int nthreads);

// callback for created thread
static void* cache_thread(void *arg);

// function to send FILE_NOT_FOUND
void send_cache_filenotfound(int segid, char *shmname, int segsize);

// function to send the file
void send_cache_file(int fildes, char *shmname, int segsize);

// add job to the queue
static void add_job(cache_request_t *request);

// remove first element from the queue
static cache_request_t* pop_job();

// serve the cache
int serve_cache();

void Usage() {
    fprintf(stdout, "%s", USAGE);
}

int main(int argc, char **argv) {
    int nthreads = 1;
    char *cachedir = "locals.txt";
    char option_char;


    while ((option_char = getopt_long(argc, argv, "t:c:h", gLongOptions, NULL)) != -1) {
        switch (option_char) {
            case 't': // thread-count
                nthreads = atoi(optarg);
                break;
            case 'c': //cache directory
                cachedir = optarg;
                break;
            case 'h': // help
                Usage();
                exit(0);
                break;
            default:
                Usage();
                exit(1);
        }
    }

    if ((nthreads < 1) || (nthreads>1024)) {
        nthreads = 1;
    }

    if (signal(SIGINT, _sig_handler) == SIG_ERR){
        fprintf(stderr,"Can't catch SIGINT...exiting.\n");
        exit(CACHE_FAILURE);
    }

    if (signal(SIGTERM, _sig_handler) == SIG_ERR){
        fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
        exit(CACHE_FAILURE);
    }

    /* Initializing the cache */
    simplecache_init(cachedir);

    //Your code here...
    // initialize the worker threads
    workerthread_init(nthreads);

    // initialize message queue to receive cache request
    cache_mqueue_init();

    // process requests from the message queue
    serve_cache();
}

void workerthread_init(int nthreads) {
    int i;

    g_queue = (steque_t*)malloc(sizeof(steque_t));
    steque_init(g_queue);

    g_workers = (pthread_t*)malloc(nthreads * sizeof(pthread_t));

    for (i = 0; i < nthreads; i++) {
        if (pthread_create(&g_workers[i], NULL, cache_thread, NULL) != 0) {
            fprintf(stderr, "Unable to create thread\n");
            exit(THREAD_FAILURE);
        }
    }
}

static void* cache_thread(void *arg) {
    int fildes;

    for (;;) {
        cache_request_t *item = pop_job();

        if ((fildes = simplecache_get(item->key)) == -1) {
            send_cache_filenotfound(fildes, item->shmname, item->segsize);
        } else {
            send_cache_file(fildes, item->shmname, item->segsize);
        }

        free(item);
    }

    pthread_exit(NULL);
}

void send_cache_filenotfound(int fildes, char *shmname, int segsize) {
    shm_data_t shmptr;
    cache_shm_init(shmname, segsize, &shmptr);

    // update filesize to FILE_NOT_FOUND
    sem_wait(&(shmptr->empty));
    shmptr->filesize = FILE_NOT_FOUND;
    sem_post(&(shmptr->full));

    cache_shm_destroy(shmptr, segsize);
}

void send_cache_file(int fildes, char *shmname, int segsize) {
    size_t filelen, buffsize, bytes_sent, readlen;

    // calculate the size of data we can put into the buffer
    buffsize = segsize - (2 * sizeof(sem_t)) - sizeof(int);

    // calculate the file size
    filelen = lseek(fildes, 0, SEEK_END);
    lseek(fildes, 0, SEEK_SET);

    shm_data_t shmptr;
    cache_shm_init(shmname, segsize, &shmptr);

    // send the file size
    sem_wait(&(shmptr->empty));
    shmptr->filesize = filelen;
    sem_post(&(shmptr->full));

    // send the contents
    bytes_sent = 0;
    while (bytes_sent < filelen) {
        sem_wait(&(shmptr->empty));

        readlen = pread(fildes, &(shmptr->data), buffsize, bytes_sent);
        if (readlen <= 0) {
            fprintf(stderr, "Unable to read cached file, %zd, %zu, %zu\n", readlen, bytes_sent, filelen);
            exit(CACHE_FAILURE);
        }
        sem_post(&(shmptr->full));
        bytes_sent += readlen;
    }

    cache_shm_destroy(shmptr, segsize);
}

static void add_job(cache_request_t *request) {
    // lock mutex
    pthread_mutex_lock(&mutex_queue);
    // add request to the end of queue
    steque_enqueue(g_queue, (steque_item)request);
    // unlock mutex
    pthread_mutex_unlock(&mutex_queue);

    // signal the job condition
    pthread_cond_signal(&cond_job);
}

static cache_request_t* pop_job() {
    steque_item item;

    // lock mutex
    pthread_mutex_lock(&mutex_queue);
    // wait until queue is not empty
    while (steque_isempty(g_queue)) {
        pthread_cond_wait(&cond_job, &mutex_queue);
    }
    // remove the first element in the queue
    item = steque_pop(g_queue);
    // unlock mutex
    pthread_mutex_unlock(&mutex_queue);

    return (cache_request_t*)item;
}

int serve_cache() {
    for (;;) {
        cache_request_t *request;
        request = malloc(sizeof(cache_request_t));

        if (receive_cache_request(request, sizeof(cache_request_t)) == -1) {
            fprintf(stderr, "Unable to receive the requested cache\n");
            free(request);
        } else {
            add_job(request);
        }
    }

    return 0;
}
