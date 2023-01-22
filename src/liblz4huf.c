
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "lz4hc.h"
#include "lz4.h"
#include "huf.h"
#include "liblz4huf.h"

// Wrapper functions over compression.

static struct lz4huf_buffer lz4_compress(const uint8_t * src, uint32_t src_size, uint8_t level) {
    uint32_t dst_capacity = LZ4_compressBound(src_size);
    uint8_t * dst = malloc(dst_capacity + sizeof(uint32_t));
    if(dst == NULL) {
        struct lz4huf_buffer buf;
        buf.error = 1;
        buf.data = NULL;
        buf.size = 0;
        return buf;
    }

    // Serialise the original size into the buffer.
    dst[0] = (src_size >> 24) & 0xFF;
    dst[1] = (src_size >> 16) & 0xFF;
    dst[2] = (src_size >> 8) & 0xFF;
    dst[3] = src_size & 0xFF;

    struct lz4huf_buffer buf;
    buf.error = 0;
    buf.data = dst;

    if (level < LZ4HC_CLEVEL_MIN) {
        buf.size = LZ4_compress_default(src, dst + sizeof(uint32_t), src_size, dst_capacity);
    } else {
        buf.size = LZ4_compress_HC(src, dst + sizeof(uint32_t), src_size, dst_capacity, level);
    }
    
    buf.size += sizeof(uint32_t);

    if(buf.size == 0) {
        buf.error = 1;
        free(buf.data);
        buf.data = NULL;
    }

    return buf;
}

static struct lz4huf_buffer lz4_decompress(const uint8_t * src, uint32_t src_size) {
    uint32_t dst_size = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];

    uint8_t * dst = malloc(dst_size);
    if(dst == NULL) {
        struct lz4huf_buffer buf;
        buf.error = 1;
        buf.data = NULL;
        buf.size = 0;
        return buf;
    }

    struct lz4huf_buffer buf;
    buf.error = 0;
    buf.data = dst;

    buf.size = LZ4_decompress_safe(src + sizeof(uint32_t), dst, src_size - sizeof(uint32_t), dst_size);
    if(buf.size == 0) {
        buf.error = 1;
        free(buf.data);
        buf.data = NULL;
    }

    return buf;
}

static struct lz4huf_buffer huf_compress(const uint8_t * src, uint32_t src_size, uint8_t level) {
    uint32_t dst_capacity = HUF_compressBound(src_size);
    uint8_t * dst = malloc(dst_capacity + 1 + sizeof(uint32_t));
    if(dst == NULL) {
        struct lz4huf_buffer buf;
        buf.error = 1;
        buf.data = NULL;
        buf.size = 0;
        return buf;
    }

    struct lz4huf_buffer buf;
    buf.error = 0;
    buf.data = dst;
    buf.size = dst_capacity;

    if(level >= 6) {
        buf.size = HUF_compress(dst + 1 + sizeof(uint32_t), dst_capacity, src, src_size);
        if(buf.size == 0) {
            // Assume that the data simply can't be compressed...
            // This requires 
            memcpy(dst + 1, src, src_size);
            buf.size = src_size;
            dst[0] = 0; // Uncompressed
        } else {
            dst[0] = 1; // Compressed
        }
    } else {
        // Store, don't compress.
        memcpy(dst + 1, src, src_size);
        buf.size = src_size;
        dst[0] = 0; // Uncompressed
    }

    // Serialise the original size into the buffer.
    dst[1] = (src_size >> 24) & 0xFF;
    dst[2] = (src_size >> 16) & 0xFF;
    dst[3] = (src_size >> 8) & 0xFF;
    dst[4] = src_size & 0xFF;

    return buf;
}

static struct lz4huf_buffer huf_decompress(const uint8_t * src, uint32_t src_size) {
    // Read the compressed flag and the original size.
    uint8_t compressed = src[0];
    uint32_t dst_size = (src[1] << 24) | (src[2] << 16) | (src[3] << 8) | src[4];

    uint8_t * dst = malloc(dst_size);
    if(dst == NULL) {
        struct lz4huf_buffer buf;
        buf.error = 1;
        buf.data = NULL;
        buf.size = 0;
        return buf;
    }

    struct lz4huf_buffer buf;
    buf.error = 0;
    buf.data = dst;
    buf.size = dst_size;

    if(compressed) {
        buf.size = HUF_decompress(dst, dst_size, src + 1 + sizeof(uint32_t), src_size - (1 + sizeof(uint32_t)));
        if(buf.size == 0) {
            buf.error = 1;
            free(buf.data);
            buf.data = NULL;
        }
    } else {
        memcpy(dst, src + 1, dst_size);
    }

    return buf;
}
