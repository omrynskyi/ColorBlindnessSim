#include "bmp.h"
#include "io.h"

#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_help(void);

int main(int argc, char *argv[]) {
    Buffer *infile = NULL;
    Buffer *outfile = NULL;

    opterr = 0;
    int option;
    while ((option = getopt(argc, argv, "i:o:h")) != -1) {
        switch (option) {
        case 'i':
            infile = read_open(optarg);
            if (infile == NULL) {
                fprintf(stderr, "Error reading input file %s\n", optarg);
                return 0;
            }
            break;
        case 'o':
            outfile = write_open(optarg);
            if (outfile == NULL) {
                fprintf(stderr, "Error opening output file %s\n", optarg);
                print_help();
                return 0;
            }

            break;
        case 'h': print_help(); return 0;
        default: fprintf(stderr, "colorb:  -i option is required\n"); return 1;
        }
    }

    BMP *image = bmp_create(infile);

    bmp_reduce_palette(image);

    bmp_write(image, outfile);

    bmp_free(&image);
}

void print_help(void) {
    fprintf(stdout, "Usage: colorb [options]\n\n");
    fprintf(stdout, "  -i <input_file>   Set the input file\n");
    fprintf(stdout, "  -o <output_file>  Set the output file\n");
    fprintf(stdout, "  -h                Print help message\n");
}
