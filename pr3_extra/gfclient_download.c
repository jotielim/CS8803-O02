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

typedef struct client_requests_t {
    int nrequests;
    char* server;
    unsigned short port;
} client_requests_t;

static pthread_mutex_t mutex_get_path = PTHREAD_MUTEX_INITIALIZER;

static void Usage() {
    fprintf(stdout, "%s", USAGE);
}

static void localPath(char *req_path, char *local_path){
    // static int counter = 0;
    // sprintf(local_path, "%s-%06d", &req_path[1], counter++);
    sprintf(local_path, "%s", &req_path[1]);
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

/* Thread func ======================================================= */
static void* request_thread(void *arg) {
    int i;
    gfcrequest_t *gfr;
    FILE *file;
    char *req_path;
    char local_path[512];
    int returncode;
    client_requests_t* req;

    // get the passed arguments
    req = (client_requests_t*) arg;

    /*Making the requests...*/
    for(i = 0; i < req->nrequests; i++){
        pthread_mutex_lock(&mutex_get_path);
        req_path = workload_get_path();
        pthread_mutex_unlock(&mutex_get_path);

        if(strlen(req_path) > 256){
            fprintf(stderr, "Request path exceeded maximum of 256 characters\n.");
            exit(EXIT_FAILURE);
        }

        localPath(req_path, local_path);

        file = openFile(local_path);

        gfr = gfc_create();
        gfc_set_server(gfr, req->server);
        gfc_set_path(gfr, req_path);
        gfc_set_port(gfr, req->port);
        gfc_set_writefunc(gfr, writecb);
        gfc_set_writearg(gfr, file);

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

        gfc_cleanup(gfr);

    }

    pthread_exit(NULL);
}

/* Main ========================================================= */
int main(int argc, char **argv) {
/* COMMAND LINE OPTIONS ============================================= */
    char *server = "localhost";
    unsigned short port = 8888;
    char *workload_path = "workload.txt";
    client_requests_t req;
    pthread_t *client_workers;

    int i;
    int option_char = 0;
    int nrequests = 10;
    int nthreads = 1;
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

    // variables to be used in request_thread
    req.nrequests = nrequests;
    req.server = server;
    req.port = port;

    client_workers = (pthread_t*) malloc(nthreads * sizeof(pthread_t));

    // create the threads
    for(i=0; i<nthreads; i++) {
        if (pthread_create(&client_workers[i], NULL, request_thread, &req) != 0) {
            fprintf(stderr, "Error creating thread");
            exit(1);
        }
    }

    // join all threads
    for (i=0; i<nthreads; i++) {
        pthread_join(client_workers[i], &rc);
    }

    gfc_global_cleanup();

    return 0;
}
