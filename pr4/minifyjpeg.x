/*
 * Complete this file and run rpcgen -MN minifyjpeg.x
 */

struct image_t {
    opaque image<>;
};

program MINIFYJPEG_PROG {
    version MINIFYJPEG_VERS {
        image_t MINIFYJPEG_PROC(image_t) = 1;
    } = 1;
} = 0x31230000;
