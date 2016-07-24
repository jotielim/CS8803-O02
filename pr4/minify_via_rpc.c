#include <stdlib.h>
#include <stdio.h>
#include "minifyjpeg_xdr.c"
#include "minifyjpeg_clnt.c"

CLIENT* get_minify_client(char *server){
    CLIENT *cl;

    /* Your code here */
    // establish tcp connection
    cl = clnt_create(server, MINIFYJPEG_PROG, MINIFYJPEG_VERS, "tcp");
    if (cl == NULL) {
        clnt_pcreateerror(server);
        exit(1);
    }

    return cl;
}


void* minify_via_rpc(CLIENT *cl, void* src_val, size_t src_len, size_t *dst_len){

	/*Your code here */
    image_t minifyjpeg_1_arg;
    // allocate memory for the result
    image_t *result = malloc(sizeof(*result));
    enum clnt_stat rpc_response;

    minifyjpeg_1_arg.image.image_val = src_val;
    minifyjpeg_1_arg.image.image_len = src_len;

    // RPC call to minify image passing result pointer as argument;
    rpc_response = minifyjpeg_proc_1(minifyjpeg_1_arg, result, cl);

    if (rpc_response != RPC_SUCCESS) {
        clnt_perror(cl, "call failed");
        return NULL;
    }

    if (result == NULL) {
        clnt_perror(cl, "result is null");
        return NULL;
    }

    // assign the returned image length to dst_len pointer;
    *dst_len = result->image.image_len;

    // return the modified image
    return result->image.image_val;
}