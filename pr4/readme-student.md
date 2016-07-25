# Project 4 README file

## Project Description
### Summary
In this project, we were working towards implementing Remote Procedure Call (RPC).

First, we specify our IDL in minifyjpeg.x. Since we were creating an RPC that took image as an input and output,
we created a single struct called `image_t`. This struct contained opaque data type that represented uninterpreted
binary data. We used the variable-length opaque encoding without specifying the length. This would translate into
2**32 (two to the power of 32) bytes. Then we executed `rpcgen -MN minifyjpeg.x` to generate the bare minimum
scaffolding for RPC.

On the server end, we implemented the server-side function minifyjpeg_proc_1_svc as a wrapper to minify the images.
This function took image_t argument and pointer to image_t result. We minified the image from image_t argument and
store it in the result pointer.

On the client end, we implemented `get_minify_client` to establish TCP connection to the server. We also updated
the timeout to be 5 seconds instead of the default 25 seconds. We also implemented `minify_via_rpc` that will construct
the argument and allocate some memory that will be required when invoking the RPC call. When the status is RPC_SUCCESS,
we return the minified image and freed the result variable that was previously allocated.

### Observations
- Trying to make the program work for large file was difficult for me because the error occur only with ASAN. I can't
reproduce it when disabling ASAN and use valgrind.
- I were to do this project again, I would improve it by fixing the bug with large files and implement multithreading
for the server. I also would like to integrate the GETFILE proxy server.

## Known Bugs/Issues/Limitations
- This project currently does not pass the large file test.
  - With ASAN, I received the following error locally:
  -     ERROR: AddressSanitizer failed to allocate 0xef000 (978944) bytes of LargeMmapAllocator: Cannot allocate memory
  - Without ASAN, I can minify large images successfully.
- This project does not support multithreading yet.

## References
- P4L1 Videos
- <http://www.cprogramming.com/tutorial/rpc/remote_procedure_call_start.html>
- <http://www.cs.rutgers.edu/~pxk/rutgers/notes/rpc/>
- <https://docs.oracle.com/cd/E19683-01/816-1435/rpcgenpguide-21470/index.html>
- <https://www.redhat.com/archives/redhat-list/2004-June/msg00439.html>
- <http://lxer.com/module/newswire/view/14311/index.html>
- <http://bderzhavets.blogspot.com/2005/11/multithreaded-rpc-server-in-white-box.html>
