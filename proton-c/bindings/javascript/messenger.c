
#include <stdio.h>
#include <stdlib.h>



void test(const char *name) {
    if (name == NULL) {
        printf("name is NULL\n");
    } else {
        printf("name = %s\n", name);
    }
}



/*
z_streamp inflateInitialise() {
    z_streamp stream = malloc(sizeof(z_stream));
    stream->zalloc = Z_NULL;
    stream->zfree = Z_NULL;
    int ret = inflateInit(stream);
    if (ret != Z_OK) {
        return Z_NULL;
    } else {
        return stream;
    }
}

void inflateDestroy(z_streamp stream) {
    inflateEnd(stream);
    free(stream);
}

int zinflate(z_streamp stream,
             unsigned char* dest, unsigned long* destLen,
             unsigned char* source, unsigned long sourceLen) {
    int err;
    int total = stream->total_out;
    stream->avail_in = sourceLen;
    stream->next_in = source;

    stream->avail_out = *destLen;
    stream->next_out = dest;

    err = inflate(stream, Z_SYNC_FLUSH);
    *destLen = stream->total_out - total;

    if (err != Z_OK) {
        inflateEnd(stream);
    }
    return err;
}
*/
