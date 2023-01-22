
#include "liblz4huf.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

int main(int argc, char * argv[]) {
    FILE * in = fopen(argv[1], "rb");
    FILE * out = fopen(argv[2], "wb");
    size_t in_size;
    uint8_t * in_buf;
    struct lz4huf_buffer out_buf;

    fseek(in, 0, SEEK_END);
    in_size = ftell(in);
    fseek(in, 0, SEEK_SET);

    in_buf = malloc(in_size);
    fread(in_buf, 1, in_size, in);

    out_buf = lz4huf_compress(in_buf, in_size, 12);

    fwrite(out_buf.data, 1, out_buf.size, out);

    free(in_buf);
    free(out_buf.data);

    fclose(in);
    fclose(out);

    return 0;
}
