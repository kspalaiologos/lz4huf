
#include "liblz4huf.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "huf.h"
#include "lz4.h"
#include "lz4hc.h"

// Wrapper functions over compression.

static struct lz4huf_buffer lz4_compress(const uint8_t * src, uint32_t src_size, uint8_t level) {
    uint32_t dst_capacity = LZ4_compressBound(src_size);
    uint8_t * dst = malloc(dst_capacity + sizeof(uint32_t));
    if (dst == NULL) {
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

    if (buf.size <= 0) {
        buf.error = 1;
        free(buf.data);
        buf.data = NULL;
    }

    return buf;
}

static struct lz4huf_buffer lz4_decompress(const uint8_t * src, uint32_t src_size) {
    uint32_t dst_size = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];

    uint8_t * dst = malloc(dst_size);
    if (dst == NULL) {
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
    if (buf.size <= 0) {
        buf.error = 1;
        free(buf.data);
        buf.data = NULL;
    }

    return buf;
}

static struct lz4huf_buffer huf_compress(const uint8_t * src, uint32_t src_size, uint8_t level) {
    uint32_t dst_capacity = HUF_compressBound(src_size);
    uint8_t * dst = malloc(dst_capacity + 5);
    if (dst == NULL) {
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

    if (level >= 6) {
        buf.size = HUF_compress(dst + 5, dst_capacity, src, src_size);
        if (buf.size <= 0) {
            // Assume that the data simply can't be compressed...
            memcpy(dst + 1 + sizeof(uint32_t), src, src_size);
            buf.size = src_size;
            dst[0] = 0;  // Uncompressed
        } else {
            dst[0] = 1;  // Compressed
        }
    } else {
        // Store, don't compress.
        memcpy(dst + 1 + sizeof(uint32_t), src, src_size);
        buf.size = src_size;
        dst[0] = 0;  // Uncompressed
    }

    buf.size += 5;

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
    if (dst == NULL) {
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

    if (compressed) {
        buf.size = HUF_decompress(dst, dst_size, src + 5, src_size - 5);
        if (buf.size <= 0) {
            buf.error = 1;
            free(buf.data);
            buf.data = NULL;
        }
    } else {
        memcpy(dst, src + 5, dst_size);
    }

    return buf;
}

// Single block compression

LZ4HUF_PUBLIC_API struct lz4huf_buffer lz4huf_compress_blk(const uint8_t * src, uint32_t src_size, uint8_t level) {
    assert(src_size <= LZ4HUF_BS && level <= 12 && level > 0);

    struct lz4huf_buffer buf = lz4_compress(src, src_size, level);
    if (buf.error) {
        return buf;
    }

    struct lz4huf_buffer buf2 = huf_compress(buf.data, buf.size, level);
    free(buf.data);

    return buf2;
}

LZ4HUF_PUBLIC_API struct lz4huf_buffer lz4huf_decompress_blk(const uint8_t * src, uint32_t src_size) {
    struct lz4huf_buffer buf = huf_decompress(src, src_size);
    if (buf.error) {
        return buf;
    }

    struct lz4huf_buffer buf2 = lz4_decompress(buf.data, buf.size);
    free(buf.data);
    return buf2;
}

// Multi block compression

LZ4HUF_PUBLIC_API struct lz4huf_buffer lz4huf_compress(const uint8_t * src, uint32_t src_size, uint8_t level) {
    uint32_t num_blocks = (src_size + LZ4HUF_BS - 1) / LZ4HUF_BS;
    uint32_t dst_capacity = num_blocks * LZ4HUF_BS + num_blocks * sizeof(uint32_t);
    uint8_t * dst = malloc(dst_capacity);
    if (dst == NULL) {
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

    uint32_t out_ptr = 0;
    for (uint32_t i = 0; i < num_blocks; i++) {
        uint32_t block_size = LZ4HUF_BS;
        if (i == num_blocks - 1) {
            block_size = src_size - (num_blocks - 1) * LZ4HUF_BS;
        }

        struct lz4huf_buffer buf2 = lz4huf_compress_blk(src + i * LZ4HUF_BS, block_size, level);
        if (buf2.error) {
            buf.error = 1;
            free(buf.data);
            buf.data = NULL;
            return buf;
        }

        // Serialise the compressed len.
        dst[out_ptr++] = (buf2.size >> 24) & 0xFF;
        dst[out_ptr++] = (buf2.size >> 16) & 0xFF;
        dst[out_ptr++] = (buf2.size >> 8) & 0xFF;
        dst[out_ptr++] = buf2.size & 0xFF;

        memcpy(dst + out_ptr, buf2.data, buf2.size);
        free(buf2.data);
        out_ptr += buf2.size;
    }

    buf.size = out_ptr;

    return buf;
}

LZ4HUF_PUBLIC_API struct lz4huf_buffer lz4huf_decompress(const uint8_t * src, uint32_t src_size) {
    // Count the number of blocks.
    uint32_t num_blocks = 0, i;
    uint64_t in_ptr = 0;
    while (in_ptr < src_size) {
        uint32_t compressed_len = (src[in_ptr++] << 24) | (src[in_ptr++] << 16) | (src[in_ptr++] << 8) | src[in_ptr++];
        in_ptr += compressed_len;
        num_blocks++;
    }

    uint32_t dst_capacity = num_blocks * LZ4HUF_BS + 256;
    uint8_t * dst = malloc(dst_capacity);
    if (dst == NULL) {
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

    in_ptr = 0;
    uint32_t out_ptr = 0;
    for (uint32_t i = 0; i < num_blocks; i++) {
        uint32_t compressed_len = (src[in_ptr++] << 24) | (src[in_ptr++] << 16) | (src[in_ptr++] << 8) | src[in_ptr++];

        struct lz4huf_buffer buf2 = lz4huf_decompress_blk(src + in_ptr, compressed_len);
        if (buf2.error || buf2.size > LZ4HUF_BS) {
            buf.error = 1;
            free(buf.data);
            buf.data = NULL;
            return buf;
        }

        memcpy(dst + out_ptr, buf2.data, buf2.size);
        free(buf2.data);
        in_ptr += compressed_len;
        out_ptr += buf2.size;
    }

    buf.size = out_ptr;

    return buf;
}

// Parallel multi block compression using OpenMP.

LZ4HUF_PUBLIC_API struct lz4huf_buffer lz4huf_compress_par(const uint8_t * src, uint32_t src_size, uint8_t level) {
    int num_blocks = (src_size + LZ4HUF_BS - 1) / LZ4HUF_BS;
    struct lz4huf_buffer * bufs = malloc(num_blocks * sizeof(struct lz4huf_buffer));
    if (bufs == NULL) {
        struct lz4huf_buffer buf;
        buf.error = 1;
        buf.data = NULL;
        buf.size = 0;
        return buf;
    }

#pragma omp parallel for
    for (int i = 0; i < num_blocks; i++) {
        uint32_t block_size = LZ4HUF_BS;
        if (i == num_blocks - 1) {
            block_size = src_size - (num_blocks - 1) * LZ4HUF_BS;
        }

        bufs[i] = lz4huf_compress_blk(src + i * LZ4HUF_BS, block_size, level);
    }

    uint32_t dst_capacity = 0;
    for (int i = 0; i < num_blocks; i++) {
        if (bufs[i].error) {
            for (int j = 0; j < num_blocks; j++) {
                free(bufs[j].data);
            }
            free(bufs);
            struct lz4huf_buffer buf;
            buf.error = 1;
            buf.data = NULL;
            buf.size = 0;
            return buf;
        }

        dst_capacity += bufs[i].size + sizeof(uint32_t);
    }

    uint8_t * dst = malloc(dst_capacity);
    if (dst == NULL) {
        for (int j = 0; j < num_blocks; j++) {
            free(bufs[j].data);
        }
        free(bufs);
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

    uint32_t out_ptr = 0;

    for (int i = 0; i < num_blocks; i++) {
        // Serialise the compressed len.
        dst[out_ptr++] = (bufs[i].size >> 24) & 0xFF;
        dst[out_ptr++] = (bufs[i].size >> 16) & 0xFF;
        dst[out_ptr++] = (bufs[i].size >> 8) & 0xFF;
        dst[out_ptr++] = bufs[i].size & 0xFF;

        memcpy(dst + out_ptr, bufs[i].data, bufs[i].size);
        free(bufs[i].data);
        out_ptr += bufs[i].size;
    }

    free(bufs);

    buf.size = out_ptr;

    return buf;
}
