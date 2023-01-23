
#ifndef _LZ4HUF_H
#define _LZ4HUF_H

#define LZ4HUF_BS (128 * 1024)

#ifndef LZ4HUF_PUBLIC_API
    #define LZ4HUF_PUBLIC_API __attribute__((visibility("default")))
#endif

#include <stdint.h>

/**
 * @brief A buffer returned by the compression functions.
 */
struct lz4huf_buffer {
    /**
     * @brief The error code. 0 if no error.
     */
    uint8_t error;

    /**
     * @brief The data buffer.
     */
    uint8_t * data;

    /**
     * @brief The size of the data buffer.
     */
    int32_t size;
};

/**
 * @brief Compresses a buffer using LZ4 and Huffman encoding.
 *
 * @param src The source buffer.
 * @param src_size The size of the source buffer. Must not exceed LZ4HUF_BS.
 * @param level The compression level. Must be between 0 and 12.
 * @return struct lz4huf_buffer The compressed buffer.
 */
struct lz4huf_buffer lz4huf_compress_blk(const uint8_t * src, uint32_t src_size, uint8_t level);

/**
 * @brief Decompresses a buffer compressed with lz4huf_compress.
 *
 * @param src The source buffer.
 * @param src_size The size of the source buffer.
 * @return struct lz4huf_buffer The decompressed buffer.
 */
struct lz4huf_buffer lz4huf_decompress_blk(const uint8_t * src, uint32_t src_size);

/**
 * @brief Compresses a buffer of arbitrary size using LZ4 and Huffman encoding.
 *
 * @param src The source buffer.
 * @param src_size The size of the source buffer.
 * @param level The compression level. Must be between 0 and 12.
 * @return struct lz4huf_buffer The compressed buffer.
 */
struct lz4huf_buffer lz4huf_compress(const uint8_t * src, uint32_t src_size, uint8_t level);

/**
 * @brief Decompresses a buffer compressed with lz4huf_compress.
 *
 * @param src The source buffer.
 * @param src_size The size of the source buffer.
 * @return struct lz4huf_buffer The decompressed buffer.
 */
struct lz4huf_buffer lz4huf_decompress(const uint8_t * src, uint32_t src_size);

/**
 * @brief Compresses a buffer of arbitrary size in parallel using LZ4 and Huffman encoding.
 *
 * @param src The source buffer.
 * @param src_size The size of the source buffer.
 * @param level The compression level. Must be between 0 and 12.
 * @return struct lz4huf_buffer The compressed buffer.
 */
struct lz4huf_buffer lz4huf_compress_par(const uint8_t * src, uint32_t src_size, uint8_t level);

#endif
