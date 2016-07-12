#ifndef _SHM_CHANNEL_H_
#define _SHM_CHANNEL_H_

#define DEFAULT_SEGMENT_SIZE 1024
#define FILE_NOT_FOUND -1
#define IPC_ERROR -1
#define SHM_ERROR -1

#define QUEUE_NAME "/my-cache-name"
#define QUEUE_PERMISSIONS 0644
#define SHM_NAME "/my-shm-name-%d"
#define SHM_PERMISSIONS 0644

typedef struct {
    char key[128];
    char shmname[20];
    int segsize;
} cache_request_t;

typedef struct {
    sem_t empty;
    sem_t full;
    int filesize;
    char data[1];
} *shm_data_t;

// initialize the queue of free shm
void shm_init();
// destroy the queue of free shm
void shm_destroy();
// add free shm to the end of the queue
void add_shm(int shmid);
// remove the shm from the top of the queue
int pop_shm();

// initialize the message queue in the proxy
int proxy_ipc_init(int nsegments, int segsize);
// destroy the message queue in the proxy
void proxy_ipc_destroy();

// initialize the message queue in the cache
int cache_mqueue_init();
// destroy the message queue in the cache
void cache_mqueue_destroy();

// initialize the shm in the cache
void cache_shm_init(char *shmname, int segsize, shm_data_t* shmptr);
// destroy the shm in the cache
void cache_shm_destroy(shm_data_t shmptr, int segsize);

// send request to the cache
int send_cache_request(cache_request_t *request);
// receive request from the proxy
int receive_cache_request(cache_request_t *request, size_t size);

// get the name of the POSIX shm
void get_shmname(int shmid, char* shmname, size_t length);
// get the size of each shm
int get_segsize();

#endif
