#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"

//Replace with an implementation of handle_with_curl and any other
//functions you may need.

typedef struct memory_struct_t {
    char *memory;
    size_t size;
} memory_struct_t;

static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    memory_struct_t *mem = (memory_struct_t *)userp;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);

    if (mem->memory == NULL) {
        // out of memory!
        printf("not enough memory (realloc returned NULL)\n");
        return SERVER_FAILURE;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg){
    size_t bytes_transferred = 0;
    ssize_t write_len;
    char buffer[4096];
    char *data_dir = arg;
    CURL *curl;
    CURLcode res;
    long http_code = 0;

    memory_struct_t response;
    response.memory = NULL;
    response.size = 0;

    strcpy(buffer,data_dir);
    strcat(buffer,path);

    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, buffer);
        // follow redirect
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

        // Perform the request, res will get the return code
        res = curl_easy_perform(curl);

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        // check for errors
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        if (http_code != 200) {
            // if http_code is not 200, then send FILE_NOT_FOUND
            return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
        }

        // sending the header
        gfs_sendheader(ctx, GF_OK, response.size);

        /* Sending the file contents chunk by chunk. */
        bytes_transferred = 0;
        while (bytes_transferred < response.size) {
            write_len = gfs_send(ctx, response.memory, response.size);
            if (write_len != response.size) {
                fprintf(stderr, "handle_with_curl write error");
                return SERVER_FAILURE;
            }
            bytes_transferred += write_len;
        }

        // clean up curl
        curl_easy_cleanup(curl);

        free(response.memory);
    }

    return bytes_transferred;
}

