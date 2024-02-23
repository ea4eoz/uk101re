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

#ifndef timeutils_h
    #define timeutils_h
    #include <time.h>
    void timerspecsub(struct timespec *stop, struct timespec *start, struct timespec *result);
    int nsleep(long ns);
    long timespec_to_ns(struct timespec *t);
    long timespec_to_us(struct timespec *t);
    long timespec_to_ms(struct timespec *t);
#endif
