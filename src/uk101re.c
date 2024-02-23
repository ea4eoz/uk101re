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

#include <stdio.h>
#include <time.h>

#include "cpu6502.h"
#include "motherboard.h"
#include "options.h"
#include "terminal.h"
#include "timeutils.h"



int main(int argc, char *argv[]) {
    // Parse command line options
    parse_options(argc, argv);
    
    // Set terminal into raw mode
    // among other things
    configure_terminal();
    
    // We are ready. Let's start the emulation!

    // Initializes hardware
    motherboard_init();
    
    // Reset all devices
    motherboard_reset();
   
    // Start execute instructions

    // Try to get 1.000 MHz speed running 20000
    // cycles in (less than) 20 milliseconds
    // and then sleep up to 20 milliseconds
    while(1){
        struct timespec start, end, elapsed;
        int cycles = 0;
        
        if (!(options.flag_turbo | options.flag_datafile)){
            clock_gettime(CLOCK_MONOTONIC, &start);
        }
          
        // Run for 20000 cycles
        while (cycles < 20000){
            cycles += cpu_execute();
        }
        
        if (ACTION){
            if (ACTION == ACTION_RESET){
                printf("\n*** CPU Reset ***\n");
                cpu_reset();
            }
            // Process other user actions here
            ACTION = ACTION_NONE;
        }
        
        if (!(options.flag_turbo | options.flag_datafile)){
            clock_gettime(CLOCK_MONOTONIC, &end);
        
            // Calculate the time spent
            timerspecsub(&end, &start, &elapsed);
            long time_spent = timespec_to_ns(&elapsed);
            
            // 20000 cycles at 1.000 MHz is 20 ms
            long sleeptime = 20000000L - time_spent;
            if(sleeptime > 0){
                nsleep(sleeptime);
            }
        }
    }        
}
