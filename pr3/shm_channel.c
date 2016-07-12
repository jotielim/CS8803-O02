#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mqueue.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>

#include "steque.h"
#include "shm_channel.h"

// globals
static mqd_t g_mqd;
static int g_nsegments;
static int g_segsize;

static pthread_mutex_t mutex_queue = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_job = PTHREAD_COND_INITIALIZER;

static steque_t *g_shm_queue;
shm_data_t *g_shm_array;

void shm_init() {
    g_shm_queue = (steque_t*)malloc(sizeof(steque_t));
    steque_init(g_shm_queue);
}

void shm_destroy() {
    steque_destroy(g_shm_queue);
    free(g_shm_queue);
}

void add_shm(int shmid) {
    int *shmid_ptr = malloc(sizeof(int));
    *shmid_ptr = shmid;
    // lock mutex
    pthread_mutex_lock(&mutex_queue);
    steque_enqueue(g_shm_queue, (steque_item)shmid_ptr);
    //unlock mutex
    pthread_mutex_unlock(&mutex_queue);

    pthread_cond_signal(&cond_job);
}

int pop_shm() {
    int shmid;
    steque_item item;

    // lock mutex
    pthread_mutex_lock(&mutex_queue);
    while (steque_isempty(g_shm_queue)) {
        pthread_cond_wait(&cond_job, &mutex_queue);
    }
    item = steque_pop(g_shm_queue);
    // unlock mutex
    pthread_mutex_unlock(&mutex_queue);

    shmid = *((int*)item);
    free((int*)item);
    return shmid;
}

int proxy_ipc_init(int nsegments, int segsize) {
    int i, fildes;
    char shmname[20];

    g_nsegments = nsegments;
    g_segsize = segsize;
    g_shm_array = malloc(g_nsegments * sizeof(shm_data_t));
    
    for (i = 0; i < g_nsegments; i++) {
        sprintf(shmname, SHM_NAME, i);
        
        if ((fildes = shm_open(shmname, O_RDWR | O_CREAT, SHM_PERMISSIONS)) == -1) {
            fprintf(stderr, "Unable to open shared memory %d\n", i);
            exit(IPC_ERROR);
        }
        
        if (ftruncate(fildes, g_segsize) == -1) {
            fprintf(stderr, "Unable to resize shared memory %d with size %d\n", fildes, g_segsize);
            exit(IPC_ERROR);
        }

        if ((g_shm_array[i] = mmap(NULL, g_segsize, PROT_READ | PROT_WRITE, MAP_SHARED, fildes, 0)) == MAP_FAILED) {
            fprintf(stderr, "Unable to map shared memory %d\n", i);
            exit(IPC_ERROR);
        }
        
        sem_init(&(g_shm_array[i]->empty), 1, 1);
        sem_init(&(g_shm_array[i]->full), 1, 0);
        
        add_shm(i);
    }
    
    // initialize the message queue. If it doesn't exist, sleep and wait
    while ((g_mqd = mq_open(QUEUE_NAME, O_WRONLY)) == -1) {
        sleep(1);
    }
    
    return 0;
}

void proxy_ipc_destroy() {
    if (mq_close(g_mqd) == -1) {
        fprintf(stderr, "Unable to close the message queue\n");
        exit(IPC_ERROR);
    }

    int i;
    char shmname[20];

    for (i = 0; i < g_nsegments; i++) {
        // destroy the semaphores
        sem_destroy(&(g_shm_array[i]->empty));
        sem_destroy(&(g_shm_array[i]->full));
        // unmap the shm segment
        munmap(g_shm_array[i], g_segsize);

        // unlink the shared memory segment
        sprintf(shmname, SHM_NAME, i);
        shm_unlink(shmname);
    }

    // free malloc
    free(g_shm_array);
}

int cache_mqueue_init() {
    struct mq_attr attr;

    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(cache_request_t);
    attr.mq_flags = 0;
    attr.mq_curmsgs = 0;

    if ((g_mqd = mq_open(QUEUE_NAME, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
        fprintf(stderr, "Unable to open message queue\n");
        exit(IPC_ERROR);
    }

    return 0;
}

void cache_mqueue_destroy() {
    if (mq_close(g_mqd) == -1) {
        fprintf(stderr, "Unable to close the message queue\n");
        exit(IPC_ERROR);
    }

    if (mq_unlink(QUEUE_NAME) == -1) {
        fprintf(stderr, "Unable to unlink the message queue\n");
        exit(IPC_ERROR);
    }
}

void cache_shm_init(char *shmname, int segsize, shm_data_t* shmptr) {
    int fildes;

    if ((fildes = shm_open(shmname, O_RDWR, SHM_PERMISSIONS)) == -1) {
        fprintf(stderr, "Unable to open shared memory %s\n", shmname);
        exit(SHM_ERROR);
    }

    if ((*shmptr = mmap(NULL, segsize, PROT_READ | PROT_WRITE, MAP_SHARED, fildes, 0)) == MAP_FAILED) {
        fprintf(stderr, "Unable to map shared memory %s\n", shmname);
        exit(SHM_ERROR);
    }
}

void cache_shm_destroy(shm_data_t shmptr, int segsize) {
    munmap(shmptr, segsize);
}

int send_cache_request(cache_request_t *request) {
    return mq_send(g_mqd, (char*)request, sizeof(cache_request_t), 0);
}

int receive_cache_request(cache_request_t *request, size_t size) {
    return mq_receive(g_mqd, (char *)request, size, NULL);
}

void get_shm_name(int shmid, char* shmname, size_t length) {
    sprintf(shmname, SHM_NAME, shmid);
}

int get_segsize() {
    return g_segsize;
}
