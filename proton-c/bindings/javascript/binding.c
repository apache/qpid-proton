
#include <stdio.h>
#include <stdlib.h>

/*
#include "proton/message.h"

typedef struct {
  size_t next;
  size_t prev;
  size_t down;
  size_t parent;
  size_t children;
  pn_atom_t atom;
  // for arrays
  bool described;
  pn_type_t type;
  bool data;
  size_t data_offset;
  size_t data_size;
  char *start;
  bool small;
} pni_node_t;

pni_node_t* pn_data_add(pn_data_t *data);

int test(pn_data_t *data, int64_t l)
{
printf("hello\n");

  pni_node_t *node = pn_data_add(data);
  node->atom.type = PN_LONG;
  node->atom.u.as_long = l;

  return 0;
}
*/





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
