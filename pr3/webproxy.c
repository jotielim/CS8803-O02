#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <curl/curl.h>
#include <semaphore.h>

#include "gfserver.h"
#include "shm_channel.h"

#define USAGE                                                                   \
"usage:\n"                                                                      \
"  webproxy [options]\n"                                                        \
"options:\n"                                                                    \
"  -n [nsegemtns]      Number of segments to use in communication with cache\n" \
"  -z [segsize]        The size (in bytes) of the segments.\n"                  \
"  -p [listen_port]    Listen port (Default: 8888)\n"                           \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"        \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)" \
"  -h                  Show this help message\n"                                \
"special options:\n"                                                            \
"  -d [drop_factor]    Drop connects if f*t pending requests (Default: 5).\n"


/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
        {"nsegments",     required_argument,      NULL,           'n'},
        {"size",          required_argument,      NULL,           'z'},
        {"port",          required_argument,      NULL,           'p'},
        {"thread-count",  required_argument,      NULL,           't'},
        {"server",        required_argument,      NULL,           's'},
        {"help",          no_argument,            NULL,           'h'},
        {NULL,            0,                      NULL,             0}
};

extern ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg);
extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);

static gfserver_t gfs;

static void _sig_handler(int signo){
    if (signo == SIGINT || signo == SIGTERM){
        gfserver_stop(&gfs);

        // clean up curl
        curl_global_cleanup();

        proxy_ipc_destroy();
        shm_destroy();

        exit(signo);
    }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
    int i, option_char = 0;
    int nsegments = 1;
    int segsize = 32;
    unsigned short port = 8888;
    unsigned short nworkerthreads = 1;
    char *server = "s3.amazonaws.com/content.udacity-data.com";

    if (signal(SIGINT, _sig_handler) == SIG_ERR){
        fprintf(stderr,"Can't catch SIGINT...exiting.\n");
        exit(SERVER_FAILURE);
    }

    if (signal(SIGTERM, _sig_handler) == SIG_ERR){
        fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
        exit(SERVER_FAILURE);
    }

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "n:z:p:t:s:h", gLongOptions, NULL)) != -1) {
        switch (option_char) {
            case 'n': // number of segments
                nsegments = atoi(optarg);
                break;
            case 'z': // size of segments
                segsize = atoi(optarg);
                break;
            case 'p': // listen-port
                port = atoi(optarg);
                break;
            case 't': // thread-count
                nworkerthreads = atoi(optarg);
                break;
            case 's': // file-path
                server = optarg;
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

    if (!server) {
        fprintf(stderr, "Invalid (null) server name\n");
        exit(1);
    }

    // initialize curl
    curl_global_init(CURL_GLOBAL_ALL);

    /* SHM initialization...*/
    shm_init();
    proxy_ipc_init(nsegments, segsize);

    /*Initializing server*/
    gfserver_init(&gfs, nworkerthreads);

    /*Setting options*/
    gfserver_setopt(&gfs, GFS_PORT, port);
    gfserver_setopt(&gfs, GFS_MAXNPENDING, 10);
    gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);
    for(i = 0; i < nworkerthreads; i++)
        gfserver_setopt(&gfs, GFS_WORKER_ARG, i, server);

    /*Loops forever*/
    gfserver_serve(&gfs);
}
