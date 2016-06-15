#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>

#include "workload.h"
#include "gfclient.h"
#include "steque.h"

#define REQSIZE 256

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webclient [options]\n"                                                     \
"options:\n"                                                                  \
"  -s [server_addr]    Server address (Default: 0.0.0.0)\n"                   \
"  -p [server_port]    Server port (Default: 8888)\n"                         \
"  -w [workload_path]  Path to workload file (Default: workload.txt)\n"       \
"  -t [nthreads]       Number of threads (Default 1)\n"                       \
"  -n [num_requests]   Requests download per thread (Default: 1)\n"           \
"  -h                  Show this help message\n"                              \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
        {"server",        required_argument,      NULL,           's'},
        {"port",          required_argument,      NULL,           'p'},
        {"workload-path", required_argument,      NULL,           'w'},
        {"nthreads",      required_argument,      NULL,           't'},
        {"nrequests",     required_argument,      NULL,           'n'},
        {"help",          no_argument,            NULL,           'h'},
        {NULL,            0,                      NULL,             0}
};

typedef struct context_t {
    char *server;
    unsigned short port;
} context_t;

typedef struct request_item_t {
    context_t *ctx;
    char path[REQSIZE];
} request_item_t;

/**
 * global variables
 */
static int g_client_num_threads;
static steque_t *g_client_queue;
static pthread_t *g_client_workers;
static pthread_mutex_t g_client_mutex_queue = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_client_cond_remove = PTHREAD_COND_INITIALIZER;

static void Usage() {
    fprintf(stdout, "%s", USAGE);
}

static void localPath(char *req_path, char *local_path){
    static int counter = 0;

    sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE* openFile(char *path){
    char *cur, *prev;
    FILE *ans;

    /* Make the directory if it isn't there */
    prev = path;
    while(NULL != (cur = strchr(prev+1, '/'))){
        *cur = '\0';

        if (0 > mkdir(&path[0], S_IRWXU)){
            if (errno != EEXIST){
                perror("Unable to create directory");
                exit(EXIT_FAILURE);
            }
        }

        *cur = '/';
        prev = cur;
    }

    if( NULL == (ans = fopen(&path[0], "w"))){
        perror("Unable to open file");
        exit(EXIT_FAILURE);
    }

    return ans;
}

/* Callbacks ========================================================= */
static void writecb(void* data, size_t data_len, void *arg){
    FILE *file = (FILE*) arg;

    fwrite(data, 1, data_len, file);
}

/**
 * function to add item to the bottom of the queue
 */
static void add_item (context_t *ctx, char *path) {
    request_item_t *req = malloc(sizeof(request_item_t));

    req->ctx = ctx;
    memcpy(req->path, path, strlen(path));

    pthread_mutex_lock(&g_client_mutex_queue);
    steque_enqueue(g_client_queue, (steque_item)req);
    pthread_mutex_unlock(&g_client_mutex_queue);

    pthread_cond_signal(&g_client_cond_remove);
}

/**
 * function to remove item from the top of the queue and return the removed item
 */
static request_item_t* remove_item () {
    steque_item item;

    pthread_mutex_lock(&g_client_mutex_queue);

    // wait until there is content to remove
    while (steque_isempty(g_client_queue)) {
        pthread_cond_wait(&g_client_cond_remove, &g_client_mutex_queue);
    }

    item = steque_pop(g_client_queue);
    pthread_mutex_unlock(&g_client_mutex_queue);

    return (request_item_t*) item;
}

/**
 * function to be executed when a thread is available
 */
static void execute_thread (context_t *ctx, char *req_path) {
    int returncode;
    gfcrequest_t *gfr;
    FILE *file;
    char local_path[512];

    localPath(req_path, local_path);

    file = openFile(local_path);

    gfr = gfc_create();
    gfc_set_server(gfr, ctx->server);
    gfc_set_path(gfr, req_path);
    gfc_set_port(gfr, ctx->port);
    gfc_set_writefunc(gfr, writecb);
    gfc_set_writearg(gfr, file);

    fprintf(stdout, "Requesting %s%s\n", ctx->server, req_path);

    if ( 0 > (returncode = gfc_perform(gfr))){
        fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
        fclose(file);
        if ( 0 > unlink(local_path))
            fprintf(stderr, "unlink failed on %s\n", local_path);
    }
    else {
        fclose(file);
    }

    if ( gfc_get_status(gfr) != GF_OK){
        if ( 0 > unlink(local_path))
            fprintf(stderr, "unlink failed on %s\n", local_path);
    }

    fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(gfr)));
    fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(gfr), gfc_get_filelen(gfr));

    gfc_cleanup(gfr);
}

static void *request_thread (void *arg) {
    while (!steque_isempty(g_client_queue)) {
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

    g_client_num_threads = nthreads;
    g_client_workers = (pthread_t*) malloc(g_client_num_threads * sizeof(pthread_t));

    // initialize
    // g_client_queue is used to pass work items to the worker thread
    g_client_queue = (steque_t*) malloc(sizeof(steque_t));
    steque_init(g_client_queue);

    // create the worker thread pool
    for (i = 0; i < g_client_num_threads; i++) {
        if (pthread_create(&g_client_workers[i], NULL, request_thread, NULL) != 0) {
            fprintf(stderr, "Error creating thread");
            exit(1);
        }
    }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
/* COMMAND LINE OPTIONS ============================================= */
    char *server = "localhost";
    unsigned short port = 8888;
    char *workload_path = "workload.txt";

    int i;
    int option_char = 0;
    int nrequests = 1;
    int nthreads = 1;
    context_t *ctx = (context_t *)malloc(sizeof(context_t));
    char *req_path;
    void *rc;

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "s:p:w:n:t:h", gLongOptions, NULL)) != -1) {
        switch (option_char) {
            case 's': // server
                server = optarg;
                break;
            case 'p': // port
                port = atoi(optarg);
                break;
            case 'w': // workload-path
                workload_path = optarg;
                break;
            case 'n': // nrequests
                nrequests = atoi(optarg);
                break;
            case 't': // nthreads
                nthreads = atoi(optarg);
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

    if( EXIT_SUCCESS != workload_init(workload_path)){
        fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
        exit(EXIT_FAILURE);
    }

    gfc_global_init();

    // initialize the thread
    worker_threads_init(nthreads);

    ctx->server = server;
    ctx->port = port;

    /*Making the requests...*/
    for(i = 0; i < nrequests * nthreads; i++){
        req_path = workload_get_path();

        if(strlen(req_path) > 256){
            fprintf(stderr, "Request path exceeded maximum of 256 characters\n.");
            exit(EXIT_FAILURE);
        }

        // boss thread add_item() to the pool
        add_item(ctx, req_path);
    }

    for(i = 0; i < nthreads; i++) {
        pthread_join(g_client_workers[i], &rc);
    }

    gfc_global_cleanup();
    free(g_client_workers);
    free(g_client_queue);

    return 0;
}
