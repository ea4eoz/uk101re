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

#include <errno.h>
#include <time.h>



// A timersub function for timespec instead timerval
void timerspecsub(struct timespec *stop, struct timespec *start, struct timespec *result){
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000L;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }
    
    return;
}



// Stops execution for a given amount of time specified in nanoseconds
int nsleep(long ns){
    struct timespec ts;
    int ret;

    if (ns < 0){
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec  = ns / 1000000000L;
    ts.tv_nsec = ns % 1000000000L;

    do {
        ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);

    return ret;
}



// timespec to nanoseconds
long timespec_to_ns(struct timespec *t){
    return t->tv_sec * 1000000000L + t->tv_nsec;
}



// timespec to microseconds
long timespec_to_us(struct timespec *t){
    return t->tv_sec * 1000000 + t->tv_nsec / 1000;
}



// timespec to milliseconds
long timespec_to_ms(struct timespec *t){
    return t->tv_sec * 1000 + t->tv_nsec / 1000000;
}

