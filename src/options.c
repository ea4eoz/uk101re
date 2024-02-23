//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Lesser General Public License as
//    published by the Free Software Foundation, either version 3 of the
//    License, or (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful, but
//    WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this program.  If not, see
//    <http://www.gnu.org/licenses/>
//

#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "options.h"

uk101re_options options;



static void show_banner(FILE *f){
    fprintf(f, "\n");
    fprintf(f, "UK101RE: Micro UK101 Replica Emulator version 1.00\n");
    fprintf(f, "\n");
}



static void show_help(FILE *f){
    fprintf(f, "Usage:\n");
    fprintf(f, "\n");
    fprintf(f, "  uk101re [options] [datafile]\n");
    fprintf(f, "\n");
    fprintf(f, "Options:\n");
    fprintf(f, "\n");
    fprintf(f, "  -h,         --help          Show this help.\n");
    fprintf(f, "  -v,         --version       Show UK101RE version.\n");
    fprintf(f, "  -t,         --turbo         Enable turbo mode.\n");
    fprintf(f, "  -r romfile, --rom romfile   Specify ROM file.\n");
    fprintf(f, "\n");
    fprintf(f, "Keyboard shortcuts:\n");
    fprintf(f, "\n");
    fprintf(f, "  Ctrl-C      Quits emulator.\n");
    fprintf(f, "  Ctrl-R      Resets 6502 CPU.\n");
    fprintf(f, "\n");
}



static void set_default_options(void){
    options.flag_help = 0;
    options.flag_turbo = 0;
    options.flag_datafile = 0;
    options.datafile = NULL;
    options.romfile = "all.rom";
}



void parse_options(int argc, char *argv[]){
    int ch;
    opterr = 0;  // We handle getopt errors
    
    struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {"turbo", no_argument, NULL, 't'},
        {"rom", required_argument, NULL, 'r'},
        {0, 0, 0, 0}
    };
    
    set_default_options();
    
    while ((ch = getopt_long(argc, argv, ":hvtr:", long_options, NULL)) != -1) {
        switch (ch) {
            case 'h':
                show_help(stdout);
                exit(EXIT_SUCCESS);
                break;
                
            case 'r':
                options.romfile = optarg;
                break;

            case 't':
                options.flag_turbo = 1;
                break;
                
            case 'v':
                show_banner(stdout);
                exit(EXIT_SUCCESS);
                break;

            case ':':
                fprintf(stderr, "Error: option %c needs an argument\n\n", optopt);
                show_banner(stderr);
                show_help(stderr);
                exit(EXIT_FAILURE);
                break;
                
            case '?':
                fprintf(stderr, "Error: unknown option %c\n\n", optopt);
                show_banner(stderr);
                show_help(stderr);
                exit(EXIT_FAILURE);
                break;
        }
    }
    
    // Remaining argument
    if (optind < argc){
        options.flag_datafile = 1;
        options.datafile = argv[optind];
    }
}
