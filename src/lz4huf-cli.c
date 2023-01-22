
#include "liblz4huf.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <omp.h>

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
                    "CLI: CC0, LZ4, Huff0: BSD License\n");
}

static void help() {
    fprintf(stdout,
            "lz4huf - fusion of a fast LZ codec (LZ4) and a fast entropy coder (Huff0).\n"
            "Usage: lz4huf [-e/-z/-d/-t/-h/-V] [-j jobs] files...\n"
            "Operations:\n"
            "  -e/-z, --encode   compress data (default)\n"
            "  -d, --decode      decompress data\n"
            "  -h, --help        display an usage overview\n"
            "  -f, --force       force overwriting output if it already exists\n"
            "  -v, --verbose     verbose mode (display more information)\n"
            "  -V, --version     display version information\n"
            "Extra flags:\n"
            "  -j N, --jobs=N    set the amount of parallel threads, set to 0 for automatic {1}\n"
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

static void process(int mode, FILE * input, FILE * output, int force, int verbose, int jobs) {
    // TODO...
}

int main(int argc, char * argv[]) {
    const char * short_options = "defhj:vVz";
    static struct option long_options[] = { { "encode", no_argument, 0, 'e' },
                                            { "decode", no_argument, 0, 'd' },
                                            { "force", no_argument, 0, 'f' },
                                            { "help", no_argument, 0, 'h' },
                                            { "version", no_argument, 0, 'V' },
                                            { "verbose", no_argument, 0, 'v' },
                                            { "jobs", required_argument, 0, 'j' },
                                            { 0, 0, 0, 0 } };
    int mode = MODE_COMPRESS;
    int force = 0, verbose = 0, jobs = 1;
    while(1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, short_options, long_options, &option_index);
        if(c == -1) break;
        switch(c) {
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
            case 'j':
                if (!is_numeric(optarg) || atoi(optarg) < 0) {
                    fprintf(stderr, "lz4huf: invalid amount of jobs: %s\n", optarg);
                    return 1;
                }
                jobs = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Try `lz4huf --help` for more information.\n");
                return 1;
        }
    }

#if defined(__MSVCRT__)
    setmode(STDIN_FILENO, O_BINARY);
    setmode(STDOUT_FILENO, O_BINARY);
#endif

    if(jobs == 0)
        jobs = omp_get_max_threads();
    
    if (optind == argc) {
        // no files specified, use stdin/stdout
        process(mode, stdin, stdout, force, verbose, jobs);
    } else {
        // process files
        for (int i = optind; i < argc; i++) {
            const char * filename = argv[i];
            if(is_dir(filename)) {
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

            if(is_dir(output_filename)) {
                fprintf(stderr, "lz4huf: output file %s is already directory.", output_filename);
                return 1;
            }

            output = fopen(output_filename, "wb");
            if (!output) {
                fprintf(stderr, "lz4huf: cannot open file %s for writing", output_filename);
                return 1;
            }

            process(mode, input, output, force, verbose, jobs);
        }
    }

    return 0;
}
