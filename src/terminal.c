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

#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "options.h"
#include "terminal.h"
#include "timeutils.h"

// Timespec struct with 20 milliseconds
static const struct timespec stdin_polling_interval = {
    .tv_sec = 0,
    .tv_nsec = 20000000
};

static struct timespec last_char_timestamp, last_key_timestamp;
static struct termios oldt;
static int oldf;
static uint8_t last_key;
static volatile int stdin_value;
static volatile uint8_t stdin_has_data = 0;
static FILE *datafile;
static long datasize;

int ACTION = 0;



// Restore computer terminal to previous state
static void restore_terminal(void){
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
}



// Executed whenever the program exists AND terminal
// has been configured into RAW mode
static void exit_hook(void){
    printf("\n*** Ctrl-X ***\n");
    restore_terminal();    
}



// Polling the keyboard (stdin) as frequently as
// the 6502 CPU does in the UK101 produces an excesive
// CPU usage. Here we are limiting the polling interval
// (to stdin) to 20 ms
static void *stdin_handler(void *args){
    while(1){
        nanosleep(&stdin_polling_interval, NULL);
        int ch = getchar();
        if (ch != EOF){
            switch(ch){
                case CTRL_R:
                    ACTION = ACTION_RESET;
                    break;
                case CTRL_X:
                    exit(EXIT_SUCCESS);
                    break;
                default:
                    while(stdin_has_data){
                        nanosleep(&stdin_polling_interval, NULL);
                    };
                    stdin_value = ch;
                    stdin_has_data = 1;
                    break;
            }
        }
    }
    return NULL;
}



void configure_terminal(void){
    // If there is a datafile, open it, get its size, and rewind it.
    if(options.flag_datafile){
        datafile = fopen(options.datafile, "rb");
        if (datafile==NULL){
            fprintf(stderr, "Error: can't open %s\n", options.datafile);
            exit(EXIT_FAILURE);
        }
        fseek(datafile, 0L, SEEK_END);
        datasize = ftell(datafile);
        rewind(datafile);
    }

    // Configure the terminal for raw mode
    tcgetattr(STDIN_FILENO, &oldt);
    struct termios newt = oldt;
    newt.c_iflag &= ~(ICRNL | IXON);
    newt.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);    
    
    // Setup exit hook
    atexit(exit_hook);
    
    // Also initializes these variables
    clock_gettime(CLOCK_MONOTONIC, &last_char_timestamp);
    last_key_timestamp = last_char_timestamp;
    
    // and start the stdin processing thread
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, &stdin_handler, NULL);
}


// Checks if a key has been typed
uint8_t check_keyboard_ready(void){
    if (options.flag_datafile){
        return datasize!=0;
    } else {
        return stdin_has_data;
    }
    return 0;
}


uint8_t read_keyboard(void){
    int ch = 0;
    if (options.flag_datafile){
        if (datasize){
            ch = fgetc(datafile);
            datasize--;
            if (datasize == 0L){
                // That was the last character
                // in file. Deactivate datafile mode
                fclose(datafile);
                options.flag_datafile = 0;
            }
        }
    } else {
        if (stdin_has_data){
            ch = stdin_value;
            stdin_has_data = 0;
        }
    }
    
    // LF -> CR translation
    if (ch==0x0A){
      ch = 0x0D;
    }
    
    return (uint8_t)ch;
}


// Write to the terminal
void write_terminal(uint8_t byte){
    putchar(byte);
    fflush(stdout);
}
