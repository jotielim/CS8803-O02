#include "minifyjpeg.h"
#include "magickminify.h"

/* Implement the needed server-side functions here */
bool_t
minifyjpeg_proc_1_svc(image_t arg1, image_t *result,  struct svc_req *rqstp)
{
    // initialize magickminify
    magickminify_init();
    ssize_t dst_len;

    // store the returned value of magickminify to the result
    result->image.image_val = magickminify(arg1.image.image_val, arg1.image.image_len, &dst_len);
    // get the image length after it is minified
    result->image.image_len = dst_len;

    // clean up magickminify
    magickminify_cleanup();

    return TRUE;
}

int
minifyjpeg_prog_1_freeresult (SVCXPRT *transp, xdrproc_t xdr_result, caddr_t result)
{
    xdr_free (xdr_result, result);

    /*
     * Insert additional freeing code here, if needed
     */

    return 1;
}
