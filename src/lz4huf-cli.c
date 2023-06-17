
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __linux__
    #include <unistd.h>
#endif

#include "liblz4huf.h"

#if defined __MSVCRT__
    #include <fcntl.h>
    #include <io.h>
#endif

#ifndef VERSION
    #define VERSION "vUNKNOWN"
#endif

#ifdef HAVE_GETOPT_LONG
    #include <getopt.h>
#else
    #include "getopt-shim.h"
#endif

static void version() {
    fprintf(stdout, "lz4huf " VERSION
                    "\n"
                    "Made by Kamila Szewczyk in 2023 using Yann Collet's LZ4 and Huff0.\n"
                    "CLI: CC0; LZ4, Huff0: BSD License\n");
}

static void help() {
    fprintf(stdout,
            "lz4huf - fusion of a fast LZ codec (LZ4) and a fast entropy coder (Huff0).\n"
            "Usage: lz4huf [-e/-z/-d/-t/-h/-V/-1..-12] [-j jobs] files...\n"
            "Operations:\n"
            "  -e/-z, --encode   compress data (default)\n"
            "  -d, --decode      decompress data\n"
            "  -h, --help        display an usage overview\n"
            "  -f, --force       force overwriting output if it already exists\n"
            "  -v, --verbose     verbose mode (display more information)\n"
            "  -V, --version     display version information\n"
            "  -p, --parallel    perform parallel compression/decompression\n"
            "  -1..-12           set compression level (default: 9)\n"
            "\n"
            "Examples:\n"
            "  lz4huf -zj0 < input > output  - creates `output` from `input`"
            "  lz4huf -j0 data.txt           - creates `data.txt.l4h` from `data.txt`"
            "  lz4huf -j0 file1 file2        - creates `file1.l4h` and `file2.l4h`"
            "\n"
            "Report bugs to: https://github.com/kspalaiologos/lz4huf\n");
}

static int is_numeric(const char * str) {
    for (; *str; str++)
        if (!isdigit(*str)) return 0;
    return 1;
}

static int is_dir(const char * path) {
    struct stat sb;
    if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) return 1;
    return 0;
}

enum { MODE_COMPRESS, MODE_EXPAND };

static void process(int mode, const char * in_name, FILE * input, FILE * output, int force,
                    int verbose, int jobs, int level) {
    if (mode == MODE_COMPRESS) {
        size_t total_read = 0, total_written = 0;
        if (jobs == 1) {
            size_t n_read = 0;
            char * buffer = malloc(32 * 1024 * 1024);
            if (!buffer) {
                fprintf(stderr, "lz4huf: memory exhausted\n");
                exit(1);
            }

            while ((n_read = fread(buffer, 1, 32 * 1024 * 1024, input)) > 0) {
                struct lz4huf_buffer b = lz4huf_compress(buffer, n_read, level);
                if (b.size == 0) {
                    fprintf(stderr, "lz4huf: compression failed\n");
                    exit(1);
                }
                if (fwrite(b.data, 1, b.size, output) != b.size) {
                    fprintf(stderr, "lz4huf: write error: %s\n", strerror(errno));
                    exit(1);
                }
                total_read += n_read;
                total_written += b.size;
                free(b.data);
            }

            free(buffer);
        } else {
            size_t n_read = 0;
            char * buffer = malloc(jobs * LZ4HUF_BS);
            if (!buffer) {
                fprintf(stderr, "lz4huf: memory exhausted\n");
                exit(1);
            }

            while ((n_read = fread(buffer, 1, jobs * LZ4HUF_BS, input)) > 0) {
                struct lz4huf_buffer b = lz4huf_compress_par(buffer, n_read, level);
                if (b.size == 0) {
                    fprintf(stderr, "lz4huf: compression failed\n");
                    exit(1);
                }
                if (fwrite(b.data, 1, b.size, output) != b.size) {
                    fprintf(stderr, "lz4huf: write error: %s\n", strerror(errno));
                    exit(1);
                }
                total_read += n_read;
                total_written += b.size;
                free(b.data);
            }

            free(buffer);
        }
        if (verbose) {
            fprintf(stderr, "%s\t%" PRIu64 " -> %" PRIu64 " bytes, %.2f%%, %.2f bpb\n", in_name, total_read, total_written,
                    (double)total_written * 100.0 / total_read, (double)total_written * 8.0 / total_read);
        }
    } else {
        size_t total_read = 0, total_written = 0;

        char * compressed = malloc(LZ4HUF_BS + 256);
        if (!compressed) {
            fprintf(stderr, "lz4huf: memory exhausted\n");
            exit(1);
        }

        // Loop on the blocks.
        while (!feof(input)) {
            uint32_t compressed_len = 0;

            // Read the compressed length.
            unsigned char num[4];
            size_t nread = fread(num, 1, 4, input);
            if (nread == 0) {
                break;
            }

            if (nread != 4) {
                fprintf(stderr, "lz4huf: read error: %s\n", strerror(errno));
                exit(1);
            }

            total_read += 4;

            compressed_len |= num[0] << 24;
            compressed_len |= num[1] << 16;
            compressed_len |= num[2] << 8;
            compressed_len |= num[3];

            if (compressed_len == 0) break;

            // Read the compressed data.
            if (fread(compressed, 1, compressed_len, input) != compressed_len) {
                fprintf(stderr, "lz4huf: read error: %s\n", strerror(errno));
                exit(1);
            }

            total_read += compressed_len;

            // Decompress the data.
            struct lz4huf_buffer b = lz4huf_decompress_blk(compressed, compressed_len);
            if (b.error) {
                fprintf(stderr, "lz4huf: decompression failed\n");
                exit(1);
            }

            // Write the decompressed data.
            if (fwrite(b.data, 1, b.size, output) != b.size) {
                fprintf(stderr, "lz4huf: write error: %s\n", strerror(errno));
                exit(1);
            }

            total_written += b.size;

            free(b.data);
        }

        free(compressed);

        if (verbose) {
            fprintf(stderr, "%s\t%" PRIu64 " <- %" PRIu64 " bytes, %.2f%%, %.2f bpb\n", in_name, total_written, total_read,
                    (double)total_read * 100.0 / total_written, (double)total_read * 8.0 / total_written);
        }
    }
}

static void close_out_file(FILE * des) {
    if (des) {
        int outfd = fileno(des);

        if (fflush(des)) {
            fprintf(stderr, "lz4huf: fflush failed: %s\n", strerror(errno));
            exit(1);
        }

#ifdef __linux__
        while (1) {
            int status = fsync(outfd);
            if (status == -1) {
                if (errno == EINVAL) break;
                if (errno == EINTR) continue;
                fprintf(stderr, "lz4huf: fsync failed: %s\n", strerror(errno));
                exit(1);
            }
            break;
        }
#endif

        if (des != stdout && fclose(des)) {
            fprintf(stderr, "lz4huf: fclose failed: %s\n", strerror(errno));
            exit(1);
        }
    }
}

int main(int argc, char * argv[]) {
    const char * short_options = "defhpvVz0123456789";
    static struct option long_options[] = { { "encode", no_argument, 0, 'e' },   { "decode", no_argument, 0, 'd' },
                                            { "force", no_argument, 0, 'f' },    { "help", no_argument, 0, 'h' },
                                            { "version", no_argument, 0, 'V' },  { "verbose", no_argument, 0, 'v' },
                                            { "parallel", no_argument, 0, 'p' }, { 0, 0, 0, 0 } };
    int mode = MODE_COMPRESS;
    int force = 0, verbose = 0, jobs = 1, level = 9;
    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, short_options, long_options, &option_index);
        if (c == -1) break;
        switch (c) {
            case 'e':
            case 'z':
                mode = MODE_COMPRESS;
                break;
            case 'd':
                mode = MODE_EXPAND;
                break;
            case 'f':
                force = 1;
                break;
            case 'h':
                help();
                return 0;
            case 'v':
                verbose = 1;
                break;
            case 'V':
                version();
                return 0;
            case 'p':
                jobs = omp_get_max_threads();
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                char * numarg = argv[optind - 1];
                if (numarg[0] == '-' && numarg[1] == c && numarg[2] == '\0') {
                    level = c - '0';
                } else if ((numarg = argv[optind]) != NULL && numarg[1] == c) {
                    char * ep;
                    int numoptind = optind;

                    level = -strtol(numarg, &ep, 10);
                    if (*ep != '\0') {
                        fprintf(stderr, "lz4huf: illegal number: %s\n", numarg);
                        return 1;
                    }

                    while (optind == numoptind) {
                        c = getopt(argc, argv, "0123456789");
                        assert(c >= '0' && c <= '9');
                    }
                } else {
                    fprintf(stderr, "lz4huf: number after other options: %s\n", numarg);
                    return 1;
                }
                break;
            default:
                fprintf(stderr, "Try `lz4huf --help` for more information.\n");
                return 1;
        }
    }

    if (level < 0 || level > 12) {
        fprintf(stderr, "lz4huf: invalid compression level: %d\n", level);
        return 1;
    }

#if defined(__MSVCRT__)
    setmode(STDIN_FILENO, O_BINARY);
    setmode(STDOUT_FILENO, O_BINARY);
#endif

    if (optind == argc) {
        // no files specified, use stdin/stdout
        process(mode, "stdin", stdin, stdout, force, verbose, jobs, level);
        close_out_file(stdout);
    } else {
        // process files
        for (int i = optind; i < argc; i++) {
            const char * filename = argv[i];
            if (is_dir(filename)) {
                fprintf(stderr, "lz4huf: %s is a directory, skipping", filename);
                continue;
            }

            FILE * input = fopen(filename, "rb");
            if (!input) {
                fprintf(stderr, "lz4huf: cannot open file %s for reading", filename);
                return 1;
            }

            char * output_filename = malloc(strlen(filename) + 5);
            strcpy(output_filename, filename);
            if (mode == MODE_COMPRESS) {
                strcat(output_filename, ".l4h");
            } else {
                if (strlen(filename) > 4 && strcmp(filename + strlen(filename) - 4, ".l4h") == 0) {
                    output_filename[strlen(filename) - 4] = '\0';
                } else {
                    fprintf(stderr, "lz4huf: cannot determine output filename for %s", filename);
                    return 1;
                }
            }

            FILE * output = fopen(output_filename, "rb");
            if (output) {
                if (!force) {
                    fprintf(stderr, "lz4huf: output file %s already exists, use -f to overwrite", output_filename);
                    return 1;
                }
                fclose(output);
            }

            if (is_dir(output_filename)) {
                fprintf(stderr, "lz4huf: output file %s is already directory.", output_filename);
                return 1;
            }

            output = fopen(output_filename, "wb");
            if (!output) {
                fprintf(stderr, "lz4huf: cannot open file %s for writing", output_filename);
                return 1;
            }

            process(mode, filename, input, output, force, verbose, jobs, level);

            close_out_file(output);
            fclose(input);
        }
    }

    return 0;
}
